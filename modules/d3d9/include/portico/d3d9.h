// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico D3D9: Direct3D 9 (fixed-function pipeline) on OpenGL ES 3 — WebGL2 in the browser, ANGLE natively.
//
// Programs keep calling the Direct3D 9 API through <d3d9.h>; linking portico::d3d9 makes Direct3DCreate9 return
// Portico's IDirect3D9. This header adds Portico's own entry points.
#ifndef PORTICO_D3D9_H
#define PORTICO_D3D9_H

#include <d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

// Portico's IDirect3D9, always (natively, Direct3DCreate9 can stay the system's: build Portico with
// PORTICO_D3D9_NO_DIRECT3DCREATE9 and link d3d9.lib too).
IDirect3D9 *WINAPI PorticoCreateDirect3D9(UINT nSDKVersion);

// Runs the Direct3D 9 calls that other threads queued (a loading screen drawn by a loader thread, with
// D3DCREATE_MULTITHREADED) and presents them. Call it on the device's thread wherever that thread waits without
// rendering. No-op on other threads and with the system's Direct3D 9.
void WINAPI Portico_D3D9_Service(void);

// True once a Portico device exists: its thread's waits should then call Portico_D3D9_Service.
BOOL WINAPI Portico_D3D9_IsActive(void);

#ifdef __EMSCRIPTEN__
// Called after every Present on the device's thread (browser builds). Portico's default gives the thread back to the
// browser until the next animation frame (JSPI); a platform layer with its own frame pacing can define it instead.
void portico_present_frame(void);
#endif

#ifdef __cplusplus
}
#endif

#endif  // PORTICO_D3D9_H
