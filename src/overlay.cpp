#include "overlay.h"

namespace LV {

constexpr UINT IDM_LOCK_POSITION   = 2001;
constexpr UINT IDM_CLICK_THROUGH   = 2002;
constexpr UINT IDM_ALWAYS_ON_TOP   = 2003;

constexpr UINT IDM_THEME_BASE      = 2100; // 2100 - 2104

constexpr UINT IDM_SEC_NETWORK     = 2200;
constexpr UINT IDM_SEC_PING        = 2201;
constexpr UINT IDM_SEC_CPU         = 2202;
constexpr UINT IDM_SEC_RAM             = 2203;
constexpr UINT IDM_SEC_POWER           = 2204;
constexpr UINT IDM_SEC_POWER_BREAKDOWN = 2205;

constexpr UINT IDM_OPACITY_100     = 2300;
constexpr UINT IDM_OPACITY_85      = 2301;
constexpr UINT IDM_OPACITY_70      = 2302;
constexpr UINT IDM_OPACITY_55      = 2303;
constexpr UINT IDM_OPACITY_40      = 2304;
constexpr UINT IDM_OPACITY_25      = 2305;

constexpr UINT IDM_REFRESH_500     = 2400;
constexpr UINT IDM_REFRESH_1000    = 2401;
constexpr UINT IDM_REFRESH_2000    = 2402;

constexpr UINT IDM_MONITOR_BASE    = 2500; // 2500+
constexpr UINT IDM_RESET_POS       = 2601;
constexpr UINT IDM_STARTUP         = 2602;
constexpr UINT IDM_EXIT            = 2603;

constexpr UINT IDM_GPU_USAGE_BASE   = 2700; // 2700 - 2719
constexpr UINT IDM_GPU_VRAM_BASE    = 2720; // 2720 - 2739
constexpr UINT IDM_MONITOR_FPS_BASE = 2740; // 2740 - 2759

Overlay& Overlay::Get() {
    static Overlay instance;
    return instance;
}

bool Overlay::Init(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    m_settings = Settings::Get().Data();

    // 1. Register Window Class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = APP_CLASS;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);

    // Custom App Icon
    HICON hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!hIcon) {
        hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }
    wc.hIcon   = hIcon;
    wc.hIconSm = hIcon;

    RegisterClassExW(&wc);

    // 2. Refresh Monitors & Apply Initial Placement
    MonitorManager::Get().RefreshMonitors();
    ApplyInitialPlacement();

    // 3. Create Layered Window
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (m_settings.alwaysOnTop)   exStyle |= WS_EX_TOPMOST;
    if (m_settings.clickThrough)  exStyle |= WS_EX_TRANSPARENT;

    m_hWnd = CreateWindowExW(
        exStyle,
        APP_CLASS,
        APP_NAME,
        WS_POPUP | WS_VISIBLE,
        m_settings.posX, m_settings.posY,
        m_settings.width, m_settings.height,
        NULL, NULL, hInstance, this
    );

    if (!m_hWnd) return false;

    // 4. Initialize GDI+ Renderer
    m_renderer.Init(m_hWnd);
    m_renderer.Resize(m_settings.width, m_settings.height);

    // 5. Initialize System Tray Icon
    InitTrayIcon();

    // 6. Start Telemetry Engine & UI Refresh Timer
    TelemetryManager::Get().Init();
    m_settings = Settings::Get().Data();
    TelemetryManager::Get().SetRefreshInterval(m_settings.refreshMs);

    SetTimer(m_hWnd, TIMER_TELEMETRY, m_settings.refreshMs, NULL);

    // Initial render with dynamic optimal height adjustment
    TelemetrySnapshot initialSnap = TelemetryManager::Get().GetSnapshot();
    int optimalH = m_renderer.CalculateOptimalHeight(initialSnap, m_settings);
    if (optimalH >= MIN_HEIGHT && optimalH != m_settings.height) {
        m_settings.height = optimalH;
        Settings::Get().Data().height = optimalH;
        SetWindowPos(m_hWnd, NULL, 0, 0, m_settings.width, optimalH, SWP_NOMOVE | SWP_NOZORDER);
        m_renderer.Resize(m_settings.width, optimalH);
    }
    m_renderer.Render(initialSnap, m_settings);

    return true;
}

void Overlay::ApplyInitialPlacement() {
    auto& mm = MonitorManager::Get();

    int targetMon = m_settings.monitorIndex;
    if (targetMon < 0) {
        targetMon = mm.GetSecondaryIndex();
        m_settings.monitorIndex = targetMon;
    }

    if (m_settings.posX < 0 || m_settings.posY < 0) {
        POINT pt = mm.GetDefaultPosition(targetMon, m_settings.width, m_settings.height);
        m_settings.posX = pt.x;
        m_settings.posY = pt.y;
    } else {
        mm.ClampToMonitor(targetMon, m_settings.posX, m_settings.posY, m_settings.width, m_settings.height);
    }
}

void Overlay::InitTrayIcon() {
    m_nid = { sizeof(m_nid) };
    m_nid.hWnd             = m_hWnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;

    HICON hIcon = (HICON)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                    IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED);
    if (!hIcon) {
        hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }
    m_nid.hIcon = hIcon;
    wcsncpy_s(m_nid.szTip, APP_NAME, _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &m_nid);
    m_trayCreated = true;
}

void Overlay::RemoveTrayIcon() {
    if (m_trayCreated) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayCreated = false;
    }
}

void Overlay::UpdateClickThrough() {
    LONG_PTR exStyle = GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
    if (m_settings.clickThrough) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(m_hWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void Overlay::UpdateAlwaysOnTop() {
    HWND insertAfter = m_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(m_hWnd, insertAfter, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

int Overlay::Run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void Overlay::Shutdown() {
    KillTimer(m_hWnd, TIMER_TELEMETRY);
    RemoveTrayIcon();
    TelemetryManager::Get().Shutdown();
    m_renderer.Shutdown();

    Settings::Get().Data() = m_settings;
    Settings::Get().Save();
}

void Overlay::ShowContextMenu(POINT pt) {
    HMENU hMenu = CreatePopupMenu();

    // 1. Lock Position
    AppendMenuW(hMenu, MF_STRING | (m_settings.locked ? MF_CHECKED : MF_UNCHECKED),
                IDM_LOCK_POSITION, L"Lock Position");

    // 2. Click-Through
    AppendMenuW(hMenu, MF_STRING | (m_settings.clickThrough ? MF_CHECKED : MF_UNCHECKED),
                IDM_CLICK_THROUGH, L"Click-Through Mode (Transparent)");

    // 3. Always On Top
    AppendMenuW(hMenu, MF_STRING | (m_settings.alwaysOnTop ? MF_CHECKED : MF_UNCHECKED),
                IDM_ALWAYS_ON_TOP, L"Always on Top");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 4. Themes Submenu
    HMENU hThemeMenu = CreatePopupMenu();
    for (int t = 0; t < THEME_COUNT; ++t) {
        UINT flags = MF_STRING | (m_settings.theme == (AppTheme)t ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hThemeMenu, flags, IDM_THEME_BASE + t, ThemeName((AppTheme)t));
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hThemeMenu, L"Theme");

    // 5. Visible Core Metrics Submenu
    HMENU hMetricsMenu = CreatePopupMenu();
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showNetwork ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_NETWORK, L"Network Traffic (Speed)");
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showPing ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_PING, L"Latency (Real-Time Ping)");
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showCpu ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_CPU, L"CPU (Usage / Clock / Temp)");
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showRam ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_RAM, L"RAM (Memory Usage / Capacity)");
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showPower ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_POWER, L"Power (Total System Draw)");
    AppendMenuW(hMetricsMenu, MF_STRING | (m_settings.showPowerBreakdown ? MF_CHECKED : MF_UNCHECKED),
                IDM_SEC_POWER_BREAKDOWN, L"Component Power Breakdown");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMetricsMenu, L"Visible Metrics");

    // 6. Graphics Cards (Multi-GPU) Submenu
    TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
    HMENU hGpuMenu = CreatePopupMenu();
    if (snap.gpus.empty()) {
        AppendMenuW(hGpuMenu, MF_STRING | MF_GRAYED, 0, L"No Physical GPUs Detected");
    } else {
        for (size_t i = 0; i < snap.gpus.size(); ++i) {
            const auto& g = snap.gpus[i];
            wchar_t itemUsage[128], itemVram[128];
            swprintf_s(itemUsage, L"%s (%s) - Core Usage (%%)", g.label, g.name);
            swprintf_s(itemVram, g.isDiscrete ? L"%s (%s) - Dedicated VRAM" : L"%s (%s) - GPU Memory (Shared)", g.label, g.name);

            bool uVis = m_settings.IsGpuUsageVisible((int)i);
            bool vVis = m_settings.IsGpuVramVisible((int)i);

            AppendMenuW(hGpuMenu, MF_STRING | (uVis ? MF_CHECKED : MF_UNCHECKED),
                        IDM_GPU_USAGE_BASE + (UINT)i, itemUsage);
            AppendMenuW(hGpuMenu, MF_STRING | (vVis ? MF_CHECKED : MF_UNCHECKED),
                        IDM_GPU_VRAM_BASE + (UINT)i, itemVram);

            if (i + 1 < snap.gpus.size()) {
                AppendMenuW(hGpuMenu, MF_SEPARATOR, 0, NULL);
            }
        }
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hGpuMenu, L"Graphics Cards (GPUs)");

    // 7. Monitor FPS Displays (Multi-Monitor) Submenu
    HMENU hFpsMenu = CreatePopupMenu();
    if (snap.monitors.empty()) {
        AppendMenuW(hFpsMenu, MF_STRING | MF_GRAYED, 0, L"No Active Displays Detected");
    } else {
        for (size_t i = 0; i < snap.monitors.size(); ++i) {
            const auto& m = snap.monitors[i];
            wchar_t itemFps[128];
            swprintf_s(itemFps, L"%s (%s) - Real-Time FPS", m.label, m.resolution);

            bool fVis = m_settings.IsMonitorFpsVisible(m.index);
            AppendMenuW(hFpsMenu, MF_STRING | (fVis ? MF_CHECKED : MF_UNCHECKED),
                        IDM_MONITOR_FPS_BASE + (UINT)i, itemFps);
        }
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFpsMenu, L"Monitor FPS Displays");

    // 8. Opacity Submenu
    HMENU hOpacityMenu = CreatePopupMenu();
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 100 ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_100, L"100%");
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 85  ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_85,  L"85%");
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 70  ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_70,  L"70%");
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 55  ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_55,  L"55%");
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 40  ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_40,  L"40%");
    AppendMenuW(hOpacityMenu, MF_STRING | (m_settings.opacityPct == 25  ? MF_CHECKED : MF_UNCHECKED), IDM_OPACITY_25,  L"25%");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOpacityMenu, L"Opacity");

    // 9. Refresh Rate Submenu
    HMENU hRefreshMenu = CreatePopupMenu();
    AppendMenuW(hRefreshMenu, MF_STRING | (m_settings.refreshMs == 500  ? MF_CHECKED : MF_UNCHECKED), IDM_REFRESH_500,  L"500 ms (Fast)");
    AppendMenuW(hRefreshMenu, MF_STRING | (m_settings.refreshMs == 1000 ? MF_CHECKED : MF_UNCHECKED), IDM_REFRESH_1000, L"1000 ms (Default)");
    AppendMenuW(hRefreshMenu, MF_STRING | (m_settings.refreshMs == 2000 ? MF_CHECKED : MF_UNCHECKED), IDM_REFRESH_2000, L"2000 ms (Eco)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hRefreshMenu, L"Refresh Rate");

    // 10. Select Monitor Submenu
    HMENU hMonMenu = CreatePopupMenu();
    auto& monitors = MonitorManager::Get().GetMonitors();
    for (size_t i = 0; i < monitors.size(); ++i) {
        wchar_t buf[128];
        swprintf_s(buf, L"Display %zu (%s)%s",
            i + 1, monitors[i].name, monitors[i].isPrimary ? L" [Primary]" : L"");
        UINT flags = MF_STRING | (m_settings.monitorIndex == (int)i ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hMonMenu, flags, IDM_MONITOR_BASE + (UINT)i, buf);
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMonMenu, L"Select Display");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 11. Reset Position
    AppendMenuW(hMenu, MF_STRING, IDM_RESET_POS, L"Reset Position");

    // 12. Run at Startup
    AppendMenuW(hMenu, MF_STRING | (m_settings.runAtStartup ? MF_CHECKED : MF_UNCHECKED),
                IDM_STARTUP, L"Run at Windows Startup");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // 13. Exit
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    // Display context menu
    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK Overlay::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    Overlay* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<Overlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hWnd, msg, wp, lp);
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

LRESULT Overlay::HandleMessage(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST: {
        if (!m_settings.locked) {
            RECT rc;
            GetWindowRect(hWnd, &rc);
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);

            bool left   = (x >= rc.left && x < rc.left + RESIZE_BORDER);
            bool right  = (x < rc.right && x >= rc.right - RESIZE_BORDER);
            bool top    = (y >= rc.top && y < rc.top + RESIZE_BORDER);
            bool bottom = (y < rc.bottom && y >= rc.bottom - RESIZE_BORDER);

            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;

            // Center area acts as caption for dragging
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_SIZING: {
        RECT* r = reinterpret_cast<RECT*>(lp);
        if (r->right - r->left < MIN_WIDTH)   r->right = r->left + MIN_WIDTH;
        if (r->bottom - r->top < MIN_HEIGHT) r->bottom = r->top + MIN_HEIGHT;
        return TRUE;
    }

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        if (w >= MIN_WIDTH && h >= MIN_HEIGHT) {
            m_settings.width  = w;
            m_settings.height = h;
            m_renderer.Resize(w, h);

            Settings::Get().Data().width  = w;
            Settings::Get().Data().height = h;
            Settings::Get().Save();

            TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
            m_renderer.Render(snap, m_settings);
        }
        return 0;
    }

    case WM_MOVE: {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        m_settings.posX = x;
        m_settings.posY = y;

        Settings::Get().Data().posX = x;
        Settings::Get().Data().posY = y;
        Settings::Get().Save();
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        ShowContextMenu(pt);
        return 0;
    }

    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(pt);
        }
        return 0;
    }

    case WM_TIMER: {
        if (wp == TIMER_TELEMETRY) {
            TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
            m_renderer.Render(snap, m_settings);
        }
        return 0;
    }

    case WM_DISPLAYCHANGE: {
        MonitorManager::Get().RefreshMonitors();
        auto& mm = MonitorManager::Get();
        mm.ClampToMonitor(m_settings.monitorIndex, m_settings.posX, m_settings.posY,
                          m_settings.width, m_settings.height);
        SetWindowPos(m_hWnd, NULL, m_settings.posX, m_settings.posY, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER);
        return 0;
    }

    case WM_COMMAND: {
        UINT id = LOWORD(wp);

        auto applyDynamicLayoutAndRender = [&]() {
            TelemetryManager::Get().NotifyTogglesChanged();

            TelemetrySnapshot curSnap = TelemetryManager::Get().GetSnapshot();
            for (size_t g = 0; g < curSnap.gpus.size(); ++g) {
                curSnap.gpus[g].showUsage = m_settings.IsGpuUsageVisible((int)g);
                curSnap.gpus[g].showVram  = m_settings.IsGpuVramVisible((int)g);
            }
            for (size_t m = 0; m < curSnap.monitors.size(); ++m) {
                curSnap.monitors[m].showFps = m_settings.IsMonitorFpsVisible(curSnap.monitors[m].index);
            }

            int optimalH = m_renderer.CalculateOptimalHeight(curSnap, m_settings);
            if (optimalH >= MIN_HEIGHT) {
                m_settings.height = optimalH;
                Settings::Get().Data().height = optimalH;
                Settings::Get().Save();
                SetWindowPos(m_hWnd, NULL, 0, 0, m_settings.width, optimalH, SWP_NOMOVE | SWP_NOZORDER);
                m_renderer.Resize(m_settings.width, optimalH);
            }

            m_renderer.Render(curSnap, m_settings);
        };

        if (id == IDM_LOCK_POSITION) {
            m_settings.locked = !m_settings.locked;
            Settings::Get().Data().locked = m_settings.locked;
            Settings::Get().Save();
        } else if (id == IDM_CLICK_THROUGH) {
            m_settings.clickThrough = !m_settings.clickThrough;
            Settings::Get().Data().clickThrough = m_settings.clickThrough;
            Settings::Get().Save();
            UpdateClickThrough();
        } else if (id == IDM_ALWAYS_ON_TOP) {
            m_settings.alwaysOnTop = !m_settings.alwaysOnTop;
            Settings::Get().Data().alwaysOnTop = m_settings.alwaysOnTop;
            Settings::Get().Save();
            UpdateAlwaysOnTop();
        } else if (id >= IDM_THEME_BASE && id < IDM_THEME_BASE + THEME_COUNT) {
            m_settings.theme = (AppTheme)(id - IDM_THEME_BASE);
            Settings::Get().Data().theme = m_settings.theme;
            Settings::Get().Save();
            TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
            m_renderer.Render(snap, m_settings);
        } else if (id == IDM_SEC_NETWORK) {
            m_settings.showNetwork = !m_settings.showNetwork;
            Settings::Get().Data().showNetwork = m_settings.showNetwork;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id == IDM_SEC_PING) {
            m_settings.showPing = !m_settings.showPing;
            Settings::Get().Data().showPing = m_settings.showPing;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id == IDM_SEC_CPU) {
            m_settings.showCpu = !m_settings.showCpu;
            Settings::Get().Data().showCpu = m_settings.showCpu;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id == IDM_SEC_RAM) {
            m_settings.showRam = !m_settings.showRam;
            Settings::Get().Data().showRam = m_settings.showRam;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id == IDM_SEC_POWER) {
            m_settings.showPower = !m_settings.showPower;
            Settings::Get().Data().showPower = m_settings.showPower;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id == IDM_SEC_POWER_BREAKDOWN) {
            m_settings.showPowerBreakdown = !m_settings.showPowerBreakdown;
            Settings::Get().Data().showPowerBreakdown = m_settings.showPowerBreakdown;
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id >= IDM_GPU_USAGE_BASE && id < IDM_GPU_USAGE_BASE + 20) {
            int gpuIdx = id - IDM_GPU_USAGE_BASE;
            bool cur = m_settings.IsGpuUsageVisible(gpuIdx);
            m_settings.SetGpuUsageVisible(gpuIdx, !cur);
            Settings::Get().SetGpuUsageVisible(gpuIdx, !cur);
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id >= IDM_GPU_VRAM_BASE && id < IDM_GPU_VRAM_BASE + 20) {
            int gpuIdx = id - IDM_GPU_VRAM_BASE;
            bool cur = m_settings.IsGpuVramVisible(gpuIdx);
            m_settings.SetGpuVramVisible(gpuIdx, !cur);
            Settings::Get().SetGpuVramVisible(gpuIdx, !cur);
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id >= IDM_MONITOR_FPS_BASE && id < IDM_MONITOR_FPS_BASE + 20) {
            int monIdx = id - IDM_MONITOR_FPS_BASE;
            bool cur = m_settings.IsMonitorFpsVisible(monIdx);
            m_settings.SetMonitorFpsVisible(monIdx, !cur);
            Settings::Get().SetMonitorFpsVisible(monIdx, !cur);
            Settings::Get().Save();
            applyDynamicLayoutAndRender();
        } else if (id >= IDM_OPACITY_100 && id <= IDM_OPACITY_25) {
            static const int opacities[] = { 100, 85, 70, 55, 40, 25 };
            m_settings.opacityPct = opacities[id - IDM_OPACITY_100];
            Settings::Get().Data().opacityPct = m_settings.opacityPct;
            Settings::Get().Save();
            TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
            m_renderer.Render(snap, m_settings);
        } else if (id == IDM_REFRESH_500 || id == IDM_REFRESH_1000 || id == IDM_REFRESH_2000) {
            int ms = (id == IDM_REFRESH_500) ? 500 : (id == IDM_REFRESH_1000 ? 1000 : 2000);
            m_settings.refreshMs = ms;
            Settings::Get().Data().refreshMs = ms;
            Settings::Get().Save();

            KillTimer(m_hWnd, TIMER_TELEMETRY);
            SetTimer(m_hWnd, TIMER_TELEMETRY, ms, NULL);
            TelemetryManager::Get().SetRefreshInterval(ms);
        } else if (id >= IDM_MONITOR_BASE && id < IDM_MONITOR_BASE + 32) {
            int monIdx = id - IDM_MONITOR_BASE;
            m_settings.monitorIndex = monIdx;
            Settings::Get().Data().monitorIndex = monIdx;

            POINT pt = MonitorManager::Get().GetDefaultPosition(monIdx, m_settings.width, m_settings.height);
            m_settings.posX = pt.x;
            m_settings.posY = pt.y;
            Settings::Get().Data().posX = pt.x;
            Settings::Get().Data().posY = pt.y;
            Settings::Get().Save();

            SetWindowPos(m_hWnd, NULL, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        } else if (id == IDM_RESET_POS) {
            POINT pt = MonitorManager::Get().GetDefaultPosition(m_settings.monitorIndex, m_settings.width, m_settings.height);
            m_settings.posX = pt.x;
            m_settings.posY = pt.y;
            m_settings.width = DEFAULT_WIDTH;
            m_settings.height = DEFAULT_HEIGHT;

            Settings::Get().Data().posX = pt.x;
            Settings::Get().Data().posY = pt.y;
            Settings::Get().Data().width = DEFAULT_WIDTH;
            Settings::Get().Data().height = DEFAULT_HEIGHT;
            Settings::Get().Save();

            SetWindowPos(m_hWnd, NULL, pt.x, pt.y, DEFAULT_WIDTH, DEFAULT_HEIGHT, SWP_NOZORDER);
            m_renderer.Resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
            TelemetrySnapshot snap = TelemetryManager::Get().GetSnapshot();
            m_renderer.Render(snap, m_settings);
        } else if (id == IDM_STARTUP) {
            m_settings.runAtStartup = !m_settings.runAtStartup;
            Settings::Get().SetRunAtStartup(m_settings.runAtStartup);
        } else if (id == IDM_EXIT) {
            DestroyWindow(m_hWnd);
        }
        return 0;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wp, lp);
}

} // namespace LV
