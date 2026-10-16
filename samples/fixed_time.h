// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Forced include for the screenshot tests (PORTICO_SAMPLES_FIXED_TIME): timeGetTime() returns a constant, so a
// sample draws the same frame every time, on Portico and on the system's Direct3D 9, with its source unchanged.
#pragma once
#include <windows.h>
#include <mmsystem.h>
#ifdef PORTICO_FIXED_TIME
#define timeGetTime() ((DWORD)(PORTICO_FIXED_TIME))
#endif
