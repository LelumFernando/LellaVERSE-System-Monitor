#include "gpu_monitor.h"
#include "settings.h"
#include <set>
#include <algorithm>

namespace LV {

static void* __cdecl AdlMalloc(int iSize) {
    return malloc(iSize);
}

static bool ContainsSubstringI(const wchar_t* haystack, const wchar_t* needle) {
    if (!haystack || !needle) return false;
    size_t needleLen = wcslen(needle);
    if (needleLen == 0) return true;
    size_t hayLen = wcslen(haystack);
    if (hayLen < needleLen) return false;

    for (size_t i = 0; i <= hayLen - needleLen; ++i) {
        if (_wcsnicmp(haystack + i, needle, needleLen) == 0) {
            return true;
        }
    }
    return false;
}

static double GetEstimatedGpuTdpWatts(const wchar_t* name, GpuVendor vendor, ULONGLONG dedicatedVram, double& outIdleW) {
    outIdleW = 10.0;
    if (!name) return 100.0;

    // NVIDIA GeForce Models
    if (ContainsSubstringI(name, L"4090")) { outIdleW = 20.0; return 450.0; }
    if (ContainsSubstringI(name, L"4080")) { outIdleW = 16.0; return 320.0; }
    if (ContainsSubstringI(name, L"4070 Ti")) { outIdleW = 14.0; return 285.0; }
    if (ContainsSubstringI(name, L"4070")) { outIdleW = 12.0; return 200.0; }
    if (ContainsSubstringI(name, L"4060 Ti")) { outIdleW = 11.0; return 160.0; }
    if (ContainsSubstringI(name, L"4060")) { outIdleW = 10.0; return 115.0; }

    if (ContainsSubstringI(name, L"3090")) { outIdleW = 22.0; return 350.0; }
    if (ContainsSubstringI(name, L"3080 Ti")) { outIdleW = 20.0; return 350.0; }
    if (ContainsSubstringI(name, L"3080")) { outIdleW = 18.0; return 320.0; }
    if (ContainsSubstringI(name, L"3070 Ti")) { outIdleW = 15.0; return 290.0; }
    if (ContainsSubstringI(name, L"3070")) { outIdleW = 13.0; return 220.0; }
    if (ContainsSubstringI(name, L"3060 Ti")) { outIdleW = 12.0; return 200.0; }
    if (ContainsSubstringI(name, L"3060")) { outIdleW = 11.0; return 170.0; }
    if (ContainsSubstringI(name, L"3050")) { outIdleW = 9.0;  return 90.0; }

    if (ContainsSubstringI(name, L"2080 Ti")) { outIdleW = 18.0; return 250.0; }
    if (ContainsSubstringI(name, L"2080")) { outIdleW = 15.0; return 215.0; }
    if (ContainsSubstringI(name, L"2070")) { outIdleW = 13.0; return 175.0; }
    if (ContainsSubstringI(name, L"2060")) { outIdleW = 12.0; return 160.0; }

    if (ContainsSubstringI(name, L"1660 Ti") || ContainsSubstringI(name, L"1660 Super")) { outIdleW = 10.0; return 125.0; }
    if (ContainsSubstringI(name, L"1660")) { outIdleW = 10.0; return 120.0; }
    if (ContainsSubstringI(name, L"1650")) { outIdleW = 7.0;  return 75.0; }

    if (ContainsSubstringI(name, L"1080 Ti")) { outIdleW = 16.0; return 250.0; }
    if (ContainsSubstringI(name, L"1080")) { outIdleW = 14.0; return 180.0; }
    if (ContainsSubstringI(name, L"1070")) { outIdleW = 12.0; return 150.0; }
    if (ContainsSubstringI(name, L"1060")) { outIdleW = 10.0; return 120.0; }
    if (ContainsSubstringI(name, L"1050 Ti") || ContainsSubstringI(name, L"1050")) { outIdleW = 6.0; return 75.0; }

    if (ContainsSubstringI(name, L"980 Ti")) { outIdleW = 16.0; return 250.0; }
    if (ContainsSubstringI(name, L"980")) { outIdleW = 14.0; return 165.0; }
    if (ContainsSubstringI(name, L"970")) { outIdleW = 12.0; return 145.0; }
    if (ContainsSubstringI(name, L"960")) { outIdleW = 10.0; return 120.0; }
    if (ContainsSubstringI(name, L"950")) { outIdleW = 8.0;  return 90.0; }
    if (ContainsSubstringI(name, L"750 Ti") || ContainsSubstringI(name, L"750")) { outIdleW = 6.0; return 60.0; }

    // AMD Radeon Models
    if (ContainsSubstringI(name, L"7900")) { outIdleW = 16.0; return 315.0; }
    if (ContainsSubstringI(name, L"7800")) { outIdleW = 14.0; return 263.0; }
    if (ContainsSubstringI(name, L"7700")) { outIdleW = 12.0; return 245.0; }
    if (ContainsSubstringI(name, L"7600")) { outIdleW = 10.0; return 165.0; }
    if (ContainsSubstringI(name, L"6900") || ContainsSubstringI(name, L"6800")) { outIdleW = 15.0; return 280.0; }
    if (ContainsSubstringI(name, L"6700")) { outIdleW = 12.0; return 230.0; }
    if (ContainsSubstringI(name, L"6600")) { outIdleW = 9.0;  return 132.0; }
    if (ContainsSubstringI(name, L"RX 590") || ContainsSubstringI(name, L"RX 580")) { outIdleW = 14.0; return 185.0; }
    if (ContainsSubstringI(name, L"RX 570") || ContainsSubstringI(name, L"RX 480")) { outIdleW = 12.0; return 150.0; }
    if (ContainsSubstringI(name, L"RX 560") || ContainsSubstringI(name, L"RX 550")) { outIdleW = 6.0;  return 75.0; }

    // Intel Arc
    if (ContainsSubstringI(name, L"A770")) { outIdleW = 18.0; return 225.0; }
    if (ContainsSubstringI(name, L"A750")) { outIdleW = 18.0; return 225.0; }
    if (ContainsSubstringI(name, L"A580")) { outIdleW = 15.0; return 185.0; }
    if (ContainsSubstringI(name, L"A380")) { outIdleW = 8.0;  return 75.0; }

    // Laptop Discrete GPUs
    if (ContainsSubstringI(name, L"Laptop") || ContainsSubstringI(name, L"Mobile") || ContainsSubstringI(name, L"Max-Q")) {
        outIdleW = 6.0;
        return 80.0;
    }

    // Generic fallback by dedicated VRAM size
    if (dedicatedVram >= 12ULL * 1024 * 1024 * 1024) { outIdleW = 16.0; return 250.0; }
    if (dedicatedVram >= 8ULL * 1024 * 1024 * 1024)  { outIdleW = 12.0; return 180.0; }
    if (dedicatedVram >= 4ULL * 1024 * 1024 * 1024)  { outIdleW = 10.0; return 120.0; }
    if (dedicatedVram >= 2ULL * 1024 * 1024 * 1024)  { outIdleW = 8.0;  return 75.0;  }

    outIdleW = 6.0;
    return 65.0;
}

GpuMonitor::GpuMonitor() {
    Init();
}

GpuMonitor::~GpuMonitor() {
    Shutdown();
}

void GpuMonitor::EnumeratePhysicalGpus() {
    for (auto& ad : m_adapters) {
        if (ad.pAdapter) {
            ad.pAdapter->Release();
            ad.pAdapter = nullptr;
        }
    }
    m_adapters.clear();

    if (!m_pFactory) {
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&m_pFactory))) {
            return;
        }
    }

    struct CandidateGpu {
        DXGI_ADAPTER_DESC1 desc;
        IDXGIAdapter1* pAdapter = nullptr;
        std::vector<std::wstring> luidPatterns;
        bool isDiscrete = false;
        GpuVendor vendor = GPU_VENDOR_UNKNOWN;
    };

    std::vector<CandidateGpu> candidates;

    IDXGIAdapter1* pAdapter = nullptr;
    UINT i = 0;

    while (m_pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(pAdapter->GetDesc1(&desc))) {
            // 1. Exclude Software & Basic Render Driver adapters
            bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (desc.VendorId == 0 || desc.VendorId == 0x1414 ||
                wcsstr(desc.Description, L"Microsoft Basic Render") != nullptr) {
                isSoftware = true;
            }

            // 2. Exclude Virtual / Remote / Indirect Display Adapters
            if (wcsstr(desc.Description, L"Parsec") != nullptr ||
                wcsstr(desc.Description, L"spacedesk") != nullptr ||
                wcsstr(desc.Description, L"Virtual") != nullptr ||
                wcsstr(desc.Description, L"RDP") != nullptr ||
                wcsstr(desc.Description, L"VDA") != nullptr ||
                wcsstr(desc.Description, L"Indirect") != nullptr ||
                wcsstr(desc.Description, L"Remote") != nullptr ||
                wcsstr(desc.Description, L"IddSample") != nullptr ||
                wcsstr(desc.Description, L"DisplayLink") != nullptr ||
                wcsstr(desc.Description, L"Citrix") != nullptr ||
                wcsstr(desc.Description, L"VMware") != nullptr ||
                wcsstr(desc.Description, L"VirtualBox") != nullptr) {
                isSoftware = true;
            }

            if (!isSoftware) {
                wchar_t luidPattern[64];
                swprintf_s(luidPattern, L"0x%08x_0x%08x",
                    (unsigned int)desc.AdapterLuid.HighPart,
                    (unsigned int)desc.AdapterLuid.LowPart);

                // 3. Deduplicate physical GPUs by hardware signature (VendorId, DeviceId, SubSysId)
                bool alreadyAdded = false;
                for (auto& cand : candidates) {
                    if (cand.desc.VendorId == desc.VendorId &&
                        cand.desc.DeviceId == desc.DeviceId &&
                        cand.desc.SubSysId == desc.SubSysId) {
                        cand.luidPatterns.push_back(luidPattern);
                        alreadyAdded = true;
                        break;
                    }
                }

                if (!alreadyAdded) {
                    CandidateGpu cand;
                    cand.desc = desc;
                    cand.pAdapter = pAdapter;
                    cand.pAdapter->AddRef();
                    cand.luidPatterns.push_back(luidPattern);

                    // Determine vendor universally
                    if (desc.VendorId == 0x10DE) {
                        cand.vendor = GPU_VENDOR_NVIDIA;
                    } else if (desc.VendorId == 0x1002 || desc.VendorId == 0x1022) {
                        cand.vendor = GPU_VENDOR_AMD;
                    } else if (desc.VendorId == 0x8086) {
                        cand.vendor = GPU_VENDOR_INTEL;
                    } else {
                        cand.vendor = GPU_VENDOR_UNKNOWN;
                    }

                    // Classify integrated (iGPU) vs discrete (dGPU)
                    if (cand.vendor == GPU_VENDOR_NVIDIA) {
                        cand.isDiscrete = true;
                    } else if (desc.DedicatedVideoMemory > 512ULL * 1024ULL * 1024ULL) {
                        cand.isDiscrete = true;
                    } else {
                        cand.isDiscrete = false;
                    }

                    // Known integrated names override if dedicated VRAM is small (<= 512MB)
                    if (wcsstr(desc.Description, L"Iris") != nullptr ||
                        wcsstr(desc.Description, L"UHD Graphics") != nullptr ||
                        wcsstr(desc.Description, L"HD Graphics") != nullptr ||
                        wcsstr(desc.Description, L"Radeon(TM) Graphics") != nullptr) {
                        if (desc.DedicatedVideoMemory <= 512ULL * 1024ULL * 1024ULL) {
                            cand.isDiscrete = false;
                        }
                    }

                    candidates.push_back(cand);
                }
            }
        }
        pAdapter->Release();
        pAdapter = nullptr;
        i++;
    }

    // 4. Deterministic GPU Indexing:
    // Always sort so iGPU (isDiscrete == false) comes before dGPU (isDiscrete == true).
    std::sort(candidates.begin(), candidates.end(), [](const CandidateGpu& a, const CandidateGpu& b) {
        if (a.isDiscrete != b.isDiscrete) {
            return !a.isDiscrete; // iGPU (false) before dGPU (true)
        }
        return a.desc.DedicatedVideoMemory < b.desc.DedicatedVideoMemory;
    });

    // 5. Assign final physical GPU adapters:
    // GPU 0 is always assigned to the integrated GPU (iGPU) (or sole GPU if single-GPU)
    // GPU 1+ is always assigned to discrete GPU(s) (dGPU)
    for (size_t idx = 0; idx < candidates.size(); ++idx) {
        const auto& cand = candidates[idx];

        PhysicalGpuAdapter ad;
        ad.index = (int)idx;
        swprintf_s(ad.label, L"GPU %d", (int)idx);
        wcsncpy_s(ad.name, cand.desc.Description, _TRUNCATE);
        ad.vendor = cand.vendor;
        ad.luid = cand.desc.AdapterLuid;
        ad.luidPatterns = cand.luidPatterns;
        ad.dedicatedVram = cand.desc.DedicatedVideoMemory;
        ad.sharedSystemMemory = cand.desc.SharedSystemMemory;
        ad.pAdapter = cand.pAdapter; // Keep ref
        ad.isDiscrete = cand.isDiscrete;

        m_adapters.push_back(ad);
    }

    // 6. Prune stale GPU settings from INI so nonexistent GPU indices don't linger
    Settings::Get().PruneStaleGpuToggles((int)m_adapters.size());
}

bool GpuMonitor::InitNvml() {
    if (m_nvmlLoaded) return true;

    const wchar_t* nvmlPaths[] = {
        L"nvml.dll",
        L"C:\\Windows\\System32\\nvml.dll",
        L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll"
    };

    for (const auto* path : nvmlPaths) {
        m_hNvml = LoadLibraryW(path);
        if (m_hNvml) break;
    }

    if (!m_hNvml) return false;

    pfn_nvmlInit = (fn_nvmlInit_v2)GetProcAddress(m_hNvml, "nvmlInit_v2");
    if (!pfn_nvmlInit) pfn_nvmlInit = (fn_nvmlInit_v2)GetProcAddress(m_hNvml, "nvmlInit");

    pfn_nvmlShutdown = (fn_nvmlShutdown)GetProcAddress(m_hNvml, "nvmlShutdown");
    pfn_nvmlDeviceGetHandleByIndex = (fn_nvmlDeviceGetHandleByIndex)GetProcAddress(m_hNvml, "nvmlDeviceGetHandleByIndex_v2");
    if (!pfn_nvmlDeviceGetHandleByIndex) pfn_nvmlDeviceGetHandleByIndex = (fn_nvmlDeviceGetHandleByIndex)GetProcAddress(m_hNvml, "nvmlDeviceGetHandleByIndex");
    pfn_nvmlDeviceGetHandleByPciBusId = (fn_nvmlDeviceGetHandleByPciBusId)GetProcAddress(m_hNvml, "nvmlDeviceGetHandleByPciBusId");

    pfn_nvmlDeviceGetCount = (fn_nvmlDeviceGetCount)GetProcAddress(m_hNvml, "nvmlDeviceGetCount");
    pfn_nvmlDeviceGetName = (fn_nvmlDeviceGetName)GetProcAddress(m_hNvml, "nvmlDeviceGetName");
    pfn_nvmlDeviceGetUtilizationRates = (fn_nvmlDeviceGetUtilizationRates)GetProcAddress(m_hNvml, "nvmlDeviceGetUtilizationRates");
    pfn_nvmlDeviceGetTemperature = (fn_nvmlDeviceGetTemperature)GetProcAddress(m_hNvml, "nvmlDeviceGetTemperature");
    pfn_nvmlDeviceGetClockInfo = (fn_nvmlDeviceGetClockInfo)GetProcAddress(m_hNvml, "nvmlDeviceGetClockInfo");
    pfn_nvmlDeviceGetMemoryInfo = (fn_nvmlDeviceGetMemoryInfo)GetProcAddress(m_hNvml, "nvmlDeviceGetMemoryInfo");
    pfn_nvmlDeviceGetPowerUsage = (fn_nvmlDeviceGetPowerUsage)GetProcAddress(m_hNvml, "nvmlDeviceGetPowerUsage");
    pfn_nvmlDeviceGetPciInfo = (fn_nvmlDeviceGetPciInfo)GetProcAddress(m_hNvml, "nvmlDeviceGetPciInfo");

    if (pfn_nvmlInit && pfn_nvmlDeviceGetHandleByIndex && pfn_nvmlInit() == 0) {
        m_nvmlLoaded = true;

        unsigned int nvCount = 0;
        if (pfn_nvmlDeviceGetCount && pfn_nvmlDeviceGetCount(&nvCount) == 0) {
            unsigned int nvIdx = 0;
            for (auto& ad : m_adapters) {
                if (ad.vendor == GPU_VENDOR_NVIDIA && nvIdx < nvCount) {
                    nvmlDevice_t dev = nullptr;
                    if (pfn_nvmlDeviceGetHandleByIndex(nvIdx, &dev) == 0) {
                        ad.nvmlDevice = dev;
                        ad.hasNvml = true;
                        if (pfn_nvmlDeviceGetPciInfo) {
                            nvmlPciInfo_t pciInfo = {};
                            if (pfn_nvmlDeviceGetPciInfo(dev, &pciInfo) == 0) {
                                ad.pciBusId = pciInfo.bus;
                            }
                        }
                        nvIdx++;
                    }
                }
            }
        }
        return true;
    }

    ShutdownNvml();
    return false;
}

void GpuMonitor::ShutdownNvml() {
    if (m_nvmlLoaded && pfn_nvmlShutdown) {
        pfn_nvmlShutdown();
    }
    m_nvmlLoaded = false;
    for (auto& ad : m_adapters) {
        ad.nvmlDevice = nullptr;
        ad.hasNvml = false;
    }

    if (m_hNvml) {
        FreeLibrary(m_hNvml);
        m_hNvml = NULL;
    }
}

bool GpuMonitor::InitNvapi() {
    if (m_nvapiLoaded) return true;

    m_hNvapi = LoadLibraryA("nvapi64.dll");
    if (!m_hNvapi) m_hNvapi = LoadLibraryA("nvapi.dll");
    if (!m_hNvapi) return false;

    auto pfnQuery = (fn_nvapi_QueryInterface)GetProcAddress(m_hNvapi, "nvapi_QueryInterface");
    if (!pfnQuery) {
        FreeLibrary(m_hNvapi);
        m_hNvapi = NULL;
        return false;
    }

    pfn_NvAPI_Initialize = (fn_NvAPI_Initialize)pfnQuery(0x0150E828);
    pfn_NvAPI_Unload = (fn_NvAPI_Unload)pfnQuery(0xD22BDD7E);
    pfn_NvAPI_EnumPhysicalGPUs = (fn_NvAPI_EnumPhysicalGPUs)pfnQuery(0xE5AC9227);
    pfn_NvAPI_GPU_GetBusId = (fn_NvAPI_GPU_GetBusId)pfnQuery(0x1BE0B8E5);
    pfn_NvAPI_GPU_GetThermalSettings = (fn_NvAPI_GPU_GetThermalSettings)pfnQuery(0xE3640A56);
    pfn_NvAPI_GPU_GetAllClockFrequencies = (fn_NvAPI_GPU_GetAllClockFrequencies)pfnQuery(0xDCB616C3);
    pfn_NvAPI_GPU_ClientPowerTopologyGetStatus = (fn_NvAPI_GPU_ClientPowerTopologyGetStatus)pfnQuery(0xEDCF624E);

    if (pfn_NvAPI_Initialize && pfn_NvAPI_Initialize() == 0 && pfn_NvAPI_EnumPhysicalGPUs) {
        m_nvapiLoaded = true;

        NvPhysicalGpuHandle gpuHandles[64] = { 0 };
        int gpuCount = 0;
        if (pfn_NvAPI_EnumPhysicalGPUs(gpuHandles, &gpuCount) == 0 && gpuCount > 0) {
            for (auto& ad : m_adapters) {
                if (ad.vendor == GPU_VENDOR_NVIDIA) {
                    for (int g = 0; g < gpuCount; ++g) {
                        DWORD busId = 0;
                        if (pfn_NvAPI_GPU_GetBusId && pfn_NvAPI_GPU_GetBusId(gpuHandles[g], &busId) == 0) {
                            if (ad.pciBusId == 0 || ad.pciBusId == busId) {
                                ad.nvapiHandle = gpuHandles[g];
                                ad.hasNvapi = true;
                                ad.pciBusId = busId;
                                break;
                            }
                        } else if (!ad.hasNvapi) {
                            ad.nvapiHandle = gpuHandles[g];
                            ad.hasNvapi = true;
                            break;
                        }
                    }
                }
            }
        }
        return true;
    }

    ShutdownNvapi();
    return false;
}

void GpuMonitor::ShutdownNvapi() {
    if (m_nvapiLoaded && pfn_NvAPI_Unload) {
        pfn_NvAPI_Unload();
    }
    m_nvapiLoaded = false;
    for (auto& ad : m_adapters) {
        ad.nvapiHandle = nullptr;
        ad.hasNvapi = false;
    }
    if (m_hNvapi) {
        FreeLibrary(m_hNvapi);
        m_hNvapi = NULL;
    }
}

bool GpuMonitor::InitAdl() {
    if (m_adlLoaded) return true;

    const wchar_t* adlPaths[] = {
        L"atiadlxx.dll", // 64-bit
        L"atiadlxy.dll"  // 32-bit fallback
    };

    for (const auto* path : adlPaths) {
        m_hAdl = LoadLibraryW(path);
        if (m_hAdl) break;
    }

    if (!m_hAdl) return false;

    pfn_ADL2_Main_Control_Create = (fn_ADL2_Main_Control_Create)GetProcAddress(m_hAdl, "ADL2_Main_Control_Create");
    pfn_ADL2_Main_Control_Destroy = (fn_ADL2_Main_Control_Destroy)GetProcAddress(m_hAdl, "ADL2_Main_Control_Destroy");
    pfn_ADL2_Adapter_NumberOfAdapters_Get = (fn_ADL2_Adapter_NumberOfAdapters_Get)GetProcAddress(m_hAdl, "ADL2_Adapter_NumberOfAdapters_Get");
    pfn_ADL2_Adapter_AdapterInfo_Get = (fn_ADL2_Adapter_AdapterInfo_Get)GetProcAddress(m_hAdl, "ADL2_Adapter_AdapterInfo_Get");
    pfn_ADL2_New_QueryPMLogData_Get = (fn_ADL2_New_QueryPMLogData_Get)GetProcAddress(m_hAdl, "ADL2_New_QueryPMLogData_Get");
    pfn_ADL2_Overdrive6_CurrentPower_Get = (fn_ADL2_Overdrive6_CurrentPower_Get)GetProcAddress(m_hAdl, "ADL2_Overdrive6_CurrentPower_Get");
    pfn_ADL2_OverdriveN_PerformanceStatus_Get = (fn_ADL2_OverdriveN_PerformanceStatus_Get)GetProcAddress(m_hAdl, "ADL2_OverdriveN_PerformanceStatus_Get");
    pfn_ADL2_Overdrive6_Temperature_Get = (fn_ADL2_Overdrive6_Temperature_Get)GetProcAddress(m_hAdl, "ADL2_Overdrive6_Temperature_Get");
    pfn_ADL2_OverdriveN_Temperature_Get = (fn_ADL2_OverdriveN_Temperature_Get)GetProcAddress(m_hAdl, "ADL2_OverdriveN_Temperature_Get");

    if (pfn_ADL2_Main_Control_Create && pfn_ADL2_Main_Control_Create(AdlMalloc, 1, &m_adlContext) == 0 && m_adlContext) {
        m_adlLoaded = true;

        int numAdapters = 0;
        if (pfn_ADL2_Adapter_NumberOfAdapters_Get &&
            pfn_ADL2_Adapter_NumberOfAdapters_Get(m_adlContext, &numAdapters) == 0 &&
            numAdapters > 0) {

            std::vector<ADLAdapterInfo> adlInfos(numAdapters);
            for (auto& info : adlInfos) {
                memset(&info, 0, sizeof(info));
                info.iSize = sizeof(ADLAdapterInfo);
            }

            if (pfn_ADL2_Adapter_AdapterInfo_Get &&
                pfn_ADL2_Adapter_AdapterInfo_Get(m_adlContext, adlInfos.data(), sizeof(ADLAdapterInfo) * numAdapters) == 0) {

                for (auto& ad : m_adapters) {
                    if (ad.vendor == GPU_VENDOR_AMD) {
                        for (int j = 0; j < numAdapters; ++j) {
                            if (adlInfos[j].iPresent && (adlInfos[j].iVendorID == 0x1002 || adlInfos[j].iVendorID == 0x1022)) {
                                ad.adlAdapterIndex = adlInfos[j].iAdapterIndex;
                                ad.pciBusId = (DWORD)adlInfos[j].iBusNumber;
                                ad.hasAdl = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    ShutdownAdl();
    return false;
}

void GpuMonitor::ShutdownAdl() {
    if (m_adlLoaded && pfn_ADL2_Main_Control_Destroy && m_adlContext) {
        pfn_ADL2_Main_Control_Destroy(m_adlContext);
    }
    m_adlContext = nullptr;
    m_adlLoaded = false;
    for (auto& ad : m_adapters) {
        ad.adlAdapterIndex = -1;
        ad.hasAdl = false;
    }

    if (m_hAdl) {
        FreeLibrary(m_hAdl);
        m_hAdl = NULL;
    }
}

bool GpuMonitor::InitIntelCtl() {
    if (m_ctlLoaded) return true;

    m_hIntelCtl = LoadLibraryA("ControlLib.dll");
    if (!m_hIntelCtl) return false;

    pfn_ctlInit = (fn_ctlInit)GetProcAddress(m_hIntelCtl, "ctlInit");
    pfn_ctlClose = (fn_ctlClose)GetProcAddress(m_hIntelCtl, "ctlClose");
    pfn_ctlEnumerateDevices = (fn_ctlEnumerateDevices)GetProcAddress(m_hIntelCtl, "ctlEnumerateDevices");
    pfn_ctlGetDeviceProperties = (fn_ctlGetDeviceProperties)GetProcAddress(m_hIntelCtl, "ctlGetDeviceProperties");
    pfn_ctlPowerGetEnergyCounter = (fn_ctlPowerGetEnergyCounter)GetProcAddress(m_hIntelCtl, "ctlPowerGetEnergyCounter");
    pfn_ctlFrequencyGetState = (fn_ctlFrequencyGetState)GetProcAddress(m_hIntelCtl, "ctlFrequencyGetState");
    pfn_ctlTemperatureGetState = (fn_ctlTemperatureGetState)GetProcAddress(m_hIntelCtl, "ctlTemperatureGetState");

    if (pfn_ctlInit && pfn_ctlEnumerateDevices && pfn_ctlGetDeviceProperties) {
        ctl_init_args_t initArgs = {};
        initArgs.Size = sizeof(ctl_init_args_t);
        initArgs.Version = 1;
        initArgs.AppVersion = 1;

        if (pfn_ctlInit(&initArgs, &m_ctlContext) == 0 && m_ctlContext) {
            m_ctlLoaded = true;

            DWORD devCount = 0;
            pfn_ctlEnumerateDevices(m_ctlContext, &devCount, nullptr);
            if (devCount > 0) {
                std::vector<ctl_device_adapter_handle_t> ctlDevs(devCount);
                if (pfn_ctlEnumerateDevices(m_ctlContext, &devCount, ctlDevs.data()) == 0) {
                    for (DWORD d = 0; d < devCount; ++d) {
                        ctl_device_adapter_properties_t props = {};
                        props.Size = sizeof(ctl_device_adapter_properties_t);
                        props.Version = 1;
                        if (pfn_ctlGetDeviceProperties(ctlDevs[d], &props) == 0) {
                            for (auto& ad : m_adapters) {
                                if (ad.vendor == GPU_VENDOR_INTEL) {
                                    if ((ad.luid.HighPart == props.adapter_luid.HighPart &&
                                         ad.luid.LowPart  == props.adapter_luid.LowPart) ||
                                        (devCount == 1)) {
                                        ad.ctlDevice = ctlDevs[d];
                                        ad.hasCtl = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return true;
        }
    }

    ShutdownIntelCtl();
    return false;
}

void GpuMonitor::ShutdownIntelCtl() {
    if (m_ctlLoaded && pfn_ctlClose && m_ctlContext) {
        pfn_ctlClose(m_ctlContext);
    }
    m_ctlContext = nullptr;
    m_ctlLoaded = false;
    for (auto& ad : m_adapters) {
        ad.ctlDevice = nullptr;
        ad.hasCtl = false;
    }
    if (m_hIntelCtl) {
        FreeLibrary(m_hIntelCtl);
        m_hIntelCtl = NULL;
    }
}

bool GpuMonitor::InitWddm() {
    if (m_wddmLoaded) return true;

    m_hGdi32 = GetModuleHandleW(L"gdi32.dll");
    if (!m_hGdi32) m_hGdi32 = LoadLibraryW(L"gdi32.dll");
    if (!m_hGdi32) return false;

    pfnD3DKMTOpenAdapterFromLuid = (PFND3DKMT_OPENADAPTERFROMLUID)GetProcAddress(m_hGdi32, "D3DKMTOpenAdapterFromLuid");
    pfnD3DKMTQueryAdapterInfo    = (PFND3DKMT_QUERYADAPTERINFO)GetProcAddress(m_hGdi32, "D3DKMTQueryAdapterInfo");
    pfnD3DKMTCloseAdapter        = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(m_hGdi32, "D3DKMTCloseAdapter");

    m_wddmLoaded = (pfnD3DKMTOpenAdapterFromLuid && pfnD3DKMTQueryAdapterInfo && pfnD3DKMTCloseAdapter);
    return m_wddmLoaded;
}

bool GpuMonitor::InitPdhGpu() {
    if (m_pdhGpuValid) return true;

    PDH_STATUS status = PdhOpenQueryW(NULL, 0, &m_hGpuQuery);
    if (status != ERROR_SUCCESS) return false;

    PdhAddEnglishCounterW(m_hGpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_hGpuCounter);
    PdhAddEnglishCounterW(m_hGpuQuery, L"\\GPU Adapter Memory(*)\\Shared Usage", 0, &m_hSharedMemCounter);
    PdhAddEnglishCounterW(m_hGpuQuery, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &m_hDedMemCounter);

    if (m_hGpuCounter != NULL) {
        PdhCollectQueryData(m_hGpuQuery);
        m_pdhGpuValid = true;
    } else {
        PdhCloseQuery(m_hGpuQuery);
        m_hGpuQuery = NULL;
        m_hGpuCounter = NULL;
        m_hSharedMemCounter = NULL;
        m_hDedMemCounter = NULL;
        m_pdhGpuValid = false;
    }

    return m_pdhGpuValid;
}

void GpuMonitor::ShutdownPdhGpu() {
    if (m_hGpuQuery) {
        PdhCloseQuery(m_hGpuQuery);
        m_hGpuQuery = NULL;
        m_hGpuCounter = NULL;
        m_hSharedMemCounter = NULL;
        m_hDedMemCounter = NULL;
    }
    m_pdhGpuValid = false;
}

bool GpuMonitor::Init() {
    EnumeratePhysicalGpus();
    InitNvml();
    InitNvapi();
    InitAdl();
    InitIntelCtl();
    InitWddm();
    InitPdhGpu();
    return !m_adapters.empty();
}

void GpuMonitor::Shutdown() {
    ShutdownNvml();
    ShutdownNvapi();
    ShutdownAdl();
    ShutdownIntelCtl();
    ShutdownPdhGpu();

    for (auto& ad : m_adapters) {
        if (ad.pAdapter) {
            ad.pAdapter->Release();
            ad.pAdapter = nullptr;
        }
    }
    m_adapters.clear();

    if (m_pFactory) {
        m_pFactory->Release();
        m_pFactory = nullptr;
    }
}

void GpuMonitor::Collect(std::vector<GpuInfo>& outGpus) {
    outGpus.clear();

    if (!m_pdhGpuValid) {
        InitPdhGpu();
    }

    std::vector<BYTE> engBuffer;
    DWORD engItemCount = 0;
    std::vector<BYTE> sharedMemBuffer;
    DWORD sharedMemItemCount = 0;
    std::vector<BYTE> dedMemBuffer;
    DWORD dedMemItemCount = 0;

    if (m_pdhGpuValid) {
        PDH_STATUS status = PdhCollectQueryData(m_hGpuQuery);
        if (status == ERROR_SUCCESS) {
            if (m_hGpuCounter) {
                DWORD bufSize = 0;
                PdhGetFormattedCounterArrayW(m_hGpuCounter, PDH_FMT_DOUBLE, &bufSize, &engItemCount, NULL);
                if (bufSize > 0) {
                    engBuffer.resize(bufSize);
                    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(engBuffer.data());
                    PdhGetFormattedCounterArrayW(m_hGpuCounter, PDH_FMT_DOUBLE, &bufSize, &engItemCount, items);
                }
            }

            if (m_hSharedMemCounter) {
                DWORD bufSize = 0;
                PdhGetFormattedCounterArrayW(m_hSharedMemCounter, PDH_FMT_LARGE, &bufSize, &sharedMemItemCount, NULL);
                if (bufSize > 0) {
                    sharedMemBuffer.resize(bufSize);
                    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(sharedMemBuffer.data());
                    PdhGetFormattedCounterArrayW(m_hSharedMemCounter, PDH_FMT_LARGE, &bufSize, &sharedMemItemCount, items);
                }
            }

            if (m_hDedMemCounter) {
                DWORD bufSize = 0;
                PdhGetFormattedCounterArrayW(m_hDedMemCounter, PDH_FMT_LARGE, &bufSize, &dedMemItemCount, NULL);
                if (bufSize > 0) {
                    dedMemBuffer.resize(bufSize);
                    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(dedMemBuffer.data());
                    PdhGetFormattedCounterArrayW(m_hDedMemCounter, PDH_FMT_LARGE, &bufSize, &dedMemItemCount, items);
                }
            }
        }
    }

    auto* engItems = engBuffer.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(engBuffer.data());
    auto* sharedMemItems = sharedMemBuffer.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(sharedMemBuffer.data());
    auto* dedMemItems = dedMemBuffer.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(dedMemBuffer.data());

    for (size_t i = 0; i < m_adapters.size(); ++i) {
        auto& ad = m_adapters[i];

        GpuInfo info;
        wcsncpy_s(info.label, ad.label, _TRUNCATE);
        wcsncpy_s(info.name, ad.name, _TRUNCATE);
        info.vendor = ad.vendor;
        info.luid = ad.luid;
        info.isAvailable = true;
        info.isDiscrete = ad.isDiscrete;
        info.showUsage = Settings::Get().IsGpuUsageVisible((int)i);
        info.showVram  = Settings::Get().IsGpuVramVisible((int)i);

        // --- Real-Time System-Wide Memory Telemetry (VRAM + Shared RAM) ---
        double dedUsedMB     = 0.0;
        double dedTotalMB    = (double)ad.dedicatedVram / (1024.0 * 1024.0);
        double sharedUsedMB  = 0.0;
        double sharedTotalMB = (double)ad.sharedSystemMemory / (1024.0 * 1024.0);

        auto sumPdhMem = [&](PDH_FMT_COUNTERVALUE_ITEM_W* items, DWORD count) -> double {
            double sum = 0.0;
            if (!items || count == 0) return 0.0;
            for (DWORD j = 0; j < count; ++j) {
                if (items[j].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                    items[j].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
                if (!items[j].szName) continue;
                for (const auto& luidPattern : ad.luidPatterns) {
                    if (ContainsSubstringI(items[j].szName, luidPattern.c_str())) {
                        if (items[j].FmtValue.largeValue > 0) {
                            sum += (double)items[j].FmtValue.largeValue / (1024.0 * 1024.0);
                        }
                        break;
                    }
                }
            }
            return sum;
        };

        // 1. Primary: PDH GPU Adapter Memory (system-wide real-time Dedicated + Shared usage)
        dedUsedMB    = sumPdhMem(dedMemItems, dedMemItemCount);
        sharedUsedMB = sumPdhMem(sharedMemItems, sharedMemItemCount);

        // 2. NVIDIA NVML system-wide Dedicated VRAM fallback/refinement
        if (ad.vendor == GPU_VENDOR_NVIDIA && ad.hasNvml && ad.nvmlDevice && pfn_nvmlDeviceGetMemoryInfo) {
            nvmlMemory_t nvMem = {};
            if (pfn_nvmlDeviceGetMemoryInfo(ad.nvmlDevice, &nvMem) == 0 && nvMem.total > 0) {
                if (dedUsedMB <= 0.0) {
                    dedUsedMB = (double)nvMem.used / (1024.0 * 1024.0);
                }
                if (dedTotalMB <= 0.0) {
                    dedTotalMB = (double)nvMem.total / (1024.0 * 1024.0);
                }
            }
        }

        // 3. DXGI QueryVideoMemoryInfo last-resort fallback (per-process if PDH unavailable)
        if (ad.pAdapter && (dedUsedMB <= 0.0 || sharedUsedMB <= 0.0)) {
            IDXGIAdapter3* pAdapter3 = nullptr;
            if (SUCCEEDED(ad.pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO memInfo = {};
                if (dedUsedMB <= 0.0 && SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                    dedUsedMB = (double)memInfo.CurrentUsage / (1024.0 * 1024.0);
                    if (dedTotalMB <= 0.0) dedTotalMB = (double)memInfo.Budget / (1024.0 * 1024.0);
                }
                if (sharedUsedMB <= 0.0 && SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memInfo))) {
                    sharedUsedMB = (double)memInfo.CurrentUsage / (1024.0 * 1024.0);
                    if (sharedTotalMB <= 0.0) sharedTotalMB = (double)memInfo.Budget / (1024.0 * 1024.0);
                }
                pAdapter3->Release();
            }
        }

        if (ad.isDiscrete) {
            info.vramUsedMB  = dedUsedMB;
            info.vramTotalMB = dedTotalMB;
        } else {
            // Integrated GPU uses Shared System RAM (plus small dedicated carveout if present)
            info.vramUsedMB  = sharedUsedMB + dedUsedMB;
            info.vramTotalMB = (sharedTotalMB > 0.0) ? (sharedTotalMB + dedTotalMB) : dedTotalMB;
        }
        info.sharedUsedMB  = sharedUsedMB;
        info.sharedTotalMB = sharedTotalMB;

        // --- Core Clock, Temperature & Board Power Resolution ---
        double clockMHz = 0.0;
        double tempC    = -1.0;
        double powerW   = -1.0;
        double usagePct = 0.0;

        // ─────────────────────────────────────────────────────
        // Tier 1A: NVIDIA Telemetry (NVML primary + NVAPI fallback)
        // ─────────────────────────────────────────────────────
        if (ad.vendor == GPU_VENDOR_NVIDIA) {
            if (ad.hasNvml && ad.nvmlDevice) {
                if (pfn_nvmlDeviceGetUtilizationRates) {
                    nvmlUtilization_t util = { 0, 0 };
                    if (pfn_nvmlDeviceGetUtilizationRates(ad.nvmlDevice, &util) == 0) {
                        usagePct = (double)util.gpu;
                    }
                }
                if (pfn_nvmlDeviceGetTemperature) {
                    unsigned int t = 0;
                    if (pfn_nvmlDeviceGetTemperature(ad.nvmlDevice, 0, &t) == 0 && t > 0) {
                        tempC = (double)t;
                    }
                }
                if (pfn_nvmlDeviceGetClockInfo) {
                    unsigned int clk = 0;
                    if (pfn_nvmlDeviceGetClockInfo(ad.nvmlDevice, 0 /* graphics */, &clk) == 0 && clk > 0) {
                        clockMHz = (double)clk;
                    }
                }
                if (pfn_nvmlDeviceGetPowerUsage) {
                    unsigned int mw = 0;
                    if (pfn_nvmlDeviceGetPowerUsage(ad.nvmlDevice, &mw) == 0 && mw > 0) {
                        powerW = (double)mw / 1000.0;
                    }
                }
            }

            // NVAPI Fallback for consumer GTX or laptops where NVML is restricted
            if (ad.hasNvapi && ad.nvapiHandle) {
                if (powerW <= 0.0 && pfn_NvAPI_GPU_ClientPowerTopologyGetStatus) {
                    NV_CLIENT_POWER_TOPOLOGY_STATUS_V1 top = {};
                    top.version = (DWORD)(sizeof(NV_CLIENT_POWER_TOPOLOGY_STATUS_V1) | (1 << 16));
                    if (pfn_NvAPI_GPU_ClientPowerTopologyGetStatus(ad.nvapiHandle, &top) == 0) {
                        double sumMw = 0.0;
                        for (DWORD e = 0; e < top.count && e < 4; ++e) {
                            sumMw += (double)top.entries[e].powerUsageMw;
                        }
                        if (sumMw > 0.0) powerW = sumMw / 1000.0;
                    }
                }
                if (clockMHz <= 0.0 && pfn_NvAPI_GPU_GetAllClockFrequencies) {
                    NV_GPU_CLOCK_FREQUENCIES_V2 freqs = {};
                    freqs.version = (DWORD)(sizeof(NV_GPU_CLOCK_FREQUENCIES_V2) | (2 << 16));
                    freqs.ClockType = 0; // Current
                    if (pfn_NvAPI_GPU_GetAllClockFrequencies(ad.nvapiHandle, &freqs) == 0) {
                        if (freqs.domain[0].bIsPresent && freqs.domain[0].frequency > 0) {
                            clockMHz = (double)freqs.domain[0].frequency / 1000.0; // kHz -> MHz
                        }
                    }
                }
                if (tempC <= 0.0 && pfn_NvAPI_GPU_GetThermalSettings) {
                    NV_GPU_THERMAL_SETTINGS_V2 therm = {};
                    therm.version = (DWORD)(sizeof(NV_GPU_THERMAL_SETTINGS_V2) | (2 << 16));
                    if (pfn_NvAPI_GPU_GetThermalSettings(ad.nvapiHandle, 0, &therm) == 0 && therm.count > 0) {
                        if (therm.sensor[0].currentTemp > 0) {
                            tempC = (double)therm.sensor[0].currentTemp;
                        }
                    }
                }
            }
        }
        // ─────────────────────────────────────────────────────
        // Tier 1B: AMD Radeon Telemetry (ADL2 PMLog + Overdrive)
        // ─────────────────────────────────────────────────────
        else if (ad.vendor == GPU_VENDOR_AMD && ad.hasAdl && ad.adlAdapterIndex >= 0) {
            // Primary: ADL2 PMLog Data Output (RDNA / RX 5000-9000+ / Vega)
            if (pfn_ADL2_New_QueryPMLogData_Get) {
                ADLPMLogDataOutput pmData = {};
                if (pfn_ADL2_New_QueryPMLogData_Get(m_adlContext, ad.adlAdapterIndex, &pmData) == 0) {
                    if (pmData.sensors[ADL_PMLOG_CLK_GFXCLK].supported && pmData.sensors[ADL_PMLOG_CLK_GFXCLK].value > 0) {
                        clockMHz = (double)pmData.sensors[ADL_PMLOG_CLK_GFXCLK].value;
                    }
                    if (pmData.sensors[ADL_PMLOG_TEMPERATURE_EDGE].supported && pmData.sensors[ADL_PMLOG_TEMPERATURE_EDGE].value > 0) {
                        tempC = (double)pmData.sensors[ADL_PMLOG_TEMPERATURE_EDGE].value;
                    }
                    if (pmData.sensors[ADL_PMLOG_BOARD_POWER].supported && pmData.sensors[ADL_PMLOG_BOARD_POWER].value > 0) {
                        powerW = (double)pmData.sensors[ADL_PMLOG_BOARD_POWER].value;
                    } else if (pmData.sensors[ADL_PMLOG_ASIC_POWER].supported && pmData.sensors[ADL_PMLOG_ASIC_POWER].value > 0) {
                        powerW = (double)pmData.sensors[ADL_PMLOG_ASIC_POWER].value;
                    }
                }
            }

            // Fallback: Overdrive 6 / OverdriveN (GCN / RX 400-500 series)
            if (powerW <= 0.0 && pfn_ADL2_Overdrive6_CurrentPower_Get) {
                int rawPwr = 0;
                if (pfn_ADL2_Overdrive6_CurrentPower_Get(m_adlContext, ad.adlAdapterIndex, 0, &rawPwr) == 0 && rawPwr > 0) {
                    powerW = (double)rawPwr / 256.0;
                }
            }
            if (clockMHz <= 0.0 && pfn_ADL2_OverdriveN_PerformanceStatus_Get) {
                ADLODNPerformanceStatus odStatus = {};
                if (pfn_ADL2_OverdriveN_PerformanceStatus_Get(m_adlContext, ad.adlAdapterIndex, &odStatus) == 0) {
                    int cClk = (odStatus.iCoreClock > 0) ? odStatus.iCoreClock : odStatus.iGfxClock;
                    if (cClk > 0) {
                        clockMHz = (double)cClk / 100.0; // 10 KHz -> MHz
                    }
                }
            }
            if (tempC <= 0.0) {
                int t = 0;
                if (pfn_ADL2_OverdriveN_Temperature_Get &&
                    pfn_ADL2_OverdriveN_Temperature_Get(m_adlContext, ad.adlAdapterIndex, 1, &t) == 0 && t > 0) {
                    tempC = (t > 1000) ? ((double)t / 1000.0) : (double)t;
                } else if (pfn_ADL2_Overdrive6_Temperature_Get &&
                    pfn_ADL2_Overdrive6_Temperature_Get(m_adlContext, ad.adlAdapterIndex, &t) == 0 && t > 0) {
                    tempC = (t > 1000) ? ((double)t / 1000.0) : (double)t;
                }
            }
        }
        // ─────────────────────────────────────────────────────
        // Tier 1C: Intel Arc Discrete Telemetry (ControlLib IGCL)
        // ─────────────────────────────────────────────────────
        else if (ad.vendor == GPU_VENDOR_INTEL && ad.hasCtl && ad.ctlDevice) {
            if (pfn_ctlFrequencyGetState) {
                ctl_freq_state_t fState = {};
                fState.Size = sizeof(ctl_freq_state_t);
                fState.Version = 1;
                if (pfn_ctlFrequencyGetState(ad.ctlDevice, nullptr, &fState) == 0 && fState.currentFreq > 0.0) {
                    clockMHz = fState.currentFreq;
                }
            }
            if (pfn_ctlTemperatureGetState) {
                ctl_temp_t tState = {};
                tState.Size = sizeof(ctl_temp_t);
                tState.Version = 1;
                if (pfn_ctlTemperatureGetState(ad.ctlDevice, nullptr, &tState) == 0 && tState.currentTemp > 0.0) {
                    tempC = tState.currentTemp;
                }
            }
            if (pfn_ctlPowerGetEnergyCounter) {
                ctl_power_energy_counter_t pCounter = {};
                pCounter.Size = sizeof(ctl_power_energy_counter_t);
                pCounter.Version = 1;
                if (pfn_ctlPowerGetEnergyCounter(ad.ctlDevice, &pCounter) == 0) {
                    if (ad.lastCtlTime > 0 && pCounter.timestamp > ad.lastCtlTime && pCounter.energy >= ad.lastCtlEnergy) {
                        ULONGLONG dEnergyUj = pCounter.energy - ad.lastCtlEnergy;
                        ULONGLONG dTimeUs = pCounter.timestamp - ad.lastCtlTime;
                        if (dTimeUs > 0) {
                            powerW = (double)dEnergyUj / (double)dTimeUs; // (uj / us) = Watts
                        }
                    }
                    ad.lastCtlEnergy = pCounter.energy;
                    ad.lastCtlTime = pCounter.timestamp;
                }
            }
        }

        // ─────────────────────────────────────────────────────
        // Tier 2: Universal Windows WDDM D3DKMT Kernel Fallback
        // (Applies to ANY GPU: AMD, NVIDIA, or Intel Arc/Xe)
        // ─────────────────────────────────────────────────────
        if (m_wddmLoaded && (clockMHz <= 0.0 || tempC <= 0.0 || (ad.isDiscrete && powerW <= 0.0))) {
            D3DKMT_OPENADAPTERFROMLUID openInfo = {};
            openInfo.AdapterLuid = ad.luid;
            if (pfnD3DKMTOpenAdapterFromLuid(&openInfo) == 0 && openInfo.hAdapter != 0) {
                // Adapter Performance Data (Temperature, Power)
                if (tempC <= 0.0 || (ad.isDiscrete && powerW <= 0.0)) {
                    D3DKMT_ADAPTER_PERFDATA perfData = {};
                    perfData.PhysicalAdapterIndex = 0;
                    D3DKMT_QUERYADAPTERINFO qInfo = {};
                    qInfo.hAdapter = openInfo.hAdapter;
                    qInfo.Type = KMTQAITYPE_ADAPTERPERFDATA; // 62
                    qInfo.pPrivateDriverData = &perfData;
                    qInfo.PrivateDriverDataSize = sizeof(perfData);

                    if (pfnD3DKMTQueryAdapterInfo(&qInfo) == 0) {
                        if (tempC <= 0.0 && perfData.Temperature > 0) {
                            if (perfData.Temperature > 1000) {
                                // Deci-Kelvin -> Celsius
                                tempC = ((double)perfData.Temperature / 10.0) - 273.15;
                            } else {
                                // Deci-Celsius -> Celsius
                                tempC = (double)perfData.Temperature / 10.0;
                            }
                            if (tempC < 0.0 || tempC > 125.0) tempC = -1.0;
                        }
                        if (powerW <= 0.0 && perfData.Power > 0) {
                            // If reported in milliwatts
                            if (perfData.Power > 1000) {
                                powerW = (double)perfData.Power / 1000.0;
                            }
                        }
                    }
                }

                // Node Performance Data (3D Engine Frequency)
                if (clockMHz <= 0.0) {
                    ULONGLONG maxFreqHz = 0;
                    for (UINT node = 0; node < 8; ++node) {
                        D3DKMT_NODE_PERFDATA nodeData = {};
                        nodeData.NodeOrdinal = node;
                        nodeData.PhysicalAdapterIndex = 0;
                        D3DKMT_QUERYADAPTERINFO qNode = {};
                        qNode.hAdapter = openInfo.hAdapter;
                        qNode.Type = KMTQAITYPE_NODEPERFDATA; // 61
                        qNode.pPrivateDriverData = &nodeData;
                        qNode.PrivateDriverDataSize = sizeof(nodeData);

                        if (pfnD3DKMTQueryAdapterInfo(&qNode) == 0 && nodeData.Frequency > maxFreqHz) {
                            maxFreqHz = nodeData.Frequency;
                        }
                    }
                    if (maxFreqHz > 0) {
                        clockMHz = (double)maxFreqHz / 1000000.0; // Hz -> MHz
                    }
                }

                D3DKMT_CLOSEADAPTER closeInfo = { openInfo.hAdapter };
                pfnD3DKMTCloseAdapter(&closeInfo);
            }
        }

        // ─────────────────────────────────────────────────────
        // Tier 3: GPU Engine Utilization Aggregation via PDH
        // (Aggregated across all LUIDs mapped to this physical GPU)
        // ─────────────────────────────────────────────────────
        if (usagePct <= 0.0 && engItems && engItemCount > 0) {
            std::map<std::wstring, double> engineTotals;

            for (DWORD j = 0; j < engItemCount; ++j) {
                if (engItems[j].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || engItems[j].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) {
                    if (!engItems[j].szName) continue;

                    bool matchesLuid = false;
                    for (const auto& luidPattern : ad.luidPatterns) {
                        if (ContainsSubstringI(engItems[j].szName, luidPattern.c_str())) {
                            matchesLuid = true;
                            break;
                        }
                    }

                    if (matchesLuid && engItems[j].FmtValue.doubleValue > 0.0) {
                        std::wstring engKey = L"other";
                        const wchar_t* pEng = wcsstr(engItems[j].szName, L"_eng_");
                        if (pEng) {
                            engKey = pEng + 1;
                        } else {
                            const wchar_t* pType = wcsstr(engItems[j].szName, L"engtype_");
                            if (pType) engKey = pType;
                        }

                        engineTotals[engKey] += engItems[j].FmtValue.doubleValue;
                    }
                }
            }

            double peakEngineUsage = 0.0;
            for (const auto& [eng, val] : engineTotals) {
                if (val > peakEngineUsage) {
                    peakEngineUsage = val;
                }
            }
            usagePct = std::clamp(peakEngineUsage, 0.0, 100.0);
        }

        // If power was not reported by hardware API on a discrete GPU (e.g. GTX 960 NVML limitation),
        // compute calibrated real-time board power using GPU TDP model
        if (ad.isDiscrete && powerW <= 0.0) {
            double idleW = 10.0;
            double tdp = GetEstimatedGpuTdpWatts(ad.name, ad.vendor, ad.dedicatedVram, idleW);
            double uRatio = std::clamp(usagePct / 100.0, 0.0, 1.0);
            double cRatio = (clockMHz > 300.0) ? std::clamp(clockMHz / 1200.0, 0.35, 1.35) : 0.35;
            powerW = idleW + (tdp - idleW) * std::pow(uRatio, 1.05) * cRatio;
        }

        info.usagePct = usagePct;
        info.clockMHz = clockMHz;
        info.tempC    = tempC;
        info.powerW   = powerW;

        outGpus.push_back(info);
    }
}

} // namespace LV
