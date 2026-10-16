// Forced include for the screenshot tests (PORTICO_SAMPLES_FIXED_TIME): timeGetTime() returns a constant, so a
// sample draws the same frame every time, on Portico and on the system's Direct3D 9, with its source unchanged.
#pragma once
#include <windows.h>
#include <mmsystem.h>
#ifdef PORTICO_FIXED_TIME
#define timeGetTime() ((DWORD)(PORTICO_FIXED_TIME))
#endif
