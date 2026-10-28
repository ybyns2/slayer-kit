#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "app/App.h"
#include "core/Crash.h"
#include "core/SelfTest.h"
#include "core/Util.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int) {
#ifdef SK_DEV
    if (cmdLine && wcsstr(cmdLine, L"--selftest")) return sk::selftest::Run();
#endif
#ifdef SK_DEV
    const wchar_t* mutexName = L"Local\\SlayerKit_ybyns2_dev";
#else
    const wchar_t* mutexName = L"Local\\SlayerKit_ybyns2";
#endif
    HANDLE mutex = CreateMutexW(nullptr, TRUE, mutexName);
    DWORD mutexError = GetLastError();
    if (!mutex || mutexError == ERROR_ALREADY_EXISTS || mutexError == ERROR_ACCESS_DENIED) {
        if (HWND h = FindWindowW(L"SlayerKitWindow", nullptr)) PostMessageW(h, sk::WM_APP_SHOW, 0, 0);
        return 0;
    }
    sk::crash::Install();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    bool tray = cmdLine && wcsstr(cmdLine, L"--tray") != nullptr;
    sk::Log("Demarrage Slayer Kit " SK_VERSION);
    int code = sk::TheApp().Run(inst, tray);
    CoUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return code;
}
