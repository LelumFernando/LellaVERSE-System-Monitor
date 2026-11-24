#include "telemetry_manager.h"
#include "hardware_sensor_driver.h"

namespace LV {

TelemetryManager& TelemetryManager::Get() {
    static TelemetryManager instance;
    return instance;
}

bool TelemetryManager::Init() {
    if (m_running) return true;

    // 1. Initialize sensor modules
    HardwareSensorDriver::Get().Init();
    m_network.Init();
    m_cpu.Init();
    m_gpu.Init();
    m_power.Init();
    m_fps.Init();

    // 2. Setup synchronization events
    m_shutdownEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    m_timer = CreateWaitableTimerW(NULL, FALSE, NULL);

    // Initial collect before UI displays
    CollectAll();

    // 3. Launch background worker thread
    m_running = true;
    m_thread = std::thread(&TelemetryManager::WorkerThread, this);

    return true;
}

void TelemetryManager::Shutdown() {
    if (!m_running) return;

    m_running = false;
    if (m_shutdownEvent) {
        SetEvent(m_shutdownEvent);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_timer) {
        CloseHandle(m_timer);
        m_timer = NULL;
    }
    if (m_shutdownEvent) {
        CloseHandle(m_shutdownEvent);
        m_shutdownEvent = NULL;
    }

    m_fps.Shutdown();
    m_power.Shutdown();
    m_gpu.Shutdown();
    m_cpu.Shutdown();
    m_network.Shutdown();
    HardwareSensorDriver::Get().Shutdown();
}

void TelemetryManager::SetRefreshInterval(int ms) {
    if (ms < 200) ms = 200;
    if (ms > 5000) ms = 5000;
    m_intervalMs.store(ms);
}

void TelemetryManager::NotifyTogglesChanged() {
    m_fps.UpdateToggles();
}

TelemetrySnapshot TelemetryManager::GetSnapshot() const {
    int idx = m_readIndex.load(std::memory_order_acquire);
    return m_snapshots[idx];
}

void TelemetryManager::CollectAll() {
    int readIdx = m_readIndex.load(std::memory_order_relaxed);
    int writeIdx = 1 - readIdx;

    TelemetrySnapshot& snap = m_snapshots[writeIdx];

    // Collect from each module
    m_network.Collect(snap);
    m_cpu.Collect(snap);
    m_memory.Collect(snap);
    m_gpu.Collect(snap.gpus);
    m_power.Collect(snap);
    m_fps.Collect(snap.monitors);

    // Swap read buffer atomically
    m_readIndex.store(writeIdx, std::memory_order_release);
}

void TelemetryManager::WorkerThread() {
    while (m_running) {
        int interval = m_intervalMs.load();

        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -10000LL * interval; // Negative = relative 100ns units
        SetWaitableTimer(m_timer, &dueTime, 0, NULL, NULL, FALSE);

        HANDLE handles[2] = { m_shutdownEvent, m_timer };
        DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitRes == WAIT_OBJECT_0) {
            // Shutdown signaled
            break;
        }

        if (waitRes == WAIT_OBJECT_0 + 1) {
            // Timer expired, collect telemetry
            CollectAll();
        }
    }
}

} // namespace LV
