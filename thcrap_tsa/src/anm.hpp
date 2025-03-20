/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * ANM structures and function declarations.
  */

#pragma once

#include <thtypes/anm_types.h>
#include <vector>

/// Blitting modes
/// --------------
// These blit a row with length [pixels] from [rep] to [dst], using [format].
typedef void(*BlitFunc_t)(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);

// Simply overwrites a number of [pixels] in [rep] with [dst].
void blit_overwrite(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);
// Alpha-blends a number of [pixels] from [rep] on top of [dst].
void blit_blend(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);
/// --------------

/// Patching types
/// --------------
struct sprite_local_t {
	BlitFunc_t blitmode; // nullptr == "auto"
	png_uint_32 x;
	png_uint_32 y;
	png_uint_32 w;
	png_uint_32 h;

	inline constexpr sprite_local_t(
		BlitFunc_t blitmode,
		png_uint_32 x, png_uint_32 y, png_uint_32 w, png_uint_32 h
	)
		: blitmode(blitmode), x(x), y(y), w(w), h(h) {
	}
};

// Alpha analysis results
typedef enum {
	SPRITE_ALPHA_EMPTY,
	SPRITE_ALPHA_OPAQUE,
	SPRITE_ALPHA_FULL
} sprite_alpha_t;

// All per-entry ANM data we need
struct anm_entry_t {
	// X and Y offsets of the THTX inside the image
	png_uint_32 x;
	png_uint_32 y;
	// Copied from the THTX header if we have one,
	// or the ANM entry header otherwise.
	png_uint_32 w;
	png_uint_32 h;

	// Relevant for TH06 which doesn't use THTX.
	bool hasdata;

	// Pointer to the next entry in the ANM archive.
	// A nullptr indicates the last one.
	uint8_t *next;

	// File name of the original PNG associated with the bitmap.
	// Can be set to a custom name by an ANM header patch.
	const char *name;

	size_t thtxoffset;

	// Guaranteed to contain at least one sprite after initialization.
	std::vector<sprite_local_t> sprites;

	// Transforms the logical sprite [s_orig] into up to four physical,
	// non-wrapping on-image sprites, and adds the results to <sprites>
	// if necessary.
	void transform_and_add_sprite(const sprite_t &s_orig, BlitFunc_t blitmode);
};

// Coordinates for sprite-based patching
struct sprite_patch_t {
	// General info
	int bpp;
	format_t format;
	BlitFunc_t blitmode;

	// Coordinates for the sprite inside the THTX
	png_uint_32 dst_x;
	png_uint_32 dst_y;
	png_uint_32 dst_stride;
	png_bytep dst_buf;

	// Coordinates for the replacement inside the PNG
	png_uint_32 rep_x;
	png_uint_32 rep_y;
	png_uint_32 rep_stride;
	png_bytep rep_buf;

	// Size of the rectangle to copy. Clamped to match the dimensions of the
	// PNG replacement in case it's smaller than the sprite.
	png_uint_32 copy_w;
	png_uint_32 copy_h;
};

// Contains all the data needed patching an image
struct img_patch_t {
	uint8_t* img_data;
	size_t img_size;
	bool was_encoded;
	format_t format;
	uint32_t stride;
	uint32_t w, h, x, y;
	const char* name;
};

/// --------------

/// Formats
/// -------
unsigned int format_Bpp(format_t format);
png_uint_32 format_png_equiv(format_t format);

// Returns the maximum alpha value (representing 100% opacity) for [format].
png_byte format_alpha_max(format_t format);
// Returns the sum of the alpha values for a number of [pixels] starting at [data].
size_t format_alpha_sum(png_bytep data, unsigned int pixels, format_t format);

// Converts a number of BGRA8888 [pixels] in [data] to the given [format] in-place.
void format_from_bgra(png_bytep data, unsigned int pixels, format_t format);
/// -------

/// ANM header patching
/// -------------------
/* Stackable modifications of non-pixel data in the ANM header can be defined
 * in <filename>.anm.jdiff. Currently, the following pieces of data can be
 * modified this way:
 *
 * • Setting the blitting mode for all sprites in this ANM:
 *   {
 *   	"blitmode": "<blitting mode>"
 *   }
 *   See BLITMODES[] in anm.cpp for more info on the supported modes.
 *
 * • Entry-specific metadata and scripts:
 *   {
 *   	"entries": {
 *   		"<Entry # in the order listed by thanm -l>": {
 *   			"name": "(different filename to be used for replacement PNGs)",
 *   			"blitmode": "<blitting mode>",
 *   			"scripts": {
 *   				"<Script # as listed by thanm -l>": {
 *
 *   All script line numbers are 0-based and are interpreted relative to the
 *   *original* script, *before* any deletions.
 *   Deletions are always applied first, followed by changes.
 *
 *   					"deletions": <line> | [<line #>, <line #>, ...]
 *
 *   Deletes one or multiple instructions from this script.
 *   For example, [0, 1, 2] would deletes the first three lines.
 *
 *   					"changes": {
 *   						"<line>#<parameter address>": "<binary hack>",
 *   						"<line>#time": <frame as a JSON integer"
 *   						...
 *
 *   Applies the given binary hack to the parameter at the given address,
 *   using thcrap binary hack syntax. (See binhack.h for ducumentation.)
 *
 *   					}
 #  				},
 #  				...
 *   		},
 *   		...
 *   	}
 *   }
 *
 * • Boundaries of existing sprites:
 *   {
 *   	"sprites": {
 *   		"<Decimal sprite # as listed by thanm -l>": [X, Y, width, height],
 *   		...
 *   	}
 *   }
 *
 * • Blitting mode of individual sprites:
 *   {
 *   	"sprites": {
 *   		"<Decimal sprite # as listed by thanm -l>": "<blitting mode>",
 *   		...
 *   	}
 *   }
 *
 * • Changing both boundaries and the blitting mode for a single sprite:
 *   {
 *   	"sprites": {
 *   		"<Decimal sprite # as listed by thanm -l>": {
 *   			"bounds": [X, Y, width, height],
 *   			"blitmode": "<blitting mode>"
 *   		},
 *   		...
 *   	}
 *   }
 */

struct script_param_change_t {
	unsigned int line;
	uint16_t param_addr;
	const char *code;
	size_t code_size;

	inline constexpr script_param_change_t(unsigned int line, uint16_t param_addr, const char* code, size_t code_size)
		: line(line), param_addr(param_addr), code(code), code_size(code_size) {}
};

struct script_time_change_t {
	unsigned int line;
	uint16_t time;

	inline constexpr script_time_change_t(unsigned int line, uint16_t time) : line(line), time(time) {}
};

class script_t {
	uint8_t *first_instr;
	uint8_t *after_last;

	template <typename T> T* _instr_at_line(unsigned int line);
	template <typename T> uint16_t _param_length_of(unsigned int line);
	template <typename T> void _delete_line(unsigned int line);
	template <typename T> void _apply_time_change(script_time_change_t change);
	template <typename T> void _apply_param_change(
		unsigned int line, uint16_t param_addr,
		const uint8_t *code, size_t code_size
	);

public:
	// Excluding the terminating instruction, as in thanm -l.
	unsigned int num_instrs;

	// Version-dependent operations
	uint16_t (script_t::*param_length_of)(unsigned int line);
	void (script_t::*delete_line)(unsigned int line);
	void (script_t::*apply_time_change)(script_time_change_t change);
	void (script_t::*apply_param_change)(
		unsigned int line, uint16_t param_addr,
		const uint8_t *code, size_t code_size
	);

	template <typename T> void init(T *first);
};

struct script_mods_t {
	uint32_t entry_num;
	int32_t script_num;
	script_t script;

	std::vector<unsigned int> deletions;
	std::vector<script_time_change_t> time_changes;
	std::vector<script_param_change_t> param_changes;
	void apply_orig();
};

struct entry_mods_t {
	// Entry number
	size_t num;

	// Applied directly to our structures, not patched into the game memory
	const char *name = nullptr;
	Option<BlitFunc_t> blitmode;

	// For parsing script mods later
	json_t *scripts = nullptr;

	// Type-checks and prepares script-specific mods for the script in
	// this specific ANM [version] at the given [offset].
	script_mods_t script_mods(uint8_t *in, anm_offset_t &offset, uint32_t version);

	void apply_ourdata(anm_entry_t &entry);
};

struct sprite_mods_t {
	// Sprite number
	size_t num;

	// Applied directly to our structures, not patched into the game memory
	Option<BlitFunc_t> blitmode;

	// Applied onto the original sprite
	Option<xywh_t> bounds;

	void apply_orig(sprite_t &orig);
};

// ANM header patching data
struct header_mods_t {
	// Number of entries/sprites seen so far. Necessary to maintain global,
	// absolute entry/sprite IDs, consistent with the output of thanm -l.
	size_t entries_seen = 0;
	size_t sprites_seen = 0;

	// Top-level JSON objects
	json_t *entries;
	json_t *sprites;
	// No Option<> here, since we want to default to "auto" (= nullptr).
	BlitFunc_t blitmode;

	// Type-checks and prepares entry-specific mods for the entry
	// with the ID <entries_seen>.
	entry_mods_t entry_mods();
	// Type-checks and prepares sprite-specific mods for the sprite
	// with the ID <sprites_seen>.
	sprite_mods_t sprite_mods();

	header_mods_t(json_t *patch);
};
/// -------------------

/// ANM structure
/// -------------
// Fills [entry] with the data of an ANM entry starting at [in], automatically
// detecting the correct source format, and applying the given ANM header [patch].
int anm_entry_init(header_mods_t& hdr_m, anm_entry_t& entry, uint8_t* in, uint8_t* in_endptr, uint8_t* out, json_t* patch);

void anm_entry_clear(anm_entry_t &entry);
/// -------------

/// Sprite-level patching
/// ---------------------
// Calculates the coordinates.
int sprite_patch_set(
	sprite_patch_t& sp,
	const img_patch_t& patch,
	const sprite_local_t& sprite,
	const png_image_ex& image
);

// Analyzes the alpha channel contents in a rectangle of size [w]*[h] in the
// bitmap [buf]. [stride] is used to jump from line to line.
sprite_alpha_t sprite_alpha_analyze(
	const png_bytep buf,
	const format_t format,
	const size_t stride,
	const png_uint_32 w,
	const png_uint_32 h
);
// Convenience functions to analyze destination and replacement sprites
sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t &sp);
sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t &sp);

// Runs the blitting function [func] on each row of [sp].
int sprite_blit(const sprite_patch_t &sp, const BlitFunc_t func);

// Runs the blitting function on the sprite data in [sp].
int sprite_patch(const sprite_patch_t &sp);

// Walks through the patch stack and patches every game-local PNG file called
// [entry->name] onto the THTX texture [entry->thtx] on sprite level, using
// the coordinates in [entry->sprites].
int stack_game_png_apply(img_patch_t& patch, std::vector<sprite_local_t>& sprites, const char* anm_fn, size_t entry_num);
/// ---------------------

/// Sprite boundary dumping
/// -----------------------
char* fn_for_bounds(const char *fn);

void bounds_init(png_image_ex &bounds, png_uint_32 w, png_uint_32 h, const char *fn);

int bounds_draw_line(
	png_image_ex &image,
	const size_t x, const size_t y,
	const size_t pixeldist, const size_t len, png_byte val
);
int bounds_draw_hline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_vline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_rect(
	png_image_ex &image, const size_t thtx_x, const size_t thtx_y, const sprite_local_t &spr
);

int bounds_store(const char *fn, png_image_ex &image);
/// -----------------------
