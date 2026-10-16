// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Portico win32: GUID, IID and DEFINE_GUID (with INITGUID defined, DEFINE_GUID defines the GUID; otherwise it
// declares it).
#ifndef PORTICO_GUIDDEF_H
#define PORTICO_GUIDDEF_H

#include <string.h>

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
    unsigned int Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif

typedef GUID IID, CLSID, *LPGUID, *LPIID, *LPCLSID;

#ifdef __cplusplus
#define REFGUID const GUID &
#define REFIID const IID &
#define REFCLSID const IID &
#else
#define REFGUID const GUID *
#define REFIID const IID *
#define REFCLSID const IID *
#endif

#ifdef __cplusplus
#define PORTICO_GUID_EXTERN extern "C"
#else
#define PORTICO_GUID_EXTERN extern
#endif

#undef DEFINE_GUID
#ifdef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    PORTICO_GUID_EXTERN const GUID name __attribute__((weak)) = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) PORTICO_GUID_EXTERN const GUID name
#endif

#ifdef __cplusplus
inline bool IsEqualGUID(REFGUID a, REFGUID b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator==(REFGUID a, REFGUID b) { return IsEqualGUID(a, b); }
inline bool operator!=(REFGUID a, REFGUID b) { return !IsEqualGUID(a, b); }
#else
#define IsEqualGUID(a, b) (!memcmp((a), (b), sizeof(GUID)))
#endif
#define IsEqualIID(a, b) IsEqualGUID(a, b)
#define IsEqualCLSID(a, b) IsEqualGUID(a, b)

#endif  // PORTICO_GUIDDEF_H
