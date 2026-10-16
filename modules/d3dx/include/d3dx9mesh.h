// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the ray tests of D3DX 9's mesh functions (src/d3dx9math.cpp). Meshes themselves are not implemented.
#ifndef PORTICO_D3DX9MESH_H
#define PORTICO_D3DX9MESH_H

#include <d3dx9math.h>

#ifdef __cplusplus
extern "C" {
#endif

// The ray (origin, direction) against the triangle p0 p1 p2, from either side. On a hit, the point is
// p0 + U * (p1 - p0) + V * (p2 - p0) and lies Dist times the direction's length along the ray.
BOOL WINAPI D3DXIntersectTri(const D3DXVECTOR3 *p0, const D3DXVECTOR3 *p1, const D3DXVECTOR3 *p2,
                             const D3DXVECTOR3 *pRayPos, const D3DXVECTOR3 *pRayDir, FLOAT *pU, FLOAT *pV,
                             FLOAT *pDist);

// Whether the ray (origin, direction) meets the sphere, ahead of its origin or with the origin inside.
BOOL WINAPI D3DXSphereBoundProbe(const D3DXVECTOR3 *pCenter, FLOAT Radius, const D3DXVECTOR3 *pRayPosition,
                                 const D3DXVECTOR3 *pRayDirection);

#ifdef __cplusplus
}
#endif

#endif  // PORTICO_D3DX9MESH_H
