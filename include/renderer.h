#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Responsive GDI+ Renderer
// Multi-GPU (GPU 0, GPU 1...) & Multi-Monitor (Monitor 1, 2...)
// StandBy Mode Pro Glass, Auto-Layout (Standard/Narrow/Wide HUD)
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include "theme.h"

namespace LV {

enum LayoutMode {
    LAYOUT_STANDARD, // Normal vertical StandBy cards (240px - 360px)
    LAYOUT_NARROW,   // Ultra-compact single-line rows (< 240px)
    LAYOUT_WIDE      // Streamer horizontal HUD bar (Width > Height * 1.8)
};

enum VectorIconType {
    ICON_NETWORK,
    ICON_LATENCY,
    ICON_CPU,
    ICON_GPU,
    ICON_VRAM,
    ICON_RAM,
    ICON_POWER,
    ICON_FPS
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Init(HWND hWnd);
    void Shutdown();

    void Resize(int width, int height);
    void Render(const TelemetrySnapshot& snap, const AppSettings& settings);

    LayoutMode GetCurrentLayoutMode(int width, int height) const;
    int CalculateOptimalHeight(const TelemetrySnapshot& snap, const AppSettings& settings) const;

private:
    void RenderStandard(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal);
    void RenderNarrow(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal);
    void RenderWide(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal);

    // Drawing primitives
    void DrawRoundedCard(Gdiplus::Graphics& g, float x, float y, float w, float h, float radius, DWORD fillColor, DWORD borderColor);
    void DrawProgressBar(Gdiplus::Graphics& g, float x, float y, float w, float h, float pct, DWORD barColor, DWORD trackColor);
    void DrawVectorIcon(Gdiplus::Graphics& g, float x, float y, float size, VectorIconType type, DWORD color);
    void DrawHeader(Gdiplus::Graphics& g, float x, float y, float w, const wchar_t* title, const ThemePalette& pal);

    HWND        m_hWnd        = NULL;
    HDC         m_hdcScreen   = NULL;
    HDC         m_hdcMem      = NULL;
    HBITMAP     m_hBitmap     = NULL;
    HBITMAP     m_hOldBitmap  = NULL;
    void*       m_pBits       = nullptr;
    int         m_width       = DEFAULT_WIDTH;
    int         m_height      = DEFAULT_HEIGHT;

    ULONG_PTR   m_gdiplusToken = 0;
};

} // namespace LV
