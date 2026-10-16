// Portico win32: the strsafe.h string functions, over the C library (always truncating, always terminated).
#ifndef PORTICO_STRSAFE_H
#define PORTICO_STRSAFE_H
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#define STRSAFE_E_INSUFFICIENT_BUFFER ((HRESULT)0x8007007AL)

static inline HRESULT StringCchCopyA(LPSTR d, size_t n, LPCSTR s) {
    if (!n) return E_INVALIDARG;
    size_t l = strlen(s);
    size_t c = l < n ? l : n - 1;
    memcpy(d, s, c);
    d[c] = 0;
    return l < n ? S_OK : STRSAFE_E_INSUFFICIENT_BUFFER;
}
static inline HRESULT StringCchCopyW(LPWSTR d, size_t n, LPCWSTR s) {
    if (!n) return E_INVALIDARG;
    size_t l = wcslen(s);
    size_t c = l < n ? l : n - 1;
    wmemcpy(d, s, c);
    d[c] = 0;
    return l < n ? S_OK : STRSAFE_E_INSUFFICIENT_BUFFER;
}
static inline HRESULT StringCchCatA(LPSTR d, size_t n, LPCSTR s) {
    size_t l = strnlen(d, n);
    return l >= n ? E_INVALIDARG : StringCchCopyA(d + l, n - l, s);
}
static inline HRESULT StringCchCatW(LPWSTR d, size_t n, LPCWSTR s) {
    size_t l = wcsnlen(d, n);
    return l >= n ? E_INVALIDARG : StringCchCopyW(d + l, n - l, s);
}
static inline HRESULT StringCchPrintfA(LPSTR d, size_t n, LPCSTR f, ...) {
    va_list a;
    va_start(a, f);
    int r = vsnprintf(d, n, f, a);
    va_end(a);
    return r >= 0 && (size_t) r < n ? S_OK : STRSAFE_E_INSUFFICIENT_BUFFER;
}
static inline HRESULT StringCchPrintfW(LPWSTR d, size_t n, LPCWSTR f, ...) {
    va_list a;
    va_start(a, f);
    int r = vswprintf(d, n, f, a);
    va_end(a);
    return r >= 0 && (size_t) r < n ? S_OK : STRSAFE_E_INSUFFICIENT_BUFFER;
}

#ifdef UNICODE
#define StringCchCopy StringCchCopyW
#define StringCchCat StringCchCatW
#define StringCchPrintf StringCchPrintfW
#else
#define StringCchCopy StringCchCopyA
#define StringCchCat StringCchCatA
#define StringCchPrintf StringCchPrintfA
#endif
#endif
