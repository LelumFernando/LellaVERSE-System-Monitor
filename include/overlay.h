#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Overlay Window Controller
// Drag-to-Resize, Click-Through, System Tray & Context Menus
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include "settings.h"
#include "monitor_manager.h"
#include "renderer.h"
#include "telemetry_manager.h"

namespace LV {

class Overlay {
public:
    static Overlay& Get();

    bool Init(HINSTANCE hInstance);
    int Run();
    void Shutdown();

    HWND GetHwnd() const { return m_hWnd; }

private:
    Overlay() = default;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

    // Context Menu & Tray
    void ShowContextMenu(POINT pt);
    void InitTrayIcon();
    void RemoveTrayIcon();

    // Window configuration
    void UpdateClickThrough();
    void UpdateAlwaysOnTop();
    void ApplyInitialPlacement();

    HINSTANCE      m_hInstance   = NULL;
    HWND           m_hWnd        = NULL;
    NOTIFYICONDATAW m_nid        = {};
    bool           m_trayCreated = false;

    Renderer       m_renderer;
    AppSettings    m_settings;
};

} // namespace LV
