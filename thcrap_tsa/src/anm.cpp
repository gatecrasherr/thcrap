/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * On-the-fly ANM patcher.
  *
  * Portions adapted from Touhou Toolkit
  * https://github.com/thpatch/thtk
  */

#include <thcrap.h>
#include <tlnote.hpp>
#include <png.h>
#include "png_ex.h"
#include "thcrap_tsa.h"
#include "anm.hpp"
#include <GdiPlus.h>
#include <filesystem>

/// GDI+
/// ----
struct AnmGdiplus {
	Gdiplus::Status initStatus;
	Gdiplus::GdiplusStartupInput startupInput;
	Gdiplus::GdiplusStartupOutput startupOutput;
	ULONG_PTR token;

	CLSID pngClsid;

	bool havePngEncoder = false;

	void GetPNGEncoderCLSID(void);
	uint8_t* Decode(const uint8_t* data, size_t len, size_t* width, size_t* height) const;
	std::pair<uint8_t*, size_t> Encode(uint8_t* data, size_t width, size_t height, size_t stride);

	AnmGdiplus() {
		Gdiplus::GdiplusStartup(&this->token, &this->startupInput, &this->startupOutput);
		this->GetPNGEncoderCLSID();
	}
	~AnmGdiplus() {
		if (this->initStatus == Gdiplus::Ok) {
			Gdiplus::GdiplusShutdown(this->token);
		}
	}
};

void AnmGdiplus::GetPNGEncoderCLSID(void) {

	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return;

	auto* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0)
		{
			this->pngClsid = pImageCodecInfo[j].Clsid;
			this->havePngEncoder = true;
		}
	}

	free(pImageCodecInfo);
}

uint8_t* AnmGdiplus::Decode(const uint8_t* data, size_t len, size_t* width, size_t* height) const {
	uint8_t* out = NULL;
	if (IStream* stream = SHCreateMemStream((const BYTE*)data, len)) {
		if (Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(stream)) {
			UINT w = bitmap->GetWidth();
			UINT h = bitmap->GetHeight();
			Gdiplus::Rect r(0, 0, w, h);
			Gdiplus::BitmapData bmdata;

			if (bitmap->LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmdata) == Gdiplus::Ok) {
				*width = w;
				*height = h;

				out = (uint8_t*)malloc(w * h * 4);

				char* p = (char*)bmdata.Scan0;
				char* q = (char*)out;
				for (size_t y = 0; y < h; y++) {
					memcpy(q, p, w * 4);
					p += bmdata.Stride;
					q += w * 4;
				}

				bitmap->UnlockBits(&bmdata);
			}
			delete bitmap;
		}
		stream->Release();
	}
	return out;
}

std::pair<uint8_t*, size_t> AnmGdiplus::Encode(uint8_t* data, size_t width, size_t height, size_t stride) {
	Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(width, height, stride, PixelFormat32bppARGB, data);
	if (!bitmap) {
		return { nullptr, 0 };
	}
	defer(delete bitmap);	

	IStream* stream = SHCreateMemStream(nullptr, 0);
	if (!stream) {
		return { nullptr, 0 };
	}
	defer(stream->Release());

	auto res = bitmap->Save(stream, &this->pngClsid);
	if (res != Gdiplus::Ok) {
		return { nullptr, 0 };
	}
	STATSTG stat;
	stream->Stat(&stat, 0);

	uint8_t* out = (uint8_t*)malloc(stat.cbSize.LowPart);

	LARGE_INTEGER seek = { };
	ULARGE_INTEGER newPos = { };
	stream->Seek(seek, 0, &newPos);

	ULONG byteRet;
	stream->Read(out, stat.cbSize.LowPart, &byteRet);

	return { out, byteRet };
}

AnmGdiplus* anmGdiplus = nullptr;

extern "C" void gdiplus_mod_init(void) {
	anmGdiplus = new AnmGdiplus();
}

extern "C" void gdiplus_mod_exit(void) {
	delete anmGdiplus;
	anmGdiplus = nullptr;
}

/// Blitting modes
/// --------------
void blit_overwrite(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format)
{
	memcpy(dst, rep, pixels * format_Bpp(format));
}

void blit_blend(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format)
{
	// Alpha values are added and clamped to the format's maximum. This avoids a
	// flaw in the blending algorithm, which may decrease the alpha value even if
	// both target and replacement pixels are fully opaque.
	// (This also seems to be what the default composition mode in GIMP does.)
	if(format == FORMAT_BGRA8888) {
		for(size_t i = 0; i < pixels; ++i, dst += 4, rep += 4) {
			const int new_alpha = dst[3] + rep[3];
			const int dst_alpha = 0xff - rep[3];

			dst[0] = (dst[0] * dst_alpha + rep[0] * rep[3]) >> 8;
			dst[1] = (dst[1] * dst_alpha + rep[1] * rep[3]) >> 8;
			dst[2] = (dst[2] * dst_alpha + rep[2] * rep[3]) >> 8;
			dst[3] = MIN(new_alpha, 0xff);
		}
	} else if(format == FORMAT_ARGB4444) {
		for(size_t i = 0; i < pixels; ++i, dst += 2, rep += 2) {
			const unsigned char rep_a = (rep[1] & 0xf0) >> 4;
			const unsigned char rep_r = (rep[1] & 0x0f) >> 0;
			const unsigned char rep_g = (rep[0] & 0xf0) >> 4;
			const unsigned char rep_b = (rep[0] & 0x0f) >> 0;
			const unsigned char dst_a = (dst[1] & 0xf0) >> 4;
			const unsigned char dst_r = (dst[1] & 0x0f) >> 0;
			const unsigned char dst_g = (dst[0] & 0xf0) >> 4;
			const unsigned char dst_b = (dst[0] & 0x0f) >> 0;
			const int new_alpha = dst_a + rep_a;
			const int dst_alpha = 0xf - rep_a;

			dst[1] =
				(MIN(new_alpha, 0xf) << 4) |
				((dst_r * dst_alpha + rep_r * rep_a) >> 4);
			dst[0] =
				(dst_g * dst_alpha + rep_g * rep_a & 0xf0) |
				((dst_b * dst_alpha + rep_b * rep_a) >> 4);
		}
	} else {
		// Other formats have no alpha channel, so we can just do...
		blit_overwrite(dst, rep, pixels, format);
	}
}

struct blitmode_t {
	stringref_t name;
	BlitFunc_t func;
	stringref_t desc;
};

constexpr blitmode_t BLITMODES[] = {
	{
		"auto",
		nullptr,
		"Chooses the appropriate blitting function based on the values in the alpha channels of the original and the replacement image. (default)"
	},
	{
		"blend",
		blit_blend,
		"Alpha-blends the replacement image on top of the game's original image."
	},
	{
		"overwrite",
		blit_overwrite,
		"Overwrites all original pixels with pixels from the replacement image."
	},
};
/// --------------

void anm_entry_t::transform_and_add_sprite(const sprite_t &s_orig, BlitFunc_t blitmode)
{
	if(w == 0 || h == 0 || s_orig.w == 0.0f || s_orig.h == 0.0f) {
		return;
	}
	sprite_local_t s_lower_right(blitmode,
		((png_uint_32)s_orig.x) % w,
		((png_uint_32)s_orig.y) % h,
		MIN((png_uint_32)s_orig.w, w),
		MIN((png_uint_32)s_orig.h, h)
	);
	auto split_w = (int32_t)(s_lower_right.x + s_lower_right.w) - (int32_t)w;
	auto split_h = (int32_t)(s_lower_right.y + s_lower_right.h) - (int32_t)h;
	if(split_w > 0) {
		s_lower_right.w = w - s_lower_right.x;
	}
	if(split_h > 0) {
		s_lower_right.h = h - s_lower_right.y;
	}
	if(split_w > 0 && split_h > 0) {
		// Upper-left split
		sprites.emplace_back(blitmode,
			0, 0, (png_uint_32)split_w, (png_uint_32)split_h
		);
	}
	if(split_w > 0) {
		// Lower-left split
		sprites.emplace_back(blitmode,
			0, s_lower_right.y, (png_uint_32)split_w, s_lower_right.h
		);
	}
	if(split_h > 0) {
		// Upper-right split
		sprites.emplace_back(blitmode,
			s_lower_right.x, 0, s_lower_right.w, (png_uint_32)split_h
		);
	}
	sprites.emplace_back(s_lower_right);
}

/// Formats
/// -------
unsigned int format_Bpp(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
			return 4;
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return 2;
		case FORMAT_GRAY8:
			return 1;
		default:
			log_printf("unknown format: %u\n", format);
			return 0;
	}
}

png_uint_32 format_png_equiv(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return PNG_FORMAT_BGRA;
		case FORMAT_GRAY8:
			return PNG_FORMAT_GRAY;
		default:
			log_printf("unknown format: %u\n", format);
			return PNG_FORMAT_INVALID;
	}
}

png_byte format_alpha_max(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
			return 0xff;
		case FORMAT_ARGB4444:
			return 0xf;
		default:
			return 0;
	}
}

size_t format_alpha_sum(png_bytep data, unsigned int pixels, format_t format)
{
	size_t ret = 0;
	if(format == FORMAT_BGRA8888) {
		for(size_t i = 0; i < pixels; ++i, data += 4) {
			ret += data[3];
		}
	} else if(format == FORMAT_ARGB4444) {
		for(size_t i = 0; i < pixels; ++i, data += 2) {
			ret += (data[1] & 0xf0) >> 4;
		}
	}
	return ret;
}

void format_from_bgra(png_bytep data, unsigned int pixels, format_t format)
{
	png_bytep in = data;

	if(format == FORMAT_ARGB4444) {
		png_bytep out = data;
		for(size_t i = 0; i < pixels; ++i, in += 4, out += 2) {
			// I don't see the point in doing any "rounding" here. Let's rather focus on
			// writing understandable code independent of endianness assumptions.
			const unsigned char b = in[0] >> 4;
			const unsigned char g = in[1] >> 4;
			const unsigned char r = in[2] >> 4;
			const unsigned char a = in[3] >> 4;
			// Yes, we start with the second byte. "Little-endian ARGB", mind you.
			out[1] = (a << 4) | r;
			out[0] = (g << 4) | b;
		}
	} else if(format == FORMAT_RGB565) {
		png_uint_16p out16 = (png_uint_16p)data;
		for(size_t i = 0; i < pixels; ++i, in += 4, ++out16) {
			const unsigned char b = in[0] >> 3;
			const unsigned char g = in[1] >> 2;
			const unsigned char r = in[2] >> 3;

			out16[0] = (r << 11) | (g << 5) | b;
		}
	}
	// FORMAT_GRAY8 is fully handled by libpng
}
/// -------

/// Sprite-level patching
/// ---------------------
int sprite_patch_set(
	sprite_patch_t &sp,
	const img_patch_t &patch,
	const sprite_local_t &sprite,
	const png_image_ex &image
)
{
	sp = {};

	// Note that we don't use the PNG_IMAGE_* macros here - the actual bit depth
	// after format_from_bgra() may no longer be equal to the one in the PNG header.
	sp.format = patch.format;
	sp.bpp = format_Bpp(sp.format);
	sp.blitmode = sprite.blitmode;

	sp.dst_x = sprite.x;
	sp.dst_y = sprite.y;

	sp.rep_x = patch.x + sp.dst_x;
	sp.rep_y = patch.y + sp.dst_y;

	assert(sp.dst_x < patch.w || sp.dst_y < patch.h);
	if(sp.rep_x >= image.img.width || sp.rep_y >= image.img.height) {
		return 2;
	}

	sp.rep_stride = image.img.width * sp.bpp;
	sp.dst_stride = patch.stride;

	// See th11's face/enemy5/face05lo.png (the dummy 8x8 one from
	// stgenm06.anm, not stgenm05.anm) for an example where the sprite
	// width and height actually exceed the dimensions of the THTX.
	assert(sprite.w <= (patch.w - sprite.x));
	assert(sprite.h <= (patch.h - sprite.y));

	sp.copy_w = MIN(sprite.w, (image.img.width - sp.rep_x));
	sp.copy_h = MIN(sprite.h, (image.img.height - sp.rep_y));

	sp.dst_buf = patch.img_data + (sp.dst_y * sp.dst_stride) + (sp.dst_x * sp.bpp);
	sp.rep_buf = image.buf + (sp.rep_y * sp.rep_stride) + (sp.rep_x * sp.bpp);
	return 0;
}

sprite_alpha_t sprite_alpha_analyze(
	const png_bytep buf,
	const format_t format,
	const size_t stride,
	const png_uint_32 w,
	const png_uint_32 h
)
{
	const size_t opaque_sum = format_alpha_max(format) * w;
	if(!buf) {
		return SPRITE_ALPHA_EMPTY;
	} else if(!opaque_sum) {
		return SPRITE_ALPHA_OPAQUE;
	} else {
		sprite_alpha_t ret = SPRITE_ALPHA_FULL;
		png_bytep p = buf;
		for(png_uint_32 row = 0; row < h; row++) {
			size_t sum = format_alpha_sum(p, w, format);
			if(sum == 0x00 && ret != SPRITE_ALPHA_OPAQUE) {
				ret = SPRITE_ALPHA_EMPTY;
			} else if(sum == opaque_sum && ret != SPRITE_ALPHA_EMPTY) {
				ret = SPRITE_ALPHA_OPAQUE;
			} else {
				return SPRITE_ALPHA_FULL;
			}
			p += stride;
		}
		return ret;
	}
}

sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t &sp)
{
	return sprite_alpha_analyze(sp.rep_buf, sp.format, sp.rep_stride, sp.copy_w, sp.copy_h);
}

sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t &sp)
{
	return sprite_alpha_analyze(sp.dst_buf, sp.format, sp.dst_stride, sp.copy_w, sp.copy_h);
}

int sprite_blit(const sprite_patch_t &sp, const BlitFunc_t func)
{
	assert(func);

	png_bytep dst_p = sp.dst_buf;
	png_bytep rep_p = sp.rep_buf;
	for(png_uint_32 row = 0; row < sp.copy_h; row++) {
		func(dst_p, rep_p, sp.copy_w, sp.format);
		dst_p += sp.dst_stride;
		rep_p += sp.rep_stride;
	}
	return 0;
}

int sprite_patch(const sprite_patch_t &sp)
{
	if(sp.blitmode) {
		return sprite_blit(sp, sp.blitmode);
	}
	sprite_alpha_t rep_alpha = sprite_alpha_analyze_rep(sp);
	if(rep_alpha != SPRITE_ALPHA_EMPTY) {
		BlitFunc_t func = NULL;
		sprite_alpha_t dst_alpha = sprite_alpha_analyze_dst(sp);
		if(dst_alpha == SPRITE_ALPHA_OPAQUE) {
			func = blit_blend;
		} else {
			func = blit_overwrite;
		}
		sprite_blit(sp, func);
	}
	return 0;
}
/// ---------------------

/// ANM header patching
/// -------------------
#define HEADER_MOD_ERROR(...) log_error_mboxf("ANM header patching error", __VA_ARGS__)

Option<BlitFunc_t> blitmode_parse(json_t *blitmode_j, const char *context, ...)
{
	if(!blitmode_j) {
		return Option<BlitFunc_t>{};
	}
	if(json_is_string(blitmode_j)) {
		auto blitmode = json_string_value(blitmode_j);
		for(const auto &mode : BLITMODES) {
			if(!strcmp(mode.name.data(), blitmode)) {
				return mode.func;
			}
		}
	}
	constexpr std::string_view MODE_DESC_FMT = "\n\n• \"%s\": %s"sv;
	size_t modes_len = 0;
	for(const auto &mode : BLITMODES) {
		modes_len += MODE_DESC_FMT.length() + mode.name.length() + mode.desc.length();
	}
	VLA(char, modes, modes_len + 1);
	char *p = modes;
	for(const auto &mode : BLITMODES) {
		p += sprintf(p, MODE_DESC_FMT.data(), mode.name.data(), mode.desc.data());
	}

	va_list va;
	va_start(va, context);
	int ctx_len = vsnprintf(NULL, 0, context, va);
	VLA(char, ctx, ctx_len + 1);
	vsprintf(ctx, context, va);
	va_end(va);
	HEADER_MOD_ERROR(
		"%s: Invalid blitting mode. Must be one of the following:%s",
		ctx, modes
	);
	VLA_FREE(ctx);
	VLA_FREE(modes);
	return Option<BlitFunc_t>{};
}

script_mods_t entry_mods_t::script_mods(uint8_t *in, anm_offset_t &offset, uint32_t version)
{
	script_mods_t ret = { num, offset.id };

#define FAIL(context, text, ...) \
	scripts = nullptr; \
	HEADER_MOD_ERROR( \
		"{\"entries\"{\"%zu\": {\"scripts\": {\"%d\"" context "}}}}: " text "\nIgnoring remaining script modifications for this entry...", \
		num, ret.script_num TH_OPT_COMMA(__VA_ARGS__) \
	); \
	return ret;

#define CHECK_LINE_NUMBER(context, ...) \
	if(line_i >= ret.script.num_instrs) { \
		FAIL( \
			context, \
			"Line number %u out of range, script only has %u instructions.", \
			__VA_ARGS__, (unsigned int)line_i, ret.script.num_instrs \
		) \
	}

#define CHECK_LINE_NUMBER_AFTER_DELETIONS(context, ...) \
	if(deletions_count == 0) { \
		CHECK_LINE_NUMBER(context, ##__VA_ARGS__); \
	} else if(line_i >= (ret.script.num_instrs - deletions_count)) { \
		FAIL( \
			context, \
			"Line number %u out of range, script only has %zu instructions after %zu deletions.", \
			__VA_ARGS__, \
			(unsigned int)line_i, \
			ret.script.num_instrs - deletions_count, \
			deletions_count \
		) \
	}

	auto mod_j = json_object_numkey_get(scripts, ret.script_num);
	if(!mod_j) {
		return ret;
	}
	if(!json_is_object(mod_j)) {
		FAIL("", "Must be a JSON object.");
	}

	auto first_instr = in + offset.offset;
	(version == 0)
		? ret.script.init((anm_instr0_t *)(first_instr))
		: ret.script.init((anm_instr_t *)(first_instr));

	auto deletions_j = json_object_get(mod_j, "deletions");
	if(deletions_j && !json_is_array(deletions_j) && !json_is_integer(deletions_j)) {
		FAIL(": {\"deletions\"}", "Must be a flexible JSON array of integers.");
	}
	size_t deletions_count = json_flex_array_size(deletions_j);
	ret.deletions.reserve(deletions_count);

	auto changes_j = json_object_get(mod_j, "changes");
	if(changes_j && !json_is_object(changes_j)) {
		FAIL(": {\"changes\"}", "Must be a JSON object.");
	}

	for(size_t i = 0; i < deletions_count; i++) {
		auto line_j = json_flex_array_get(deletions_j, i);
		auto line_i = json_integer_value(line_j);
		if(!json_is_integer(line_j) || line_i < 0) {
			FAIL(": {\"deletions\"[%zu]}", "Must be a positive JSON integer.", i);
		}
		CHECK_LINE_NUMBER(": {\"deletions\"[%zu]}", i);
		auto line = (unsigned int)line_i;
		for(const auto &l : ret.deletions) {
			if(l == line) {
				FAIL(
					": {\"deletions\"[%zu]}",
					"Duplicate deletion of line %u.",
					i, l
				);
			}
		}
		ret.deletions.emplace_back(line);
	}
	// Make the line numbers relative to the previous deletion
	for(size_t i = 1; i < deletions_count; i++) {
		ret.deletions[i] -= i;
	}

	const char *key;
	json_t *val_j;
	json_object_foreach_fast(changes_j, key, val_j) {
		auto fail_key_syntax = [&]() {
			FAIL(
				": {\"changes\": {\"%s\"}",
				"Invalid key syntax, must be one of:\n"
				"• \"<line>#<parameter address>\" (to change instruction parameters)\n"
				"• \"<line>#time\" (to change the frame an instruction is executed on)\n",
				key
			);
		};

		auto key_sep = strchr(key, '#');
		if(!key_sep || key_sep == key || key_sep[0] == '\0') {
			return fail_key_syntax();
		}
		char *endptr = nullptr;

		long line = strtoul(key, &endptr, 10);
		if(endptr != key_sep || line < 0) {
			return fail_key_syntax();
		}
		unsigned int line_i = line;
		CHECK_LINE_NUMBER_AFTER_DELETIONS(": {\"changes\": {\"%s\"}", key);

		if(!strcmp(key_sep + 1, "time")) {
			auto time = json_integer_value(val_j);
			if(!json_is_integer(val_j) || (time < INT16_MIN) || (time > INT16_MAX)) {
				FAIL(
					": {\"changes\": {\"%s\"}", "Time must be a signed 16-bit integer, between -32767 and 32767.",
					key
				);
			}
			ret.time_changes.emplace_back(
				(unsigned int)(line_i - deletions_count), (uint16_t)time
			);
		} else {
			long addr = strtoul(key_sep + 1, &endptr, 10);
			if(endptr == (key_sep + 1) || endptr[0] != '\0' || addr < 0) {
				return fail_key_syntax();
			}

			const char* code = json_string_value(val_j);
			size_t code_size = binhack_calc_size(code);
			if(!code || !code_size) {
				FAIL(
					": {\"changes\": {\"%s\"}", "Must be a binary hack string.",
					key
				);
			}

			uint16_t param_length = ((ret.script).*(ret.script.param_length_of))(line_i);
			if((size_t)addr > (param_length - code_size)) {
				FAIL(
					": {\"changes\": {\"%s\"}",
					"Address %u + binary hack of length %zu exceeds the parameter length of line %u (%u).",
					key, addr, code_size, line, param_length
				);
			}

			ret.param_changes.emplace_back(
				(unsigned int)(line_i - deletions_count), (uint16_t)addr, code, code_size
			);
		}
	}
	return ret;

#undef FAIL
}

void script_mods_t::apply_orig()
{
#define LOG(text, ...) \
	log_printf("(Header) Entry #%u, Script #%d: " text "\n", \
		entry_num, script_num, ##__VA_ARGS__ \
	);

	for(size_t i = 0; i < deletions.size(); i++) {
		decltype(i) line = deletions[i];
		LOG("Deleting line %zu", line + i);
		((script).*(script.delete_line))(line);
	}
	for(size_t i = 0; i < param_changes.size(); i++) {
		const auto &pc = param_changes[i];
		VLA(uint8_t, code, pc.code_size);

		// Should have failed in the size calculation already.
		assert(code_string_render(code, 0, pc.code, 0) == 0);

		LOG(
			"Changing parameter data at offset %u on line %zu",
			pc.param_addr, deletions.size() + pc.line
		);
		((script).*(script.apply_param_change))(
			pc.line, pc.param_addr, code, pc.code_size
		);
		VLA_FREE(code);
	}
	for(const auto &tc : time_changes) {
		LOG("Changing time on line %zu", deletions.size() + tc.line);
		((script).*(script.apply_time_change))(tc);
	}

#undef LOG
}

entry_mods_t header_mods_t::entry_mods()
{
	entry_mods_t ret;
	if(!entries) {
		return ret;
	}

	ret.num = entries_seen++;

#define FAIL(context, text) \
	entries = nullptr; \
	HEADER_MOD_ERROR( \
		"\"entries\"{\"%zu\"" context "}: " text "\nIgnoring remaining entry modifications for this file...", ret.num \
	); \
	return ret;

	auto mod_j = json_object_numkey_get(entries, ret.num);
	if(!mod_j) {
		return ret;
	}
	if(!json_is_object(mod_j)) {
		FAIL("", "Must be a JSON object.");
	}

	// File name
	auto name_j = json_object_get(mod_j, "name");
	if(name_j && !json_is_string(name_j)) {
		FAIL(": {\"name\"}", "Must be a JSON string.");
	}
	ret.name = json_string_value(name_j);

	// Blitting mode
	auto blitmode_j = json_object_get(mod_j, "blitmode");
	ret.blitmode = blitmode_parse(
		blitmode_j, "\"entries\"{\"%zu\": {\"blitmode\"}}", ret.num
	);

	// Scripts
	auto scripts_j = json_object_get(mod_j, "scripts");
	if(scripts_j && !json_is_object(scripts_j)) {
		FAIL(": {\"scripts\"}", "Must be a JSON object.");
	}
	ret.scripts = scripts_j;

	return ret;

#undef FAIL
}

void entry_mods_t::apply_ourdata(anm_entry_t &entry)
{
	if(name) {
		log_printf(
			"(Header) Entry #%zu: %s → %s\n",
			num, entry.name, name
		);
		entry.name = name;
	}
}

sprite_mods_t header_mods_t::sprite_mods()
{
	sprite_mods_t ret;
	if(!sprites) {
		return ret;
	}

	ret.num = sprites_seen++;

#define FAIL(context, text, ...) \
	sprites = nullptr; \
	HEADER_MOD_ERROR( \
		"\"sprites\"{\"%zu\"" context "}: " text "\nIgnoring remaining sprite mods for this file...", \
		ret.num TH_OPT_COMMA(__VA_ARGS__) \
	);

#define BOUNDS_PARSE(context, bounds_j) \
	if(auto rect = json_xywh_value(bounds_j); !rect.err.empty()) { \
		FAIL(context, "(Bounds) %s", rect.err.c_str()); \
	} \
	else { \
		ret.bounds = rect.v; \
	}

	auto mod_j = json_object_numkey_get(sprites, ret.num);
	if(json_is_object(mod_j)) {
		auto bounds_j = json_object_get(mod_j, "bounds");
		if(bounds_j) {
			BOUNDS_PARSE(": {\"bounds\"}", bounds_j);
		}
		auto blitmode_j = json_object_get(mod_j, "blitmode");
		ret.blitmode = blitmode_parse(
			blitmode_j, "\"sprites\"{\"%zu\": {\"blitmode\"}}", ret.num
		);
	} else if(json_is_array(mod_j)) {
		BOUNDS_PARSE("", mod_j);
	} else if(json_is_string(mod_j)) {
		ret.blitmode = blitmode_parse(
			mod_j, "\"sprites\"{\"%zu\"}", ret.num
		);
	} else if(mod_j) {
		FAIL("", "Invalid data type. See anm.hpp for documentation on ANM header patching.");
	}
	return ret;

#undef FAIL
}

void sprite_mods_t::apply_orig(sprite_t &orig)
{
	if(bounds.is_some()) {
		const auto &b = bounds.unwrap();
		log_printf(
			"(Header) Sprite #%zu: [%.0f, %.0f, %.0f, %.0f] → [%.0f, %.0f, %.0f, %.0f]\n",
			num,
			orig.x, orig.y, orig.w, orig.h,
			   b.x,    b.y,    b.w,    b.h
		);
		orig.x = b.x;
		orig.y = b.y;
		orig.w = b.w;
		orig.h = b.h;
	}
}

header_mods_t::header_mods_t(json_t *patch)
{
	auto object_get = [this, patch] (const char *key) -> json_t* {
		auto ret = json_object_get(patch, key);
		if(ret && !json_is_object(ret)) {
			HEADER_MOD_ERROR("\"%s\" must be a JSON object.", key);
			return NULL;
		}
		return ret;
	};

	entries = object_get("entries");
	sprites = object_get("sprites");

	auto blitmode_j = json_object_get(patch, "blitmode");
	auto blitmode_o = blitmode_parse(blitmode_j, "\"blitmode\"");
	blitmode = blitmode_o.unwrap_or(nullptr);
}
/// -------------------

/// ANM structure
/// -------------
inline bool instr_is_last(anm_instr0_t *instr)
{
	return (instr->type == 0) && (instr->time == 0);
}

inline bool instr_is_last(anm_instr_t *instr)
{
	return (instr->type == 0xffff);
}

inline void instr_make_last(anm_instr0_t *instr)
{
	instr->type = 0;
	instr->time = 0;
}

inline void instr_make_last(anm_instr_t *instr)
{
	instr->type = 0xffff;
}

inline anm_instr0_t* instr_next(anm_instr0_t *instr)
{
	return (anm_instr0_t*)((char *)instr + sizeof(*instr) + instr->length);
}

inline anm_instr_t* instr_next(anm_instr_t *instr)
{
	return (anm_instr_t*)((char *)instr + instr->length);
}

template <typename T> T* script_t::_instr_at_line(unsigned int line)
{
	assert(line < num_instrs);
	auto instr = (T *)first_instr;
	unsigned int i = 0;
	while(i < line) {
		instr = instr_next(instr);
		i++;
	}
	return instr;
}

template <typename T> uint16_t script_t::_param_length_of(unsigned int line)
{
	auto instr = _instr_at_line<T>(line);
	auto instr_diff = ((uint8_t *)instr_next(instr) - (uint8_t *)instr);
	return (uint16_t)(instr_diff - sizeof(T));
}

template <typename T> void script_t::_delete_line(unsigned int line)
{
	auto instr = _instr_at_line<T>(line);
	auto instr_after = instr_next(instr);
	size_t bytes_removed = (uint8_t *)instr_after - (uint8_t *)instr;
	memmove(instr, instr_after, after_last - (uint8_t *)instr_after);
	after_last -= bytes_removed;
	instr_make_last((T *)after_last);
	num_instrs--;
}

template <typename T> void script_t::_apply_time_change(script_time_change_t change)
{
	auto instr = _instr_at_line<T>(change.line);
	instr->time = change.time;
}

template <typename T> void script_t::_apply_param_change(
	unsigned int line, uint16_t param_addr,
	const uint8_t *code, size_t code_size
)
{
	auto instr = _instr_at_line<T>(line);
	memcpy(instr->data + param_addr, code, code_size);
}

template <typename T> void script_t::init(T *first)
{
	auto instr = first;
	while(instr_is_last(instr) == false) {
		num_instrs++;
		instr = instr_next(instr);
	}
	first_instr = (uint8_t *)first;
	after_last = (uint8_t *)instr_next(instr);
	param_length_of = &script_t::_param_length_of<T>;
	delete_line = &script_t::_delete_line<T>;
	apply_time_change = &script_t::_apply_time_change<T>;
	apply_param_change = &script_t::_apply_param_change<T>;
}

#define ANM_ENTRY_FILTER(in, type) \
	type *header = (type *)in; \
	version = header->version; \
	entry.w = header->w; \
	entry.h = header->h; \
	entry.next = (header->nextoffset) ? in + header->nextoffset : nullptr; \
	sprite_orig_num = header->sprites; \
	entry.name = (const char*)(header->nameoffset + (size_t)header); \
	script_num = header->scripts; \
	thtxoffset = header->thtxoffset; \
	entry.hasdata = (header->hasdata != 0); \
	headersize = sizeof(type);

int anm_entry_init(header_mods_t &hdr_m, anm_entry_t &entry, uint8_t *in, uint8_t* in_endptr, uint8_t* out, json_t *patch)
{
	if(!in) {
		return -1;
	}

	size_t thtxoffset = 0;
	size_t headersize = 0;
	size_t sprite_orig_num = 0;
	size_t script_num;
	uint32_t version;

	anm_entry_clear(entry);
	auto ent_m = hdr_m.entry_mods();
	auto entry_blitmode = ent_m.blitmode.unwrap_or(hdr_m.blitmode);
	if(game_id >= TH11) {
		ANM_ENTRY_FILTER(in, anm_header11_t);
		entry.x = header->x;
		entry.y = header->y;
	} else {
		ANM_ENTRY_FILTER(in, anm_header06_t);
	}

	assert((entry.hasdata == false) == (thtxoffset == 0));
	assert(headersize);

	entry.hasdata |= (entry.name[0] != '@');
	if(thtxoffset) {
		thtx_header_t* thtx = (thtx_header_t*)(in + thtxoffset);
		if(memcmp(thtx->magic, "THTX", sizeof(thtx->magic))) {
			return 1;
		}
		entry.w = thtx->w;
		entry.h = thtx->h;
		entry.thtxoffset = thtxoffset;
		memcpy(out, in, thtxoffset + sizeof(thtx_header_t));
	}
	else if(entry.next) {
		memcpy(out, in, entry.next - in);
	}
	else {
		memcpy(out, in, in_endptr - in);
	}

	if(sprite_orig_num == 0) {
		// Construct a fake sprite covering the entire texture
		entry.sprites.emplace_back(entry_blitmode, 0, 0, entry.w, entry.h);
	} else {
		entry.sprites.reserve(sprite_orig_num);
		auto *sprite_in = (uint32_t*)(out + headersize);
		for(size_t i = 0; i < sprite_orig_num; i++, sprite_in++) {
			auto *s_orig = (sprite_t*)(out + *sprite_in);

			// This needs, in fact, be called for every sprite in order
			// to not mess up its internal counter! Which is why we have
			// to duplicate the condition from above, not only to keep
			// that one sprite moddable for the game itself.
			auto spr_m = hdr_m.sprite_mods();
			spr_m.apply_orig(*s_orig);

			auto blitmode = spr_m.blitmode.unwrap_or(entry_blitmode);
			// See TH09.5's help_0?.anm and TH16.5's title/title_insta.png
			// in title.anm for cases where the patchable image is larger
			// than the one defined sprite. In the latter case, we wouldn't
			// want to resize the sprite, as this would shift the texture
			// and distort its on-screen geometry.
			// And always expanding a single sprite to the entire texture
			// is never wrong, so before we start messing around with
			// custom/temporary sprite redefinitions...
			if(sprite_orig_num <= 1) {
				entry.sprites.emplace_back(blitmode, 0, 0, entry.w, entry.h);
			} else {
				entry.transform_and_add_sprite(*s_orig, blitmode);
			}
		}
	}

	auto script_offsets = (anm_offset_t*)(
		out + headersize + sprite_orig_num * sizeof(uint32_t)
	);
	for(size_t i = 0; i < script_num; i++) {
		ent_m.script_mods(out, script_offsets[i], version).apply_orig();
	}
	ent_m.apply_ourdata(entry);
	return 0;
}

#undef ANM_ENTRY_FILTER

void anm_entry_clear(anm_entry_t &entry)
{
	entry.x = 0;
	entry.y = 0;
	entry.w = 0;
	entry.h = 0;
	entry.hasdata = false;
	entry.next = nullptr;
	entry.name = nullptr;
	entry.thtxoffset = 0;
	entry.sprites.clear();
}
/// -------------

int patch_png_load_for_thtx(png_image_ex &image, const patch_t *patch_info, const char *fn, format_t format)
{
	SAFE_FREE(image.buf);
	png_image_free(&image.img);
	ZeroMemory(&image.img, sizeof(png_image));
	image.img.version = PNG_IMAGE_VERSION;

	size_t file_size;
	void* file_buffer = patch_file_load(patch_info, fn, &file_size);
	if(!file_buffer) {
		return 2;
	}

	if(png_image_begin_read_from_memory(&image.img, file_buffer, file_size)) {
		image.img.format = format_png_equiv(format);
		if(image.img.format != PNG_FORMAT_INVALID) {
			size_t png_size = PNG_IMAGE_SIZE(image.img);
			image.buf = (png_bytep)malloc(png_size);

			if(image.buf) {
				png_image_finish_read(&image.img, 0, image.buf, 0, NULL);
			}
		}
	}
	free(file_buffer);
	if(image.buf) {
		unsigned int pixels = image.img.width * image.img.height;
		format_from_bgra(image.buf, pixels, format);
	}
	return !image.buf;
}

// Helper function for stack_game_png_apply.
int patch_png_apply(img_patch_t &patch, std::vector<sprite_local_t>& sprites, const patch_t *patch_info, const char *fn)
{
	int ret = -1;
	if(patch_info && fn) {
		png_image_ex png = {};
		ret = patch_png_load_for_thtx(png, patch_info, fn, patch.format);
		if(!ret && png.buf) {
			for(const auto &sprite : sprites) {
				sprite_patch_t sp;
				if(!sprite_patch_set(sp, patch, sprite, png)) {
					sprite_patch(sp);
				}
			}
			patch_print_fn(patch_info, fn);
		}
		SAFE_FREE(png.buf);
	}
	return ret;
}

int stack_game_png_apply(img_patch_t& patch, std::vector<sprite_local_t>& sprites, const char* anm_fn, size_t entry_num)
{
	std::filesystem::path img_path(patch.name);
	std::filesystem::path anm_path(anm_fn);

	char* thtk_fn = nullptr;
	_asprintf(&thtk_fn, "%s/%s@%s@%d%s",
		img_path.parent_path().u8string().c_str(),
		img_path.stem().u8string().c_str(),
		anm_path.stem().u8string().c_str(),
		entry_num,
		img_path.extension().u8string().c_str()
	);

	int ret = 0;
	auto iterate_png = [&](const char* filename) {
		char** chain = resolve_chain_game(filename);
		stack_chain_iterate_t sci;
		sci.fn = NULL;
		log_printf("(PNG) Resolving %s... ", chain[0]);
		while (stack_chain_iterate(&sci, chain, SCI_FORWARDS)) {
			if (!patch_png_apply(patch, sprites, sci.patch_info, sci.fn)) {
				ret = 1;
			}
		}
		log_print(ret ? "\n" : "not found\n");
		chain_free(chain);
	};

	if (thtk_fn) {
		iterate_png(thtk_fn);
		free(thtk_fn);
		if (ret) {
			return ret;
		}
	}

	iterate_png(patch.name);
	return ret;
}

img_patch_t img_get_patch(anm_entry_t& entry, thtx_header_t* thtx) {
	img_patch_t img_patch = {};
	img_patch.name = entry.name;
	img_patch.x = entry.x;
	img_patch.y = entry.y;

	size_t w, h;
	uint8_t* orig_img = anmGdiplus->Decode(thtx->data, thtx->size, &w, &h);

	if (orig_img) {
		img_patch.was_encoded = true;
		img_patch.img_data = orig_img;
		img_patch.img_size = w * h * 4;
		img_patch.format = FORMAT_BGRA8888;
		img_patch.w = w;
		img_patch.h = h;
		img_patch.stride = w * 4;
	}
	else {
		orig_img = (uint8_t*)malloc(thtx->size);
		memcpy(orig_img, thtx->data, thtx->size);

		img_patch.was_encoded = false;
		img_patch.img_data = orig_img;
		img_patch.img_size = thtx->size;
		img_patch.format = (format_t)thtx->format;
		img_patch.w = entry.w;
		img_patch.h = entry.h;
		img_patch.stride = format_Bpp(img_patch.format) * img_patch.w;
	}

	assert(img_patch.w == entry.w && img_patch.w == thtx->w);
	assert(img_patch.h == entry.h && img_patch.h == thtx->h);

	return img_patch;
}

void img_encode(img_patch_t& patched) {
	if (!patched.was_encoded) {
		return;
	}
	auto [encoded, out_size] = anmGdiplus->Encode(patched.img_data, patched.w, patched.h, patched.stride);
	if (!encoded) {
		return;
	}
	free(patched.img_data);
	patched.img_data = encoded;
	patched.img_size = out_size;
}

inline uint32_t* anm_get_nextoffset_ptr(uint8_t* entry) {
	if (game_id >= TH11) {
		return &((anm_header11_t*)entry)->nextoffset;
	}
	else {
		return &((anm_header06_t*)entry)->nextoffset;
	}
}

int patch_anm(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	const char *dat_dump = runconfig_dat_dump_get();

	// Some ANMs reference the same file name multiple times in a row
	const char *name_prev = NULL;

	header_mods_t hdr_m(patch);
	anm_entry_t entry = {};
	png_image_ex png = {};
	png_image_ex bounds = {};
	size_t entry_num = 0;

	auto* file_in = (uint8_t*)malloc(size_in);
	auto* anm_entry_in = file_in;
	memcpy(anm_entry_in, file_inout, size_in);
	memset(file_inout, 0, size_out);

	auto *anm_entry_out = (uint8_t *)file_inout;
	auto *endptr = anm_entry_in + size_in;

	log_debugf(
		"---- ANM ----\n"
		"-- %s --\n"
		, fn
	);

	while(anm_entry_in && anm_entry_in < endptr) {
		if(!anm_entry_init(hdr_m, entry, anm_entry_in, endptr, anm_entry_out, patch)); else {
			log_print("Corrupt ANM file or format definition, aborting ...\n");
			break;
		}
		if(entry.hasdata) {
			if(!name_prev || strcmp(entry.name, name_prev)) {
				if (dat_dump) {
					bounds_store(name_prev, bounds);
					bounds_init(bounds, entry.w, entry.h, entry.name);
				}
				name_prev = entry.name;
			}
			png_image_resize(bounds, entry.x + entry.w, entry.y + entry.h);
			for(const auto &sprite : entry.sprites) {
				bounds_draw_rect(bounds, entry.x, entry.y, sprite);
			}

			if (entry.thtxoffset) {
				// Do the patching
				thtx_header_t* thtx_in = (thtx_header_t*)(anm_entry_in + entry.thtxoffset);

				img_patch_t img_patch = img_get_patch(entry, thtx_in);
				stack_game_png_apply(img_patch, entry.sprites, fn, entry_num);
				img_encode(img_patch);

				thtx_header_t* thtx_out = (thtx_header_t*)(anm_entry_out + entry.thtxoffset);
				thtx_out->size = img_patch.img_size;

				memcpy(thtx_out->data, img_patch.img_data, img_patch.img_size);

				uint32_t* nextoffset_ptr = anm_get_nextoffset_ptr(anm_entry_out);
				size_t out_nextoffset = entry.thtxoffset + sizeof(thtx_header_t) + img_patch.img_size;
				if (*nextoffset_ptr)
					*nextoffset_ptr = out_nextoffset;
				anm_entry_out += out_nextoffset;

				free(img_patch.img_data);
			}
			else {
				goto next_no_thtx;
			}
		}
		else if (entry.next) {
next_no_thtx:
			anm_entry_out += entry.next - anm_entry_in;
		}
		if(!entry.next) {
			bounds_store(name_prev, bounds);
		}
		anm_entry_in = entry.next;
		anm_entry_clear(entry);

		entry_num++;
	}
	png_image_clear(bounds);
	png_image_clear(png);
	free(file_in);

	log_debug("-------------\n");
	return 1;
}

template<typename T>
inline size_t anm_get_imgsize_diff(void* anm_file, size_t anm_size) {
	auto* entry = (T*)anm_file;
	size_t ret = 0;
	while ((size_t)entry - (size_t)anm_file < anm_size) {
		if (entry->thtxoffset) {
			thtx_header_t* thtx = (thtx_header_t*)((uintptr_t)entry + entry->thtxoffset);
			ret += (thtx->w * thtx->h * 4) - thtx->size;
		}

		if (entry->nextoffset) {
			entry = (T*)((uintptr_t)entry + entry->nextoffset);
		}
		else {
			break;
		}
	}
	return ret;
}

size_t anm_get_size(const char* fn, json_t* patch, size_t patch_size) {
	tlnote_remove();

	(void)patch_size;

	typedef void* __stdcall file_load_t(const char* filename, size_t* out_size);
	typedef void __cdecl ingame_free_t(void* mem);

	const json_t* runcfg = runconfig_json_get();
	const char* file_load_func_name = json_object_get_string(runcfg, "file_load_func");
	const char* free_func_name = json_object_get_string(runcfg, "free_func");

	if (!file_load_func_name || !free_func_name) {
		return 0;
	}

	auto* file_load = (file_load_t*)json_object_get_eval_addr_default(runcfg, "file_load_func", NULL, JEVAL_DEFAULT);
	auto* _ingame_free = (ingame_free_t*)json_object_get_eval_addr_default(runcfg, "free_func", NULL, JEVAL_DEFAULT);

	if (!file_load || !_ingame_free) {
		return 0;
	}

	// Hack
	file_rep_t* fr = fr_tls_get();
	fr->disable = true;

	size_t out_size;
	void* anm_file;

	anm_file = file_load(fn, &out_size);
	fr->disable = false;
	
	size_t d_size = anm_get_imgsize_diff<anm_header11_t>(anm_file, out_size);

	_ingame_free(anm_file);
	return d_size;
}
