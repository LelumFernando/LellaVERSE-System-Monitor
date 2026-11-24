#include "monitor_manager.h"

namespace LV {

MonitorManager& MonitorManager::Get() {
    static MonitorManager instance;
    return instance;
}

BOOL CALLBACK MonitorManager::MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(lp);

    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        MonitorInfo info = {};
        info.hMon      = hMon;
        info.rcWork    = mi.rcWork;
        info.rcMonitor = mi.rcMonitor;
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.index     = (int)monitors->size();
        wcsncpy_s(info.name, mi.szDevice, _TRUNCATE);
        monitors->push_back(info);
    }
    return TRUE;
}

void MonitorManager::RefreshMonitors() {
    m_monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&m_monitors));

    // Fallback if none enumerated
    if (m_monitors.empty()) {
        MonitorInfo info = {};
        info.hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(info.hMon, &mi)) {
            info.rcWork    = mi.rcWork;
            info.rcMonitor = mi.rcMonitor;
            info.isPrimary = true;
            info.index     = 0;
            wcsncpy_s(info.name, L"Primary Display", _TRUNCATE);
            m_monitors.push_back(info);
        }
    }
}

int MonitorManager::GetPrimaryIndex() const {
    for (size_t i = 0; i < m_monitors.size(); ++i) {
        if (m_monitors[i].isPrimary) return (int)i;
    }
    return 0;
}

int MonitorManager::GetSecondaryIndex() const {
    // If dual-monitor setup exists, return the first non-primary monitor
    for (size_t i = 0; i < m_monitors.size(); ++i) {
        if (!m_monitors[i].isPrimary) return (int)i;
    }
    return GetPrimaryIndex();
}

const MonitorInfo* MonitorManager::GetMonitor(int index) const {
    if (index >= 0 && index < (int)m_monitors.size()) {
        return &m_monitors[index];
    }
    int p = GetPrimaryIndex();
    if (p >= 0 && p < (int)m_monitors.size()) {
        return &m_monitors[p];
    }
    return nullptr;
}

POINT MonitorManager::GetDefaultPosition(int monitorIndex, int windowWidth, int windowHeight, int marginX, int marginY) const {
    const MonitorInfo* mon = GetMonitor(monitorIndex);
    if (!mon) {
        return POINT{ 100, 100 };
    }

    // Top-right corner of working area
    int x = mon->rcWork.right - windowWidth - marginX;
    int y = mon->rcWork.top + marginY;
    return POINT{ x, y };
}

void MonitorManager::ClampToMonitor(int monitorIndex, int& x, int& y, int width, int height) const {
    const MonitorInfo* mon = GetMonitor(monitorIndex);
    if (!mon) return;

    if (x + width > mon->rcWork.right) {
        x = mon->rcWork.right - width;
    }
    if (x < mon->rcWork.left) {
        x = mon->rcWork.left;
    }
    if (y + height > mon->rcWork.bottom) {
        y = mon->rcWork.bottom - height;
    }
    if (y < mon->rcWork.top) {
        y = mon->rcWork.top;
    }
}

} // namespace LV
