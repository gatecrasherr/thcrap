/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory patching and Import Address Table detouring.
  */

#pragma once

// IsBadReadPtr() without the flawed implementation.
// Returns TRUE if [ptr] points to at least [len] bytes of valid memory in the
// address space of the current process.
THCRAP_API BOOL VirtualCheckRegion(const void *ptr, const size_t len);
THCRAP_API BOOL VirtualCheckCode(const void *ptr);

// Writes [len] bytes from [new] to [ptr] in the address space of the current
// or another process if the current value in [ptr] equals [prev].
// Returns TRUE on success, FALSE on failure.
int PatchRegion(void *ptr, const void *Prev, const void *New, size_t len);
int PatchRegionEx(HANDLE hProcess, void *ptr, const void *Prev, const void *New, size_t len);

// If the current value in [ptr] equals [Prev], copies [len] bytes from [ptr] to [CpyBuf]
// and then writes [len] bytes from [New] to [ptr].
// Returns [CpyBuf] on success, NULL on failure.
void* PatchRegionCopySrc(void *ptr, const void *Prev, const void *New, void *CpyBuf, size_t len);

/// Import Address Table patching
/// =============================

// Information about a single function to detour
typedef struct {
	const char *old_func;
	const void *old_ptr;
	const void *new_ptr;
} iat_detour_t;

// Sets up [detour] using the most appropriate low-level detouring function.
int iat_detour_func(HMODULE hMod, PIMAGE_IMPORT_DESCRIPTOR pImpDesc, const iat_detour_t *detour);
/// ----------

/// Detour chaining
/// ---------------
/**
  * This system allows the support of an arbitrary number of detour hooks
  * for any given API function.
  *
  * Prior to this, detours weren't really reliably extensible by plugins.
  * Since they mostly need to call the original function they hook, they used
  * to imply a certain hierarchy and required knowledge of the entire
  * detour sequence (with the win32_utf8 functions mostly being on the lowest
  * level) in order to work as intended. This could have led to cases where
  * newer detours could have potentially override existing functionality
  * (see https://bitbucket.org/nmlgc/thpatch-bugs/issue/18).
  *
  * It took a while to come up with a proper solution to this problem that
  * wouldn't impose any restrictions on what detour functions are able to do.
  * After all, they may need to call the original function multiple times,
  * or pass locally constructed data as a parameter.
  *
  * After some further thought, it then became clear that, for most reasonable
  * cases where detours only modify function parameters or return values, the
  * order in which they are called doesn't actually matter. Sure, win32_utf8
  * must be at the bottom, but as an integral part of the engine, it can
  * always be treated separately.
  *
  * This realization allows us to build a "detour chaining" mechanism, similar
  * to the interrupt chaining used in MS-DOS TSR applications. Instead of
  * patching the IAT directly with some function pointer, modules first
  * register sequences of detours for each API function in advance, using
  * detour_chain().
  * For every detoured function, detour_chain() records the pointer to the
  * last hook registered. Adding a new hook by calling detour_chain() again
  * for the same DLL and function then returns this pointer, before replacing
  * it with the new one.
  *
  * Detours then use their returned chaining pointer to invoke the original
  * functionality anywhere they need it. As a result, this calls all
  * subsequent hooks in the chain recursively.
  * By initalizing its chaining pointer to point to the original API function,
  * every link in the chain also defaults to well-defined, reliable end point.
  *
  * To apply the chain, we then simply patch the IAT with the last recorded
  * function pointers, once we have a module handle.
  */

void detour_disable(const char* dll_name, const char* func_name);

// Defines a function pointer used to continue a detour chain. It is
// initialized to [func], which should be the default function to be called
// if this is the last link in the chain. The pointer will be of the type
// "[func]_type", which must have been typedef'd before.
#define DETOUR_CHAIN_DEF(func) \
	static func##_type *chain_##func = func;

// DETOUR_CHAIN_DEF for wrappers defined in win32_utf8.
#define W32U8_DETOUR_CHAIN_DEF(func) \
	static func##A_type *chain_##func##U = func##U;

#define DETOUR_CHAIN_RETURN_OLD_POINTERS 1
/**
  * Inserts a new hook for a number of functions in [dll_name] at the
  * beginning of their respective detour chains. For each function,
  * 3 or 2 (if (flags & 1) == 0) additional parameters of the form
  *
  *	"exported name", new_func_ptr, (&old_func_ptr,)
  *	"exported name", new_func_ptr, (&old_func_ptr,)
  *	...,
  *
  * are consumed.
  * If ([flags] & 1) is nonzero, [old_func_ptr] is set to the next
  * function pointer in the chain, or left unchanged if no hook was
  * registered before.
  */
THCRAP_API int detour_chain(const char *dll_name, int flags, ...);
/**
  * detour_chain() for the function pair list of a single DLL returned by
  * win32_utf8. Does obviously not return any pointers to functions that may
  * have been part of the respective chains before.
  */
THCRAP_INTERNAL_API int detour_chain_w32u8(const w32u8_dll_t *dll);

// Using a double pointer for [old_func] because you want both vtable_detour()
// to provide the pointer, and the correct function pointer type in your usage
// code.
typedef struct {
	// # of this function in the vtable. *Not* the byte offset.
	const size_t index;

	void *new_func;

	// Set this to the address of your own function pointer with the correct
	// type. Filled out by vtable_detour(). Can also be a nullptr.
	void **old_func;
} vtable_detour_t;

// Applies the detours to the [vtable] and fills in the original function
// pointers, if necessary according to [det->old_func]:
// •  old_func == NULL: Always write to [vtable]
// • *old_func == NULL: Write new_func to [vtable] and set *old_func to its
//                      previous value
// • *old_func != NULL: Do nothing
// Returns the number of functions detoured.
THCRAP_API size_t vtable_detour(void **vtable, const vtable_detour_t *det, size_t det_count);

// Returns a pointer to the first function in a specific detour chain, or
// [fallback] if no hook has been registered so far.
THCRAP_API FARPROC detour_top(const char *dll_name, const char *func_name, FARPROC fallback);

// Applies the cached detours to [hMod].
THCRAP_API int iat_detour_apply(HMODULE hMod);
/// ---------------


// x64 manipulation of virtual memory
// that can be accessed with absolute encodings
//
// On x32 these are exact duplicates of
// the regular virtual memory functions
#if TH_X64
THCRAP_API LPVOID VirtualAllocLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
THCRAP_API LPVOID VirtualAllocLow(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
THCRAP_API BOOL VirtualFreeLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
THCRAP_API BOOL VirtualFreeLow(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
#else
static TH_FORCEINLINE LPVOID VirtualAllocLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	return VirtualAllocEx(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
}
static TH_FORCEINLINE LPVOID VirtualAllocLow(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	return VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
}
static TH_FORCEINLINE BOOL VirtualFreeLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	return VirtualFreeEx(hProcess, lpAddress, dwSize, dwFreeType);
}
static TH_FORCEINLINE BOOL VirtualFreeLow(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	return VirtualFree(lpAddress, dwSize, dwFreeType);
}
#endif

/// =============================
