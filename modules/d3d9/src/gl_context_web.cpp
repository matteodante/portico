// WebGL2 context of the browser build (Emscripten), the counterpart of gl_context.cpp.
//
// The thread that creates the device owns the context; no GL call ever waits for another thread. Two setups work:
//   - the program on the page's main thread (the default): the context is the page's #canvas;
//   - the program on a worker (-sPROXY_TO_PTHREAD, portico_d3d9_worker): #canvas is transferred to the worker as an
//     OffscreenCanvas (-sOFFSCREENCANVAS_SUPPORT, -sOFFSCREENCANVASES_TO_PTHREAD) and the context is created there,
//     with proxying to the main thread disallowed. Programs that block (Sleep, waits on a loader thread) need this.
//
// Presentation: the browser shows the canvas when the owning thread returns to its event loop. A program written for
// Windows never does (its loop runs forever), so after every Present this thread waits for the next animation frame
// through JSPI: the WebAssembly stack is suspended, the browser shows the frame and runs its events (input, network,
// timers), and Present returns. Build with -DPORTICO_D3D9_EXPLICIT_SWAP to present through
// emscripten_webgl_commit_frame instead, or -DPORTICO_D3D9_BITMAP_PRESENT (with --pre-js web/present.js) to hand
// each frame to the page as an ImageBitmap from a worker that never yields.
#include "common.h"

#ifdef __EMSCRIPTEN__

#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include <emscripten/em_js.h>
#include <emscripten/em_asm.h>

namespace d3d9gl {

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_context = 0;
static GLCaps g_caps;

// Test hook: a page-side BroadcastChannel('portico-d3d9-test') message 'lose-context' loses the context for real
// (WEBGL_lose_context), handled when the owning thread yields. Works on a worker too, where the page cannot reach the
// context itself.
EM_JS(void, portico_d3d9_install_test_hook, (), {
    var gl = GL.currentContext && GL.currentContext.GLctx;
    if (!gl || typeof BroadcastChannel === 'undefined') return;
    var ch = new BroadcastChannel('portico-d3d9-test');
    ch.onmessage = function(e) {
        if (e.data !== 'lose-context') return;
        var ext = gl.getExtension('WEBGL_lose_context');
        console.warn('[portico-d3d9] test: losing the WebGL context' + (ext ? "" : " (no WEBGL_lose_context)"));
        if (ext) ext.loseContext();
    };
});

bool CreateGLContext(HWND, int) {
    if (g_context) return true;
    EmscriptenWebGLContextAttributes a;
    emscripten_webgl_init_context_attributes(&a);
    a.majorVersion = 2;
    a.minorVersion = 0;
    a.alpha = EM_FALSE;
    a.depth = EM_FALSE;       // the back buffer is our own framebuffer, like in the native build
    a.stencil = EM_FALSE;
    a.antialias = EM_FALSE;
    a.premultipliedAlpha = EM_FALSE;
    a.preserveDrawingBuffer = EM_FALSE;
    a.enableExtensionsByDefault = EM_FALSE;
    a.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
    a.renderViaOffscreenBackBuffer = EM_FALSE;
#ifdef PORTICO_D3D9_EXPLICIT_SWAP
    a.explicitSwapControl = EM_TRUE;
#endif
    g_context = emscripten_webgl_create_context("#canvas", &a);
    if (g_context <= 0) {
        Log("emscripten_webgl_create_context failed %d", (int) g_context);
        g_context = 0;
        return false;
    }
    emscripten_webgl_make_context_current(g_context);
    g_caps.s3tc = emscripten_webgl_enable_extension(g_context, "WEBGL_compressed_texture_s3tc") != 0;
    g_caps.anisotropic = emscripten_webgl_enable_extension(g_context, "EXT_texture_filter_anisotropic") != 0;
    if (g_caps.anisotropic) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_caps.maxAnisotropy);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g_caps.maxTextureSize);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &g_caps.maxUBOSize);
    Log("WebGL2: %s, s3tc %d anisotropic %d (%.0f), %s thread", (const char *) glGetString(GL_RENDERER), g_caps.s3tc,
        g_caps.anisotropic, g_caps.maxAnisotropy, emscripten_is_main_browser_thread() ? "main" : "worker");
    portico_d3d9_install_test_hook();
    return true;
}

void DestroyGLContext() {
    if (g_context) emscripten_webgl_destroy_context(g_context);
    g_context = 0;
}

#ifdef PORTICO_D3D9_BITMAP_PRESENT
// Present without yielding: the drawing buffer becomes an ImageBitmap that goes to the page thread, which shows it in
// a "bitmaprenderer" canvas (web/present.js). Works while the worker never returns to its event loop.
EM_JS(void, portico_d3d9_present_bitmap, (), {
    var gl = GL.currentContext && GL.currentContext.GLctx;
    if (!gl || !gl.canvas.transferToImageBitmap) return;
    if (!Module.porticoD3D9Present) Module.porticoD3D9Present = new BroadcastChannel('portico-d3d9-present');
    Module.porticoD3D9Present.postMessage(gl.canvas.transferToImageBitmap());
});
#endif

bool SwapGLBuffers() {
#if defined(PORTICO_D3D9_BITMAP_PRESENT)
    portico_d3d9_present_bitmap();
#elif defined(PORTICO_D3D9_EXPLICIT_SWAP)
    emscripten_webgl_commit_frame();  // no-op where WebGL contexts have no commit() (Chrome)
#endif
    return !emscripten_is_webgl_context_lost(g_context);
}

// Runs on the page thread: the page's Module.onWebGLContextLost() decides what to do; without one, the page reloads
// after logging why.
void NotifyContextLost() {
    MAIN_THREAD_ASYNC_EM_ASM({
        console.error('[portico-d3d9] WebGL context lost: drawing stops');
        if (typeof Module['onWebGLContextLost'] === 'function') Module['onWebGLContextLost']();
        else location.reload();
    });
}

bool IsContextLost() { return g_context && emscripten_is_webgl_context_lost(g_context); }

}  // namespace d3d9gl

#if !defined(PORTICO_D3D9_BITMAP_PRESENT) && !defined(PORTICO_D3D9_EXPLICIT_SWAP)
// Resolves at the thread's next animation frame, or after 100 ms when there is none (a hidden page), so the program
// keeps running and answering its network peers.
EM_ASYNC_JS(void, portico_d3d9_wait_frame, (), {
    await new Promise(function(resolve) {
        var timer = setTimeout(resolve, 100);
        if (typeof requestAnimationFrame === 'function')
            requestAnimationFrame(function() { clearTimeout(timer); resolve(); });
    });
});

// A platform layer with its own frame pacing defines this; Portico's default waits for the next animation frame.
extern "C" __attribute__((weak)) void portico_present_frame(void) { portico_d3d9_wait_frame(); }
#endif

namespace d3d9gl {

// After a Present on the GL thread, with the device mutex released (other threads keep going while this one waits).
void AfterPresent() {
#if !defined(PORTICO_D3D9_BITMAP_PRESENT) && !defined(PORTICO_D3D9_EXPLICIT_SWAP)
    portico_present_frame();
#endif
}

void SetSwapInterval(int) {}

void GetDrawableSize(int &w, int &h) { emscripten_webgl_get_drawing_buffer_size(g_context, &w, &h); }

const GLCaps &GetGLCaps() { return g_caps; }

}  // namespace d3d9gl

#endif  // __EMSCRIPTEN__
