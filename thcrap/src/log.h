/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  * Log to both a file and, if requested, an on-screen console.
  *
  * As of now, we do not enforce char strings to be in UTF-8.
  */

#pragma once

void log_set_hook(void(*hookproc)(const char*), void(*hookproc2)(const char*, size_t));

/// ---------------
/// Standard output
/// ---------------
// Basic
void log_print(const char *text);
// Specific length
void log_nprint(const char *text, size_t n);
// Formatted
void log_vprintf(const char *text, va_list va);
void log_printf(const char *text, ...);

#ifdef _MSC_VER
# define log_func_printf(text, ...) \
	log_printf("[" __FUNCTION__ "]: "text, __VA_ARGS__)
#else
# define log_func_printf(text, ...) \
	log_printf("[%s]: " text, __func__, ##__VA_ARGS__)
#endif
/// ---------------

/// -------------
/// Message boxes
// Technically not a "logging function", but hey, it has variable arguments.
/// -------------
// Basic
int log_mbox(const char *caption, const UINT type, const char *text);
// Formatted
int log_vmboxf(const char *caption, const UINT type, const char *text, va_list va);
int log_mboxf(const char *caption, const UINT type, const char *text, ...);
// Set the owner hwnd for the log_mbox* functions
void log_mbox_set_owner(HWND hwnd);
/// -------------

void log_init(int console);
void log_exit(void);
