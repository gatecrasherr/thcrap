/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Stuff related to text rendering.
  */

#include "thcrap.h"
#include "textdisp.h"

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(CreateFont);
W32U8_DETOUR_CHAIN_DEF(CreateFontIndirectEx);
auto chain_CreateFontW = CreateFontW;
auto chain_CreateFontIndirectExW = CreateFontIndirectExW;
auto chain_SelectObject = SelectObject;

#define CHAIN(f) auto chain_##f = f
W32U8_DETOUR_CHAIN_DEF(DrawText);
W32U8_DETOUR_CHAIN_DEF(DrawTextEx);
W32U8_DETOUR_CHAIN_DEF(ExtTextOut);
W32U8_DETOUR_CHAIN_DEF(PolyTextOut);
W32U8_DETOUR_CHAIN_DEF(TabbedTextOut);
W32U8_DETOUR_CHAIN_DEF(TextOut);

CHAIN(DrawTextW);
CHAIN(DrawTextExW);
CHAIN(ExtTextOutW);
CHAIN(PolyTextOutW);
CHAIN(TabbedTextOutW);
CHAIN(TextOutW);

/// -------------
HFONT hSystemFont = NULL;
/// -------------

#define CUSTOM_SYSTEM_FONT \
	do { \
		if (GetCurrentObject(hdc, OBJ_FONT) == GetStockObject(SYSTEM_FONT)) { \
			SelectObject(hdc, hSystemFont); \
		} \
	} while(0)

int WINAPI textdisp_DrawTextW(HDC hdc, LPWSTR lpchText, int cchText, LPRECT lprc, UINT format) {
	CUSTOM_SYSTEM_FONT;
	return chain_DrawTextW(hdc, lpchText, cchText, lprc, format);
}

int WINAPI textdisp_DrawTextA(HDC hdc, LPSTR lpchText, int cchText, LPRECT lprc, UINT format) {
	CUSTOM_SYSTEM_FONT;
	return chain_DrawTextU(hdc, lpchText, cchText, lprc, format);
}

int WINAPI textdisp_DrawTextExW(HDC hdc, LPWSTR lpchText, int cchText, LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp) {
	CUSTOM_SYSTEM_FONT;
	return chain_DrawTextExW(hdc, lpchText, cchText, lprc, format, lpdtp);
}

int WINAPI textdisp_DrawTextExA(HDC hdc, LPSTR lpchText, int cchText, LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp) {
	CUSTOM_SYSTEM_FONT;
	return chain_DrawTextExU(hdc, lpchText, cchText, lprc, format, lpdtp);
}

BOOL WINAPI textdisp_ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT * lprect, LPCWSTR lpString, UINT c, const INT* lpDx) {
	CUSTOM_SYSTEM_FONT;
	return chain_ExtTextOutW(hdc, x, y, options, lprect, lpString, x, lpDx);
}

BOOL WINAPI textdisp_ExtTextOutA(HDC hdc, int x, int y, UINT options, const RECT* lprect, LPCSTR lpString, UINT c, const INT* lpDx) {
	CUSTOM_SYSTEM_FONT;
	return chain_ExtTextOutU(hdc, x, y, options, lprect, lpString, x, lpDx);
}

BOOL WINAPI textdisp_PolyTextOutW(HDC hdc, const POLYTEXTW* ppt, int nstrings) {
	CUSTOM_SYSTEM_FONT;
	return chain_PolyTextOutW(hdc, ppt, nstrings);
}

BOOL WINAPI textdisp_PolyTextOutA(HDC hdc, const POLYTEXTA* ppt, int nstrings) {
	CUSTOM_SYSTEM_FONT;
	return chain_PolyTextOutU(hdc, ppt, nstrings);
}

LONG WINAPI textdisp_TabbedTextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int chCount, int nTabPositions, const INT* lpnTabStopPositions, int nTabOrigin) {
	CUSTOM_SYSTEM_FONT;
	return chain_TabbedTextOutW(hdc, x, y, lpString, chCount, nTabPositions, lpnTabStopPositions, nTabOrigin);
}

LONG WINAPI textdisp_TabbedTextOutA(HDC hdc, int x, int y, LPCSTR lpString, int chCount, int nTabPositions, const INT* lpnTabStopPositions, int nTabOrigin) {
	CUSTOM_SYSTEM_FONT;
	return chain_TabbedTextOutU(hdc, x, y, lpString, chCount, nTabPositions, lpnTabStopPositions, nTabOrigin);
}

BOOL WINAPI textdisp_TextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int c) {
	CUSTOM_SYSTEM_FONT;
	return chain_TextOutW(hdc, x, y, lpString, c);
}

BOOL WINAPI textdisp_TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
	CUSTOM_SYSTEM_FONT;
	return chain_TextOutU(hdc, x, y, lpString, c);
}

HGDIOBJ WINAPI textdisp_SelectObject(HDC hdc, HGDIOBJ h) {
	if (!h || h == GetStockObject(SYSTEM_FONT)) {
		return chain_SelectObject(hdc, hSystemFont);
	}
	else {
		return chain_SelectObject(hdc, h);
	}
}

/// Quality enum
/// ------------
#define UNSPECIFIED_QUALITY 0xFF

static const char *QUALITY_STRINGS[] = {
	/* 0 */ "DEFAULT_QUALITY",
	/* 1 */ "DRAFT_QUALITY",
	/* 2 */ "PROOF_QUALITY",
	/* 3 */ "NONANTIALIASED_QUALITY",
	/* 4 */ "ANTIALIASED_QUALITY",
	/* 5 */ "CLEARTYPE_QUALITY",
	/* 6 */ "CLEARTYPE_NATURAL_QUALITY",
};

const char* quality_to_string(BYTE quality)
{
	if(quality < elementsof(QUALITY_STRINGS)) {
		return QUALITY_STRINGS[quality];
	}
	return "(quality out of range)";
}

BYTE string_to_quality(const char *str)
{
	BYTE ret;
	if(!str || str[0] == '\0') {
		return UNSPECIFIED_QUALITY;
	}
	for(ret = 0; ret < elementsof(QUALITY_STRINGS); ret++) {
		if(!strcmp(str, QUALITY_STRINGS[ret])) {
			return ret;
		}
	}
	log_printf("(Font) Invalid quality value, ignoring: %s\n", str);
	return UNSPECIFIED_QUALITY;
}
/// ------------

/// LOGFONT stringification
/// -----------------------
const char* logfont_stringify(const LOGFONTA *lf)
{
	char face[LF_FACESIZE + 2];
	if(!lf) {
		return NULL;
	}
	if(lf->lfFaceName[0]) {
		snprintf(face, sizeof(face), "'%s'", lf->lfFaceName);
	} else {
		snprintf(face, sizeof(face), "%u", lf->lfPitchAndFamily);
	}
	return strings_sprintf((size_t)lf, "(%s %d %d %d %s)",
		face, lf->lfHeight, lf->lfWidth, lf->lfWeight,
		quality_to_string(lf->lfQuality)
	);
}
/// -----------------------

/// Font rules
/// ----------
// Fills [arg] with the font rule argument at [str].
// Returns [str], advanced to the next argument.
const char* fontrule_arg(char *arg, size_t arg_len, const char *str)
{
	size_t i = 0;
	char delim = ' ';
	if(!arg || !arg_len || !str) {
		return str;
	}
	if(*str == '\'') {
		delim = '\'';
		str++;
	}
	while(i < (arg_len - 1) && *str && *str != delim) {
		arg[i++] = *str++;
	}
	arg[i] = '\0';
	// Skip any garbage after the argument...
	while(*str && *str != ' ') {
		str++;
	}
	// ...and skip from there to the next one.
	while(*str && *str == ' ') {
		str++;
	}
	return str;
}

int fontrule_arg_int(const char **str, int *score)
{
	int ret;
	char arg[LF_FACESIZE] = {0};
	if(!str || !*str) {
		return 0;
	}
	*str = fontrule_arg(arg, sizeof(arg), *str);
	ret = atoi(arg);
	if(score) {
		*score += ret != 0;
	}
	return ret;
}

BYTE fontrule_arg_quality(const char **str, int *score)
{
	BYTE ret;
	char arg[LF_FACESIZE] = {0};
	if(!str || !*str) {
		return UNSPECIFIED_QUALITY;
	}
	*str = fontrule_arg(arg, sizeof(arg), *str);

	ret = string_to_quality(arg);
	if(score) {
		*score += ret != UNSPECIFIED_QUALITY;
	}
	return ret;
}

int fontrule_parse(LOGFONTA *lf, const char *str)
{
	int score = 0;
	if(!lf || !str) {
		return -1;
	}
	if(*str == '\'') {
		str = fontrule_arg(lf->lfFaceName, LF_FACESIZE, str);
		score++;
	} else {
		lf->lfPitchAndFamily = fontrule_arg_int(&str, &score);
		lf->lfFaceName[0] = '\0';
	}
	lf->lfHeight = fontrule_arg_int(&str, &score);
	lf->lfWidth = fontrule_arg_int(&str, &score);
	lf->lfWeight = fontrule_arg_int(&str, &score);
	lf->lfQuality = fontrule_arg_quality(&str, &score);
	return score;
}

#define FONTRULE_MATCH(rule, dst, val, unspecified) \
	if(rule->val != unspecified && rule->val != dst->val) {\
		return 0; \
	}

#define FONTRULE_APPLY(dst, rep, val, unspecified) \
	if(rep->val != unspecified && (priority ? 1 : !dst->val)) { \
		dst->val = rep->val; \
	}

#define FONTRULE_MACRO_EXPAND(macro, p1, p2) \
	macro(p1, p2, lfPitchAndFamily, 0); \
	macro(p1, p2, lfHeight, 0); \
	macro(p1, p2, lfWidth, 0); \
	macro(p1, p2, lfWeight, 0); \
	macro(p1, p2, lfQuality, UNSPECIFIED_QUALITY);

// Returns 1 if the relevant nonzero members in [rule] match those in [dst].
int fontrule_match(const LOGFONTA *rule, const LOGFONTA *dst)
{
	if(rule->lfFaceName[0] && strncmp(
		rule->lfFaceName, dst->lfFaceName, LF_FACESIZE
	)) {
		return 0;
	}
	FONTRULE_MACRO_EXPAND(FONTRULE_MATCH, rule, dst);
	return 1;
}

// Copies all relevant nonzero members from [rep] to [dst]. If [priority] is
// zero, values are only copied if the same value is not yet set in [dst].
int fontrule_apply(LOGFONTA *dst, const LOGFONTA *rep, int priority)
{
	if(rep->lfFaceName[0] && priority ? 1 : !dst->lfFaceName[0]) {
		strncpy(dst->lfFaceName, rep->lfFaceName, LF_FACESIZE);
	}
	FONTRULE_MACRO_EXPAND(FONTRULE_APPLY, dst, rep);
	return 1;
}

// Returns 1 if a font rule was applied, 0 otherwise.
int fontrules_apply(LOGFONTA *lf)
{
	json_t *fontrules = json_object_get(runconfig_json_get(), "fontrules");
	LOGFONTA rep_full = {};
	rep_full.lfQuality = UNSPECIFIED_QUALITY;
	int rep_score = 0;
	int log_header = 0;
	const char *key;
	json_t *val;
	if(!lf) {
		return -1;
	}
	json_object_foreach_fast(fontrules, key, val) {
		LOGFONTA rule = {};
		int rule_score = fontrule_parse(&rule, key);
		int priority = rule_score >= rep_score;
		const char *rep_str = json_string_value(val);
		if(!fontrule_match(&rule, lf)) {
			continue;
		}
		if(!log_header) {
			log_printf(
				"(Font) Replacement rules applied to %s:\n",
				logfont_stringify(lf)
			);
			log_header = 1;
		}
		fontrule_parse(&rule, rep_str);
		log_printf(
			"(Font) • (%s) → (%s)%s\n",
			key, rep_str, priority ? " (priority)" : ""
		);
		fontrule_apply(&rep_full, &rule, priority);
		rep_score = MAX(rule_score, rep_score);
	}
	return fontrule_apply(lf, &rep_full, 1);
}
/// ------------------

// This detour is kept for backwards compatibility to patch configurations
// that replace multiple fonts via hardcoded string translation. Due to the
// fact that lower levels copy [pszFaceName] into the LOGFONT structure,
// it would be impossible to look up a replacement font name there.
HFONT WINAPI textdisp_CreateFontA(
	int cHeight,
	int cWidth,
	int cEscapement,
	int cOrientation,
	int cWeight,
	DWORD bItalic,
	DWORD bUnderline,
	DWORD bStrikeOut,
	DWORD iCharSet,
	DWORD iOutPrecision,
	DWORD iClipPrecision,
	DWORD iQuality,
	DWORD iPitchAndFamily,
	LPCSTR pszFaceName
)
{
	// Check hardcoded strings and the run configuration for a replacement
	// font. Hardcoded strings take priority here.
	const char *string_font = strings_lookup(pszFaceName, NULL);
	if(string_font != pszFaceName) {
		pszFaceName = string_font;
	} else {
		const json_t *run_font = json_object_get(runconfig_json_get(), "font");
		if(json_is_string(run_font)) {
			pszFaceName = json_string_value(run_font);
		}
	}
	return (HFONT)chain_CreateFontU(
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, pszFaceName
	);
}

HFONT WINAPI textdisp_CreateFontIndirectExA(
	ENUMLOGFONTEXDVA *lpelfe
)
{
	LOGFONTA *lf = NULL;
	wchar_t face_w[LF_FACESIZE];
	if(!lpelfe) {
		return NULL;
	}
	lf = &lpelfe->elfEnumLogfontEx.elfLogFont;
	// Ensure that the font face is in UTF-8
	StringToUTF16(face_w, lf->lfFaceName, LF_FACESIZE);
	StringToUTF8(lf->lfFaceName, face_w, LF_FACESIZE);
	fontrules_apply(lf);
	/**
	  * CreateFont() prioritizes [lfCharSet] and ensures that the font
	  * created can display the given charset. If the font given in
	  * [lfFaceName] is not found *or* does not claim to cover the range of
	  * codepoints used in [lfCharSet], GDI will just ignore [lfFaceName]
	  * and instead select the font that provides the closest match for
	  * [lfPitchAndFamily], out of all installed fonts that cover
	  * [lfCharSet].
	  *
	  * To ensure that we actually get the named font, we instead specify
	  * DEFAULT_CHARSET, which refers to the charset of the current system
	  * locale. Yes, there's unfortunately no DONTCARE_CHARSET, but this
	  * should be fine for most use cases.
	  *
	  * However, we only do this if we *have* a face name, either from the
	  * original code or the run configuration. If [lfFaceName] is NULL or
	  * empty, the original intention of the CreateFont() call was indeed
	  * to select the default match for [lfPitchAndFamily] and [lfCharSet].
	  * DEFAULT_CHARSET might therefore result in a different font.
	  */
	if(lf->lfFaceName[0]) {
		lf->lfCharSet = DEFAULT_CHARSET;
	}
	log_printf("(Font) Creating %s...\n", logfont_stringify(lf));
	return chain_CreateFontIndirectExU(lpelfe);
}

HFONT WINAPI textdisp_CreateFontW(
	int cHeight,
	int cWidth,
	int cEscapement,
	int cOrientation,
	int cWeight,
	DWORD bItalic,
	DWORD bUnderline,
	DWORD bStrikeOut,
	DWORD iCharSet,
	DWORD iOutPrecision,
	DWORD iClipPrecision,
	DWORD iQuality,
	DWORD iPitchAndFamily,
	LPCWSTR pszFaceName
)
{
	wchar_t* face_name_real = NULL;
	defer(SAFE_FREE(face_name_real));

	// Check hardcoded strings and the run configuration for a replacement
	// font. Hardcoded strings take priority here.
	const char* string_font = strings_lookup((char*)pszFaceName, NULL);
	if (string_font != (char*)pszFaceName) {
		face_name_real = (wchar_t*)utf8_to_utf16(string_font);
	}
	else {
		const json_t* run_font = json_object_get(runconfig_json_get(), "font");
		if (json_is_string(run_font)) {
			face_name_real = (wchar_t*)utf8_to_utf16(json_string_value(run_font));
		}
	}
	return (HFONT)chain_CreateFontW(
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, face_name_real ? face_name_real : pszFaceName
	);
}

HFONT WINAPI textdisp_CreateFontIndirectExW(
	ENUMLOGFONTEXDVW* lpelfe
)
{
	if (!lpelfe) {
		return NULL;
	}

	LOGFONTA lf = {};

	{
		static_assert(offsetof(LOGFONTA, lfFaceName) == offsetof(LOGFONTW, lfFaceName));
		memcpy(&lf, &lpelfe->elfEnumLogfontEx.elfLogFont, offsetof(LOGFONTA, lfFaceName));

		// Ensure that the font face is in UTF-8
		wchar_t* face_name = lpelfe->elfEnumLogfontEx.elfLogFont.lfFaceName;
		UTF8_DEC(face_name);
		UTF8_CONV(face_name);
		strcpy(lf.lfFaceName, face_name_utf8);
		UTF8_FREE(face_name);
	}

	fontrules_apply(&lf);
	/**
	  * CreateFont() prioritizes [lfCharSet] and ensures that the font
	  * created can display the given charset. If the font given in
	  * [lfFaceName] is not found *or* does not claim to cover the range of
	  * codepoints used in [lfCharSet], GDI will just ignore [lfFaceName]
	  * and instead select the font that provides the closest match for
	  * [lfPitchAndFamily], out of all installed fonts that cover
	  * [lfCharSet].
	  *
	  * To ensure that we actually get the named font, we instead specify
	  * DEFAULT_CHARSET, which refers to the charset of the current system
	  * locale. Yes, there's unfortunately no DONTCARE_CHARSET, but this
	  * should be fine for most use cases.
	  *
	  * However, we only do this if we *have* a face name, either from the
	  * original code or the run configuration. If [lfFaceName] is NULL or
	  * empty, the original intention of the CreateFont() call was indeed
	  * to select the default match for [lfPitchAndFamily] and [lfCharSet].
	  * DEFAULT_CHARSET might therefore result in a different font.
	  */
	if (lf.lfFaceName[0]) {
		lf.lfCharSet = DEFAULT_CHARSET;
	}
	log_printf("(Font) Creating %s...\n", logfont_stringify(&lf));
	memcpy(&lpelfe->elfEnumLogfontEx.elfLogFont, &lf, offsetof(LOGFONTW, lfFaceName));
	return chain_CreateFontIndirectExW(lpelfe);
}

void patch_fonts_load(const patch_t *patch_info)
{
	for (size_t i = 0; patch_info->fonts && patch_info->fonts[i]; i++) {
		size_t font_size;
		void *font_buffer = patch_file_load(patch_info, patch_info->fonts[i], &font_size);

		if(font_buffer) {
			DWORD ret;
			log_printf("(Font) Loading %s (%zu bytes)...\n", patch_info->fonts[i], font_size);
			AddFontMemResourceEx(font_buffer, font_size, NULL, &ret);
			free(font_buffer);
			/**
			  * "However, when the process goes away, the system will unload the fonts
			  * even if the process did not call RemoveFontMemResource."
			  * http://msdn.microsoft.com/en-us/library/windows/desktop/dd183325%28v=vs.85%29.aspx
			  */
		}
	}
}

void textdisp_load_system_font() {
	const char* legacy_fontname;

	if (legacy_fontname = json_string_value(strings_get("System"))) {
		goto checked_legacy_font_name;
	}
	if (legacy_fontname = json_object_get_string(runconfig_json_get(), "font")) {
		goto checked_legacy_font_name;
	}

checked_legacy_font_name:
	ENUMLOGFONTEXDVA elfe = {};
#define lf elfe.elfEnumLogfontEx.elfLogFont

	GetObjectA(GetStockObject(SYSTEM_FONT), sizeof(lf), &lf);

	if (legacy_fontname) {
		strncpy(lf.lfFaceName, legacy_fontname, sizeof(lf.lfFaceName));
	}

	fontrules_apply(&lf);
#undef lf

	hSystemFont = chain_CreateFontIndirectExU(&elfe);
}

extern "C" {

TH_EXPORT void textdisp_mod_detour(void)
{
	detour_chain("gdi32.dll", 1,
		"CreateFontA", textdisp_CreateFontA, &chain_CreateFontU,
		"CreateFontIndirectExA", textdisp_CreateFontIndirectExA, &chain_CreateFontIndirectExU,
		"CreateFontW", textdisp_CreateFontW, &chain_CreateFontW,
		"CreateFontIndirectExW", textdisp_CreateFontIndirectExW, &chain_CreateFontIndirectExW,
		"SelectObject", textdisp_SelectObject, &chain_SelectObject,

		"DrawTextW", textdisp_DrawTextW, & chain_DrawTextW,
		"DrawTextExW", textdisp_DrawTextExW, & chain_DrawTextExW,
		"ExtTextOutW", textdisp_ExtTextOutW, & chain_ExtTextOutW,
		"PolyTextOutW", textdisp_PolyTextOutW, & chain_PolyTextOutW,
		"TabbedTextOutW", textdisp_TabbedTextOutW, & chain_TabbedTextOutW,
		"TextOutW", textdisp_TextOutW, & chain_TextOutW,

		"DrawTextA", textdisp_DrawTextA, & chain_DrawTextU,
		"DrawTextExA", textdisp_DrawTextExA, & chain_DrawTextExU,
		"ExtTextOutA", textdisp_ExtTextOutA, & chain_ExtTextOutU,
		"PolyTextOutA", textdisp_PolyTextOutA, & chain_PolyTextOutU,
		"TabbedTextOutA", textdisp_TabbedTextOutA, & chain_TabbedTextOutU,
		"TextOutA", textdisp_TextOutA, &chain_TextOutU,
		
		NULL
	);

	textdisp_load_system_font();
}

TH_EXPORT void textdisp_mod_init(void)
{
	stack_foreach([](const patch_t *patch, void*) {
		patch_fonts_load(patch);
	}, nullptr);
}

}
