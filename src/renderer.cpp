#include "renderer.h"

namespace LV {

Renderer::Renderer() {
    Gdiplus::GdiplusStartupInput gdiInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiInput, NULL);
}

Renderer::~Renderer() {
    Shutdown();
    if (m_gdiplusToken) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}

bool Renderer::Init(HWND hWnd) {
    m_hWnd = hWnd;
    m_hdcScreen = GetDC(NULL);
    m_hdcMem = CreateCompatibleDC(m_hdcScreen);

    Resize(m_width, m_height);
    return true;
}

void Renderer::Shutdown() {
    if (m_hdcMem) {
        if (m_hOldBitmap) {
            SelectObject(m_hdcMem, m_hOldBitmap);
            m_hOldBitmap = NULL;
        }
        DeleteDC(m_hdcMem);
        m_hdcMem = NULL;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
    }
    if (m_hdcScreen) {
        ReleaseDC(NULL, m_hdcScreen);
        m_hdcScreen = NULL;
    }
}

void Renderer::Resize(int width, int height) {
    if (width < MIN_WIDTH) width = MIN_WIDTH;
    if (height < MIN_HEIGHT) height = MIN_HEIGHT;

    m_width = width;
    m_height = height;

    if (m_hdcMem) {
        if (m_hOldBitmap) {
            SelectObject(m_hdcMem, m_hOldBitmap);
            m_hOldBitmap = NULL;
        }
        if (m_hBitmap) {
            DeleteObject(m_hBitmap);
            m_hBitmap = NULL;
        }

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = m_width;
        bi.bmiHeader.biHeight      = -m_height; // Top-down
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        m_hBitmap = CreateDIBSection(m_hdcMem, &bi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
        if (m_hBitmap) {
            m_hOldBitmap = (HBITMAP)SelectObject(m_hdcMem, m_hBitmap);
        }
    }
}

LayoutMode Renderer::GetCurrentLayoutMode(int width, int height) const {
    if (width > (int)(height * 1.8f)) {
        return LAYOUT_WIDE;
    }
    if (width < 240) {
        return LAYOUT_NARROW;
    }
    return LAYOUT_STANDARD;
}

int Renderer::CalculateOptimalHeight(const TelemetrySnapshot& snap, const AppSettings& settings) const {
    int activeCount = 0;
    if (settings.showNetwork) activeCount++;
    if (settings.showPing)    activeCount++;
    if (settings.showCpu)     activeCount++;

    for (const auto& g : snap.gpus) {
        if (g.showUsage) activeCount++;
        if (g.showVram)  activeCount++;
    }

    if (settings.showRam)   activeCount++;
    if (settings.showPower) activeCount++;

    for (const auto& m : snap.monitors) {
        if (m.showFps) activeCount++;
    }

    if (activeCount == 0) return MIN_HEIGHT;

    int optH = 28 + activeCount * 52;
    if (settings.showPower && settings.showPowerBreakdown && snap.powerBreakdown[0] != L'\0') {
        optH += 10;
    }
    return std::clamp(optH, MIN_HEIGHT, MAX_HEIGHT);
}

void Renderer::DrawRoundedCard(Gdiplus::Graphics& g, float x, float y, float w, float h, float radius, DWORD fillColor, DWORD borderColor) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2.0f;

    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    Gdiplus::Color cFill(fillColor);
    Gdiplus::SolidBrush fillBrush(cFill);
    g.FillPath(&fillBrush, &path);

    if ((borderColor & 0xFF000000) != 0) {
        Gdiplus::Color cBorder(borderColor);
        Gdiplus::Pen borderPen(cBorder, 1.0f);
        g.DrawPath(&borderPen, &path);
    }
}

void Renderer::DrawProgressBar(Gdiplus::Graphics& g, float x, float y, float w, float h, float pct, DWORD barColor, DWORD trackColor) {
    pct = std::clamp(pct, 0.0f, 100.0f);
    float radius = h * 0.5f;

    // Track
    DrawRoundedCard(g, x, y, w, h, radius, trackColor, 0);

    // Fill
    float fillW = w * (pct / 100.0f);
    if (fillW >= h) {
        DrawRoundedCard(g, x, y, fillW, h, radius, barColor, 0);
    } else if (fillW > 0.0f) {
        Gdiplus::Color cBar(barColor);
        Gdiplus::SolidBrush barBrush(cBar);
        g.FillEllipse(&barBrush, x, y, h, h);
    }
}

void Renderer::DrawVectorIcon(Gdiplus::Graphics& g, float x, float y, float size, VectorIconType type, DWORD color) {
    Gdiplus::Color c(color);
    Gdiplus::SolidBrush brush(c);
    Gdiplus::Pen pen(c, 1.3f);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);

    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;

    switch (type) {
    case ICON_NETWORK: {
        float ax1 = x + 3.0f;
        g.DrawLine(&pen, ax1, y + 2.0f, ax1, y + 10.0f);
        g.DrawLine(&pen, ax1 - 2.0f, y + 7.5f, ax1, y + 10.0f);
        g.DrawLine(&pen, ax1 + 2.0f, y + 7.5f, ax1, y + 10.0f);

        float ax2 = x + size - 3.0f;
        g.DrawLine(&pen, ax2, y + 10.0f, ax2, y + 2.0f);
        g.DrawLine(&pen, ax2 - 2.0f, y + 4.5f, ax2, y + 2.0f);
        g.DrawLine(&pen, ax2 + 2.0f, y + 4.5f, ax2, y + 2.0f);
        break;
    }
    case ICON_LATENCY: {
        g.FillEllipse(&brush, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
        Gdiplus::Pen pulsePen(c, 1.2f);
        g.DrawEllipse(&pulsePen, x + 1.5f, y + 1.5f, size - 3.0f, size - 3.0f);
        break;
    }
    case ICON_CPU: {
        Gdiplus::Pen chipPen(c, 1.2f);
        g.DrawRectangle(&chipPen, x + 2.5f, y + 2.5f, size - 5.0f, size - 5.0f);
        g.FillRectangle(&brush, x + 4.5f, y + 4.5f, size - 9.0f, size - 9.0f);

        g.DrawLine(&chipPen, x + 4.5f, y + 1.0f, x + 4.5f, y + 2.5f);
        g.DrawLine(&chipPen, x + 7.5f, y + 1.0f, x + 7.5f, y + 2.5f);
        g.DrawLine(&chipPen, x + 4.5f, y + size - 2.5f, x + 4.5f, y + size - 1.0f);
        g.DrawLine(&chipPen, x + 7.5f, y + size - 2.5f, x + 7.5f, y + size - 1.0f);

        g.DrawLine(&chipPen, x + 1.0f, y + 4.5f, x + 2.5f, y + 4.5f);
        g.DrawLine(&chipPen, x + 1.0f, y + 7.5f, x + 2.5f, y + 7.5f);
        g.DrawLine(&chipPen, x + size - 2.5f, y + 4.5f, x + size - 1.0f, y + 4.5f);
        g.DrawLine(&chipPen, x + size - 2.5f, y + 7.5f, x + size - 1.0f, y + 7.5f);
        break;
    }
    case ICON_GPU: {
        Gdiplus::Pen gpuPen(c, 1.2f);
        g.DrawRectangle(&gpuPen, x + 1.0f, y + 2.5f, size - 2.0f, size - 5.0f);
        g.DrawEllipse(&gpuPen, x + 2.5f, y + 4.0f, 3.2f, 3.2f);
        g.DrawEllipse(&gpuPen, x + 6.3f, y + 4.0f, 3.2f, 3.2f);
        g.DrawLine(&gpuPen, x + 2.5f, y + size - 2.5f, x + 6.0f, y + size - 2.5f);
        break;
    }
    case ICON_VRAM:
    case ICON_RAM: {
        Gdiplus::Pen memPen(c, 1.2f);
        g.DrawRectangle(&memPen, x + 1.0f, y + 3.0f, size - 2.0f, size - 6.0f);
        g.FillRectangle(&brush, x + 2.5f, y + 4.5f, 1.8f, 3.0f);
        g.FillRectangle(&brush, x + 5.1f, y + 4.5f, 1.8f, 3.0f);
        g.FillRectangle(&brush, x + 7.7f, y + 4.5f, 1.8f, 3.0f);
        break;
    }
    case ICON_POWER: {
        Gdiplus::PointF pts[6] = {
            Gdiplus::PointF(x + 6.5f, y + 1.0f),
            Gdiplus::PointF(x + 2.5f, y + 6.5f),
            Gdiplus::PointF(x + 6.0f, y + 6.5f),
            Gdiplus::PointF(x + 5.0f, y + 11.0f),
            Gdiplus::PointF(x + 9.5f, y + 5.0f),
            Gdiplus::PointF(x + 6.0f, y + 5.0f)
        };
        g.FillPolygon(&brush, pts, 6);
        break;
    }
    case ICON_FPS: {
        Gdiplus::PointF pts[3] = {
            Gdiplus::PointF(x + 3.0f, y + 2.0f),
            Gdiplus::PointF(x + 10.0f, y + 6.0f),
            Gdiplus::PointF(x + 3.0f, y + 10.0f)
        };
        g.FillPolygon(&brush, pts, 3);
        break;
    }
    }
}

void Renderer::DrawHeader(Gdiplus::Graphics& g, float x, float y, float w, const wchar_t* title, const ThemePalette& pal) {
    Gdiplus::Font font(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Color cText(pal.primaryText);
    Gdiplus::SolidBrush textBrush(cText);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentNear);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::RectF rect(x, y, w, 20.0f);
    g.DrawString(title, -1, &font, rect, &sf, &textBrush);
}

void Renderer::Render(const TelemetrySnapshot& snap, const AppSettings& settings) {
    if (!m_hdcMem || !m_hBitmap || !m_pBits) return;

    ZeroMemory(m_pBits, m_width * m_height * 4);

    Gdiplus::Graphics g(m_hdcMem);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    ThemePalette pal = GetThemePalette(settings.theme);

    DrawRoundedCard(g, 1.0f, 1.0f, (float)(m_width - 2), (float)(m_height - 2), 16.0f, pal.background, pal.cardBorder);

    LayoutMode mode = GetCurrentLayoutMode(m_width, m_height);
    if (mode == LAYOUT_WIDE) {
        RenderWide(g, snap, settings, pal);
    } else if (mode == LAYOUT_NARROW) {
        RenderNarrow(g, snap, settings, pal);
    } else {
        RenderStandard(g, snap, settings, pal);
    }

    POINT ptSrc = { 0, 0 };
    SIZE  wndSize = { m_width, m_height };
    BLENDFUNCTION bf = {};
    bf.BlendOp             = AC_SRC_OVER;
    bf.BlendFlags          = 0;
    bf.SourceConstantAlpha = (BYTE)(settings.opacityPct * 255 / 100);
    bf.AlphaFormat         = AC_SRC_ALPHA;

    POINT ptDst = { settings.posX, settings.posY };

    UpdateLayeredWindow(m_hWnd, m_hdcScreen, &ptDst, &wndSize,
                        m_hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
}

void Renderer::RenderStandard(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal) {
    // 1. Count active items
    int activeCount = 0;
    if (settings.showNetwork) activeCount++;
    if (settings.showPing)    activeCount++;
    if (settings.showCpu)     activeCount++;

    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) activeCount++;
        if (gpu.showVram)  activeCount++;
    }

    if (settings.showRam)   activeCount++;
    if (settings.showPower) activeCount++;

    for (const auto& mon : snap.monitors) {
        if (mon.showFps) activeCount++;
    }

    if (activeCount == 0) return;

    float padX = 14.0f;
    float topY = 14.0f;
    float availW = (float)m_width - padX * 2.0f;
    float availH = (float)m_height - topY - 14.0f;

    float rowSpacing = 6.0f;
    float blockH = (availH - (activeCount - 1) * rowSpacing) / (float)activeCount;
    if (blockH > 56.0f) blockH = 56.0f;

    Gdiplus::Font fontTitle(L"Segoe UI", 8.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontValue(L"Segoe UI", 11.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontDetail(L"Segoe UI", 7.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    Gdiplus::Color cWhite(pal.primaryText);
    Gdiplus::Color cDim(pal.secondaryText);
    Gdiplus::SolidBrush brushWhite(cWhite);
    Gdiplus::SolidBrush brushDim(cDim);

    Gdiplus::StringFormat sfRight;
    sfRight.SetAlignment(Gdiplus::StringAlignmentFar);

    float curY = topY;

    // Network Traffic
    if (settings.showNetwork) {
        DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
        DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_NETWORK, pal.netAccent);

        Gdiplus::Color cNet(pal.netAccent);
        Gdiplus::SolidBrush brushNet(cNet);
        g.DrawString(L"NETWORK", -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushNet);

        wchar_t bufDown[32], bufUp[32];
        if (snap.netDownKBs >= 1024.0) swprintf_s(bufDown, L"%.2f MB/s", snap.netDownKBs / 1024.0);
        else swprintf_s(bufDown, L"%.1f KB/s", snap.netDownKBs);

        if (snap.netUpKBs >= 1024.0) swprintf_s(bufUp, L"%.2f MB/s", snap.netUpKBs / 1024.0);
        else swprintf_s(bufUp, L"%.1f KB/s", snap.netUpKBs);

        wchar_t speedText[64];
        swprintf_s(speedText, L"\u2193 %s   \u2191 %s", bufDown, bufUp);
        g.DrawString(speedText, -1, &fontDetail, Gdiplus::RectF(padX, curY + 7.0f, availW - 8.0f, 16.0f), &sfRight, &brushWhite);

        curY += blockH + rowSpacing;
    }

    // Ping Latency
    if (settings.showPing) {
        DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);

        DWORD pingColor = pal.pingRed;
        wchar_t pingStr[32];
        if (snap.pingSuccess && snap.netPingMs >= 0.0) {
            swprintf_s(pingStr, L"%.0f ms", snap.netPingMs);
            if (snap.netPingMs < 60.0) pingColor = pal.pingGreen;
            else if (snap.netPingMs <= 120.0) pingColor = pal.pingOrange;
            else pingColor = pal.pingRed;
        } else {
            swprintf_s(pingStr, L"Timeout");
            pingColor = pal.pingRed;
        }

        DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_LATENCY, pingColor);

        Gdiplus::Color cPing(pingColor);
        Gdiplus::SolidBrush brushPing(cPing);
        g.DrawString(L"LATENCY", -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushPing);
        g.DrawString(pingStr, -1, &fontValue, Gdiplus::RectF(padX, curY + 4.0f, availW - 8.0f, 20.0f), &sfRight, &brushPing);

        curY += blockH + rowSpacing;
    }

    // CPU
    if (settings.showCpu) {
        DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
        DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_CPU, pal.cpuAccent);

        Gdiplus::Color cCpu(pal.cpuAccent);
        Gdiplus::SolidBrush brushCpu(cCpu);
        g.DrawString(L"CPU", -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushCpu);

        wchar_t cpuStr[64];
        if (snap.cpuTempAvail && snap.cpuTempC > 0.0) {
            swprintf_s(cpuStr, L"%.0f%%  \u00B7  %.2f GHz  \u00B7  %.0f\u00B0C",
                snap.cpuUsagePct, snap.cpuClockMHz / 1000.0, snap.cpuTempC);
        } else {
            swprintf_s(cpuStr, L"%.0f%%  \u00B7  %.2f GHz",
                snap.cpuUsagePct, snap.cpuClockMHz / 1000.0);
        }
        g.DrawString(cpuStr, -1, &fontValue, Gdiplus::RectF(padX, curY + 4.0f, availW - 8.0f, 20.0f), &sfRight, &brushWhite);

        if (blockH >= 42.0f) {
            DrawProgressBar(g, padX + 8.0f, curY + blockH - 9.0f, availW - 16.0f, 4.0f, (float)snap.cpuUsagePct, pal.cpuAccent, pal.barBackground);
        }

        curY += blockH + rowSpacing;
    }

    // Multi-GPU Rendering (GPU 0, GPU 1, etc.)
    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) {
            DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
            DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_GPU, pal.gpuAccent);

            Gdiplus::Color cGpu(pal.gpuAccent);
            Gdiplus::SolidBrush brushGpu(cGpu);
            g.DrawString(gpu.label, -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushGpu);

            wchar_t gpuStr[64];
            bool hasClock = (gpu.clockMHz > 0.0);
            bool hasTemp  = (gpu.tempC > 0.0);

            if (gpu.isDiscrete) {
                // Discrete GPU (GPU 1, GPU 2, etc.): format exact same layout and visual styling as CPU row:
                // <Usage>% · <Clock> GHz · <Temp>°C (e.g. 5% · 1.19 GHz · 38°C)
                if (hasClock && hasTemp) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.2f GHz  \u00B7  %.0f\u00B0C",
                        gpu.usagePct, gpu.clockMHz / 1000.0, gpu.tempC);
                } else if (hasClock) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.2f GHz",
                        gpu.usagePct, gpu.clockMHz / 1000.0);
                } else if (hasTemp) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.0f\u00B0C",
                        gpu.usagePct, gpu.tempC);
                } else {
                    swprintf_s(gpuStr, L"%.0f%%", gpu.usagePct);
                }
            } else {
                // Integrated GPU (GPU 0): show <Usage>%, and append · <Clock> GHz or · <Temp>°C only when available
                if (hasClock && hasTemp) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.2f GHz  \u00B7  %.0f\u00B0C",
                        gpu.usagePct, gpu.clockMHz / 1000.0, gpu.tempC);
                } else if (hasClock) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.2f GHz",
                        gpu.usagePct, gpu.clockMHz / 1000.0);
                } else if (hasTemp) {
                    swprintf_s(gpuStr, L"%.0f%%  \u00B7  %.0f\u00B0C",
                        gpu.usagePct, gpu.tempC);
                } else {
                    swprintf_s(gpuStr, L"%.0f%%", gpu.usagePct);
                }
            }

            float labelOffset = 58.0f;
            g.DrawString(gpuStr, -1, &fontValue, Gdiplus::RectF(padX + labelOffset, curY + 4.0f, availW - labelOffset - 8.0f, 20.0f), &sfRight, &brushWhite);

            if (blockH >= 42.0f) {
                DrawProgressBar(g, padX + 8.0f, curY + blockH - 9.0f, availW - 16.0f, 4.0f, (float)gpu.usagePct, pal.gpuAccent, pal.barBackground);
            }

            curY += blockH + rowSpacing;
        }

        if (gpu.showVram) {
            DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
            DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_VRAM, pal.vramAccent);

            wchar_t vramTitle[32];
            swprintf_s(vramTitle, gpu.isDiscrete ? L"%s VRAM" : L"%s MEM", gpu.label);

            Gdiplus::Color cVram(pal.vramAccent);
            Gdiplus::SolidBrush brushVram(cVram);
            g.DrawString(vramTitle, -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushVram);

            double pct = (gpu.vramTotalMB > 0.0) ? (gpu.vramUsedMB / gpu.vramTotalMB * 100.0) : 0.0;
            wchar_t vramStr[64];
            swprintf_s(vramStr, L"%.1f / %.1f GB (%.0f%%)",
                gpu.vramUsedMB / 1024.0, gpu.vramTotalMB / 1024.0, pct);

            g.DrawString(vramStr, -1, &fontDetail, Gdiplus::RectF(padX, curY + 6.0f, availW - 8.0f, 16.0f), &sfRight, &brushWhite);

            if (blockH >= 38.0f && gpu.sharedTotalMB > 0.0) {
                wchar_t sharedStr[64];
                swprintf_s(sharedStr, L"Shared: %.1f / %.1f GB",
                    gpu.sharedUsedMB / 1024.0, gpu.sharedTotalMB / 1024.0);
                g.DrawString(sharedStr, -1, &fontDetail, Gdiplus::PointF(padX + 24.0f, curY + 21.0f), &brushDim);
            }

            if (blockH >= 38.0f) {
                DrawProgressBar(g, padX + 8.0f, curY + blockH - 9.0f, availW - 16.0f, 4.0f, (float)pct, pal.vramAccent, pal.barBackground);
            }

            curY += blockH + rowSpacing;
        }
    }

    // System RAM
    if (settings.showRam) {
        DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
        DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_RAM, pal.ramAccent);

        Gdiplus::Color cRam(pal.ramAccent);
        Gdiplus::SolidBrush brushRam(cRam);
        g.DrawString(L"RAM", -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushRam);

        double pct = (snap.ramTotalMB > 0.0) ? (snap.ramUsedMB / snap.ramTotalMB * 100.0) : 0.0;
        wchar_t ramStr[64];
        swprintf_s(ramStr, L"%.1f / %.1f GB (%.0f%%)",
            snap.ramUsedMB / 1024.0, snap.ramTotalMB / 1024.0, pct);

        g.DrawString(ramStr, -1, &fontDetail, Gdiplus::RectF(padX, curY + 6.0f, availW - 8.0f, 16.0f), &sfRight, &brushWhite);

        if (blockH >= 38.0f) {
            DrawProgressBar(g, padX + 8.0f, curY + blockH - 9.0f, availW - 16.0f, 4.0f, (float)pct, pal.ramAccent, pal.barBackground);
        }

        curY += blockH + rowSpacing;
    }

    // Power
    if (settings.showPower) {
        DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
        DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_POWER, pal.powerAccent);

        Gdiplus::Color cPwr(pal.powerAccent);
        Gdiplus::SolidBrush brushPwr(cPwr);
        g.DrawString(L"POWER", -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushPwr);

        wchar_t pwrStr[64];
        if (snap.totalPowerAvail && snap.totalPowerW > 0.0) {
            swprintf_s(pwrStr, L"%.1f W", snap.totalPowerW);
        } else if (snap.totalPowerW > 0.0) {
            swprintf_s(pwrStr, L"%.1f W", snap.totalPowerW);
        } else {
            swprintf_s(pwrStr, L"N/A");
        }
        g.DrawString(pwrStr, -1, &fontValue, Gdiplus::RectF(padX, curY + 4.0f, availW - 8.0f, 20.0f), &sfRight, &brushWhite);

        if (settings.showPowerBreakdown && snap.powerBreakdown[0] != L'\0' && blockH >= 38.0f) {
            g.DrawString(snap.powerBreakdown, -1, &fontDetail, Gdiplus::PointF(padX + 24.0f, curY + 22.0f), &brushDim);
        }

        curY += blockH + rowSpacing;
    }

    // Multi-Monitor FPS Rendering (Monitor 1 FPS, Monitor 2 FPS...)
    for (const auto& mon : snap.monitors) {
        if (mon.showFps) {
            DrawRoundedCard(g, padX, curY, availW, blockH, 8.0f, pal.cardBackground, 0);
            DrawVectorIcon(g, padX + 8.0f, curY + 7.0f, 12.0f, ICON_FPS, pal.fpsAccent);

            wchar_t fpsTitle[32];
            swprintf_s(fpsTitle, L"%s FPS", mon.label);

            Gdiplus::Color cFps(pal.fpsAccent);
            Gdiplus::SolidBrush brushFps(cFps);
            g.DrawString(fpsTitle, -1, &fontTitle, Gdiplus::PointF(padX + 24.0f, curY + 5.0f), &brushFps);

            wchar_t fpsStr[32];
            if (mon.fps > 0.0) {
                swprintf_s(fpsStr, L"%.0f FPS", mon.fps);
            } else {
                swprintf_s(fpsStr, L"0 FPS"); // Clean idle: 0 FPS when display is static
            }

            g.DrawString(fpsStr, -1, &fontValue, Gdiplus::RectF(padX, curY + 4.0f, availW - 8.0f, 20.0f), &sfRight, &brushWhite);

            curY += blockH + rowSpacing;
        }
    }
}

void Renderer::RenderNarrow(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal) {
    int activeCount = 0;
    if (settings.showNetwork) activeCount++;
    if (settings.showPing)    activeCount++;
    if (settings.showCpu)     activeCount++;

    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) activeCount++;
        if (gpu.showVram)  activeCount++;
    }

    if (settings.showRam)   activeCount++;
    if (settings.showPower) activeCount++;

    for (const auto& mon : snap.monitors) {
        if (mon.showFps) activeCount++;
    }

    if (activeCount == 0) return;

    float padX = 8.0f;
    float curY = 8.0f;
    float availW = (float)m_width - padX * 2.0f;
    float rowH = ((float)m_height - 16.0f) / (float)activeCount;
    if (rowH > 36.0f) rowH = 36.0f;

    Gdiplus::Font fontKey(L"Segoe UI", 8.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontVal(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

    Gdiplus::Color cWhite(pal.primaryText);
    Gdiplus::SolidBrush brushWhite(cWhite);

    Gdiplus::StringFormat sfRight;
    sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
    sfRight.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::StringFormat sfLeft;
    sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
    sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    auto drawRow = [&](VectorIconType icon, const wchar_t* label, const wchar_t* val, DWORD color) {
        float iconY = curY + (rowH - 12.0f) * 0.5f;
        DrawVectorIcon(g, padX + 2.0f, iconY, 12.0f, icon, color);

        Gdiplus::Color col(color);
        Gdiplus::SolidBrush bColor(col);
        Gdiplus::RectF rcLabel(padX + 18.0f, curY, availW * 0.35f, rowH);
        Gdiplus::RectF rcVal(padX + availW * 0.35f, curY, availW * 0.65f - 2.0f, rowH);

        g.DrawString(label, -1, &fontKey, rcLabel, &sfLeft, &bColor);
        g.DrawString(val, -1, &fontVal, rcVal, &sfRight, &brushWhite);
        curY += rowH;
    };

    if (settings.showNetwork) {
        wchar_t buf[32];
        if (snap.netDownKBs >= 1024.0) swprintf_s(buf, L"%.1f MB/s", snap.netDownKBs / 1024.0);
        else swprintf_s(buf, L"%.0f KB/s", snap.netDownKBs);
        drawRow(ICON_NETWORK, L"NET", buf, pal.netAccent);
    }
    if (settings.showPing) {
        wchar_t buf[32];
        DWORD c = pal.pingRed;
        if (snap.pingSuccess && snap.netPingMs >= 0.0) {
            swprintf_s(buf, L"%.0f ms", snap.netPingMs);
            c = (snap.netPingMs < 60.0) ? pal.pingGreen : (snap.netPingMs <= 120.0 ? pal.pingOrange : pal.pingRed);
        } else {
            swprintf_s(buf, L"Timeout");
        }
        drawRow(ICON_LATENCY, L"PING", buf, c);
    }
    if (settings.showCpu) {
        wchar_t buf[32];
        swprintf_s(buf, L"%.0f%%", snap.cpuUsagePct);
        drawRow(ICON_CPU, L"CPU", buf, pal.cpuAccent);
    }
    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) {
            wchar_t buf[32];
            if (gpu.clockMHz > 0.0 && gpu.tempC > 0.0) {
                swprintf_s(buf, L"%.0f%% \u00B7 %.1fG \u00B7 %.0f\u00B0C", gpu.usagePct, gpu.clockMHz / 1000.0, gpu.tempC);
            } else if (gpu.clockMHz > 0.0) {
                swprintf_s(buf, L"%.0f%% \u00B7 %.1fG", gpu.usagePct, gpu.clockMHz / 1000.0);
            } else if (gpu.tempC > 0.0) {
                swprintf_s(buf, L"%.0f%% \u00B7 %.0f\u00B0C", gpu.usagePct, gpu.tempC);
            } else {
                swprintf_s(buf, L"%.0f%%", gpu.usagePct);
            }
            drawRow(ICON_GPU, gpu.label, buf, pal.gpuAccent);
        }
        if (gpu.showVram) {
            wchar_t buf[32];
            double pct = (gpu.vramTotalMB > 0.0) ? (gpu.vramUsedMB / gpu.vramTotalMB * 100.0) : 0.0;
            swprintf_s(buf, L"%.0f%%", pct);
            wchar_t lbl[32];
            swprintf_s(lbl, gpu.isDiscrete ? L"%s V" : L"%s M", gpu.label);
            drawRow(ICON_VRAM, lbl, buf, pal.vramAccent);
        }
    }
    if (settings.showRam) {
        wchar_t buf[32];
        double pct = (snap.ramTotalMB > 0.0) ? (snap.ramUsedMB / snap.ramTotalMB * 100.0) : 0.0;
        swprintf_s(buf, L"%.0f%%", pct);
        drawRow(ICON_RAM, L"RAM", buf, pal.ramAccent);
    }
    if (settings.showPower) {
        wchar_t buf[32];
        if (snap.totalPowerW > 0.0) swprintf_s(buf, L"%.0f W", snap.totalPowerW);
        else swprintf_s(buf, L"N/A");
        drawRow(ICON_POWER, L"PWR", buf, pal.powerAccent);
    }
    for (const auto& mon : snap.monitors) {
        if (mon.showFps) {
            wchar_t buf[32];
            if (mon.fps > 0.0) swprintf_s(buf, L"%.0f", mon.fps);
            else swprintf_s(buf, L"0");
            wchar_t lbl[32];
            swprintf_s(lbl, L"M%d FPS", mon.index + 1);
            drawRow(ICON_FPS, lbl, buf, pal.fpsAccent);
        }
    }
}

void Renderer::RenderWide(Gdiplus::Graphics& g, const TelemetrySnapshot& snap, const AppSettings& settings, const ThemePalette& pal) {
    int activeCount = 0;
    if (settings.showNetwork) activeCount++;
    if (settings.showPing)    activeCount++;
    if (settings.showCpu)     activeCount++;

    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) activeCount++;
        if (gpu.showVram)  activeCount++;
    }

    if (settings.showRam)   activeCount++;
    if (settings.showPower) activeCount++;

    for (const auto& mon : snap.monitors) {
        if (mon.showFps) activeCount++;
    }

    if (activeCount == 0) return;

    float padX = 12.0f;
    float padY = 8.0f;
    float availW = (float)m_width - padX * 2.0f;
    float cardH = (float)m_height - padY * 2.0f;
    float colSpacing = 8.0f;
    float colW = (availW - (activeCount - 1) * colSpacing) / (float)activeCount;

    Gdiplus::Font fontTitle(L"Segoe UI", 7.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontValue(L"Segoe UI", 11.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fontSub(L"Segoe UI", 7.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    Gdiplus::Color cWhite(pal.primaryText);
    Gdiplus::Color cDim(pal.secondaryText);
    Gdiplus::SolidBrush brushWhite(cWhite);
    Gdiplus::SolidBrush brushDim(cDim);

    Gdiplus::StringFormat sfCenter;
    sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
    sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    float curX = padX;

    auto drawHudCard = [&](VectorIconType icon, const wchar_t* title, const wchar_t* val, const wchar_t* sub, DWORD accentColor) {
        DrawRoundedCard(g, curX, padY, colW, cardH, 8.0f, pal.cardBackground, 0);

        DrawVectorIcon(g, curX + 6.0f, padY + 5.0f, 10.0f, icon, accentColor);

        Gdiplus::Color col(accentColor);
        Gdiplus::SolidBrush bAccent(col);
        g.DrawString(title, -1, &fontTitle, Gdiplus::RectF(curX + 16.0f, padY + 4.0f, colW - 18.0f, 14.0f), &sfCenter, &bAccent);
        g.DrawString(val, -1, &fontValue, Gdiplus::RectF(curX, padY + 18.0f, colW, 20.0f), &sfCenter, &brushWhite);
        if (sub && sub[0] != L'\0') {
            g.DrawString(sub, -1, &fontSub, Gdiplus::RectF(curX, padY + 38.0f, colW, 14.0f), &sfCenter, &brushDim);
        }

        curX += colW + colSpacing;
    };

    if (settings.showNetwork) {
        wchar_t bufD[32], bufU[32];
        if (snap.netDownKBs >= 1024.0) swprintf_s(bufD, L"%.1f M", snap.netDownKBs / 1024.0);
        else swprintf_s(bufD, L"%.0f K", snap.netDownKBs);
        if (snap.netUpKBs >= 1024.0) swprintf_s(bufU, L"%.1f M", snap.netUpKBs / 1024.0);
        else swprintf_s(bufU, L"%.0f K", snap.netUpKBs);

        wchar_t sub[32];
        swprintf_s(sub, L"\u2191 %s", bufU);
        drawHudCard(ICON_NETWORK, L"NETWORK", bufD, sub, pal.netAccent);
    }
    if (settings.showPing) {
        wchar_t buf[32];
        DWORD c = pal.pingRed;
        if (snap.pingSuccess && snap.netPingMs >= 0.0) {
            swprintf_s(buf, L"%.0f ms", snap.netPingMs);
            c = (snap.netPingMs < 60.0) ? pal.pingGreen : (snap.netPingMs <= 120.0 ? pal.pingOrange : pal.pingRed);
        } else {
            swprintf_s(buf, L"Timeout");
        }
        drawHudCard(ICON_LATENCY, L"PING", buf, L"1.1.1.1", c);
    }
    if (settings.showCpu) {
        wchar_t buf[32], sub[32];
        swprintf_s(buf, L"%.0f%%", snap.cpuUsagePct);
        if (snap.cpuTempAvail && snap.cpuTempC > 0.0) swprintf_s(sub, L"%.0f\u00B0C", snap.cpuTempC);
        else swprintf_s(sub, L"%.1f GHz", snap.cpuClockMHz / 1000.0);
        drawHudCard(ICON_CPU, L"CPU", buf, sub, pal.cpuAccent);
    }
    for (const auto& gpu : snap.gpus) {
        if (gpu.showUsage) {
            wchar_t buf[32], sub[32];
            swprintf_s(buf, L"%.0f%%", gpu.usagePct);
            if (gpu.clockMHz > 0.0 && gpu.tempC > 0.0) {
                swprintf_s(sub, L"%.2fG \u00B7 %.0f\u00B0C", gpu.clockMHz / 1000.0, gpu.tempC);
            } else if (gpu.clockMHz > 0.0) {
                swprintf_s(sub, L"%.2f GHz", gpu.clockMHz / 1000.0);
            } else if (gpu.tempC > 0.0) {
                swprintf_s(sub, L"%.0f\u00B0C", gpu.tempC);
            } else {
                sub[0] = L'\0';
            }
            drawHudCard(ICON_GPU, gpu.label, buf, sub, pal.gpuAccent);
        }
        if (gpu.showVram) {
            wchar_t buf[32], sub[32];
            double pct = (gpu.vramTotalMB > 0.0) ? (gpu.vramUsedMB / gpu.vramTotalMB * 100.0) : 0.0;
            swprintf_s(buf, L"%.0f%%", pct);
            swprintf_s(sub, L"%.1f GB", gpu.vramUsedMB / 1024.0);
            wchar_t lbl[32];
            swprintf_s(lbl, gpu.isDiscrete ? L"%s V" : L"%s M", gpu.label);
            drawHudCard(ICON_VRAM, lbl, buf, sub, pal.vramAccent);
        }
    }
    if (settings.showRam) {
        wchar_t buf[32], sub[32];
        double pct = (snap.ramTotalMB > 0.0) ? (snap.ramUsedMB / snap.ramTotalMB * 100.0) : 0.0;
        swprintf_s(buf, L"%.0f%%", pct);
        swprintf_s(sub, L"%.1f GB", snap.ramUsedMB / 1024.0);
        drawHudCard(ICON_RAM, L"RAM", buf, sub, pal.ramAccent);
    }
    if (settings.showPower) {
        wchar_t buf[32];
        if (snap.totalPowerW > 0.0) swprintf_s(buf, L"%.0f W", snap.totalPowerW);
        else swprintf_s(buf, L"N/A");
        const wchar_t* sub = (settings.showPowerBreakdown && snap.powerBreakdown[0] != L'\0') ? snap.powerBreakdown : L"Total";
        drawHudCard(ICON_POWER, L"POWER", buf, sub, pal.powerAccent);
    }
    for (const auto& mon : snap.monitors) {
        if (mon.showFps) {
            wchar_t buf[32];
            if (mon.fps > 0.0) swprintf_s(buf, L"%.0f", mon.fps);
            else swprintf_s(buf, L"0");
            wchar_t lbl[32];
            swprintf_s(lbl, L"M%d FPS", mon.index + 1);
            drawHudCard(ICON_FPS, lbl, buf, mon.resolution, pal.fpsAccent);
        }
    }
}

} // namespace LV
