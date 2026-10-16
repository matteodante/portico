// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Shared internals of Portico D3D9 (Direct3D 9 on GLES3).
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d9.h>

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#ifndef __EMSCRIPTEN__
#include <GLES2/gl2ext_angle.h>
#endif

#ifndef GL_BUFFER
#define GL_BUFFER 0x82E0  // object kind for Device::DeleteGLLater (GL ES 3.2 enum)
#endif
// WEBGL_compressed_texture_s3tc / EXT_texture_compression_s3tc (same values as the ANGLE names).
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE
#define GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE 0x83F3
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <pthread.h>
#include <strings.h>
#endif

#ifdef __EMSCRIPTEN__
extern "C" const GUID IID_IUnknown, IID_IDirect3D9, IID_IDirect3DDevice9, IID_IDirect3DResource9,
    IID_IDirect3DBaseTexture9, IID_IDirect3DTexture9, IID_IDirect3DVertexBuffer9, IID_IDirect3DIndexBuffer9,
    IID_IDirect3DSurface9, IID_IDirect3DVertexDeclaration9;
#endif

namespace d3d9gl {

void Log(const char *fmt, ...);
void InstallCrashLog();

// The few OS services the layer itself needs; Win32 natively, POSIX/Emscripten in the browser.
#ifdef __EMSCRIPTEN__
inline uintptr_t CurrentThreadId() { return (uintptr_t) pthread_self(); }
inline bool GetEnv(const char *name, char *buf, size_t size) {
    const char *v = getenv(name);
    if (!v || !*v) return false;
    snprintf(buf, size, "%s", v);
    return true;
}
inline int StrICmp(const char *a, const char *b) { return strcasecmp(a, b); }
#else
inline uintptr_t CurrentThreadId() { return (uintptr_t) GetCurrentThreadId(); }
inline bool GetEnv(const char *name, char *buf, size_t size) {
    return GetEnvironmentVariableA(name, buf, (DWORD) size) != 0;
}
inline int StrICmp(const char *a, const char *b) { return _stricmp(a, b); }
#endif
// Size of the drawing surface of a window (client rect natively, canvas in the browser).
void GetWindowSize(HWND hwnd, UINT &w, UINT &h);
// Logs a message once per call site (for unimplemented paths hit every frame).
// CPU copies held by the layer, in bytes, by kind (PORTICO_D3D9_STATS=<presents> logs them).
enum CpuKind { CPU_TEX_MANAGED, CPU_TEX_DEFAULT, CPU_TEX_SYSTEM, CPU_SURFACE, CPU_VB_STATIC, CPU_VB_DYNAMIC,
               CPU_IB_STATIC, CPU_IB_DYNAMIC, CPU_KINDS };
// Accesses to a CPU copy after its contents went to GL, by kind of access.
enum TouchKind { TOUCH_TEX_LOCK, TOUCH_TEX_COPY, TOUCH_BUF_STATIC, TOUCH_BUF_DYNAMIC, TOUCH_BUF_READONLY,
                 TOUCH_LATE_GL, TOUCH_LATE_OTHER, TOUCH_BUF_UNREADABLE, TOUCH_KINDS };
// A late access: to a resource untouched for more than this many presents.
constexpr unsigned LATE_PRESENTS = 120;
extern std::atomic<unsigned> g_presentCount;
void CpuBytes(int kind, int64_t delta);
void Touched(int kind, const char *fmt, ...);  // counts, and logs the first few of each kind
void LogStats(unsigned presents);
// Live GL objects the layer made, by kind (logged with the CPU copies), to see growth in long sessions.
enum GLObjKind { GLO_TEXTURE, GLO_BUFFER, GLO_RENDERBUFFER, GLO_FRAMEBUFFER, GLO_SAMPLER, GLO_PROGRAM, GLO_KINDS };
void GLObjects(int kind, int delta);

// PORTICO_D3D9_PROFILE=1: CPU time of the GL thread in the layer's draw, clear, texture upload and present
// paths, logged every 5 s as ms per frame (uploads also count inside the draw that triggers them).
enum ProfKind { PROF_DRAW, PROF_CLEAR, PROF_UPLOAD, PROF_PRESENT, PROF_KINDS };
bool ProfileOn();
double NowMs();
void ProfileAdd(int kind, double ms);
void ProfileFrame();  // at every present
struct ProfileScope {
    int kind;
    double start;
    explicit ProfileScope(int k) : kind(k), start(ProfileOn() ? NowMs() : 0.0) {}
    ~ProfileScope() { if (start) ProfileAdd(kind, NowMs() - start); }
};

#define D3D9GL_ONCE(...) do { static bool s_done = false; if (!s_done) { s_done = true; ::d3d9gl::Log(__VA_ARGS__); } } while (0)

class Device;

// Base for every COM object of the layer: reference count plus a link to the device.
template <class I>
class Unknown : public I {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG) ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG n = --m_ref;
        if (n == 0) Destroy();
        return (ULONG) n;
    }

protected:
    virtual ~Unknown() {}
    virtual void Destroy() { delete this; }
    std::atomic<LONG> m_ref{1};
};

// --- Formats -------------------------------------------------------------------------------

struct FormatInfo {
    D3DFORMAT d3d;
    UINT bpp;            // bits per pixel (for block formats: per pixel of a 4x4 block)
    bool block;          // DXT
    bool depth;
    GLenum internal;     // GL internal format (0 = unsupported)
    GLenum format;       // upload format
    GLenum type;         // upload type
    bool renderable;
};

const FormatInfo *GetFormatInfo(D3DFORMAT fmt);
UINT GetPitch(D3DFORMAT fmt, UINT width);       // bytes per row (per block row for DXT)
UINT GetRows(D3DFORMAT fmt, UINT height);       // rows (block rows for DXT)
UINT GetLevelSize(D3DFORMAT fmt, UINT width, UINT height);
// Converts a level from D3D memory layout to the layout GL expects (BGRA -> RGBA etc.).
// Returns false when the data can be uploaded as is.
bool ConvertForUpload(D3DFORMAT fmt, const uint8_t *src, UINT width, UINT height, std::vector<uint8_t> &out);
// Converts RGBA8 rows read back from GL to the D3D format.
void ConvertFromRGBA8(D3DFORMAT fmt, const uint8_t *rgba, UINT width, UINT height, uint8_t *dst, UINT dstPitch);

// --- GL context ----------------------------------------------------------------------------

struct GLCaps {
    bool s3tc = false;          // DXT1/3/5 (EXT_texture_compression_s3tc or the WebGL equivalents)
    bool anisotropic = false;
    float maxAnisotropy = 1.0f;
    GLint maxTextureSize = 4096;
    GLint maxUBOSize = 16384;
};

// What the device caps report, whatever the GL context (d3d9.cpp, Direct3D::GetDeviceCaps).
constexpr UINT kCapsMaxAnisotropy = 16;
constexpr UINT kCapsMaxTextureSize = 8192;

bool CreateGLContext(HWND hwnd, int interval);
void DestroyGLContext();
bool SwapGLBuffers();          // false when the context is lost
bool IsContextLost();
void NotifyContextLost();      // once, after the device saw the loss
void AfterPresent();           // GL thread, after Present, without the device mutex
void SetSwapInterval(int interval);
void GetDrawableSize(int &w, int &h);
const GLCaps &GetGLCaps();

}  // namespace d3d9gl
