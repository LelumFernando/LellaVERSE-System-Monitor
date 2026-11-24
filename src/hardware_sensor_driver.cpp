#include "hardware_sensor_driver.h"
#include "winring0_driver_bytes.h"

namespace LV {

#define OLS_TYPE 40000
#define IOCTL_OLS_READ_MSR CTL_CODE(OLS_TYPE, 0x821, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Intel MSR Constants
constexpr DWORD MSR_IA32_THERM_STATUS         = 0x019C;
constexpr DWORD MSR_IA32_TEMPERATURE_TARGET   = 0x01A2;
constexpr DWORD MSR_IA32_PACKAGE_THERM_STATUS = 0x01B1;
constexpr DWORD MSR_RAPL_POWER_UNIT           = 0x0606;
constexpr DWORD MSR_PKG_ENERGY_STATUS         = 0x0611;
constexpr DWORD MSR_DRAM_ENERGY_STATUS        = 0x0618;

// AMD MSR Constants
constexpr DWORD MSR_AMD_K10_TEMP              = 0xC0010064;
constexpr DWORD MSR_AMD_RAPL_POWER_UNIT       = 0xC0010299;
constexpr DWORD MSR_AMD_PKG_ENERGY_STATUS     = 0xC001029B;

HardwareSensorDriver& HardwareSensorDriver::Get() {
    static HardwareSensorDriver instance;
    return instance;
}

bool HardwareSensorDriver::Init() {
    if (IsLoaded()) return true;

    m_serviceName = L"WinRing0_1_2_0_LV";

    // 1. Try to open existing driver device first
    m_hDriver = CreateFileW(L"\\\\.\\WinRing0_1_2_0",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_hDriver != INVALID_HANDLE_VALUE) {
        return true;
    }

    // 2. Extract and start service
    if (ExtractAndInstallDriver()) {
        if (StartDriverService()) {
            m_hDriver = CreateFileW(L"\\\\.\\WinRing0_1_2_0",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_hDriver != INVALID_HANDLE_VALUE) {
                return true;
            }
        }
    }

    return false;
}

void HardwareSensorDriver::Shutdown() {
    if (m_hDriver != INVALID_HANDLE_VALUE && m_hDriver != NULL) {
        CloseHandle(m_hDriver);
        m_hDriver = INVALID_HANDLE_VALUE;
    }
    StopDriverService();
}

bool HardwareSensorDriver::ExtractAndInstallDriver() {
    wchar_t tempPath[MAX_PATH] = { 0 };
    if (!GetTempPathW(MAX_PATH, tempPath)) return false;

    m_driverPath = std::wstring(tempPath) + L"WinRing0x64_LV.sys";

    // Write embedded bytes
    HANDLE hFile = CreateFileW(m_driverPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hFile, WINRING0_DRIVER_BYTES, (DWORD)WINRING0_DRIVER_SIZE, &bytesWritten, NULL);
    CloseHandle(hFile);

    return (ok && bytesWritten == WINRING0_DRIVER_SIZE);
}

bool HardwareSensorDriver::StartDriverService() {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, m_serviceName.c_str(), SERVICE_ALL_ACCESS);
    if (!hService) {
        // Create service
        hService = CreateServiceW(hSCM, m_serviceName.c_str(), m_serviceName.c_str(),
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL, m_driverPath.c_str(), NULL, NULL, NULL, NULL, NULL);
    }

    bool started = false;
    if (hService) {
        if (StartServiceW(hService, 0, NULL)) {
            started = true;
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_SERVICE_ALREADY_RUNNING) {
                started = true;
            }
        }
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCM);
    return started;
}

void HardwareSensorDriver::StopDriverService() {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return;

    SC_HANDLE hService = OpenServiceW(hSCM, m_serviceName.c_str(), SERVICE_ALL_ACCESS);
    if (hService) {
        SERVICE_STATUS status;
        ControlService(hService, SERVICE_CONTROL_STOP, &status);
        DeleteService(hService);
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSCM);

    if (!m_driverPath.empty()) {
        DeleteFileW(m_driverPath.c_str());
    }
}

bool HardwareSensorDriver::ReadMsr(DWORD msrIndex, uint64_t& outValue) {
    if (!IsLoaded()) return false;

    DWORD inBuf = msrIndex;
    uint32_t outBuf[2] = { 0, 0 }; // EAX (low 32), EDX (high 32)
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(m_hDriver, IOCTL_OLS_READ_MSR,
        &inBuf, sizeof(inBuf),
        outBuf, sizeof(outBuf),
        &bytesReturned, NULL);

    if (ok && bytesReturned == sizeof(outBuf)) {
        outValue = ((uint64_t)outBuf[1] << 32) | outBuf[0];
        return true;
    }
    return false;
}

double HardwareSensorDriver::ReadIntelPackageTemp() {
    if (!IsLoaded()) return -1.0;

    // 1. Get TjMax (Target Temp)
    uint64_t targetVal = 0;
    double tjMax = 100.0; // Standard default
    if (ReadMsr(MSR_IA32_TEMPERATURE_TARGET, targetVal)) {
        uint32_t raw = (uint32_t)((targetVal >> 16) & 0xFF);
        if (raw >= 60 && raw <= 115) {
            tjMax = (double)raw;
        }
    }

    // 2. Read Package Therm Status (0x1B1)
    uint64_t pkgVal = 0;
    if (ReadMsr(MSR_IA32_PACKAGE_THERM_STATUS, pkgVal)) {
        if (pkgVal & (1ULL << 31)) { // Valid reading bit
            uint32_t dts = (uint32_t)((pkgVal >> 16) & 0x7F);
            double temp = tjMax - (double)dts;
            if (temp >= 10.0 && temp <= 115.0) {
                return temp;
            }
        }
    }

    // 3. Fallback: Core 0 Therm Status (0x19C)
    uint64_t coreVal = 0;
    if (ReadMsr(MSR_IA32_THERM_STATUS, coreVal)) {
        if (coreVal & (1ULL << 31)) {
            uint32_t dts = (uint32_t)((coreVal >> 16) & 0x7F);
            double temp = tjMax - (double)dts;
            if (temp >= 10.0 && temp <= 115.0) {
                return temp;
            }
        }
    }

    return -1.0;
}

double HardwareSensorDriver::ReadIntelPackagePower() {
    if (!IsLoaded()) return -1.0;

    if (!m_raplInitialized) {
        uint64_t unitVal = 0;
        if (ReadMsr(MSR_RAPL_POWER_UNIT, unitVal)) {
            uint32_t energyUnitRaw = (uint32_t)((unitVal >> 8) & 0x1F);
            m_raplEnergyUnits = 1.0 / (double)(1ULL << energyUnitRaw); // Joules per count (typically 1/65536 J)
            m_raplInitialized = true;
        } else {
            return -1.0;
        }
    }

    uint64_t energyVal = 0;
    if (!ReadMsr(MSR_PKG_ENERGY_STATUS, energyVal)) {
        return -1.0;
    }

    LARGE_INTEGER nowQpc, freq;
    QueryPerformanceCounter(&nowQpc);
    QueryPerformanceFrequency(&freq);

    uint32_t curEnergy = (uint32_t)(energyVal & 0xFFFFFFFF);

    if (m_lastRaplQpc.QuadPart == 0) {
        m_lastPkgEnergy = curEnergy;
        m_lastRaplQpc = nowQpc;
        return -1.0; // Needs 1 delta cycle
    }

    double deltaSeconds = (double)(nowQpc.QuadPart - m_lastRaplQpc.QuadPart) / (double)freq.QuadPart;
    if (deltaSeconds <= 0.05) return -1.0;

    // Handle 32-bit roll-over
    uint32_t deltaEnergy = 0;
    if (curEnergy >= m_lastPkgEnergy) {
        deltaEnergy = curEnergy - (uint32_t)m_lastPkgEnergy;
    } else {
        deltaEnergy = (0xFFFFFFFF - (uint32_t)m_lastPkgEnergy) + curEnergy + 1;
    }

    m_lastPkgEnergy = curEnergy;
    m_lastRaplQpc = nowQpc;

    double joules = (double)deltaEnergy * m_raplEnergyUnits;
    double watts = joules / deltaSeconds;

    if (watts >= 0.0 && watts <= 500.0) {
        return watts;
    }

    return -1.0;
}

double HardwareSensorDriver::ReadIntelDramPower() {
    if (!IsLoaded()) return -1.0;

    if (!m_raplInitialized) {
        uint64_t unitVal = 0;
        if (ReadMsr(MSR_RAPL_POWER_UNIT, unitVal)) {
            uint32_t energyUnitRaw = (uint32_t)((unitVal >> 8) & 0x1F);
            m_raplEnergyUnits = 1.0 / (double)(1ULL << energyUnitRaw);
            m_raplInitialized = true;
        } else {
            return -1.0;
        }
    }

    uint64_t energyVal = 0;
    if (!ReadMsr(MSR_DRAM_ENERGY_STATUS, energyVal)) {
        return -1.0;
    }

    LARGE_INTEGER nowQpc, freq;
    QueryPerformanceCounter(&nowQpc);
    QueryPerformanceFrequency(&freq);

    uint32_t curEnergy = (uint32_t)(energyVal & 0xFFFFFFFF);

    if (m_lastDramQpc.QuadPart == 0) {
        m_lastDramEnergy = curEnergy;
        m_lastDramQpc = nowQpc;
        return -1.0;
    }

    double deltaSeconds = (double)(nowQpc.QuadPart - m_lastDramQpc.QuadPart) / (double)freq.QuadPart;
    if (deltaSeconds <= 0.05) return -1.0;

    uint32_t deltaEnergy = 0;
    if (curEnergy >= m_lastDramEnergy) {
        deltaEnergy = curEnergy - (uint32_t)m_lastDramEnergy;
    } else {
        deltaEnergy = (0xFFFFFFFF - (uint32_t)m_lastDramEnergy) + curEnergy + 1;
    }

    m_lastDramEnergy = curEnergy;
    m_lastDramQpc = nowQpc;

    double joules = (double)deltaEnergy * m_raplEnergyUnits;
    double watts = joules / deltaSeconds;

    if (watts >= 0.0 && watts <= 150.0) {
        return watts;
    }

    return -1.0;
}

double HardwareSensorDriver::ReadAmdPackageTemp() {
    if (!IsLoaded()) return -1.0;

    // AMD Family 10h+ / Zen MSR temperature
    uint64_t tempVal = 0;
    if (ReadMsr(MSR_AMD_K10_TEMP, tempVal)) {
        // Bits [31:21] indicate raw temperature in 0.125 C steps
        uint32_t raw = (uint32_t)((tempVal >> 21) & 0x7FF);
        double temp = (double)raw * 0.125;
        if (temp >= 10.0 && temp <= 115.0) {
            return temp;
        }
    }
    return -1.0;
}

double HardwareSensorDriver::ReadAmdPackagePower() {
    if (!IsLoaded()) return -1.0;

    uint64_t energyVal = 0;
    if (!ReadMsr(MSR_AMD_PKG_ENERGY_STATUS, energyVal)) {
        return -1.0;
    }

    LARGE_INTEGER nowQpc, freq;
    QueryPerformanceCounter(&nowQpc);
    QueryPerformanceFrequency(&freq);

    uint32_t curEnergy = (uint32_t)(energyVal & 0xFFFFFFFF);

    if (m_lastAmdQpc.QuadPart == 0) {
        m_lastAmdEnergy = curEnergy;
        m_lastAmdQpc = nowQpc;
        return -1.0;
    }

    double deltaSeconds = (double)(nowQpc.QuadPart - m_lastAmdQpc.QuadPart) / (double)freq.QuadPart;
    if (deltaSeconds <= 0.05) return -1.0;

    uint32_t deltaEnergy = (curEnergy >= m_lastAmdEnergy)
        ? (curEnergy - (uint32_t)m_lastAmdEnergy)
        : ((0xFFFFFFFF - (uint32_t)m_lastAmdEnergy) + curEnergy + 1);

    m_lastAmdEnergy = curEnergy;
    m_lastAmdQpc = nowQpc;

    // AMD energy unit is typically ~15.3 microjoules (1 / 65536 J default)
    double joules = (double)deltaEnergy / 65536.0;
    double watts = joules / deltaSeconds;

    if (watts >= 0.0 && watts <= 500.0) {
        return watts;
    }

    return -1.0;
}

} // namespace LV
