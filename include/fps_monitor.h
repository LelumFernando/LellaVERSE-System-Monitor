#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Multi-Monitor FPS Engine
// DXGI Desktop Duplication (IDXGIOutput1) + DWM Composition Timing
// Per-Monitor Real-Time FPS (Monitor 1, Monitor 2, etc.)
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

struct ActiveMonitorOutput {
    int            index = 0;
    wchar_t        label[16] = { 0 };      // e.g. "Monitor 1"
    wchar_t        deviceName[32] = { 0 }; // e.g. "\\.\DISPLAY1"
    wchar_t        resolution[32] = { 0 }; // e.g. "1920x1080"
    bool           isPrimary = false;
    HMONITOR       hMonitor = NULL;
    RECT           desktopCoordinates = {};
    IDXGIAdapter1* pAdapter = nullptr;
    IDXGIOutput*   pOutput = nullptr;

    // Worker thread & control
    std::thread    thread;
    std::atomic<bool> running{ false };
    std::atomic<bool> enabled{ true };

    // Timestamp ring buffer
    static constexpr int MAX_TS = 512;
    int64_t        timestamps[MAX_TS] = {};
    int            head = 0;
    int            count = 0;
    std::mutex     mutex;

    std::atomic<double> currentFps{ 0.0 };
};

class FpsMonitor {
public:
    static FpsMonitor& Get();
    FpsMonitor() = default;
    ~FpsMonitor() { Shutdown(); }

    bool Init();
    void Shutdown();

    // Collect real-time FPS for all monitors
    void Collect(std::vector<MonitorFpsInfo>& outMonitors);

    // Refresh enabled states from settings
    void UpdateToggles();

private:
    void EnumerateActiveOutputs();
    void MonitorWorkerThread(ActiveMonitorOutput* pMon);

    std::vector<std::unique_ptr<ActiveMonitorOutput>> m_monitors;
    std::atomic<bool> m_running{ false };
    LARGE_INTEGER     m_qpcFreq = {};
};

} // namespace LV
