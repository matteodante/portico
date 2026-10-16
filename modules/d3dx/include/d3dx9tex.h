// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3DX: the D3DX 9 texture functions Portico implements (loading DDS, TGA, BMP and PNG into any
// IDirect3DDevice9), with the declarations of the public D3DX documentation.
#ifndef PORTICO_D3DX9TEX_H
#define PORTICO_D3DX9TEX_H

#include <d3d9.h>
#include <d3dx9core.h>

#define D3DX_DEFAULT ((UINT)-1)
#define D3DX_DEFAULT_NONPOW2 ((UINT)-2)
#define D3DX_DEFAULT_FLOAT FLT_MAX
#define D3DX_FROM_FILE ((UINT)-3)
#define D3DFMT_FROM_FILE ((D3DFORMAT)-3)

#define D3DX_FILTER_NONE (1 << 0)
#define D3DX_FILTER_POINT (2 << 0)
#define D3DX_FILTER_LINEAR (3 << 0)
#define D3DX_FILTER_TRIANGLE (4 << 0)
#define D3DX_FILTER_BOX (5 << 0)
#define D3DX_FILTER_MIRROR_U (1 << 16)
#define D3DX_FILTER_MIRROR_V (2 << 16)
#define D3DX_FILTER_MIRROR_W (4 << 16)
#define D3DX_FILTER_MIRROR (7 << 16)
#define D3DX_FILTER_DITHER (1 << 19)
#define D3DX_FILTER_SRGB_IN (1 << 21)
#define D3DX_FILTER_SRGB_OUT (2 << 21)
#define D3DX_FILTER_SRGB (3 << 21)

#define _FACDD 0x876
#define MAKE_DDHRESULT(code) MAKE_HRESULT(1, _FACDD, code)
#define D3DXERR_CANNOTMODIFYINDEXBUFFER MAKE_DDHRESULT(2900)
#define D3DXERR_INVALIDMESH MAKE_DDHRESULT(2901)
#define D3DXERR_CANNOTATTRSORT MAKE_DDHRESULT(2902)
#define D3DXERR_SKINNINGNOTSUPPORTED MAKE_DDHRESULT(2903)
#define D3DXERR_TOOMANYINFLUENCES MAKE_DDHRESULT(2904)
#define D3DXERR_INVALIDDATA MAKE_DDHRESULT(2905)
#define D3DXERR_LOADEDMESHASNODATA MAKE_DDHRESULT(2906)
#define D3DXERR_DUPLICATENAMEDFRAGMENT MAKE_DDHRESULT(2907)
#define D3DXERR_CANNOTREMOVELASTITEM MAKE_DDHRESULT(2908)

typedef enum _D3DXIMAGE_FILEFORMAT {
    D3DXIFF_BMP = 0,
    D3DXIFF_JPG = 1,
    D3DXIFF_TGA = 2,
    D3DXIFF_PNG = 3,
    D3DXIFF_DDS = 4,
    D3DXIFF_PPM = 5,
    D3DXIFF_DIB = 6,
    D3DXIFF_HDR = 7,
    D3DXIFF_PFM = 8,
    D3DXIFF_FORCE_DWORD = 0x7fffffff
} D3DXIMAGE_FILEFORMAT;

typedef struct _D3DXIMAGE_INFO {
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    D3DFORMAT Format;
    D3DRESOURCETYPE ResourceType;
    D3DXIMAGE_FILEFORMAT ImageFileFormat;
} D3DXIMAGE_INFO;

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXGetImageInfoFromFileA(LPCSTR pSrcFile, D3DXIMAGE_INFO *pSrcInfo);
HRESULT WINAPI D3DXGetImageInfoFromFileW(LPCWSTR pSrcFile, D3DXIMAGE_INFO *pSrcInfo);
HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo);

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                                 D3DFORMAT Format, D3DPOOL Pool, LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile, LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileW(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile, UINT Width, UINT Height,
                                            UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                            DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                            PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileExW(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, UINT Width, UINT Height,
                                            UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                            DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                            PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileInMemory(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                                 LPDIRECT3DTEXTURE9 *ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                                   UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                                                   D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter,
                                                   D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette,
                                                   LPDIRECT3DTEXTURE9 *ppTexture);

HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(LPDIRECT3DSURFACE9 pDestSurface, const PALETTEENTRY *pDestPalette,
                                               const RECT *pDestRect, LPCVOID pSrcData, UINT SrcDataSize,
                                               const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey,
                                               D3DXIMAGE_INFO *pSrcInfo);
// Not implemented (screenshots to files): they fail with E_NOTIMPL and give back no buffer.
HRESULT WINAPI D3DXSaveSurfaceToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, LPDIRECT3DSURFACE9 pSrcSurface,
                                      const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect);
HRESULT WINAPI D3DXSaveSurfaceToFileInMemory(LPD3DXBUFFER *ppDestBuf, D3DXIMAGE_FILEFORMAT DestFormat,
                                             LPDIRECT3DSURFACE9 pSrcSurface, const PALETTEENTRY *pSrcPalette,
                                             const RECT *pSrcRect);

HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9 pDestSurface, const PALETTEENTRY *pDestPalette,
                                          const RECT *pDestRect, LPDIRECT3DSURFACE9 pSrcSurface,
                                          const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                          D3DCOLOR ColorKey);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define D3DXGetImageInfoFromFile D3DXGetImageInfoFromFileW
#define D3DXCreateTextureFromFile D3DXCreateTextureFromFileW
#define D3DXCreateTextureFromFileEx D3DXCreateTextureFromFileExW
#else
#define D3DXGetImageInfoFromFile D3DXGetImageInfoFromFileA
#define D3DXCreateTextureFromFile D3DXCreateTextureFromFileA
#define D3DXCreateTextureFromFileEx D3DXCreateTextureFromFileExA
#endif

#endif  // PORTICO_D3DX9TEX_H
