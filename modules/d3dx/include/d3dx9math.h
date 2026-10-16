// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the D3DX 9 math types and functions, written from the public D3DX documentation (clean room).
// Row vectors and row-major matrices, as in Direct3D 9: a point p is transformed as p * M.
#ifndef PORTICO_D3DX9MATH_H
#define PORTICO_D3DX9MATH_H

#include <d3d9.h>
#include <math.h>
#include <string.h>

#ifndef __cplusplus
#error "Portico D3DX needs C++"
#endif

#define D3DX_PI ((FLOAT)3.141592654f)
#define D3DX_1BYPI ((FLOAT)0.318309886f)
#define D3DXToRadian(degree) ((degree) * (D3DX_PI / 180.0f))
#define D3DXToDegree(radian) ((radian) * (180.0f / D3DX_PI))

#if defined(_MSC_VER)
#define D3DX_ALIGN16 __declspec(align(16))
#else
#define D3DX_ALIGN16 __attribute__((aligned(16)))
#endif

// ---- Vectors ----------------------------------------------------------------------------------------------------------
struct D3DXVECTOR2 {
    FLOAT x, y;
    D3DXVECTOR2() {}
    D3DXVECTOR2(const FLOAT *pf) : x(pf[0]), y(pf[1]) {}
    D3DXVECTOR2(FLOAT fx, FLOAT fy) : x(fx), y(fy) {}
    operator FLOAT *() { return &x; }
    operator const FLOAT *() const { return &x; }
    D3DXVECTOR2 &operator+=(const D3DXVECTOR2 &v) { x += v.x; y += v.y; return *this; }
    D3DXVECTOR2 &operator-=(const D3DXVECTOR2 &v) { x -= v.x; y -= v.y; return *this; }
    D3DXVECTOR2 &operator*=(FLOAT f) { x *= f; y *= f; return *this; }
    D3DXVECTOR2 &operator/=(FLOAT f) { FLOAT r = 1.0f / f; x *= r; y *= r; return *this; }
    D3DXVECTOR2 operator+() const { return *this; }
    D3DXVECTOR2 operator-() const { return D3DXVECTOR2(-x, -y); }
    D3DXVECTOR2 operator+(const D3DXVECTOR2 &v) const { return D3DXVECTOR2(x + v.x, y + v.y); }
    D3DXVECTOR2 operator-(const D3DXVECTOR2 &v) const { return D3DXVECTOR2(x - v.x, y - v.y); }
    D3DXVECTOR2 operator*(FLOAT f) const { return D3DXVECTOR2(x * f, y * f); }
    D3DXVECTOR2 operator/(FLOAT f) const { FLOAT r = 1.0f / f; return D3DXVECTOR2(x * r, y * r); }
    friend D3DXVECTOR2 operator*(FLOAT f, const D3DXVECTOR2 &v) { return D3DXVECTOR2(f * v.x, f * v.y); }
    bool operator==(const D3DXVECTOR2 &v) const { return x == v.x && y == v.y; }
    bool operator!=(const D3DXVECTOR2 &v) const { return !(*this == v); }
};
typedef D3DXVECTOR2 *LPD3DXVECTOR2;

struct D3DXVECTOR3 : public D3DVECTOR {
    D3DXVECTOR3() {}
    D3DXVECTOR3(const FLOAT *pf) { x = pf[0]; y = pf[1]; z = pf[2]; }
    D3DXVECTOR3(const D3DVECTOR &v) { x = v.x; y = v.y; z = v.z; }
    D3DXVECTOR3(FLOAT fx, FLOAT fy, FLOAT fz) { x = fx; y = fy; z = fz; }
    operator FLOAT *() { return &x; }
    operator const FLOAT *() const { return &x; }
    D3DXVECTOR3 &operator+=(const D3DXVECTOR3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DXVECTOR3 &operator-=(const D3DXVECTOR3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DXVECTOR3 &operator*=(FLOAT f) { x *= f; y *= f; z *= f; return *this; }
    D3DXVECTOR3 &operator/=(FLOAT f) { FLOAT r = 1.0f / f; x *= r; y *= r; z *= r; return *this; }
    D3DXVECTOR3 operator+() const { return *this; }
    D3DXVECTOR3 operator-() const { return D3DXVECTOR3(-x, -y, -z); }
    D3DXVECTOR3 operator+(const D3DXVECTOR3 &v) const { return D3DXVECTOR3(x + v.x, y + v.y, z + v.z); }
    D3DXVECTOR3 operator-(const D3DXVECTOR3 &v) const { return D3DXVECTOR3(x - v.x, y - v.y, z - v.z); }
    D3DXVECTOR3 operator*(FLOAT f) const { return D3DXVECTOR3(x * f, y * f, z * f); }
    D3DXVECTOR3 operator/(FLOAT f) const { FLOAT r = 1.0f / f; return D3DXVECTOR3(x * r, y * r, z * r); }
    friend D3DXVECTOR3 operator*(FLOAT f, const D3DXVECTOR3 &v) { return D3DXVECTOR3(f * v.x, f * v.y, f * v.z); }
    bool operator==(const D3DXVECTOR3 &v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const D3DXVECTOR3 &v) const { return !(*this == v); }
};
typedef D3DXVECTOR3 *LPD3DXVECTOR3;

struct D3DXVECTOR4 {
    FLOAT x, y, z, w;
    D3DXVECTOR4() {}
    D3DXVECTOR4(const FLOAT *pf) : x(pf[0]), y(pf[1]), z(pf[2]), w(pf[3]) {}
    D3DXVECTOR4(const D3DVECTOR &v, FLOAT fw) : x(v.x), y(v.y), z(v.z), w(fw) {}
    D3DXVECTOR4(FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw) : x(fx), y(fy), z(fz), w(fw) {}
    operator FLOAT *() { return &x; }
    operator const FLOAT *() const { return &x; }
    D3DXVECTOR4 &operator+=(const D3DXVECTOR4 &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    D3DXVECTOR4 &operator-=(const D3DXVECTOR4 &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    D3DXVECTOR4 &operator*=(FLOAT f) { x *= f; y *= f; z *= f; w *= f; return *this; }
    D3DXVECTOR4 &operator/=(FLOAT f) { FLOAT r = 1.0f / f; x *= r; y *= r; z *= r; w *= r; return *this; }
    D3DXVECTOR4 operator+() const { return *this; }
    D3DXVECTOR4 operator-() const { return D3DXVECTOR4(-x, -y, -z, -w); }
    D3DXVECTOR4 operator+(const D3DXVECTOR4 &v) const { return D3DXVECTOR4(x + v.x, y + v.y, z + v.z, w + v.w); }
    D3DXVECTOR4 operator-(const D3DXVECTOR4 &v) const { return D3DXVECTOR4(x - v.x, y - v.y, z - v.z, w - v.w); }
    D3DXVECTOR4 operator*(FLOAT f) const { return D3DXVECTOR4(x * f, y * f, z * f, w * f); }
    D3DXVECTOR4 operator/(FLOAT f) const { FLOAT r = 1.0f / f; return D3DXVECTOR4(x * r, y * r, z * r, w * r); }
    friend D3DXVECTOR4 operator*(FLOAT f, const D3DXVECTOR4 &v) { return D3DXVECTOR4(f * v.x, f * v.y, f * v.z, f * v.w); }
    bool operator==(const D3DXVECTOR4 &v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const D3DXVECTOR4 &v) const { return !(*this == v); }
};
typedef D3DXVECTOR4 *LPD3DXVECTOR4;

// ---- Matrices ---------------------------------------------------------------------------------------------------------
struct D3DXMATRIX : public D3DMATRIX {
    D3DXMATRIX() {}
    D3DXMATRIX(const FLOAT *pf) { memcpy(&_11, pf, sizeof(D3DMATRIX)); }
    D3DXMATRIX(const D3DMATRIX &m) { memcpy(&_11, &m, sizeof(D3DMATRIX)); }
    D3DXMATRIX(FLOAT f11, FLOAT f12, FLOAT f13, FLOAT f14, FLOAT f21, FLOAT f22, FLOAT f23, FLOAT f24,
               FLOAT f31, FLOAT f32, FLOAT f33, FLOAT f34, FLOAT f41, FLOAT f42, FLOAT f43, FLOAT f44) {
        _11 = f11; _12 = f12; _13 = f13; _14 = f14; _21 = f21; _22 = f22; _23 = f23; _24 = f24;
        _31 = f31; _32 = f32; _33 = f33; _34 = f34; _41 = f41; _42 = f42; _43 = f43; _44 = f44;
    }
    FLOAT &operator()(UINT row, UINT col) { return m[row][col]; }
    FLOAT operator()(UINT row, UINT col) const { return m[row][col]; }
    operator FLOAT *() { return &_11; }
    operator const FLOAT *() const { return &_11; }
    D3DXMATRIX &operator*=(const D3DXMATRIX &b);
    D3DXMATRIX &operator+=(const D3DXMATRIX &b) { for (int i = 0; i < 16; i++) (&_11)[i] += (&b._11)[i]; return *this; }
    D3DXMATRIX &operator-=(const D3DXMATRIX &b) { for (int i = 0; i < 16; i++) (&_11)[i] -= (&b._11)[i]; return *this; }
    D3DXMATRIX &operator*=(FLOAT f) { for (int i = 0; i < 16; i++) (&_11)[i] *= f; return *this; }
    D3DXMATRIX &operator/=(FLOAT f) { return *this *= 1.0f / f; }
    D3DXMATRIX operator+() const { return *this; }
    D3DXMATRIX operator-() const { D3DXMATRIX r(*this); return r *= -1.0f; }
    D3DXMATRIX operator*(const D3DXMATRIX &b) const;
    D3DXMATRIX operator+(const D3DXMATRIX &b) const { D3DXMATRIX r(*this); return r += b; }
    D3DXMATRIX operator-(const D3DXMATRIX &b) const { D3DXMATRIX r(*this); return r -= b; }
    D3DXMATRIX operator*(FLOAT f) const { D3DXMATRIX r(*this); return r *= f; }
    D3DXMATRIX operator/(FLOAT f) const { D3DXMATRIX r(*this); return r /= f; }
    friend D3DXMATRIX operator*(FLOAT f, const D3DXMATRIX &a) { return a * f; }
    bool operator==(const D3DXMATRIX &b) const { return memcmp(&_11, &b._11, sizeof(D3DMATRIX)) == 0; }
    bool operator!=(const D3DXMATRIX &b) const { return !(*this == b); }
};
typedef D3DXMATRIX *LPD3DXMATRIX;

// D3DXMATRIX aligned to 16 bytes, for SIMD code; otherwise the same matrix.
struct D3DX_ALIGN16 D3DXMATRIXA16 : public D3DXMATRIX {
    D3DXMATRIXA16() {}
    D3DXMATRIXA16(const FLOAT *pf) : D3DXMATRIX(pf) {}
    D3DXMATRIXA16(const D3DMATRIX &m) : D3DXMATRIX(m) {}
    D3DXMATRIXA16(FLOAT f11, FLOAT f12, FLOAT f13, FLOAT f14, FLOAT f21, FLOAT f22, FLOAT f23, FLOAT f24,
                  FLOAT f31, FLOAT f32, FLOAT f33, FLOAT f34, FLOAT f41, FLOAT f42, FLOAT f43, FLOAT f44)
        : D3DXMATRIX(f11, f12, f13, f14, f21, f22, f23, f24, f31, f32, f33, f34, f41, f42, f43, f44) {}
    D3DXMATRIXA16 &operator=(const D3DXMATRIX &m) { memcpy(&_11, &m, sizeof(D3DMATRIX)); return *this; }
};

// ---- Quaternions, planes, colors --------------------------------------------------------------------------------------
struct D3DXQUATERNION {
    FLOAT x, y, z, w;
    D3DXQUATERNION() {}
    D3DXQUATERNION(const FLOAT *pf) : x(pf[0]), y(pf[1]), z(pf[2]), w(pf[3]) {}
    D3DXQUATERNION(FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw) : x(fx), y(fy), z(fz), w(fw) {}
    operator FLOAT *() { return &x; }
    operator const FLOAT *() const { return &x; }
    D3DXQUATERNION &operator+=(const D3DXQUATERNION &q) { x += q.x; y += q.y; z += q.z; w += q.w; return *this; }
    D3DXQUATERNION &operator-=(const D3DXQUATERNION &q) { x -= q.x; y -= q.y; z -= q.z; w -= q.w; return *this; }
    D3DXQUATERNION &operator*=(const D3DXQUATERNION &q);
    D3DXQUATERNION &operator*=(FLOAT f) { x *= f; y *= f; z *= f; w *= f; return *this; }
    D3DXQUATERNION &operator/=(FLOAT f) { return *this *= 1.0f / f; }
    D3DXQUATERNION operator+() const { return *this; }
    D3DXQUATERNION operator-() const { return D3DXQUATERNION(-x, -y, -z, -w); }
    D3DXQUATERNION operator+(const D3DXQUATERNION &q) const { return D3DXQUATERNION(x + q.x, y + q.y, z + q.z, w + q.w); }
    D3DXQUATERNION operator-(const D3DXQUATERNION &q) const { return D3DXQUATERNION(x - q.x, y - q.y, z - q.z, w - q.w); }
    D3DXQUATERNION operator*(const D3DXQUATERNION &q) const;
    D3DXQUATERNION operator*(FLOAT f) const { return D3DXQUATERNION(x * f, y * f, z * f, w * f); }
    D3DXQUATERNION operator/(FLOAT f) const { FLOAT r = 1.0f / f; return D3DXQUATERNION(x * r, y * r, z * r, w * r); }
    friend D3DXQUATERNION operator*(FLOAT f, const D3DXQUATERNION &q) { return q * f; }
    bool operator==(const D3DXQUATERNION &q) const { return x == q.x && y == q.y && z == q.z && w == q.w; }
    bool operator!=(const D3DXQUATERNION &q) const { return !(*this == q); }
};
typedef D3DXQUATERNION *LPD3DXQUATERNION;

// The plane a*x + b*y + c*z + d = 0.
struct D3DXPLANE {
    FLOAT a, b, c, d;
    D3DXPLANE() {}
    D3DXPLANE(const FLOAT *pf) : a(pf[0]), b(pf[1]), c(pf[2]), d(pf[3]) {}
    D3DXPLANE(FLOAT fa, FLOAT fb, FLOAT fc, FLOAT fd) : a(fa), b(fb), c(fc), d(fd) {}
    operator FLOAT *() { return &a; }
    operator const FLOAT *() const { return &a; }
    D3DXPLANE operator+() const { return *this; }
    D3DXPLANE operator-() const { return D3DXPLANE(-a, -b, -c, -d); }
    bool operator==(const D3DXPLANE &p) const { return a == p.a && b == p.b && c == p.c && d == p.d; }
    bool operator!=(const D3DXPLANE &p) const { return !(*this == p); }
};
typedef D3DXPLANE *LPD3DXPLANE;

struct D3DXCOLOR {
    FLOAT r, g, b, a;
    D3DXCOLOR() {}
    D3DXCOLOR(DWORD argb)
        : r(((argb >> 16) & 0xff) / 255.0f), g(((argb >> 8) & 0xff) / 255.0f), b((argb & 0xff) / 255.0f),
          a(((argb >> 24) & 0xff) / 255.0f) {}
    D3DXCOLOR(const FLOAT *pf) : r(pf[0]), g(pf[1]), b(pf[2]), a(pf[3]) {}
    D3DXCOLOR(const D3DCOLORVALUE &c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    D3DXCOLOR(FLOAT fr, FLOAT fg, FLOAT fb, FLOAT fa) : r(fr), g(fg), b(fb), a(fa) {}
    operator DWORD() const {
        auto c = [](FLOAT v) -> DWORD { return v >= 1.0f ? 0xffu : v <= 0.0f ? 0u : (DWORD)(v * 255.0f + 0.5f); };
        return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
    }
    operator FLOAT *() { return &r; }
    operator const FLOAT *() const { return &r; }
    operator D3DCOLORVALUE *() { return (D3DCOLORVALUE *)&r; }
    operator const D3DCOLORVALUE *() const { return (const D3DCOLORVALUE *)&r; }
    operator D3DCOLORVALUE &() { return *(D3DCOLORVALUE *)&r; }
    operator const D3DCOLORVALUE &() const { return *(const D3DCOLORVALUE *)&r; }
    D3DXCOLOR &operator+=(const D3DXCOLOR &c) { r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
    D3DXCOLOR &operator-=(const D3DXCOLOR &c) { r -= c.r; g -= c.g; b -= c.b; a -= c.a; return *this; }
    D3DXCOLOR &operator*=(FLOAT f) { r *= f; g *= f; b *= f; a *= f; return *this; }
    D3DXCOLOR &operator/=(FLOAT f) { return *this *= 1.0f / f; }
    D3DXCOLOR operator+() const { return *this; }
    D3DXCOLOR operator-() const { return D3DXCOLOR(-r, -g, -b, -a); }
    D3DXCOLOR operator+(const D3DXCOLOR &c) const { return D3DXCOLOR(r + c.r, g + c.g, b + c.b, a + c.a); }
    D3DXCOLOR operator-(const D3DXCOLOR &c) const { return D3DXCOLOR(r - c.r, g - c.g, b - c.b, a - c.a); }
    D3DXCOLOR operator*(FLOAT f) const { return D3DXCOLOR(r * f, g * f, b * f, a * f); }
    D3DXCOLOR operator/(FLOAT f) const { FLOAT i = 1.0f / f; return D3DXCOLOR(r * i, g * i, b * i, a * i); }
    friend D3DXCOLOR operator*(FLOAT f, const D3DXCOLOR &c) { return c * f; }
    bool operator==(const D3DXCOLOR &c) const { return r == c.r && g == c.g && b == c.b && a == c.a; }
    bool operator!=(const D3DXCOLOR &c) const { return !(*this == c); }
};
typedef D3DXCOLOR *LPD3DXCOLOR;

// ---- Inline functions -------------------------------------------------------------------------------------------------
inline FLOAT D3DXVec2Length(const D3DXVECTOR2 *v) { return sqrtf(v->x * v->x + v->y * v->y); }
inline FLOAT D3DXVec2LengthSq(const D3DXVECTOR2 *v) { return v->x * v->x + v->y * v->y; }
inline FLOAT D3DXVec2Dot(const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { return a->x * b->x + a->y * b->y; }
inline FLOAT D3DXVec2CCW(const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { return a->x * b->y - a->y * b->x; }
inline D3DXVECTOR2 *D3DXVec2Add(D3DXVECTOR2 *o, const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { o->x = a->x + b->x; o->y = a->y + b->y; return o; }
inline D3DXVECTOR2 *D3DXVec2Subtract(D3DXVECTOR2 *o, const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { o->x = a->x - b->x; o->y = a->y - b->y; return o; }
inline D3DXVECTOR2 *D3DXVec2Minimize(D3DXVECTOR2 *o, const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { o->x = a->x < b->x ? a->x : b->x; o->y = a->y < b->y ? a->y : b->y; return o; }
inline D3DXVECTOR2 *D3DXVec2Maximize(D3DXVECTOR2 *o, const D3DXVECTOR2 *a, const D3DXVECTOR2 *b) { o->x = a->x > b->x ? a->x : b->x; o->y = a->y > b->y ? a->y : b->y; return o; }
inline D3DXVECTOR2 *D3DXVec2Scale(D3DXVECTOR2 *o, const D3DXVECTOR2 *v, FLOAT s) { o->x = v->x * s; o->y = v->y * s; return o; }
inline D3DXVECTOR2 *D3DXVec2Lerp(D3DXVECTOR2 *o, const D3DXVECTOR2 *a, const D3DXVECTOR2 *b, FLOAT s) { o->x = a->x + s * (b->x - a->x); o->y = a->y + s * (b->y - a->y); return o; }

inline FLOAT D3DXVec3Length(const D3DXVECTOR3 *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }
inline FLOAT D3DXVec3LengthSq(const D3DXVECTOR3 *v) { return v->x * v->x + v->y * v->y + v->z * v->z; }
inline FLOAT D3DXVec3Dot(const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { return a->x * b->x + a->y * b->y + a->z * b->z; }
inline D3DXVECTOR3 *D3DXVec3Cross(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) {
    D3DXVECTOR3 r(a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x);
    *o = r;
    return o;
}
inline D3DXVECTOR3 *D3DXVec3Add(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { o->x = a->x + b->x; o->y = a->y + b->y; o->z = a->z + b->z; return o; }
inline D3DXVECTOR3 *D3DXVec3Subtract(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) { o->x = a->x - b->x; o->y = a->y - b->y; o->z = a->z - b->z; return o; }
inline D3DXVECTOR3 *D3DXVec3Minimize(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) {
    o->x = a->x < b->x ? a->x : b->x; o->y = a->y < b->y ? a->y : b->y; o->z = a->z < b->z ? a->z : b->z; return o;
}
inline D3DXVECTOR3 *D3DXVec3Maximize(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b) {
    o->x = a->x > b->x ? a->x : b->x; o->y = a->y > b->y ? a->y : b->y; o->z = a->z > b->z ? a->z : b->z; return o;
}
inline D3DXVECTOR3 *D3DXVec3Scale(D3DXVECTOR3 *o, const D3DXVECTOR3 *v, FLOAT s) { o->x = v->x * s; o->y = v->y * s; o->z = v->z * s; return o; }
inline D3DXVECTOR3 *D3DXVec3Lerp(D3DXVECTOR3 *o, const D3DXVECTOR3 *a, const D3DXVECTOR3 *b, FLOAT s) {
    o->x = a->x + s * (b->x - a->x); o->y = a->y + s * (b->y - a->y); o->z = a->z + s * (b->z - a->z); return o;
}

inline FLOAT D3DXVec4Length(const D3DXVECTOR4 *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z + v->w * v->w); }
inline FLOAT D3DXVec4LengthSq(const D3DXVECTOR4 *v) { return v->x * v->x + v->y * v->y + v->z * v->z + v->w * v->w; }
inline FLOAT D3DXVec4Dot(const D3DXVECTOR4 *a, const D3DXVECTOR4 *b) { return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w; }
inline D3DXVECTOR4 *D3DXVec4Add(D3DXVECTOR4 *o, const D3DXVECTOR4 *a, const D3DXVECTOR4 *b) { *o = *a + *b; return o; }
inline D3DXVECTOR4 *D3DXVec4Subtract(D3DXVECTOR4 *o, const D3DXVECTOR4 *a, const D3DXVECTOR4 *b) { *o = *a - *b; return o; }
inline D3DXVECTOR4 *D3DXVec4Scale(D3DXVECTOR4 *o, const D3DXVECTOR4 *v, FLOAT s) { *o = *v * s; return o; }
inline D3DXVECTOR4 *D3DXVec4Lerp(D3DXVECTOR4 *o, const D3DXVECTOR4 *a, const D3DXVECTOR4 *b, FLOAT s) { *o = *a + (*b - *a) * s; return o; }

inline D3DXMATRIX *D3DXMatrixIdentity(D3DXMATRIX *o) {
    memset(o, 0, sizeof(D3DMATRIX));
    o->_11 = o->_22 = o->_33 = o->_44 = 1.0f;
    return o;
}
inline BOOL D3DXMatrixIsIdentity(const D3DXMATRIX *m) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (m->m[r][c] != (r == c ? 1.0f : 0.0f)) return FALSE;
    return TRUE;
}

inline FLOAT D3DXQuaternionLength(const D3DXQUATERNION *q) { return sqrtf(q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w); }
inline FLOAT D3DXQuaternionLengthSq(const D3DXQUATERNION *q) { return q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w; }
inline FLOAT D3DXQuaternionDot(const D3DXQUATERNION *a, const D3DXQUATERNION *b) { return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w; }
inline D3DXQUATERNION *D3DXQuaternionIdentity(D3DXQUATERNION *o) { o->x = o->y = o->z = 0.0f; o->w = 1.0f; return o; }
inline BOOL D3DXQuaternionIsIdentity(const D3DXQUATERNION *q) { return q->x == 0.0f && q->y == 0.0f && q->z == 0.0f && q->w == 1.0f; }
inline D3DXQUATERNION *D3DXQuaternionConjugate(D3DXQUATERNION *o, const D3DXQUATERNION *q) { o->x = -q->x; o->y = -q->y; o->z = -q->z; o->w = q->w; return o; }

inline FLOAT D3DXPlaneDot(const D3DXPLANE *p, const D3DXVECTOR4 *v) { return p->a * v->x + p->b * v->y + p->c * v->z + p->d * v->w; }
inline FLOAT D3DXPlaneDotCoord(const D3DXPLANE *p, const D3DXVECTOR3 *v) { return p->a * v->x + p->b * v->y + p->c * v->z + p->d; }
inline FLOAT D3DXPlaneDotNormal(const D3DXPLANE *p, const D3DXVECTOR3 *v) { return p->a * v->x + p->b * v->y + p->c * v->z; }
inline D3DXPLANE *D3DXPlaneScale(D3DXPLANE *o, const D3DXPLANE *p, FLOAT s) { o->a = p->a * s; o->b = p->b * s; o->c = p->c * s; o->d = p->d * s; return o; }

inline D3DXCOLOR *D3DXColorNegative(D3DXCOLOR *o, const D3DXCOLOR *c) { o->r = 1.0f - c->r; o->g = 1.0f - c->g; o->b = 1.0f - c->b; o->a = c->a; return o; }
inline D3DXCOLOR *D3DXColorAdd(D3DXCOLOR *o, const D3DXCOLOR *a, const D3DXCOLOR *b) { *o = *a + *b; return o; }
inline D3DXCOLOR *D3DXColorSubtract(D3DXCOLOR *o, const D3DXCOLOR *a, const D3DXCOLOR *b) { *o = *a - *b; return o; }
inline D3DXCOLOR *D3DXColorScale(D3DXCOLOR *o, const D3DXCOLOR *c, FLOAT s) { *o = *c * s; return o; }
inline D3DXCOLOR *D3DXColorModulate(D3DXCOLOR *o, const D3DXCOLOR *a, const D3DXCOLOR *b) {
    o->r = a->r * b->r; o->g = a->g * b->g; o->b = a->b * b->b; o->a = a->a * b->a; return o;
}
inline D3DXCOLOR *D3DXColorLerp(D3DXCOLOR *o, const D3DXCOLOR *a, const D3DXCOLOR *b, FLOAT s) { *o = *a + (*b - *a) * s; return o; }

// ---- Functions (src/d3dx9math.cpp) -------------------------------------------------------------------------------------
extern "C" {

D3DXVECTOR2 *WINAPI D3DXVec2Normalize(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV);
D3DXVECTOR4 *WINAPI D3DXVec2Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM);
D3DXVECTOR2 *WINAPI D3DXVec2TransformCoord(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM);
D3DXVECTOR2 *WINAPI D3DXVec2TransformNormal(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV, const D3DXMATRIX *pM);

D3DXVECTOR3 *WINAPI D3DXVec3Normalize(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV);
D3DXVECTOR4 *WINAPI D3DXVec3Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *WINAPI D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *WINAPI D3DXVec3TransformNormal(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *WINAPI D3DXVec3Project(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                                    const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld);
D3DXVECTOR3 *WINAPI D3DXVec3Unproject(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                                      const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld);

D3DXVECTOR4 *WINAPI D3DXVec4Normalize(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV);
D3DXVECTOR4 *WINAPI D3DXVec4Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM);

FLOAT WINAPI D3DXMatrixDeterminant(const D3DXMATRIX *pM);
D3DXMATRIX *WINAPI D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2);
D3DXMATRIX *WINAPI D3DXMatrixMultiplyTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2);
D3DXMATRIX *WINAPI D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM);
D3DXMATRIX *WINAPI D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM);
D3DXMATRIX *WINAPI D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz);
D3DXMATRIX *WINAPI D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z);
D3DXMATRIX *WINAPI D3DXMatrixRotationX(D3DXMATRIX *pOut, FLOAT angle);
D3DXMATRIX *WINAPI D3DXMatrixRotationY(D3DXMATRIX *pOut, FLOAT angle);
D3DXMATRIX *WINAPI D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT angle);
D3DXMATRIX *WINAPI D3DXMatrixRotationAxis(D3DXMATRIX *pOut, const D3DXVECTOR3 *pV, FLOAT angle);
D3DXMATRIX *WINAPI D3DXMatrixRotationQuaternion(D3DXMATRIX *pOut, const D3DXQUATERNION *pQ);
D3DXMATRIX *WINAPI D3DXMatrixRotationYawPitchRoll(D3DXMATRIX *pOut, FLOAT yaw, FLOAT pitch, FLOAT roll);
D3DXMATRIX *WINAPI D3DXMatrixLookAtLH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt, const D3DXVECTOR3 *pUp);
D3DXMATRIX *WINAPI D3DXMatrixLookAtRH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt, const D3DXVECTOR3 *pUp);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveLH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveRH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT aspect, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveFovRH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT aspect, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixPerspectiveOffCenterRH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixOrthoLH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixOrthoRH(D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixOrthoOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixOrthoOffCenterRH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf);
D3DXMATRIX *WINAPI D3DXMatrixTransformation(D3DXMATRIX *pOut, const D3DXVECTOR3 *pScalingCenter,
                                            const D3DXQUATERNION *pScalingRotation, const D3DXVECTOR3 *pScaling,
                                            const D3DXVECTOR3 *pRotationCenter, const D3DXQUATERNION *pRotation,
                                            const D3DXVECTOR3 *pTranslation);
D3DXMATRIX *WINAPI D3DXMatrixTransformation2D(D3DXMATRIX *pOut, const D3DXVECTOR2 *pScalingCenter, FLOAT ScalingRotation,
                                              const D3DXVECTOR2 *pScaling, const D3DXVECTOR2 *pRotationCenter,
                                              FLOAT Rotation, const D3DXVECTOR2 *pTranslation);
HRESULT WINAPI D3DXMatrixDecompose(D3DXVECTOR3 *pOutScale, D3DXQUATERNION *pOutRotation, D3DXVECTOR3 *pOutTranslation,
                                   const D3DXMATRIX *pM);
D3DXMATRIX *WINAPI D3DXMatrixReflect(D3DXMATRIX *pOut, const D3DXPLANE *pPlane);
D3DXMATRIX *WINAPI D3DXMatrixShadow(D3DXMATRIX *pOut, const D3DXVECTOR4 *pLight, const D3DXPLANE *pPlane);

D3DXQUATERNION *WINAPI D3DXQuaternionNormalize(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ);
D3DXQUATERNION *WINAPI D3DXQuaternionInverse(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ);
D3DXQUATERNION *WINAPI D3DXQuaternionMultiply(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2);
D3DXQUATERNION *WINAPI D3DXQuaternionRotationAxis(D3DXQUATERNION *pOut, const D3DXVECTOR3 *pV, FLOAT angle);
D3DXQUATERNION *WINAPI D3DXQuaternionRotationMatrix(D3DXQUATERNION *pOut, const D3DXMATRIX *pM);
D3DXQUATERNION *WINAPI D3DXQuaternionRotationYawPitchRoll(D3DXQUATERNION *pOut, FLOAT yaw, FLOAT pitch, FLOAT roll);
D3DXQUATERNION *WINAPI D3DXQuaternionSlerp(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2, FLOAT t);
void WINAPI D3DXQuaternionToAxisAngle(const D3DXQUATERNION *pQ, D3DXVECTOR3 *pAxis, FLOAT *pAngle);
D3DXQUATERNION *WINAPI D3DXQuaternionLn(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ);

D3DXPLANE *WINAPI D3DXPlaneNormalize(D3DXPLANE *pOut, const D3DXPLANE *pP);
D3DXPLANE *WINAPI D3DXPlaneFromPointNormal(D3DXPLANE *pOut, const D3DXVECTOR3 *pPoint, const D3DXVECTOR3 *pNormal);
D3DXPLANE *WINAPI D3DXPlaneFromPoints(D3DXPLANE *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2, const D3DXVECTOR3 *pV3);
D3DXVECTOR3 *WINAPI D3DXPlaneIntersectLine(D3DXVECTOR3 *pOut, const D3DXPLANE *pP, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2);
D3DXPLANE *WINAPI D3DXPlaneTransform(D3DXPLANE *pOut, const D3DXPLANE *pP, const D3DXMATRIX *pM);

D3DXCOLOR *WINAPI D3DXColorAdjustSaturation(D3DXCOLOR *pOut, const D3DXCOLOR *pC, FLOAT s);
D3DXCOLOR *WINAPI D3DXColorAdjustContrast(D3DXCOLOR *pOut, const D3DXCOLOR *pC, FLOAT c);

}  // extern "C"

inline D3DXMATRIX &D3DXMATRIX::operator*=(const D3DXMATRIX &b) { D3DXMatrixMultiply(this, this, &b); return *this; }
inline D3DXMATRIX D3DXMATRIX::operator*(const D3DXMATRIX &b) const { D3DXMATRIX r; D3DXMatrixMultiply(&r, this, &b); return r; }
inline D3DXQUATERNION &D3DXQUATERNION::operator*=(const D3DXQUATERNION &q) { D3DXQuaternionMultiply(this, this, &q); return *this; }
inline D3DXQUATERNION D3DXQUATERNION::operator*(const D3DXQUATERNION &q) const { D3DXQUATERNION r; D3DXQuaternionMultiply(&r, this, &q); return r; }

#endif  // PORTICO_D3DX9MATH_H
