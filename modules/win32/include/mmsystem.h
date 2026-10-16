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
