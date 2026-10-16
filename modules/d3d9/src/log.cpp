// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Layer log (PORTICO_D3D9_LOG natively, the browser console in Emscripten) and window size.
#include "common.h"

#include <cstdarg>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#else
#include <share.h>
#endif

namespace d3d9gl {

void Log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifdef __EMSCRIPTEN__
    fprintf(stderr, "[d3d9gl] %s\n", buf);
#else
    OutputDebugStringA("[d3d9gl] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

    static FILE *s_file = nullptr;
    static bool s_tried = false;
    if (!s_tried) {
        s_tried = true;
        char path[MAX_PATH];
        if (GetEnv("PORTICO_D3D9_LOG", path, sizeof(path))) s_file = _fsopen(path, "w", _SH_DENYNO);
    }
    if (s_file) {
        fprintf(s_file, "%s\n", buf);
        fflush(s_file);
    }
#endif
}

void GetWindowSize(HWND hwnd, UINT &w, UINT &h) {
#ifdef __EMSCRIPTEN__
    (void) hwnd;
    int cw = 0, ch = 0;
    emscripten_get_canvas_element_size("#canvas", &cw, &ch);
    w = cw > 0 ? (UINT) cw : 1;
    h = ch > 0 ? (UINT) ch : 1;
#else
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    w = rc.right > rc.left ? (UINT) (rc.right - rc.left) : 1;
    h = rc.bottom > rc.top ? (UINT) (rc.bottom - rc.top) : 1;
#endif
}

}  // namespace d3d9gl
