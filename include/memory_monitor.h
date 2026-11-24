#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Physical Memory Monitor
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class MemoryMonitor {
public:
    MemoryMonitor() = default;
    ~MemoryMonitor() = default;

    void Collect(TelemetrySnapshot& snapshot);
};

} // namespace LV
