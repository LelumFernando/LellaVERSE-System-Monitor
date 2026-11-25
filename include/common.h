#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor (Universal Edition)
// Common Definitions, Telemetry Structs & Application Constants
// ═══════════════════════════════════════════════════════════════

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000000
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <dwmapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <icmpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <powrprof.h>
#include <objbase.h>
#include <gdiplus.h>
#include <intrin.h>

typedef LONG NTSTATUS;

#include <string>
#include <vector>
#include <array>
#include <map>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <memory>
#include <thread>
#include <chrono>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "kernel32.lib")

#define IDI_APP_ICON 101

namespace LV {

// Application Identifiers
constexpr wchar_t APP_NAME[]       = L"LellaVERSE System Monitor";
constexpr wchar_t APP_VERSION[]    = L"2.1.0 (Universal Multi-GPU & Multi-Monitor)";
constexpr wchar_t APP_CLASS[]      = L"LellaVERSE_Overlay_Class";
constexpr wchar_t APP_MUTEX[]      = L"Global\\LellaVERSE_SystemMonitor_SingleInstance";
constexpr wchar_t SETTINGS_DIR[]   = L"LellaVERSE_System_Monitor";
constexpr wchar_t SETTINGS_FILE[]  = L"settings.ini";

constexpr UINT WM_TRAYICON        = WM_USER + 100;
constexpr UINT TIMER_TELEMETRY    = 1;

// Default Dimensions & Resize Limits
constexpr int DEFAULT_WIDTH        = 280;
constexpr int DEFAULT_HEIGHT       = 480;
constexpr int MIN_WIDTH            = 190;
constexpr int MIN_HEIGHT           = 140;
constexpr int MAX_WIDTH            = 1400;
constexpr int MAX_HEIGHT           = 1400;
constexpr int RESIZE_BORDER        = 8; // Border drag hit-test thickness

// 5 Built-in Themes
enum AppTheme : uint8_t {
    THEME_NEON         = 0, // LellaVERSE Neon (Pitch black + Neon Cyan & Vibrant Purple)
    THEME_STANDBY_DARK = 1, // StandBy Dark Glass (Charcoal + Color-coded accents)
    THEME_CRIMSON      = 2, // Crimson Gamer (Obsidian + Neon Red & Amber)
    THEME_MATRIX       = 3, // Emerald Matrix (Slate + Phosphor Green)
    THEME_FROST        = 4, // Minimal Frost (Translucent frosted light + Dark slate text)
    THEME_COUNT        = 5
};

inline const wchar_t* ThemeName(AppTheme t) {
    static const wchar_t* names[] = {
        L"LellaVERSE Neon",
        L"StandBy Dark Glass",
        L"Crimson Gamer",
        L"Emerald Matrix",
        L"Minimal Frost"
    };
    return (t < THEME_COUNT) ? names[t] : L"Unknown";
}

// GPU Vendor Categorization
enum GpuVendor : uint8_t {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_NVIDIA  = 1,
    GPU_VENDOR_AMD     = 2,
    GPU_VENDOR_INTEL   = 3
};

// Per-GPU Telemetry Data
struct GpuInfo {
    wchar_t   label[16]         = { 0 }; // e.g. "GPU 0", "GPU 1"
    wchar_t   name[64]          = { 0 }; // e.g. "NVIDIA GeForce GTX 960"
    GpuVendor vendor            = GPU_VENDOR_UNKNOWN;
    LUID      luid              = {};
    double    usagePct          = 0.0;
    double    clockMHz          = 0.0;
    double    tempC             = -1.0;
    double    powerW            = -1.0;
    double    vramUsedMB        = 0.0;
    double    vramTotalMB       = 0.0;
    double    sharedUsedMB      = 0.0;
    double    sharedTotalMB     = 0.0;
    bool      isAvailable       = false;
    bool      isDiscrete        = false;
    bool      showUsage         = true;
    bool      showVram          = true;
};

// Per-Monitor FPS Telemetry Data
struct MonitorFpsInfo {
    int       index             = 0;
    wchar_t   label[16]         = { 0 }; // e.g. "Monitor 1", "Monitor 2"
    wchar_t   name[32]          = { 0 }; // e.g. "\\.\DISPLAY1"
    wchar_t   resolution[32]    = { 0 }; // e.g. "1920x1080"
    double    fps               = 0.0;   // Live presented FPS (0 = idle/static)
    bool      isPrimary         = false;
    bool      showFps           = true;
};

// Complete Telemetry Snapshot
struct TelemetrySnapshot {
    // Network
    double netDownKBs    = 0.0;
    double netUpKBs      = 0.0;
    double netPingMs     = -1.0; // -1 = N/A or timeout

    // CPU (Universal: Intel Core, AMD Ryzen, etc.)
    double cpuUsagePct   = 0.0;
    double cpuClockMHz   = 0.0;
    double cpuTempC      = -1.0; // -1 = N/A
    double cpuPowerW     = -1.0;
    wchar_t cpuName[64]  = { 0 };
    bool   isAmdCpu      = false;
    int    cpuCores      = 0;
    int    cpuThreads    = 0;

    // Physical RAM
    double ramUsedMB     = 0.0;
    double ramTotalMB    = 0.0;

    // Power (Universal Multi-Component Power Aggregation)
    double totalPowerW   = 0.0;
    double dramPowerW    = 0.0;
    double otherPowerW   = 0.0;
    bool   totalPowerAvail = false;
    wchar_t powerBreakdown[128] = { 0 };

    // Multi-GPU Telemetry (GPU 0, GPU 1, etc.)
    std::vector<GpuInfo> gpus;

    // Multi-Monitor FPS Telemetry (Monitor 1, Monitor 2, etc.)
    std::vector<MonitorFpsInfo> monitors;

    // Validity flags
    bool cpuTempAvail    = false;
    bool cpuPowerAvail   = false;
    bool pingSuccess     = false;
};

// Persistent Settings
struct AppSettings {
    int      posX            = -1; // -1 = auto placement
    int      posY            = -1;
    int      width           = DEFAULT_WIDTH;
    int      height          = DEFAULT_HEIGHT;
    int      monitorIndex    = -1; // -1 = secondary or fallback
    bool     hasSavedSize    = false; // runtime flag: true if size was loaded from INI
    int      refreshMs       = 1000;
    int      opacityPct      = 88;
    bool     locked          = false;
    bool     clickThrough    = false;
    bool     alwaysOnTop     = true;
    AppTheme theme           = THEME_NEON;
    bool     runAtStartup    = false;

    // Standard Core Metrics
    bool     showNetwork     = true;
    bool     showPing        = true;
    bool     showCpu         = true;
    bool     showRam         = true;
    bool     showPower       = true;
    bool     showPowerBreakdown = true;

    // Dynamic Multi-GPU toggles (keyed by "ShowGPU_%d_Usage", "ShowGPU_%d_VRAM", etc.)
    std::map<std::wstring, bool> gpuToggles;

    // Dynamic Multi-Monitor FPS toggles (keyed by "ShowMonitor_%d_FPS", etc.)
    std::map<std::wstring, bool> monitorFpsToggles;

    bool IsGpuUsageVisible(int gpuIndex) const {
        wchar_t key[32];
        swprintf_s(key, L"ShowGPU_%d_Usage", gpuIndex);
        auto it = gpuToggles.find(key);
        if (it != gpuToggles.end()) return it->second;
        return true;
    }

    void SetGpuUsageVisible(int gpuIndex, bool visible) {
        wchar_t key[32];
        swprintf_s(key, L"ShowGPU_%d_Usage", gpuIndex);
        gpuToggles[key] = visible;
    }

    bool IsGpuVramVisible(int gpuIndex) const {
        wchar_t key[32];
        swprintf_s(key, L"ShowGPU_%d_VRAM", gpuIndex);
        auto it = gpuToggles.find(key);
        if (it != gpuToggles.end()) return it->second;
        return true;
    }

    void SetGpuVramVisible(int gpuIndex, bool visible) {
        wchar_t key[32];
        swprintf_s(key, L"ShowGPU_%d_VRAM", gpuIndex);
        gpuToggles[key] = visible;
    }

    bool IsMonitorFpsVisible(int monitorIndex) const {
        wchar_t key[32];
        swprintf_s(key, L"ShowMonitor_%d_FPS", monitorIndex + 1);
        auto it = monitorFpsToggles.find(key);
        if (it != monitorFpsToggles.end()) return it->second;
        return true;
    }

    void SetMonitorFpsVisible(int monitorIndex, bool visible) {
        wchar_t key[32];
        swprintf_s(key, L"ShowMonitor_%d_FPS", monitorIndex + 1);
        monitorFpsToggles[key] = visible;
    }
};

// Monitor Details for Window Positioning
struct MonitorInfo {
    HMONITOR hMon;
    RECT     rcWork;
    RECT     rcMonitor;
    bool     isPrimary;
    int      index;
    wchar_t  name[64];
};

} // namespace LV
