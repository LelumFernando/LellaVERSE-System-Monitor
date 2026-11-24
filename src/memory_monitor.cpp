#include "memory_monitor.h"

namespace LV {

void MemoryMonitor::Collect(TelemetrySnapshot& snapshot) {
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);

    if (GlobalMemoryStatusEx(&memStatus)) {
        snapshot.ramTotalMB = (double)memStatus.ullTotalPhys / (1024.0 * 1024.0);
        snapshot.ramUsedMB  = (double)(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0);
    }
}

} // namespace LV
