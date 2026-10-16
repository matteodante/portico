// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// GLSL ES 3.00 generator for the D3D9 fixed-function pipeline (see ffp.h for sources).
#include "ffp.h"

#include <cstdarg>
#include <cstdio>

namespace d3d9gl {

namespace {

struct Out {
    std::string s;
    void operator()(const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        s += buf;
    }
};

const char *MaterialSource(uint8_t src, const char *material, const FFPKey &k) {
    // D3DMCS_MATERIAL 0, COLOR1 1, COLOR2 2; vertex colors only count when present.
    if (k.colorVertex) {
        if (src == D3DMCS_COLOR1 && k.hasDiffuse) return "inDiffuse";
        if (src == D3DMCS_COLOR2 && k.hasSpecular) return "inSpecular";
    }
    return material;
}

void VertexShader(const FFPKey &k, Out &o) {
    o("#version 300 es\nprecision highp float;\nprecision highp int;\n");
    o("layout(location=%d) in vec4 aPosition;\n", A_POSITION);
    if (k.blendMode) o("layout(location=%d) in vec4 aBlendWeight;\n", A_BLENDWEIGHT);
    if (k.blendMode == 2) o("layout(location=%d) in vec4 aBlendIndices;\n", A_BLENDINDICES);
    if (k.hasNormal) o("layout(location=%d) in vec3 aNormal;\n", A_NORMAL);
    o("layout(location=%d) in vec4 aDiffuse;\n", A_DIFFUSE);
    o("layout(location=%d) in vec4 aSpecular;\n", A_SPECULAR);
    bool texUsed[8] = {};
    for (int i = 0; i < k.numStages; i++)
        if (k.stage[i].texGen == 0 && k.stage[i].inputDims) texUsed[k.stage[i].texIndex & 7] = true;
    for (int i = 0; i < 8; i++)
        if (texUsed[i]) o("layout(location=%d) in vec4 aTexCoord%d;\n", A_TEXCOORD0 + i, i);

    o("uniform mat4 uWorldView;\nuniform mat4 uView;\nuniform mat4 uProj;\nuniform mat3 uNormalMat;\n");
    o("uniform vec4 uViewport;\nuniform vec2 uPixelOffset;\n");
    if (k.blendMode) o("layout(std140) uniform Bones { vec4 uBones[768]; };\n");
    if (k.lighting && !k.rhw) {
        o("uniform vec4 uMatDiffuse, uMatAmbient, uMatSpecular, uMatEmissive;\nuniform float uMatPower;\n");
        o("uniform vec4 uGlobalAmbient;\n");
        if (k.numLights) {
            o("uniform vec4 uLightDiffuse[%d], uLightSpecular[%d], uLightAmbient[%d];\n", k.numLights, k.numLights, k.numLights);
            o("uniform vec4 uLightPos[%d], uLightDir[%d], uLightAtt[%d], uLightSpot[%d];\n", k.numLights, k.numLights, k.numLights, k.numLights);
        }
    }
    if (k.fog) o("uniform vec4 uFogParams;\n");  // start, end, density, 1/(end-start)
    bool anyTexMat = false;
    for (int i = 0; i < k.numStages; i++)
        if (k.stage[i].ttffCount) {
            o("uniform mat4 uTexMat%d;\n", i);
            anyTexMat = true;
        }
    (void) anyTexMat;

    o("out vec4 vDiffuse;\nout vec4 vSpecular;\n");
    for (int i = 0; i < k.numStages; i++)
        if (k.stage[i].texture) o("out vec4 vTex%d;\n", i);
    if (k.fog) o("out float vFog;\nout float vEyeZ;\n");

    o("void main() {\n");
    o("  vec4 inDiffuse = %s;\n", k.hasDiffuse ? (k.diffuseBGRA ? "aDiffuse.zyxw" : "aDiffuse") : "vec4(1.0)");
    o("  vec4 inSpecular = %s;\n", k.hasSpecular ? (k.specularBGRA ? "aSpecular.zyxw" : "aSpecular") : "vec4(0.0)");
    o("  vec4 clip;\n  vec3 posV = vec3(0.0);\n  vec3 nV = vec3(0.0);\n  float eyeZ = 0.0;\n");
    if (k.rhw) {
        // Pre-transformed vertex: screen x, y, z and 1/w.
        o("  float w = aPosition.w != 0.0 ? 1.0 / aPosition.w : 1.0;\n");
        o("  clip = vec4(((aPosition.x - uViewport.x) / uViewport.z * 2.0 - 1.0) * w,\n");
        o("              (1.0 - (aPosition.y - uViewport.y) / uViewport.w * 2.0) * w, aPosition.z * w, w);\n");
        o("  eyeZ = w;\n");
    } else {
        o("  vec4 p = vec4(aPosition.xyz, 1.0);\n");
        if (k.blendMode) {
            // Blend in world space: sum w_i * World_i * p, the last weight is 1 - sum.
            o("  vec3 posW = vec3(0.0);\n  vec3 nW = vec3(0.0);\n  float wsum = 0.0;\n");
            o("  vec4 bw = aBlendWeight;\n");
            if (k.blendMode == 2) o("  vec4 bi = %s;\n", k.indicesBGRA ? "aBlendIndices.zyxw * 255.0" : "aBlendIndices");
            for (int i = 0; i <= k.blendWeights; i++) {
                if (i < k.blendWeights) o("  { float wt = bw[%d]; wsum += wt;\n", i);
                else o("  { float wt = 1.0 - wsum;\n");
                if (k.blendMode == 2) o("    int m = int(bi[%d] + 0.5) * 3;\n", i);
                else o("    int m = %d;\n", i * 3);
                o("    vec4 r0 = uBones[m], r1 = uBones[m + 1], r2 = uBones[m + 2];\n");
                o("    posW += wt * vec3(dot(r0, p), dot(r1, p), dot(r2, p));\n");
                if (k.hasNormal) o("    nW += wt * vec3(dot(r0.xyz, aNormal), dot(r1.xyz, aNormal), dot(r2.xyz, aNormal));\n");
                o("  }\n");
            }
            o("  posV = (uView * vec4(posW, 1.0)).xyz;\n");
            if (k.hasNormal) o("  nV = mat3(uView) * nW;\n");
        } else {
            o("  posV = (uWorldView * p).xyz;\n");
            if (k.hasNormal) o("  nV = uNormalMat * aNormal;\n");
        }
        if (k.normalize && k.hasNormal) o("  nV = normalize(nV);\n");
        o("  clip = uProj * vec4(posV, 1.0);\n");
        o("  eyeZ = posV.z;\n");
    }

    // Lighting.
    if (k.lighting && !k.rhw) {
        o("  vec4 mDiff = %s;\n", MaterialSource(k.diffuseSrc, "uMatDiffuse", k));
        o("  vec4 mAmb = %s;\n", MaterialSource(k.ambientSrc, "uMatAmbient", k));
        o("  vec4 mSpec = %s;\n", MaterialSource(k.specularSrc, "uMatSpecular", k));
        o("  vec4 mEmis = %s;\n", MaterialSource(k.emissiveSrc, "uMatEmissive", k));
        o("  vec3 amb = uGlobalAmbient.rgb, dif = vec3(0.0), spe = vec3(0.0);\n");
        o("  vec3 V = %s;\n", k.localViewer ? "-normalize(posV)" : "vec3(0.0, 0.0, -1.0)");
        for (int i = 0; i < k.numLights; i++) {
            o("  {\n    vec3 L; float att = 1.0;\n");
            if (k.lightType[i] == D3DLIGHT_DIRECTIONAL) {
                o("    L = -uLightDir[%d].xyz;\n", i);
            } else {
                o("    vec3 d = uLightPos[%d].xyz - posV; float dist = length(d); L = d / max(dist, 1e-6);\n", i);
                o("    vec4 a = uLightAtt[%d];\n", i);
                o("    att = dist <= a.x ? 1.0 / max(a.y + a.z * dist + a.w * dist * dist, 1e-6) : 0.0;\n");
                if (k.lightType[i] == D3DLIGHT_SPOT) {
                    o("    vec4 sp = uLightSpot[%d]; float rho = dot(-L, uLightDir[%d].xyz);\n", i, i);
                    o("    float sf = rho > sp.x ? 1.0 : (rho <= sp.y ? 0.0 : pow(max((rho - sp.y) / max(sp.x - sp.y, 1e-6), 0.0), sp.z));\n");
                    o("    att *= sf;\n");
                }
            }
            o("    float ndl = max(dot(nV, L), 0.0);\n");
            o("    amb += uLightAmbient[%d].rgb * att;\n", i);
            o("    dif += uLightDiffuse[%d].rgb * ndl * att;\n", i);
            if (k.specular)
                o("    if (ndl > 0.0 && uMatPower > 0.0) spe += uLightSpecular[%d].rgb * pow(max(dot(nV, normalize(L + V)), 0.0), uMatPower) * att;\n", i);
            o("  }\n");
        }
        o("  vDiffuse = vec4(clamp(mEmis.rgb + mAmb.rgb * amb + mDiff.rgb * dif, 0.0, 1.0), mDiff.a);\n");
        o("  vSpecular = vec4(clamp(mSpec.rgb * spe, 0.0, 1.0), inSpecular.a);\n");
    } else {
        o("  vDiffuse = inDiffuse;\n  vSpecular = inSpecular;\n");
    }

    // Fog factor (vertex fog); table fog is computed per pixel from vEyeZ.
    if (k.fog) {
        o("  vEyeZ = eyeZ;\n");
        if (k.fogTableMode == D3DFOG_NONE && k.fogVertexMode != D3DFOG_NONE && !k.rhw) {
            o("  float fd = %s;\n", k.rangeFog ? "length(posV)" : "abs(posV.z)");
            if (k.fogVertexMode == D3DFOG_LINEAR) o("  vFog = clamp((uFogParams.y - fd) * uFogParams.w, 0.0, 1.0);\n");
            else if (k.fogVertexMode == D3DFOG_EXP) o("  vFog = clamp(exp(-uFogParams.z * fd), 0.0, 1.0);\n");
            else o("  vFog = clamp(exp(-(uFogParams.z * fd) * (uFogParams.z * fd)), 0.0, 1.0);\n");
        } else {
            o("  vFog = inSpecular.a;\n");  // D3D9: fog factor from the specular alpha
        }
    }

    // Texture coordinates per stage.
    for (int i = 0; i < k.numStages; i++) {
        const FFPStage &s = k.stage[i];
        if (!s.texture) continue;
        o("  {\n    vec4 tc;\n");
        switch (s.texGen) {
        case D3DTSS_TCI_CAMERASPACENORMAL >> 16: o("    tc = vec4(nV, 1.0);\n"); break;
        case D3DTSS_TCI_CAMERASPACEPOSITION >> 16: o("    tc = vec4(posV, 1.0);\n"); break;
        case D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR >> 16: o("    tc = vec4(reflect(normalize(posV), nV), 1.0);\n"); break;
        case D3DTSS_TCI_SPHEREMAP >> 16:
            o("    vec3 R = reflect(normalize(posV), nV); float m = 2.0 * sqrt(R.x * R.x + R.y * R.y + (R.z + 1.0) * (R.z + 1.0));\n");
            o("    tc = vec4(R.x / m + 0.5, R.y / m + 0.5, 0.0, 1.0);\n");
            break;
        default: {
            int idx = s.texIndex & 7;
            // D3D9 pads a texture coordinate with 1 in the first missing component.
            switch (s.inputDims) {
            case 0: o("    tc = vec4(0.0, 0.0, 0.0, 1.0);\n"); break;
            case 1: o("    tc = vec4(aTexCoord%d.x, 1.0, 0.0, 0.0);\n", idx); break;
            case 2: o("    tc = vec4(aTexCoord%d.xy, 1.0, 0.0);\n", idx); break;
            case 3: o("    tc = vec4(aTexCoord%d.xyz, 1.0);\n", idx); break;
            default: o("    tc = aTexCoord%d;\n", idx); break;
            }
        }
        }
        if (s.ttffCount) o("    tc = uTexMat%d * tc;\n", i);
        o("    vTex%d = tc;\n  }\n", i);
    }

    // D3D clip space -> GL: z from [0,w] to [-w,w], y flipped (render targets keep the D3D row
    // order, row 0 on top), half-pixel offset of the D3D9 rasterization rules.
    o("  gl_Position = vec4(clip.x + clip.w * uPixelOffset.x, -clip.y + clip.w * uPixelOffset.y, 2.0 * clip.z - clip.w, clip.w);\n");
    o("  gl_PointSize = 1.0;\n");
    o("}\n");
}

// Argument of a stage as a GLSL vec4 expression.
std::string Arg(uint8_t a, int stage) {
    std::string base;
    switch (a & D3DTA_SELECTMASK) {
    case D3DTA_DIFFUSE: base = "vDiffuse"; break;
    case D3DTA_CURRENT: base = stage == 0 ? "vDiffuse" : "cur"; break;
    case D3DTA_TEXTURE: base = "tex"; break;
    case D3DTA_TFACTOR: base = "uTFactor"; break;
    case D3DTA_SPECULAR: base = "vSpecular"; break;
    case D3DTA_TEMP: base = "tmp"; break;
    case D3DTA_CONSTANT: {
        char b[32];
        snprintf(b, sizeof(b), "uStageConst[%d]", stage);
        base = b;
        break;
    }
    default: base = "vec4(1.0)"; break;
    }
    if (a & D3DTA_ALPHAREPLICATE) base = "vec4((" + base + ").a)";
    if (a & D3DTA_COMPLEMENT) base = "(vec4(1.0) - " + base + ")";
    return base;
}

// One D3DTOP on swizzle `sw` ("rgb" or "a") of the arguments.
std::string Op(uint8_t op, const std::string &a0, const std::string &a1, const std::string &a2, const char *sw, int stage) {
    std::string A0 = "(" + a0 + ")." + sw, A1 = "(" + a1 + ")." + sw, A2 = "(" + a2 + ")." + sw;
    bool rgb = sw[0] == 'r';
    std::string one = rgb ? "vec3(1.0)" : "1.0", half = rgb ? "vec3(0.5)" : "0.5";
    switch (op) {
    case D3DTOP_SELECTARG1: return A1;
    case D3DTOP_SELECTARG2: return A2;
    case D3DTOP_MODULATE: return A1 + " * " + A2;
    case D3DTOP_MODULATE2X: return "2.0 * " + A1 + " * " + A2;
    case D3DTOP_MODULATE4X: return "4.0 * " + A1 + " * " + A2;
    case D3DTOP_ADD: return A1 + " + " + A2;
    case D3DTOP_ADDSIGNED: return A1 + " + " + A2 + " - " + half;
    case D3DTOP_ADDSIGNED2X: return "2.0 * (" + A1 + " + " + A2 + " - " + half + ")";
    case D3DTOP_SUBTRACT: return A1 + " - " + A2;
    case D3DTOP_ADDSMOOTH: return A1 + " + " + A2 + " - " + A1 + " * " + A2;
    case D3DTOP_BLENDDIFFUSEALPHA: return "mix(" + A2 + ", " + A1 + ", vDiffuse.a)";
    case D3DTOP_BLENDTEXTUREALPHA: return "mix(" + A2 + ", " + A1 + ", tex.a)";
    case D3DTOP_BLENDFACTORALPHA: return "mix(" + A2 + ", " + A1 + ", uTFactor.a)";
    case D3DTOP_BLENDTEXTUREALPHAPM: return A1 + " + " + A2 + " * (1.0 - tex.a)";
    case D3DTOP_BLENDCURRENTALPHA: return std::string("mix(") + A2 + ", " + A1 + ", " + (stage == 0 ? "vDiffuse.a" : "cur.a") + ")";
    case D3DTOP_PREMODULATE: return A1;
    case D3DTOP_MODULATEALPHA_ADDCOLOR: return rgb ? "(" + a1 + ").rgb + (" + a1 + ").a * " + A2 : A1;
    case D3DTOP_MODULATECOLOR_ADDALPHA: return rgb ? A1 + " * " + A2 + " + (" + a1 + ").a" : A1;
    case D3DTOP_MODULATEINVALPHA_ADDCOLOR: return rgb ? "(1.0 - (" + a1 + ").a) * " + A2 + " + " + A1 : A1;
    case D3DTOP_MODULATEINVCOLOR_ADDALPHA: return rgb ? "(" + one + " - " + A1 + ") * " + A2 + " + (" + a1 + ").a" : A1;
    case D3DTOP_DOTPRODUCT3: {
        std::string d = "dot((" + a1 + ").rgb * 2.0 - 1.0, (" + a2 + ").rgb * 2.0 - 1.0)";
        return rgb ? "vec3(" + d + ")" : d;
    }
    case D3DTOP_MULTIPLYADD: return A0 + " + " + A1 + " * " + A2;
    case D3DTOP_LERP: return "mix(" + A2 + ", " + A1 + ", " + A0 + ")";
    case D3DTOP_BUMPENVMAP:
    case D3DTOP_BUMPENVMAPLUMINANCE:
    default: return A1;
    }
}

void FragmentShader(const FFPKey &k, Out &o) {
    o("#version 300 es\nprecision highp float;\nprecision highp int;\n");
    o("in vec4 vDiffuse;\nin vec4 vSpecular;\n");
    for (int i = 0; i < k.numStages; i++)
        if (k.stage[i].texture) o("in vec4 vTex%d;\nuniform sampler2D uTex%d;\n", i, i);
    if (k.fog) o("in float vFog;\nin float vEyeZ;\nuniform vec4 uFogColor;\nuniform vec4 uFogParams;\n");
    o("uniform vec4 uTFactor;\nuniform vec4 uStageConst[8];\nuniform float uLodBias[8];\nuniform float uAlphaRef;\n");
    o("uniform vec4 uBorderColor[8];\n");
    o("out vec4 fragColor;\n");
    o("void main() {\n  vec4 cur = vDiffuse;\n  vec4 tmp = vec4(0.0);\n  vec4 tex = vec4(1.0);\n");
    for (int i = 0; i < k.numStages; i++) {
        const FFPStage &s = k.stage[i];
        o("  // stage %d\n", i);
        if (s.texture) {
            int n = s.ttffCount ? s.ttffCount : 2;
            if (s.projected && s.ttffCount >= 2) {
                const char *comp = n == 2 ? "y" : n == 3 ? "z" : "w";
                o("  { vec2 uv = vTex%d.xy / vTex%d.%s;\n", i, i, comp);
            } else {
                o("  { vec2 uv = vTex%d.xy;\n", i);
            }
            o("    tex = texture(uTex%d, uv, uLodBias[%d]);\n", i, i);
            if (s.border) {
                // WebGL2 has no CLAMP_TO_BORDER: outside [0,1] on a BORDER axis the stage sees
                // D3DSAMP_BORDERCOLOR (the sampler clamps to edge, so the inside is unaffected).
                std::string outside;
                if (s.border & 1) outside += "uv.x < 0.0 || uv.x > 1.0";
                if (s.border & 2) outside += std::string(outside.empty() ? "" : " || ") + "uv.y < 0.0 || uv.y > 1.0";
                o("    if (%s) tex = uBorderColor[%d];\n", outside.c_str(), i);
            }
            o("  }\n");
        } else {
            o("  tex = vec4(1.0);\n");  // no texture bound: the stage sees opaque white
        }
        std::string c = Op(s.colorOp, Arg(s.colorArg[0], i), Arg(s.colorArg[1], i), Arg(s.colorArg[2], i), "rgb", i);
        std::string a;
        if (s.colorOp == D3DTOP_DOTPRODUCT3)
            a = Op(D3DTOP_DOTPRODUCT3, Arg(s.colorArg[0], i), Arg(s.colorArg[1], i), Arg(s.colorArg[2], i), "a", i);
        else if (s.alphaOp == D3DTOP_DISABLE)
            a = std::string(i == 0 ? "vDiffuse" : "cur") + ".a";
        else
            a = Op(s.alphaOp, Arg(s.alphaArg[0], i), Arg(s.alphaArg[1], i), Arg(s.alphaArg[2], i), "a", i);
        o("  %s = clamp(vec4(%s, %s), 0.0, 1.0);\n", s.resultTemp ? "tmp" : "cur", c.c_str(), a.c_str());
    }
    if (k.specular) o("  cur.rgb = clamp(cur.rgb + vSpecular.rgb, 0.0, 1.0);\n");
    if (k.fog) {
        if (k.fogTableMode != D3DFOG_NONE) {
            o("  float fd = abs(vEyeZ);\n");
            if (k.fogTableMode == D3DFOG_LINEAR) o("  float f = clamp((uFogParams.y - fd) * uFogParams.w, 0.0, 1.0);\n");
            else if (k.fogTableMode == D3DFOG_EXP) o("  float f = clamp(exp(-uFogParams.z * fd), 0.0, 1.0);\n");
            else o("  float f = clamp(exp(-(uFogParams.z * fd) * (uFogParams.z * fd)), 0.0, 1.0);\n");
        } else {
            o("  float f = clamp(vFog, 0.0, 1.0);\n");
        }
        o("  cur.rgb = mix(uFogColor.rgb, cur.rgb, f);\n");
    }
    if (k.alphaFunc && k.alphaFunc != D3DCMP_ALWAYS) {
        // D3D9 compares the 8-bit alpha with ALPHAREF.
        o("  float a8 = floor(cur.a * 255.0 + 0.5);\n");
        const char *cmp = "false";
        switch (k.alphaFunc) {
        case D3DCMP_NEVER: cmp = "false"; break;
        case D3DCMP_LESS: cmp = "a8 < uAlphaRef"; break;
        case D3DCMP_EQUAL: cmp = "a8 == uAlphaRef"; break;
        case D3DCMP_LESSEQUAL: cmp = "a8 <= uAlphaRef"; break;
        case D3DCMP_GREATER: cmp = "a8 > uAlphaRef"; break;
        case D3DCMP_NOTEQUAL: cmp = "a8 != uAlphaRef"; break;
        case D3DCMP_GREATEREQUAL: cmp = "a8 >= uAlphaRef"; break;
        }
        o("  if (!(%s)) discard;\n", cmp);
    }
    o("  fragColor = cur;\n}\n");
}

GLuint Compile(GLenum type, const std::string &src) {
    GLuint sh = glCreateShader(type);
    const char *p = src.c_str();
    glShaderSource(sh, 1, &p, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        Log("shader compile failed: %s\n%s", log, src.c_str());
    }
    return sh;
}

}  // namespace

Program *BuildProgram(const FFPKey &key) {
    Out vs, fs;
    VertexShader(key, vs);
    FragmentShader(key, fs);
    GLuint v = Compile(GL_VERTEX_SHADER, vs.s), f = Compile(GL_FRAGMENT_SHADER, fs.s);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        Log("program link failed: %s\n--- vs\n%s\n--- fs\n%s", log, vs.s.c_str(), fs.s.c_str());
        glDeleteProgram(prog);
        return nullptr;
    }

    Program *p = new Program();
    p->program = prog;
    GLObjects(GLO_PROGRAM, 1);
    glUseProgram(prog);
#define LOC(n) p->n = glGetUniformLocation(prog, #n)
    LOC(uWorldView); LOC(uView); LOC(uProj); LOC(uNormalMat); LOC(uViewport); LOC(uPixelOffset);
    LOC(uMatDiffuse); LOC(uMatAmbient); LOC(uMatSpecular); LOC(uMatEmissive); LOC(uMatPower); LOC(uGlobalAmbient);
    LOC(uLightDiffuse); LOC(uLightSpecular); LOC(uLightAmbient); LOC(uLightPos); LOC(uLightDir); LOC(uLightAtt); LOC(uLightSpot);
    LOC(uFogColor); LOC(uFogParams); LOC(uTFactor); LOC(uAlphaRef); LOC(uStageConst); LOC(uLodBias); LOC(uBorderColor);
#undef LOC
    for (int i = 0; i < 8; i++) {
        char name[16];
        snprintf(name, sizeof(name), "uTexMat%d", i);
        p->uTexMat[i] = glGetUniformLocation(prog, name);
        snprintf(name, sizeof(name), "uTex%d", i);
        GLint s = glGetUniformLocation(prog, name);
        if (s >= 0) glUniform1i(s, i);
    }
    p->uBonesBlock = (GLint) glGetUniformBlockIndex(prog, "Bones");
    if (p->uBonesBlock != (GLint) GL_INVALID_INDEX) glUniformBlockBinding(prog, (GLuint) p->uBonesBlock, 0);
    for (uint32_t &g : p->gen) g = ~0u;
    return p;
}

void DestroyProgram(Program *p) {
    if (!p) return;
    glDeleteProgram(p->program);
    GLObjects(GLO_PROGRAM, -1);
    delete p;
}

}  // namespace d3d9gl
