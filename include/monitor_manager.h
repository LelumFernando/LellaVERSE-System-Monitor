#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Multi-Monitor Manager
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class MonitorManager {
public:
    static MonitorManager& Get();

    void RefreshMonitors();
    const std::vector<MonitorInfo>& GetMonitors() const { return m_monitors; }

    int GetPrimaryIndex() const;
    int GetSecondaryIndex() const; // Returns secondary monitor index, or primary if single-mon

    const MonitorInfo* GetMonitor(int index) const;

    // Calculate default top-right corner placement with margins
    POINT GetDefaultPosition(int monitorIndex, int windowWidth, int windowHeight, int marginX = 24, int marginY = 24) const;

    // Ensure given rect is kept within the monitor bounds
    void ClampToMonitor(int monitorIndex, int& x, int& y, int width, int height) const;

private:
    MonitorManager() = default;
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT rc, LPARAM lp);

    std::vector<MonitorInfo> m_monitors;
};

} // namespace LV
