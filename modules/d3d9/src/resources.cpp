// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Textures, surfaces, vertex/index buffers and vertex declarations of the GL layer.
#include "device.h"

#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <webgl/webgl2.h>  // glGetBufferSubData
#endif

namespace d3d9gl {

// PORTICO_D3D9_KEEP_CPU_COPY=1 keeps the CPU copy of every buffer and texture (to measure the difference).
static bool KeepCPUCopies() {
    static const bool s_keep = [] {
        char v[8] = {0};
        return GetEnv("PORTICO_D3D9_KEEP_CPU_COPY", v, sizeof(v)) && v[0] == '1';
    }();
    return s_keep;
}

// Textures keep their CPU copy, as Direct3D 9's managed pool does, so a later lock sees their contents.
// PORTICO_D3D9_DROP_TEXTURE_COPY=1 frees it once a level is in GL: a large saving for programs that never lock a
// texture again after using it. One that does starts from zeros, and it is logged.
static bool DropTextureCopies() {
    static const bool s_drop = [] {
        char v[8] = {0};
        return GetEnv("PORTICO_D3D9_DROP_TEXTURE_COPY", v, sizeof(v)) && v[0] == '1';
    }();
    return s_drop && !KeepCPUCopies();
}

// --- Resource ------------------------------------------------------------------------------

template <class I>
Resource<I>::Resource(Device *dev) : m_dev(dev) {}

template <class I>
Resource<I>::~Resource() {}

template <class I>
HRESULT Resource<I>::GetDevice(IDirect3DDevice9 **ppDevice) {
    if (!ppDevice) return D3DERR_INVALIDCALL;
    m_dev->AddRef();
    *ppDevice = m_dev;
    return D3D_OK;
}

template class Resource<IDirect3DSurface9>;
template class Resource<IDirect3DTexture9>;
template class Resource<IDirect3DVertexBuffer9>;
template class Resource<IDirect3DIndexBuffer9>;

// --- Surface -------------------------------------------------------------------------------

Surface::Surface(Device *dev, Texture *tex, UINT level)
    : Resource(dev), m_kind(SurfaceKind::TextureLevel), m_tex(tex), m_level(level) {
    tex->GetLevelDesc(level, &m_desc);
}

Surface::Surface(Device *dev, SurfaceKind kind, UINT width, UINT height, D3DFORMAT fmt, D3DPOOL pool)
    : Resource(dev), m_kind(kind) {
    m_desc.Format = fmt;
    m_desc.Type = D3DRTYPE_SURFACE;
    m_desc.Pool = pool;
    m_desc.Width = width;
    m_desc.Height = height;
    switch (kind) {
    case SurfaceKind::Offscreen:
        m_data.assign(GetLevelSize(fmt, width, height), 0);
        CpuBytes(CPU_SURFACE, (int64_t) m_data.size());
        break;
    case SurfaceKind::DepthStencil:
        m_desc.Usage = D3DUSAGE_DEPTHSTENCIL;
        break;
    case SurfaceKind::RenderTarget:
        m_desc.Usage = D3DUSAGE_RENDERTARGET;
        m_tex = new Texture(dev, width, height, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT);
        break;
    default:
        break;
    }
}

Surface::~Surface() {
    ReleaseGL();
    CpuBytes(CPU_SURFACE, -(int64_t) m_data.size());
    if (m_kind == SurfaceKind::RenderTarget && m_tex) m_tex->Release();
}

HRESULT Surface::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 || riid == IID_IDirect3DSurface9) {
        AddRef();
        *ppv = static_cast<IDirect3DSurface9 *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG Surface::AddRef() {
    if (m_kind == SurfaceKind::TextureLevel) return m_tex->AddRef();
    return Unknown::AddRef();
}

ULONG Surface::Release() {
    if (m_kind == SurfaceKind::TextureLevel) return m_tex->Release();
    return Unknown::Release();
}

HRESULT Surface::GetContainer(REFIID riid, void **ppContainer) {
    if (!ppContainer) return D3DERR_INVALIDCALL;
    if (m_kind == SurfaceKind::TextureLevel) return m_tex->QueryInterface(riid, ppContainer);
    return m_dev->QueryInterface(riid, ppContainer);
}

HRESULT Surface::GetDesc(D3DSURFACE_DESC *pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    *pDesc = m_desc;
    return D3D_OK;
}

uint8_t *Surface::Data() {
    if (m_kind == SurfaceKind::TextureLevel) {
        m_tex->NoteTouch(TOUCH_TEX_COPY, m_level, 0);
        return m_tex->LevelData(m_level);
    }
    if (m_kind == SurfaceKind::Offscreen) return m_data.data();
    return nullptr;
}

UINT Surface::Pitch() const { return GetPitch(m_desc.Format, m_desc.Width); }

void Surface::MarkDirty() {
    if (m_kind == SurfaceKind::TextureLevel) m_tex->MarkDirty(m_level);
}

HRESULT Surface::LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) {
    if (!pLockedRect) return D3DERR_INVALIDCALL;
    if (m_kind == SurfaceKind::TextureLevel) return m_tex->LockRect(m_level, pLockedRect, pRect, Flags);
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    if (m_kind != SurfaceKind::Offscreen || m_locked) return D3DERR_INVALIDCALL;
    const FormatInfo *f = GetFormatInfo(m_desc.Format);
    UINT pitch = Pitch();
    size_t off = 0;
    if (pRect) {
        if (f && f->block) off = (pRect->top / 4) * pitch + (pRect->left / 4) * (f->bpp * 2);
        else off = size_t(pRect->top) * pitch + size_t(pRect->left) * (f ? f->bpp / 8 : 4);
    }
    pLockedRect->pBits = m_data.data() + off;
    pLockedRect->Pitch = (INT) pitch;
    m_locked = true;
    return D3D_OK;
}

HRESULT Surface::UnlockRect() {
    if (m_kind == SurfaceKind::TextureLevel) return m_tex->UnlockRect(m_level);
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    if (!m_locked) return D3DERR_INVALIDCALL;
    m_locked = false;
    return D3D_OK;
}

GLuint Surface::GLRenderbuffer() {
    if (!m_rb) {
        glGenRenderbuffers(1, &m_rb);
        GLObjects(GLO_RENDERBUFFER, 1);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_desc.Width, m_desc.Height);
    }
    return m_rb;
}

void Surface::BindAsColor(GLenum target) {
    Texture *t = m_tex;
    glFramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t ? t->GLTexture() : 0, m_level);
}

void Surface::ReleaseGL() {
    if (m_rb) {
        std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
        m_dev->DeleteGLLater(GL_RENDERBUFFER, m_rb);
        m_rb = 0;
    }
}

// --- Texture -------------------------------------------------------------------------------

Texture::Texture(Device *dev, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
    : Resource(dev), m_width(width), m_height(height), m_usage(usage), m_format(fmt), m_pool(pool) {
    UINT maxLevels = 1;
    for (UINT w = width, h = height; w > 1 || h > 1; w = std::max(1u, w / 2), h = std::max(1u, h / 2)) maxLevels++;
    if (levels == 0 || levels > maxLevels) levels = maxLevels;
    if (usage & D3DUSAGE_AUTOGENMIPMAP) m_genMips = true;
    m_levels.resize(levels);
    UINT w = width, h = height;
    for (UINT i = 0; i < levels; i++) {
        m_levels[i].width = w;
        m_levels[i].height = h;
        m_levels[i].pitch = GetPitch(fmt, w);
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    m_surfaces.assign(levels, nullptr);
}

Texture::~Texture() {
    for (Surface *s : m_surfaces) delete s;
    ReleaseGL();
    CpuBytes(CpuKind(), -m_cpuBytes);
}

int Texture::CpuKind() const {
    return m_pool == D3DPOOL_MANAGED ? CPU_TEX_MANAGED : m_pool == D3DPOOL_DEFAULT ? CPU_TEX_DEFAULT : CPU_TEX_SYSTEM;
}

void Texture::NoteTouch(int kind, UINT level, DWORD flags) {
    unsigned last = m_lastTouch;
    m_lastTouch = g_presentCount;
    if (!m_levels[level].uploaded) return;
    if (g_presentCount - last > LATE_PRESENTS)
        Touched(m_dev->OnGLThread() ? TOUCH_LATE_GL : TOUCH_LATE_OTHER, "texture %ux%u fmt %u pool %u idle %u presents, level %u flags 0x%x",
                m_width, m_height, m_format, m_pool, g_presentCount - last, level, flags);
    Touched(kind, "texture %ux%u fmt %u pool %u usage 0x%x level %u/%u flags 0x%x", m_width, m_height, m_format, m_pool,
            m_usage, level, (UINT) m_levels.size(), flags);
}

HRESULT Texture::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 || riid == IID_IDirect3DBaseTexture9 ||
        riid == IID_IDirect3DTexture9) {
        AddRef();
        *ppv = static_cast<IDirect3DTexture9 *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

void Texture::GenerateMipSubLevels() {
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    m_genMips = true;
    m_anyDirty = true;
}

HRESULT Texture::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) {
    if (!pDesc || Level >= m_levels.size()) return D3DERR_INVALIDCALL;
    pDesc->Format = m_format;
    pDesc->Type = D3DRTYPE_SURFACE;
    pDesc->Usage = m_usage;
    pDesc->Pool = m_pool;
    pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
    pDesc->MultiSampleQuality = 0;
    pDesc->Width = m_levels[Level].width;
    pDesc->Height = m_levels[Level].height;
    return D3D_OK;
}

HRESULT Texture::GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) {
    if (!ppSurfaceLevel || Level >= m_levels.size()) return D3DERR_INVALIDCALL;
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    if (!m_surfaces[Level]) m_surfaces[Level] = new Surface(m_dev, this, Level);
    AddRef();
    *ppSurfaceLevel = m_surfaces[Level];
    return D3D_OK;
}

uint8_t *Texture::LevelData(UINT i) {
    Level &l = m_levels[i];
    if (l.data.empty()) {
        if (l.dropped)
            D3D9GL_ONCE("texture %ux%u fmt %u level %u: CPU copy needed again after it was dropped (starts zeroed)",
                        m_width, m_height, m_format, i);
        l.dropped = false;
        l.data.assign(GetLevelSize(m_format, l.width, l.height), 0);
        m_cpuBytes += (int64_t) l.data.size();
        CpuBytes(CpuKind(), (int64_t) l.data.size());
    }
    return l.data.data();
}

HRESULT Texture::LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) {
    if (!pLockedRect || Level >= m_levels.size()) return D3DERR_INVALIDCALL;
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    Texture::Level &l = m_levels[Level];
    if (IsRenderTarget() || l.locked) return D3DERR_INVALIDCALL;
    NoteTouch(TOUCH_TEX_LOCK, Level, Flags);
    const FormatInfo *f = GetFormatInfo(m_format);
    uint8_t *data = LevelData(Level);
    size_t off = 0;
    if (pRect) {
        if (f && f->block) off = (pRect->top / 4) * l.pitch + (pRect->left / 4) * (f->bpp * 2);
        else off = size_t(pRect->top) * l.pitch + size_t(pRect->left) * (f ? f->bpp / 8 : 4);
    }
    pLockedRect->pBits = data + off;
    pLockedRect->Pitch = (INT) l.pitch;
    l.locked = true;
    l.readOnly = (Flags & D3DLOCK_READONLY) != 0;
    return D3D_OK;
}

HRESULT Texture::UnlockRect(UINT Level) {
    if (Level >= m_levels.size()) return D3DERR_INVALIDCALL;
    std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
    Texture::Level &l = m_levels[Level];
    if (!l.locked) return D3DERR_INVALIDCALL;
    l.locked = false;
    if (!l.readOnly) MarkDirty(Level);
    return D3D_OK;
}

void Texture::MarkDirty(UINT level) {
    m_levels[level].dirty = true;
    m_anyDirty = true;
}

static GLenum RenderTargetInternal(D3DFORMAT fmt, GLenum &format, GLenum &type) {
    type = GL_UNSIGNED_BYTE;
    switch (fmt) {
    case D3DFMT_X8R8G8B8:
    case D3DFMT_X8B8G8R8:
        format = GL_RGB;
        return GL_RGB8;  // no alpha channel: destination alpha reads 1 like D3D9 X8
    case D3DFMT_R5G6B5:
        format = GL_RGB;
        type = GL_UNSIGNED_SHORT_5_6_5;
        return GL_RGB565;
    default:
        format = GL_RGBA;
        return GL_RGBA8;
    }
}

GLuint Texture::GLTexture() {
    if (!m_gl) {
        while (glGetError() != GL_NO_ERROR) {}
        glGenTextures(1, &m_gl);
        GLObjects(GLO_TEXTURE, 1);
        m_dev->BindTextureGL(m_gl);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint) m_levels.size() - 1);
        const FormatInfo *f = GetFormatInfo(m_format);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (UINT i = 0; i < m_levels.size(); i++) {
            Level &l = m_levels[i];
            if (IsRenderTarget() || !f) {
                GLenum format, type;
                GLenum internal = RenderTargetInternal(m_format, format, type);
                glTexImage2D(GL_TEXTURE_2D, i, internal, l.width, l.height, 0, format, type, nullptr);
            } else if (f->block && GetGLCaps().s3tc) {
                // Compressed levels are allocated with their data (WebGL needs it).
                UINT size = GetLevelSize(m_format, l.width, l.height);
                const uint8_t *data = LevelData(i);
                glCompressedTexImage2D(GL_TEXTURE_2D, i, f->internal, l.width, l.height, 0, size, data);
                l.dirty = false;
                l.uploaded = true;
            } else if (f->block) {
                glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, l.width, l.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                l.dirty = true;
            } else {
                glTexImage2D(GL_TEXTURE_2D, i, f->internal, l.width, l.height, 0, f->format, f->type, nullptr);
                if (!l.data.empty()) l.dirty = true;
            }
        }
        GLenum err = glGetError();
        if (err) Log("texture %ux%u fmt %u levels %u: GL error 0x%x", m_width, m_height, m_format, (UINT) m_levels.size(), err);
        m_allocated = true;
        m_anyDirty = true;
    } else {
        m_dev->BindTextureGL(m_gl);
    }
    if (m_anyDirty && !IsRenderTarget()) {
        bool pending = false;
        for (UINT i = 0; i < m_levels.size(); i++) {
            if (!m_levels[i].dirty) continue;
            // Locked by another thread, which may be writing it: uploaded after its unlock.
            if (m_levels[i].locked) pending = true;
            else Upload(i);
        }
        if (m_genMips && m_levels.size() > 1) glGenerateMipmap(GL_TEXTURE_2D);
        m_anyDirty = pending;
        DropCPUCopy();
    }
    return m_gl;
}

// With PORTICO_D3D9_DROP_TEXTURE_COPY=1 (see DropTextureCopies), GL holds the only copy of a level once it is
// uploaded. PORTICO_D3D9_STATS counts the locks that come later. System memory textures (sources of UpdateTexture)
// and dynamic textures always keep their copy.
void Texture::DropCPUCopy() {
    if (!DropTextureCopies() || IsRenderTarget() || m_pool == D3DPOOL_SYSTEMMEM || (m_usage & D3DUSAGE_DYNAMIC)) return;
    for (Level &l : m_levels) {
        if (l.locked || l.dirty || !l.uploaded || l.data.empty()) continue;
        m_cpuBytes -= (int64_t) l.data.size();
        CpuBytes(CpuKind(), -(int64_t) l.data.size());
        std::vector<uint8_t>().swap(l.data);
        l.dropped = true;
    }
}

void Texture::Upload(UINT i) {
    ProfileScope prof(PROF_UPLOAD);
    Level &l = m_levels[i];
    l.dirty = false;
    if (l.data.empty()) return;
    l.uploaded = true;
    const FormatInfo *f = GetFormatInfo(m_format);
    if (!f) return;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (f->block && GetGLCaps().s3tc) {
        glCompressedTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, l.width, l.height, f->internal,
                                  GetLevelSize(m_format, l.width, l.height), l.data.data());
        return;
    }
    std::vector<uint8_t> conv;
    if (ConvertForUpload(m_format, l.data.data(), l.width, l.height, conv)) {
        GLenum format = f->block ? GL_RGBA : f->format, type = f->block ? GL_UNSIGNED_BYTE : f->type;
        glTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, l.width, l.height, format, type, conv.data());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, l.width, l.height, f->format, f->type, l.data.data());
    }
}

void Texture::SetBaseLevel(UINT level) {
    if (level >= m_levels.size()) level = (UINT) m_levels.size() - 1;
    if (level != m_baseLevel) {
        m_baseLevel = level;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, (GLint) level);
    }
}

bool Texture::ReadPixels(UINT level, std::vector<uint8_t> &rgba) {
    if (!m_gl) return false;
    const Level &l = m_levels[level];
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gl, level);
    rgba.resize(size_t(l.width) * l.height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, l.width, l.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    m_dev->m_boundFBO = ~0u;
    return glGetError() == GL_NO_ERROR;
}

void Texture::ReleaseGL() {
    if (m_gl) {
        std::lock_guard<std::recursive_mutex> lk(m_dev->Mutex());
        m_dev->DeleteGLLater(GL_TEXTURE, m_gl);
        m_gl = 0;
    }
}

// --- Buffers -------------------------------------------------------------------------------

// Static buffers of 4 KB and more keep no CPU copy. Context loss reloads the page instead of
// restoring buffers, so GL holds the only copy. A Lock still sees the
// buffer's contents: bytes never written are zeros (GL buffers start zeroed), and when the locked
// range holds written bytes the GL thread reads them back from GL. A read-only lock uploads
// nothing at its Unlock. The one case without its contents is a lock by another thread (the
// loader) over bytes already written: that thread cannot read GL. It is logged; loaders usually
// fill ranges they never wrote before (pools, meshes, terrain). Small buffers keep their copy:
// programs read back small quads with plain locks.
static constexpr UINT NO_COPY_MIN_BYTES = 4096;

template <class I, class DESC>
bool Buffer<I, DESC>::NoCPUCopy() const {
    return !KeepCPUCopies() && m_length >= NO_COPY_MIN_BYTES && !(m_usage & D3DUSAGE_DYNAMIC) &&
           m_pool != D3DPOOL_SYSTEMMEM;
}

template <class I, class DESC>
bool Buffer<I, DESC>::Written(UINT begin, UINT end) const {
    auto it = std::upper_bound(m_written.begin(), m_written.end(), std::make_pair(begin, ~0u));
    if (it != m_written.begin() && std::prev(it)->second > begin) return true;
    return it != m_written.end() && it->first < end;
}

template <class I, class DESC>
void Buffer<I, DESC>::NoteWritten(UINT begin, UINT end) {
    if (begin >= end) return;
    auto it = std::lower_bound(m_written.begin(), m_written.end(), std::make_pair(begin, 0u));
    if (it != m_written.begin() && std::prev(it)->second >= begin) --it;
    auto last = it;
    while (last != m_written.end() && last->first <= end) {
        begin = std::min(begin, last->first);
        end = std::max(end, last->second);
        ++last;
    }
    it = m_written.erase(it, last);
    m_written.insert(it, std::make_pair(begin, end));
}

// GL thread: the queue first (staging blocks and snapshots of earlier unlocks), then GL.
template <class I, class DESC>
void Buffer<I, DESC>::ReadBack(UINT offset, UINT size, uint8_t *out) {
    this->m_dev->Service();
    GLenum target = std::is_same<I, IDirect3DIndexBuffer9>::value ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    if (!m_gl) return;  // nothing uploaded yet: the zeros are the contents
    this->m_dev->BindBufferGL(target, m_gl);
#ifdef __EMSCRIPTEN__
    glGetBufferSubData(target, offset, size, out);
#else
    if (void *p = glMapBufferRange(target, offset, size, GL_MAP_READ_BIT)) {
        memcpy(out, p, size);
        glUnmapBuffer(target);
    } else {
        D3D9GL_ONCE("buffer read-back failed: GL error 0x%x", glGetError());
    }
#endif
}

template <class I, class DESC>
Buffer<I, DESC>::Buffer(Device *dev, UINT length, DWORD usage, D3DPOOL pool)
    : Resource<I>(dev), m_length(length), m_usage(usage), m_pool(pool) {
    m_noCopy = NoCPUCopy();
    if (!m_noCopy) m_data.assign(length, 0);
    CpuBytes(CpuKind(), (int64_t) m_data.size());
}

template <class I, class DESC>
Buffer<I, DESC>::~Buffer() {
    ReleaseGL();
    CpuBytes(CpuKind(), -(int64_t) m_data.size());
}

template <class I, class DESC>
int Buffer<I, DESC>::CpuKind() const {
    bool dyn = (m_usage & D3DUSAGE_DYNAMIC) != 0;
    if (std::is_same<I, IDirect3DIndexBuffer9>::value) return dyn ? CPU_IB_DYNAMIC : CPU_IB_STATIC;
    return dyn ? CPU_VB_DYNAMIC : CPU_VB_STATIC;
}

template <class I> static const GUID &InterfaceId();
template <> const GUID &InterfaceId<IDirect3DVertexBuffer9>() { return IID_IDirect3DVertexBuffer9; }
template <> const GUID &InterfaceId<IDirect3DIndexBuffer9>() { return IID_IDirect3DIndexBuffer9; }

template <class I, class DESC>
HRESULT Buffer<I, DESC>::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 || riid == InterfaceId<I>()) {
        this->AddRef();
        *ppv = static_cast<I *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

template <class I, class DESC>
HRESULT Buffer<I, DESC>::Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) {
    if (!ppbData || OffsetToLock > m_length) return D3DERR_INVALIDCALL;
    std::lock_guard<std::recursive_mutex> lk(this->m_dev->Mutex());
    if (SizeToLock == 0 || OffsetToLock + SizeToLock > m_length) SizeToLock = m_length - OffsetToLock;
    if (m_gl || m_writtenOffThread)
        Touched((Flags & D3DLOCK_READONLY) ? TOUCH_BUF_READONLY : (m_usage & D3DUSAGE_DYNAMIC) ? TOUCH_BUF_DYNAMIC : TOUCH_BUF_STATIC,
                "%s %u bytes usage 0x%x pool %u lock %u+%u flags 0x%x",
                std::is_same<I, IDirect3DIndexBuffer9>::value ? "ib" : "vb", m_length, m_usage, m_pool, OffsetToLock, SizeToLock, Flags);
    if ((m_gl || m_writtenOffThread) && !(m_usage & D3DUSAGE_DYNAMIC) && g_presentCount - m_lastTouch > LATE_PRESENTS)
        Touched(this->m_dev->OnGLThread() ? TOUCH_LATE_GL : TOUCH_LATE_OTHER, "%s %u bytes pool %u idle %u presents, lock %u+%u flags 0x%x",
                std::is_same<I, IDirect3DIndexBuffer9>::value ? "ib" : "vb", m_length, m_pool, g_presentCount - m_lastTouch,
                OffsetToLock, SizeToLock, Flags);
    m_lastTouch = g_presentCount;
    if (m_noCopy) {
        bool readOnly = (Flags & D3DLOCK_READONLY) != 0;
        m_stage.push_back({OffsetToLock, std::vector<uint8_t>(SizeToLock, 0), readOnly});
        uint8_t *bytes = m_stage.back().bytes.data();
        if (!(Flags & D3DLOCK_DISCARD) && Written(OffsetToLock, OffsetToLock + SizeToLock)) {
            if (this->m_dev->OnGLThread()) {
                ReadBack(OffsetToLock, SizeToLock, bytes);
            } else {
                Touched(TOUCH_BUF_UNREADABLE, "%s %u bytes: lock %u+%u flags 0x%x by another thread over written bytes, "
                        "which it cannot read back (they read as zeros)",
                        std::is_same<I, IDirect3DIndexBuffer9>::value ? "ib" : "vb", m_length, OffsetToLock, SizeToLock, Flags);
            }
        }
        if (!readOnly) NoteWritten(OffsetToLock, OffsetToLock + SizeToLock);
        *ppbData = bytes;
        m_lockCount++;
        return D3D_OK;
    }
    *ppbData = m_data.data() + OffsetToLock;
    if (m_lockCount == 0) {
        m_lockOff = OffsetToLock;
        m_lockSize = SizeToLock;
    } else {
        UINT hi = std::max(m_lockOff + m_lockSize, OffsetToLock + SizeToLock);
        m_lockOff = std::min(m_lockOff, OffsetToLock);
        m_lockSize = hi - m_lockOff;
    }
    m_lockCount++;
    return D3D_OK;
}

template <class I, class DESC>
HRESULT Buffer<I, DESC>::Unlock() {
    std::lock_guard<std::recursive_mutex> lk(this->m_dev->Mutex());
    if (m_lockCount <= 0) return D3DERR_INVALIDCALL;
    if (--m_lockCount) return D3D_OK;
    if (m_noCopy) {
        // The staging blocks, in lock order, through the queue: draws queued before this point
        // still see the old contents, like with D3D9. Read-only blocks go nowhere.
        for (auto &st : m_stage) {
            if (st.readOnly) continue;
            this->AddRef();
            UINT off = st.offset;
            this->m_dev->Defer([this, off, bytes = std::move(st.bytes)]() {
                UploadRange(off, bytes.data(), (UINT) bytes.size());
                this->Release();
            });
        }
        m_stage.clear();
        return D3D_OK;
    }
    if (!m_lockSize) return D3D_OK;
    if (this->m_dev->OnGLThread()) {
        if (m_dirtyHi == m_dirtyLo) {
            m_dirtyLo = m_lockOff;
            m_dirtyHi = m_lockOff + m_lockSize;
        } else {
            m_dirtyLo = std::min(m_dirtyLo, m_lockOff);
            m_dirtyHi = std::max(m_dirtyHi, m_lockOff + m_lockSize);
        }
    } else {
        // Another thread wrote the buffer: queue the bytes as they are now, so draws queued
        // before and after this point see the contents they would see with D3D9.
        m_writtenOffThread = true;
        UINT off = m_lockOff;
        std::vector<uint8_t> bytes(m_data.begin() + off, m_data.begin() + off + m_lockSize);
        this->AddRef();
        this->m_dev->Defer([this, off, bytes]() {
            UploadRange(off, bytes.data(), (UINT) bytes.size());
            this->Release();
        });
    }
    return D3D_OK;
}

// Creates the GL buffer. Contents come from the CPU copy only for buffers written on the GL thread
// alone; buffers written elsewhere, or without a CPU copy, start zeroed and receive their queued
// snapshots or staging blocks.
template <class I, class DESC>
void Buffer<I, DESC>::Allocate(GLenum target) {
    glGenBuffers(1, &m_gl);
    GLObjects(GLO_BUFFER, 1);
    this->m_dev->BindBufferGL(target, m_gl);
    GLenum usage = (m_usage & D3DUSAGE_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    if (m_writtenOffThread || m_noCopy) {
        glBufferData(target, m_length, nullptr, usage);
        // Ranges the GL thread itself wrote are already in the CPU copy and still pending.
        if (m_dirtyHi > m_dirtyLo) glBufferSubData(target, m_dirtyLo, m_dirtyHi - m_dirtyLo, m_data.data() + m_dirtyLo);
    } else {
        glBufferData(target, m_length, m_data.data(), usage);
    }
    m_dirtyLo = m_dirtyHi = 0;
    m_allocated = true;
}

template <class I, class DESC>
GLuint Buffer<I, DESC>::Bind(GLenum target) {
    if (!m_gl) {
        Allocate(target);
        return m_gl;
    }
    this->m_dev->BindBufferGL(target, m_gl);
    if (m_dirtyHi > m_dirtyLo) {
        glBufferSubData(target, m_dirtyLo, m_dirtyHi - m_dirtyLo, m_data.data() + m_dirtyLo);
        m_dirtyLo = m_dirtyHi = 0;
    }
    return m_gl;
}

// GL thread, from the queue: applies one snapshot taken at an Unlock on another thread.
template <class I, class DESC>
void Buffer<I, DESC>::UploadRange(UINT offset, const uint8_t *bytes, UINT size) {
    GLenum target = std::is_same<I, IDirect3DIndexBuffer9>::value ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    if (!m_gl) Allocate(target);
    else this->m_dev->BindBufferGL(target, m_gl);
    glBufferSubData(target, offset, size, bytes);
}

template <class I, class DESC>
void Buffer<I, DESC>::ReleaseGL() {
    if (m_gl) {
        std::lock_guard<std::recursive_mutex> lk(this->m_dev->Mutex());
        this->m_dev->DeleteGLLater(GL_BUFFER, m_gl);
        m_gl = 0;
    }
}

template class Buffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC>;
template class Buffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC>;

HRESULT VertexBuffer::GetDesc(D3DVERTEXBUFFER_DESC *pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = D3DFMT_VERTEXDATA;
    pDesc->Type = D3DRTYPE_VERTEXBUFFER;
    pDesc->Usage = m_usage;
    pDesc->Pool = m_pool;
    pDesc->Size = m_length;
    pDesc->FVF = m_fvf;
    return D3D_OK;
}

HRESULT IndexBuffer::GetDesc(D3DINDEXBUFFER_DESC *pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = m_format;
    pDesc->Type = D3DRTYPE_INDEXBUFFER;
    pDesc->Usage = m_usage;
    pDesc->Pool = m_pool;
    pDesc->Size = m_length;
    return D3D_OK;
}

// --- Vertex declaration ----------------------------------------------------------------------

VertexDecl::VertexDecl(Device *dev, const D3DVERTEXELEMENT9 *elements, DWORD fvf) : m_dev(dev), m_fvf(fvf) {
    for (const D3DVERTEXELEMENT9 *e = elements; e->Stream != 0xFF; e++) m_elements.push_back(*e);
}

HRESULT VertexDecl::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DVertexDeclaration9) {
        AddRef();
        *ppv = static_cast<IDirect3DVertexDeclaration9 *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

HRESULT VertexDecl::GetDevice(IDirect3DDevice9 **ppDevice) {
    if (!ppDevice) return D3DERR_INVALIDCALL;
    m_dev->AddRef();
    *ppDevice = m_dev;
    return D3D_OK;
}

HRESULT VertexDecl::GetDeclaration(D3DVERTEXELEMENT9 *pElement, UINT *pNumElements) {
    if (!pNumElements) return D3DERR_INVALIDCALL;
    UINT n = (UINT) m_elements.size() + 1;
    if (pElement) {
        memcpy(pElement, m_elements.data(), m_elements.size() * sizeof(D3DVERTEXELEMENT9));
        pElement[m_elements.size()] = D3DVERTEXELEMENT9{0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0};
    }
    *pNumElements = n;
    return D3D_OK;
}

std::vector<D3DVERTEXELEMENT9> ElementsFromFVF(DWORD fvf) {
    std::vector<D3DVERTEXELEMENT9> el;
    WORD off = 0;
    auto push = [&](BYTE type, BYTE usage, BYTE index, WORD size) {
        el.push_back(D3DVERTEXELEMENT9{0, off, type, D3DDECLMETHOD_DEFAULT, usage, index});
        off += size;
    };
    DWORD pos = fvf & D3DFVF_POSITION_MASK;
    switch (pos) {
    case D3DFVF_XYZ: push(D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0, 12); break;
    case D3DFVF_XYZW: push(D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0, 16); break;
    case D3DFVF_XYZRHW: push(D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITIONT, 0, 16); break;
    case D3DFVF_XYZB1:
    case D3DFVF_XYZB2:
    case D3DFVF_XYZB3:
    case D3DFVF_XYZB4:
    case D3DFVF_XYZB5: {
        push(D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0, 12);
        int betas = (int) ((pos - D3DFVF_XYZB1) / 2) + 1;
        bool lastIndices = (fvf & (D3DFVF_LASTBETA_UBYTE4 | D3DFVF_LASTBETA_D3DCOLOR)) != 0;
        int weights = lastIndices ? betas - 1 : betas;
        if (weights > 0) push((BYTE) (D3DDECLTYPE_FLOAT1 + std::min(weights, 4) - 1), D3DDECLUSAGE_BLENDWEIGHT, 0, (WORD) (4 * weights));
        if (lastIndices)
            push((fvf & D3DFVF_LASTBETA_D3DCOLOR) ? D3DDECLTYPE_D3DCOLOR : D3DDECLTYPE_UBYTE4, D3DDECLUSAGE_BLENDINDICES, 0, 4);
        break;
    }
    default: break;
    }
    if (fvf & D3DFVF_NORMAL) push(D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0, 12);
    if (fvf & D3DFVF_PSIZE) push(D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_PSIZE, 0, 4);
    if (fvf & D3DFVF_DIFFUSE) push(D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0, 4);
    if (fvf & D3DFVF_SPECULAR) push(D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1, 4);
    UINT count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (UINT i = 0; i < count && i < 8; i++) {
        switch ((fvf >> (16 + 2 * i)) & 3) {
        case D3DFVF_TEXTUREFORMAT2: push(D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, (BYTE) i, 8); break;
        case D3DFVF_TEXTUREFORMAT3: push(D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, (BYTE) i, 12); break;
        case D3DFVF_TEXTUREFORMAT4: push(D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, (BYTE) i, 16); break;
        case D3DFVF_TEXTUREFORMAT1: push(D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_TEXCOORD, (BYTE) i, 4); break;
        }
    }
    return el;
}

UINT FVFStride(DWORD fvf) {
    static const UINT typeSize[] = {4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8};
    UINT stride = 0;
    for (const D3DVERTEXELEMENT9 &e : ElementsFromFVF(fvf)) stride = std::max<UINT>(stride, e.Offset + typeSize[e.Type]);
    return stride;
}

}  // namespace d3d9gl
