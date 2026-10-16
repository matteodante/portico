// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX math: checked against mathematical properties (inverse, round trips, equivalent constructions,
// known points of the view and projection transforms), not against another implementation's numbers.
#include <d3dx9.h>

#include <cstdio>
#include <cmath>

static int g_failures = 0, g_checks = 0;

#define CHECK(cond)                                                                     \
    do {                                                                                \
        g_checks++;                                                                     \
        if (!(cond)) {                                                                  \
            g_failures++;                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
        }                                                                               \
    } while (0)

static bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps * (1.0f + std::fabs(a) + std::fabs(b)); }
static bool Near(const D3DXVECTOR3 &a, const D3DXVECTOR3 &b, float eps = 1e-4f) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}
static bool Near(const D3DXMATRIX &a, const D3DXMATRIX &b, float eps = 1e-4f) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (!Near(a.m[i][j], b.m[i][j], eps)) return false;
    return true;
}
static bool SameRotation(const D3DXQUATERNION &a, const D3DXQUATERNION &b) {
    float d = std::fabs(D3DXQuaternionDot(&a, &b));
    return Near(d, 1.0f, 1e-4f);  // q and -q are the same rotation
}

static D3DXVECTOR3 Point(const D3DXVECTOR3 &v, const D3DXMATRIX &m) {
    D3DXVECTOR3 r;
    D3DXVec3TransformCoord(&r, &v, &m);
    return r;
}

static void Matrices() {
    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    CHECK(D3DXMatrixIsIdentity(&id));

    // Translation and scaling act on row vectors.
    D3DXMATRIX t, s;
    D3DXMatrixTranslation(&t, 1.0f, 2.0f, 3.0f);
    D3DXMatrixScaling(&s, 2.0f, 3.0f, 4.0f);
    CHECK(Near(Point(D3DXVECTOR3(1, 1, 1), t), D3DXVECTOR3(2, 3, 4)));
    CHECK(Near(Point(D3DXVECTOR3(1, 1, 1), s * t), D3DXVECTOR3(3, 5, 7)));  // scale first, then translate

    // Axis rotations: x -> -z for +90 degrees about Y, y -> z about X, x -> y about Z (Direct3D's left-handed rule).
    D3DXMATRIX rx, ry, rz;
    D3DXMatrixRotationX(&rx, D3DX_PI / 2);
    D3DXMatrixRotationY(&ry, D3DX_PI / 2);
    D3DXMatrixRotationZ(&rz, D3DX_PI / 2);
    CHECK(Near(Point(D3DXVECTOR3(0, 1, 0), rx), D3DXVECTOR3(0, 0, 1)));
    CHECK(Near(Point(D3DXVECTOR3(1, 0, 0), ry), D3DXVECTOR3(0, 0, -1)));
    CHECK(Near(Point(D3DXVECTOR3(1, 0, 0), rz), D3DXVECTOR3(0, 1, 0)));

    // RotationAxis agrees with the axis rotations, and with an unnormalized axis.
    D3DXMATRIX ra;
    D3DXVECTOR3 ax(3, 0, 0), ay(0, 2, 0), az(0, 0, 5);
    D3DXMatrixRotationAxis(&ra, &ax, 0.7f);
    D3DXMatrixRotationX(&rx, 0.7f);
    CHECK(Near(ra, rx));
    D3DXMatrixRotationAxis(&ra, &ay, -1.1f);
    D3DXMatrixRotationY(&ry, -1.1f);
    CHECK(Near(ra, ry));
    D3DXMatrixRotationAxis(&ra, &az, 2.3f);
    D3DXMatrixRotationZ(&rz, 2.3f);
    CHECK(Near(ra, rz));

    // Yaw/pitch/roll = roll, then pitch, then yaw.
    D3DXMATRIX ypr, r, p, y;
    D3DXMatrixRotationYawPitchRoll(&ypr, 0.3f, -0.4f, 1.2f);
    D3DXMatrixRotationZ(&r, 1.2f);
    D3DXMatrixRotationX(&p, -0.4f);
    D3DXMatrixRotationY(&y, 0.3f);
    CHECK(Near(ypr, r * p * y));

    // Inverse and determinant.
    D3DXMATRIX m = s * ypr * t, inv;
    float det = 0.0f;
    CHECK(D3DXMatrixInverse(&inv, &det, &m) != nullptr);
    CHECK(Near(det, 24.0f, 1e-3f));  // rotations and translations keep the volume; the scale is 2 * 3 * 4
    CHECK(Near(m * inv, id, 1e-4f));
    CHECK(Near(inv * m, id, 1e-4f));
    CHECK(Near(D3DXMatrixDeterminant(&m), 24.0f, 1e-3f));
    D3DXMATRIX general(1, 2, 0, 1, 0, 1, 3, 0, 2, 0, 1, 4, 1, 1, 0, 2), ginv;
    CHECK(D3DXMatrixInverse(&ginv, nullptr, &general) != nullptr);
    CHECK(Near(general * ginv, id, 1e-4f));
    D3DXMATRIX singular(1, 2, 3, 4, 2, 4, 6, 8, 0, 1, 0, 1, 1, 0, 1, 0);
    CHECK(D3DXMatrixInverse(&ginv, &det, &singular) == nullptr);
    CHECK(det == 0.0f);

    // Transpose and multiply-transpose.
    D3DXMATRIX tr, mt;
    D3DXMatrixTranspose(&tr, &general);
    CHECK(tr._12 == general._21 && tr._43 == general._34);
    D3DXMatrixMultiplyTranspose(&mt, &m, &general);
    D3DXMATRIX prod = m * general;
    D3DXMatrixTranspose(&prod, &prod);
    CHECK(Near(mt, prod));

    // Aliasing: the output may be an input.
    D3DXMATRIX a = m;
    D3DXMatrixMultiply(&a, &a, &inv);
    CHECK(Near(a, id, 1e-4f));
}

static void ViewAndProjection() {
    const D3DXVECTOR3 eye(0, 3, -5), at(0, 0, 0), up(0, 1, 0);
    D3DXMATRIX v;
    D3DXMatrixLookAtLH(&v, &eye, &at, &up);
    CHECK(Near(Point(eye, v), D3DXVECTOR3(0, 0, 0)));
    D3DXVECTOR3 d = at - eye;
    CHECK(Near(Point(at, v), D3DXVECTOR3(0, 0, D3DXVec3Length(&d))));  // in front of the camera: +z
    D3DXMatrixLookAtRH(&v, &eye, &at, &up);
    CHECK(Near(Point(eye, v), D3DXVECTOR3(0, 0, 0)));
    CHECK(Near(Point(at, v), D3DXVECTOR3(0, 0, -D3DXVec3Length(&d))));  // right-handed: in front is -z

    const float zn = 1.0f, zf = 100.0f;
    D3DXMATRIX pr;
    D3DXMatrixPerspectiveFovLH(&pr, D3DX_PI / 4, 4.0f / 3.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(0, 0, zn), pr).z, 0.0f));
    CHECK(Near(Point(D3DXVECTOR3(0, 0, zf), pr).z, 1.0f));
    // The edge of the field of view lands on the edge of the clip space.
    float h = zn * tanf(D3DX_PI / 8);
    CHECK(Near(Point(D3DXVECTOR3(0, h, zn), pr).y, 1.0f));
    CHECK(Near(Point(D3DXVECTOR3(h * 4.0f / 3.0f, 0, zn), pr).x, 1.0f));
    D3DXMatrixPerspectiveFovRH(&pr, D3DX_PI / 4, 4.0f / 3.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(0, 0, -zn), pr).z, 0.0f));
    CHECK(Near(Point(D3DXVECTOR3(0, 0, -zf), pr).z, 1.0f));

    D3DXMatrixPerspectiveLH(&pr, 2.0f, 1.5f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(1.0f, 0.75f, zn), pr), D3DXVECTOR3(1, 1, 0)));
    CHECK(Near(Point(D3DXVECTOR3(0, 0, zf), pr).z, 1.0f));
    D3DXMatrixPerspectiveRH(&pr, 2.0f, 1.5f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(1.0f, 0.75f, -zn), pr), D3DXVECTOR3(1, 1, 0)));
    D3DXMatrixPerspectiveOffCenterLH(&pr, -1.0f, 3.0f, -2.0f, 1.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(-1.0f, -2.0f, zn), pr), D3DXVECTOR3(-1, -1, 0)));
    CHECK(Near(Point(D3DXVECTOR3(3.0f, 1.0f, zn), pr), D3DXVECTOR3(1, 1, 0)));
    D3DXMatrixPerspectiveOffCenterRH(&pr, -1.0f, 3.0f, -2.0f, 1.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(3.0f, 1.0f, -zn), pr), D3DXVECTOR3(1, 1, 0)));

    D3DXMatrixOrthoLH(&pr, 4.0f, 2.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(2, 1, zn), pr), D3DXVECTOR3(1, 1, 0)));
    CHECK(Near(Point(D3DXVECTOR3(-2, -1, zf), pr), D3DXVECTOR3(-1, -1, 1)));
    D3DXMatrixOrthoRH(&pr, 4.0f, 2.0f, zn, zf);
    CHECK(Near(Point(D3DXVECTOR3(2, 1, -zf), pr), D3DXVECTOR3(1, 1, 1)));
    D3DXMatrixOrthoOffCenterLH(&pr, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 1.0f);
    CHECK(Near(Point(D3DXVECTOR3(0, 0, 0), pr), D3DXVECTOR3(-1, 1, 0)));
    CHECK(Near(Point(D3DXVECTOR3(640, 480, 1), pr), D3DXVECTOR3(1, -1, 1)));
    D3DXMatrixOrthoOffCenterRH(&pr, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 1.0f);
    CHECK(Near(Point(D3DXVECTOR3(640, 480, -1), pr), D3DXVECTOR3(1, -1, 1)));

    // Project and Unproject undo each other.
    D3DVIEWPORT9 vp = {10, 20, 640, 480, 0.0f, 1.0f};
    D3DXMATRIX world, view, proj;
    D3DXMatrixRotationYawPitchRoll(&world, 0.5f, 0.2f, 0.1f);
    D3DXMatrixLookAtLH(&view, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&proj, D3DX_PI / 3, 640.0f / 480.0f, 0.5f, 50.0f);
    D3DXVECTOR3 pt(0.3f, -0.2f, 0.4f), screen, back;
    D3DXVec3Project(&screen, &pt, &vp, &proj, &view, &world);
    CHECK(screen.x > vp.X && screen.x < vp.X + vp.Width && screen.y > vp.Y && screen.y < vp.Y + vp.Height);
    D3DXVec3Unproject(&back, &screen, &vp, &proj, &view, &world);
    CHECK(Near(back, pt, 1e-3f));
}

static void Quaternions() {
    D3DXVECTOR3 axis(1, 2, 3);
    D3DXQUATERNION q;
    D3DXQuaternionRotationAxis(&q, &axis, 0.9f);
    CHECK(Near(D3DXQuaternionLength(&q), 1.0f));
    D3DXMATRIX mq, ma;
    D3DXMatrixRotationQuaternion(&mq, &q);
    D3DXMatrixRotationAxis(&ma, &axis, 0.9f);
    CHECK(Near(mq, ma));

    // RotationMatrix inverts RotationQuaternion, for every branch of the extraction.
    const float angles[] = {0.2f, 2.0f, 3.0f};
    const D3DXVECTOR3 axes[] = {D3DXVECTOR3(1, 0, 0), D3DXVECTOR3(0, 1, 0), D3DXVECTOR3(0, 0, 1), D3DXVECTOR3(1, -1, 2)};
    for (float a : angles)
        for (const D3DXVECTOR3 &v : axes) {
            D3DXQUATERNION q1, q2;
            D3DXQuaternionRotationAxis(&q1, &v, a);
            D3DXMATRIX m;
            D3DXMatrixRotationQuaternion(&m, &q1);
            D3DXQuaternionRotationMatrix(&q2, &m);
            CHECK(SameRotation(q1, q2));
        }

    // Multiply(q1, q2) is the rotation q1 followed by q2, like the matrix product M1 * M2.
    D3DXQUATERNION q1, q2, q12;
    D3DXVECTOR3 a1(0, 1, 0), a2(1, 0, 1);
    D3DXQuaternionRotationAxis(&q1, &a1, 0.5f);
    D3DXQuaternionRotationAxis(&q2, &a2, 1.3f);
    D3DXQuaternionMultiply(&q12, &q1, &q2);
    D3DXMATRIX m1, m2, m12;
    D3DXMatrixRotationQuaternion(&m1, &q1);
    D3DXMatrixRotationQuaternion(&m2, &q2);
    D3DXMatrixRotationQuaternion(&m12, &q12);
    CHECK(Near(m12, m1 * m2));
    CHECK(SameRotation(q1 * q2, q12));

    // Yaw/pitch/roll agrees with the matrix version.
    D3DXQUATERNION qypr;
    D3DXQuaternionRotationYawPitchRoll(&qypr, 0.3f, -0.4f, 1.2f);
    D3DXMATRIX mypr, mfromq;
    D3DXMatrixRotationYawPitchRoll(&mypr, 0.3f, -0.4f, 1.2f);
    D3DXMatrixRotationQuaternion(&mfromq, &qypr);
    CHECK(Near(mypr, mfromq));

    // Inverse, slerp end points and middle, axis-angle.
    D3DXQUATERNION inv, ident, prod;
    D3DXQuaternionInverse(&inv, &q1);
    D3DXQuaternionMultiply(&prod, &q1, &inv);
    D3DXQuaternionIdentity(&ident);
    CHECK(SameRotation(prod, ident));
    D3DXQUATERNION s0, s1, sh, half;
    D3DXQuaternionSlerp(&s0, &ident, &q1, 0.0f);
    D3DXQuaternionSlerp(&s1, &ident, &q1, 1.0f);
    D3DXQuaternionSlerp(&sh, &ident, &q1, 0.5f);
    D3DXQuaternionRotationAxis(&half, &a1, 0.25f);
    CHECK(SameRotation(s0, ident) && SameRotation(s1, q1) && SameRotation(sh, half));
    D3DXVECTOR3 outAxis;
    float outAngle = 0.0f;
    D3DXQuaternionToAxisAngle(&q1, &outAxis, &outAngle);
    D3DXVec3Normalize(&outAxis, &outAxis);
    CHECK(Near(outAngle, 0.5f) && Near(outAxis, a1));
}

static void Planes() {
    D3DXVECTOR3 p1(1, 0, 0), p2(0, 1, 0), p3(0, 0, 1);
    D3DXPLANE pl;
    D3DXPlaneFromPoints(&pl, &p1, &p2, &p3);
    CHECK(Near(D3DXPlaneDotCoord(&pl, &p1), 0.0f) && Near(D3DXPlaneDotCoord(&pl, &p2), 0.0f) &&
          Near(D3DXPlaneDotCoord(&pl, &p3), 0.0f));
    D3DXVECTOR3 o(0, 0, 0), distant(1, 1, 1), hit;
    CHECK(D3DXPlaneIntersectLine(&hit, &pl, &o, &distant) != nullptr);
    CHECK(Near(hit, D3DXVECTOR3(1.0f / 3, 1.0f / 3, 1.0f / 3)));
    D3DXVECTOR3 par(1, -1, 0);
    CHECK(D3DXPlaneIntersectLine(&hit, &pl, &o, &par) == nullptr);  // parallel to the plane

    // Reflecting twice gives the identity; a point of the plane stays where it is.
    D3DXPLANE ground(0, 1, 0, -2);  // y = 2
    D3DXMATRIX refl, id;
    D3DXMatrixReflect(&refl, &ground);
    D3DXMatrixIdentity(&id);
    CHECK(Near(refl * refl, id));
    CHECK(Near(Point(D3DXVECTOR3(5, 2, 7), refl), D3DXVECTOR3(5, 2, 7)));
    CHECK(Near(Point(D3DXVECTOR3(5, 3, 7), refl), D3DXVECTOR3(5, 1, 7)));

    // Shadow: points of the plane stay, others fall onto it along the light.
    D3DXVECTOR4 sun(0.0f, 1.0f, 0.0f, 0.0f);  // directional, from above
    D3DXMATRIX sh;
    D3DXMatrixShadow(&sh, &sun, &ground);
    CHECK(Near(Point(D3DXVECTOR3(5, 2, 7), sh), D3DXVECTOR3(5, 2, 7)));
    CHECK(Near(Point(D3DXVECTOR3(5, 9, 7), sh), D3DXVECTOR3(5, 2, 7)));
    D3DXVECTOR4 lamp(0.0f, 10.0f, 0.0f, 1.0f);  // a point light at (0, 10, 0)
    D3DXMatrixShadow(&sh, &lamp, &ground);
    CHECK(Near(Point(D3DXVECTOR3(4, 6, 0), sh), D3DXVECTOR3(8, 2, 0)));

    // A plane moved with a translation (its matrix: the inverse transpose).
    D3DXMATRIX t, ti;
    D3DXMatrixTranslation(&t, 0, 3, 0);
    D3DXMatrixInverse(&ti, nullptr, &t);
    D3DXMatrixTranspose(&ti, &ti);
    D3DXPLANE moved;
    D3DXPlaneTransform(&moved, &ground, &ti);
    D3DXVECTOR3 on(1, 5, 1);
    CHECK(Near(D3DXPlaneDotCoord(&moved, &on), 0.0f));
}

static void Vectors() {
    D3DXVECTOR3 a(1, 2, 3), b(4, 5, 6), c;
    D3DXVec3Cross(&c, &a, &b);
    CHECK(Near(c, D3DXVECTOR3(-3, 6, -3)));
    CHECK(Near(D3DXVec3Dot(&a, &b), 32.0f));
    D3DXVec3Normalize(&c, &b);
    CHECK(Near(D3DXVec3Length(&c), 1.0f));
    D3DXVECTOR3 zero(0, 0, 0);
    D3DXVec3Normalize(&c, &zero);
    CHECK(c.x == 0.0f && c.y == 0.0f && c.z == 0.0f);
    D3DXMATRIX t;
    D3DXMatrixTranslation(&t, 10, 0, 0);
    D3DXVec3TransformNormal(&c, &a, &t);
    CHECK(Near(c, a));  // directions ignore translation
    D3DXVECTOR4 h;
    D3DXVec3Transform(&h, &a, &t);
    CHECK(Near(h.x, 11.0f) && Near(h.w, 1.0f));
}

static void Colors() {
    D3DXCOLOR c(0x80FF4000u);
    CHECK(Near(c.a, 128.0f / 255) && Near(c.r, 1.0f) && Near(c.g, 64.0f / 255) && Near(c.b, 0.0f));
    CHECK((DWORD)c == 0x80FF4000u);
    D3DXCOLOR grey, same;
    D3DXColorAdjustSaturation(&grey, &c, 0.0f);
    CHECK(Near(grey.r, grey.g) && Near(grey.g, grey.b) && Near(grey.a, c.a));
    D3DXColorAdjustContrast(&same, &c, 1.0f);
    CHECK(Near(same.r, c.r) && Near(same.g, c.g) && Near(same.b, c.b));
}

int main() {
    Matrices();
    ViewAndProjection();
    Quaternions();
    Planes();
    Vectors();
    Colors();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
