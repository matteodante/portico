// IDirect3DDevice9 on GLES3 (WebGL2 subset).
//
// Threading: GL is used only on the thread that created the
// device. Calls from other threads update the CPU-side state at once (so Get* answer) and queue
// the GL work, which the GL thread runs, in call order, before its own next call or in
// Portico_D3D9_Service(). A queued call never waits for the GL thread.
#pragma once

#include "common.h"
#include "ffp.h"
#include "resources.h"

namespace d3d9gl {

class Direct3D;
struct Program;
struct FFPKey;

constexpr int kStages = 8;
constexpr int kSamplers = 16;
constexpr int kStreams = 16;
constexpr int kMaxWorld = 256;
constexpr int kMaxLights = 8;

// Complete D3D9 device state used by the fixed-function pipeline. The device keeps two copies:
// `api` follows every call immediately (answers Get*), `gl` is what the GL thread applies.
struct State {
    DWORD rs[256];
    DWORD tss[kStages][33];
    DWORD samp[kSamplers][14];
    D3DMATRIX view, proj, tex[kStages];
    D3DMATRIX world[kMaxWorld];
    D3DMATERIAL9 material;
    std::vector<std::pair<DWORD, D3DLIGHT9>> lights;  // index -> light
    std::vector<DWORD> enabledLights;                  // in enable order, at most 8 are used
    D3DVIEWPORT9 viewport;
    IDirect3DBaseTexture9 *textures[kSamplers];
    struct Stream { VertexBuffer *vb; UINT offset, stride; } streams[kStreams];
    IndexBuffer *indices;
    VertexDecl *decl;
    DWORD fvf;
    Surface *rt;
    Surface *depth;
    IDirect3DVertexShader9 *vs;
    IDirect3DPixelShader9 *ps;

    void SetDefaults(bool zenable);
    const D3DLIGHT9 *FindLight(DWORD index) const;
};

class Device : public Unknown<IDirect3DDevice9> {
public:
    Device(Direct3D *d3d, HWND hwnd, DWORD behavior, const D3DPRESENT_PARAMETERS &pp);
    ~Device() override;
    bool Init();

    // Threading helpers.
    std::recursive_mutex &Mutex() { return m_mx; }
    bool OnGLThread() const { return CurrentThreadId() == m_glThread; }
    void Service();                             // GL thread: run queued work
    void Defer(std::function<void()> fn);       // any thread, mutex held
    void DeleteGLLater(GLenum kind, GLuint name);  // any thread, mutex held

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;

    // IDirect3DDevice9
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override;
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override;
    HRESULT STDMETHODCALLTYPE EvictManagedResources() override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9 **ppD3D9) override;
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9 *pCaps) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override;
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override;
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT, UINT, IDirect3DSurface9 *) override { return D3D_OK; }
    void STDMETHODCALLTYPE SetCursorPosition(int, int, DWORD) override {}
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL) override { return FALSE; }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *, IDirect3DSwapChain9 **) override;
    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT, IDirect3DSwapChain9 **) override;
    UINT STDMETHODCALLTYPE GetNumberOfSwapChains() override { return 1; }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
    HRESULT STDMETHODCALLTYPE Present(CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *) override;
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT, D3DRASTER_STATUS *) override;
    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL) override { return D3D_OK; }
    void STDMETHODCALLTYPE SetGammaRamp(UINT, DWORD, CONST D3DGAMMARAMP *pRamp) override;
    void STDMETHODCALLTYPE GetGammaRamp(UINT, D3DGAMMARAMP *pRamp) override;
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override;
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9 **, HANDLE *) override;
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9 **, HANDLE *) override;
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override;
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) override;
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, DWORD, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *) override;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, DWORD, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *) override;
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9 *pSourceSurface, CONST RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, CONST POINT *pDestPoint) override;
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) override;
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) override;
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT, IDirect3DSurface9 *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9 *pSourceSurface, CONST RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, CONST RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9 *pSurface, CONST RECT *pRect, D3DCOLOR color) override;
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override;
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override;
    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) override;
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) override;
    HRESULT STDMETHODCALLTYPE BeginScene() override;
    HRESULT STDMETHODCALLTYPE EndScene() override;
    HRESULT STDMETHODCALLTYPE Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix) override;
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override;
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX *) override;
    HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT9 *pViewport) override;
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9 *pViewport) override;
    HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL9 *pMaterial) override;
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9 *pMaterial) override;
    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, CONST D3DLIGHT9 *pLight) override;
    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT9 *pLight) override;
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable) override;
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL *pEnable) override;
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD, CONST float *) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD, float *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override;
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override;
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE, IDirect3DStateBlock9 **) override;
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override;
    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9 **) override;
    HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS9 *) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9 *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override;
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override;
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override;
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
    HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override;
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override;
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD *pNumPasses) override;
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT, CONST PALETTEENTRY *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT, PALETTEENTRY *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetScissorRect(CONST RECT *pRect) override;
    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT *pRect) override;
    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware) override { m_swvp = bSoftware; return D3D_OK; }
    BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing() override { return m_swvp; }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(float) override { return D3D_OK; }
    float STDMETHODCALLTYPE GetNPatchMode() override { return 0.0f; }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override;
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer9 *, IDirect3DVertexDeclaration9 *, DWORD) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(CONST D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl) override;
    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) override;
    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl) override;
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF) override;
    HRESULT STDMETHODCALLTYPE GetFVF(DWORD *pFVF) override;
    HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD *pFunction, IDirect3DVertexShader9 **ppShader) override;
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9 *pShader) override;
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9 **ppShader) override;
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT, CONST float *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT, float *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT, CONST int *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT, int *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT, CONST BOOL *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT, BOOL *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) override;
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *pOffsetInBytes, UINT *pStride) override;
    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT, UINT *pSetting) override { if (pSetting) *pSetting = 1; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9 *pIndexData) override;
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9 **ppIndexData) override;
    HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD *pFunction, IDirect3DPixelShader9 **ppShader) override;
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9 *pShader) override;
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9 **ppShader) override;
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT, CONST float *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT, float *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT, CONST int *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT, int *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT, CONST BOOL *, UINT) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT, BOOL *, UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT, CONST float *, CONST D3DRECTPATCH_INFO *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT, CONST float *, CONST D3DTRIPATCH_INFO *) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT) override { return D3DERR_INVALIDCALL; }
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE, IDirect3DQuery9 **) override { return D3DERR_NOTAVAILABLE; }

    Surface *BackBuffer() const { return m_backBuffer; }

    // GL binding cache (GL thread). WebGL calls cost more than native ones (most in Firefox), so
    // binds and attribute setups that would change nothing are skipped. Every bind of these kinds
    // goes through here; a deleted object leaves every binding (ForgetGL).
    void BindBufferGL(GLenum target, GLuint name);
    void ActiveTextureGL(GLuint unit);
    void BindTextureGL(GLuint name);  // GL_TEXTURE_2D on the active unit
    void ForgetGL(GLenum kind, GLuint name);

private:
    // The device lives as long as the process: resources may still be released after the program
    // releases the device (exit paths), and they reach the device for its mutex.
    void Destroy() override { ReleaseState(m_gl); }

    // Runs fn now on the GL thread (after the queue), otherwise queues it.
    template <class F> void Exec(F &&fn);
    void Drain();
    void ApplyDefaultsGL();
    bool CreateBackBuffer();
    void ReleaseState(State &s);
    void ResetStates();

    // GL-thread implementations.
    void SetRenderStateGL(DWORD state, DWORD value);
    void SetTextureGL(DWORD stage, IDirect3DBaseTexture9 *tex);
    void SetStreamGL(UINT n, VertexBuffer *vb, UINT off, UINT stride);
    void SetIndicesGL(IndexBuffer *ib);
    void SetDeclGL(VertexDecl *decl);
    void SetTargetsGL(Surface *rt, Surface *depth);
    void ClearGL(std::vector<D3DRECT> rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
    void PresentGL();
    void StretchGL(Surface *src, RECT srcRect, Surface *dst, RECT dstRect, D3DTEXTUREFILTERTYPE filter);
    struct DrawCall {
        D3DPRIMITIVETYPE type;
        UINT primCount;
        bool indexed;
        INT baseVertex;
        UINT startVertex;     // non-indexed
        UINT startIndex;      // indexed
        // UP draws carry their data.
        std::vector<uint8_t> upVertices;
        UINT upStride = 0;
        std::vector<uint8_t> upIndices;
        D3DFORMAT upIndexFormat = D3DFMT_INDEX16;
        bool up = false;
    };
    void DrawGL(const DrawCall &dc);

    // Draw helpers (device_draw.cpp).
    bool BindFramebuffer();
    void ApplyFixedState();
    bool SetupVertexInput(const DrawCall &dc, FFPKey &key);
    void BindTextures(FFPKey &key);
    void UploadUniforms(Program *prog, const FFPKey &key);
    GLuint GetSampler(DWORD stage);
    GLuint GetFramebuffer(Surface *rt, Surface *depth);

    Direct3D *m_d3d;
    HWND m_hwnd;
    DWORD m_behavior;
    D3DPRESENT_PARAMETERS m_pp;
    uintptr_t m_glThread;

    std::recursive_mutex m_mx;
    std::vector<std::function<void()>> m_queue;
    std::atomic<bool> m_queued{false};
    int m_queuedPresents = 0;      // presents waiting in m_queue
    bool m_skipDraws = false;      // replaying a backlog: only the last queued frame is drawn
    bool m_contextLost = false;    // the GL context is gone: nothing is drawn any more
    unsigned m_presents = 0;
    bool Drawing() const { return !m_skipDraws && !m_contextLost; }
    void OnContextLost();
    bool TestContextLoss();
    std::vector<std::pair<GLenum, GLuint>> m_deadGL;

    State m_api{};
    State m_gl{};
    Surface *m_backBuffer = nullptr;
    Surface *m_autoDepth = nullptr;
    BOOL m_swvp = FALSE;
    D3DGAMMARAMP m_gamma{};
    RECT m_scissor{};
    bool m_inScene = false;

    // GL objects.
    GLuint m_vao = 0;
    GLuint m_upVB = 0, m_upIB = 0;
    UINT m_upVBSize = 0, m_upIBSize = 0;
    GLuint m_bonesUBO = 0;
    std::unordered_map<uint64_t, GLuint> m_samplers;
    uint64_t m_stageSamplerKey[kStages] = {};  // the last sampler key of each stage and its sampler (no map lookup)
    GLuint m_stageSampler[kStages] = {};
    // The render target and depth of the last BindFramebuffer and the framebuffer it bound: while they are
    // the same and nothing else bound another framebuffer (m_boundFBO), a draw needs no lookup.
    Surface *m_fbRT = nullptr, *m_fbDepth = nullptr;
    GLuint m_fbFBO = 0;
    std::unordered_map<uint64_t, GLuint> m_fbos;
    std::unordered_map<uint32_t, Surface *> m_sizedDepth;  // depth buffers matching smaller render targets
    uint32_t m_enabledAttribs = 0;
    GLuint m_boundFBO = ~0u;
    GLuint m_boundArray = ~0u, m_boundElement = ~0u;
    GLuint m_activeUnit = ~0u;
    GLuint m_unitTexture[kStages + 1];  // the last unit is the one used outside draws
    GLuint m_unitSampler[kStages];
    struct AttribPointer {
        GLuint buffer;
        GLint size;
        GLenum type;
        GLboolean normalized;
        GLsizei stride;
        size_t offset;
    };
    AttribPointer m_attrib[16];  // by attribute location (A_COUNT, ffp.h)
    D3DVIEWPORT9 m_glViewport;
    bool m_glViewportSet = false;
    void ResetBindingCache();

    // Change tracking for uniforms: each group has a generation number, bumped on change.
    // G_TRANSFORM: world 0 * view (and the normal matrix); G_VIEW and G_PROJ: the view and projection alone.
    enum Group { G_TRANSFORM, G_VIEW, G_PROJ, G_WORLD, G_LIGHTS, G_MATERIAL, G_FOG, G_TFACTOR, G_TEXMAT, G_VIEWPORT, G_ALPHAREF,
                 G_STAGE, G_COUNT };
    uint32_t m_gen[G_COUNT] = {};
    uint32_t m_bonesGen = ~0u;
    UINT m_worldHigh = 0;          // highest world matrix index set + 1
    bool m_fixedDirty = true;      // blend/depth/cull/... must be re-applied
    // The fixed GL state last applied by ApplyFixedState, so that only what changed is sent (most render state
    // changes leave it as it was). valid = false after another path (clear, present, fill) touched it.
    struct FixedGL {
        bool valid = false;
        bool blend, depthTest, cull, polyOffset, stencil;
        // Whether the parameters of each feature are known: they are sent only while it is on.
        bool blendKnown, depthKnown, cullKnown, offsetKnown, stencilKnown;
        GLenum blendSrc, blendDst, blendSrcA, blendDstA, blendEq, blendEqA;
        DWORD blendColor;
        GLenum depthFunc, frontFace;
        GLboolean depthMask;
        DWORD colorWrite;
        float slope, bias;
        GLenum stencilFunc, stencilFail, stencilZFail, stencilPass;
        GLint stencilRef;
        GLuint stencilMask, stencilWriteMask;
    } m_fixedGL;
    void InvalidateFixedGL() { m_fixedDirty = true; m_fixedGL.valid = false; }
    std::unordered_map<std::string, Program *> m_programs;
    Program *m_lastProgram = nullptr;
    // The key of the last draw and its program: consecutive draws mostly share it (no map lookup then).
    FFPKey m_lastKey;
    Program *m_lastKeyProgram = nullptr;

    friend class Texture;
    friend class Surface;
};

extern Device *g_device;  // the device of this process (one window, one device)

}  // namespace d3d9gl
