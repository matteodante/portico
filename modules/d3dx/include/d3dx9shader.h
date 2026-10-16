// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: D3DX 9's shader assembler, declared so that programs with a fixed-function fallback link. Portico runs
// the fixed-function pipeline only (its caps report no shader model): D3DXAssembleShader fails (src/d3dx9shader.cpp).
#ifndef PORTICO_D3DX9SHADER_H
#define PORTICO_D3DX9SHADER_H

#include <d3dx9core.h>

typedef struct _D3DXMACRO {
    LPCSTR Name;
    LPCSTR Definition;
} D3DXMACRO, *LPD3DXMACRO;

struct ID3DXInclude;
typedef struct ID3DXInclude *LPD3DXINCLUDE;

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXAssembleShader(LPCSTR pSrcData, UINT SrcDataLen, const D3DXMACRO *pDefines, LPD3DXINCLUDE pInclude,
                                  DWORD Flags, LPD3DXBUFFER *ppShader, LPD3DXBUFFER *ppErrorMsgs);

#ifdef __cplusplus
}
#endif

#endif  // PORTICO_D3DX9SHADER_H
