// D3D9 surface formats in GLES3/WebGL2 terms, and the CPU conversions between the two layouts.
#include "common.h"

namespace d3d9gl {

// D3D9 stores A8R8G8B8 as B,G,R,A bytes and packs 16-bit formats with alpha in the top bits;
// WebGL2 has no BGRA upload and wants alpha in the low bits, so those are converted on upload.
static const FormatInfo g_formats[] = {
    {D3DFMT_A8R8G8B8, 32, false, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true},
    {D3DFMT_X8R8G8B8, 32, false, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true},
    {D3DFMT_A8B8G8R8, 32, false, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true},
    {D3DFMT_X8B8G8R8, 32, false, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true},
    {D3DFMT_R5G6B5, 16, false, false, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, true},
    {D3DFMT_A4R4G4B4, 16, false, false, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, true},
    {D3DFMT_X4R4G4B4, 16, false, false, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, true},
    {D3DFMT_A1R5G5B5, 16, false, false, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, true},
    {D3DFMT_X1R5G5B5, 16, false, false, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, true},
    {D3DFMT_A8, 8, false, false, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, false},
    {D3DFMT_L8, 8, false, false, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, false},
    {D3DFMT_A8L8, 16, false, false, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, false},
    {D3DFMT_DXT1, 4, true, false, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, false},
    {D3DFMT_DXT2, 8, true, false, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, 0, 0, false},
    {D3DFMT_DXT3, 8, true, false, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, 0, 0, false},
    {D3DFMT_DXT4, 8, true, false, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, 0, 0, false},
    {D3DFMT_DXT5, 8, true, false, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, 0, 0, false},
    {D3DFMT_A32B32G32R32F, 128, false, false, GL_RGBA32F, GL_RGBA, GL_FLOAT, false},
    {D3DFMT_A16B16G16R16F, 64, false, false, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false},
    {D3DFMT_R32F, 32, false, false, GL_R32F, GL_RED, GL_FLOAT, false},
    {D3DFMT_D16, 16, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D15S1, 16, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D24S8, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D24X8, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D24X4S4, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D32, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D24FS8, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
    {D3DFMT_D32F_LOCKABLE, 32, false, true, GL_DEPTH24_STENCIL8, 0, 0, true},
};

// Some DDS writers store DXT3 as the non-standard format code 240; it is loaded as DXT3.
const FormatInfo *GetFormatInfo(D3DFORMAT fmt) {
    for (const FormatInfo &f : g_formats)
        if (f.d3d == fmt) return &f;
    return nullptr;
}

UINT GetPitch(D3DFORMAT fmt, UINT width) {
    const FormatInfo *f = GetFormatInfo(fmt);
    if (!f) return width * 4;
    if (f->block) return ((width + 3) / 4) * (f->bpp * 2);  // 16 pixels * bpp / 8
    return width * (f->bpp / 8);
}

UINT GetRows(D3DFORMAT fmt, UINT height) {
    const FormatInfo *f = GetFormatInfo(fmt);
    if (f && f->block) return (height + 3) / 4;
    return height;
}

UINT GetLevelSize(D3DFORMAT fmt, UINT width, UINT height) {
    return GetPitch(fmt, width) * GetRows(fmt, height);
}

// --- DXT decoding, used when the context has no S3TC ------------------------------------

static void Rgb565(uint16_t c, int &r, int &g, int &b) {
    r = ((c >> 11) & 31) * 255 / 31;
    g = ((c >> 5) & 63) * 255 / 63;
    b = (c & 31) * 255 / 31;
}

static void DecodeColorBlock(const uint8_t *blk, uint8_t out[16][4], bool dxt1) {
    uint16_t c0 = blk[0] | (blk[1] << 8), c1 = blk[2] | (blk[3] << 8);
    int col[4][4];
    Rgb565(c0, col[0][0], col[0][1], col[0][2]);
    Rgb565(c1, col[1][0], col[1][1], col[1][2]);
    col[0][3] = col[1][3] = 255;
    if (!dxt1 || c0 > c1) {
        for (int k = 0; k < 3; k++) {
            col[2][k] = (2 * col[0][k] + col[1][k]) / 3;
            col[3][k] = (col[0][k] + 2 * col[1][k]) / 3;
        }
        col[2][3] = col[3][3] = 255;
    } else {
        for (int k = 0; k < 3; k++) col[2][k] = (col[0][k] + col[1][k]) / 2;
        col[2][3] = 255;
        col[3][0] = col[3][1] = col[3][2] = col[3][3] = 0;
    }
    uint32_t bits = blk[4] | (blk[5] << 8) | (blk[6] << 16) | ((uint32_t) blk[7] << 24);
    for (int i = 0; i < 16; i++) {
        int idx = (bits >> (2 * i)) & 3;
        for (int k = 0; k < 4; k++) out[i][k] = (uint8_t) col[idx][k];
    }
}

static void DecodeDXT(D3DFORMAT fmt, const uint8_t *src, UINT width, UINT height, std::vector<uint8_t> &out) {
    out.assign(size_t(width) * height * 4, 0);
    UINT bw = (width + 3) / 4, bh = (height + 3) / 4;
    bool dxt1 = fmt == D3DFMT_DXT1;
    bool dxt3 = fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3;
    UINT blockSize = dxt1 ? 8 : 16;
    for (UINT by = 0; by < bh; by++)
        for (UINT bx = 0; bx < bw; bx++) {
            const uint8_t *blk = src + (by * bw + bx) * blockSize;
            uint8_t px[16][4];
            DecodeColorBlock(dxt1 ? blk : blk + 8, px, dxt1);
            if (dxt3) {
                for (int i = 0; i < 16; i++) {
                    int a = (blk[i / 2] >> ((i & 1) * 4)) & 15;
                    px[i][3] = (uint8_t) (a * 17);
                }
            } else if (!dxt1) {
                int a[8];
                a[0] = blk[0];
                a[1] = blk[1];
                if (a[0] > a[1])
                    for (int k = 1; k < 7; k++) a[k + 1] = ((7 - k) * a[0] + k * a[1]) / 7;
                else {
                    for (int k = 1; k < 5; k++) a[k + 1] = ((5 - k) * a[0] + k * a[1]) / 5;
                    a[6] = 0;
                    a[7] = 255;
                }
                uint64_t bits = 0;
                for (int k = 0; k < 6; k++) bits |= uint64_t(blk[2 + k]) << (8 * k);
                for (int i = 0; i < 16; i++) px[i][3] = (uint8_t) a[(bits >> (3 * i)) & 7];
            }
            for (int i = 0; i < 16; i++) {
                UINT x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x < width && y < height) memcpy(&out[(size_t(y) * width + x) * 4], px[i], 4);
            }
        }
}

bool ConvertForUpload(D3DFORMAT fmt, const uint8_t *src, UINT width, UINT height, std::vector<uint8_t> &out) {
    size_t n = size_t(width) * height;
    switch (fmt) {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8: {
        out.resize(n * 4);
        bool x = fmt == D3DFMT_X8R8G8B8;
        for (size_t i = 0; i < n; i++) {
            out[i * 4 + 0] = src[i * 4 + 2];
            out[i * 4 + 1] = src[i * 4 + 1];
            out[i * 4 + 2] = src[i * 4 + 0];
            out[i * 4 + 3] = x ? 255 : src[i * 4 + 3];
        }
        return true;
    }
    case D3DFMT_X8B8G8R8: {
        out.assign(src, src + n * 4);
        for (size_t i = 0; i < n; i++) out[i * 4 + 3] = 255;
        return true;
    }
    case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: {
        out.resize(n * 2);
        const uint16_t *s = (const uint16_t *) src;
        uint16_t *d = (uint16_t *) out.data();
        bool x = fmt == D3DFMT_X4R4G4B4;
        for (size_t i = 0; i < n; i++) {
            uint16_t v = s[i];
            uint16_t a = x ? 15 : (v >> 12);
            d[i] = (uint16_t) ((v << 4) | a);
        }
        return true;
    }
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5: {
        out.resize(n * 2);
        const uint16_t *s = (const uint16_t *) src;
        uint16_t *d = (uint16_t *) out.data();
        bool x = fmt == D3DFMT_X1R5G5B5;
        for (size_t i = 0; i < n; i++) {
            uint16_t v = s[i];
            uint16_t a = x ? 1 : (v >> 15);
            d[i] = (uint16_t) (((v & 0x7FFF) << 1) | a);
        }
        return true;
    }
    case D3DFMT_DXT1:
    case D3DFMT_DXT2:
    case D3DFMT_DXT3:
    case D3DFMT_DXT4:
    case D3DFMT_DXT5:
        if (GetGLCaps().s3tc) return false;
        DecodeDXT(fmt, src, width, height, out);
        return true;
    default:
        return false;
    }
}

void ConvertFromRGBA8(D3DFORMAT fmt, const uint8_t *rgba, UINT width, UINT height, uint8_t *dst, UINT dstPitch) {
    for (UINT y = 0; y < height; y++) {
        const uint8_t *s = rgba + size_t(y) * width * 4;
        uint8_t *d = dst + size_t(y) * dstPitch;
        for (UINT x = 0; x < width; x++, s += 4) {
            switch (fmt) {
            case D3DFMT_A8R8G8B8:
            case D3DFMT_X8R8G8B8:
                d[x * 4 + 0] = s[2];
                d[x * 4 + 1] = s[1];
                d[x * 4 + 2] = s[0];
                d[x * 4 + 3] = fmt == D3DFMT_X8R8G8B8 ? 255 : s[3];
                break;
            case D3DFMT_A8B8G8R8:
            case D3DFMT_X8B8G8R8:
                memcpy(d + x * 4, s, 4);
                break;
            case D3DFMT_R5G6B5:
                ((uint16_t *) d)[x] = (uint16_t) (((s[0] >> 3) << 11) | ((s[1] >> 2) << 5) | (s[2] >> 3));
                break;
            default:
                break;
            }
        }
    }
}

}  // namespace d3d9gl
