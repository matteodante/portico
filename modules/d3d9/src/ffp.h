// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Fixed-function pipeline of D3D9 rebuilt as GLSL ES 3.00 programs, one per state combination.
//
// Semantics follow the D3D9 documentation of the fixed-function vertex and pixel pipelines;
// the structure of the generated code was checked against Wine's wined3d GLSL fixed-function
// replacement (LGPL, glsl_shader.c: shader_glsl_generate_ffp_vertex_shader / ffp_fragment) and
// LostMyCode/d3d9-webgl (MIT). No code is copied from either.
#pragma once

#include "common.h"

namespace d3d9gl {

// Vertex attribute locations used by every generated program.
enum Attrib {
    A_POSITION = 0, A_BLENDWEIGHT, A_BLENDINDICES, A_NORMAL, A_PSIZE, A_DIFFUSE, A_SPECULAR,
    A_TEXCOORD0, A_COUNT = A_TEXCOORD0 + 8
};

struct FFPStage {
    uint8_t colorOp, colorArg[3], alphaOp, alphaArg[3];  // arg[0] = ARG0, [1] = ARG1, [2] = ARG2
    uint8_t resultTemp;     // D3DTSS_RESULTARG == D3DTA_TEMP
    uint8_t texture;        // a texture is bound
    uint8_t texIndex;       // D3DTSS_TEXCOORDINDEX low bits
    uint8_t texGen;         // D3DTSS_TEXCOORDINDEX >> 16 (D3DTSS_TCI_*)
    uint8_t ttffCount;      // D3DTSS_TEXTURETRANSFORMFLAGS count (0 = disabled)
    uint8_t projected;
    uint8_t inputDims;      // components of the source texture coordinate in the vertex (0 = absent)
    uint8_t border;         // bit 0: ADDRESSU is BORDER, bit 1: ADDRESSV is BORDER (emulated in GLSL)
    uint8_t pad;
};

// Everything that changes the generated code. Compared byte by byte.
struct FFPKey {
    // Vertex input
    uint8_t rhw;            // D3DFVF_XYZRHW / POSITIONT
    uint8_t hasNormal, hasDiffuse, hasSpecular;
    uint8_t diffuseBGRA, specularBGRA, indicesBGRA;
    uint8_t blendWeights;   // weights in the vertex used by VERTEXBLEND (0..3)
    uint8_t blendMode;      // 0 off, 1 non-indexed, 2 indexed
    // Lighting
    uint8_t lighting, normalize, localViewer, specular, colorVertex;
    uint8_t diffuseSrc, ambientSrc, specularSrc, emissiveSrc;
    uint8_t numLights;
    uint8_t lightType[8];   // D3DLIGHTTYPE
    // Fog
    uint8_t fog, fogVertexMode, fogTableMode, rangeFog;
    // Pixel
    uint8_t numStages;
    uint8_t alphaFunc;      // D3DCMP_* (0 or ALWAYS = off)
    uint8_t pad[2];
    FFPStage stage[8];

    std::string Bytes() const { return std::string((const char *) this, sizeof(*this)); }
};

struct Program {
    GLuint program = 0;
    // Uniform locations.
    GLint uWorldView, uView, uProj, uNormalMat, uViewport, uPixelOffset;
    GLint uMatDiffuse, uMatAmbient, uMatSpecular, uMatEmissive, uMatPower, uGlobalAmbient;
    GLint uLightDiffuse, uLightSpecular, uLightAmbient, uLightPos, uLightDir, uLightAtt, uLightSpot;
    GLint uFogColor, uFogParams, uTFactor, uAlphaRef;
    GLint uTexMat[8], uStageConst, uLodBias, uBorderColor;
    GLint uBonesBlock;
    // Last uploaded generation of each uniform group (see Device::Group).
    uint32_t gen[16];
};

Program *BuildProgram(const FFPKey &key);
void DestroyProgram(Program *p);

}  // namespace d3d9gl
