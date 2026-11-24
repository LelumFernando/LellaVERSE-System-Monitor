#include "power_monitor.h"

namespace LV {

// ═══════════════════════════════════════════════════════════════
// Windows Energy Metering Interface (EMI) v1/v2
// ═══════════════════════════════════════════════════════════════
// GUID {45bd8344-7ed6-49cf-a440-c276c933b053}
static const GUID GUID_DEVICE_ENERGY_METER =
    { 0x45bd8344, 0x7ed6, 0x49cf, { 0xa4, 0x40, 0xc2, 0x76, 0xc9, 0x33, 0xb0, 0x53 } };

#define IOCTL_EMI_GET_MEASUREMENT \
    CTL_CODE(FILE_DEVICE_BATTERY, 0x0004, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_EMI_GET_METADATA \
    CTL_CODE(FILE_DEVICE_BATTERY, 0x0002, METHOD_BUFFERED, FILE_READ_ACCESS)

// EMI_MEASUREMENT_DATA_V1 (Windows 10 1709+)
struct EMI_MEASUREMENT_DATA {
    ULONGLONG AbsoluteEnergy; // Picowatt-hours (pWh)
    ULONGLONG AbsoluteTime;   // 100-nanosecond intervals since boot
};

// EMI_METADATA_V1 — describes each channel (not all systems populate this)
struct EMI_METADATA_V1 {
    ULONG  MeasurementUnitBitmap; // 0x1 = pWh
    WCHAR  HardwareOEM[16];
    WCHAR  HardwareModel[16];
    WCHAR  HardwareRevision[16];
    ULONG  MeteredHardwareCount;  // Number of channels (usually 1 per device)
    WCHAR  Description[1];       // Variable-length channel description
};

PowerMonitor::PowerMonitor() {
    Init();
}

PowerMonitor::~PowerMonitor() {
    Shutdown();
}

bool PowerMonitor::Init() {
    InitEmi();
    // Probe battery availability
    SYSTEM_POWER_STATUS sps = {};
    if (GetSystemPowerStatus(&sps)) {
        // BatteryFlag != 128 (no battery) && != 255 (unknown)
        m_batteryAvail = (sps.BatteryFlag != 128 && sps.BatteryFlag != 255);
    }
    return true;
}

void PowerMonitor::Shutdown() {
    ShutdownEmi();
}

// ─────────────────────────────────────────────────────────────
// EMI: Enumerate ALL energy meter device interfaces
// Many modern Intel/AMD laptops expose multiple EMI channels:
//   - CPU Package (Intel RAPL domain)
//   - SoC / Uncore
//   - DRAM
//   - System / Platform (total board power)
// On desktop PCs these may not be available.
// ─────────────────────────────────────────────────────────────
void PowerMonitor::InitEmi() {
    if (m_emiAvailable) return;

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        &GUID_DEVICE_ENERGY_METER,
        NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    if (hDevInfo == INVALID_HANDLE_VALUE) return;

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVICE_ENERGY_METER, i, &ifData); ++i) {
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, NULL, 0, &reqSize, NULL);
        if (reqSize == 0) continue;

        std::vector<BYTE> detailBuf(reqSize);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devData = {};
        devData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, detail, reqSize, NULL, &devData))
            continue;

        HANDLE hDevice = CreateFileW(
            detail->DevicePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL
        );
        if (hDevice == INVALID_HANDLE_VALUE) continue;

        // Verify this device responds to EMI_GET_MEASUREMENT
        EMI_MEASUREMENT_DATA mData = {};
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(hDevice, IOCTL_EMI_GET_MEASUREMENT, NULL, 0,
            &mData, sizeof(mData), &bytesReturned, NULL)) {
            CloseHandle(hDevice);
            continue;
        }

        EmiChannel ch;
        ch.hDevice = hDevice;
        ch.lastEnergyPwh = mData.AbsoluteEnergy;
        ch.lastTime100ns = mData.AbsoluteTime;

        // Try to get channel metadata / description
        BYTE metaBuf[512] = {};
        DWORD metaBytes = 0;
        if (DeviceIoControl(hDevice, IOCTL_EMI_GET_METADATA, NULL, 0,
            metaBuf, sizeof(metaBuf), &metaBytes, NULL) && metaBytes >= sizeof(EMI_METADATA_V1)) {
            auto* meta = reinterpret_cast<EMI_METADATA_V1*>(metaBuf);
            if (meta->Description[0] != L'\0') {
                wcsncpy_s(ch.name, meta->Description, _TRUNCATE);
            }
        }

        // Get Hardware Instance ID for classification
        wchar_t instanceId[128] = {};
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devData, instanceId, 128, NULL)) {
            wcsncpy_s(ch.hwId, instanceId, _TRUNCATE);
        }

        // Also use device path for classification if name is empty
        if (ch.name[0] == L'\0') {
            // Use part of device path as name hint
            const wchar_t* pathStr = detail->DevicePath;
            if (pathStr) {
                // Extract meaningful portion
                wcsncpy_s(ch.name, L"EMI Channel", _TRUNCATE);
            }
        }

        ClassifyEmiChannel(ch);
        m_emiChannels.push_back(ch);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    m_emiAvailable = !m_emiChannels.empty();
}

void PowerMonitor::ClassifyEmiChannel(EmiChannel& ch) {
    // Classify by name or hardware ID keywords
    // Common EMI channel names from Intel/AMD BIOS implementations:
    //   "RAPL_Package0", "CPU Package", "CPU", "Package"
    //   "RAPL_DRAM0", "DRAM", "Memory"
    //   "SoC", "Uncore", "GT" (Intel graphics)
    //   "PSU", "System", "Platform", "Board"
    auto containsI = [](const wchar_t* haystack, const wchar_t* needle) -> bool {
        if (!haystack || !needle) return false;
        std::wstring h(haystack), n(needle);
        for (auto& c : h) c = towlower(c);
        for (auto& c : n) c = towlower(c);
        return h.find(n) != std::wstring::npos;
    };

    const wchar_t* src = (ch.name[0] != L'\0') ? ch.name : ch.hwId;

    if (containsI(src, L"Package") || containsI(src, L"CPU") ||
        containsI(src, L"RAPL_Package") || containsI(src, L"Processor") ||
        containsI(src, L"Core")) {
        ch.isCpuPackage = true;
    }
    else if (containsI(src, L"DRAM") || containsI(src, L"Memory") ||
             containsI(src, L"RAPL_DRAM")) {
        ch.isDram = true;
    }
    else if (containsI(src, L"SoC") || containsI(src, L"Uncore")) {
        ch.isSoC = true;
    }
    else if (containsI(src, L"GPU") || containsI(src, L"GT") ||
             containsI(src, L"Graphics")) {
        ch.isGpu = true;
    }
    else if (containsI(src, L"System") || containsI(src, L"Platform") ||
             containsI(src, L"Board") || containsI(src, L"PSU") ||
             containsI(src, L"Total")) {
        ch.isSystem = true;
    }
    else {
        // If only 1 EMI channel and it's unclassified, assume it's total system power
        // (many single-channel EMI implementations report total platform power)
        // This will be re-evaluated after all channels are enumerated.
    }
}

void PowerMonitor::ShutdownEmi() {
    for (auto& ch : m_emiChannels) {
        if (ch.hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(ch.hDevice);
            ch.hDevice = INVALID_HANDLE_VALUE;
        }
    }
    m_emiChannels.clear();
    m_emiAvailable = false;
}

void PowerMonitor::ReadAllEmiChannels() {
    for (auto& ch : m_emiChannels) {
        if (ch.hDevice == INVALID_HANDLE_VALUE) continue;

        EMI_MEASUREMENT_DATA data = {};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(ch.hDevice, IOCTL_EMI_GET_MEASUREMENT, NULL, 0,
            &data, sizeof(data), &bytesReturned, NULL)) {
            if (data.AbsoluteTime > ch.lastTime100ns && data.AbsoluteEnergy >= ch.lastEnergyPwh) {
                ULONGLONG dEnergyPwh = data.AbsoluteEnergy - ch.lastEnergyPwh;
                ULONGLONG dTime100ns = data.AbsoluteTime - ch.lastTime100ns;

                if (dTime100ns > 0) {
                    // pWh to Watts: (pWh * 3.6e-9 J/pWh) / (100ns * 1e-7 s/100ns)
                    double powerW = ((double)dEnergyPwh * 0.036) / (double)dTime100ns;
                    if (powerW >= 0.0 && powerW < 2000.0) {
                        ch.lastPowerW = powerW;
                    }
                }
            } else {
                // Reset baseline (counter rollover or first read)
                ch.lastPowerW = 0.0;
            }
            ch.lastEnergyPwh = data.AbsoluteEnergy;
            ch.lastTime100ns = data.AbsoluteTime;
        }
    }
}

// ─────────────────────────────────────────────────────────────
// ACPI Battery Discharge Rate
// On laptops: Total system power ≈ BatteryDischargeRate
// Uses WMI-free GetSystemPowerStatus + IOCTL_BATTERY_STATUS
// for real-time milliwatt discharge rate
// ─────────────────────────────────────────────────────────────
double PowerMonitor::ReadBatteryDischargeRateW() {
    if (!m_batteryAvail) return 0.0;

    // Use SetupDi to find battery devices and query IOCTL_BATTERY_STATUS
    // for real-time discharge rate in milliwatts
    static const GUID GUID_DEVCLASS_BATTERY =
        { 0x72631e54, 0x78a4, 0x11d0, { 0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a } };

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_BATTERY, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    if (hDevInfo == INVALID_HANDLE_VALUE) return 0.0;

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    double totalDischargeW = 0.0;

    // Check first battery (index 0)
    if (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVCLASS_BATTERY, 0, &ifData)) {
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, NULL, 0, &reqSize, NULL);
        if (reqSize > 0) {
            std::vector<BYTE> detailBuf(reqSize);
            auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuf.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, detail, reqSize, NULL, NULL)) {
                HANDLE hBat = CreateFileW(
                    detail->DevicePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
                );
                if (hBat != INVALID_HANDLE_VALUE) {
                    // 1. Query battery tag
                    ULONG batteryTag = 0;
                    ULONG waitTimeout = 0;
                    DWORD bytesReturned = 0;
                    #ifndef IOCTL_BATTERY_QUERY_TAG
                    #define IOCTL_BATTERY_QUERY_TAG CTL_CODE(FILE_DEVICE_BATTERY, 0x10, METHOD_BUFFERED, FILE_READ_ACCESS)
                    #endif
                    if (DeviceIoControl(hBat, IOCTL_BATTERY_QUERY_TAG,
                        &waitTimeout, sizeof(waitTimeout),
                        &batteryTag, sizeof(batteryTag),
                        &bytesReturned, NULL) && batteryTag != 0) {

                        // 2. Query battery status for discharge rate
                        #ifndef IOCTL_BATTERY_QUERY_STATUS
                        #define IOCTL_BATTERY_QUERY_STATUS CTL_CODE(FILE_DEVICE_BATTERY, 0x13, METHOD_BUFFERED, FILE_READ_ACCESS)
                        #endif

                        struct BATTERY_WAIT_STATUS {
                            ULONG BatteryTag;
                            ULONG Timeout;
                            ULONG PowerState;
                            ULONG LowCapacity;
                            ULONG HighCapacity;
                        };
                        struct BATTERY_STATUS {
                            ULONG PowerState;
                            ULONG Capacity;
                            ULONG Voltage;
                            LONG  Rate;  // milliwatts (negative = discharging)
                        };

                        BATTERY_WAIT_STATUS bws = {};
                        bws.BatteryTag = batteryTag;
                        bws.Timeout = 0;
                        bws.PowerState = 0;
                        bws.LowCapacity = 0;
                        bws.HighCapacity = 0xFFFFFFFF;

                        BATTERY_STATUS bs = {};
                        if (DeviceIoControl(hBat, IOCTL_BATTERY_QUERY_STATUS,
                            &bws, sizeof(bws),
                            &bs, sizeof(bs),
                            &bytesReturned, NULL)) {

                            // Rate is in milliwatts; negative means discharging
                            if (bs.Rate < 0 && bs.Rate != (LONG)0x80000000) {
                                totalDischargeW = (double)(-bs.Rate) / 1000.0;
                            } else if (bs.Rate > 0 && !(bs.PowerState & 0x1 /* AC */)) {
                                // Some batteries report positive rate even when discharging
                                totalDischargeW = (double)bs.Rate / 1000.0;
                            }
                        }
                    }
                    CloseHandle(hBat);
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    if (totalDischargeW > 0.0 && totalDischargeW < 1000.0) {
        m_batteryLastW = totalDischargeW;
        return totalDischargeW;
    }
    return 0.0;
}

// ═══════════════════════════════════════════════════════════════
// Master Collect: Aggregate all power sources
// ═══════════════════════════════════════════════════════════════
void PowerMonitor::Collect(TelemetrySnapshot& snapshot) {
    // ── 1. Read all available EMI channels ──
    if (m_emiAvailable) {
        ReadAllEmiChannels();

        // If CPU Package power wasn't obtained from MSR RAPL, use EMI CPU channel
        if (!snapshot.cpuPowerAvail || snapshot.cpuPowerW <= 0.0) {
            for (const auto& ch : m_emiChannels) {
                if (ch.isCpuPackage && ch.lastPowerW > 0.0) {
                    snapshot.cpuPowerW = ch.lastPowerW;
                    snapshot.cpuPowerAvail = true;
                    break;
                }
            }
        }

        // If still no CPU power, and there's exactly 1 unclassified EMI channel,
        // treat it as total system / CPU power (common on many OEMs)
        if (!snapshot.cpuPowerAvail || snapshot.cpuPowerW <= 0.0) {
            if (m_emiChannels.size() == 1) {
                auto& ch = m_emiChannels[0];
                if (!ch.isCpuPackage && !ch.isDram && !ch.isSoC && !ch.isGpu && !ch.isSystem) {
                    // Single unclassified channel — treat as CPU Package
                    ch.isCpuPackage = true;
                }
                if (ch.lastPowerW > 0.0) {
                    snapshot.cpuPowerW = ch.lastPowerW;
                    snapshot.cpuPowerAvail = true;
                }
            }
        }
    }

    // ── 2. Universal Zero Double-Counting Summation ──
    // Total Power = CPU Package + Sum(Discrete GPU Powers) + DRAM + Other EMI Rails
    // iGPU power is NOT added when CPU Package is present (physically inside CPU die).
    double total = 0.0;
    std::vector<std::wstring> parts;

    bool hasCpuPower = (snapshot.cpuPowerAvail && snapshot.cpuPowerW > 0.0);
    if (hasCpuPower) {
        total += snapshot.cpuPowerW;
        wchar_t buf[48];
        swprintf_s(buf, L"CPU: %.1f W", snapshot.cpuPowerW);
        parts.push_back(buf);
    }

    // ── 3. Discrete GPU Power from vendor APIs ──
    for (const auto& g : snapshot.gpus) {
        if (g.powerW > 0.0) {
            if (g.isDiscrete) {
                total += g.powerW;
                wchar_t buf[48];
                swprintf_s(buf, L"%s: %.1f W", g.label, g.powerW);
                parts.push_back(buf);
            } else if (!hasCpuPower) {
                // Only add iGPU power if CPU Package power is unavailable
                total += g.powerW;
                wchar_t buf[48];
                swprintf_s(buf, L"%s: %.1f W", g.label, g.powerW);
                parts.push_back(buf);
            }
        }
    }

    // ── 4. DRAM Power Telemetry & Estimation ──
    double dramW = 0.0;
    // Check hardware MSR / EMI first
    if (snapshot.dramPowerW > 0.0) {
        dramW = snapshot.dramPowerW;
    } else {
        for (const auto& ch : m_emiChannels) {
            if (ch.isDram && ch.lastPowerW > 0.0) {
                dramW = ch.lastPowerW;
                break;
            }
        }
    }

    // Physical RAM Power Estimation fallback (DDR3/4/5 ~1.8W idle per 8GB, up to ~3.8W under load)
    if (dramW <= 0.0 && snapshot.ramTotalMB > 0.0) {
        double ramRatio = std::clamp(snapshot.ramUsedMB / snapshot.ramTotalMB, 0.0, 1.0);
        double sticks = std::max(1.0, std::round(snapshot.ramTotalMB / 8192.0));
        dramW = sticks * (1.8 + 2.0 * ramRatio);
    }

    if (dramW > 0.0) {
        snapshot.dramPowerW = dramW;
        total += dramW;
        wchar_t buf[48];
        swprintf_s(buf, L"RAM: %.1f W", dramW);
        parts.push_back(buf);
    }

    // ── 5. Platform / Motherboard / Fans / Storage / VRM ("Other") ──
    // In any PC or laptop, the motherboard chipset, SSD/NVMe drives, cooling fans,
    // and power supply VRM efficiency losses (~8-12%) consume baseline platform power.
    bool usedSystemRail = false;
    double systemEmiW = 0.0;
    for (const auto& ch : m_emiChannels) {
        if (ch.isSystem && ch.lastPowerW > 0.0) {
            systemEmiW = ch.lastPowerW;
            break;
        }
    }

    if (systemEmiW > 0.0 && systemEmiW > total) {
        // Authoritative platform EMI rail exists
        total = systemEmiW;
        usedSystemRail = true;
        snapshot.otherPowerW = systemEmiW - (snapshot.cpuPowerW + dramW);
    } else {
        // Calculate platform other components power:
        // Laptops: ~4-6W (low-power SoC motherboard, display, single SSD)
        // Desktops: ~12-18W (desktop chipset, multiple fans, storage, PCIe rails, VRM losses)
        double vrmLossW = total * 0.08; // ~8% VRM conversion loss
        double basePlatformW = m_batteryAvail ? 4.5 : 12.0;
        double otherW = basePlatformW + vrmLossW;
        snapshot.otherPowerW = otherW;
        total += otherW;

        wchar_t buf[48];
        swprintf_s(buf, L"Other: %.1f W", otherW);
        parts.push_back(buf);
    }

    // ── 6. Battery Discharge Rate (Laptops on Battery) ──
    // When running on battery without AC power, the battery gas gauge reports
    // the exact physical real-time system discharge rate in milliwatts.
    double batteryW = ReadBatteryDischargeRateW();
    if (batteryW > 0.0) {
        // True physical wall/battery total
        total = batteryW;
        double measuredComponents = snapshot.cpuPowerW + dramW;
        for (const auto& g : snapshot.gpus) {
            if (g.powerW > 0.0) measuredComponents += g.powerW;
        }
        double remainingOther = (batteryW > measuredComponents) ? (batteryW - measuredComponents) : 3.5;
        snapshot.otherPowerW = remainingOther;

        // Rebuild clean breakdown matching battery total
        parts.clear();
        if (snapshot.cpuPowerAvail && snapshot.cpuPowerW > 0.0) {
            wchar_t buf[48];
            swprintf_s(buf, L"CPU: %.1f W", snapshot.cpuPowerW);
            parts.push_back(buf);
        }
        for (const auto& g : snapshot.gpus) {
            if (g.powerW > 0.0) {
                wchar_t buf[48];
                swprintf_s(buf, L"%s: %.1f W", g.label, g.powerW);
                parts.push_back(buf);
            }
        }
        if (snapshot.dramPowerW > 0.0) {
            wchar_t buf[48];
            swprintf_s(buf, L"RAM: %.1f W", snapshot.dramPowerW);
            parts.push_back(buf);
        }
        wchar_t buf[48];
        swprintf_s(buf, L"Other: %.1f W", remainingOther);
        parts.push_back(buf);
    }

    // ── 7. Final Snapshot ──
    snapshot.totalPowerW = total;
    snapshot.totalPowerAvail = (total > 0.0);

    // Format Component Power Breakdown String
    snapshot.powerBreakdown[0] = L'\0';
    if (!parts.empty()) {
        std::wstring breakdown;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) breakdown += L"  \u00B7  ";
            breakdown += parts[i];
        }
        wcsncpy_s(snapshot.powerBreakdown, breakdown.c_str(), _TRUNCATE);
    }
}

} // namespace LV
