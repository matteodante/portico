// Development aid: with PORTICO_D3D9_LOG set, faults (access violations and the like, not C++
// exceptions) are written to the layer log with a symbolized stack before the program's own crash
// dialog appears. Native Windows only.
#include "common.h"

#ifdef __EMSCRIPTEN__
namespace d3d9gl {
void InstallCrashLog() {}
}  // namespace d3d9gl
#else

#include <dbghelp.h>
#include <tlhelp32.h>

#pragma comment(lib, "dbghelp.lib")

namespace d3d9gl {

static LONG CALLBACK OnFault(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Informational codes and RPC status codes (e.g. 0x6A6 from CloseServiceHandle, handled by RPC) are not faults.
    if (code < 0xC0000000 && code != 0xE06D7363 && code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    // C++ exceptions (0xE06D7363) are usually caught by the program; log the first few with their
    // stack, faults once.
    static LONG s_cpp = 0, s_fault = 0;
    if (code == 0xE06D7363) {
        if (InterlockedIncrement(&s_cpp) > 5) return EXCEPTION_CONTINUE_SEARCH;
    } else if (InterlockedExchange(&s_fault, 1)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, nullptr, TRUE);
    Log("%s 0x%08lx at %p thread %lu", code == 0xE06D7363 ? "C++ EXCEPTION" : "FAULT", code,
        ep->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION)
        Log("  %s address %p", ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
            (void *) ep->ExceptionRecord->ExceptionInformation[1]);

    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 f = {};
    f.AddrPC.Offset = ctx.Eip;
    f.AddrFrame.Offset = ctx.Ebp;
    f.AddrStack.Offset = ctx.Esp;
    f.AddrPC.Mode = f.AddrFrame.Mode = f.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 32; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_I386, proc, GetCurrentThread(), &f, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) || !f.AddrPC.Offset)
            break;
        char buf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO *sym = (SYMBOL_INFO *) buf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line = {sizeof(line)};
        DWORD ldisp = 0;
        const char *name = SymFromAddr(proc, f.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
        if (SymGetLineFromAddr64(proc, f.AddrPC.Offset, &ldisp, &line))
            Log("  #%d %s+0x%llx %s:%lu", i, name, disp, line.FileName, line.LineNumber);
        else
            Log("  #%d %s+0x%llx (%p)", i, name, disp, (void *) (uintptr_t) f.AddrPC.Offset);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void WalkThread(HANDLE proc, HANDLE th, DWORD tid) {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    if (SuspendThread(th) == (DWORD) -1) return;
    if (GetThreadContext(th, &ctx)) {
        // Collect addresses while the thread is suspended, symbolize after it runs again.
        DWORD64 pcs[32];
        int n = 0;
        STACKFRAME64 f = {};
        f.AddrPC.Offset = ctx.Eip;
        f.AddrFrame.Offset = ctx.Ebp;
        f.AddrStack.Offset = ctx.Esp;
        f.AddrPC.Mode = f.AddrFrame.Mode = f.AddrStack.Mode = AddrModeFlat;
        while (n < 32 && StackWalk64(IMAGE_FILE_MACHINE_I386, proc, th, &f, &ctx, nullptr, SymFunctionTableAccess64,
                                     SymGetModuleBase64, nullptr) && f.AddrPC.Offset)
            pcs[n++] = f.AddrPC.Offset;
        ResumeThread(th);
        Log("thread %lu:", tid);
        for (int i = 0; i < n; i++) {
            char buf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO *sym = (SYMBOL_INFO *) buf;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 255;
            DWORD64 disp = 0;
            IMAGEHLP_LINE64 line = {sizeof(line)};
            DWORD ldisp = 0;
            const char *name = SymFromAddr(proc, pcs[i], &disp, sym) ? sym->Name : "?";
            if (SymGetLineFromAddr64(proc, pcs[i], &ldisp, &line)) Log("  %s %s:%lu", name, line.FileName, line.LineNumber);
            else Log("  %s+0x%llx", name, disp);
        }
        return;
    }
    ResumeThread(th);
}

// Signal the event "d3d9gl-stacks-<pid>" to log the stack of every thread (hang diagnosis).
static DWORD WINAPI StackDumper(LPVOID ev) {
    HANDLE proc = GetCurrentProcess();
    for (;;) {
        WaitForSingleObject((HANDLE) ev, INFINITE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        SymInitialize(proc, nullptr, TRUE);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te = {sizeof(te)};
        for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
            HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
            if (th) {
                WalkThread(proc, th, te.th32ThreadID);
                CloseHandle(th);
            }
        }
        CloseHandle(snap);
        SymCleanup(proc);
        Log("stacks done");
    }
}

void InstallCrashLog() {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("PORTICO_D3D9_LOG", path, MAX_PATH)) return;
    AddVectoredExceptionHandler(1, OnFault);
    char name[64];
    sprintf_s(name, "d3d9gl-stacks-%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, name);
    if (ev) CloseHandle(CreateThread(nullptr, 0, StackDumper, ev, 0, nullptr));
}

}  // namespace d3d9gl

#endif  // !__EMSCRIPTEN__
