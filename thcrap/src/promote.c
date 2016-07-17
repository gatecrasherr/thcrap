/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 detour function level promotion.
  */

#include "thcrap.h"
#include "promote.h"

/// CreateFont() family (promoting to CreateFontIndirectExA)
/// -------------------
HFONT WINAPI promote_CreateFontA(
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
	CreateFontIndirectA_type *target = (CreateFontIndirectA_type*)detour_top(
		"gdi32.dll", "CreateFontIndirectA", (FARPROC)CreateFontIndirectU
	);
	return lower_CreateFontA(target,
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision,
		iClipPrecision, iQuality, iPitchAndFamily, pszFaceName
	);
}

HFONT WINAPI promote_CreateFontIndirectA(
	CONST LOGFONTA *lplf
)
{
	CreateFontIndirectExA_type *target = (CreateFontIndirectExA_type*)detour_top(
		"gdi32.dll", "CreateFontIndirectExA", (FARPROC)CreateFontIndirectExU
	);
	return lower_CreateFontIndirectA(target, lplf);
}
/// -------------------

void promote_mod_init(void)
{
	detour_chain("gdi32.dll", 0,
		"CreateFontIndirectA", promote_CreateFontIndirectA,
		"CreateFontA", promote_CreateFontA,
		NULL
	);
}
