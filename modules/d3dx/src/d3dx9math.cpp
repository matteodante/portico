// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the D3DX 9 math functions, written from the public D3DX documentation and standard linear algebra
// (clean room). Direct3D 9 conventions: row vectors (v' = v * M), row-major matrices, left-handed unless the name ends
// in RH.
#include <d3dx9math.h>
#include <d3dx9mesh.h>

namespace {

D3DXVECTOR4 Row(const D3DXVECTOR4 &v, const D3DXMATRIX &m) {
    return D3DXVECTOR4(v.x * m._11 + v.y * m._21 + v.z * m._31 + v.w * m._41,
                       v.x * m._12 + v.y * m._22 + v.z * m._32 + v.w * m._42,
                       v.x * m._13 + v.y * m._23 + v.z * m._33 + v.w * m._43,
                       v.x * m._14 + v.y * m._24 + v.z * m._34 + v.w * m._44);
}

D3DXMATRIX Zero() {
    D3DXMATRIX m;
    memset(&m, 0, sizeof(D3DMATRIX));
    return m;
}

// Hamilton product a * b.
D3DXQUATERNION Hamilton(const D3DXQUATERNION &a, const D3DXQUATERNION &b) {
    return D3DXQUATERNION(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

}  // namespace

extern "C" {

// ---- Vectors ----------------------------------------------------------------------------------------------------------
D3DXVECTOR2 *WINAPI D3DXVec2Normalize(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV) {
    FLOAT l = D3DXVec2Length(pV);
    *pOut = l > 0.0f ? *pV / l : D3DXVECTOR2(0.0f, 0.0f);
    return pOut;
}

D3DXVECTOR4 *WINAPI D3DXVec2Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM) {
    *pOut = Row(D3DXVECTOR4(pV->x, pV->y, 0.0f, 1.0f), *pM);
    return pOut;
}

D3DXVECTOR2 *WINAPI D3DXVec2TransformCoord(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM) {
    D3DXVECTOR4 r = Row(D3DXVECTOR4(pV->x, pV->y, 0.0f, 1.0f), *pM);
    FLOAT iw = r.w != 0.0f ? 1.0f / r.w : 0.0f;
    *pOut = D3DXVECTOR2(r.x * iw, r.y * iw);
    return pOut;
}

D3DXVECTOR2 *WINAPI D3DXVec2TransformNormal(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM) {
    D3DXVECTOR4 r = Row(D3DXVECTOR4(pV->x, pV->y, 0.0f, 0.0f), *pM);
    *pOut = D3DXVECTOR2(r.x, r.y);
    return pOut;
}

D3DXVECTOR3 *WINAPI D3DXVec3Normalize(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV) {
    FLOAT l = D3DXVec3Length(pV);
    *pOut = l > 0.0f ? *pV / l : D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    return pOut;
}

D3DXVECTOR4 *WINAPI D3DXVec3Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM) {
    *pOut = Row(D3DXVECTOR4(*pV, 1.0f), *pM);
    return pOut;
}

D3DXVECTOR3 *WINAPI D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM) {
    D3DXVECTOR4 r = Row(D3DXVECTOR4(*pV, 1.0f), *pM);
    FLOAT iw = r.w != 0.0f ? 1.0f / r.w : 0.0f;
    *pOut = D3DXVECTOR3(r.x * iw, r.y * iw, r.z * iw);
    return pOut;
}

D3DXVECTOR3 *WINAPI D3DXVec3TransformNormal(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM) {
    D3DXVECTOR4 r = Row(D3DXVECTOR4(*pV, 0.0f), *pM);
    *pOut = D3DXVECTOR3(r.x, r.y, r.z);
    return pOut;
}

D3DXVECTOR3 *WINAPI D3DXVec3Project(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                                    const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld) {
    D3DXMATRIX m;
    D3DXMatrixIdentity(&m);
    if (pWorld) m = m * *pWorld;
    if (pView) m = m * *pView;
    if (pProjection) m = m * *pProjection;
    D3DXVECTOR3 p;
    D3DXVec3TransformCoord(&p, pV, &m);
    if (pViewport) {
        p.x = pViewport->X + (1.0f + p.x) * pViewport->Width * 0.5f;
        p.y = pViewport->Y + (1.0f - p.y) * pViewport->Height * 0.5f;
        p.z = pViewport->MinZ + p.z * (pViewport->MaxZ - pViewport->MinZ);
    }
    *pOut = p;
    return pOut;
}

D3DXVECTOR3 *WINAPI D3DXVec3Unproject(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                                      const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld) {
    D3DXMATRIX m;
    D3DXMatrixIdentity(&m);
    if (pWorld) m = m * *pWorld;
    if (pView) m = m * *pView;
    if (pProjection) m = m * *pProjection;
    if (!D3DXMatrixInverse(&m, nullptr, &m)) return nullptr;
    D3DXVECTOR3 p = *pV;
    if (pViewport) {
        p.x = 2.0f * (p.x - pViewport->X) / pViewport->Width - 1.0f;
        p.y = 1.0f - 2.0f * (p.y - pViewport->Y) / pViewport->Height;
        FLOAT range = pViewport->MaxZ - pViewport->MinZ;
        p.z = range != 0.0f ? (p.z - pViewport->MinZ) / range : 0.0f;
    }
    D3DXVec3TransformCoord(pOut, &p, &m);
    return pOut;
}

D3DXVECTOR4 *WINAPI D3DXVec4Normalize(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV) {
    FLOAT l = D3DXVec4Length(pV);
    *pOut = l > 0.0f ? *pV / l : D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);
    return pOut;
}

D3DXVECTOR4 *WINAPI D3DXVec4Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM) {
    *pOut = Row(*pV, *pM);
    return pOut;
}

// ---- Matrices ---------------------------------------------------------------------------------------------------------
D3DXMATRIX *WINAPI D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2) {
    D3DXMATRIX r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = pM1->m[i][0] * pM2->m[0][j] + pM1->m[i][1] * pM2->m[1][j] + pM1->m[i][2] * pM2->m[2][j] +
                        pM1->m[i][3] * pM2->m[3][j];
    *pOut = r;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM) {
    D3DXMATRIX r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) r.m[i][j] = pM->m[j][i];
    *pOut = r;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixMultiplyTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2) {
    D3DXMATRIX r;
    D3DXMatrixMultiply(&r, pM1, pM2);
    return D3DXMatrixTranspose(pOut, &r);
}

// Determinant and inverse through the 2x2 minors of the top and bottom row pairs (Laplace expansion by complementary
// minors): six minors from rows 0-1, six from rows 2-3.
namespace {
struct Minors {
    FLOAT s[6], c[6];
};
Minors ComputeMinors(const D3DXMATRIX &m) {
    Minors r;
    r.s[0] = m._11 * m._22 - m._21 * m._12;
    r.s[1] = m._11 * m._23 - m._21 * m._13;
    r.s[2] = m._11 * m._24 - m._21 * m._14;
    r.s[3] = m._12 * m._23 - m._22 * m._13;
    r.s[4] = m._12 * m._24 - m._22 * m._14;
    r.s[5] = m._13 * m._24 - m._23 * m._14;
    r.c[5] = m._33 * m._44 - m._43 * m._34;
    r.c[4] = m._32 * m._44 - m._42 * m._34;
    r.c[3] = m._32 * m._43 - m._42 * m._33;
    r.c[2] = m._31 * m._44 - m._41 * m._34;
    r.c[1] = m._31 * m._43 - m._41 * m._33;
    r.c[0] = m._31 * m._42 - m._41 * m._32;
    return r;
}
FLOAT Determinant(const Minors &n) {
    return n.s[0] * n.c[5] - n.s[1] * n.c[4] + n.s[2] * n.c[3] + n.s[3] * n.c[2] - n.s[4] * n.c[1] + n.s[5] * n.c[0];
}
}  // namespace

FLOAT WINAPI D3DXMatrixDeterminant(const D3DXMATRIX *pM) { return Determinant(ComputeMinors(*pM)); }

D3DXMATRIX *WINAPI D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM) {
    const D3DXMATRIX &m = *pM;
    Minors n = ComputeMinors(m);
    FLOAT det = Determinant(n);
    if (pDeterminant) *pDeterminant = det;
    if (det == 0.0f || !isfinite(det)) return nullptr;
    FLOAT id = 1.0f / det;
    const FLOAT *s = n.s, *c = n.c;
    D3DXMATRIX r;
    r._11 = (m._22 * c[5] - m._23 * c[4] + m._24 * c[3]) * id;
    r._12 = (-m._12 * c[5] + m._13 * c[4] - m._14 * c[3]) * id;
    r._13 = (m._42 * s[5] - m._43 * s[4] + m._44 * s[3]) * id;
    r._14 = (-m._32 * s[5] + m._33 * s[4] - m._34 * s[3]) * id;
    r._21 = (-m._21 * c[5] + m._23 * c[2] - m._24 * c[1]) * id;
    r._22 = (m._11 * c[5] - m._13 * c[2] + m._14 * c[1]) * id;
    r._23 = (-m._41 * s[5] + m._43 * s[2] - m._44 * s[1]) * id;
    r._24 = (m._31 * s[5] - m._33 * s[2] + m._34 * s[1]) * id;
    r._31 = (m._21 * c[4] - m._22 * c[2] + m._24 * c[0]) * id;
    r._32 = (-m._11 * c[4] + m._12 * c[2] - m._14 * c[0]) * id;
    r._33 = (m._41 * s[4] - m._42 * s[2] + m._44 * s[0]) * id;
    r._34 = (-m._31 * s[4] + m._32 * s[2] - m._34 * s[0]) * id;
    r._41 = (-m._21 * c[3] + m._22 * c[1] - m._23 * c[0]) * id;
    r._42 = (m._11 * c[3] - m._12 * c[1] + m._13 * c[0]) * id;
    r._43 = (-m._41 * s[3] + m._42 * s[1] - m._43 * s[0]) * id;
    r._44 = (m._31 * s[3] - m._32 * s[1] + m._33 * s[0]) * id;
    *pOut = r;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz) {
    *pOut = Zero();
    pOut->_11 = sx;
    pOut->_22 = sy;
    pOut->_33 = sz;
    pOut->_44 = 1.0f;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z) {
    D3DXMatrixIdentity(pOut);
    pOut->_41 = x;
    pOut->_42 = y;
    pOut->_43 = z;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationX(D3DXMATRIX *pOut, FLOAT angle) {
    FLOAT s = sinf(angle), c = cosf(angle);
    D3DXMatrixIdentity(pOut);
    pOut->_22 = c;
    pOut->_23 = s;
    pOut->_32 = -s;
    pOut->_33 = c;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationY(D3DXMATRIX *pOut, FLOAT angle) {
    FLOAT s = sinf(angle), c = cosf(angle);
    D3DXMatrixIdentity(pOut);
    pOut->_11 = c;
    pOut->_13 = -s;
    pOut->_31 = s;
    pOut->_33 = c;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT angle) {
    FLOAT s = sinf(angle), c = cosf(angle);
    D3DXMatrixIdentity(pOut);
    pOut->_11 = c;
    pOut->_12 = s;
    pOut->_21 = -s;
    pOut->_22 = c;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationAxis(D3DXMATRIX *pOut, const D3DXVECTOR3 *pV, FLOAT angle) {
    D3DXVECTOR3 a;
    D3DXVec3Normalize(&a, pV);
    FLOAT s = sinf(angle), c = cosf(angle), t = 1.0f - c;
    D3DXMatrixIdentity(pOut);
    pOut->_11 = t * a.x * a.x + c;
    pOut->_12 = t * a.x * a.y + s * a.z;
    pOut->_13 = t * a.x * a.z - s * a.y;
    pOut->_21 = t * a.x * a.y - s * a.z;
    pOut->_22 = t * a.y * a.y + c;
    pOut->_23 = t * a.y * a.z + s * a.x;
    pOut->_31 = t * a.x * a.z + s * a.y;
    pOut->_32 = t * a.y * a.z - s * a.x;
    pOut->_33 = t * a.z * a.z + c;
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationQuaternion(D3DXMATRIX *pOut, const D3DXQUATERNION *pQ) {
    FLOAT x = pQ->x, y = pQ->y, z = pQ->z, w = pQ->w;
    D3DXMatrixIdentity(pOut);
    pOut->_11 = 1.0f - 2.0f * (y * y + z * z);
    pOut->_12 = 2.0f * (x * y + z * w);
    pOut->_13 = 2.0f * (x * z - y * w);
    pOut->_21 = 2.0f * (x * y - z * w);
    pOut->_22 = 1.0f - 2.0f * (x * x + z * z);
    pOut->_23 = 2.0f * (y * z + x * w);
    pOut->_31 = 2.0f * (x * z + y * w);
    pOut->_32 = 2.0f * (y * z - x * w);
    pOut->_33 = 1.0f - 2.0f * (x * x + y * y);
    return pOut;
}

// Roll about Z first, then pitch about X, then yaw about Y.
D3DXMATRIX *WINAPI D3DXMatrixRotationYawPitchRoll(D3DXMATRIX *pOut, FLOAT yaw, FLOAT pitch, FLOAT roll) {
    D3DXMATRIX r, p, y;
    D3DXMatrixRotationZ(&r, roll);
    D3DXMatrixRotationX(&p, pitch);
    D3DXMatrixRotationY(&y, yaw);
    *pOut = r * p * y;
    return pOut;
}

namespace {
D3DXMATRIX *View(D3DXMATRIX *pOut, const D3DXVECTOR3 &eye, const D3DXVECTOR3 &zaxis, const D3DXVECTOR3 *pUp) {
    D3DXVECTOR3 xaxis, yaxis;
    D3DXVec3Cross(&xaxis, pUp, &zaxis);
    D3DXVec3Normalize(&xaxis, &xaxis);
    D3DXVec3Cross(&yaxis, &zaxis, &xaxis);
    *pOut = D3DXMATRIX(xaxis.x, yaxis.x, zaxis.x, 0.0f,
                       xaxis.y, yaxis.y, zaxis.y, 0.0f,
                       xaxis.z, yaxis.z, zaxis.z, 0.0f,
                       -D3DXVec3Dot(&xaxis, &eye), -D3DXVec3Dot(&yaxis, &eye), -D3DXVec3Dot(&zaxis, &eye), 1.0f);
    return pOut;
}
}  // namespace

D3DXMATRIX *WINAPI D3DXMatrixLookAtLH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt, const D3DXVECTOR3 *pUp) {
    D3DXVECTOR3 z = *pAt - *pEye;
    D3DXVec3Normalize(&z, &z);
    return View(pOut, *pEye, z, pUp);
}

D3DXMATRIX *WINAPI D3DXMatrixLookAtRH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt, const D3DXVECTOR3 *pUp) {
    D3DXVECTOR3 z = *pEye - *pAt;
    D3DXVec3Normalize(&z, &z);
    return View(pOut, *pEye, z, pUp);
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveLH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) {
    *pOut = Zero();
    pOut->_11 = 2.0f * zn / w;
    pOut->_22 = 2.0f * zn / h;
    pOut->_33 = zf / (zf - zn);
    pOut->_34 = 1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveRH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) {
    *pOut = Zero();
    pOut->_11 = 2.0f * zn / w;
    pOut->_22 = 2.0f * zn / h;
    pOut->_33 = zf / (zn - zf);
    pOut->_34 = -1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT aspect, FLOAT zn, FLOAT zf) {
    FLOAT ys = 1.0f / tanf(fovy * 0.5f), xs = ys / aspect;
    *pOut = Zero();
    pOut->_11 = xs;
    pOut->_22 = ys;
    pOut->_33 = zf / (zf - zn);
    pOut->_34 = 1.0f;
    pOut->_43 = -zn * zf / (zf - zn);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveFovRH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT aspect, FLOAT zn, FLOAT zf) {
    FLOAT ys = 1.0f / tanf(fovy * 0.5f), xs = ys / aspect;
    *pOut = Zero();
    pOut->_11 = xs;
    pOut->_22 = ys;
    pOut->_33 = zf / (zn - zf);
    pOut->_34 = -1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf) {
    *pOut = Zero();
    pOut->_11 = 2.0f * zn / (r - l);
    pOut->_22 = 2.0f * zn / (t - b);
    pOut->_31 = (l + r) / (l - r);
    pOut->_32 = (t + b) / (b - t);
    pOut->_33 = zf / (zf - zn);
    pOut->_34 = 1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixPerspectiveOffCenterRH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf) {
    *pOut = Zero();
    pOut->_11 = 2.0f * zn / (r - l);
    pOut->_22 = 2.0f * zn / (t - b);
    pOut->_31 = (l + r) / (r - l);
    pOut->_32 = (t + b) / (t - b);
    pOut->_33 = zf / (zn - zf);
    pOut->_34 = -1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixOrthoLH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) {
    D3DXMatrixIdentity(pOut);
    pOut->_11 = 2.0f / w;
    pOut->_22 = 2.0f / h;
    pOut->_33 = 1.0f / (zf - zn);
    pOut->_43 = zn / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixOrthoRH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) {
    D3DXMatrixIdentity(pOut);
    pOut->_11 = 2.0f / w;
    pOut->_22 = 2.0f / h;
    pOut->_33 = 1.0f / (zn - zf);
    pOut->_43 = zn / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixOrthoOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf) {
    D3DXMatrixIdentity(pOut);
    pOut->_11 = 2.0f / (r - l);
    pOut->_22 = 2.0f / (t - b);
    pOut->_33 = 1.0f / (zf - zn);
    pOut->_41 = (l + r) / (l - r);
    pOut->_42 = (t + b) / (b - t);
    pOut->_43 = zn / (zn - zf);
    return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixOrthoOffCenterRH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf) {
    D3DXMatrixIdentity(pOut);
    pOut->_11 = 2.0f / (r - l);
    pOut->_22 = 2.0f / (t - b);
    pOut->_33 = 1.0f / (zn - zf);
    pOut->_41 = (l + r) / (l - r);
    pOut->_42 = (t + b) / (b - t);
    pOut->_43 = zn / (zn - zf);
    return pOut;
}

// Reflection about the (normalized) plane: v' = v - 2 (n . v + d) n.
// The documented order: Msc^-1 * Msr^-1 * Ms * Msr * Msc * Mrc^-1 * Mr * Mrc * Mt, where Msc and Mrc translate to the
// scaling and rotation centers, Msr and Mr rotate, Ms scales and Mt translates. A null argument is the identity.
D3DXMATRIX *WINAPI D3DXMatrixTransformation(D3DXMATRIX *pOut, const D3DXVECTOR3 *pScalingCenter,
                                            const D3DXQUATERNION *pScalingRotation, const D3DXVECTOR3 *pScaling,
                                            const D3DXVECTOR3 *pRotationCenter, const D3DXQUATERNION *pRotation,
                                            const D3DXVECTOR3 *pTranslation) {
    D3DXMATRIX m, t;
    D3DXMatrixIdentity(&m);
    if (pScaling) {
        D3DXVECTOR3 c = pScalingCenter ? *pScalingCenter : D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        D3DXMATRIX r, rInv, s;
        D3DXMatrixIdentity(&r);
        if (pScalingRotation) D3DXMatrixRotationQuaternion(&r, pScalingRotation);
        D3DXMatrixInverse(&rInv, nullptr, &r);
        D3DXMatrixScaling(&s, pScaling->x, pScaling->y, pScaling->z);
        D3DXMatrixTranslation(&t, -c.x, -c.y, -c.z);
        m = t * rInv * s * r;
        D3DXMatrixTranslation(&t, c.x, c.y, c.z);
        m *= t;
    }
    if (pRotation) {
        D3DXVECTOR3 c = pRotationCenter ? *pRotationCenter : D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        D3DXMATRIX r;
        D3DXMatrixRotationQuaternion(&r, pRotation);
        D3DXMatrixTranslation(&t, -c.x, -c.y, -c.z);
        m = m * t * r;
        D3DXMatrixTranslation(&t, c.x, c.y, c.z);
        m *= t;
    }
    if (pTranslation) {
        D3DXMatrixTranslation(&t, pTranslation->x, pTranslation->y, pTranslation->z);
        m *= t;
    }
    *pOut = m;
    return pOut;
}

// The same in the XY plane: rotations about Z, no scaling in Z.
D3DXMATRIX *WINAPI D3DXMatrixTransformation2D(D3DXMATRIX *pOut, const D3DXVECTOR2 *pScalingCenter, FLOAT ScalingRotation,
                                              const D3DXVECTOR2 *pScaling, const D3DXVECTOR2 *pRotationCenter,
                                              FLOAT Rotation, const D3DXVECTOR2 *pTranslation) {
    const D3DXVECTOR3 z(0.0f, 0.0f, 1.0f);
    D3DXVECTOR3 sc, s, rc, t;
    D3DXQUATERNION sr, r;
    if (pScalingCenter) sc = D3DXVECTOR3(pScalingCenter->x, pScalingCenter->y, 0.0f);
    if (pScaling) s = D3DXVECTOR3(pScaling->x, pScaling->y, 1.0f);
    if (pRotationCenter) rc = D3DXVECTOR3(pRotationCenter->x, pRotationCenter->y, 0.0f);
    if (pTranslation) t = D3DXVECTOR3(pTranslation->x, pTranslation->y, 0.0f);
    D3DXQuaternionRotationAxis(&sr, &z, ScalingRotation);
    D3DXQuaternionRotationAxis(&r, &z, Rotation);
    return D3DXMatrixTransformation(pOut, pScalingCenter ? &sc : nullptr, &sr, pScaling ? &s : nullptr,
                                    pRotationCenter ? &rc : nullptr, &r, pTranslation ? &t : nullptr);
}

// Scale, rotation and translation of an affine matrix, with D3DX's observable behaviour (checked against the system's
// d3dx9_43.dll with the same inputs):
//   - the scales are the lengths of the first three rows; the rotation is what is left of them;
//   - a zero row is rebuilt from the other two (x from y x z, y from x x z, z from x x y), two zero rows around the
//     remaining one, and the call still succeeds;
//   - a reflection (negative determinant) goes to the largest scale, the first of equal ones;
//   - rows that are not perpendicular (a shear) are not a rotation: D3DERR_INVALIDCALL, rotation (0, 0, 0, 0).
HRESULT WINAPI D3DXMatrixDecompose(D3DXVECTOR3 *pOutScale, D3DXQUATERNION *pOutRotation, D3DXVECTOR3 *pOutTranslation,
                                   const D3DXMATRIX *pM) {
    if (!pOutScale || !pOutRotation || !pOutTranslation || !pM) return D3DERR_INVALIDCALL;
    const D3DXMATRIX &m = *pM;
    D3DXVECTOR3 a[3] = {D3DXVECTOR3(m._11, m._12, m._13), D3DXVECTOR3(m._21, m._22, m._23), D3DXVECTOR3(m._31, m._32, m._33)};
    FLOAT s[3];
    int zeros = 0, kept = 0;
    for (int i = 0; i < 3; i++) {
        s[i] = D3DXVec3Length(&a[i]);
        if (s[i] == 0.0f) zeros++;
        else { a[i] /= s[i]; kept = i; }
    }
    *pOutTranslation = D3DXVECTOR3(m._41, m._42, m._43);

    if (zeros == 3) {
        a[0] = D3DXVECTOR3(1.0f, 0.0f, 0.0f); a[1] = D3DXVECTOR3(0.0f, 1.0f, 0.0f); a[2] = D3DXVECTOR3(0.0f, 0.0f, 1.0f);
    } else if (zeros == 2) {
        // Two perpendicular axes around the one left, in the order that keeps the basis right-handed.
        const D3DXVECTOR3 &k = a[kept];
        D3DXVECTOR3 h = fabsf(k.x) <= fabsf(k.y) && fabsf(k.x) <= fabsf(k.z) ? D3DXVECTOR3(1.0f, 0.0f, 0.0f)
                      : fabsf(k.y) <= fabsf(k.z) ? D3DXVECTOR3(0.0f, 1.0f, 0.0f) : D3DXVECTOR3(0.0f, 0.0f, 1.0f);
        D3DXVECTOR3 b, c;
        D3DXVec3Cross(&b, &k, &h);
        D3DXVec3Normalize(&b, &b);
        D3DXVec3Cross(&c, &k, &b);
        a[(kept + 1) % 3] = b;
        a[(kept + 2) % 3] = c;
    } else if (zeros == 1) {
        int z = s[0] == 0.0f ? 0 : (s[1] == 0.0f ? 1 : 2);
        const D3DXVECTOR3 &p = a[z == 0 ? 1 : 0], &q = a[z == 2 ? 1 : 2];
        D3DXVec3Cross(&a[z], &p, &q);
        D3DXVec3Normalize(&a[z], &a[z]);
    }

    D3DXVECTOR3 cross;
    D3DXVec3Cross(&cross, &a[1], &a[2]);
    FLOAT det = D3DXVec3Dot(&a[0], &cross);
    if (fabsf(fabsf(det) - 1.0f) > 1e-2f) {
        *pOutScale = D3DXVECTOR3(s[0], s[1], s[2]);
        *pOutRotation = D3DXQUATERNION(0.0f, 0.0f, 0.0f, 0.0f);
        return D3DERR_INVALIDCALL;
    }
    if (det < 0.0f) {
        int big = 0;
        for (int i = 1; i < 3; i++)
            if (s[i] > s[big]) big = i;
        s[big] = -s[big];
        a[big] = -a[big];
    }
    D3DXMATRIX r;
    D3DXMatrixIdentity(&r);
    r._11 = a[0].x; r._12 = a[0].y; r._13 = a[0].z;
    r._21 = a[1].x; r._22 = a[1].y; r._23 = a[1].z;
    r._31 = a[2].x; r._32 = a[2].y; r._33 = a[2].z;
    *pOutScale = D3DXVECTOR3(s[0], s[1], s[2]);
    D3DXQuaternionRotationMatrix(pOutRotation, &r);
    return D3D_OK;
}

D3DXMATRIX *WINAPI D3DXMatrixReflect(D3DXMATRIX *pOut, const D3DXPLANE *pPlane) {
    D3DXPLANE p;
    D3DXPlaneNormalize(&p, pPlane);
    *pOut = D3DXMATRIX(1.0f - 2.0f * p.a * p.a, -2.0f * p.a * p.b, -2.0f * p.a * p.c, 0.0f,
                       -2.0f * p.b * p.a, 1.0f - 2.0f * p.b * p.b, -2.0f * p.b * p.c, 0.0f,
                       -2.0f * p.c * p.a, -2.0f * p.c * p.b, 1.0f - 2.0f * p.c * p.c, 0.0f,
                       -2.0f * p.d * p.a, -2.0f * p.d * p.b, -2.0f * p.d * p.c, 1.0f);
    return pOut;
}

// Flattens geometry onto the plane as seen from the light (w = 0: a directional light, w = 1: a point light):
// M = (P . L) I - P^T L for row vectors, so points of the plane stay where they are.
D3DXMATRIX *WINAPI D3DXMatrixShadow(D3DXMATRIX *pOut, const D3DXVECTOR4 *pLight, const D3DXPLANE *pPlane) {
    D3DXPLANE p;
    D3DXPlaneNormalize(&p, pPlane);
    const D3DXVECTOR4 &l = *pLight;
    FLOAT d = p.a * l.x + p.b * l.y + p.c * l.z + p.d * l.w;
    const FLOAT pv[4] = {p.a, p.b, p.c, p.d}, lv[4] = {l.x, l.y, l.z, l.w};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) pOut->m[i][j] = (i == j ? d : 0.0f) - pv[i] * lv[j];
    return pOut;
}

// ---- Quaternions ------------------------------------------------------------------------------------------------------
D3DXQUATERNION *WINAPI D3DXQuaternionNormalize(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ) {
    FLOAT l = D3DXQuaternionLength(pQ);
    *pOut = l > 0.0f ? *pQ / l : D3DXQUATERNION(0.0f, 0.0f, 0.0f, 0.0f);
    return pOut;
}

D3DXQUATERNION *WINAPI D3DXQuaternionInverse(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ) {
    FLOAT n = D3DXQuaternionLengthSq(pQ);
    if (n == 0.0f) return nullptr;
    D3DXQUATERNION c;
    D3DXQuaternionConjugate(&c, pQ);
    *pOut = c / n;
    return pOut;
}

// The rotation pQ1 followed by the rotation pQ2: the Hamilton product pQ2 * pQ1.
D3DXQUATERNION *WINAPI D3DXQuaternionMultiply(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2) {
    *pOut = Hamilton(*pQ2, *pQ1);
    return pOut;
}

D3DXQUATERNION *WINAPI D3DXQuaternionRotationAxis(D3DXQUATERNION *pOut, const D3DXVECTOR3 *pV, FLOAT angle) {
    D3DXVECTOR3 a;
    D3DXVec3Normalize(&a, pV);
    FLOAT s = sinf(angle * 0.5f);
    *pOut = D3DXQUATERNION(a.x * s, a.y * s, a.z * s, cosf(angle * 0.5f));
    return pOut;
}

// The quaternion of a rotation matrix (row-vector convention, so the off-diagonal pairs are the transpose of the
// column-vector formulas), through its largest diagonal term for precision.
D3DXQUATERNION *WINAPI D3DXQuaternionRotationMatrix(D3DXQUATERNION *pOut, const D3DXMATRIX *pM) {
    const D3DXMATRIX &m = *pM;
    FLOAT trace = m._11 + m._22 + m._33;
    D3DXQUATERNION q;
    if (trace > 0.0f) {
        FLOAT s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m._23 - m._32) / s;
        q.y = (m._31 - m._13) / s;
        q.z = (m._12 - m._21) / s;
    } else if (m._11 > m._22 && m._11 > m._33) {
        FLOAT s = sqrtf(1.0f + m._11 - m._22 - m._33) * 2.0f;
        q.w = (m._23 - m._32) / s;
        q.x = 0.25f * s;
        q.y = (m._12 + m._21) / s;
        q.z = (m._13 + m._31) / s;
    } else if (m._22 > m._33) {
        FLOAT s = sqrtf(1.0f + m._22 - m._11 - m._33) * 2.0f;
        q.w = (m._31 - m._13) / s;
        q.x = (m._12 + m._21) / s;
        q.y = 0.25f * s;
        q.z = (m._23 + m._32) / s;
    } else {
        FLOAT s = sqrtf(1.0f + m._33 - m._11 - m._22) * 2.0f;
        q.w = (m._12 - m._21) / s;
        q.x = (m._13 + m._31) / s;
        q.y = (m._23 + m._32) / s;
        q.z = 0.25f * s;
    }
    *pOut = q;
    return pOut;
}

// Roll about Z first, then pitch about X, then yaw about Y, like D3DXMatrixRotationYawPitchRoll.
D3DXQUATERNION *WINAPI D3DXQuaternionRotationYawPitchRoll(D3DXQUATERNION *pOut, FLOAT yaw, FLOAT pitch, FLOAT roll) {
    D3DXQUATERNION r, p, y;
    const D3DXVECTOR3 ax(1.0f, 0.0f, 0.0f), ay(0.0f, 1.0f, 0.0f), az(0.0f, 0.0f, 1.0f);
    D3DXQuaternionRotationAxis(&r, &az, roll);
    D3DXQuaternionRotationAxis(&p, &ax, pitch);
    D3DXQuaternionRotationAxis(&y, &ay, yaw);
    D3DXQUATERNION rp;
    D3DXQuaternionMultiply(&rp, &r, &p);
    return D3DXQuaternionMultiply(pOut, &rp, &y);
}

// Spherical interpolation along the shorter arc.
D3DXQUATERNION *WINAPI D3DXQuaternionSlerp(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2, FLOAT t) {
    D3DXQUATERNION b = *pQ2;
    FLOAT cosom = D3DXQuaternionDot(pQ1, &b);
    if (cosom < 0.0f) {
        cosom = -cosom;
        b = -b;
    }
    FLOAT k0, k1;
    if (1.0f - cosom > 1e-5f) {
        FLOAT om = acosf(cosom), sinom = sinf(om);
        k0 = sinf((1.0f - t) * om) / sinom;
        k1 = sinf(t * om) / sinom;
    } else {
        k0 = 1.0f - t;
        k1 = t;
    }
    *pOut = *pQ1 * k0 + b * k1;
    return pOut;
}

void WINAPI D3DXQuaternionToAxisAngle(const D3DXQUATERNION *pQ, D3DXVECTOR3 *pAxis, FLOAT *pAngle) {
    if (pAxis) *pAxis = D3DXVECTOR3(pQ->x, pQ->y, pQ->z);
    if (pAngle) *pAngle = 2.0f * acosf(pQ->w < -1.0f ? -1.0f : pQ->w > 1.0f ? 1.0f : pQ->w);
}

// The natural logarithm of a unit quaternion (v sin t, cos t): (v t, 0).
D3DXQUATERNION *WINAPI D3DXQuaternionLn(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ) {
    FLOAT w = pQ->w > 1.0f ? 1.0f : (pQ->w < -1.0f ? -1.0f : pQ->w);
    FLOAT t = acosf(w), s = sinf(t);
    FLOAT k = fabsf(s) > 1e-6f ? t / s : 1.0f;
    *pOut = D3DXQUATERNION(pQ->x * k, pQ->y * k, pQ->z * k, 0.0f);
    return pOut;
}

// ---- Planes -----------------------------------------------------------------------------------------------------------
D3DXPLANE *WINAPI D3DXPlaneNormalize(D3DXPLANE *pOut, const D3DXPLANE *pP) {
    FLOAT l = sqrtf(pP->a * pP->a + pP->b * pP->b + pP->c * pP->c);
    *pOut = l > 0.0f ? D3DXPLANE(pP->a / l, pP->b / l, pP->c / l, pP->d / l) : D3DXPLANE(0.0f, 0.0f, 0.0f, 0.0f);
    return pOut;
}

D3DXPLANE *WINAPI D3DXPlaneFromPointNormal(D3DXPLANE *pOut, const D3DXVECTOR3 *pPoint, const D3DXVECTOR3 *pNormal) {
    *pOut = D3DXPLANE(pNormal->x, pNormal->y, pNormal->z, -D3DXVec3Dot(pPoint, pNormal));
    return pOut;
}

D3DXPLANE *WINAPI D3DXPlaneFromPoints(D3DXPLANE *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2, const D3DXVECTOR3 *pV3) {
    D3DXVECTOR3 e1 = *pV2 - *pV1, e2 = *pV3 - *pV1, n;
    D3DXVec3Cross(&n, &e1, &e2);
    D3DXVec3Normalize(&n, &n);
    return D3DXPlaneFromPointNormal(pOut, pV1, &n);
}

D3DXVECTOR3 *WINAPI D3DXPlaneIntersectLine(D3DXVECTOR3 *pOut, const D3DXPLANE *pP, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2) {
    D3DXVECTOR3 dir = *pV2 - *pV1;
    FLOAT denom = D3DXPlaneDotNormal(pP, &dir);
    if (denom == 0.0f) return nullptr;
    FLOAT t = -D3DXPlaneDotCoord(pP, pV1) / denom;
    *pOut = *pV1 + dir * t;
    return pOut;
}

// pM is the inverse transpose of the transformation applied to the plane's points.
D3DXPLANE *WINAPI D3DXPlaneTransform(D3DXPLANE *pOut, const D3DXPLANE *pP, const D3DXMATRIX *pM) {
    D3DXVECTOR4 r = Row(D3DXVECTOR4(pP->a, pP->b, pP->c, pP->d), *pM);
    *pOut = D3DXPLANE(r.x, r.y, r.z, r.w);
    return pOut;
}

// ---- Colors -----------------------------------------------------------------------------------------------------------
// Interpolates between the color's grey (Rec. 709 luminance) and the color; alpha is kept.
D3DXCOLOR *WINAPI D3DXColorAdjustSaturation(D3DXCOLOR *pOut, const D3DXCOLOR *pC, FLOAT s) {
    FLOAT grey = pC->r * 0.2125f + pC->g * 0.7154f + pC->b * 0.0721f;
    *pOut = D3DXCOLOR(grey + s * (pC->r - grey), grey + s * (pC->g - grey), grey + s * (pC->b - grey), pC->a);
    return pOut;
}

// Interpolates between 50% grey and the color; alpha is kept.
D3DXCOLOR *WINAPI D3DXColorAdjustContrast(D3DXCOLOR *pOut, const D3DXCOLOR *pC, FLOAT c) {
    *pOut = D3DXCOLOR(0.5f + c * (pC->r - 0.5f), 0.5f + c * (pC->g - 0.5f), 0.5f + c * (pC->b - 0.5f), pC->a);
    return pOut;
}

// ---- Ray tests (d3dx9mesh.h) ------------------------------------------------------------------------------------------
// Moller-Trumbore, without culling: the triangle is hit from either side.
BOOL WINAPI D3DXIntersectTri(const D3DXVECTOR3 *p0, const D3DXVECTOR3 *p1, const D3DXVECTOR3 *p2,
                             const D3DXVECTOR3 *pRayPos, const D3DXVECTOR3 *pRayDir, FLOAT *pU, FLOAT *pV,
                             FLOAT *pDist) {
    D3DXVECTOR3 e1 = *p1 - *p0, e2 = *p2 - *p0, p, q;
    D3DXVec3Cross(&p, pRayDir, &e2);
    FLOAT det = D3DXVec3Dot(&e1, &p);
    if (det == 0.0f) return FALSE;  // the ray runs along the triangle's plane (no threshold: Dist scales with the direction)
    FLOAT inv = 1.0f / det;
    D3DXVECTOR3 t = *pRayPos - *p0;
    FLOAT u = D3DXVec3Dot(&t, &p) * inv;
    if (u < 0.0f || u > 1.0f) return FALSE;
    D3DXVec3Cross(&q, &t, &e1);
    FLOAT v = D3DXVec3Dot(pRayDir, &q) * inv;
    if (v < 0.0f || u + v > 1.0f) return FALSE;
    FLOAT d = D3DXVec3Dot(&e2, &q) * inv;
    if (d < 0.0f) return FALSE;  // behind the ray's origin
    if (pU) *pU = u;
    if (pV) *pV = v;
    if (pDist) *pDist = d;
    return TRUE;
}

// |o + t d - c| = r for some t >= 0. With the origin outside (c > 0 below) both roots have the same sign, so the
// sphere is ahead when their sum, -b / a, is positive.
BOOL WINAPI D3DXSphereBoundProbe(const D3DXVECTOR3 *pCenter, FLOAT Radius, const D3DXVECTOR3 *pRayPosition,
                                 const D3DXVECTOR3 *pRayDirection) {
    D3DXVECTOR3 oc = *pRayPosition - *pCenter;
    FLOAT a = D3DXVec3Dot(pRayDirection, pRayDirection);
    FLOAT b = 2.0f * D3DXVec3Dot(&oc, pRayDirection);
    FLOAT c = D3DXVec3Dot(&oc, &oc) - Radius * Radius;
    if (c <= 0.0f) return TRUE;  // the origin is inside
    if (a <= 0.0f || b * b - 4.0f * a * c < 0.0f) return FALSE;
    return b < 0.0f;
}

}  // extern "C"
