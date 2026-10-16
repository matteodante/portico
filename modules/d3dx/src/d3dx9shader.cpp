// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the shader assembler fails. Portico runs the fixed-function pipeline only, and programs that check
// the caps (no shader model) never call it; the others get E_NOTIMPL and no buffers.
#include <d3dx9.h>

extern "C" HRESULT WINAPI D3DXAssembleShader(LPCSTR, UINT, const D3DXMACRO *, LPD3DXINCLUDE, DWORD,
                                             LPD3DXBUFFER *ppShader, LPD3DXBUFFER *ppErrorMsgs) {
    if (ppShader) *ppShader = nullptr;
    if (ppErrorMsgs) *ppErrorMsgs = nullptr;
    return E_NOTIMPL;
}
