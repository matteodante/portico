// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico win32: the multimedia timer.
#ifndef PORTICO_MMSYSTEM_H
#define PORTICO_MMSYSTEM_H
#include <windows.h>
PORTICO_BEGIN_C
DWORD WINAPI timeGetTime(void);
UINT WINAPI timeBeginPeriod(UINT uPeriod);
UINT WINAPI timeEndPeriod(UINT uPeriod);
PORTICO_END_C
#endif
