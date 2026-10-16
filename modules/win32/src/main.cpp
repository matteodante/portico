// Portico win32 for Emscripten: main() for programs written for Windows, which start in wWinMain or WinMain
// (link portico::win32_main). Whichever of the two the program defines is called, as the Windows C runtime does.
#include <windows.h>

#include <stdio.h>

extern "C" {
__attribute__((weak)) int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);
__attribute__((weak)) int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
}

int main(int, char **) {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    if (wWinMain) {
        wchar_t empty[1] = {0};
        return wWinMain(instance, nullptr, empty, SW_SHOWDEFAULT);
    }
    if (WinMain) {
        char empty[1] = {0};
        return WinMain(instance, nullptr, empty, SW_SHOWDEFAULT);
    }
    fprintf(stderr, "[portico-win32] the program defines neither wWinMain nor WinMain\n");
    return 1;
}
