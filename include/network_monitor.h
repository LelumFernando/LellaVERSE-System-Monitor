#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Network Bandwidth & ICMP Ping
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    bool Init();
    void Shutdown();

    void Collect(TelemetrySnapshot& snapshot);

private:
    double PerformPing();

    uint64_t m_lastInOctets  = 0;
    uint64_t m_lastOutOctets = 0;
    LARGE_INTEGER m_lastQpc  = { 0 };
    bool m_firstSample       = true;

    // ICMP Ping Handle
    HANDLE m_hIcmp           = INVALID_HANDLE_VALUE;
};

} // namespace LV
