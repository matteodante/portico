// D3D9 resources of the GL layer. Every resource keeps a CPU copy of its contents (Lock/Unlock
// write there, never into mapped GL memory) and creates or updates its GL object lazily on the
// GL thread.
#pragma once

#include "common.h"

namespace d3d9gl {

class Texture;

// Resource base: device link, priority, private data (kept minimal).
template <class I>
class Resource : public Unknown<I> {
public:
    explicit Resource(Device *dev);
    ~Resource() override;

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, CONST void *, DWORD, DWORD) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD p) override { DWORD o = m_priority; m_priority = p; return o; }
    DWORD STDMETHODCALLTYPE GetPriority() override { return m_priority; }
    void STDMETHODCALLTYPE PreLoad() override {}

    Device *GetOwner() const { return m_dev; }

protected:
    Device *m_dev;
    DWORD m_priority = 0;
};

// --- Surface -------------------------------------------------------------------------------

enum class SurfaceKind { TextureLevel, Offscreen, DepthStencil, RenderTarget };

class Surface : public Resource<IDirect3DSurface9> {
public:
    // Texture level: reference counting goes to the texture (D3D9 container semantics).
    Surface(Device *dev, Texture *tex, UINT level);
    // Standalone surface: offscreen plain (CPU only), depth stencil, render target (own texture).
    Surface(Device *dev, SurfaceKind kind, UINT width, UINT height, D3DFORMAT fmt, D3DPOOL pool);
    ~Surface() override;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDirect3DResource9
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_SURFACE; }

    // IDirect3DSurface9
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer) override;
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) override;
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE UnlockRect() override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC) override { return D3DERR_INVALIDCALL; }

    SurfaceKind Kind() const { return m_kind; }
    Texture *GetTexture() const { return m_tex; }  // texture level / render target backing
    UINT Level() const { return m_level; }
    UINT Width() const { return m_desc.Width; }
    UINT Height() const { return m_desc.Height; }
    D3DFORMAT Format() const { return m_desc.Format; }

    // CPU bytes of this surface (texture level or offscreen data); null for GPU-only surfaces.
    uint8_t *Data();
    UINT Pitch() const;
    void MarkDirty();

    // GL thread only.
    GLuint GLRenderbuffer();       // depth stencil
    void BindAsColor(GLenum target);  // attaches to the bound framebuffer
    void ReleaseGL();

private:
    SurfaceKind m_kind;
    Texture *m_tex = nullptr;     // container (TextureLevel) or own backing texture (RenderTarget)
    UINT m_level = 0;
    D3DSURFACE_DESC m_desc = {};
    std::vector<uint8_t> m_data;  // Offscreen
    GLuint m_rb = 0;              // DepthStencil
    bool m_locked = false;
};

// --- Texture -------------------------------------------------------------------------------

class Texture : public Resource<IDirect3DTexture9> {
public:
    Texture(Device *dev, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool);
    ~Texture() override;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_TEXTURE; }

    // IDirect3DBaseTexture9
    DWORD STDMETHODCALLTYPE SetLOD(DWORD lod) override { DWORD o = m_lod; m_lod = lod; return o; }
    DWORD STDMETHODCALLTYPE GetLOD() override { return m_lod; }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { return (DWORD) m_levels.size(); }
    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return D3D_OK; }
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override { return D3DTEXF_LINEAR; }
    void STDMETHODCALLTYPE GenerateMipSubLevels() override;

    // IDirect3DTexture9
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override;
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override;
    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT *) override { return D3D_OK; }

    struct Level {
        UINT width, height, pitch;
        std::vector<uint8_t> data;
        bool dirty = false;
        bool locked = false;
        bool uploaded = false;  // its contents went to GL
        bool dropped = false;   // its CPU copy was freed after the upload
    };

    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }
    D3DFORMAT Format() const { return m_format; }
    DWORD Usage() const { return m_usage; }
    D3DPOOL Pool() const { return m_pool; }
    bool IsRenderTarget() const { return (m_usage & D3DUSAGE_RENDERTARGET) != 0; }
    Level &GetLevel(UINT i) { return m_levels[i]; }
    uint8_t *LevelData(UINT i);
    void MarkDirty(UINT level);

    // GL thread only: creates the GL texture and uploads dirty levels.
    GLuint GLTexture();
    void SetBaseLevel(UINT level);
    void ReleaseGL();
    // Read the color contents back (render targets only), RGBA8 rows top to bottom.
    bool ReadPixels(UINT level, std::vector<uint8_t> &rgba);
    void NoteRendered() { m_gpuNewer = true; }
    void NoteTouch(int kind, UINT level, DWORD flags);  // statistics: access after upload

private:
    void Upload(UINT level);
    void DropCPUCopy();

    UINT m_width, m_height;
    DWORD m_usage;
    D3DFORMAT m_format;
    D3DPOOL m_pool;
    DWORD m_lod = 0;
    std::vector<Level> m_levels;
    std::vector<Surface *> m_surfaces;
    GLuint m_gl = 0;
    bool m_allocated = false;
    bool m_anyDirty = false;
    bool m_genMips = false;
    bool m_gpuNewer = false;  // a render target holds data the CPU copy does not have
    UINT m_baseLevel = 0;
    int64_t m_cpuBytes = 0;
    unsigned m_lastTouch = 0;  // present count at the last CPU access (statistics)
    int CpuKind() const;
};

// --- Buffers -------------------------------------------------------------------------------

template <class I, class DESC>
class Buffer : public Resource<I> {
public:
    Buffer(Device *dev, UINT length, DWORD usage, D3DPOOL pool);
    ~Buffer() override;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
    HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE Unlock() override;

    UINT Length() const { return m_length; }
    // GL thread only: creates the GL buffer, uploads the dirty range and binds it to target.
    GLuint Bind(GLenum target);
    void UploadRange(UINT offset, const uint8_t *bytes, UINT size);  // GL thread
    void ReleaseGL();

protected:
    UINT m_length;
    DWORD m_usage;
    D3DPOOL m_pool;
    std::vector<uint8_t> m_data;
    GLuint m_gl = 0;
    UINT m_dirtyLo = 0, m_dirtyHi = 0;
    UINT m_lockOff = 0, m_lockSize = 0;
    int m_lockCount = 0;
    bool m_allocated = false;
    int CpuKind() const;
    unsigned m_lastTouch = 0;  // present count at the last Lock (statistics)
    // No CPU copy (see NoCPUCopy): each Lock gets a staging block of the locked range with the
    // buffer's contents, which goes to GL at the Unlock (unless the lock was read-only).
    bool m_noCopy = false;
    struct Stage {
        UINT offset;
        std::vector<uint8_t> bytes;
        bool readOnly;
    };
    std::vector<Stage> m_stage;
    std::vector<std::pair<UINT, UINT>> m_written;  // [begin, end) ranges ever written, sorted, merged
    bool NoCPUCopy() const;
    bool Written(UINT begin, UINT end) const;
    void NoteWritten(UINT begin, UINT end);
    void ReadBack(UINT offset, UINT size, uint8_t *out);  // GL thread
    // Written by a thread other than the GL thread: its contents reach GL only through the
    // snapshots queued at each Unlock, in queue order, never from the live CPU copy (which that
    // thread may already have overwritten, or may be writing while the GL thread reads it).
    bool m_writtenOffThread = false;
    void Allocate(GLenum target);
};

class VertexBuffer : public Buffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC> {
public:
    VertexBuffer(Device *dev, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
        : Buffer(dev, length, usage, pool), m_fvf(fvf) {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_VERTEXBUFFER; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override;
    static constexpr GLenum Target = GL_ARRAY_BUFFER;

private:
    DWORD m_fvf;
};

class IndexBuffer : public Buffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC> {
public:
    IndexBuffer(Device *dev, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
        : Buffer(dev, length, usage, pool), m_format(fmt) {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_INDEXBUFFER; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *pDesc) override;
    D3DFORMAT Format() const { return m_format; }
    static constexpr GLenum Target = GL_ELEMENT_ARRAY_BUFFER;

private:
    D3DFORMAT m_format;
};

// --- Vertex declaration ----------------------------------------------------------------------

class VertexDecl : public Unknown<IDirect3DVertexDeclaration9> {
public:
    VertexDecl(Device *dev, const D3DVERTEXELEMENT9 *elements, DWORD fvf);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9 *pElement, UINT *pNumElements) override;

    const std::vector<D3DVERTEXELEMENT9> &Elements() const { return m_elements; }  // without END
    DWORD Fvf() const { return m_fvf; }

private:
    Device *m_dev;
    std::vector<D3DVERTEXELEMENT9> m_elements;
    DWORD m_fvf;
};

// Builds the element list D3D9 derives from an FVF code.
std::vector<D3DVERTEXELEMENT9> ElementsFromFVF(DWORD fvf);
UINT FVFStride(DWORD fvf);

// --- Shaders (accepted and ignored: the device declares no VS/PS 3.0, so programs use the
// fixed-function path). ----

}  // namespace d3d9gl
