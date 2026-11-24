#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Universal Multi-Rail Power Aggregator
// ═══════════════════════════════════════════════════════════════
// Enumerates ALL available power sources for accurate Total System Power:
//   Tier 1: Windows EMI v1/v2 — multi-channel energy meter devices
//           (CPU Package, SoC, DRAM, System Board — OEM BIOS dependent)
//   Tier 2: MSR RAPL Package Energy (Intel + AMD) via WinRing0
//   Tier 3: NVIDIA NVML / AMD ADL2 / Intel IGCL per-GPU power
//   Tier 4: ACPI Battery discharge rate (laptops — total system draw)
//   Tier 5: PDH ACPI Power Meter objects (server/workstation PSUs)
//
// Total Power = max(Battery Discharge Rate,
//               CPU Package + Sum(Discrete GPU Power) + DRAM + Other Rails)
//
// Zero double-counting rule:
//   iGPU power is physically inside CPU Package — never added on top.
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include <setupapi.h>

namespace LV {

// Represents a single EMI energy-meter channel
struct EmiChannel {
    HANDLE    hDevice          = INVALID_HANDLE_VALUE;
    wchar_t   name[64]         = { 0 };     // Channel name from EMI_METADATA_V1
    wchar_t   hwId[128]        = { 0 };     // Hardware instance ID for classification
    ULONGLONG lastEnergyPwh    = 0;         // Picowatt-hours
    ULONGLONG lastTime100ns    = 0;         // 100-nanosecond ticks
    double    lastPowerW       = 0.0;       // Most recent computed Watts
    bool      isCpuPackage     = false;     // Classification flags
    bool      isSoC            = false;
    bool      isDram           = false;
    bool      isSystem         = false;     // System/board-level power
    bool      isGpu            = false;     // GPU rail (some OEMs expose this)
};

class PowerMonitor {
public:
    PowerMonitor();
    ~PowerMonitor();

    bool Init();
    void Shutdown();

    void Collect(TelemetrySnapshot& snapshot);

private:
    // EMI multi-channel enumeration
    void InitEmi();
    void ShutdownEmi();
    void ReadAllEmiChannels();
    void ClassifyEmiChannel(EmiChannel& ch);

    // ACPI Battery discharge rate (laptops)
    double ReadBatteryDischargeRateW();

    // State
    std::vector<EmiChannel> m_emiChannels;
    bool  m_emiAvailable     = false;
    bool  m_batteryAvail     = false;
    double m_batteryLastW    = 0.0;
};

} // namespace LV
