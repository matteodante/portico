// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// IDirect3D9 of the GL layer and the entry points of portico/d3d9.h.
#include "d3d9_impl.h"
#include "device.h"

#include <portico/d3d9.h>

#include <algorithm>

namespace d3d9gl {

HRESULT Direct3D::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
        AddRef();
        *ppv = static_cast<IDirect3D9 *>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

HRESULT Direct3D::GetAdapterIdentifier(UINT Adapter, DWORD, D3DADAPTER_IDENTIFIER9 *p) {
    if (Adapter || !p) return D3DERR_INVALIDCALL;
    memset(p, 0, sizeof(*p));
    snprintf(p->Driver, sizeof(p->Driver), "d3d9gl");
    snprintf(p->Description, sizeof(p->Description), "D3D9 on OpenGL ES 3 (ANGLE / WebGL2)");
    snprintf(p->DeviceName, sizeof(p->DeviceName), "DISPLAY1");
    p->WHQLLevel = 1;
    return D3D_OK;
}

void Direct3D::LoadModes() {
    if (!m_modes.empty()) return;
#ifndef __EMSCRIPTEN__
    DEVMODEA dm = {};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); i++) {
        if (dm.dmBitsPerPel != 32) continue;
        D3DDISPLAYMODE m = {dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency, D3DFMT_X8R8G8B8};
        bool dup = false;
        for (const D3DDISPLAYMODE &e : m_modes)
            if (e.Width == m.Width && e.Height == m.Height && e.RefreshRate == m.RefreshRate) dup = true;
        if (!dup) m_modes.push_back(m);
    }
#endif
    if (m_modes.empty()) {
        D3DDISPLAYMODE cur;
        GetAdapterDisplayMode(0, &cur);
        m_modes.push_back(cur);
    }
}

UINT Direct3D::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) {
    if (Adapter || Format != D3DFMT_X8R8G8B8) return 0;
    LoadModes();
    return (UINT) m_modes.size();
}

HRESULT Direct3D::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE *pMode) {
    if (Adapter || !pMode || Format != D3DFMT_X8R8G8B8) return D3DERR_INVALIDCALL;
    LoadModes();
    if (Mode >= m_modes.size()) return D3DERR_INVALIDCALL;
    *pMode = m_modes[Mode];
    return D3D_OK;
}

HRESULT Direct3D::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) {
    if (Adapter || !pMode) return D3DERR_INVALIDCALL;
#ifdef __EMSCRIPTEN__
    // The "display" is the canvas.
    UINT w = 0, h = 0;
    GetWindowSize(nullptr, w, h);
    pMode->Width = w;
    pMode->Height = h;
    pMode->RefreshRate = 60;
    pMode->Format = D3DFMT_X8R8G8B8;
    return D3D_OK;
#else
    DEVMODEA dm = {};
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
        dm.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
        dm.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
        dm.dmDisplayFrequency = 60;
    }
    pMode->Width = dm.dmPelsWidth;
    pMode->Height = dm.dmPelsHeight;
    pMode->RefreshRate = dm.dmDisplayFrequency;
    pMode->Format = D3DFMT_X8R8G8B8;
    return D3D_OK;
#endif
}

HRESULT Direct3D::CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) {
    const FormatInfo *f = GetFormatInfo(CheckFormat);
    if (!f) return D3DERR_NOTAVAILABLE;
    if (Usage & (D3DUSAGE_QUERY_VERTEXTEXTURE | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE)) return D3DERR_NOTAVAILABLE;
    if (Usage & D3DUSAGE_DEPTHSTENCIL) return f->depth ? D3D_OK : D3DERR_NOTAVAILABLE;
    if (f->depth) return D3DERR_NOTAVAILABLE;
    if ((Usage & D3DUSAGE_RENDERTARGET) && !f->renderable) return D3DERR_NOTAVAILABLE;
    if (RType == D3DRTYPE_CUBETEXTURE || RType == D3DRTYPE_VOLUMETEXTURE) return D3DERR_NOTAVAILABLE;
    return D3D_OK;
}

HRESULT Direct3D::CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels) {
    if (pQualityLevels) *pQualityLevels = 1;
    return MultiSampleType == D3DMULTISAMPLE_NONE ? D3D_OK : D3DERR_NOTAVAILABLE;
}

// Fixed-function caps. VS/PS 2.0 keep programs that need shader model 3.0 on their fixed-function
// path. Values follow what the D3D9 runtime reports on a D3D10-class GPU. They are constants: like
// D3D9, the answer is the same before the device exists (programs often read the caps then) and
// after CreateDevice or Reset. WebGL2 contexts with
// EXT_texture_filter_anisotropic offer 16x and textures of 8192 and more on every target GPU;
// Device::Init logs a context that offers less, and the samplers clamp to what it has.
HRESULT Direct3D::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *c) {
    if (Adapter || !c) return D3DERR_INVALIDCALL;
    memset(c, 0, sizeof(*c));
    c->DeviceType = DeviceType;
    c->AdapterOrdinal = 0;
    c->Caps2 = D3DCAPS2_CANAUTOGENMIPMAP | D3DCAPS2_DYNAMICTEXTURES;
    c->Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD | D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM;
    c->PresentationIntervals = D3DPRESENT_INTERVAL_IMMEDIATE | D3DPRESENT_INTERVAL_ONE;
    c->DevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
                 D3DDEVCAPS_TLVERTEXVIDEOMEMORY | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY |
                 D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_CANRENDERAFTERFLIP | D3DDEVCAPS_TEXTURENONLOCALVIDMEM |
                 D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX | D3DDEVCAPS_HWTRANSFORMANDLIGHT |
                 D3DDEVCAPS_CANBLTSYSTONONLOCAL | D3DDEVCAPS_HWRASTERIZATION;
    c->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW |
                           D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_BLENDOP | D3DPMISCCAPS_TSSARGTEMP |
                           D3DPMISCCAPS_PERSTAGECONSTANT | D3DPMISCCAPS_SEPARATEALPHABLEND;
    c->RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE |
                    D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZFOG | D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_FOGRANGE |
                    D3DPRASTERCAPS_DEPTHBIAS | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS | D3DPRASTERCAPS_ANISOTROPY;
    c->ZCmpCaps = c->AlphaCmpCaps = 0xFF;
    c->SrcBlendCaps = c->DestBlendCaps = 0x3FFF;
    c->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB | D3DPSHADECAPS_ALPHAGOURAUDBLEND |
                   D3DPSHADECAPS_FOGGOURAUD;
    c->TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_PROJECTED |
                     D3DPTEXTURECAPS_MIPMAP;
    c->TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MIPFPOINT |
                           D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR |
                           D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC;
    c->StretchRectFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFPOINT |
                               D3DPTFILTERCAPS_MAGFLINEAR;
    c->TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR | D3DPTADDRESSCAPS_CLAMP |
                            D3DPTADDRESSCAPS_BORDER | D3DPTADDRESSCAPS_INDEPENDENTUV | D3DPTADDRESSCAPS_MIRRORONCE;
    c->LineCaps = D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST | D3DLINECAPS_BLEND | D3DLINECAPS_ALPHACMP | D3DLINECAPS_FOG;
    c->MaxTextureWidth = c->MaxTextureHeight = kCapsMaxTextureSize;
    c->MaxTextureRepeat = 8192;
    c->MaxTextureAspectRatio = 8192;
    c->MaxAnisotropy = kCapsMaxAnisotropy;
    c->MaxVertexW = 1e10f;
    c->GuardBandLeft = c->GuardBandTop = -1e8f;
    c->GuardBandRight = c->GuardBandBottom = 1e8f;
    // Every stencil operation; not D3DSTENCILCAPS_TWOSIDED (the CCW states are not applied yet).
    c->StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_ZERO | D3DSTENCILCAPS_REPLACE | D3DSTENCILCAPS_INCRSAT |
                     D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT | D3DSTENCILCAPS_INCR | D3DSTENCILCAPS_DECR;
    c->FVFCaps = 8 | D3DFVFCAPS_PSIZE;
    c->TextureOpCaps = 0x03FFFFFF;
    c->MaxTextureBlendStages = 8;
    c->MaxSimultaneousTextures = 8;
    c->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 | D3DVTXPCAPS_DIRECTIONALLIGHTS |
                              D3DVTXPCAPS_POSITIONALLIGHTS | D3DVTXPCAPS_LOCALVIEWER | D3DVTXPCAPS_TEXGEN_SPHEREMAP;
    c->MaxActiveLights = 8;
    c->MaxUserClipPlanes = 0;
    c->MaxVertexBlendMatrices = 4;
    c->MaxVertexBlendMatrixIndex = 255;
    c->MaxPointSize = 64.0f;
    c->MaxPrimitiveCount = 0xFFFFFF;
    c->MaxVertexIndex = 0xFFFFFF;
    c->MaxStreams = 16;
    c->MaxStreamStride = 255;
    // Fixed function only: no shader model, so programs with a fixed-function path choose it (CreateVertexShader and
    // CreatePixelShader fail).
    c->VertexShaderVersion = D3DVS_VERSION(0, 0);
    c->PixelShaderVersion = D3DPS_VERSION(0, 0);
    c->DevCaps2 = D3DDEVCAPS2_STREAMOFFSET;
    c->MasterAdapterOrdinal = 0;
    c->AdapterOrdinalInGroup = 0;
    c->NumberOfAdaptersInGroup = 1;
    c->DeclTypes = D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N | D3DDTCAPS_SHORT4N |
                   D3DDTCAPS_USHORT2N | D3DDTCAPS_USHORT4N | D3DDTCAPS_FLOAT16_2 | D3DDTCAPS_FLOAT16_4;
    c->NumSimultaneousRTs = 1;
    return D3D_OK;
}

HMONITOR Direct3D::GetAdapterMonitor(UINT) {
#ifdef __EMSCRIPTEN__
    return nullptr;
#else
    POINT p = {0, 0};
    return MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY);
#endif
}

HRESULT Direct3D::CreateDevice(UINT Adapter, D3DDEVTYPE, HWND hFocusWindow, DWORD BehaviorFlags,
                               D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **ppDevice) {
    if (Adapter || !pp || !ppDevice) return D3DERR_INVALIDCALL;
    HWND hwnd = pp->hDeviceWindow ? pp->hDeviceWindow : hFocusWindow;
    if (g_device) {
        // One device per process (see Device::Destroy): a second CreateDevice resets it and keeps its first window.
        Log("CreateDevice: a device already exists, reusing it");
        HRESULT hr = g_device->Reset(pp);
        if (FAILED(hr)) {
            *ppDevice = nullptr;
            return hr;
        }
        g_device->AddRef();
        *ppDevice = g_device;
        return D3D_OK;
    }
    Log("CreateDevice hwnd %p %ux%u fmt %u windowed %d", hwnd, pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat, pp->Windowed);
    Device *dev = new Device(this, hwnd, BehaviorFlags, *pp);
    if (!dev->Init()) {
        Log("CreateDevice failed");
        *ppDevice = nullptr;
        return D3DERR_NOTAVAILABLE;
    }
    *ppDevice = dev;
    return D3D_OK;
}

}  // namespace d3d9gl

extern "C" IDirect3D9 *WINAPI PorticoCreateDirect3D9(UINT) {
#ifndef __EMSCRIPTEN__
    d3d9gl::InstallCrashLog();
#endif
    return new d3d9gl::Direct3D();
}

// Direct3DCreate9 itself, so programs link against Portico instead of d3d9.lib: always in the browser, and natively
// unless PORTICO_D3D9_NO_DIRECT3DCREATE9 is defined (a program that wants both renderers calls PorticoCreateDirect3D9
// and links d3d9.lib for the system's Direct3D9).
#if defined(__EMSCRIPTEN__) || !defined(PORTICO_D3D9_NO_DIRECT3DCREATE9)
extern "C" IDirect3D9 *WINAPI Direct3DCreate9(UINT nSDKVersion) { return PorticoCreateDirect3D9(nSDKVersion); }
#endif

extern "C" BOOL WINAPI Portico_D3D9_IsActive(void) { return d3d9gl::g_device != nullptr; }

extern "C" void WINAPI Portico_D3D9_Service(void) {
    if (d3d9gl::g_device) d3d9gl::g_device->Service();
}
