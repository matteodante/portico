// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the D3DX 9 texture functions, over any IDirect3DDevice9 (Portico's WebGL2 device, or the
// system's Direct3D 9 on Windows). Images are DDS (DXT1-5 and uncompressed), TGA, BMP and, when zlib is
// available (PORTICO_D3DX_PNG), PNG; pixels are converted through 32-bit ARGB.
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#ifdef PORTICO_D3DX_PNG
#include <zlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

void Log(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[d3dx] %s\n", buf);
}

// ---- pixel formats -----------------------------------------------------------------------------
// Pixels in flight are 32-bit ARGB (D3DCOLOR, 0xAARRGGBB).
bool IsDxt(D3DFORMAT f) { return f == D3DFMT_DXT1 || f == D3DFMT_DXT2 || f == D3DFMT_DXT3 || f == D3DFMT_DXT4 || f == D3DFMT_DXT5; }

UINT BytesPerPixel(D3DFORMAT f)
{
    switch (f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8: return 4;
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_R5G6B5: case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5:
    case D3DFMT_A8L8: return 2;
    case D3DFMT_A8: case D3DFMT_L8: return 1;
    default: return 0;
    }
}

UINT Expand(UINT v, int bits) { return bits == 8 ? v : (v * 255 + ((1u << bits) - 1) / 2) / ((1u << bits) - 1); }

DWORD Read(D3DFORMAT f, const BYTE *p)
{
    switch (f) {
    case D3DFMT_A8R8G8B8: return *(const DWORD *)p;
    case D3DFMT_X8R8G8B8: return *(const DWORD *)p | 0xFF000000;
    case D3DFMT_A8B8G8R8: { DWORD v = *(const DWORD *)p; return (v & 0xFF00FF00) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF); }
    case D3DFMT_X8B8G8R8: { DWORD v = *(const DWORD *)p; return 0xFF000000 | (v & 0xFF00) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF); }
    case D3DFMT_R8G8B8: return 0xFF000000 | ((DWORD)p[2] << 16) | ((DWORD)p[1] << 8) | p[0];
    case D3DFMT_R5G6B5: { WORD v = *(const WORD *)p; return 0xFF000000 | (Expand(v >> 11, 5) << 16) | (Expand((v >> 5) & 63, 6) << 8) | Expand(v & 31, 5); }
    case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: {
        WORD v = *(const WORD *)p;
        DWORD a = f == D3DFMT_X4R4G4B4 ? 255 : Expand(v >> 12, 4);
        return (a << 24) | (Expand((v >> 8) & 15, 4) << 16) | (Expand((v >> 4) & 15, 4) << 8) | Expand(v & 15, 4);
    }
    case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: {
        WORD v = *(const WORD *)p;
        DWORD a = f == D3DFMT_X1R5G5B5 || (v & 0x8000) ? 255 : 0;
        return (a << 24) | (Expand((v >> 10) & 31, 5) << 16) | (Expand((v >> 5) & 31, 5) << 8) | Expand(v & 31, 5);
    }
    case D3DFMT_A8: return (DWORD)p[0] << 24;
    case D3DFMT_L8: return 0xFF000000 | ((DWORD)p[0] * 0x010101);
    case D3DFMT_A8L8: return ((DWORD)p[1] << 24) | ((DWORD)p[0] * 0x010101);
    default: return 0;
    }
}

void Write(D3DFORMAT f, BYTE *p, DWORD c)
{
    DWORD a = c >> 24, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    switch (f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: *(DWORD *)p = c; break;
    case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8: *(DWORD *)p = (a << 24) | (b << 16) | (g << 8) | r; break;
    case D3DFMT_R8G8B8: p[0] = (BYTE)b; p[1] = (BYTE)g; p[2] = (BYTE)r; break;
    case D3DFMT_R5G6B5: *(WORD *)p = (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); break;
    case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: *(WORD *)p = (WORD)(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)); break;
    case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: *(WORD *)p = (WORD)(((a >= 128) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)); break;
    case D3DFMT_A8: p[0] = (BYTE)a; break;
    case D3DFMT_L8: p[0] = (BYTE)((r * 77 + g * 151 + b * 28) >> 8); break;
    case D3DFMT_A8L8: p[0] = (BYTE)((r * 77 + g * 151 + b * 28) >> 8); p[1] = (BYTE)a; break;
    default: break;
    }
}

// ---- DXT ---------------------------------------------------------------------------------------
DWORD Color565(WORD v) { return 0xFF000000 | (Expand(v >> 11, 5) << 16) | (Expand((v >> 5) & 63, 6) << 8) | Expand(v & 31, 5); }

void DecodeColorBlock(const BYTE *b, DWORD out[16], bool dxt1)
{
    WORD c0 = (WORD)(b[0] | (b[1] << 8)), c1 = (WORD)(b[2] | (b[3] << 8));
    DWORD k[4];
    k[0] = Color565(c0);
    k[1] = Color565(c1);
    auto mix = [](DWORD x, DWORD y, int wx, int wy, int d) {
        DWORD r = 0;
        for (int s = 0; s < 24; s += 8)
            r |= ((((x >> s) & 255) * wx + ((y >> s) & 255) * wy) / d) << s;
        return r | 0xFF000000;
    };
    if (c0 > c1 || !dxt1) {
        k[2] = mix(k[0], k[1], 2, 1, 3);
        k[3] = mix(k[0], k[1], 1, 2, 3);
    } else {
        k[2] = mix(k[0], k[1], 1, 1, 2);
        k[3] = 0x00000000;  // transparent black
    }
    DWORD bits = (DWORD)b[4] | ((DWORD)b[5] << 8) | ((DWORD)b[6] << 16) | ((DWORD)b[7] << 24);
    for (int i = 0; i < 16; i++)
        out[i] = k[(bits >> (2 * i)) & 3];
}

// Decodes a DXT image into ARGB (width x height, rows of `width` pixels).
void DecodeDxt(D3DFORMAT f, const BYTE *src, UINT w, UINT h, DWORD *dst)
{
    bool dxt1 = f == D3DFMT_DXT1;
    UINT blockSize = dxt1 ? 8 : 16;
    for (UINT by = 0; by < (h + 3) / 4; by++)
        for (UINT bx = 0; bx < (w + 3) / 4; bx++, src += blockSize) {
            DWORD px[16];
            const BYTE *color = dxt1 ? src : src + 8;
            DecodeColorBlock(color, px, dxt1);
            if (f == D3DFMT_DXT2 || f == D3DFMT_DXT3) {
                for (int i = 0; i < 16; i++) {
                    DWORD a = (src[i / 2] >> ((i & 1) * 4)) & 15;
                    px[i] = (px[i] & 0x00FFFFFF) | (Expand(a, 4) << 24);
                }
            } else if (f == D3DFMT_DXT4 || f == D3DFMT_DXT5) {
                DWORD a[8];
                a[0] = src[0];
                a[1] = src[1];
                if (a[0] > a[1]) {
                    for (int i = 1; i < 7; i++)
                        a[i + 1] = ((7 - i) * a[0] + i * a[1]) / 7;
                } else {
                    for (int i = 1; i < 5; i++)
                        a[i + 1] = ((5 - i) * a[0] + i * a[1]) / 5;
                    a[6] = 0;
                    a[7] = 255;
                }
                ULONGLONG bits = 0;
                for (int i = 0; i < 6; i++)
                    bits |= (ULONGLONG)src[2 + i] << (8 * i);
                for (int i = 0; i < 16; i++)
                    px[i] = (px[i] & 0x00FFFFFF) | (a[(bits >> (3 * i)) & 7] << 24);
            }
            for (int i = 0; i < 16; i++) {
                UINT x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x < w && y < h)
                    dst[y * w + x] = px[i];
            }
        }
}

UINT DxtLevelSize(D3DFORMAT f, UINT w, UINT h) { return ((w + 3) / 4) * ((h + 3) / 4) * (f == D3DFMT_DXT1 ? 8 : 16); }

// ---- DXT encoding (D3DX converts into the requested block format) ---------------------------------
WORD To565(DWORD c) { return (WORD)(((((c >> 16) & 255) >> 3) << 11) | ((((c >> 8) & 255) >> 2) << 5) | ((c & 255) >> 3)); }

int Dist2(DWORD a, DWORD b)
{
    int d = 0;
    for (int s = 0; s < 24; s += 8) {
        int x = (int)((a >> s) & 255) - (int)((b >> s) & 255);
        d += x * x;
    }
    return d;
}

// Colour block: endpoints are the darkest and brightest opaque pixels (weighted 2:4:1 like the eye);
// with transparent pixels (DXT1 alpha < 128) the block uses the 3-colour mode and index 3.
void EncodeColorBlock(const DWORD px[16], BYTE out[8], bool dxt1)
{
    bool transparent = false;
    int lo = -1, hi = -1, loV = 1 << 30, hiV = -1;
    for (int i = 0; i < 16; i++) {
        if (dxt1 && (px[i] >> 24) < 128) {
            transparent = true;
            continue;
        }
        int v = (int)((px[i] >> 16) & 255) * 2 + (int)((px[i] >> 8) & 255) * 4 + (int)(px[i] & 255);
        if (v < loV) { loV = v; lo = i; }
        if (v > hiV) { hiV = v; hi = i; }
    }
    WORD c0 = lo < 0 ? 0 : To565(px[hi]), c1 = lo < 0 ? 0 : To565(px[lo]);
    bool three = transparent;
    if (three ? c0 > c1 : c0 < c1)
        std::swap(c0, c1);
    if (!three && c0 == c1) {
        if (c0 > 0) c1 = (WORD)(c0 - 1);  // keep the 4-colour mode (c0 > c1) for opaque blocks
        else c0 = 1;
    }
    DWORD k[4];
    k[0] = Color565(c0);
    k[1] = Color565(c1);
    auto mix = [](DWORD x, DWORD y, int wx, int wy, int d) {
        DWORD r = 0;
        for (int s = 0; s < 24; s += 8)
            r |= ((((x >> s) & 255) * wx + ((y >> s) & 255) * wy) / d) << s;
        return r | 0xFF000000;
    };
    int ncol = three ? 3 : 4;
    if (three) {
        k[2] = mix(k[0], k[1], 1, 1, 2);
    } else {
        k[2] = mix(k[0], k[1], 2, 1, 3);
        k[3] = mix(k[0], k[1], 1, 2, 3);
    }
    DWORD bits = 0;
    for (int i = 0; i < 16; i++) {
        int best = 0;
        if (three && (px[i] >> 24) < 128) {
            best = 3;
        } else {
            int bd = 1 << 30;
            for (int j = 0; j < ncol; j++) {
                int d = Dist2(px[i], k[j]);
                if (d < bd) { bd = d; best = j; }
            }
        }
        bits |= (DWORD)best << (2 * i);
    }
    out[0] = (BYTE)c0; out[1] = (BYTE)(c0 >> 8); out[2] = (BYTE)c1; out[3] = (BYTE)(c1 >> 8);
    out[4] = (BYTE)bits; out[5] = (BYTE)(bits >> 8); out[6] = (BYTE)(bits >> 16); out[7] = (BYTE)(bits >> 24);
}

// Encodes an ARGB image (w x h) into DXT blocks, one row of blocks every `pitch` bytes.
void EncodeDxt(D3DFORMAT f, const DWORD *src, UINT w, UINT h, BYTE *dst, INT pitch)
{
    bool dxt1 = f == D3DFMT_DXT1;
    for (UINT by = 0; by < (h + 3) / 4; by++) {
        BYTE *out = dst + (size_t)pitch * by;
        for (UINT bx = 0; bx < (w + 3) / 4; bx++, out += dxt1 ? 8 : 16) {
            DWORD px[16];
            for (int i = 0; i < 16; i++) {
                UINT x = std::min(w - 1, bx * 4 + (i & 3)), y = std::min(h - 1, by * 4 + (i >> 2));
                px[i] = src[(size_t)y * w + x];
            }
            if (dxt1) {
                EncodeColorBlock(px, out, true);
                continue;
            }
            if (f == D3DFMT_DXT2 || f == D3DFMT_DXT3) {
                for (int i = 0; i < 16; i += 2) {
                    DWORD a0 = ((px[i] >> 24) * 15 + 127) / 255, a1 = ((px[i + 1] >> 24) * 15 + 127) / 255;
                    out[i / 2] = (BYTE)(a0 | (a1 << 4));
                }
            } else {
                // DXT5 alpha: try the 8-value mode (endpoints min/max) and the 6-value mode with
                // explicit 0 and 255 (endpoints min/max of the other values); keep the smaller error.
                DWORD amin = 255, amax = 0, imin = 255, imax = 0;
                for (int i = 0; i < 16; i++) {
                    DWORD v = px[i] >> 24;
                    amin = std::min(amin, v);
                    amax = std::max(amax, v);
                    if (v != 0 && v != 255) {
                        imin = std::min(imin, v);
                        imax = std::max(imax, v);
                    }
                }
                if (imin > imax) imin = imax = 0;
                auto build = [](DWORD e0, DWORD e1, DWORD a[8]) {
                    a[0] = e0;
                    a[1] = e1;
                    if (e0 > e1) {
                        for (int i = 1; i < 7; i++) a[i + 1] = ((7 - i) * e0 + i * e1) / 7;
                    } else {
                        for (int i = 1; i < 5; i++) a[i + 1] = ((5 - i) * e0 + i * e1) / 5;
                        a[6] = 0;
                        a[7] = 255;
                    }
                };
                auto fit = [&](const DWORD a[8], ULONGLONG &bits) {
                    DWORD err = 0;
                    bits = 0;
                    for (int i = 0; i < 16; i++) {
                        DWORD v = px[i] >> 24, best = 0, bd = 1u << 30;
                        for (DWORD j = 0; j < 8; j++) {
                            DWORD d = v > a[j] ? v - a[j] : a[j] - v;
                            if (d < bd) { bd = d; best = j; }
                        }
                        err += bd;
                        bits |= (ULONGLONG)best << (3 * i);
                    }
                    return err;
                };
                DWORD a8[8], a6[8];
                ULONGLONG b8, b6;
                build(amax, amax == amin ? (amax ? amax - 1 : 0) : amin, a8);
                if (amax == amin && amax == 0) build(1, 0, a8);
                build(imin, imax, a6);
                DWORD e8 = fit(a8, b8), e6 = fit(a6, b6);
                const DWORD *a = e6 < e8 ? a6 : a8;
                ULONGLONG bits = e6 < e8 ? b6 : b8;
                out[0] = (BYTE)a[0];
                out[1] = (BYTE)a[1];
                for (int i = 0; i < 6; i++)
                    out[2 + i] = (BYTE)(bits >> (8 * i));
            }
            EncodeColorBlock(px, out + 8, false);
        }
    }
}

// ---- image files -------------------------------------------------------------------------------
struct Image {
    UINT width = 0, height = 0, mipLevels = 1;
    D3DFORMAT format = D3DFMT_UNKNOWN;   // format of the stored data
    D3DXIMAGE_FILEFORMAT fileFormat = D3DXIFF_BMP;
    const BYTE *data = nullptr;          // first level, in `format` (DDS), or null when decoded
    UINT dataSize = 0;
    std::vector<DWORD> argb;             // decoded top level (TGA, BMP)
};

DWORD Le32(const BYTE *p) { return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24); }

bool ParseDds(const BYTE *p, UINT size, Image &img, bool decode)
{
    if (size < 128 || memcmp(p, "DDS ", 4) != 0)
        return false;
    img.fileFormat = D3DXIFF_DDS;
    img.height = Le32(p + 12);
    img.width = Le32(p + 16);
    img.mipLevels = std::max<DWORD>(1, (Le32(p + 8) & 0x20000) ? Le32(p + 28) : 1);
    DWORD pfFlags = Le32(p + 80), fourCC = Le32(p + 84), bits = Le32(p + 88);
    DWORD rMask = Le32(p + 92), gMask = Le32(p + 96), bMask = Le32(p + 100), aMask = Le32(p + 104);
    if (pfFlags & 4) {  // DDPF_FOURCC
        switch (fourCC) {
        case MAKEFOURCC('D', 'X', 'T', '1'): img.format = D3DFMT_DXT1; break;
        case MAKEFOURCC('D', 'X', 'T', '2'): img.format = D3DFMT_DXT2; break;
        case MAKEFOURCC('D', 'X', 'T', '3'): img.format = D3DFMT_DXT3; break;
        case MAKEFOURCC('D', 'X', 'T', '4'): img.format = D3DFMT_DXT4; break;
        case MAKEFOURCC('D', 'X', 'T', '5'): img.format = D3DFMT_DXT5; break;
        default: img.format = (D3DFORMAT)fourCC; break;  // e.g. 116 = A32B32G32R32F
        }
    } else if (bits == 32) {
        img.format = rMask == 0xFF0000 ? (aMask ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8) : (aMask ? D3DFMT_A8B8G8R8 : D3DFMT_X8B8G8R8);
    } else if (bits == 24) {
        img.format = D3DFMT_R8G8B8;
    } else if (bits == 16) {
        img.format = gMask == 0x7E0 ? D3DFMT_R5G6B5 : aMask == 0xF000 ? D3DFMT_A4R4G4B4 : aMask == 0x8000 ? D3DFMT_A1R5G5B5
                                                                                                         : D3DFMT_X1R5G5B5;
    } else if (bits == 8) {
        img.format = aMask ? D3DFMT_A8 : D3DFMT_L8;
    }
    (void)bMask;
    img.data = p + 128;
    img.dataSize = size - 128;
    if (decode) {
        img.argb.assign((size_t)img.width * img.height, 0);
        if (IsDxt(img.format)) {
            if (img.dataSize < DxtLevelSize(img.format, img.width, img.height))
                return false;
            DecodeDxt(img.format, img.data, img.width, img.height, img.argb.data());
        } else if (UINT bpp = BytesPerPixel(img.format)) {
            if (img.dataSize < img.width * img.height * bpp)
                return false;
            for (UINT i = 0; i < img.width * img.height; i++)
                img.argb[i] = Read(img.format, img.data + i * bpp);
        } else {
            return false;
        }
    }
    return true;
}

bool ParseTga(const BYTE *p, UINT size, Image &img, bool decode)
{
    if (size < 18)
        return false;
    BYTE idLen = p[0], cmapType = p[1], type = p[2], bpp = p[16], desc = p[17];
    if ((size_t)18 + idLen > size)
        return false;
    if (cmapType != 0 || (type != 2 && type != 10 && type != 3 && type != 11) || (bpp != 24 && bpp != 32 && bpp != 8))
        return false;
    UINT w = p[12] | (p[13] << 8), h = p[14] | (p[15] << 8);
    if (!w || !h)
        return false;
    img.fileFormat = D3DXIFF_TGA;
    img.width = w;
    img.height = h;
    img.format = bpp == 32 ? D3DFMT_A8R8G8B8 : bpp == 24 ? D3DFMT_X8R8G8B8 : D3DFMT_L8;
    if (!decode)
        return true;  // like D3DX, image info reads the header only
    img.argb.assign((size_t)w * h, 0);
    const BYTE *s = p + 18 + idLen, *end = p + size;
    UINT n = bpp / 8, count = w * h;
    auto pixel = [&](const BYTE *q) -> DWORD {
        if (n == 1)
            return 0xFF000000 | (q[0] * 0x010101u);
        return ((n == 4 ? (DWORD)q[3] : 255) << 24) | ((DWORD)q[2] << 16) | ((DWORD)q[1] << 8) | q[0];
    };
    std::vector<DWORD> lin(count);
    if (type == 2 || type == 3) {
        if ((size_t)(end - s) < (size_t)count * n)
            return false;
        for (UINT i = 0; i < count; i++)
            lin[i] = pixel(s + i * n);
    } else {
        // RLE: every packet is checked against the bytes left, and the packets must cover exactly
        // width x height pixels; anything else is D3DXERR_INVALIDDATA.
        UINT i = 0;
        while (i < count) {
            if (s >= end)
                return false;
            BYTE hdr = *s++;
            UINT run = (hdr & 127) + 1;
            if (run > w - i % w)
                return false;  // D3DX rejects packets that cross the end of a scanline
            if (hdr & 128) {
                if ((size_t)(end - s) < n)
                    return false;
                DWORD c = pixel(s);
                s += n;
                for (UINT k = 0; k < run; k++)
                    lin[i++] = c;
            } else {
                if ((size_t)(end - s) < (size_t)run * n)
                    return false;
                for (UINT k = 0; k < run; k++, s += n)
                    lin[i++] = pixel(s);
            }
        }
    }
    bool topDown = (desc & 0x20) != 0;
    for (UINT y = 0; y < h; y++)
        memcpy(&img.argb[(size_t)y * w], &lin[(size_t)(topDown ? y : h - 1 - y) * w], w * 4);
    return true;
}

bool ParseBmp(const BYTE *p, UINT size, Image &img)
{
    if (size < 54 || p[0] != 'B' || p[1] != 'M')
        return false;
    DWORD off = Le32(p + 10);
    LONG w = (LONG)Le32(p + 18), h = (LONG)Le32(p + 22);
    WORD bpp = (WORD)(p[28] | (p[29] << 8));
    if (w <= 0 || h == 0 || (bpp != 24 && bpp != 32) || Le32(p + 30) != 0)
        return false;
    bool topDown = h < 0;
    UINT uh = (UINT)(topDown ? -h : h), uw = (UINT)w, n = bpp / 8, stride = (uw * n + 3) & ~3u;
    if (off + (size_t)stride * uh > size)
        return false;
    img.fileFormat = D3DXIFF_BMP;
    img.width = uw;
    img.height = uh;
    img.format = bpp == 32 ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    img.argb.assign((size_t)uw * uh, 0);
    for (UINT y = 0; y < uh; y++) {
        const BYTE *row = p + off + (size_t)stride * (topDown ? y : uh - 1 - y);
        for (UINT x = 0; x < uw; x++)
            img.argb[(size_t)y * uw + x] = 0xFF000000 | ((DWORD)row[x * n + 2] << 16) | ((DWORD)row[x * n + 1] << 8) | row[x * n];
    }
    return true;
}

// PNG: 8-bit and 16-bit gray, RGB, palette (1-8 bits), gray+alpha and RGBA; no interlacing.
DWORD Be32(const BYTE *p) { return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }

#ifdef PORTICO_D3DX_PNG
bool ParsePng(const BYTE *p, UINT size, Image &img, bool decode)
{
    static const BYTE sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    if (size < 33 || memcmp(p, sig, 8) != 0 || memcmp(p + 12, "IHDR", 4) != 0)
        return false;
    UINT w = Be32(p + 16), h = Be32(p + 20);
    BYTE depth = p[24], color = p[25], interlace = p[28];
    img.fileFormat = D3DXIFF_PNG;
    img.width = w;
    img.height = h;
    img.format = color == 4 || color == 6 ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    // A palette with a tRNS chunk has alpha too (it comes before the image data).
    if (color == 3)
        for (UINT off = 8; off + 12 <= size;) {
            UINT len = Be32(p + off);
            const BYTE *type = p + off + 4;
            if (!memcmp(type, "tRNS", 4))
                img.format = D3DFMT_A8R8G8B8;
            if (!memcmp(type, "tRNS", 4) || !memcmp(type, "IDAT", 4) || off + 12 + (size_t)len > size)
                break;
            off += 12 + len;
        }
    if (!decode)
        return true;
    if (interlace) {
        Log("interlaced PNG not supported");
        return false;
    }
    int channels = color == 0 ? 1 : color == 2 ? 3 : color == 3 ? 1 : color == 4 ? 2 : color == 6 ? 4 : 0;
    if (!channels || !w || !h)
        return false;
    std::vector<BYTE> idat;
    BYTE palette[256][4];
    memset(palette, 255, sizeof(palette));
    for (UINT off = 8; off + 12 <= size;) {
        UINT len = Be32(p + off);
        const BYTE *type = p + off + 4, *data = p + off + 8;
        if (off + 12 + (size_t)len > size)
            return false;
        if (!memcmp(type, "IDAT", 4))
            idat.insert(idat.end(), data, data + len);
        else if (!memcmp(type, "PLTE", 4))
            for (UINT i = 0; i < len / 3 && i < 256; i++) {
                palette[i][0] = data[i * 3];
                palette[i][1] = data[i * 3 + 1];
                palette[i][2] = data[i * 3 + 2];
            }
        else if (!memcmp(type, "tRNS", 4) && color == 3)
            for (UINT i = 0; i < len && i < 256; i++)
                palette[i][3] = data[i];
        else if (!memcmp(type, "IEND", 4))
            break;
        off += 12 + len;
    }
    size_t bitsPerPixel = (size_t)channels * depth, stride = (w * bitsPerPixel + 7) / 8, bpp = std::max<size_t>(1, bitsPerPixel / 8);
    std::vector<BYTE> raw((stride + 1) * h);
    z_stream zs = {};
    zs.next_in = idat.data();
    zs.avail_in = (uInt)idat.size();
    zs.next_out = raw.data();
    zs.avail_out = (uInt)raw.size();
    if (inflateInit(&zs) != Z_OK)
        return false;
    int z = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (z != Z_STREAM_END && zs.avail_out != 0)
        return false;
    // Undo the per-row filters.
    std::vector<BYTE> prev(stride, 0), cur(stride);
    img.argb.assign((size_t)w * h, 0);
    for (UINT y = 0; y < h; y++) {
        const BYTE *row = &raw[y * (stride + 1)];
        BYTE filter = row[0];
        for (size_t i = 0; i < stride; i++) {
            BYTE x = row[1 + i], a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            switch (filter) {
            case 1: x = (BYTE)(x + a); break;
            case 2: x = (BYTE)(x + b); break;
            case 3: x = (BYTE)(x + ((a + b) >> 1)); break;
            case 4: {
                int pa = abs(b - c), pb = abs(a - c), pc = abs(a + b - 2 * c);
                x = (BYTE)(x + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c));
                break;
            }
            default: break;
            }
            cur[i] = x;
        }
        for (UINT xpx = 0; xpx < w; xpx++) {
            auto sample = [&](int ch) -> BYTE {
                if (depth == 8)
                    return cur[xpx * channels + ch];
                if (depth == 16)
                    return cur[(xpx * channels + ch) * 2];
                size_t bit = (size_t)xpx * depth;  // palette or gray below 8 bits
                BYTE v = (BYTE)((cur[bit / 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1));
                return color == 3 ? v : (BYTE)(v * 255 / ((1 << depth) - 1));
            };
            DWORD c;
            switch (color) {
            case 0: c = 0xFF000000 | (sample(0) * 0x010101u); break;
            case 2: c = 0xFF000000 | ((DWORD)sample(0) << 16) | ((DWORD)sample(1) << 8) | sample(2); break;
            case 3: { const BYTE *e = palette[sample(0)]; c = ((DWORD)e[3] << 24) | ((DWORD)e[0] << 16) | ((DWORD)e[1] << 8) | e[2]; break; }
            case 4: c = ((DWORD)sample(1) << 24) | (sample(0) * 0x010101u); break;
            default: c = ((DWORD)sample(3) << 24) | ((DWORD)sample(0) << 16) | ((DWORD)sample(1) << 8) | sample(2); break;
            }
            img.argb[(size_t)y * w + xpx] = c;
        }
        prev.swap(cur);
    }
    return true;
}
#else
bool ParsePng(const BYTE *, UINT, Image &, bool) { return false; }
#endif

bool ParseImage(LPCVOID data, UINT size, Image &img, bool decode)
{
    const BYTE *p = static_cast<const BYTE *>(data);
    if (!p || !size)
        return false;
    if (ParseDds(p, size, img, decode) || ParsePng(p, size, img, decode) || ParseBmp(p, size, img) || ParseTga(p, size, img, decode))
        return true;
    Log("unknown image format (%u bytes, starts %02x %02x %02x %02x)", size, p[0], size > 1 ? p[1] : 0,
        size > 2 ? p[2] : 0, size > 3 ? p[3] : 0);
    return false;
}

void FillInfo(const Image &img, D3DXIMAGE_INFO *info)
{
    if (!info)
        return;
    info->Width = img.width;
    info->Height = img.height;
    info->Depth = 1;
    info->MipLevels = img.mipLevels;
    info->Format = img.format;
    info->ResourceType = D3DRTYPE_TEXTURE;
    info->ImageFileFormat = img.fileFormat;
}

// Copies ARGB pixels (sw x sh) into a rectangle of a locked surface of format f, with point
// sampling when the sizes differ (D3DX_FILTER_POINT; programs mostly use FILTER_NONE with equal
// sizes).
void Blit(const DWORD *src, UINT sw, UINT sh, const RECT &sr, BYTE *dst, INT pitch, D3DFORMAT f, const RECT &dr, D3DCOLOR colorKey)
{
    UINT bpp = BytesPerPixel(f);
    LONG dw = dr.right - dr.left, dh = dr.bottom - dr.top, srw = sr.right - sr.left, srh = sr.bottom - sr.top;
    if (!bpp || dw <= 0 || dh <= 0 || srw <= 0 || srh <= 0)
        return;
    for (LONG y = 0; y < dh; y++) {
        LONG syy = sr.top + (y * srh) / dh;
        if (syy < 0 || (UINT)syy >= sh)
            continue;
        BYTE *row = dst + (size_t)pitch * (dr.top + y) + (size_t)bpp * dr.left;
        for (LONG x = 0; x < dw; x++) {
            LONG sxx = sr.left + (x * srw) / dw;
            if (sxx < 0 || (UINT)sxx >= sw)
                continue;
            DWORD c = src[(size_t)syy * sw + sxx];
            if (colorKey && c == colorKey)
                c = 0;
            Write(f, row + (size_t)bpp * x, c);
        }
    }
}

// Reads a whole surface into ARGB.
bool ReadSurface(IDirect3DSurface9 *s, const RECT *rect, std::vector<DWORD> &argb, UINT &w, UINT &h, RECT &used)
{
    D3DSURFACE_DESC d;
    if (FAILED(s->GetDesc(&d)))
        return false;
    w = d.Width;
    h = d.Height;
    used = rect ? *rect : RECT{ 0, 0, (LONG)w, (LONG)h };
    D3DLOCKED_RECT lr;
    if (FAILED(s->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
        return false;
    argb.assign((size_t)w * h, 0);
    if (IsDxt(d.Format)) {
        // A locked DXT surface is the block array: decode it as a whole.
        std::vector<BYTE> blocks;
        UINT rowBytes = ((w + 3) / 4) * (d.Format == D3DFMT_DXT1 ? 8 : 16);
        for (UINT by = 0; by < (h + 3) / 4; by++)
            blocks.insert(blocks.end(), (const BYTE *)lr.pBits + (size_t)lr.Pitch * by,
                          (const BYTE *)lr.pBits + (size_t)lr.Pitch * by + rowBytes);
        DecodeDxt(d.Format, blocks.data(), w, h, argb.data());
    } else {
        UINT bpp = BytesPerPixel(d.Format);
        if (!bpp) {
            s->UnlockRect();
            Log("ReadSurface: unsupported format %u", (unsigned)d.Format);
            return false;
        }
        for (UINT y = 0; y < h; y++)
            for (UINT x = 0; x < w; x++)
                argb[(size_t)y * w + x] = Read(d.Format, (const BYTE *)lr.pBits + (size_t)lr.Pitch * y + bpp * x);
    }
    s->UnlockRect();
    return true;
}

HRESULT WriteSurface(IDirect3DSurface9 *dst, const RECT *dstRect, const DWORD *src, UINT sw, UINT sh, const RECT &sr, D3DCOLOR colorKey)
{
    D3DSURFACE_DESC d;
    if (FAILED(dst->GetDesc(&d)))
        return D3DERR_INVALIDCALL;
    if (IsDxt(d.Format)) {
        Log("writing decoded pixels into a %u surface is not supported", (unsigned)d.Format);
        return E_NOTIMPL;
    }
    RECT dr = dstRect ? *dstRect : RECT{ 0, 0, (LONG)d.Width, (LONG)d.Height };
    D3DLOCKED_RECT lr;
    if (FAILED(dst->LockRect(&lr, nullptr, 0)))
        return D3DERR_INVALIDCALL;
    Blit(src, sw, sh, sr, (BYTE *)lr.pBits, lr.Pitch, d.Format, dr, colorKey);
    dst->UnlockRect();
    return D3D_OK;
}

UINT FullMipCount(UINT w, UINT h)
{
    UINT n = 1;
    while (w > 1 || h > 1) {
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
        n++;
    }
    return n;
}

// Box-filters an ARGB image to half size.
std::vector<DWORD> Half(const std::vector<DWORD> &src, UINT w, UINT h, UINT &nw, UINT &nh)
{
    nw = std::max(1u, w / 2);
    nh = std::max(1u, h / 2);
    std::vector<DWORD> out((size_t)nw * nh);
    for (UINT y = 0; y < nh; y++)
        for (UINT x = 0; x < nw; x++) {
            DWORD acc[4] = { 0, 0, 0, 0 };
            int n = 0;
            for (UINT dy = 0; dy < 2; dy++)
                for (UINT dx = 0; dx < 2; dx++) {
                    UINT sx = std::min(w - 1, x * 2 + dx), sy = std::min(h - 1, y * 2 + dy);
                    DWORD c = src[(size_t)sy * w + sx];
                    for (int k = 0; k < 4; k++)
                        acc[k] += (c >> (8 * k)) & 255;
                    n++;
                }
            DWORD c = 0;
            for (int k = 0; k < 4; k++)
                c |= (acc[k] / n) << (8 * k);
            out[(size_t)y * nw + x] = c;
        }
    return out;
}

}  // namespace

extern "C" {

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                                 D3DFORMAT Format, D3DPOOL Pool, LPDIRECT3DTEXTURE9 *ppTexture)
{
    if (!pDevice || !ppTexture)
        return D3DERR_INVALIDCALL;
    if (Width == 0 || Width == D3DX_DEFAULT)
        Width = 256;
    if (Height == 0 || Height == D3DX_DEFAULT)
        Height = 256;
    if (Format == D3DFMT_UNKNOWN || Format == (D3DFORMAT)D3DX_DEFAULT)
        Format = D3DFMT_A8R8G8B8;
    if (MipLevels == D3DX_DEFAULT || MipLevels == 0)
        MipLevels = FullMipCount(Width, Height);
    return pDevice->CreateTexture(Width, Height, MipLevels, Usage, Format, Pool, ppTexture, nullptr);
}

HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo)
{
    Image img;
    if (!ParseImage(pSrcData, SrcDataSize, img, false))
        return D3DXERR_INVALIDDATA;
    FillInfo(img, pSrcInfo);
    return D3D_OK;
}

HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width,
                                                   UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                                   DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                                   PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture)
{
    (void)Filter;
    (void)MipFilter;
    (void)pPalette;
    if (!pDevice || !ppTexture)
        return D3DERR_INVALIDCALL;
    *ppTexture = nullptr;
    Image img;
    if (!ParseImage(pSrcData, SrcDataSize, img, false))
        return D3DXERR_INVALIDDATA;
    FillInfo(img, pSrcInfo);
    UINT w = Width == 0 || Width == D3DX_DEFAULT || Width == D3DX_DEFAULT_NONPOW2 ? img.width : Width;
    UINT h = Height == 0 || Height == D3DX_DEFAULT || Height == D3DX_DEFAULT_NONPOW2 ? img.height : Height;
    UINT fileLevels = img.mipLevels;
    UINT levels = MipLevels == D3DX_DEFAULT || MipLevels == 0 ? FullMipCount(w, h)
                  : MipLevels == D3DX_FROM_FILE ? fileLevels : MipLevels;

    // The requested format (D3DX converts into it); unknown or default means the file's format.
    D3DFORMAT fmt = Format == D3DFMT_UNKNOWN || Format == (D3DFORMAT)D3DX_DEFAULT ? img.format : Format;
    if (!IsDxt(fmt) && !BytesPerPixel(fmt))
        fmt = D3DFMT_A8R8G8B8;
    bool sameSize = w == img.width && h == img.height;

    // Compressed DDS data already in the requested format goes in as it is.
    if (IsDxt(img.format) && fmt == img.format && sameSize) {
        levels = std::min(levels, fileLevels);
        LPDIRECT3DTEXTURE9 tex = nullptr;
        HRESULT hr = pDevice->CreateTexture(w, h, levels, Usage, img.format, Pool, &tex, nullptr);
        if (FAILED(hr))
            return hr;
        const BYTE *p = img.data, *end = img.data + img.dataSize;
        UINT lw = w, lh = h;
        for (UINT l = 0; l < levels; l++) {
            UINT bytes = DxtLevelSize(img.format, lw, lh), rowBytes = ((lw + 3) / 4) * (img.format == D3DFMT_DXT1 ? 8 : 16);
            if (p + bytes > end)
                break;
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(tex->LockRect(l, &lr, nullptr, 0))) {
                for (UINT by = 0; by < (lh + 3) / 4; by++)
                    memcpy((BYTE *)lr.pBits + (size_t)lr.Pitch * by, p + (size_t)rowBytes * by, rowBytes);
                tex->UnlockRect(l);
            }
            p += bytes;
            lw = std::max(1u, lw / 2);
            lh = std::max(1u, lh / 2);
        }
        *ppTexture = tex;
        return D3D_OK;
    }

    // Everything else: decode and convert into the requested format, level by level. The levels
    // stored in a DDS of the same size are decoded and converted one by one; only the missing ones
    // are made with a box filter from the previous level.
    Image full;
    if (!ParseImage(pSrcData, SrcDataSize, full, true))
        return D3DXERR_INVALIDDATA;
    LPDIRECT3DTEXTURE9 tex = nullptr;
    HRESULT hr = pDevice->CreateTexture(w, h, levels, Usage, fmt, Pool, &tex, nullptr);
    if (FAILED(hr))
        return hr;
    std::vector<DWORD> cur = full.argb;
    UINT cw = full.width, ch = full.height;
    UINT lw = w, lh = h;
    const BYTE *stored = nullptr, *storedEnd = nullptr;  // next stored level of a DDS
    UINT storedLevels = 1, sbpp = BytesPerPixel(full.format);
    if (full.fileFormat == D3DXIFF_DDS && sameSize && (IsDxt(full.format) || sbpp)) {
        stored = full.data + (IsDxt(full.format) ? DxtLevelSize(full.format, full.width, full.height)
                                                 : full.width * full.height * sbpp);
        storedEnd = full.data + full.dataSize;
        storedLevels = fileLevels;
    }
    for (UINT l = 0; l < levels; l++) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(tex->LockRect(l, &lr, nullptr, 0))) {
            if (IsDxt(fmt)) {
                std::vector<DWORD> scaled;
                const DWORD *src = cur.data();
                if (cw != lw || ch != lh || ColorKey) {
                    scaled.resize((size_t)lw * lh);
                    RECT sr = { 0, 0, (LONG)cw, (LONG)ch }, dr = { 0, 0, (LONG)lw, (LONG)lh };
                    Blit(cur.data(), cw, ch, sr, (BYTE *)scaled.data(), (INT)lw * 4, D3DFMT_A8R8G8B8, dr, ColorKey);
                    src = scaled.data();
                }
                EncodeDxt(fmt, src, lw, lh, (BYTE *)lr.pBits, lr.Pitch);
            } else {
                RECT sr = { 0, 0, (LONG)cw, (LONG)ch }, dr = { 0, 0, (LONG)lw, (LONG)lh };
                Blit(cur.data(), cw, ch, sr, (BYTE *)lr.pBits, lr.Pitch, fmt, dr, ColorKey);
            }
            tex->UnlockRect(l);
        }
        if (l + 1 >= levels)
            break;
        UINT nw = std::max(1u, cw / 2), nh = std::max(1u, ch / 2);
        UINT bytes = IsDxt(full.format) ? DxtLevelSize(full.format, nw, nh) : nw * nh * sbpp;
        if (stored && l + 1 < storedLevels && stored + bytes <= storedEnd) {
            cur.assign((size_t)nw * nh, 0);
            if (IsDxt(full.format))
                DecodeDxt(full.format, stored, nw, nh, cur.data());
            else
                for (UINT i = 0; i < nw * nh; i++)
                    cur[i] = Read(full.format, stored + i * sbpp);
            stored += bytes;
        } else {
            stored = nullptr;
            cur = Half(cur, cw, ch, nw, nh);
        }
        cw = nw;
        ch = nh;
        lw = std::max(1u, lw / 2);
        lh = std::max(1u, lh / 2);
    }
    *ppTexture = tex;
    return D3D_OK;
}

}  // extern "C"

namespace {

// A file as bytes; backslashes count as slashes, so Windows paths work in the browser's file system.
bool ReadWholeFile(std::string path, std::vector<BYTE> &data)
{
    for (char &c : path)
        if (c == '\\')
            c = '/';
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    bool ok = fseek(f, 0, SEEK_END) == 0;
    long size = ok ? ftell(f) : -1;
    ok = size >= 0 && fseek(f, 0, SEEK_SET) == 0;
    if (ok) {
        data.resize((size_t)size);
        ok = fread(data.data(), 1, data.size(), f) == data.size();
    }
    fclose(f);
    return ok;
}

// UTF-8 of a wide path (the file system's encoding outside Windows; ANSI file names stay ASCII in practice).
std::string NarrowPath(LPCWSTR s)
{
    std::string out;
    for (; s && *s; s++) {
        unsigned c = (unsigned)*s;
        if (c < 0x80) {
            out += (char)c;
        } else if (c < 0x800) {
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        } else {
            out += (char)(0xF0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3F));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }
    return out;
}

HRESULT TextureFromFile(LPDIRECT3DDEVICE9 pDevice, const std::string &path, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                 D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                 D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture)
{
    std::vector<BYTE> data;
    if (!ReadWholeFile(path, data)) {
        Log("cannot read %s", path.c_str());
        return D3DXERR_INVALIDDATA;
    }
    return D3DXCreateTextureFromFileInMemoryEx(pDevice, data.data(), (UINT)data.size(), Width, Height, MipLevels, Usage,
                                               Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT ImageInfoFromFile(const std::string &path, D3DXIMAGE_INFO *pSrcInfo)
{
    std::vector<BYTE> data;
    if (!ReadWholeFile(path, data))
        return D3DXERR_INVALIDDATA;
    return D3DXGetImageInfoFromFileInMemory(data.data(), (UINT)data.size(), pSrcInfo);
}

}  // namespace

extern "C" {

HRESULT WINAPI D3DXCreateTextureFromFileInMemory(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                                 LPDIRECT3DTEXTURE9 *ppTexture)
{
    return D3DXCreateTextureFromFileInMemoryEx(pDevice, pSrcData, SrcDataSize, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                                               0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               nullptr, nullptr, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile, UINT Width, UINT Height,
                                            UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                            DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                            PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture)
{
    return TextureFromFile(pDevice, pSrcFile ? pSrcFile : "", Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter,
                    ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileExW(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, UINT Width, UINT Height,
                                            UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                            DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                            PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture)
{
    return TextureFromFile(pDevice, NarrowPath(pSrcFile), Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter,
                    ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile, LPDIRECT3DTEXTURE9 *ppTexture)
{
    return D3DXCreateTextureFromFileExA(pDevice, pSrcFile, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN,
                                        D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, nullptr, nullptr, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileW(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, LPDIRECT3DTEXTURE9 *ppTexture)
{
    return D3DXCreateTextureFromFileExW(pDevice, pSrcFile, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN,
                                        D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, nullptr, nullptr, ppTexture);
}

HRESULT WINAPI D3DXGetImageInfoFromFileA(LPCSTR pSrcFile, D3DXIMAGE_INFO *pSrcInfo)
{
    return ImageInfoFromFile(pSrcFile ? pSrcFile : "", pSrcInfo);
}

HRESULT WINAPI D3DXGetImageInfoFromFileW(LPCWSTR pSrcFile, D3DXIMAGE_INFO *pSrcInfo)
{
    return ImageInfoFromFile(NarrowPath(pSrcFile), pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(LPDIRECT3DSURFACE9 pDestSurface, CONST PALETTEENTRY *, CONST RECT *pDestRect,
                                               LPCVOID pSrcData, UINT SrcDataSize, CONST RECT *pSrcRect, DWORD,
                                               D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo)
{
    if (!pDestSurface)
        return D3DERR_INVALIDCALL;
    Image img;
    if (!ParseImage(pSrcData, SrcDataSize, img, true))
        return D3DXERR_INVALIDDATA;
    FillInfo(img, pSrcInfo);
    RECT sr = pSrcRect ? *pSrcRect : RECT{ 0, 0, (LONG)img.width, (LONG)img.height };
    return WriteSurface(pDestSurface, pDestRect, img.argb.data(), img.width, img.height, sr, ColorKey);
}

HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9 pDestSurface, CONST PALETTEENTRY *, CONST RECT *pDestRect,
                                          LPDIRECT3DSURFACE9 pSrcSurface, CONST PALETTEENTRY *, CONST RECT *pSrcRect, DWORD,
                                          D3DCOLOR ColorKey)
{
    if (!pDestSurface || !pSrcSurface)
        return D3DERR_INVALIDCALL;
    std::vector<DWORD> argb;
    UINT w, h;
    RECT sr;
    if (!ReadSurface(pSrcSurface, pSrcRect, argb, w, h, sr))
        return D3DERR_INVALIDCALL;
    return WriteSurface(pDestSurface, pDestRect, argb.data(), w, h, sr, ColorKey);
}

// Not implemented: a program's screenshots need a file format encoder and, for render targets, a readback.
HRESULT WINAPI D3DXSaveSurfaceToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT, LPDIRECT3DSURFACE9, const PALETTEENTRY *,
                                      const RECT *) {
    Log("D3DXSaveSurfaceToFile %s: not implemented", pDestFile ? pDestFile : "");
    return E_NOTIMPL;
}

HRESULT WINAPI D3DXSaveSurfaceToFileInMemory(LPD3DXBUFFER *ppDestBuf, D3DXIMAGE_FILEFORMAT, LPDIRECT3DSURFACE9,
                                             const PALETTEENTRY *, const RECT *) {
    if (ppDestBuf) *ppDestBuf = nullptr;
    return E_NOTIMPL;
}

}  // extern "C"
