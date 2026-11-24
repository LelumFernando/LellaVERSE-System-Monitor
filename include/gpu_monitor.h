#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Universal Multi-GPU Monitor
// Vendor-agnostic telemetry across AMD Radeon, NVIDIA GeForce/RTX,
// Intel Arc discrete GPUs, and native Windows WDDM D3DKMT fallbacks
// ═══════════════════════════════════════════════════════════════

#include "common.h"
#include <d3dkmthk.h>

namespace LV {

// ─────────────────────────────────────────────────────────────
// 1. NVIDIA NVML & NVAPI Types
// ─────────────────────────────────────────────────────────────
typedef void* nvmlDevice_t;
typedef int   nvmlReturn_t;

struct nvmlUtilization_t {
    unsigned int gpu;
    unsigned int memory;
};

struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct nvmlPciInfo_t {
    char         busIdLegacy[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
    char         busId[32];
};

typedef void* NvPhysicalGpuHandle;
typedef void* (*fn_nvapi_QueryInterface)(unsigned int offset);

#define NVAPI_MAX_THERMAL_SENSORS_PER_GPU 3
struct NV_GPU_THERMAL_SETTINGS_V2 {
    DWORD version;
    DWORD count;
    struct {
        int controller;
        int defaultMinTemp;
        int defaultMaxTemp;
        int currentTemp;
        int target;
    } sensor[NVAPI_MAX_THERMAL_SENSORS_PER_GPU];
};

struct NV_GPU_CLOCK_FREQUENCIES_V2 {
    DWORD version;
    DWORD ClockType;
    struct {
        DWORD bIsPresent : 1;
        DWORD reserved   : 31;
        DWORD frequency; // in kHz
    } domain[32];
};

struct NV_CLIENT_POWER_TOPOLOGY_ENTRY {
    DWORD domain;
    DWORD powerUsageMw; // mW
    DWORD percentage;
};

struct NV_CLIENT_POWER_TOPOLOGY_STATUS_V1 {
    DWORD version;
    DWORD count;
    NV_CLIENT_POWER_TOPOLOGY_ENTRY entries[4];
};

// ─────────────────────────────────────────────────────────────
// 2. AMD ADL2 Types (PMLog & Overdrive)
// ─────────────────────────────────────────────────────────────
typedef void* ADL_CONTEXT_HANDLE;
typedef void* (__cdecl *ADL_MAIN_MALLOC_CALLBACK)(int);

struct ADLAdapterInfo {
    int  iSize;
    int  iAdapterIndex;
    char strUDID[256];
    int  iBusNumber;
    int  iDeviceNumber;
    int  iFunctionNumber;
    int  iVendorID;
    char strAdapterName[256];
    char strDisplayName[256];
    int  iPresent;
    int  iExist;
    char strDriverPath[256];
    char strDriverPathExt[256];
    char strPNPString[256];
    int  iOSDisplayIndex;
};

#define ADL_PMLOG_MAX_SENSORS 256
#define ADL_PMLOG_CLK_GFXCLK 0
#define ADL_PMLOG_TEMPERATURE_EDGE 2
#define ADL_PMLOG_BOARD_POWER 12
#define ADL_PMLOG_ASIC_POWER 13

struct ADLSingleSensorData {
    int supported;
    int value;
};

struct ADLPMLogDataOutput {
    ADLSingleSensorData sensors[ADL_PMLOG_MAX_SENSORS];
};

struct ADLODNPerformanceStatus {
    int iCoreClock;   // 10 KHz units: iCoreClock / 100.0 = MHz
    int iMemoryClock;
    int iDcefClock;
    int iGfxClock;
    int iUvdClock;
    int iVceClock;
};

// ─────────────────────────────────────────────────────────────
// 3. Intel Arc IGCL Types (ControlLib.dll)
// ─────────────────────────────────────────────────────────────
typedef void* ctl_api_handle_t;
typedef void* ctl_device_adapter_handle_t;

struct ctl_init_args_t {
    DWORD Size;
    DWORD Version;
    DWORD AppVersion;
    DWORD flags;
    DWORD SupportedVersion;
};

struct ctl_device_adapter_properties_t {
    DWORD Size;
    DWORD Version;
    DWORD device_id;
    DWORD rev_id;
    DWORD num_slices;
    DWORD num_sub_slices_per_slice;
    DWORD num_eus_per_sub_slice;
    DWORD num_threads_per_eu;
    DWORD graphics_engine_clock;
    DWORD memory_clock;
    DWORD max_memory_bandwidth;
    DWORD memory_size;
    DWORD adapter_bdf;
    LUID  adapter_luid;
    char  name[256];
};

struct ctl_power_energy_counter_t {
    DWORD     Size;
    DWORD     Version;
    ULONGLONG energy;    // microjoules
    ULONGLONG timestamp; // microseconds
};

struct ctl_freq_state_t {
    DWORD  Size;
    DWORD  Version;
    double currentFreq;  // MHz
    double targetFreq;
    double resolvedFreq;
    double minFreq;
    double maxFreq;
};

struct ctl_temp_t {
    DWORD  Size;
    DWORD  Version;
    double currentTemp;  // Celsius
};

// ─────────────────────────────────────────────────────────────
// Physical GPU Adapter Representation
// ─────────────────────────────────────────────────────────────
struct PhysicalGpuAdapter {
    int                       index = 0;
    wchar_t                   label[16] = { 0 }; // e.g. "GPU 0", "GPU 1"
    wchar_t                   name[64] = { 0 };
    GpuVendor                 vendor = GPU_VENDOR_UNKNOWN;
    LUID                      luid = {};
    std::vector<std::wstring> luidPatterns; // All LUID patterns mapped to this physical chip
    SIZE_T                    dedicatedVram = 0;
    SIZE_T                    sharedSystemMemory = 0;
    IDXGIAdapter1*            pAdapter = nullptr;
    bool                      isDiscrete = false;
    DWORD                     pciBusId = 0;

    // NVIDIA
    nvmlDevice_t              nvmlDevice = nullptr;
    bool                      hasNvml = false;
    NvPhysicalGpuHandle       nvapiHandle = nullptr;
    bool                      hasNvapi = false;

    // AMD
    int                       adlAdapterIndex = -1;
    bool                      hasAdl = false;

    // Intel Arc
    ctl_device_adapter_handle_t ctlDevice = nullptr;
    bool                      hasCtl = false;
    ULONGLONG                 lastCtlEnergy = 0;
    ULONGLONG                 lastCtlTime   = 0;
};

class GpuMonitor {
public:
    GpuMonitor();
    ~GpuMonitor();

    bool Init();
    void Shutdown();

    void Collect(std::vector<GpuInfo>& outGpus);

private:
    void EnumeratePhysicalGpus();

    // Vendor library dynamic loaders
    bool InitNvml();
    void ShutdownNvml();

    bool InitNvapi();
    void ShutdownNvapi();

    bool InitAdl();
    void ShutdownAdl();

    bool InitIntelCtl();
    void ShutdownIntelCtl();

    bool InitWddm();

    // Windows PDH GPU Engine & Memory query
    bool InitPdhGpu();
    void ShutdownPdhGpu();

    IDXGIFactory1* m_pFactory = nullptr;
    std::vector<PhysicalGpuAdapter> m_adapters;

    // NVML Dynamic Binding
    HMODULE m_hNvml = NULL;
    bool    m_nvmlLoaded = false;

    typedef nvmlReturn_t (*fn_nvmlInit_v2)();
    typedef nvmlReturn_t (*fn_nvmlShutdown)();
    typedef nvmlReturn_t (*fn_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetHandleByPciBusId)(const char*, nvmlDevice_t*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetCount)(unsigned int*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetTemperature)(nvmlDevice_t, int, unsigned int*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetClockInfo)(nvmlDevice_t, int, unsigned int*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);
    typedef nvmlReturn_t (*fn_nvmlDeviceGetPciInfo)(nvmlDevice_t, nvmlPciInfo_t*);

    fn_nvmlInit_v2                   pfn_nvmlInit                   = nullptr;
    fn_nvmlShutdown                  pfn_nvmlShutdown               = nullptr;
    fn_nvmlDeviceGetHandleByIndex    pfn_nvmlDeviceGetHandleByIndex = nullptr;
    fn_nvmlDeviceGetHandleByPciBusId pfn_nvmlDeviceGetHandleByPciBusId = nullptr;
    fn_nvmlDeviceGetCount            pfn_nvmlDeviceGetCount         = nullptr;
    fn_nvmlDeviceGetName             pfn_nvmlDeviceGetName          = nullptr;
    fn_nvmlDeviceGetUtilizationRates pfn_nvmlDeviceGetUtilizationRates = nullptr;
    fn_nvmlDeviceGetTemperature      pfn_nvmlDeviceGetTemperature   = nullptr;
    fn_nvmlDeviceGetClockInfo        pfn_nvmlDeviceGetClockInfo     = nullptr;
    fn_nvmlDeviceGetMemoryInfo       pfn_nvmlDeviceGetMemoryInfo    = nullptr;
    fn_nvmlDeviceGetPowerUsage       pfn_nvmlDeviceGetPowerUsage    = nullptr;
    fn_nvmlDeviceGetPciInfo          pfn_nvmlDeviceGetPciInfo       = nullptr;

    // NVAPI Dynamic Binding
    HMODULE m_hNvapi = NULL;
    bool    m_nvapiLoaded = false;

    typedef int (*fn_NvAPI_Initialize)();
    typedef int (*fn_NvAPI_Unload)();
    typedef int (*fn_NvAPI_EnumPhysicalGPUs)(NvPhysicalGpuHandle[64], int*);
    typedef int (*fn_NvAPI_GPU_GetBusId)(NvPhysicalGpuHandle, DWORD*);
    typedef int (*fn_NvAPI_GPU_GetThermalSettings)(NvPhysicalGpuHandle, DWORD, NV_GPU_THERMAL_SETTINGS_V2*);
    typedef int (*fn_NvAPI_GPU_GetAllClockFrequencies)(NvPhysicalGpuHandle, NV_GPU_CLOCK_FREQUENCIES_V2*);
    typedef int (*fn_NvAPI_GPU_ClientPowerTopologyGetStatus)(NvPhysicalGpuHandle, NV_CLIENT_POWER_TOPOLOGY_STATUS_V1*);

    fn_NvAPI_Initialize                      pfn_NvAPI_Initialize                      = nullptr;
    fn_NvAPI_Unload                          pfn_NvAPI_Unload                          = nullptr;
    fn_NvAPI_EnumPhysicalGPUs                pfn_NvAPI_EnumPhysicalGPUs                = nullptr;
    fn_NvAPI_GPU_GetBusId                    pfn_NvAPI_GPU_GetBusId                    = nullptr;
    fn_NvAPI_GPU_GetThermalSettings          pfn_NvAPI_GPU_GetThermalSettings          = nullptr;
    fn_NvAPI_GPU_GetAllClockFrequencies      pfn_NvAPI_GPU_GetAllClockFrequencies      = nullptr;
    fn_NvAPI_GPU_ClientPowerTopologyGetStatus pfn_NvAPI_GPU_ClientPowerTopologyGetStatus = nullptr;

    // AMD ADL2 Dynamic Binding
    HMODULE            m_hAdl       = NULL;
    ADL_CONTEXT_HANDLE m_adlContext = nullptr;
    bool               m_adlLoaded  = false;

    typedef int (*fn_ADL2_Main_Control_Create)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
    typedef int (*fn_ADL2_Main_Control_Destroy)(ADL_CONTEXT_HANDLE);
    typedef int (*fn_ADL2_Adapter_NumberOfAdapters_Get)(ADL_CONTEXT_HANDLE, int*);
    typedef int (*fn_ADL2_Adapter_AdapterInfo_Get)(ADL_CONTEXT_HANDLE, ADLAdapterInfo*, int);
    typedef int (*fn_ADL2_New_QueryPMLogData_Get)(ADL_CONTEXT_HANDLE, int, ADLPMLogDataOutput*);
    typedef int (*fn_ADL2_Overdrive6_CurrentPower_Get)(ADL_CONTEXT_HANDLE, int, int, int*);
    typedef int (*fn_ADL2_OverdriveN_PerformanceStatus_Get)(ADL_CONTEXT_HANDLE, int, ADLODNPerformanceStatus*);
    typedef int (*fn_ADL2_Overdrive6_Temperature_Get)(ADL_CONTEXT_HANDLE, int, int*);
    typedef int (*fn_ADL2_OverdriveN_Temperature_Get)(ADL_CONTEXT_HANDLE, int, int, int*);

    fn_ADL2_Main_Control_Create             pfn_ADL2_Main_Control_Create             = nullptr;
    fn_ADL2_Main_Control_Destroy            pfn_ADL2_Main_Control_Destroy            = nullptr;
    fn_ADL2_Adapter_NumberOfAdapters_Get     pfn_ADL2_Adapter_NumberOfAdapters_Get     = nullptr;
    fn_ADL2_Adapter_AdapterInfo_Get         pfn_ADL2_Adapter_AdapterInfo_Get         = nullptr;
    fn_ADL2_New_QueryPMLogData_Get          pfn_ADL2_New_QueryPMLogData_Get          = nullptr;
    fn_ADL2_Overdrive6_CurrentPower_Get     pfn_ADL2_Overdrive6_CurrentPower_Get     = nullptr;
    fn_ADL2_OverdriveN_PerformanceStatus_Get pfn_ADL2_OverdriveN_PerformanceStatus_Get = nullptr;
    fn_ADL2_Overdrive6_Temperature_Get      pfn_ADL2_Overdrive6_Temperature_Get      = nullptr;
    fn_ADL2_OverdriveN_Temperature_Get      pfn_ADL2_OverdriveN_Temperature_Get      = nullptr;

    // Intel Arc IGCL Dynamic Binding
    HMODULE          m_hIntelCtl  = NULL;
    ctl_api_handle_t m_ctlContext = nullptr;
    bool             m_ctlLoaded  = false;

    typedef int (*fn_ctlInit)(ctl_init_args_t*, ctl_api_handle_t*);
    typedef int (*fn_ctlClose)(ctl_api_handle_t);
    typedef int (*fn_ctlEnumerateDevices)(ctl_api_handle_t, DWORD*, ctl_device_adapter_handle_t*);
    typedef int (*fn_ctlGetDeviceProperties)(ctl_device_adapter_handle_t, ctl_device_adapter_properties_t*);
    typedef int (*fn_ctlPowerGetEnergyCounter)(ctl_device_adapter_handle_t, ctl_power_energy_counter_t*);
    typedef int (*fn_ctlFrequencyGetState)(ctl_device_adapter_handle_t, void*, ctl_freq_state_t*);
    typedef int (*fn_ctlTemperatureGetState)(ctl_device_adapter_handle_t, void*, ctl_temp_t*);

    fn_ctlInit                 pfn_ctlInit                 = nullptr;
    fn_ctlClose                pfn_ctlClose                = nullptr;
    fn_ctlEnumerateDevices     pfn_ctlEnumerateDevices     = nullptr;
    fn_ctlGetDeviceProperties  pfn_ctlGetDeviceProperties  = nullptr;
    fn_ctlPowerGetEnergyCounter pfn_ctlPowerGetEnergyCounter = nullptr;
    fn_ctlFrequencyGetState    pfn_ctlFrequencyGetState    = nullptr;
    fn_ctlTemperatureGetState  pfn_ctlTemperatureGetState  = nullptr;

    // Windows WDDM D3DKMT Dynamic Binding
    HMODULE m_hGdi32 = NULL;
    bool    m_wddmLoaded = false;

    PFND3DKMT_OPENADAPTERFROMLUID pfnD3DKMTOpenAdapterFromLuid = nullptr;
    PFND3DKMT_QUERYADAPTERINFO    pfnD3DKMTQueryAdapterInfo    = nullptr;
    PFND3DKMT_CLOSEADAPTER        pfnD3DKMTCloseAdapter        = nullptr;

    // PDH Fallback for Multi-GPU Engine & Memory Utilization
    PDH_HQUERY   m_hGpuQuery         = NULL;
    PDH_HCOUNTER m_hGpuCounter       = NULL; // \GPU Engine(*)\Utilization Percentage
    PDH_HCOUNTER m_hSharedMemCounter = NULL; // \GPU Adapter Memory(*)\Shared Usage
    PDH_HCOUNTER m_hDedMemCounter    = NULL; // \GPU Adapter Memory(*)\Dedicated Usage
    bool         m_pdhGpuValid       = false;
};

} // namespace LV
