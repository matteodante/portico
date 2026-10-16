// Portico win32: the subset of the Windows API that Portico's modules and portable Direct3D 9 programs need, for
// builds that are not Windows (Emscripten). Types, structures and constants have the layouts and values that the
// Windows documentation gives; the functions are implemented in modules/win32/src.
//
// _WIN32 is deliberately not defined: the C and C++ standard libraries would switch to their Windows code.
#ifndef PORTICO_WINDOWS_H
#define PORTICO_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef __cplusplus
#define PORTICO_EXTERN_C extern "C"
#define PORTICO_BEGIN_C extern "C" {
#define PORTICO_END_C }
#else
#define PORTICO_EXTERN_C extern
#define PORTICO_BEGIN_C
#define PORTICO_END_C
#endif

// ---- Calling conventions and annotations: no meaning outside Windows -----------------------------------------------
#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define STDAPICALLTYPE
#define STDMETHODCALLTYPE
#define STDMETHODVCALLTYPE
#define __stdcall
#define __cdecl
#define FAR
#define NEAR
#define IN
#define OUT
#define OPTIONAL
#define CONST const
#define VOID void
#define DECLSPEC_NOVTABLE
#define DECLSPEC_UUID(x)
#define DECLSPEC_SELECTANY __attribute__((weak))
#define DECLSPEC_IMPORT
#define DECLSPEC_EXPORT

// Names of the anonymous unions and structures in Windows headers (empty: the members are reached directly), and
// the MinGW-w64 headers' marker for 32-bit LONG constants.
#define DUMMYUNIONNAME
#define DUMMYUNIONNAME2
#define DUMMYUNIONNAME3
#define DUMMYUNIONNAME4
#define DUMMYUNIONNAME5
#define DUMMYSTRUCTNAME
#define DUMMYSTRUCTNAME2
#define DUMMYSTRUCTNAME3
#define DUMMYSTRUCTNAME4
#define DUMMYSTRUCTNAME5
#define __MSABI_LONG(x) x

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define MAX_PATH 260

// ---- Base types (ILP32 and LP64 both keep LONG and DWORD at 32 bits, as on Windows) ---------------------------------
typedef unsigned char BYTE, UCHAR, *PBYTE, *LPBYTE, BOOLEAN;
typedef unsigned short WORD, USHORT, *PWORD, *LPWORD;
typedef uint32_t DWORD, ULONG, UINT32, *PDWORD, *LPDWORD, *PULONG;
typedef int32_t LONG, INT32, *PLONG, *LPLONG;
typedef int BOOL, INT, WINBOOL, *PBOOL, *LPBOOL, *PINT, *LPINT;
typedef unsigned int UINT, *PUINT;
typedef short SHORT;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef float FLOAT;
typedef double DOUBLE;
typedef int64_t LONGLONG, INT64;
typedef uint64_t ULONGLONG, UINT64, DWORD64;
typedef intptr_t INT_PTR, LONG_PTR, SSIZE_T;
typedef uintptr_t UINT_PTR, ULONG_PTR, DWORD_PTR, SIZE_T;
typedef void *PVOID, *LPVOID, *HANDLE;
typedef const void *LPCVOID;
typedef LONG HRESULT;
typedef LONG_PTR LPARAM, LRESULT;
typedef UINT_PTR WPARAM;
typedef WORD ATOM;
typedef DWORD COLORREF;

typedef CHAR *LPSTR, *PSTR;
typedef const CHAR *LPCSTR, *PCSTR;
typedef WCHAR *LPWSTR, *PWSTR;
typedef const WCHAR *LPCWSTR, *PCWSTR;
typedef LPCSTR LPCCH;
typedef CHAR *LPCH;

#ifdef UNICODE
typedef WCHAR TCHAR;
typedef LPWSTR LPTSTR;
typedef LPCWSTR LPCTSTR;
#define __TEXT(s) L##s
#else
typedef CHAR TCHAR;
typedef LPSTR LPTSTR;
typedef LPCSTR LPCTSTR;
#define __TEXT(s) s
#endif
#define TEXT(s) __TEXT(s)

#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HKEY);
typedef HICON HCURSOR;
typedef HINSTANCE HMODULE;
typedef void *HGDIOBJ;

// ---- Small helpers ----------------------------------------------------------------------------------------------------
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define ZeroMemory(p, n) memset((p), 0, (n))
#define SecureZeroMemory(p, n) memset((p), 0, (n))
#define FillMemory(p, n, v) memset((p), (v), (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define UNREFERENCED_PARAMETER(p) (void)(p)

// ---- HRESULT ----------------------------------------------------------------------------------------------------------
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_POINTER ((HRESULT)0x80004003L)
#define E_ABORT ((HRESULT)0x80004004L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)

// ---- Structures -------------------------------------------------------------------------------------------------------
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;
typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;
typedef struct _RGNDATAHEADER { DWORD dwSize, iType, nCount, nRgnSize; RECT rcBound; } RGNDATAHEADER, *PRGNDATAHEADER;
typedef struct _RGNDATA { RGNDATAHEADER rdh; char Buffer[1]; } RGNDATA, *PRGNDATA, *LPRGNDATA;
typedef struct _LUID { DWORD LowPart; LONG HighPart; } LUID, *PLUID;
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSA {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
    HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;
typedef struct tagWNDCLASSW {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
    HBRUSH hbrBackground; LPCWSTR lpszMenuName, lpszClassName;
} WNDCLASSW, *LPWNDCLASSW;
typedef struct tagWNDCLASSEXA {
    UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon;
    HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; HICON hIconSm;
} WNDCLASSEXA, *LPWNDCLASSEXA;
typedef struct tagWNDCLASSEXW {
    UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon;
    HCURSOR hCursor; HBRUSH hbrBackground; LPCWSTR lpszMenuName, lpszClassName; HICON hIconSm;
} WNDCLASSEXW, *LPWNDCLASSEXW;

typedef struct tagPAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore, fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT, *LPPAINTSTRUCT;

// ---- GUID and COM -----------------------------------------------------------------------------------------------------
#include <guiddef.h>
#include <unknwn.h>

// ---- Window, message and dialog constants -----------------------------------------------------------------------------
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define CS_CLASSDC 0x0040

#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_VISIBLE 0x10000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_EX_APPWINDOW 0x00040000L
#define CW_USEDEFAULT ((int)0x80000000)

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10

#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_ACTIVATEAPP 0x001C
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_USER 0x0400

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_YESNO 0x00000004L
#define MB_ICONERROR 0x00000010L
#define MB_ICONWARNING 0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28

#define CP_ACP 0
#define CP_UTF8 65001

// ---- Functions (modules/win32/src) -------------------------------------------------------------------------------------
PORTICO_BEGIN_C

// The program's entry point, as on Windows: portico_win32_main provides main() and calls the one the program defines.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd);

ATOM WINAPI RegisterClassA(const WNDCLASSA *lpWndClass);
ATOM WINAPI RegisterClassW(const WNDCLASSW *lpWndClass);
ATOM WINAPI RegisterClassExA(const WNDCLASSEXA *lpWndClass);
ATOM WINAPI RegisterClassExW(const WNDCLASSEXW *lpWndClass);
BOOL WINAPI UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
BOOL WINAPI UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance);
HWND WINAPI CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                            int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
HWND WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y,
                            int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
BOOL WINAPI DestroyWindow(HWND hWnd);
BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow);
BOOL WINAPI UpdateWindow(HWND hWnd);
BOOL WINAPI IsWindow(HWND hWnd);
BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
BOOL WINAPI AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
BOOL WINAPI SetWindowTextA(HWND hWnd, LPCSTR lpString);
BOOL WINAPI SetWindowTextW(HWND hWnd, LPCWSTR lpString);
BOOL WINAPI ValidateRect(HWND hWnd, const RECT *lpRect);
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
HDC WINAPI BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL WINAPI EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);

BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL WINAPI PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL WINAPI GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL WINAPI TranslateMessage(const MSG *lpMsg);
LRESULT WINAPI DispatchMessageA(const MSG *lpMsg);
LRESULT WINAPI DispatchMessageW(const MSG *lpMsg);
BOOL WINAPI PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void WINAPI PostQuitMessage(int nExitCode);
LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
int WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
HMODULE WINAPI GetModuleHandleW(LPCWSTR lpModuleName);
void WINAPI OutputDebugStringA(LPCSTR lpOutputString);
void WINAPI OutputDebugStringW(LPCWSTR lpOutputString);
DWORD WINAPI GetCurrentThreadId(void);
int WINAPI GetSystemMetrics(int nIndex);
void WINAPI Sleep(DWORD dwMilliseconds);
DWORD WINAPI GetTickCount(void);
BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
DWORD WINAPI GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
int WINAPI MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr,
                               int cchWideChar);
int WINAPI WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr,
                               int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);

PORTICO_END_C

#define CreateWindowA(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExA(0, c, n, s, x, y, w, h, p, m, i, l)
#define CreateWindowW(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExW(0, c, n, s, x, y, w, h, p, m, i, l)

#ifdef UNICODE
#define WNDCLASS WNDCLASSW
#define WNDCLASSEX WNDCLASSEXW
#define RegisterClass RegisterClassW
#define RegisterClassEx RegisterClassExW
#define UnregisterClass UnregisterClassW
#define CreateWindowEx CreateWindowExW
#define CreateWindow CreateWindowW
#define SetWindowText SetWindowTextW
#define PeekMessage PeekMessageW
#define GetMessage GetMessageW
#define DispatchMessage DispatchMessageW
#define PostMessage PostMessageW
#define SendMessage SendMessageW
#define DefWindowProc DefWindowProcW
#define MessageBox MessageBoxW
#define GetModuleHandle GetModuleHandleW
#define OutputDebugString OutputDebugStringW
#else
#define WNDCLASS WNDCLASSA
#define WNDCLASSEX WNDCLASSEXA
#define RegisterClass RegisterClassA
#define RegisterClassEx RegisterClassExA
#define UnregisterClass UnregisterClassA
#define CreateWindowEx CreateWindowExA
#define CreateWindow CreateWindowA
#define SetWindowText SetWindowTextA
#define PeekMessage PeekMessageA
#define GetMessage GetMessageA
#define DispatchMessage DispatchMessageA
#define PostMessage PostMessageA
#define SendMessage SendMessageA
#define DefWindowProc DefWindowProcA
#define MessageBox MessageBoxA
#define GetModuleHandle GetModuleHandleA
#define OutputDebugString OutputDebugStringA
#endif

#include <mmsystem.h>

#endif  // PORTICO_WINDOWS_H
