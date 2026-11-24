// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor (Universal Edition)
// Entry Point & Application Lifecycle
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include "settings.h"
#include "monitor_manager.h"
#include "overlay.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // 1. Single Instance Verification
    HANDLE hMutex = CreateMutexW(NULL, TRUE, LV::APP_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        // Bring existing window to foreground if possible
        HWND existingWnd = FindWindowW(LV::APP_CLASS, LV::APP_NAME);
        if (existingWnd) {
            SetForegroundWindow(existingWnd);
        }
        return 0;
    }

    // 2. High-DPI Awareness (Per Monitor v2)
    typedef BOOL(WINAPI* fn_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto pfnSetDpi = (fn_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfnSetDpi) {
            pfnSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            SetProcessDPIAware();
        }
    }

    // 3. COM Initialization for DXGI
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // 4. Load & Persist Settings
    LV::Settings::Get().Load();
    LV::Settings::Get().Save();

    // 5. Initialize & Run Overlay
    auto& overlay = LV::Overlay::Get();
    if (!overlay.Init(hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize LellaVERSE System Monitor.",
                    LV::APP_NAME, MB_ICONERROR);
        CoUninitialize();
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 1;
    }

    int result = overlay.Run();

    // 6. Cleanup
    overlay.Shutdown();
    CoUninitialize();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return result;
}
