#include "cpu_monitor.h"
#include "hardware_sensor_driver.h"

namespace LV {

struct PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
};

static double EstimateCpuTdpWatts(const wchar_t* name, int threads, bool isAmd) {
    if (!name) return 65.0;
    if (wcsstr(name, L"i5-6500") || wcsstr(name, L"i5-6400") || wcsstr(name, L"i5-6600")) return 65.0;
    if (wcsstr(name, L"1135G7") || wcsstr(name, L"1165G7") || wcsstr(name, L"1065G7")) return 28.0;
    if (wcsstr(name, L"G7") || wcsstr(name, L"G4") || wcsstr(name, L"G1")) return 25.0;

    const wchar_t* pU = wcsstr(name, L"U");
    if (pU && (pU[1] == L' ' || pU[1] == L'\0' || pU[1] == L'@')) return 15.0;

    const wchar_t* pH = wcsstr(name, L"H");
    if (pH && (pH[1] == L' ' || pH[1] == L'S' || pH[1] == L'X' || pH[1] == L'\0')) return 45.0;

    if (wcsstr(name, L"i9") || wcsstr(name, L"Ryzen 9") || threads >= 24) return 125.0;
    if (wcsstr(name, L"i7") || wcsstr(name, L"Ryzen 7") || threads >= 16) return 95.0;
    if (wcsstr(name, L"i5") || wcsstr(name, L"Ryzen 5") || threads >= 8)  return 65.0;
    if (wcsstr(name, L"i3") || wcsstr(name, L"Ryzen 3")) return 50.0;
    if (threads <= 4) return 35.0;

    return 65.0;
}

CpuMonitor::CpuMonitor() {
    DetectCpuInfo();
}

CpuMonitor::~CpuMonitor() {
    Shutdown();
}

void CpuMonitor::DetectCpuInfo() {
    int cpuInfo[4] = { 0 };

    // 1. Detect Vendor
    __cpuid(cpuInfo, 0);
    char vendor[13] = { 0 };
    memcpy(vendor, &cpuInfo[1], 4);     // EBX
    memcpy(vendor + 4, &cpuInfo[3], 4); // EDX
    memcpy(vendor + 8, &cpuInfo[2], 4); // ECX
    vendor[12] = '\0';

    if (strcmp(vendor, "AuthenticAMD") == 0) {
        m_isAmd = true;
    } else {
        m_isAmd = false; // Default / Intel
    }

    // 2. Detect CPU Brand String
    __cpuid(cpuInfo, 0x80000000);
    unsigned int maxExt = (unsigned int)cpuInfo[0];

    char brand[49] = { 0 };
    if (maxExt >= 0x80000004) {
        __cpuid((int*)(brand), 0x80000002);
        __cpuid((int*)(brand + 16), 0x80000003);
        __cpuid((int*)(brand + 32), 0x80000004);
        brand[48] = '\0';

        // Trim leading spaces
        char* p = brand;
        while (*p == ' ') p++;

        MultiByteToWideChar(CP_UTF8, 0, p, -1, m_cpuName, 64);
    } else {
        wcsncpy_s(m_cpuName, m_isAmd ? L"AMD Ryzen Processor" : L"Intel Core Processor", _TRUNCATE);
    }

    // 3. Detect Topology (Physical Cores vs Logical Threads)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_logicalThreads = (int)si.dwNumberOfProcessors;
    if (m_logicalThreads < 1) m_logicalThreads = 1;

    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &length);
    if (length > 0) {
        std::vector<BYTE> buffer(length);
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer.data(), &length)) {
            DWORD offset = 0;
            int cores = 0;
            while (offset < length) {
                auto* info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buffer.data() + offset);
                if (info->Relationship == RelationProcessorCore) {
                    cores++;
                }
                offset += info->Size;
            }
            if (cores > 0) m_physicalCores = cores;
        }
    }
    if (m_physicalCores < 1) m_physicalCores = m_logicalThreads;

    // 4. Detect Nominal Base Clock (MHz) from Registry
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD mhz = 0;
        DWORD cb = sizeof(mhz);
        if (RegQueryValueExW(hKey, L"~MHz", NULL, NULL, (LPBYTE)&mhz, &cb) == ERROR_SUCCESS && mhz > 0) {
            m_baseClockMHz = (double)mhz;
        }
        RegCloseKey(hKey);
    }

    // Fallback parsing from brand string if ~MHz registry key is absent
    if (m_baseClockMHz <= 0.0) {
        const wchar_t* pAt = wcsstr(m_cpuName, L"@");
        if (pAt) {
            double ghz = 0.0;
            if (swscanf_s(pAt + 1, L"%lfGHz", &ghz) == 1 && ghz > 0.0) {
                m_baseClockMHz = ghz * 1000.0;
            }
        }
    }
    if (m_baseClockMHz <= 0.0) {
        m_baseClockMHz = 2400.0;
    }
}

bool CpuMonitor::Init() {
    if (m_pdhValid) return true;

    PDH_STATUS status = PdhOpenQueryW(NULL, 0, &m_hQuery);
    if (status != ERROR_SUCCESS) return false;

    // 1. Processor Utilization Counter
    status = PdhAddEnglishCounterW(m_hQuery, L"\\Processor Information(_Total)\\% Processor Utility", 0, &m_hCounter);
    if (status != ERROR_SUCCESS) {
        status = PdhAddEnglishCounterW(m_hQuery, L"\\Processor(_Total)\\% Processor Time", 0, &m_hCounter);
    }

    // 2. Processor Performance Counter (for Dynamic Turbo Boost / AMD Precision Boost effective clock)
    PdhAddEnglishCounterW(m_hQuery, L"\\Processor Information(_Total)\\% Processor Performance", 0, &m_hPerfCounter);

    if (status == ERROR_SUCCESS) {
        PdhCollectQueryData(m_hQuery); // Initial seed sample
        m_pdhValid = true;
    } else {
        PdhCloseQuery(m_hQuery);
        m_hQuery = NULL;
        m_hCounter = NULL;
        m_hPerfCounter = NULL;
        m_pdhValid = false;
    }

    return m_pdhValid;
}

void CpuMonitor::Shutdown() {
    if (m_hQuery) {
        PdhCloseQuery(m_hQuery);
        m_hQuery = NULL;
        m_hCounter = NULL;
        m_hPerfCounter = NULL;
    }
    m_pdhValid = false;
}

void CpuMonitor::Collect(TelemetrySnapshot& snapshot) {
    wcsncpy_s(snapshot.cpuName, m_cpuName, _TRUNCATE);
    snapshot.isAmdCpu   = m_isAmd;
    snapshot.cpuCores   = m_physicalCores;
    snapshot.cpuThreads = m_logicalThreads;

    bool clockCollected = false;

    // 1. CPU Usage % & Dynamic Boost Clock via PDH
    if (m_pdhValid) {
        PDH_STATUS status = PdhCollectQueryData(m_hQuery);
        if (status == ERROR_SUCCESS) {
            // Utilization
            if (m_hCounter) {
                PDH_FMT_COUNTERVALUE val;
                if (PdhGetFormattedCounterValue(m_hCounter, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS &&
                    (val.CStatus == PDH_CSTATUS_VALID_DATA || val.CStatus == PDH_CSTATUS_NEW_DATA)) {
                    snapshot.cpuUsagePct = std::clamp(val.doubleValue, 0.0, 100.0);
                }
            }

            // Real-time Turbo Boost / Precision Boost Clock (scaled by nominal base clock)
            if (m_hPerfCounter) {
                PDH_FMT_COUNTERVALUE valPerf;
                if (PdhGetFormattedCounterValue(m_hPerfCounter, PDH_FMT_DOUBLE, NULL, &valPerf) == ERROR_SUCCESS &&
                    (valPerf.CStatus == PDH_CSTATUS_VALID_DATA || valPerf.CStatus == PDH_CSTATUS_NEW_DATA) &&
                    valPerf.doubleValue > 0.0) {
                    snapshot.cpuClockMHz = m_baseClockMHz * (valPerf.doubleValue / 100.0);
                    clockCollected = true;
                }
            }
        }
    } else {
        Init();
    }

    // 2. Fallback CPU Clock Speed via CallNtPowerInformation if PDH Perf counter unavailable
    if (!clockCollected) {
        std::vector<PROCESSOR_POWER_INFORMATION> ppi(m_logicalThreads);
        NTSTATUS ntStatus = CallNtPowerInformation(ProcessorInformation, NULL, 0, ppi.data(), (ULONG)(sizeof(PROCESSOR_POWER_INFORMATION) * m_logicalThreads));
        if (ntStatus == 0 && !ppi.empty()) {
            double totalMhz = 0.0;
            int validCores = 0;
            for (const auto& core : ppi) {
                if (core.CurrentMhz > 0) {
                    totalMhz += (double)core.CurrentMhz;
                    validCores++;
                }
            }
            if (validCores > 0) {
                snapshot.cpuClockMHz = totalMhz / (double)validCores;
            } else if (ppi[0].MaxMhz > 0) {
                snapshot.cpuClockMHz = (double)ppi[0].MaxMhz;
            } else {
                snapshot.cpuClockMHz = m_baseClockMHz;
            }
        } else {
            snapshot.cpuClockMHz = m_baseClockMHz;
        }
    }

    // 3. CPU Temperature & Package Power via Hardware Sensor Driver (MSR RAPL)
    auto& driver = HardwareSensorDriver::Get();
    if (driver.IsLoaded()) {
        if (m_isAmd) {
            double temp = driver.ReadAmdPackageTemp();
            if (temp > 0.0) {
                snapshot.cpuTempC = temp;
                snapshot.cpuTempAvail = true;
            }
            double power = driver.ReadAmdPackagePower();
            if (power >= 0.0) {
                snapshot.cpuPowerW = power;
                snapshot.cpuPowerAvail = true;
            }
        } else {
            double temp = driver.ReadIntelPackageTemp();
            if (temp > 0.0) {
                snapshot.cpuTempC = temp;
                snapshot.cpuTempAvail = true;
            }
            double power = driver.ReadIntelPackagePower();
            if (power >= 0.0) {
                snapshot.cpuPowerW = power;
                snapshot.cpuPowerAvail = true;
            }
            double dram = driver.ReadIntelDramPower();
            if (dram > 0.0) {
                snapshot.dramPowerW = dram;
            }
        }
    } else {
        snapshot.cpuTempC = -1.0;
        snapshot.cpuTempAvail = false;
    }

    // 4. Universal Calibrated CPU Package Power Fallback
    // If MSR RAPL is unavailable (non-admin or virtualized), estimate package power from real-time load, clock & TDP
    if (!snapshot.cpuPowerAvail || snapshot.cpuPowerW <= 0.0) {
        double tdp = EstimateCpuTdpWatts(snapshot.cpuName, m_logicalThreads, m_isAmd);
        double idleW = (tdp <= 30.0) ? 2.5 : (tdp <= 65.0 ? 7.0 : 12.0);
        double loadRatio = std::clamp(snapshot.cpuUsagePct / 100.0, 0.0, 1.0);
        double clockRatio = 1.0;
        if (m_baseClockMHz > 0 && snapshot.cpuClockMHz > 0) {
            clockRatio = std::clamp(snapshot.cpuClockMHz / (double)m_baseClockMHz, 0.5, 1.6);
        }
        double estW = idleW + (tdp - idleW) * std::pow(loadRatio, 1.05) * clockRatio;
        if (estW > 0.0 && estW < 500.0) {
            snapshot.cpuPowerW = estW;
            snapshot.cpuPowerAvail = true;
        }
    }
}

} // namespace LV
