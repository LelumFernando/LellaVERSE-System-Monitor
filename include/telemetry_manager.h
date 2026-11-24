#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Telemetry Manager
// Double-buffered lock-free background polling engine
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include "network_monitor.h"
#include "cpu_monitor.h"
#include "gpu_monitor.h"
#include "memory_monitor.h"
#include "power_monitor.h"
#include "fps_monitor.h"

namespace LV {

class TelemetryManager {
public:
    static TelemetryManager& Get();

    bool Init();
    void Shutdown();

    // Lock-free double-buffered read
    TelemetrySnapshot GetSnapshot() const;

    void SetRefreshInterval(int ms);
    void NotifyTogglesChanged();

private:
    TelemetryManager() = default;

    void WorkerThread();
    void CollectAll();

    NetworkMonitor m_network;
    CpuMonitor     m_cpu;
    GpuMonitor     m_gpu;
    MemoryMonitor  m_memory;
    PowerMonitor   m_power;
    FpsMonitor     m_fps;

    TelemetrySnapshot m_snapshots[2];
    std::atomic<int>  m_readIndex{ 0 };
    std::mutex        m_writeMutex;

    std::thread       m_thread;
    HANDLE            m_shutdownEvent = NULL;
    HANDLE            m_timer         = NULL;
    std::atomic<bool> m_running{ false };
    std::atomic<int>  m_intervalMs{ 1000 };
};

} // namespace LV
