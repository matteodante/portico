// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Drawing on the GL thread: framebuffers, fixed GL state, vertex input, textures, uniforms.
#include "device.h"
#include "ffp.h"

#include <algorithm>
#include <cmath>

namespace d3d9gl {

static float DWToF(DWORD d) {
    float f;
    memcpy(&f, &d, 4);
    return f;
}

static void ColorToVec4(D3DCOLOR c, float *v) {
    v[0] = ((c >> 16) & 255) / 255.0f;
    v[1] = ((c >> 8) & 255) / 255.0f;
    v[2] = (c & 255) / 255.0f;
    v[3] = ((c >> 24) & 255) / 255.0f;
}

static void Mul(const D3DMATRIX &a, const D3DMATRIX &b, D3DMATRIX &out) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            float v = 0;
            for (int k = 0; k < 4; k++) v += a.m[r][k] * b.m[k][c];
            out.m[r][c] = v;
        }
}

// --- Framebuffers --------------------------------------------------------------------------

GLuint Device::GetFramebuffer(Surface *rt, Surface *depth) {
    Texture *tex = rt ? rt->GetTexture() : nullptr;
    if (!tex) return 0;
    GLuint color = tex->GLTexture();
    GLuint rb = depth ? depth->GLRenderbuffer() : 0;
    uint64_t key = (uint64_t(color) << 32) | rb;
    auto it = m_fbos.find(key);
    if (it != m_fbos.end()) return it->second;
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, rt->Level());
    if (rb) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        Log("framebuffer %ux%u fmt %u depth %p incomplete 0x%x", rt->Width(), rt->Height(), rt->Format(), depth, status);
    m_boundFBO = fbo;
    m_fbos[key] = fbo;
    GLObjects(GLO_FRAMEBUFFER, 1);
    return fbo;
}

bool Device::BindFramebuffer() {
    Surface *rt = m_gl.rt;
    if (!rt) return false;
    if (rt == m_fbRT && m_gl.depth == m_fbDepth && m_fbFBO && m_boundFBO == m_fbFBO) {
        rt->GetTexture()->NoteRendered();
        return true;
    }
    Surface *depth = m_gl.depth;
    if (depth && (depth->Width() < rt->Width() || depth->Height() < rt->Height())) depth = nullptr;
    if (depth && (depth->Width() != rt->Width() || depth->Height() != rt->Height())) {
        // D3D9 renders into the top-left part of a larger depth buffer; WebGL2 wants attachments
        // of equal size, so a render target gets its own depth buffer of its size (not shared with
        // the bigger one: render-to-texture passes clear it themselves).
        uint32_t key = (rt->Width() << 16) | rt->Height();
        Surface *&own = m_sizedDepth[key];
        if (!own) own = new Surface(this, SurfaceKind::DepthStencil, rt->Width(), rt->Height(), D3DFMT_D24S8, D3DPOOL_DEFAULT);
        depth = own;
    }
    GLuint fbo = GetFramebuffer(rt, depth);
    if (fbo != m_boundFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        m_boundFBO = fbo;
    }
    m_fbRT = rt, m_fbDepth = m_gl.depth, m_fbFBO = fbo;
    if (rt->GetTexture()) rt->GetTexture()->NoteRendered();
    return fbo != 0;
}

// --- Fixed GL state ------------------------------------------------------------------------

static GLenum BlendFactor(DWORD b) {
    switch (b) {
    case D3DBLEND_ZERO: return GL_ZERO;
    case D3DBLEND_ONE: return GL_ONE;
    case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
    case D3DBLEND_BLENDFACTOR: return GL_CONSTANT_COLOR;
    case D3DBLEND_INVBLENDFACTOR: return GL_ONE_MINUS_CONSTANT_COLOR;
    default: return GL_ONE;
    }
}

static GLenum BlendOp(DWORD op) {
    switch (op) {
    case D3DBLENDOP_SUBTRACT: return GL_FUNC_SUBTRACT;
    case D3DBLENDOP_REVSUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case D3DBLENDOP_MIN: return GL_MIN;
    case D3DBLENDOP_MAX: return GL_MAX;
    default: return GL_FUNC_ADD;
    }
}

static GLenum CompareFunc(DWORD f) {
    switch (f) {
    case D3DCMP_NEVER: return GL_NEVER;
    case D3DCMP_LESS: return GL_LESS;
    case D3DCMP_EQUAL: return GL_EQUAL;
    case D3DCMP_LESSEQUAL: return GL_LEQUAL;
    case D3DCMP_GREATER: return GL_GREATER;
    case D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
    case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
    default: return GL_ALWAYS;
    }
}

static GLenum StencilOp(DWORD op) {
    switch (op) {
    case D3DSTENCILOP_ZERO: return GL_ZERO;
    case D3DSTENCILOP_REPLACE: return GL_REPLACE;
    case D3DSTENCILOP_INCRSAT: return GL_INCR;
    case D3DSTENCILOP_DECRSAT: return GL_DECR;
    case D3DSTENCILOP_INVERT: return GL_INVERT;
    case D3DSTENCILOP_INCR: return GL_INCR_WRAP;
    case D3DSTENCILOP_DECR: return GL_DECR_WRAP;
    default: return GL_KEEP;
    }
}

void Device::ApplyFixedState() {
    if (!m_fixedDirty) return;
    m_fixedDirty = false;
    const DWORD *rs = m_gl.rs;
    FixedGL &g = m_fixedGL;
    const bool all = !g.valid;
    if (all) g.blendKnown = g.depthKnown = g.cullKnown = g.offsetKnown = g.stencilKnown = false;
    g.valid = true;
    // Sends a GL state only when it differs from what was applied last (or after another path touched it).
    auto enable = [all](bool &cur, bool want, GLenum cap) {
        if (!all && cur == want) return;
        cur = want;
        if (want) glEnable(cap);
        else glDisable(cap);
    };

    bool blend = rs[D3DRS_ALPHABLENDENABLE] != 0;
    enable(g.blend, blend, GL_BLEND);
    if (blend) {
        auto factors = [](DWORD src, DWORD dst, GLenum &s, GLenum &d) {
            if (src == D3DBLEND_BOTHSRCALPHA) { s = GL_SRC_ALPHA; d = GL_ONE_MINUS_SRC_ALPHA; return; }
            if (src == D3DBLEND_BOTHINVSRCALPHA) { s = GL_ONE_MINUS_SRC_ALPHA; d = GL_SRC_ALPHA; return; }
            s = BlendFactor(src);
            d = BlendFactor(dst);
        };
        GLenum s, d, sa, da, eq = BlendOp(rs[D3DRS_BLENDOP]), eqa = eq;
        factors(rs[D3DRS_SRCBLEND], rs[D3DRS_DESTBLEND], s, d);
        sa = s, da = d;
        if (rs[D3DRS_SEPARATEALPHABLENDENABLE]) {
            factors(rs[D3DRS_SRCBLENDALPHA], rs[D3DRS_DESTBLENDALPHA], sa, da);
            eqa = BlendOp(rs[D3DRS_BLENDOPALPHA]);
        }
        bool known = g.blendKnown;
        g.blendKnown = true;
        if (!known || g.blendSrc != s || g.blendDst != d || g.blendSrcA != sa || g.blendDstA != da) {
            g.blendSrc = s, g.blendDst = d, g.blendSrcA = sa, g.blendDstA = da;
            glBlendFuncSeparate(s, d, sa, da);
        }
        if (!known || g.blendEq != eq || g.blendEqA != eqa) {
            g.blendEq = eq, g.blendEqA = eqa;
            glBlendEquationSeparate(eq, eqa);
        }
        if (!known || g.blendColor != rs[D3DRS_BLENDFACTOR]) {
            g.blendColor = rs[D3DRS_BLENDFACTOR];
            float bf[4];
            ColorToVec4(g.blendColor, bf);
            glBlendColor(bf[0], bf[1], bf[2], bf[3]);
        }
    }

    bool depth = rs[D3DRS_ZENABLE] != 0;
    enable(g.depthTest, depth, GL_DEPTH_TEST);
    if (depth) {
        GLenum f = CompareFunc(rs[D3DRS_ZFUNC]);
        if (!g.depthKnown || g.depthFunc != f) glDepthFunc(g.depthFunc = f);
        g.depthKnown = true;
    }
    GLboolean zw = rs[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE;
    if (all || g.depthMask != zw) glDepthMask(g.depthMask = zw);

    // Render targets keep the D3D row order (y flipped in the vertex shader), which mirrors the
    // winding: a D3D clockwise triangle is counter-clockwise for GL.
    DWORD cull = rs[D3DRS_CULLMODE];
    enable(g.cull, cull != D3DCULL_NONE, GL_CULL_FACE);
    if (cull != D3DCULL_NONE) {
        GLenum front = cull == D3DCULL_CW ? GL_CW : GL_CCW;
        if (!g.cullKnown) glCullFace(GL_BACK);
        if (!g.cullKnown || g.frontFace != front) glFrontFace(g.frontFace = front);
        g.cullKnown = true;
    }

    DWORD cw = rs[D3DRS_COLORWRITEENABLE] & 15;
    if (all || g.colorWrite != cw) {
        g.colorWrite = cw;
        glColorMask(cw & 1 ? GL_TRUE : GL_FALSE, cw & 2 ? GL_TRUE : GL_FALSE, cw & 4 ? GL_TRUE : GL_FALSE, cw & 8 ? GL_TRUE : GL_FALSE);
    }

    // D3D9 adds DEPTHBIAS to the depth value; for a 24-bit buffer that is bias * 2^24 GL units
    // (prototype measurement: trunc(float32(bias) * 2^24), docs/reference/prototype.md).
    float bias = DWToF(rs[D3DRS_DEPTHBIAS]), slope = DWToF(rs[D3DRS_SLOPESCALEDEPTHBIAS]);
    bool offset = bias != 0.0f || slope != 0.0f;
    enable(g.polyOffset, offset, GL_POLYGON_OFFSET_FILL);
    if (offset && (!g.offsetKnown || g.slope != slope || g.bias != bias)) {
        g.slope = slope, g.bias = bias, g.offsetKnown = true;
        glPolygonOffset(slope, bias * 16777216.0f);
    }

    bool stencil = rs[D3DRS_STENCILENABLE] != 0;
    enable(g.stencil, stencil, GL_STENCIL_TEST);
    if (stencil) {
        GLenum f = CompareFunc(rs[D3DRS_STENCILFUNC]);
        GLint ref = (GLint) rs[D3DRS_STENCILREF];
        GLuint mask = rs[D3DRS_STENCILMASK];
        bool known = g.stencilKnown;
        g.stencilKnown = true;
        if (!known || g.stencilFunc != f || g.stencilRef != ref || g.stencilMask != mask) {
            g.stencilFunc = f, g.stencilRef = ref, g.stencilMask = mask;
            glStencilFunc(f, ref, mask);
        }
        GLenum fail = StencilOp(rs[D3DRS_STENCILFAIL]), zfail = StencilOp(rs[D3DRS_STENCILZFAIL]),
               pass = StencilOp(rs[D3DRS_STENCILPASS]);
        if (!known || g.stencilFail != fail || g.stencilZFail != zfail || g.stencilPass != pass) {
            g.stencilFail = fail, g.stencilZFail = zfail, g.stencilPass = pass;
            glStencilOp(fail, zfail, pass);
        }
        GLuint wm = rs[D3DRS_STENCILWRITEMASK];
        if (!known || g.stencilWriteMask != wm) glStencilMask(g.stencilWriteMask = wm);
    }
    if (all) {
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DITHER);
    }
}

// --- Vertex input --------------------------------------------------------------------------

struct GLType {
    GLint size;
    GLenum type;
    GLboolean normalized;
    UINT bytes;
};

static bool DeclType(BYTE t, GLType &g) {
    switch (t) {
    case D3DDECLTYPE_FLOAT1: g = {1, GL_FLOAT, GL_FALSE, 4}; return true;
    case D3DDECLTYPE_FLOAT2: g = {2, GL_FLOAT, GL_FALSE, 8}; return true;
    case D3DDECLTYPE_FLOAT3: g = {3, GL_FLOAT, GL_FALSE, 12}; return true;
    case D3DDECLTYPE_FLOAT4: g = {4, GL_FLOAT, GL_FALSE, 16}; return true;
    case D3DDECLTYPE_D3DCOLOR: g = {4, GL_UNSIGNED_BYTE, GL_TRUE, 4}; return true;
    case D3DDECLTYPE_UBYTE4: g = {4, GL_UNSIGNED_BYTE, GL_FALSE, 4}; return true;
    case D3DDECLTYPE_UBYTE4N: g = {4, GL_UNSIGNED_BYTE, GL_TRUE, 4}; return true;
    case D3DDECLTYPE_SHORT2: g = {2, GL_SHORT, GL_FALSE, 4}; return true;
    case D3DDECLTYPE_SHORT4: g = {4, GL_SHORT, GL_FALSE, 8}; return true;
    case D3DDECLTYPE_SHORT2N: g = {2, GL_SHORT, GL_TRUE, 4}; return true;
    case D3DDECLTYPE_SHORT4N: g = {4, GL_SHORT, GL_TRUE, 8}; return true;
    case D3DDECLTYPE_USHORT2N: g = {2, GL_UNSIGNED_SHORT, GL_TRUE, 4}; return true;
    case D3DDECLTYPE_USHORT4N: g = {4, GL_UNSIGNED_SHORT, GL_TRUE, 8}; return true;
    case D3DDECLTYPE_FLOAT16_2: g = {2, GL_HALF_FLOAT, GL_FALSE, 4}; return true;
    case D3DDECLTYPE_FLOAT16_4: g = {4, GL_HALF_FLOAT, GL_FALSE, 8}; return true;
    default: return false;
    }
}

static int Location(const D3DVERTEXELEMENT9 &e) {
    switch (e.Usage) {
    case D3DDECLUSAGE_POSITION:
    case D3DDECLUSAGE_POSITIONT: return e.UsageIndex == 0 ? A_POSITION : -1;
    case D3DDECLUSAGE_BLENDWEIGHT: return A_BLENDWEIGHT;
    case D3DDECLUSAGE_BLENDINDICES: return A_BLENDINDICES;
    case D3DDECLUSAGE_NORMAL: return e.UsageIndex == 0 ? A_NORMAL : -1;
    case D3DDECLUSAGE_PSIZE: return A_PSIZE;
    case D3DDECLUSAGE_COLOR: return e.UsageIndex == 0 ? A_DIFFUSE : e.UsageIndex == 1 ? A_SPECULAR : -1;
    case D3DDECLUSAGE_TEXCOORD: return e.UsageIndex < 8 ? A_TEXCOORD0 + e.UsageIndex : -1;
    default: return -1;
    }
}

static_assert(A_COUNT <= 16, "m_attrib holds 16 locations");

bool Device::SetupVertexInput(const DrawCall &dc, FFPKey &key) {
    static std::unordered_map<DWORD, std::vector<D3DVERTEXELEMENT9>> s_fvf;
    const std::vector<D3DVERTEXELEMENT9> *elements;
    if (m_gl.decl) {
        elements = &m_gl.decl->Elements();
    } else {
        auto it = s_fvf.find(m_gl.fvf);
        if (it == s_fvf.end()) it = s_fvf.emplace(m_gl.fvf, ElementsFromFVF(m_gl.fvf)).first;
        elements = &it->second;
    }
    if (elements->empty()) {
        D3D9GL_ONCE("draw without vertex format");
        return false;
    }

    if (dc.up) {
        BindBufferGL(GL_ARRAY_BUFFER, m_upVB);
        UINT size = (UINT) dc.upVertices.size();
        if (size > m_upVBSize) {
            m_upVBSize = std::max(size, 64u * 1024u);
            glBufferData(GL_ARRAY_BUFFER, m_upVBSize, nullptr, GL_STREAM_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, dc.upVertices.data());
    }

    uint32_t enabled = 0;
    UINT texDims[8] = {};
    bool hasWeights = false, hasIndices = false;
    for (const D3DVERTEXELEMENT9 &e : *elements) {
        int loc = Location(e);
        GLType t;
        if (loc < 0 || !DeclType(e.Type, t)) continue;
        if (enabled & (1u << loc)) continue;  // first element of a usage wins
        UINT stride;
        size_t offset;
        GLuint buffer;
        if (dc.up) {
            if (e.Stream != 0) continue;
            buffer = m_upVB;
            stride = dc.upStride;
            offset = e.Offset;
        } else {
            const State::Stream &st = m_gl.streams[e.Stream];
            if (!st.vb) continue;
            buffer = st.vb->Bind(GL_ARRAY_BUFFER);  // also uploads pending writes
            stride = st.stride;
            long long off = (long long) st.offset + e.Offset + (long long) dc.baseVertex * stride;
            if (off < 0) {
                D3D9GL_ONCE("negative base vertex offset");
                return false;
            }
            offset = (size_t) off;
        }
        AttribPointer &ap = m_attrib[loc];
        if (ap.buffer != buffer || ap.size != t.size || ap.type != t.type || ap.normalized != t.normalized ||
            ap.stride != (GLsizei) stride || ap.offset != offset) {
            BindBufferGL(GL_ARRAY_BUFFER, buffer);
            glVertexAttribPointer(loc, t.size, t.type, t.normalized, stride, (const void *) offset);
            ap = {buffer, t.size, t.type, t.normalized, (GLsizei) stride, offset};
        }
        enabled |= 1u << loc;
        switch (loc) {
        case A_POSITION: key.rhw = e.Usage == D3DDECLUSAGE_POSITIONT; break;
        case A_NORMAL: key.hasNormal = 1; break;
        case A_DIFFUSE: key.hasDiffuse = 1; key.diffuseBGRA = e.Type == D3DDECLTYPE_D3DCOLOR; break;
        case A_SPECULAR: key.hasSpecular = 1; key.specularBGRA = e.Type == D3DDECLTYPE_D3DCOLOR; break;
        case A_BLENDWEIGHT: hasWeights = true; break;
        case A_BLENDINDICES: hasIndices = true; key.indicesBGRA = e.Type == D3DDECLTYPE_D3DCOLOR; break;
        default:
            if (loc >= A_TEXCOORD0) texDims[loc - A_TEXCOORD0] = (UINT) t.size;
            break;
        }
    }
    if (!(enabled & 1)) {
        D3D9GL_ONCE("draw without position");
        return false;
    }
    uint32_t changed = enabled ^ m_enabledAttribs;
    for (int i = 0; i < A_COUNT; i++)
        if (changed & (1u << i)) {
            if (enabled & (1u << i)) glEnableVertexAttribArray(i);
            else glDisableVertexAttribArray(i);
        }
    m_enabledAttribs = enabled;

    // Vertex blending (skinning on the fixed-function path).
    DWORD vb = m_gl.rs[D3DRS_VERTEXBLEND];
    if (!key.rhw && vb != D3DVBF_DISABLE && vb != D3DVBF_TWEENING) {
        key.blendWeights = vb == D3DVBF_0WEIGHTS ? 0 : (uint8_t) std::min<DWORD>(vb, 3);
        if (key.blendWeights && !hasWeights) key.blendWeights = 0;
        key.blendMode = (m_gl.rs[D3DRS_INDEXEDVERTEXBLENDENABLE] && hasIndices) ? 2 : 1;
        if (!key.blendWeights && key.blendMode == 1) key.blendMode = 0;
    }
    if (!key.blendMode) key.indicesBGRA = 0;
    for (int i = 0; i < 8; i++) key.stage[i].inputDims = 0;
    // Store the dims temporarily in the pad of each stage (resolved in BindTextures).
    for (int i = 0; i < 8; i++) key.stage[i].pad = (uint8_t) texDims[i];

    if (dc.indexed) {
        if (dc.up) {
            BindBufferGL(GL_ELEMENT_ARRAY_BUFFER, m_upIB);
            UINT size = (UINT) dc.upIndices.size();
            if (size > m_upIBSize) {
                m_upIBSize = std::max(size, 64u * 1024u);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_upIBSize, nullptr, GL_STREAM_DRAW);
            }
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, dc.upIndices.data());
        } else {
            if (!m_gl.indices) {
                D3D9GL_ONCE("indexed draw without index buffer");
                return false;
            }
            m_gl.indices->Bind(GL_ELEMENT_ARRAY_BUFFER);
        }
    }
    return true;
}

// --- Textures and samplers -----------------------------------------------------------------

GLuint Device::GetSampler(DWORD stage) {
    const DWORD *s = m_gl.samp[stage];
    DWORD minF = s[D3DSAMP_MINFILTER], magF = s[D3DSAMP_MAGFILTER], mipF = s[D3DSAMP_MIPFILTER];
    DWORD au = s[D3DSAMP_ADDRESSU], av = s[D3DSAMP_ADDRESSV];
    DWORD aniso = (minF == D3DTEXF_ANISOTROPIC || magF == D3DTEXF_ANISOTROPIC) ? std::max<DWORD>(1, s[D3DSAMP_MAXANISOTROPY]) : 1;
    uint64_t key = uint64_t(minF) | (uint64_t(magF) << 4) | (uint64_t(mipF) << 8) | (uint64_t(au) << 12) |
                   (uint64_t(av) << 16) | (uint64_t(aniso) << 20);
    if (m_stageSampler[stage] && m_stageSamplerKey[stage] == key) return m_stageSampler[stage];
    auto it = m_samplers.find(key);
    if (it != m_samplers.end()) {
        m_stageSamplerKey[stage] = key, m_stageSampler[stage] = it->second;
        return it->second;
    }

    GLuint smp = 0;
    glGenSamplers(1, &smp);
    bool minLinear = minF == D3DTEXF_LINEAR || minF == D3DTEXF_ANISOTROPIC;
    bool magLinear = magF == D3DTEXF_LINEAR || magF == D3DTEXF_ANISOTROPIC;
    GLenum minGL;
    if (mipF == D3DTEXF_NONE) minGL = minLinear ? GL_LINEAR : GL_NEAREST;
    else if (mipF == D3DTEXF_POINT) minGL = minLinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
    else minGL = minLinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
    glSamplerParameteri(smp, GL_TEXTURE_MIN_FILTER, minGL);
    glSamplerParameteri(smp, GL_TEXTURE_MAG_FILTER, magLinear ? GL_LINEAR : GL_NEAREST);
    auto wrap = [](DWORD a) -> GLint {
        switch (a) {
        case D3DTADDRESS_MIRROR:
        case D3DTADDRESS_MIRRORONCE: return GL_MIRRORED_REPEAT;
        case D3DTADDRESS_CLAMP:
        case D3DTADDRESS_BORDER: return GL_CLAMP_TO_EDGE;
        default: return GL_REPEAT;
        }
    };
    glSamplerParameteri(smp, GL_TEXTURE_WRAP_S, wrap(au));
    glSamplerParameteri(smp, GL_TEXTURE_WRAP_T, wrap(av));
    if (aniso > 1 && GetGLCaps().anisotropic)
        glSamplerParameterf(smp, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min((float) aniso, GetGLCaps().maxAnisotropy));
    m_samplers[key] = smp;
    m_stageSamplerKey[stage] = key, m_stageSampler[stage] = smp;
    GLObjects(GLO_SAMPLER, 1);
    return smp;
}

void Device::BindTextures(FFPKey &key) {
    int n = 0;
    for (int i = 0; i < kStages; i++) {
        const DWORD *t = m_gl.tss[i];
        if (t[D3DTSS_COLOROP] == D3DTOP_DISABLE) break;
        FFPStage &s = key.stage[i];
        s.colorOp = (uint8_t) t[D3DTSS_COLOROP];
        s.colorArg[0] = (uint8_t) t[D3DTSS_COLORARG0];
        s.colorArg[1] = (uint8_t) t[D3DTSS_COLORARG1];
        s.colorArg[2] = (uint8_t) t[D3DTSS_COLORARG2];
        s.alphaOp = (uint8_t) t[D3DTSS_ALPHAOP];
        s.alphaArg[0] = (uint8_t) t[D3DTSS_ALPHAARG0];
        s.alphaArg[1] = (uint8_t) t[D3DTSS_ALPHAARG1];
        s.alphaArg[2] = (uint8_t) t[D3DTSS_ALPHAARG2];
        s.resultTemp = (t[D3DTSS_RESULTARG] & D3DTA_SELECTMASK) == D3DTA_TEMP;
        DWORD tci = t[D3DTSS_TEXCOORDINDEX];
        s.texIndex = (uint8_t) (tci & 7);
        s.texGen = (uint8_t) (tci >> 16);
        DWORD ttff = t[D3DTSS_TEXTURETRANSFORMFLAGS];
        s.ttffCount = key.rhw ? 0 : (uint8_t) (ttff & 0xFF);
        s.projected = (ttff & D3DTTFF_PROJECTED) ? 1 : 0;

        IDirect3DBaseTexture9 *bt = m_gl.textures[i];
        if (bt) {
            Texture *tex = static_cast<Texture *>(static_cast<IDirect3DTexture9 *>(bt));
            ActiveTextureGL(i);
            tex->GLTexture();
            tex->SetBaseLevel(m_gl.samp[i][D3DSAMP_MAXMIPLEVEL]);
            GLuint smp = GetSampler(i);
            if (m_unitSampler[i] != smp) {
                glBindSampler(i, smp);
                m_unitSampler[i] = smp;
            }
            s.texture = 1;
            s.border = (m_gl.samp[i][D3DSAMP_ADDRESSU] == D3DTADDRESS_BORDER ? 1 : 0) |
                       (m_gl.samp[i][D3DSAMP_ADDRESSV] == D3DTADDRESS_BORDER ? 2 : 0);
        }
        n++;
    }
    for (int i = 0; i < 8; i++) {
        FFPStage &s = key.stage[i];
        s.inputDims = i < n && s.texture && s.texGen == 0 ? key.stage[s.texIndex].pad : 0;
    }
    for (int i = 0; i < 8; i++) key.stage[i].pad = 0;
    for (int i = n; i < 8; i++) memset(&key.stage[i], 0, sizeof(FFPStage));
    key.numStages = (uint8_t) n;
}

// --- Uniforms ------------------------------------------------------------------------------

static void Inverse3x3(const D3DMATRIX &m, float out[9]) {
    // Inverse of the upper 3x3 (row-major), written row-major.
    float a = m._11, b = m._12, c = m._13, d = m._21, e = m._22, f = m._23, g = m._31, h = m._32, i = m._33;
    float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    float det = a * A + b * B + c * C;
    if (std::fabs(det) < 1e-20f) det = 1.0f;
    float inv = 1.0f / det;
    out[0] = A * inv; out[1] = -(b * i - c * h) * inv; out[2] = (b * f - c * e) * inv;
    out[3] = B * inv; out[4] = (a * i - c * g) * inv; out[5] = -(a * f - c * d) * inv;
    out[6] = C * inv; out[7] = -(a * h - b * g) * inv; out[8] = (a * e - b * d) * inv;
}

void Device::UploadUniforms(Program *p, const FFPKey &key) {
    auto stale = [&](Group g) {
        if (p->gen[g] == m_gen[g]) return false;
        p->gen[g] = m_gen[g];
        return true;
    };

    if (stale(G_TRANSFORM)) {
        // D3D matrices are row-major for row vectors (v * M); as GLSL column-major data they
        // are the transposes, so M_glsl * v gives the same result.
        D3DMATRIX wv;
        Mul(m_gl.world[0], m_gl.view, wv);
        if (p->uWorldView >= 0) glUniformMatrix4fv(p->uWorldView, 1, GL_FALSE, &wv.m[0][0]);
        if (p->uNormalMat >= 0) {
            float n[9];
            Inverse3x3(wv, n);
            // Column-vector normal transform is inverse(WV3); n is row-major.
            glUniformMatrix3fv(p->uNormalMat, 1, GL_TRUE, n);
        }
    }
    if (stale(G_VIEW) && p->uView >= 0) glUniformMatrix4fv(p->uView, 1, GL_FALSE, &m_gl.view.m[0][0]);
    if (stale(G_PROJ) && p->uProj >= 0) glUniformMatrix4fv(p->uProj, 1, GL_FALSE, &m_gl.proj.m[0][0]);
    if (key.blendMode && m_bonesGen != m_gen[G_WORLD]) {
        m_bonesGen = m_gen[G_WORLD];  // the bones buffer is shared by all programs
        static float rows[768 * 4];
        UINT count = std::min<UINT>(m_worldHigh, kMaxWorld);
        for (UINT k = 0; k < count; k++) {
            const D3DMATRIX &m = m_gl.world[k];
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 4; i++) rows[(k * 3 + j) * 4 + i] = m.m[i][j];
        }
        glBindBuffer(GL_UNIFORM_BUFFER, m_bonesUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, count * 48, rows);
    }
    if (stale(G_VIEWPORT)) {
        const D3DVIEWPORT9 &vp = m_gl.viewport;
        if (p->uViewport >= 0) glUniform4f(p->uViewport, (float) vp.X, (float) vp.Y, (float) vp.Width, (float) vp.Height);
        if (p->uPixelOffset >= 0)
            glUniform2f(p->uPixelOffset, vp.Width ? 1.0f / vp.Width : 0.0f, vp.Height ? 1.0f / vp.Height : 0.0f);
    }
    if (key.lighting && stale(G_LIGHTS)) {
        float amb[4];
        ColorToVec4(m_gl.rs[D3DRS_AMBIENT], amb);
        if (p->uGlobalAmbient >= 0) glUniform4fv(p->uGlobalAmbient, 1, amb);
        float dif[32] = {}, spe[32] = {}, am[32] = {}, pos[32] = {}, dir[32] = {}, att[32] = {}, spot[32] = {};
        const D3DMATRIX &v = m_gl.view;
        for (int i = 0; i < key.numLights; i++) {
            const D3DLIGHT9 *l = m_gl.FindLight(m_gl.enabledLights[i]);
            if (!l) continue;
            memcpy(&dif[i * 4], &l->Diffuse, 16);
            memcpy(&spe[i * 4], &l->Specular, 16);
            memcpy(&am[i * 4], &l->Ambient, 16);
            const D3DVECTOR &P = l->Position, &D = l->Direction;
            for (int c = 0; c < 3; c++) {
                pos[i * 4 + c] = P.x * v.m[0][c] + P.y * v.m[1][c] + P.z * v.m[2][c] + v.m[3][c];
                dir[i * 4 + c] = D.x * v.m[0][c] + D.y * v.m[1][c] + D.z * v.m[2][c];
            }
            float len = std::sqrt(dir[i * 4] * dir[i * 4] + dir[i * 4 + 1] * dir[i * 4 + 1] + dir[i * 4 + 2] * dir[i * 4 + 2]);
            if (len > 0)
                for (int c = 0; c < 3; c++) dir[i * 4 + c] /= len;
            att[i * 4 + 0] = l->Range;
            att[i * 4 + 1] = l->Attenuation0;
            att[i * 4 + 2] = l->Attenuation1;
            att[i * 4 + 3] = l->Attenuation2;
            spot[i * 4 + 0] = std::cos(l->Theta * 0.5f);
            spot[i * 4 + 1] = std::cos(l->Phi * 0.5f);
            spot[i * 4 + 2] = l->Falloff;
        }
        GLsizei n = key.numLights;
        if (n) {
            if (p->uLightDiffuse >= 0) glUniform4fv(p->uLightDiffuse, n, dif);
            if (p->uLightSpecular >= 0) glUniform4fv(p->uLightSpecular, n, spe);
            if (p->uLightAmbient >= 0) glUniform4fv(p->uLightAmbient, n, am);
            if (p->uLightPos >= 0) glUniform4fv(p->uLightPos, n, pos);
            if (p->uLightDir >= 0) glUniform4fv(p->uLightDir, n, dir);
            if (p->uLightAtt >= 0) glUniform4fv(p->uLightAtt, n, att);
            if (p->uLightSpot >= 0) glUniform4fv(p->uLightSpot, n, spot);
        }
    }
    if (key.lighting && stale(G_MATERIAL)) {
        const D3DMATERIAL9 &m = m_gl.material;
        if (p->uMatDiffuse >= 0) glUniform4fv(p->uMatDiffuse, 1, &m.Diffuse.r);
        if (p->uMatAmbient >= 0) glUniform4fv(p->uMatAmbient, 1, &m.Ambient.r);
        if (p->uMatSpecular >= 0) glUniform4fv(p->uMatSpecular, 1, &m.Specular.r);
        if (p->uMatEmissive >= 0) glUniform4fv(p->uMatEmissive, 1, &m.Emissive.r);
        if (p->uMatPower >= 0) glUniform1f(p->uMatPower, m.Power);
    }
    if (key.fog && stale(G_FOG)) {
        float c[4];
        ColorToVec4(m_gl.rs[D3DRS_FOGCOLOR], c);
        if (p->uFogColor >= 0) glUniform4fv(p->uFogColor, 1, c);
        float start = DWToF(m_gl.rs[D3DRS_FOGSTART]), end = DWToF(m_gl.rs[D3DRS_FOGEND]);
        float range = end - start;
        if (p->uFogParams >= 0)
            glUniform4f(p->uFogParams, start, end, DWToF(m_gl.rs[D3DRS_FOGDENSITY]), range != 0.0f ? 1.0f / range : 0.0f);
    }
    if (stale(G_TFACTOR)) {
        float c[4];
        ColorToVec4(m_gl.rs[D3DRS_TEXTUREFACTOR], c);
        if (p->uTFactor >= 0) glUniform4fv(p->uTFactor, 1, c);
    }
    if (stale(G_ALPHAREF)) {
        if (p->uAlphaRef >= 0) glUniform1f(p->uAlphaRef, (float) (m_gl.rs[D3DRS_ALPHAREF] & 0xFF));
    }
    if (stale(G_STAGE)) {
        float c[32], bias[8], border[32];
        for (int i = 0; i < 8; i++) {
            ColorToVec4(m_gl.tss[i][D3DTSS_CONSTANT], &c[i * 4]);
            ColorToVec4(m_gl.samp[i][D3DSAMP_BORDERCOLOR], &border[i * 4]);
            bias[i] = DWToF(m_gl.samp[i][D3DSAMP_MIPMAPLODBIAS]);
        }
        if (p->uStageConst >= 0) glUniform4fv(p->uStageConst, 8, c);
        if (p->uLodBias >= 0) glUniform1fv(p->uLodBias, 8, bias);
        if (p->uBorderColor >= 0) glUniform4fv(p->uBorderColor, 8, border);
    }
    if (stale(G_TEXMAT)) {
        for (int i = 0; i < 8; i++)
            if (p->uTexMat[i] >= 0) glUniformMatrix4fv(p->uTexMat[i], 1, GL_FALSE, &m_gl.tex[i].m[0][0]);
    }
}

// --- Draw ----------------------------------------------------------------------------------

void Device::DrawGL(const DrawCall &dc) {
    ProfileScope prof(PROF_DRAW);
    if (!BindFramebuffer()) return;

    FFPKey key;
    memset(&key, 0, sizeof(key));
    if (!SetupVertexInput(dc, key)) return;
    BindTextures(key);

    const DWORD *rs = m_gl.rs;
    key.lighting = rs[D3DRS_LIGHTING] && !key.rhw;
    if (key.lighting) {
        key.normalize = rs[D3DRS_NORMALIZENORMALS] ? 1 : 0;
        key.localViewer = rs[D3DRS_LOCALVIEWER] ? 1 : 0;
        key.colorVertex = rs[D3DRS_COLORVERTEX] ? 1 : 0;
        key.diffuseSrc = (uint8_t) rs[D3DRS_DIFFUSEMATERIALSOURCE];
        key.ambientSrc = (uint8_t) rs[D3DRS_AMBIENTMATERIALSOURCE];
        key.specularSrc = (uint8_t) rs[D3DRS_SPECULARMATERIALSOURCE];
        key.emissiveSrc = (uint8_t) rs[D3DRS_EMISSIVEMATERIALSOURCE];
        key.numLights = (uint8_t) std::min<size_t>(m_gl.enabledLights.size(), kMaxLights);
        for (int i = 0; i < key.numLights; i++) {
            const D3DLIGHT9 *l = m_gl.FindLight(m_gl.enabledLights[i]);
            key.lightType[i] = (uint8_t) (l ? l->Type : D3DLIGHT_DIRECTIONAL);
        }
    } else {
        key.normalize = rs[D3DRS_NORMALIZENORMALS] ? 1 : 0;
    }
    key.specular = rs[D3DRS_SPECULARENABLE] ? 1 : 0;
    if (rs[D3DRS_FOGENABLE]) {
        key.fog = 1;
        key.fogTableMode = (uint8_t) rs[D3DRS_FOGTABLEMODE];
        key.fogVertexMode = (uint8_t) rs[D3DRS_FOGVERTEXMODE];
        key.rangeFog = rs[D3DRS_RANGEFOGENABLE] ? 1 : 0;
    }
    key.alphaFunc = rs[D3DRS_ALPHATESTENABLE] ? (uint8_t) rs[D3DRS_ALPHAFUNC] : 0;

    Program *prog;
    if (m_lastKeyProgram && !memcmp(&key, &m_lastKey, sizeof(key))) {
        prog = m_lastKeyProgram;
    } else {
        std::string bytes = key.Bytes();
        auto it = m_programs.find(bytes);
        if (it != m_programs.end()) {
            prog = it->second;
        } else {
            prog = BuildProgram(key);
            m_programs[bytes] = prog;
            if (m_programs.size() % 50 == 0) Log("%u fixed-function programs", (UINT) m_programs.size());
        }
        if (!prog) return;
        m_lastKey = key;
        m_lastKeyProgram = prog;
    }
    if (prog != m_lastProgram) {
        glUseProgram(prog->program);
        m_lastProgram = prog;
    }
    UploadUniforms(prog, key);
    ApplyFixedState();
    const D3DVIEWPORT9 &vp = m_gl.viewport;
    if (!m_glViewportSet || memcmp(&vp, &m_glViewport, sizeof(vp))) {
        glViewport(vp.X, vp.Y, vp.Width, vp.Height);
        glDepthRangef(vp.MinZ, vp.MaxZ);
        m_glViewport = vp;
        m_glViewportSet = true;
    }

    GLenum mode;
    UINT count;
    switch (dc.type) {
    case D3DPT_POINTLIST: mode = GL_POINTS; count = dc.primCount; break;
    case D3DPT_LINELIST: mode = GL_LINES; count = dc.primCount * 2; break;
    case D3DPT_LINESTRIP: mode = GL_LINE_STRIP; count = dc.primCount + 1; break;
    case D3DPT_TRIANGLELIST: mode = GL_TRIANGLES; count = dc.primCount * 3; break;
    case D3DPT_TRIANGLESTRIP: mode = GL_TRIANGLE_STRIP; count = dc.primCount + 2; break;
    case D3DPT_TRIANGLEFAN: mode = GL_TRIANGLE_FAN; count = dc.primCount + 2; break;
    default: return;
    }
    if (dc.indexed) {
        bool is32 = dc.up ? dc.upIndexFormat == D3DFMT_INDEX32 : m_gl.indices->Format() == D3DFMT_INDEX32;
        size_t offset = dc.up ? 0 : size_t(dc.startIndex) * (is32 ? 4 : 2);
        glDrawElements(mode, count, is32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (const void *) offset);
    } else {
        glDrawArrays(mode, dc.up ? 0 : dc.startVertex, count);
    }
}

}  // namespace d3d9gl
