// EGL context on ANGLE, restricted to WebGL2 (EGL_ANGLE_create_context_webgl_compatibility):
// extensions are off until requested, so only what WebGL2 offers in Chrome is usable here too.
#include "common.h"

#ifndef __EMSCRIPTEN__

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>


namespace d3d9gl {

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static GLCaps g_caps;

static bool HasExtension(const char *list, const char *name) {
    if (!list) return false;
    size_t n = strlen(name);
    for (const char *p = strstr(list, name); p; p = strstr(p + 1, name))
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || p[n] == 0)) return true;
    return false;
}

// WebGL extensions the layer relies on, by their ANGLE names.
static void RequestExtensions() {
    auto request = (PFNGLREQUESTEXTENSIONANGLEPROC) eglGetProcAddress("glRequestExtensionANGLE");
    const char *requestable = (const char *) glGetString(GL_REQUESTABLE_EXTENSIONS_ANGLE);
    const char *wanted[] = {
        // WEBGL_compressed_texture_s3tc
        "GL_EXT_texture_compression_dxt1",
        "GL_ANGLE_texture_compression_dxt3",
        "GL_ANGLE_texture_compression_dxt5",
        // EXT_texture_filter_anisotropic
        "GL_EXT_texture_filter_anisotropic",
    };
    for (const char *ext : wanted) {
        if (request && HasExtension(requestable, ext)) request(ext);
        else Log("extension %s not requestable", ext);
    }

    const char *exts = (const char *) glGetString(GL_EXTENSIONS);
    g_caps.s3tc = HasExtension(exts, "GL_EXT_texture_compression_dxt1") &&
                  HasExtension(exts, "GL_ANGLE_texture_compression_dxt3") &&
                  HasExtension(exts, "GL_ANGLE_texture_compression_dxt5");
    g_caps.anisotropic = HasExtension(exts, "GL_EXT_texture_filter_anisotropic");
    if (g_caps.anisotropic) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_caps.maxAnisotropy);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g_caps.maxTextureSize);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &g_caps.maxUBOSize);
    Log("GL_RENDERER %s", (const char *) glGetString(GL_RENDERER));
    Log("GL_VERSION %s", (const char *) glGetString(GL_VERSION));
    Log("s3tc %d anisotropic %d (%.0f) maxTex %d maxUBO %d", g_caps.s3tc, g_caps.anisotropic,
        g_caps.maxAnisotropy, g_caps.maxTextureSize, g_caps.maxUBOSize);
}

bool CreateGLContext(HWND hwnd, int interval) {
    if (g_context != EGL_NO_CONTEXT) return true;

    auto getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!getPlatformDisplay) {
        Log("eglGetPlatformDisplayEXT missing");
        return false;
    }

    // Backend: D3D11 like Chrome on Windows; PORTICO_ANGLE_BACKEND=vulkan|gl|d3d11 to change it.
    EGLint type = EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
    char backend[32] = {0};
    if (GetEnv("PORTICO_ANGLE_BACKEND", backend, sizeof(backend))) {
        if (!StrICmp(backend, "vulkan")) type = EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
        else if (!StrICmp(backend, "gl")) type = EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE;
    }
    const EGLint displayAttribs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, type, EGL_NONE};
    g_display = getPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, (void *) EGL_DEFAULT_DISPLAY, displayAttribs);
    EGLint major = 0, minor = 0;
    if (g_display == EGL_NO_DISPLAY || !eglInitialize(g_display, &major, &minor)) {
        Log("eglInitialize failed 0x%x", eglGetError());
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE};
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(g_display, configAttribs, &config, 1, &count) || count < 1) {
        Log("eglChooseConfig failed 0x%x", eglGetError());
        return false;
    }

    g_surface = eglCreateWindowSurface(g_display, config, (EGLNativeWindowType) hwnd, nullptr);
    if (g_surface == EGL_NO_SURFACE) {
        Log("eglCreateWindowSurface failed 0x%x", eglGetError());
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_TRUE,
        EGL_NONE};
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, contextAttribs);
    if (g_context == EGL_NO_CONTEXT) {
        Log("eglCreateContext (WebGL compatibility) failed 0x%x", eglGetError());
        return false;
    }
    if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
        Log("eglMakeCurrent failed 0x%x", eglGetError());
        return false;
    }
    Log("EGL %d.%d, WebGL compatibility context on HWND %p", major, minor, hwnd);
    SetSwapInterval(interval);
    RequestExtensions();
    return true;
}

void DestroyGLContext() {
    if (g_display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
    if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
    eglTerminate(g_display);
    g_display = EGL_NO_DISPLAY;
    g_surface = EGL_NO_SURFACE;
    g_context = EGL_NO_CONTEXT;
}

bool SwapGLBuffers() {
    if (eglSwapBuffers(g_display, g_surface)) return true;
    EGLint err = eglGetError();
    Log("eglSwapBuffers failed 0x%x", err);
    return err != EGL_CONTEXT_LOST;
}

// The device has logged the loss and stopped drawing; the window stays, frozen on the last frame.
void NotifyContextLost() {}

// Natively the loss shows at eglSwapBuffers (EGL_CONTEXT_LOST).
bool IsContextLost() { return false; }

// eglSwapBuffers already presented the frame.
void AfterPresent() {}

void SetSwapInterval(int interval) {
    if (g_display != EGL_NO_DISPLAY) eglSwapInterval(g_display, interval);
}

void GetDrawableSize(int &w, int &h) {
    EGLint ew = 0, eh = 0;
    eglQuerySurface(g_display, g_surface, EGL_WIDTH, &ew);
    eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &eh);
    w = ew;
    h = eh;
}

const GLCaps &GetGLCaps() { return g_caps; }

}  // namespace d3d9gl

#endif  // !__EMSCRIPTEN__
