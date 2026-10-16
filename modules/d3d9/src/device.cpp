// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// IDirect3DDevice9 on GLES3: state, threading, resources, render targets, Present.
// Drawing (vertex input, fixed-function programs, GL state) is in device_draw.cpp.
#include "device.h"
#include "d3d9_impl.h"
#include "ffp.h"

#include <algorithm>

namespace d3d9gl {

Device *g_device = nullptr;

static const D3DMATRIX kIdentity = {{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}};

static DWORD F2DW(float f) {
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

void State::SetDefaults(bool zenable) {
    memset(rs, 0, sizeof(rs));
    rs[D3DRS_ZENABLE] = zenable ? D3DZB_TRUE : D3DZB_FALSE;
    rs[D3DRS_FILLMODE] = D3DFILL_SOLID;
    rs[D3DRS_SHADEMODE] = D3DSHADE_GOURAUD;
    rs[D3DRS_ZWRITEENABLE] = TRUE;
    rs[D3DRS_LASTPIXEL] = TRUE;
    rs[D3DRS_SRCBLEND] = D3DBLEND_ONE;
    rs[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
    rs[D3DRS_CULLMODE] = D3DCULL_CCW;
    rs[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
    rs[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_FOGEND] = F2DW(1.0f);
    rs[D3DRS_FOGDENSITY] = F2DW(1.0f);
    rs[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_STENCILFAIL] = rs[D3DRS_STENCILZFAIL] = rs[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
    rs[D3DRS_STENCILMASK] = rs[D3DRS_STENCILWRITEMASK] = 0xFFFFFFFF;
    rs[D3DRS_TEXTUREFACTOR] = 0xFFFFFFFF;
    rs[D3DRS_CLIPPING] = TRUE;
    rs[D3DRS_LIGHTING] = TRUE;
    rs[D3DRS_COLORVERTEX] = TRUE;
    rs[D3DRS_LOCALVIEWER] = TRUE;
    rs[D3DRS_DIFFUSEMATERIALSOURCE] = D3DMCS_COLOR1;
    rs[D3DRS_SPECULARMATERIALSOURCE] = D3DMCS_COLOR2;
    rs[D3DRS_AMBIENTMATERIALSOURCE] = D3DMCS_MATERIAL;
    rs[D3DRS_EMISSIVEMATERIALSOURCE] = D3DMCS_MATERIAL;
    rs[D3DRS_POINTSIZE] = F2DW(1.0f);
    rs[D3DRS_POINTSIZE_MIN] = F2DW(1.0f);
    rs[D3DRS_POINTSCALE_A] = F2DW(1.0f);
    rs[D3DRS_POINTSIZE_MAX] = F2DW(64.0f);
    rs[D3DRS_MULTISAMPLEANTIALIAS] = TRUE;
    rs[D3DRS_MULTISAMPLEMASK] = 0xFFFFFFFF;
    rs[D3DRS_COLORWRITEENABLE] = rs[D3DRS_COLORWRITEENABLE1] = rs[D3DRS_COLORWRITEENABLE2] = rs[D3DRS_COLORWRITEENABLE3] = 0xF;
    rs[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
    rs[D3DRS_CCW_STENCILFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_CCW_STENCILFAIL] = rs[D3DRS_CCW_STENCILZFAIL] = rs[D3DRS_CCW_STENCILPASS] = D3DSTENCILOP_KEEP;
    rs[D3DRS_BLENDFACTOR] = 0xFFFFFFFF;
    rs[D3DRS_SRCBLENDALPHA] = D3DBLEND_ONE;
    rs[D3DRS_DESTBLENDALPHA] = D3DBLEND_ZERO;
    rs[D3DRS_BLENDOPALPHA] = D3DBLENDOP_ADD;

    memset(tss, 0, sizeof(tss));
    for (int i = 0; i < kStages; i++) {
        tss[i][D3DTSS_COLOROP] = i == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE;
        tss[i][D3DTSS_ALPHAOP] = i == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
        tss[i][D3DTSS_COLORARG1] = tss[i][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
        tss[i][D3DTSS_COLORARG2] = tss[i][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
        tss[i][D3DTSS_COLORARG0] = tss[i][D3DTSS_ALPHAARG0] = tss[i][D3DTSS_RESULTARG] = D3DTA_CURRENT;
        tss[i][D3DTSS_TEXCOORDINDEX] = i;
        tss[i][D3DTSS_TEXTURETRANSFORMFLAGS] = D3DTTFF_DISABLE;
    }
    memset(samp, 0, sizeof(samp));
    for (int i = 0; i < kSamplers; i++) {
        samp[i][D3DSAMP_ADDRESSU] = samp[i][D3DSAMP_ADDRESSV] = samp[i][D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
        samp[i][D3DSAMP_MAGFILTER] = samp[i][D3DSAMP_MINFILTER] = D3DTEXF_POINT;
        samp[i][D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
        samp[i][D3DSAMP_MAXANISOTROPY] = 1;
    }
    view = proj = kIdentity;
    for (D3DMATRIX &m : tex) m = kIdentity;
    for (D3DMATRIX &m : world) m = kIdentity;
    memset(&material, 0, sizeof(material));
    lights.clear();
    enabledLights.clear();
    fvf = 0;
}

const D3DLIGHT9 *State::FindLight(DWORD index) const {
    for (const auto &l : lights)
        if (l.first == index) return &l.second;
    return nullptr;
}

// --- Construction --------------------------------------------------------------------------

Device::Device(Direct3D *d3d, HWND hwnd, DWORD behavior, const D3DPRESENT_PARAMETERS &pp)
    : m_d3d(d3d), m_hwnd(hwnd), m_behavior(behavior), m_pp(pp), m_glThread(CurrentThreadId()) {
    m_d3d->AddRef();
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++) (&m_gamma.red)[c][i] = (WORD) (i * 257);
}

Device::~Device() {}

bool Device::Init() {
    if (!m_pp.hDeviceWindow) m_pp.hDeviceWindow = m_hwnd;
    UINT ww = 0, wh = 0;
    GetWindowSize(m_hwnd, ww, wh);
    if (!m_pp.BackBufferWidth) m_pp.BackBufferWidth = std::max(1u, ww);
    if (!m_pp.BackBufferHeight) m_pp.BackBufferHeight = std::max(1u, wh);
    if (m_pp.BackBufferFormat == D3DFMT_UNKNOWN) m_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    if (!m_pp.BackBufferCount) m_pp.BackBufferCount = 1;

    int interval = m_pp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ? 0 : 1;
    if (!CreateGLContext(m_hwnd, interval)) return false;
    const GLCaps &gc = GetGLCaps();
    if (!gc.anisotropic || gc.maxAnisotropy < (float) kCapsMaxAnisotropy || gc.maxTextureSize < (GLint) kCapsMaxTextureSize)
        Log("warning: the GL context offers anisotropy %d (%.0f) and textures up to %d, less than the device caps report "
            "(%u, %u)", gc.anisotropic, gc.maxAnisotropy, gc.maxTextureSize, kCapsMaxAnisotropy, kCapsMaxTextureSize);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    ResetBindingCache();
    glGenBuffers(1, &m_upVB);
    glGenBuffers(1, &m_upIB);
    glGenBuffers(1, &m_bonesUBO);
    GLObjects(GLO_BUFFER, 3);
    glBindBuffer(GL_UNIFORM_BUFFER, m_bonesUBO);
    glBufferData(GL_UNIFORM_BUFFER, 768 * 16, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_bonesUBO);

    if (!CreateBackBuffer()) return false;
    ResetStates();
    g_device = this;
    Log("device %ux%u fmt %u depth %d/%u interval %d", m_pp.BackBufferWidth, m_pp.BackBufferHeight,
        m_pp.BackBufferFormat, m_pp.EnableAutoDepthStencil, m_pp.AutoDepthStencilFormat, interval);
    return true;
}

bool Device::CreateBackBuffer() {
    if (m_backBuffer) m_backBuffer->Release();
    if (m_autoDepth) m_autoDepth->Release();
    m_backBuffer = new Surface(this, SurfaceKind::RenderTarget, m_pp.BackBufferWidth, m_pp.BackBufferHeight,
                               m_pp.BackBufferFormat, D3DPOOL_DEFAULT);
    m_autoDepth = nullptr;
    if (m_pp.EnableAutoDepthStencil)
        m_autoDepth = new Surface(this, SurfaceKind::DepthStencil, m_pp.BackBufferWidth, m_pp.BackBufferHeight,
                                  m_pp.AutoDepthStencilFormat, D3DPOOL_DEFAULT);
    return true;
}

void Device::ReleaseState(State &s) {
    for (auto &t : s.textures) if (t) { t->Release(); t = nullptr; }
    for (auto &st : s.streams) if (st.vb) { st.vb->Release(); st.vb = nullptr; }
    if (s.indices) { s.indices->Release(); s.indices = nullptr; }
    if (s.decl) { s.decl->Release(); s.decl = nullptr; }
    if (s.rt) { s.rt->Release(); s.rt = nullptr; }
    if (s.depth) { s.depth->Release(); s.depth = nullptr; }
}

// Both copies back to the D3D9 defaults (CreateDevice and Reset).
void Device::ResetStates() {
    ReleaseState(m_gl);
    memset(m_api.textures, 0, sizeof(m_api.textures));
    memset(m_api.streams, 0, sizeof(m_api.streams));
    m_api.indices = nullptr;
    m_api.decl = nullptr;
    m_api.vs = nullptr;
    m_api.ps = nullptr;
    m_gl.vs = nullptr;
    m_gl.ps = nullptr;
    bool z = m_pp.EnableAutoDepthStencil != FALSE;
    m_api.SetDefaults(z);
    m_gl.SetDefaults(z);
    D3DVIEWPORT9 vp = {0, 0, m_pp.BackBufferWidth, m_pp.BackBufferHeight, 0.0f, 1.0f};
    m_api.viewport = m_gl.viewport = vp;
    m_api.rt = m_backBuffer;
    m_api.depth = m_autoDepth;
    m_gl.rt = m_backBuffer;
    m_backBuffer->AddRef();
    m_gl.depth = m_autoDepth;
    if (m_autoDepth) m_autoDepth->AddRef();
    m_scissor = {0, 0, (LONG) m_pp.BackBufferWidth, (LONG) m_pp.BackBufferHeight};
    for (uint32_t &g : m_gen) g++;
    m_worldHigh = 1;
    InvalidateFixedGL();
    m_boundFBO = ~0u;
}

// --- Threading -----------------------------------------------------------------------------

template <class F>
void Device::Exec(F &&fn) {
    if (OnGLThread()) {
        if (m_queued) Drain();
        fn();
    } else {
        Defer(std::function<void()>(std::forward<F>(fn)));
    }
}

void Device::Defer(std::function<void()> fn) {
    m_queue.push_back(std::move(fn));
    m_queued = true;
}

void Device::ResetBindingCache() {
    m_boundArray = m_boundElement = ~0u;
    m_activeUnit = ~0u;
    for (GLuint &t : m_unitTexture) t = ~0u;
    for (GLuint &s : m_unitSampler) s = ~0u;
    for (AttribPointer &a : m_attrib) a.buffer = ~0u;
    m_glViewportSet = false;
}

void Device::BindBufferGL(GLenum target, GLuint name) {
    GLuint *bound = target == GL_ARRAY_BUFFER ? &m_boundArray : target == GL_ELEMENT_ARRAY_BUFFER ? &m_boundElement : nullptr;
    if (bound && *bound == name) return;
    glBindBuffer(target, name);
    if (bound) *bound = name;
}

void Device::ActiveTextureGL(GLuint unit) {
    if (m_activeUnit == unit) return;
    glActiveTexture(GL_TEXTURE0 + unit);
    m_activeUnit = unit;
}

void Device::BindTextureGL(GLuint name) {
    if (m_activeUnit > (GLuint) kStages) ActiveTextureGL(kStages);
    if (m_unitTexture[m_activeUnit] == name) return;
    glBindTexture(GL_TEXTURE_2D, name);
    m_unitTexture[m_activeUnit] = name;
}

// GL unbinds a deleted buffer or texture everywhere (including the attribute pointers of the
// bound vertex array); the cache must not believe it is still bound, since GL reuses names.
void Device::ForgetGL(GLenum kind, GLuint name) {
    if (kind == GL_BUFFER) {
        if (m_boundArray == name) m_boundArray = ~0u;
        if (m_boundElement == name) m_boundElement = ~0u;
        for (AttribPointer &a : m_attrib)
            if (a.buffer == name) a.buffer = ~0u;
    } else if (kind == GL_TEXTURE) {
        for (GLuint &t : m_unitTexture)
            if (t == name) t = ~0u;
    }
}

void Device::DeleteGLLater(GLenum kind, GLuint name) {
    if (OnGLThread()) {
        ForgetGL(kind, name);
        switch (kind) {
        case GL_TEXTURE: glDeleteTextures(1, &name); GLObjects(GLO_TEXTURE, -1); break;
        case GL_BUFFER: glDeleteBuffers(1, &name); GLObjects(GLO_BUFFER, -1); break;
        case GL_RENDERBUFFER: glDeleteRenderbuffers(1, &name); GLObjects(GLO_RENDERBUFFER, -1); break;
        }
        if (kind != GL_BUFFER)
            for (auto it = m_fbos.begin(); it != m_fbos.end();) {
                // FBO keys hold the attachment names (see GetFramebuffer).
                if ((GLuint) (it->first >> 32) == name || (GLuint) (it->first & 0xFFFFFFFF) == name) {
                    glDeleteFramebuffers(1, &it->second);
                    GLObjects(GLO_FRAMEBUFFER, -1);
                    it = m_fbos.erase(it);
                    m_boundFBO = ~0u;
                } else {
                    ++it;
                }
            }
        return;
    }
    m_deadGL.emplace_back(kind, name);
    m_queued = true;
}

void Device::Drain() {
    if (!m_queue.empty()) {
        std::vector<std::function<void()>> q;
        q.swap(m_queue);
        m_skipDraws = m_queuedPresents > 1;
        for (auto &fn : q) fn();
        m_skipDraws = false;
    }
    if (!m_deadGL.empty()) {
        auto dead = std::move(m_deadGL);
        m_deadGL.clear();
        for (auto &d : dead) DeleteGLLater(d.first, d.second);
    }
    m_queued = !m_queue.empty() || !m_deadGL.empty();
}

void Device::Service() {
    if (!OnGLThread()) return;
    std::lock_guard<std::recursive_mutex> lk(m_mx);
    if (m_queued) Drain();
}

#define LOCK() std::lock_guard<std::recursive_mutex> lk_(m_mx)

// Takes a reference for the queued call; released after it ran.
template <class T>
static T *Hold(T *p) {
    if (p) p->AddRef();
    return p;
}
template <class T>
static void Drop(T *p) {
    if (p) p->Release();
}
template <class T>
static void Assign(T *&slot, T *p) {
    if (p) p->AddRef();
    if (slot) slot->Release();
    slot = p;
}

// --- IUnknown / misc -----------------------------------------------------------------------

HRESULT Device::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9) {
        AddRef();
        *ppv = static_cast<IDirect3DDevice9 *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

HRESULT Device::TestCooperativeLevel() { return D3D_OK; }

UINT Device::GetAvailableTextureMem() { return 2047u * 1024u * 1024u; }

HRESULT Device::GetDirect3D(IDirect3D9 **ppD3D9) {
    if (!ppD3D9) return D3DERR_INVALIDCALL;
    m_d3d->AddRef();
    *ppD3D9 = m_d3d;
    return D3D_OK;
}

HRESULT Device::GetDeviceCaps(D3DCAPS9 *pCaps) { return m_d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, pCaps); }

HRESULT Device::GetDisplayMode(UINT, D3DDISPLAYMODE *pMode) { return m_d3d->GetAdapterDisplayMode(0, pMode); }

HRESULT Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *p) {
    if (!p) return D3DERR_INVALIDCALL;
    p->AdapterOrdinal = 0;
    p->DeviceType = D3DDEVTYPE_HAL;
    p->hFocusWindow = m_hwnd;
    p->BehaviorFlags = m_behavior;
    return D3D_OK;
}

HRESULT Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *, IDirect3DSwapChain9 **) {
    D3D9GL_ONCE("CreateAdditionalSwapChain not supported");
    return D3DERR_NOTAVAILABLE;
}

HRESULT Device::GetSwapChain(UINT, IDirect3DSwapChain9 **) {
    D3D9GL_ONCE("GetSwapChain not supported");
    return D3DERR_NOTAVAILABLE;
}

HRESULT Device::GetRasterStatus(UINT, D3DRASTER_STATUS *p) {
    if (!p) return D3DERR_INVALIDCALL;
    p->InVBlank = FALSE;
    p->ScanLine = 0;
    return D3D_OK;
}

void Device::SetGammaRamp(UINT, DWORD, CONST D3DGAMMARAMP *pRamp) {
    if (pRamp) m_gamma = *pRamp;
}

void Device::GetGammaRamp(UINT, D3DGAMMARAMP *pRamp) {
    if (pRamp) *pRamp = m_gamma;
}

HRESULT Device::Reset(D3DPRESENT_PARAMETERS *pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    if (!OnGLThread()) return D3DERR_INVALIDCALL;
    if (m_queued) Drain();
    m_pp = *pp;
    UINT ww = 0, wh = 0;
    GetWindowSize(m_hwnd, ww, wh);
    if (!m_pp.BackBufferWidth) m_pp.BackBufferWidth = std::max(1u, ww);
    if (!m_pp.BackBufferHeight) m_pp.BackBufferHeight = std::max(1u, wh);
    if (m_pp.BackBufferFormat == D3DFMT_UNKNOWN) m_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    ReleaseState(m_gl);
    CreateBackBuffer();
    ResetStates();
    SetSwapInterval(m_pp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ? 0 : 1);
    Log("reset %ux%u", m_pp.BackBufferWidth, m_pp.BackBufferHeight);
    return D3D_OK;
}

// --- Resource creation (any thread, CPU objects only) ----------------------------------------

HRESULT Device::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                              IDirect3DTexture9 **ppTexture, HANDLE *) {
    if (!ppTexture || !Width || !Height) return D3DERR_INVALIDCALL;
    if (!GetFormatInfo(Format)) {
        Log("CreateTexture: unsupported format %u", Format);
        return D3DERR_INVALIDCALL;
    }
    LOCK();
    *ppTexture = new Texture(this, Width, Height, Levels, Usage, Format, Pool);
    return D3D_OK;
}

HRESULT Device::CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9 **, HANDLE *) {
    D3D9GL_ONCE("CreateVolumeTexture not supported");
    return D3DERR_NOTAVAILABLE;
}

HRESULT Device::CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9 **, HANDLE *) {
    D3D9GL_ONCE("CreateCubeTexture not supported");
    return D3DERR_NOTAVAILABLE;
}

HRESULT Device::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **pp, HANDLE *) {
    if (!pp || !Length) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = new VertexBuffer(this, Length, Usage, FVF, Pool);
    return D3D_OK;
}

HRESULT Device::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **pp, HANDLE *) {
    if (!pp || !Length) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = new IndexBuffer(this, Length, Usage, Format, Pool);
    return D3D_OK;
}

HRESULT Device::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, DWORD, BOOL,
                                   IDirect3DSurface9 **pp, HANDLE *) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = new Surface(this, SurfaceKind::RenderTarget, Width, Height, Format, D3DPOOL_DEFAULT);
    return D3D_OK;
}

HRESULT Device::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, DWORD, BOOL,
                                          IDirect3DSurface9 **pp, HANDLE *) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = new Surface(this, SurfaceKind::DepthStencil, Width, Height, Format, D3DPOOL_DEFAULT);
    return D3D_OK;
}

HRESULT Device::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **pp, HANDLE *) {
    if (!pp || !Width || !Height) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = new Surface(this, SurfaceKind::Offscreen, Width, Height, Format, Pool);
    return D3D_OK;
}

HRESULT Device::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl) {
    if (!pVertexElements || !ppDecl) return D3DERR_INVALIDCALL;
    *ppDecl = new VertexDecl(this, pVertexElements, 0);
    return D3D_OK;
}

// Vertex and pixel shaders are refused: the caps report no shader model, and programs written for
// fixed-function hardware use the fixed-function pipeline (vertex blending included).
HRESULT Device::CreateVertexShader(CONST DWORD *, IDirect3DVertexShader9 **ppShader) {
    if (ppShader) *ppShader = nullptr;
    D3D9GL_ONCE("CreateVertexShader refused (fixed-function only)");
    return D3DERR_INVALIDCALL;
}

HRESULT Device::CreatePixelShader(CONST DWORD *, IDirect3DPixelShader9 **ppShader) {
    if (ppShader) *ppShader = nullptr;
    D3D9GL_ONCE("CreatePixelShader refused (fixed-function only)");
    return D3DERR_INVALIDCALL;
}

HRESULT Device::SetVertexShader(IDirect3DVertexShader9 *pShader) {
    LOCK();
    m_api.vs = pShader;
    if (pShader) D3D9GL_ONCE("SetVertexShader with a shader ignored");
    return D3D_OK;
}

HRESULT Device::GetVertexShader(IDirect3DVertexShader9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    *pp = nullptr;
    return D3D_OK;
}

HRESULT Device::SetPixelShader(IDirect3DPixelShader9 *pShader) {
    LOCK();
    m_api.ps = pShader;
    if (pShader) D3D9GL_ONCE("SetPixelShader with a shader ignored");
    return D3D_OK;
}

HRESULT Device::GetPixelShader(IDirect3DPixelShader9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    *pp = nullptr;
    return D3D_OK;
}

HRESULT Device::CreateStateBlock(D3DSTATEBLOCKTYPE, IDirect3DStateBlock9 **) {
    D3D9GL_ONCE("state blocks not supported");
    return D3DERR_NOTAVAILABLE;
}
HRESULT Device::BeginStateBlock() { return D3DERR_NOTAVAILABLE; }
HRESULT Device::EndStateBlock(IDirect3DStateBlock9 **) { return D3DERR_NOTAVAILABLE; }

HRESULT Device::ValidateDevice(DWORD *pNumPasses) {
    if (pNumPasses) *pNumPasses = 1;
    return D3D_OK;
}

// --- State setters -------------------------------------------------------------------------

HRESULT Device::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    if ((UINT) State >= 256) return D3DERR_INVALIDCALL;
    LOCK();
    m_api.rs[State] = Value;
    Exec([this, State, Value] { SetRenderStateGL(State, Value); });
    return D3D_OK;
}

void Device::SetRenderStateGL(DWORD state, DWORD value) {
    if (m_gl.rs[state] == value) return;
    m_gl.rs[state] = value;
    switch (state) {
    case D3DRS_AMBIENT: m_gen[G_LIGHTS]++; break;
    case D3DRS_FOGCOLOR:
    case D3DRS_FOGSTART:
    case D3DRS_FOGEND:
    case D3DRS_FOGDENSITY: m_gen[G_FOG]++; break;
    case D3DRS_TEXTUREFACTOR: m_gen[G_TFACTOR]++; break;
    case D3DRS_ALPHAREF: m_gen[G_ALPHAREF]++; break;
    default: m_fixedDirty = true; break;
    }
}

HRESULT Device::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) {
    if (!pValue || (UINT) State >= 256) return D3DERR_INVALIDCALL;
    LOCK();
    *pValue = m_api.rs[State];
    return D3D_OK;
}

HRESULT Device::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    if (Stage >= kStages || (UINT) Type >= 33) return D3DERR_INVALIDCALL;
    LOCK();
    m_api.tss[Stage][Type] = Value;
    Exec([this, Stage, Type, Value] {
        m_gl.tss[Stage][Type] = Value;
        if (Type == D3DTSS_CONSTANT) m_gen[G_STAGE]++;
    });
    return D3D_OK;
}

HRESULT Device::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) {
    if (!pValue || Stage >= kStages || (UINT) Type >= 33) return D3DERR_INVALIDCALL;
    LOCK();
    *pValue = m_api.tss[Stage][Type];
    return D3D_OK;
}

HRESULT Device::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    if (Sampler >= kSamplers || (UINT) Type >= 14) return D3DERR_INVALIDCALL;
    LOCK();
    m_api.samp[Sampler][Type] = Value;
    Exec([this, Sampler, Type, Value] {
        m_gl.samp[Sampler][Type] = Value;
        if (Type == D3DSAMP_MIPMAPLODBIAS || Type == D3DSAMP_BORDERCOLOR) m_gen[G_STAGE]++;
    });
    return D3D_OK;
}

HRESULT Device::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) {
    if (!pValue || Sampler >= kSamplers || (UINT) Type >= 14) return D3DERR_INVALIDCALL;
    LOCK();
    *pValue = m_api.samp[Sampler][Type];
    return D3D_OK;
}

static D3DMATRIX *TransformSlot(State &s, D3DTRANSFORMSTATETYPE t) {
    if (t == D3DTS_VIEW) return &s.view;
    if (t == D3DTS_PROJECTION) return &s.proj;
    if (t >= D3DTS_TEXTURE0 && t <= D3DTS_TEXTURE7) return &s.tex[t - D3DTS_TEXTURE0];
    if ((UINT) t >= 256 && (UINT) t < 256 + kMaxWorld) return &s.world[t - 256];
    return nullptr;
}

HRESULT Device::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    LOCK();
    D3DMATRIX *slot = TransformSlot(m_api, State);
    if (!slot) return D3DERR_INVALIDCALL;
    *slot = *pMatrix;
    D3DMATRIX m = *pMatrix;
    Exec([this, State, m] {
        D3DMATRIX *gl = TransformSlot(m_gl, State);
        bool world = (UINT) State >= 256;
        if (world && (UINT) State - 256 + 1 > m_worldHigh) {
            m_worldHigh = (UINT) State - 256 + 1;  // the bones uploaded grow even if this one keeps its value
            m_gen[G_WORLD]++;
        }
        if (!memcmp(gl, &m, sizeof(m))) return;  // programs often set the same matrix again
        *gl = m;
        if (State == D3DTS_VIEW) { m_gen[G_TRANSFORM]++; m_gen[G_VIEW]++; m_gen[G_LIGHTS]++; m_gen[G_WORLD]++; }
        else if (State == D3DTS_PROJECTION) m_gen[G_PROJ]++;
        else if (State >= D3DTS_TEXTURE0 && State <= D3DTS_TEXTURE7) m_gen[G_TEXMAT]++;
        else {
            m_gen[G_WORLD]++;
            if (State == D3DTS_WORLD) m_gen[G_TRANSFORM]++;
        }
    });
    return D3D_OK;
}

HRESULT Device::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    LOCK();
    D3DMATRIX *slot = TransformSlot(m_api, State);
    if (!slot) return D3DERR_INVALIDCALL;
    *pMatrix = *slot;
    return D3D_OK;
}

HRESULT Device::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    D3DMATRIX cur, out;
    if (FAILED(GetTransform(State, &cur))) return D3DERR_INVALIDCALL;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            float v = 0;
            for (int k = 0; k < 4; k++) v += pMatrix->m[r][k] * cur.m[k][c];
            out.m[r][c] = v;
        }
    return SetTransform(State, &out);
}

HRESULT Device::SetViewport(CONST D3DVIEWPORT9 *pViewport) {
    if (!pViewport) return D3DERR_INVALIDCALL;
    LOCK();
    m_api.viewport = *pViewport;
    D3DVIEWPORT9 vp = *pViewport;
    Exec([this, vp] {
        m_gl.viewport = vp;
        m_gen[G_VIEWPORT]++;
    });
    return D3D_OK;
}

HRESULT Device::GetViewport(D3DVIEWPORT9 *pViewport) {
    if (!pViewport) return D3DERR_INVALIDCALL;
    LOCK();
    *pViewport = m_api.viewport;
    return D3D_OK;
}

HRESULT Device::SetMaterial(CONST D3DMATERIAL9 *pMaterial) {
    if (!pMaterial) return D3DERR_INVALIDCALL;
    LOCK();
    m_api.material = *pMaterial;
    D3DMATERIAL9 m = *pMaterial;
    Exec([this, m] {
        m_gl.material = m;
        m_gen[G_MATERIAL]++;
    });
    return D3D_OK;
}

HRESULT Device::GetMaterial(D3DMATERIAL9 *pMaterial) {
    if (!pMaterial) return D3DERR_INVALIDCALL;
    LOCK();
    *pMaterial = m_api.material;
    return D3D_OK;
}

static void StoreLight(State &s, DWORD index, const D3DLIGHT9 &l) {
    for (auto &e : s.lights)
        if (e.first == index) {
            e.second = l;
            return;
        }
    s.lights.emplace_back(index, l);
}

static void EnableLight(State &s, DWORD index, BOOL enable) {
    if (!s.FindLight(index)) {
        // D3D9 creates a default light when an unset index is enabled.
        D3DLIGHT9 l = {};
        l.Type = D3DLIGHT_DIRECTIONAL;
        l.Diffuse.r = l.Diffuse.g = l.Diffuse.b = 1.0f;
        l.Direction.z = 1.0f;
        StoreLight(s, index, l);
    }
    auto it = std::find(s.enabledLights.begin(), s.enabledLights.end(), index);
    if (enable && it == s.enabledLights.end()) s.enabledLights.push_back(index);
    if (!enable && it != s.enabledLights.end()) s.enabledLights.erase(it);
}

HRESULT Device::SetLight(DWORD Index, CONST D3DLIGHT9 *pLight) {
    if (!pLight) return D3DERR_INVALIDCALL;
    LOCK();
    StoreLight(m_api, Index, *pLight);
    D3DLIGHT9 l = *pLight;
    Exec([this, Index, l] {
        StoreLight(m_gl, Index, l);
        m_gen[G_LIGHTS]++;
    });
    return D3D_OK;
}

HRESULT Device::GetLight(DWORD Index, D3DLIGHT9 *pLight) {
    if (!pLight) return D3DERR_INVALIDCALL;
    LOCK();
    const D3DLIGHT9 *l = m_api.FindLight(Index);
    if (!l) return D3DERR_INVALIDCALL;
    *pLight = *l;
    return D3D_OK;
}

HRESULT Device::LightEnable(DWORD Index, BOOL Enable) {
    LOCK();
    EnableLight(m_api, Index, Enable);
    Exec([this, Index, Enable] {
        EnableLight(m_gl, Index, Enable);
        m_gen[G_LIGHTS]++;
    });
    return D3D_OK;
}

HRESULT Device::GetLightEnable(DWORD Index, BOOL *pEnable) {
    if (!pEnable) return D3DERR_INVALIDCALL;
    LOCK();
    if (!m_api.FindLight(Index)) return D3DERR_INVALIDCALL;
    *pEnable = std::find(m_api.enabledLights.begin(), m_api.enabledLights.end(), Index) != m_api.enabledLights.end() ? 128 : 0;
    return D3D_OK;
}

HRESULT Device::SetScissorRect(CONST RECT *pRect) {
    if (!pRect) return D3DERR_INVALIDCALL;
    LOCK();
    m_scissor = *pRect;
    D3D9GL_ONCE("SetScissorRect stored, scissor test not implemented");
    return D3D_OK;
}

HRESULT Device::GetScissorRect(RECT *pRect) {
    if (!pRect) return D3DERR_INVALIDCALL;
    LOCK();
    *pRect = m_scissor;
    return D3D_OK;
}

// --- Bindings ------------------------------------------------------------------------------

HRESULT Device::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) {
    if (Stage >= kSamplers) {
        if (Stage >= D3DDMAPSAMPLER) return D3D_OK;  // displacement / vertex samplers: unused
        return D3DERR_INVALIDCALL;
    }
    LOCK();
    m_api.textures[Stage] = pTexture;
    IDirect3DBaseTexture9 *t = Hold(pTexture);
    Exec([this, Stage, t] {
        SetTextureGL(Stage, t);
        Drop(t);
    });
    return D3D_OK;
}

void Device::SetTextureGL(DWORD stage, IDirect3DBaseTexture9 *tex) { Assign(m_gl.textures[stage], tex); }

HRESULT Device::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) {
    if (!ppTexture || Stage >= kSamplers) return D3DERR_INVALIDCALL;
    LOCK();
    *ppTexture = m_api.textures[Stage];
    if (*ppTexture) (*ppTexture)->AddRef();
    return D3D_OK;
}

HRESULT Device::SetStreamSource(UINT n, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) {
    if (n >= kStreams) return D3DERR_INVALIDCALL;
    LOCK();
    VertexBuffer *vb = static_cast<VertexBuffer *>(pStreamData);
    m_api.streams[n] = {vb, OffsetInBytes, Stride};
    Hold(vb);
    Exec([this, n, vb, OffsetInBytes, Stride] {
        SetStreamGL(n, vb, OffsetInBytes, Stride);
        Drop(vb);
    });
    return D3D_OK;
}

void Device::SetStreamGL(UINT n, VertexBuffer *vb, UINT off, UINT stride) {
    Assign(m_gl.streams[n].vb, vb);
    m_gl.streams[n].offset = off;
    m_gl.streams[n].stride = stride;
}

HRESULT Device::GetStreamSource(UINT n, IDirect3DVertexBuffer9 **pp, UINT *pOffset, UINT *pStride) {
    if (n >= kStreams || !pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = m_api.streams[n].vb;
    if (*pp) (*pp)->AddRef();
    if (pOffset) *pOffset = m_api.streams[n].offset;
    if (pStride) *pStride = m_api.streams[n].stride;
    return D3D_OK;
}

HRESULT Device::SetIndices(IDirect3DIndexBuffer9 *pIndexData) {
    LOCK();
    IndexBuffer *ib = static_cast<IndexBuffer *>(pIndexData);
    m_api.indices = ib;
    Hold(ib);
    Exec([this, ib] {
        SetIndicesGL(ib);
        Drop(ib);
    });
    return D3D_OK;
}

void Device::SetIndicesGL(IndexBuffer *ib) { Assign(m_gl.indices, ib); }

HRESULT Device::GetIndices(IDirect3DIndexBuffer9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = m_api.indices;
    if (*pp) (*pp)->AddRef();
    return D3D_OK;
}

HRESULT Device::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) {
    LOCK();
    VertexDecl *d = static_cast<VertexDecl *>(pDecl);
    m_api.decl = d;
    m_api.fvf = d ? d->Fvf() : 0;
    Hold(d);
    Exec([this, d] {
        SetDeclGL(d);
        Drop(d);
    });
    return D3D_OK;
}

void Device::SetDeclGL(VertexDecl *decl) {
    Assign(m_gl.decl, decl);
    m_gl.fvf = decl ? decl->Fvf() : 0;
}

HRESULT Device::GetVertexDeclaration(IDirect3DVertexDeclaration9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = m_api.decl;
    if (*pp) (*pp)->AddRef();
    return D3D_OK;
}

HRESULT Device::SetFVF(DWORD FVF) {
    LOCK();
    m_api.fvf = FVF;
    m_api.decl = nullptr;
    Exec([this, FVF] {
        Assign(m_gl.decl, (VertexDecl *) nullptr);
        m_gl.fvf = FVF;
    });
    return D3D_OK;
}

HRESULT Device::GetFVF(DWORD *pFVF) {
    if (!pFVF) return D3DERR_INVALIDCALL;
    LOCK();
    *pFVF = m_api.fvf;
    return D3D_OK;
}

// --- Render targets ------------------------------------------------------------------------

HRESULT Device::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) {
    if (RenderTargetIndex != 0) return pRenderTarget ? D3DERR_INVALIDCALL : D3D_OK;
    if (!pRenderTarget) return D3DERR_INVALIDCALL;
    LOCK();
    Surface *s = static_cast<Surface *>(pRenderTarget);
    m_api.rt = s;
    // D3D9 resets the viewport to the whole new render target.
    D3DVIEWPORT9 vp = {0, 0, s->Width(), s->Height(), 0.0f, 1.0f};
    m_api.viewport = vp;
    Hold(s);
    Exec([this, s, vp] {
        Assign(m_gl.rt, s);
        m_gl.viewport = vp;
        m_gen[G_VIEWPORT]++;
        Drop(s);
    });
    return D3D_OK;
}

HRESULT Device::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    if (RenderTargetIndex != 0 || !m_api.rt) {
        *pp = nullptr;
        return D3DERR_NOTFOUND;
    }
    *pp = m_api.rt;
    (*pp)->AddRef();
    return D3D_OK;
}

HRESULT Device::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) {
    LOCK();
    Surface *s = static_cast<Surface *>(pNewZStencil);
    m_api.depth = s;
    Hold(s);
    Exec([this, s] {
        Assign(m_gl.depth, s);
        Drop(s);
    });
    return D3D_OK;
}

HRESULT Device::GetDepthStencilSurface(IDirect3DSurface9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = m_api.depth;
    if (!*pp) return D3DERR_NOTFOUND;
    (*pp)->AddRef();
    return D3D_OK;
}

HRESULT Device::GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9 **pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    LOCK();
    *pp = m_backBuffer;
    m_backBuffer->AddRef();
    return D3D_OK;
}

// --- Scene, clear, present -----------------------------------------------------------------

HRESULT Device::BeginScene() {
    LOCK();
    m_inScene = true;
    return D3D_OK;
}

HRESULT Device::EndScene() {
    LOCK();
    m_inScene = false;
    return D3D_OK;
}

HRESULT Device::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    LOCK();
    std::vector<D3DRECT> rects;
    if (Count && pRects) rects.assign(pRects, pRects + Count);
    Exec([this, rects, Flags, Color, Z, Stencil] {
        if (Drawing()) ClearGL(rects, Flags, Color, Z, Stencil);
    });
    return D3D_OK;
}

void Device::ClearGL(std::vector<D3DRECT> rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) {
    ProfileScope prof(PROF_CLEAR);
    if (!BindFramebuffer()) return;
    GLbitfield mask = 0;
    if (flags & D3DCLEAR_TARGET) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f,
                     ((color >> 24) & 255) / 255.0f);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if ((flags & D3DCLEAR_ZBUFFER) && m_gl.depth) {
        glDepthMask(GL_TRUE);
        glClearDepthf(z);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if ((flags & D3DCLEAR_STENCIL) && m_gl.depth) {
        glStencilMask(0xFF);
        glClearStencil((GLint) stencil);
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    if (!mask) return;
    // D3D9 clears the viewport rectangle (intersected with the rects, if any).
    const D3DVIEWPORT9 &vp = m_gl.viewport;
    glEnable(GL_SCISSOR_TEST);
    if (rects.empty()) rects.push_back({(LONG) vp.X, (LONG) vp.Y, (LONG) (vp.X + vp.Width), (LONG) (vp.Y + vp.Height)});
    for (const D3DRECT &r : rects) {
        LONG x1 = std::max<LONG>(r.x1, vp.X), y1 = std::max<LONG>(r.y1, vp.Y);
        LONG x2 = std::min<LONG>(r.x2, vp.X + vp.Width), y2 = std::min<LONG>(r.y2, vp.Y + vp.Height);
        if (x2 <= x1 || y2 <= y1) continue;
        glScissor(x1, y1, x2 - x1, y2 - y1);
        glClear(mask);
    }
    glDisable(GL_SCISSOR_TEST);
    InvalidateFixedGL();  // masks changed
}

HRESULT Device::Present(CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *) {
    if (OnGLThread()) {
        {
            LOCK();
            if (m_queued) Drain();
            PresentGL();
        }
        AfterPresent();
        return D3D_OK;
    }
    LOCK();
    {
        m_queuedPresents++;
        Defer([this] {
            m_queuedPresents--;
            if (Drawing()) PresentGL();
            m_skipDraws = m_queuedPresents > 1;
        });
    }
    return D3D_OK;
}

// The GL context is gone (WebGL context loss, ANGLE device removal): every texture and buffer
// on the GPU went with it. Nothing is restored: drawing stops, and the platform is told once. In
// the browser the page decides (Module.onWebGLContextLost, or a reload); natively the program
// keeps its window, without drawing.
void Device::OnContextLost() {
    if (m_contextLost) return;
    m_contextLost = true;
    Log("GL context lost after %u presents: drawing stops", m_presents);
    NotifyContextLost();
}

// PORTICO_D3D9_TEST_LOSE_AFTER=n: behave as if the context were lost at the n-th present (native test
// of the path above; in the browser, WEBGL_lose_context does it for real).
bool Device::TestContextLoss() {
    static const int s_after = [] {
        char v[16] = {0};
        return GetEnv("PORTICO_D3D9_TEST_LOSE_AFTER", v, sizeof(v)) ? atoi(v) : 0;
    }();
    return s_after > 0 && m_presents >= (unsigned) s_after;
}

void Device::PresentGL() {
    ProfileFrame();
    ProfileScope prof(PROF_PRESENT);
    m_presents++;
    g_presentCount = m_presents;
    static const unsigned s_stats = [] {
        char v[16] = {0};
        return GetEnv("PORTICO_D3D9_STATS", v, sizeof(v)) ? (unsigned) atoi(v) : 0u;
    }();
    if (s_stats && m_presents % s_stats == 0) LogStats(m_presents);
    // A lost WebGL context also reports a 0x0 drawing buffer: test the loss first.
    if (IsContextLost() || TestContextLoss()) {
        OnContextLost();
        return;
    }
    int dw = 0, dh = 0;
    GetDrawableSize(dw, dh);
    if (dw <= 0 || dh <= 0) return;
    GLuint src = GetFramebuffer(m_backBuffer, nullptr);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    InvalidateFixedGL();
    GLint bw = (GLint) m_backBuffer->Width(), bh = (GLint) m_backBuffer->Height();
    // Back buffer rows are in D3D order (row 0 on top): flip to the window.
    glBlitFramebuffer(0, 0, bw, bh, 0, dh, dw, 0, GL_COLOR_BUFFER_BIT, (bw == dw && bh == dh) ? GL_NEAREST : GL_LINEAR);
    m_boundFBO = ~0u;
    if (!SwapGLBuffers()) OnContextLost();
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) D3D9GL_ONCE("GL error 0x%x seen at Present", err);
}

// --- Copies between surfaces ---------------------------------------------------------------

HRESULT Device::GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) {
    if (!pRenderTarget || !pDestSurface) return D3DERR_INVALIDCALL;
    LOCK();
    // Readback needs the GL thread; other threads (a loader thread's capture) get an error instead
    // of waiting for it.
    if (!OnGLThread()) return D3DERR_INVALIDCALL;
    if (m_queued) Drain();
    Surface *src = static_cast<Surface *>(pRenderTarget), *dst = static_cast<Surface *>(pDestSurface);
    Texture *tex = src->GetTexture();
    if (!tex || !tex->IsRenderTarget() || !dst->Data()) return D3DERR_INVALIDCALL;
    std::vector<uint8_t> rgba;
    if (!tex->ReadPixels(src->Level(), rgba)) return D3DERR_INVALIDCALL;
    UINT w = std::min(src->Width(), dst->Width()), h = std::min(src->Height(), dst->Height());
    if (w != src->Width()) {
        std::vector<uint8_t> cut(size_t(w) * h * 4);
        for (UINT y = 0; y < h; y++) memcpy(&cut[size_t(y) * w * 4], &rgba[size_t(y) * src->Width() * 4], w * 4);
        rgba.swap(cut);
    }
    ConvertFromRGBA8(dst->Format(), rgba.data(), w, h, dst->Data(), dst->Pitch());
    dst->MarkDirty();
    return D3D_OK;
}

static bool IsGPUSurface(Surface *s) {
    return s->Kind() == SurfaceKind::RenderTarget || (s->GetTexture() && s->GetTexture()->IsRenderTarget());
}

static void CopyCPU(Surface *src, const RECT &sr, Surface *dst, const RECT &dr) {
    const FormatInfo *f = GetFormatInfo(src->Format());
    if (!f || src->Format() != dst->Format() || !src->Data() || !dst->Data()) {
        Log("CPU surface copy %u -> %u not supported", src->Format(), dst->Format());
        return;
    }
    if (f->block) {
        UINT rows = GetRows(src->Format(), sr.bottom - sr.top);
        UINT bytes = GetPitch(src->Format(), sr.right - sr.left);
        UINT bs = f->bpp * 2;
        for (UINT y = 0; y < rows; y++)
            memcpy(dst->Data() + (dr.top / 4 + y) * dst->Pitch() + (dr.left / 4) * bs,
                   src->Data() + (sr.top / 4 + y) * src->Pitch() + (sr.left / 4) * bs, bytes);
        dst->MarkDirty();
        return;
    }
    UINT bpp = f->bpp / 8;
    LONG sw = sr.right - sr.left, sh = sr.bottom - sr.top, dw = dr.right - dr.left, dh = dr.bottom - dr.top;
    for (LONG y = 0; y < dh; y++) {
        LONG syy = sr.top + (sh == dh ? y : y * sh / dh);
        uint8_t *d = dst->Data() + size_t(dr.top + y) * dst->Pitch() + size_t(dr.left) * bpp;
        const uint8_t *s = src->Data() + size_t(syy) * src->Pitch();
        if (sw == dw) memcpy(d, s + size_t(sr.left) * bpp, size_t(dw) * bpp);
        else
            for (LONG x = 0; x < dw; x++) memcpy(d + x * bpp, s + size_t(sr.left + x * sw / dw) * bpp, bpp);
    }
    dst->MarkDirty();
}

HRESULT Device::StretchRect(IDirect3DSurface9 *pSourceSurface, CONST RECT *pSourceRect, IDirect3DSurface9 *pDestSurface,
                            CONST RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) {
    if (!pSourceSurface || !pDestSurface) return D3DERR_INVALIDCALL;
    LOCK();
    Surface *src = static_cast<Surface *>(pSourceSurface), *dst = static_cast<Surface *>(pDestSurface);
    RECT sr = pSourceRect ? *pSourceRect : RECT{0, 0, (LONG) src->Width(), (LONG) src->Height()};
    RECT dr = pDestRect ? *pDestRect : RECT{0, 0, (LONG) dst->Width(), (LONG) dst->Height()};
    if (!IsGPUSurface(src) && !IsGPUSurface(dst)) {
        CopyCPU(src, sr, dst, dr);
        return D3D_OK;
    }
    Hold(src);
    Hold(dst);
    Exec([this, src, sr, dst, dr, Filter] {
        StretchGL(src, sr, dst, dr, Filter);
        Drop(src);
        Drop(dst);
    });
    return D3D_OK;
}

void Device::StretchGL(Surface *src, RECT sr, Surface *dst, RECT dr, D3DTEXTUREFILTERTYPE filter) {
    if (!IsGPUSurface(dst)) {
        D3D9GL_ONCE("StretchRect from a render target to a CPU surface not supported");
        return;
    }
    if (!IsGPUSurface(src)) {
        // CPU source into a render target texture: make sure the source texture is uploaded and
        // draw it through a temporary framebuffer.
        Texture *t = src->GetTexture();
        if (!t) {
            D3D9GL_ONCE("StretchRect from an offscreen surface to a render target not supported");
            return;
        }
        GLuint tex = t->GLTexture();
        // GetFramebuffer binds GL_FRAMEBUFFER when it creates one: get it before binding the read side.
        GLuint dstFBO = GetFramebuffer(dst, nullptr);
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, src->Level());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(sr.left, sr.top, sr.right, sr.bottom, dr.left, dr.top, dr.right, dr.bottom, GL_COLOR_BUFFER_BIT,
                          filter == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);
        glDeleteFramebuffers(1, &fbo);
        m_boundFBO = ~0u;
        return;
    }
    // Both framebuffers first: GetFramebuffer binds GL_FRAMEBUFFER (read and draw) when it creates one.
    GLuint srcFBO = GetFramebuffer(src, nullptr), dstFBO = GetFramebuffer(dst, nullptr);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(sr.left, sr.top, sr.right, sr.bottom, dr.left, dr.top, dr.right, dr.bottom, GL_COLOR_BUFFER_BIT,
                      filter == D3DTEXF_POINT || filter == D3DTEXF_NONE ? GL_NEAREST : GL_LINEAR);
    m_boundFBO = ~0u;
}

HRESULT Device::UpdateSurface(IDirect3DSurface9 *pSourceSurface, CONST RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface,
                              CONST POINT *pDestPoint) {
    if (!pSourceSurface || !pDestinationSurface) return D3DERR_INVALIDCALL;
    LOCK();
    Surface *src = static_cast<Surface *>(pSourceSurface), *dst = static_cast<Surface *>(pDestinationSurface);
    RECT sr = pSourceRect ? *pSourceRect : RECT{0, 0, (LONG) src->Width(), (LONG) src->Height()};
    POINT p = pDestPoint ? *pDestPoint : POINT{0, 0};
    RECT dr = {p.x, p.y, p.x + (sr.right - sr.left), p.y + (sr.bottom - sr.top)};
    if (IsGPUSurface(dst)) {
        D3D9GL_ONCE("UpdateSurface into a render target not supported");
        return D3DERR_INVALIDCALL;
    }
    CopyCPU(src, sr, dst, dr);
    return D3D_OK;
}

HRESULT Device::UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) {
    if (!pSourceTexture || !pDestinationTexture) return D3DERR_INVALIDCALL;
    LOCK();
    Texture *s = static_cast<Texture *>(static_cast<IDirect3DTexture9 *>(pSourceTexture));
    Texture *d = static_cast<Texture *>(static_cast<IDirect3DTexture9 *>(pDestinationTexture));
    UINT n = std::min(s->GetLevelCount(), d->GetLevelCount());
    for (UINT i = 0; i < n; i++) {
        if (s->GetLevel(i).data.empty()) continue;
        memcpy(d->LevelData(i), s->GetLevel(i).data.data(), std::min(s->GetLevel(i).data.size(), d->GetLevel(i).data.size()));
        d->MarkDirty(i);
    }
    return D3D_OK;
}

HRESULT Device::ColorFill(IDirect3DSurface9 *pSurface, CONST RECT *pRect, D3DCOLOR color) {
    if (!pSurface) return D3DERR_INVALIDCALL;
    LOCK();
    Surface *s = static_cast<Surface *>(pSurface);
    RECT r = pRect ? *pRect : RECT{0, 0, (LONG) s->Width(), (LONG) s->Height()};
    if (!IsGPUSurface(s)) {
        const FormatInfo *f = GetFormatInfo(s->Format());
        if (!f || f->bpp != 32 || !s->Data()) return D3DERR_INVALIDCALL;
        for (LONG y = r.top; y < r.bottom; y++)
            for (LONG x = r.left; x < r.right; x++) memcpy(s->Data() + size_t(y) * s->Pitch() + x * 4, &color, 4);
        s->MarkDirty();
        return D3D_OK;
    }
    Hold(s);
    Exec([this, s, r, color] {
        glBindFramebuffer(GL_FRAMEBUFFER, GetFramebuffer(s, nullptr));
        m_boundFBO = ~0u;
        glEnable(GL_SCISSOR_TEST);
        glScissor(r.left, r.top, r.right - r.left, r.bottom - r.top);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f,
                     ((color >> 24) & 255) / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        InvalidateFixedGL();
        Drop(s);
    });
    return D3D_OK;
}

// --- Draw entry points ---------------------------------------------------------------------

static UINT VertexCount(D3DPRIMITIVETYPE t, UINT prims) {
    switch (t) {
    case D3DPT_POINTLIST: return prims;
    case D3DPT_LINELIST: return prims * 2;
    case D3DPT_LINESTRIP: return prims + 1;
    case D3DPT_TRIANGLELIST: return prims * 3;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN: return prims + 2;
    default: return 0;
    }
}

HRESULT Device::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    if (!PrimitiveCount) return D3D_OK;
    LOCK();
    auto dc = std::make_shared<DrawCall>();
    dc->type = PrimitiveType;
    dc->primCount = PrimitiveCount;
    dc->indexed = false;
    dc->baseVertex = 0;
    dc->startVertex = StartVertex;
    dc->startIndex = 0;
    Exec([this, dc] { if (Drawing()) DrawGL(*dc); });
    return D3D_OK;
}

HRESULT Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT, UINT, UINT startIndex, UINT primCount) {
    if (!primCount) return D3D_OK;
    LOCK();
    auto dc = std::make_shared<DrawCall>();
    dc->type = PrimitiveType;
    dc->primCount = primCount;
    dc->indexed = true;
    dc->baseVertex = BaseVertexIndex;
    dc->startVertex = 0;
    dc->startIndex = startIndex;
    Exec([this, dc] { if (Drawing()) DrawGL(*dc); });
    return D3D_OK;
}

HRESULT Device::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pData, UINT Stride) {
    if (!PrimitiveCount || !pData) return D3D_OK;
    LOCK();
    auto dc = std::make_shared<DrawCall>();
    dc->type = PrimitiveType;
    dc->primCount = PrimitiveCount;
    dc->indexed = false;
    dc->baseVertex = 0;
    dc->startVertex = 0;
    dc->startIndex = 0;
    dc->up = true;
    dc->upStride = Stride;
    const uint8_t *p = (const uint8_t *) pData;
    dc->upVertices.assign(p, p + size_t(VertexCount(PrimitiveType, PrimitiveCount)) * Stride);
    Exec([this, dc] { if (Drawing()) DrawGL(*dc); });
    // D3D9 unbinds stream 0 after a UP draw.
    m_api.streams[0] = {nullptr, 0, 0};
    Exec([this] { SetStreamGL(0, nullptr, 0, 0); });
    return D3D_OK;
}

HRESULT Device::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount,
                                       CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexData, UINT Stride) {
    if (!PrimitiveCount || !pIndexData || !pVertexData) return D3D_OK;
    LOCK();
    auto dc = std::make_shared<DrawCall>();
    dc->type = PrimitiveType;
    dc->primCount = PrimitiveCount;
    dc->indexed = true;
    dc->baseVertex = 0;
    dc->startVertex = 0;
    dc->startIndex = 0;
    dc->up = true;
    dc->upStride = Stride;
    const uint8_t *v = (const uint8_t *) pVertexData;
    dc->upVertices.assign(v, v + size_t(MinVertexIndex + NumVertices) * Stride);
    UINT isz = IndexDataFormat == D3DFMT_INDEX32 ? 4 : 2;
    const uint8_t *ix = (const uint8_t *) pIndexData;
    dc->upIndices.assign(ix, ix + size_t(VertexCount(PrimitiveType, PrimitiveCount)) * isz);
    dc->upIndexFormat = IndexDataFormat;
    Exec([this, dc] { if (Drawing()) DrawGL(*dc); });
    m_api.streams[0] = {nullptr, 0, 0};
    m_api.indices = nullptr;
    Exec([this] {
        SetStreamGL(0, nullptr, 0, 0);
        SetIndicesGL(nullptr);
    });
    return D3D_OK;
}

}  // namespace d3d9gl
