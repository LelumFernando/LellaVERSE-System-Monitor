#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Universal CPU Monitor
// Supports Intel Core, AMD Ryzen, and all x64 processors
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class CpuMonitor {
public:
    CpuMonitor();
    ~CpuMonitor();

    bool Init();
    void Shutdown();

    // Collect telemetry snapshot
    void Collect(TelemetrySnapshot& snapshot);

private:
    void DetectCpuInfo();

    PDH_HQUERY   m_hQuery       = NULL;
    PDH_HCOUNTER m_hCounter     = NULL; // % Processor Utility or % Processor Time
    PDH_HCOUNTER m_hPerfCounter = NULL; // \Processor Information(_Total)\% Processor Performance
    bool         m_pdhValid     = false;

    // CPU identity & topology
    wchar_t m_cpuName[64]    = { 0 };
    bool    m_isAmd          = false;
    int     m_physicalCores  = 1;
    int     m_logicalThreads = 1;
    double  m_baseClockMHz   = 2400.0;
};

} // namespace LV
