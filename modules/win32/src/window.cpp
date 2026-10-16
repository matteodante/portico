// Portico win32 for Emscripten: windows, the message queue and the small system calls of windows.h.
//
// One window class table and windows that all draw into the page's canvas (#canvas): the first window created sets
// the canvas size to its client area. Input events of the canvas become window messages; they are queued on the
// JavaScript side and taken by PeekMessage/GetMessage, so no JavaScript handler ever calls into the program while it
// runs. GetMessage with an empty queue gives the thread back to the browser (JSPI) until a message arrives.
#include <windows.h>
#include <mmsystem.h>

#include <emscripten.h>
#include <emscripten/em_js.h>
#include <emscripten/html5.h>

#include <pthread.h>
#include <stdio.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

struct WindowClass {
    UINT style = 0;
    WNDPROC proc = nullptr;
};

struct Window {
    std::string className;
    WNDPROC proc = nullptr;
    DWORD style = 0;
    RECT window = {0, 0, 0, 0};  // screen coordinates of the whole window
    RECT client = {0, 0, 0, 0};  // client area, from (0, 0)
    bool invalid = false;        // needs a WM_PAINT
    bool alive = true;
};

std::map<std::string, WindowClass> &Classes() {
    static std::map<std::string, WindowClass> classes;
    return classes;
}

std::vector<std::unique_ptr<Window>> &Windows() {
    static std::vector<std::unique_ptr<Window>> windows;
    return windows;
}

std::deque<MSG> &Queue() {
    static std::deque<MSG> queue;
    return queue;
}

bool g_quit = false;
int g_quitCode = 0;
HWND g_canvasWindow = nullptr;  // the window whose client area is the canvas

Window *Get(HWND hwnd) {
    for (auto &w : Windows())
        if ((HWND)w.get() == hwnd && w->alive) return w.get();
    return nullptr;
}

std::string Narrow(LPCWSTR s) {
    std::string out;
    for (; s && *s; s++) {
        unsigned c = (unsigned)*s;
        if (c < 0x80) {
            out += (char)c;
        } else if (c < 0x800) {
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        } else {
            out += (char)(0xF0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3F));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// The frame Windows 10 and 11 draw around a window at 96 DPI: so a window created with a given outer size gets the
// same client area, and back buffer, as on Windows.
RECT FrameOf(DWORD style) {
    RECT r = {0, 0, 0, 0};
    if (style & WS_THICKFRAME) { r.left = -8; r.right = 8; r.top = -8; r.bottom = 8; }
    else if (style & WS_BORDER) { r.left = -1; r.right = 1; r.top = -1; r.bottom = 1; }
    if ((style & WS_CAPTION) == WS_CAPTION) r.top -= 23;
    return r;
}

void Post(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MSG m = {};
    m.hwnd = hwnd;
    m.message = msg;
    m.wParam = wp;
    m.lParam = lp;
    m.time = timeGetTime();
    Queue().push_back(m);
}

LRESULT Send(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window *w = Get(hwnd);
    return w && w->proc ? w->proc(hwnd, msg, wp, lp) : 0;
}

}  // namespace

// ---- Input from the page --------------------------------------------------------------------------------------------
// The canvas' mouse and keyboard events, kept as (message, wParam, lParam) triples until the program takes them.
EM_JS(void, portico_w32_listen, (), {
    var canvas = document.getElementById('canvas');
    if (!canvas || Module.porticoInput) return;
    Module.porticoInput = [];
    var wake = function() { if (Module.porticoWake) { var w = Module.porticoWake; Module.porticoWake = null; w(); } };
    var push = function(msg, wp, lp) { Module.porticoInput.push(msg, wp, lp); wake(); };
    var buttons = function(e) { return (e.buttons & 1 ? 1 : 0) | (e.buttons & 2 ? 2 : 0) | (e.buttons & 4 ? 16 : 0) | (e.shiftKey ? 4 : 0) | (e.ctrlKey ? 8 : 0); };
    var pos = function(e) {
        var r = canvas.getBoundingClientRect();
        var x = Math.round((e.clientX - r.left) * canvas.width / Math.max(1, r.width));
        var y = Math.round((e.clientY - r.top) * canvas.height / Math.max(1, r.height));
        return ((y & 0xffff) << 16) | (x & 0xffff);
    };
    canvas.addEventListener('mousemove', function(e) { push(0x0200, buttons(e), pos(e)); });
    canvas.addEventListener('mousedown', function(e) { push([0x0201, 0x0207, 0x0204][e.button] || 0x0201, buttons(e), pos(e)); canvas.focus(); });
    canvas.addEventListener('mouseup', function(e) { push([0x0202, 0x0208, 0x0205][e.button] || 0x0202, buttons(e), pos(e)); });
    canvas.addEventListener('contextmenu', function(e) { e.preventDefault(); });
    canvas.addEventListener('wheel', function(e) { push(0x020A, ((-Math.sign(e.deltaY) * 120) & 0xffff) << 16, pos(e)); e.preventDefault(); }, { passive: false });
    var vk = function(e) {
        var k = e.keyCode || 0;
        return k;
    };
    canvas.tabIndex = 0;
    canvas.addEventListener('keydown', function(e) {
        push(0x0100, vk(e), 1);
        if (e.key && e.key.length === 1) push(0x0102, e.key.codePointAt(0), 1);
        if (e.key !== 'F5' && e.key !== 'F12') e.preventDefault();
    });
    canvas.addEventListener('keyup', function(e) { push(0x0101, vk(e), 0xC0000001 | 0); });
});

EM_JS(int, portico_w32_take, (UINT *msg, WPARAM *wp, LPARAM *lp), {
    var q = Module.porticoInput;
    if (!q || !q.length) return 0;
    HEAPU32[msg >> 2] = q.shift();
    HEAPU32[wp >> 2] = q.shift();
    HEAP32[lp >> 2] = q.shift();
    return 1;
});

// Gives the thread back to the browser until an input event arrives, or 100 ms pass.
EM_ASYNC_JS(void, portico_w32_wait, (), {
    await new Promise(function(resolve) {
        var timer = setTimeout(function() { Module.porticoWake = null; resolve(); }, 100);
        Module.porticoWake = function() { clearTimeout(timer); resolve(); };
    });
});

EM_JS(int, portico_w32_screen, (int which), { return which ? screen.height : screen.width; });

EM_JS(void, portico_w32_title, (const char *text), { document.title = UTF8ToString(text); });

namespace {

void PumpInput() {
    UINT msg;
    WPARAM wp;
    LPARAM lp;
    while (portico_w32_take(&msg, &wp, &lp)) Post(g_canvasWindow, msg, wp, lp);
}

// The next message for hwnd (or any window), or WM_PAINT for an invalid window once the queue is empty.
bool Next(MSG *out, HWND hwnd, UINT lo, UINT hi, bool remove) {
    PumpInput();
    auto &q = Queue();
    for (auto it = q.begin(); it != q.end(); ++it) {
        if (hwnd && it->hwnd != hwnd) continue;
        if ((lo || hi) && (it->message < lo || it->message > hi)) continue;
        *out = *it;
        if (remove) q.erase(it);
        return true;
    }
    if (g_quit) {
        MSG m = {};
        m.message = WM_QUIT;
        m.wParam = (WPARAM)g_quitCode;
        *out = m;
        if (remove) g_quit = false;
        return true;
    }
    for (auto &w : Windows()) {
        if (!w->alive || !w->invalid || (hwnd && (HWND)w.get() != hwnd)) continue;
        if ((lo || hi) && (WM_PAINT < lo || WM_PAINT > hi)) continue;
        MSG m = {};
        m.hwnd = (HWND)w.get();
        m.message = WM_PAINT;
        m.time = timeGetTime();
        *out = m;
        return true;
    }
    return false;
}

ATOM Register(const std::string &name, UINT style, WNDPROC proc) {
    if (name.empty() || Classes().count(name)) return 0;
    Classes()[name] = WindowClass{style, proc};
    return (ATOM)Classes().size();
}

HWND Create(DWORD style, const std::string &className, const std::string &title, int x, int y, int w, int h) {
    auto it = Classes().find(className);
    if (it == Classes().end()) {
        fprintf(stderr, "[portico-win32] CreateWindow: no window class \"%s\"\n", className.c_str());
        return nullptr;
    }
    if (w == CW_USEDEFAULT || w <= 0) w = 800;
    if (h == CW_USEDEFAULT || h <= 0) h = 600;
    if (x == CW_USEDEFAULT) x = 0;
    if (y == CW_USEDEFAULT) y = 0;
    RECT f = FrameOf(style);
    auto win = std::make_unique<Window>();
    win->className = className;
    win->proc = it->second.proc;
    win->style = style;
    win->window = RECT{x, y, x + w, y + h};
    LONG cw = w - (f.right - f.left), ch = h - (f.bottom - f.top);
    win->client = RECT{0, 0, cw > 1 ? cw : 1, ch > 1 ? ch : 1};
    HWND hwnd = (HWND)win.get();
    Windows().push_back(std::move(win));
    if (!g_canvasWindow) {
        g_canvasWindow = hwnd;
        Window *cw2 = Get(hwnd);
        emscripten_set_canvas_element_size("#canvas", cw2->client.right, cw2->client.bottom);
        portico_w32_listen();
        if (!title.empty()) portico_w32_title(title.c_str());
    }
    Send(hwnd, WM_CREATE, 0, 0);
    return hwnd;
}

}  // namespace

extern "C" {

// ---- Window classes and windows -------------------------------------------------------------------------------------
ATOM WINAPI RegisterClassA(const WNDCLASSA *c) { return c ? Register(c->lpszClassName ? c->lpszClassName : "", c->style, c->lpfnWndProc) : 0; }
ATOM WINAPI RegisterClassW(const WNDCLASSW *c) { return c ? Register(Narrow(c->lpszClassName), c->style, c->lpfnWndProc) : 0; }
ATOM WINAPI RegisterClassExA(const WNDCLASSEXA *c) { return c ? Register(c->lpszClassName ? c->lpszClassName : "", c->style, c->lpfnWndProc) : 0; }
ATOM WINAPI RegisterClassExW(const WNDCLASSEXW *c) { return c ? Register(Narrow(c->lpszClassName), c->style, c->lpfnWndProc) : 0; }
BOOL WINAPI UnregisterClassA(LPCSTR name, HINSTANCE) { return name && Classes().erase(name) ? TRUE : FALSE; }
BOOL WINAPI UnregisterClassW(LPCWSTR name, HINSTANCE) { return Classes().erase(Narrow(name)) ? TRUE : FALSE; }

HWND WINAPI CreateWindowExA(DWORD, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND, HMENU,
                            HINSTANCE, LPVOID) {
    return Create(style, cls ? cls : "", title ? title : "", x, y, w, h);
}

HWND WINAPI CreateWindowExW(DWORD, LPCWSTR cls, LPCWSTR title, DWORD style, int x, int y, int w, int h, HWND, HMENU,
                            HINSTANCE, LPVOID) {
    return Create(style, Narrow(cls), Narrow(title), x, y, w, h);
}

BOOL WINAPI DestroyWindow(HWND hwnd) {
    Window *w = Get(hwnd);
    if (!w) return FALSE;
    Send(hwnd, WM_DESTROY, 0, 0);
    w->alive = false;
    return TRUE;
}

BOOL WINAPI ShowWindow(HWND hwnd, int cmd) {
    Window *w = Get(hwnd);
    if (!w) return FALSE;
    if (cmd != SW_HIDE) w->invalid = true;
    return TRUE;
}

BOOL WINAPI UpdateWindow(HWND hwnd) {
    Window *w = Get(hwnd);
    if (!w) return FALSE;
    if (w->invalid) Send(hwnd, WM_PAINT, 0, 0);
    return TRUE;
}

BOOL WINAPI IsWindow(HWND hwnd) { return Get(hwnd) ? TRUE : FALSE; }

BOOL WINAPI GetClientRect(HWND hwnd, LPRECT r) {
    Window *w = Get(hwnd);
    if (!w || !r) return FALSE;
    *r = w->client;
    return TRUE;
}

BOOL WINAPI GetWindowRect(HWND hwnd, LPRECT r) {
    Window *w = Get(hwnd);
    if (!w || !r) return FALSE;
    *r = w->window;
    return TRUE;
}

BOOL WINAPI AdjustWindowRectEx(LPRECT r, DWORD style, BOOL, DWORD) {
    if (!r) return FALSE;
    RECT f = FrameOf(style);
    r->left += f.left;
    r->top += f.top;
    r->right += f.right;
    r->bottom += f.bottom;
    return TRUE;
}

BOOL WINAPI AdjustWindowRect(LPRECT r, DWORD style, BOOL menu) { return AdjustWindowRectEx(r, style, menu, 0); }

BOOL WINAPI SetWindowTextA(HWND hwnd, LPCSTR s) {
    if (hwnd == g_canvasWindow && s) portico_w32_title(s);
    return Get(hwnd) ? TRUE : FALSE;
}

BOOL WINAPI SetWindowTextW(HWND hwnd, LPCWSTR s) { return SetWindowTextA(hwnd, Narrow(s).c_str()); }

BOOL WINAPI ValidateRect(HWND hwnd, const RECT *) {
    Window *w = Get(hwnd);
    if (w) w->invalid = false;
    return w ? TRUE : FALSE;
}

BOOL WINAPI InvalidateRect(HWND hwnd, const RECT *, BOOL) {
    Window *w = Get(hwnd);
    if (w) w->invalid = true;
    return w ? TRUE : FALSE;
}

HDC WINAPI BeginPaint(HWND hwnd, LPPAINTSTRUCT ps) {
    Window *w = Get(hwnd);
    if (ps) {
        memset(ps, 0, sizeof(*ps));
        if (w) ps->rcPaint = w->client;
    }
    if (w) w->invalid = false;
    return (HDC)hwnd;
}

BOOL WINAPI EndPaint(HWND, const PAINTSTRUCT *) { return TRUE; }

// ---- Messages -------------------------------------------------------------------------------------------------------
BOOL WINAPI PeekMessageA(LPMSG msg, HWND hwnd, UINT lo, UINT hi, UINT remove) {
    return msg && Next(msg, hwnd, lo, hi, (remove & PM_REMOVE) != 0) ? TRUE : FALSE;
}

BOOL WINAPI PeekMessageW(LPMSG msg, HWND hwnd, UINT lo, UINT hi, UINT remove) { return PeekMessageA(msg, hwnd, lo, hi, remove); }

BOOL WINAPI GetMessageA(LPMSG msg, HWND hwnd, UINT lo, UINT hi) {
    if (!msg) return -1;
    while (!Next(msg, hwnd, lo, hi, true)) portico_w32_wait();
    return msg->message == WM_QUIT ? FALSE : TRUE;
}

BOOL WINAPI GetMessageW(LPMSG msg, HWND hwnd, UINT lo, UINT hi) { return GetMessageA(msg, hwnd, lo, hi); }

BOOL WINAPI TranslateMessage(const MSG *) { return FALSE; }  // WM_CHAR comes with the key event already

LRESULT WINAPI DispatchMessageA(const MSG *msg) {
    if (!msg || !msg->hwnd) return 0;
    return Send(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

LRESULT WINAPI DispatchMessageW(const MSG *msg) { return DispatchMessageA(msg); }

BOOL WINAPI PostMessageA(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) {
    Post(hwnd, m, wp, lp);
    return TRUE;
}

BOOL WINAPI PostMessageW(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) { return PostMessageA(hwnd, m, wp, lp); }
LRESULT WINAPI SendMessageA(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) { return Send(hwnd, m, wp, lp); }
LRESULT WINAPI SendMessageW(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) { return Send(hwnd, m, wp, lp); }

void WINAPI PostQuitMessage(int code) {
    g_quit = true;
    g_quitCode = code;
}

LRESULT WINAPI DefWindowProcA(HWND hwnd, UINT m, WPARAM, LPARAM) {
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        return 0;
    }
}

LRESULT WINAPI DefWindowProcW(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) { return DefWindowProcA(hwnd, m, wp, lp); }

// ---- System ---------------------------------------------------------------------------------------------------------
int WINAPI MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT) {
    fprintf(stderr, "[MessageBox] %s: %s\n", caption ? caption : "", text ? text : "");
    return IDOK;
}

int WINAPI MessageBoxW(HWND hwnd, LPCWSTR text, LPCWSTR caption, UINT type) {
    return MessageBoxA(hwnd, Narrow(text).c_str(), Narrow(caption).c_str(), type);
}

HMODULE WINAPI GetModuleHandleA(LPCSTR) { return (HMODULE)(uintptr_t)0x400000; }
HMODULE WINAPI GetModuleHandleW(LPCWSTR) { return (HMODULE)(uintptr_t)0x400000; }
void WINAPI OutputDebugStringA(LPCSTR s) { if (s) fputs(s, stderr); }
void WINAPI OutputDebugStringW(LPCWSTR s) { OutputDebugStringA(Narrow(s).c_str()); }
DWORD WINAPI GetCurrentThreadId(void) { return (DWORD)(uintptr_t)pthread_self(); }

int WINAPI GetSystemMetrics(int index) {
    switch (index) {
    case SM_CXSCREEN: return portico_w32_screen(0);
    case SM_CYSCREEN: return portico_w32_screen(1);
    default: return 0;
    }
}

void WINAPI Sleep(DWORD ms) { emscripten_sleep(ms); }

DWORD WINAPI timeGetTime(void) { return (DWORD)(int64_t)emscripten_get_now(); }
UINT WINAPI timeBeginPeriod(UINT) { return 0; }
UINT WINAPI timeEndPeriod(UINT) { return 0; }
DWORD WINAPI GetTickCount(void) { return timeGetTime(); }

BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *f) {
    if (!f) return FALSE;
    f->QuadPart = 1000000;
    return TRUE;
}

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *c) {
    if (!c) return FALSE;
    c->QuadPart = (LONGLONG)(emscripten_get_now() * 1000.0);
    return TRUE;
}

DWORD WINAPI GetEnvironmentVariableA(LPCSTR name, LPSTR buf, DWORD size) {
    const char *v = name ? getenv(name) : nullptr;
    if (!v) return 0;
    DWORD n = (DWORD)strlen(v);
    if (!buf || size <= n) return n + 1;
    memcpy(buf, v, n + 1);
    return n;
}

// UTF-8 for CP_UTF8, and for CP_ACP too: outside Windows the "ANSI" code page is UTF-8.
int WINAPI MultiByteToWideChar(UINT, DWORD, LPCCH src, int srcLen, LPWSTR dst, int dstLen) {
    if (!src) return 0;
    size_t n = srcLen < 0 ? strlen(src) + 1 : (size_t)srcLen;
    std::wstring out;
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)src[i];
        unsigned cp = c, extra = 0;
        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
        i++;
        for (unsigned k = 0; k < extra && i < n; k++, i++) cp = (cp << 6) | ((unsigned char)src[i] & 0x3F);
        out += (wchar_t)cp;
    }
    if (!dst || !dstLen) return (int)out.size();
    if ((int)out.size() > dstLen) return 0;
    wmemcpy(dst, out.data(), out.size());
    return (int)out.size();
}

int WINAPI WideCharToMultiByte(UINT, DWORD, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCCH, LPBOOL used) {
    if (used) *used = FALSE;
    if (!src) return 0;
    std::wstring in = srcLen < 0 ? std::wstring(src) + L'\0' : std::wstring(src, (size_t)srcLen);
    std::string out;
    for (wchar_t wc : in) {
        if (!wc) { out += '\0'; continue; }
        out += Narrow(std::wstring(1, wc).c_str());
    }
    if (!dst || !dstLen) return (int)out.size();
    if ((int)out.size() > dstLen) return 0;
    memcpy(dst, out.data(), out.size());
    return (int)out.size();
}

}  // extern "C"
