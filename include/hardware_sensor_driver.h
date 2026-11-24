#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Hardware Sensor Kernel Driver
// Direct MSR access for Intel Core & AMD Ryzen processors
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class HardwareSensorDriver {
public:
    static HardwareSensorDriver& Get();

    bool Init();
    void Shutdown();
    bool IsLoaded() const { return m_hDriver != INVALID_HANDLE_VALUE && m_hDriver != NULL; }

    // Read Model-Specific Register (MSR)
    bool ReadMsr(DWORD msrIndex, uint64_t& outValue);

    // Intel Temperature & Power
    double ReadIntelPackageTemp();
    double ReadIntelPackagePower();
    double ReadIntelDramPower();

    // AMD Ryzen Temperature & Power
    double ReadAmdPackageTemp();
    double ReadAmdPackagePower();

private:
    HardwareSensorDriver() = default;

    bool ExtractAndInstallDriver();
    bool StartDriverService();
    void StopDriverService();

    HANDLE m_hDriver = INVALID_HANDLE_VALUE;
    std::wstring m_driverPath;
    std::wstring m_serviceName;

    // RAPL tracking
    uint64_t m_lastPkgEnergy = 0;
    LARGE_INTEGER m_lastRaplQpc = { 0 };
    uint64_t m_lastDramEnergy = 0;
    LARGE_INTEGER m_lastDramQpc = { 0 };
    double m_raplEnergyUnits = 0.0;
    bool m_raplInitialized = false;

    // AMD tracking
    uint64_t m_lastAmdEnergy = 0;
    LARGE_INTEGER m_lastAmdQpc = { 0 };
    bool m_amdInitialized = false;
};

} // namespace LV
