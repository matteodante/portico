// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the core types other D3DX headers refer to. ID3DXBuffer is only declared: the functions that would
// return one (D3DXAssembleShader, D3DXSaveSurfaceToFileInMemory) are not implemented and give back NULL.
#ifndef PORTICO_D3DX9CORE_H
#define PORTICO_D3DX9CORE_H

#include <d3d9.h>

struct ID3DXBuffer;
typedef struct ID3DXBuffer *LPD3DXBUFFER;

#endif  // PORTICO_D3DX9CORE_H
