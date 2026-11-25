#include "settings.h"

namespace LV {

Settings& Settings::Get() {
    static Settings instance;
    return instance;
}

std::wstring Settings::GetSettingsFilePath() const {
    wchar_t appData[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\" + SETTINGS_DIR;
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\" + SETTINGS_FILE;
    }
    return std::wstring(SETTINGS_FILE);
}

void Settings::Load() {
    std::wstring path = GetSettingsFilePath();

    m_data.posX         = (int)GetPrivateProfileIntW(L"Window", L"PosX", -1, path.c_str());
    m_data.posY         = (int)GetPrivateProfileIntW(L"Window", L"PosY", -1, path.c_str());
    int savedW          = (int)GetPrivateProfileIntW(L"Window", L"Width", -1, path.c_str());
    int savedH          = (int)GetPrivateProfileIntW(L"Window", L"Height", -1, path.c_str());
    m_data.hasSavedSize = (savedW >= MIN_WIDTH && savedH >= MIN_HEIGHT);
    m_data.width        = (savedW > 0) ? savedW : DEFAULT_WIDTH;
    m_data.height       = (savedH > 0) ? savedH : DEFAULT_HEIGHT;
    m_data.monitorIndex = (int)GetPrivateProfileIntW(L"Window", L"MonitorIndex", -1, path.c_str());
    m_data.refreshMs    = (int)GetPrivateProfileIntW(L"Window", L"RefreshMs", 1000, path.c_str());
    m_data.opacityPct   = (int)GetPrivateProfileIntW(L"Window", L"OpacityPct", 88, path.c_str());
    m_data.locked       = GetPrivateProfileIntW(L"Window", L"Locked", 0, path.c_str()) != 0;
    m_data.clickThrough = GetPrivateProfileIntW(L"Window", L"ClickThrough", 0, path.c_str()) != 0;
    m_data.alwaysOnTop  = GetPrivateProfileIntW(L"Window", L"AlwaysOnTop", 1, path.c_str()) != 0;
    m_data.theme        = (AppTheme)GetPrivateProfileIntW(L"Window", L"Theme", (int)THEME_NEON, path.c_str());

    if (m_data.theme >= THEME_COUNT) m_data.theme = THEME_NEON;
    if (m_data.width < MIN_WIDTH) m_data.width = MIN_WIDTH;
    if (m_data.width > MAX_WIDTH) m_data.width = MAX_WIDTH;
    if (m_data.height < MIN_HEIGHT) m_data.height = MIN_HEIGHT;
    if (m_data.height > MAX_HEIGHT) m_data.height = MAX_HEIGHT;

    // Core Metrics
    m_data.showNetwork        = GetPrivateProfileIntW(L"Sections", L"ShowNetwork", 1, path.c_str()) != 0;
    m_data.showPing           = GetPrivateProfileIntW(L"Sections", L"ShowPing", 1, path.c_str()) != 0;
    m_data.showCpu            = GetPrivateProfileIntW(L"Sections", L"ShowCPU", 1, path.c_str()) != 0;
    m_data.showRam            = GetPrivateProfileIntW(L"Sections", L"ShowRAM", 1, path.c_str()) != 0;
    m_data.showPower          = GetPrivateProfileIntW(L"Sections", L"ShowPower", 1, path.c_str()) != 0;
    m_data.showPowerBreakdown = GetPrivateProfileIntW(L"Sections", L"ShowPowerBreakdown", 1, path.c_str()) != 0;

    // Multi-GPU Toggles (Support up to 8 GPUs)
    for (int i = 0; i < 8; ++i) {
        wchar_t keyUsage[32], keyVram[32];
        swprintf_s(keyUsage, L"ShowGPU_%d_Usage", i);
        swprintf_s(keyVram, L"ShowGPU_%d_VRAM", i);

        m_data.gpuToggles[keyUsage] = GetPrivateProfileIntW(L"GpuToggles", keyUsage, 1, path.c_str()) != 0;
        m_data.gpuToggles[keyVram]  = GetPrivateProfileIntW(L"GpuToggles", keyVram, 1, path.c_str()) != 0;
    }

    // Multi-Monitor FPS Toggles (Support up to 8 Monitors)
    for (int i = 0; i < 8; ++i) {
        wchar_t keyFps[32];
        swprintf_s(keyFps, L"ShowMonitor_%d_FPS", i + 1);

        m_data.monitorFpsToggles[keyFps] = GetPrivateProfileIntW(L"MonitorFpsToggles", keyFps, 1, path.c_str()) != 0;
    }

    m_data.runAtStartup = IsRunAtStartup();
}

void Settings::Save() {
    std::wstring path = GetSettingsFilePath();

    auto writeInt = [&](const wchar_t* sec, const wchar_t* key, int val) {
        wchar_t buf[32];
        swprintf_s(buf, L"%d", val);
        WritePrivateProfileStringW(sec, key, buf, path.c_str());
    };

    writeInt(L"Window", L"PosX", m_data.posX);
    writeInt(L"Window", L"PosY", m_data.posY);
    writeInt(L"Window", L"Width", m_data.width);
    writeInt(L"Window", L"Height", m_data.height);
    writeInt(L"Window", L"MonitorIndex", m_data.monitorIndex);
    writeInt(L"Window", L"RefreshMs", m_data.refreshMs);
    writeInt(L"Window", L"OpacityPct", m_data.opacityPct);
    writeInt(L"Window", L"Locked", m_data.locked ? 1 : 0);
    writeInt(L"Window", L"ClickThrough", m_data.clickThrough ? 1 : 0);
    writeInt(L"Window", L"AlwaysOnTop", m_data.alwaysOnTop ? 1 : 0);
    writeInt(L"Window", L"Theme", (int)m_data.theme);

    // Core Metrics
    writeInt(L"Sections", L"ShowNetwork", m_data.showNetwork ? 1 : 0);
    writeInt(L"Sections", L"ShowPing", m_data.showPing ? 1 : 0);
    writeInt(L"Sections", L"ShowCPU", m_data.showCpu ? 1 : 0);
    writeInt(L"Sections", L"ShowRAM", m_data.showRam ? 1 : 0);
    writeInt(L"Sections", L"ShowPower", m_data.showPower ? 1 : 0);
    writeInt(L"Sections", L"ShowPowerBreakdown", m_data.showPowerBreakdown ? 1 : 0);

    // Multi-GPU Toggles
    for (const auto& [key, val] : m_data.gpuToggles) {
        writeInt(L"GpuToggles", key.c_str(), val ? 1 : 0);
    }

    // Multi-Monitor FPS Toggles
    for (const auto& [key, val] : m_data.monitorFpsToggles) {
        writeInt(L"MonitorFpsToggles", key.c_str(), val ? 1 : 0);
    }
}

void Settings::PruneStaleGpuToggles(int physicalGpuCount) {
    if (physicalGpuCount < 1) physicalGpuCount = 1;

    std::wstring path = GetSettingsFilePath();
    for (int i = physicalGpuCount; i < 16; ++i) {
        wchar_t keyUsage[32], keyVram[32];
        swprintf_s(keyUsage, L"ShowGPU_%d_Usage", i);
        swprintf_s(keyVram, L"ShowGPU_%d_VRAM", i);

        m_data.gpuToggles.erase(keyUsage);
        m_data.gpuToggles.erase(keyVram);

        WritePrivateProfileStringW(L"GpuToggles", keyUsage, NULL, path.c_str());
        WritePrivateProfileStringW(L"GpuToggles", keyVram, NULL, path.c_str());
    }
}

bool Settings::IsGpuUsageVisible(int gpuIndex) const {
    wchar_t key[32];
    swprintf_s(key, L"ShowGPU_%d_Usage", gpuIndex);
    auto it = m_data.gpuToggles.find(key);
    if (it != m_data.gpuToggles.end()) return it->second;
    return true; // Default ON
}

void Settings::SetGpuUsageVisible(int gpuIndex, bool visible) {
    wchar_t key[32];
    swprintf_s(key, L"ShowGPU_%d_Usage", gpuIndex);
    m_data.gpuToggles[key] = visible;
}

bool Settings::IsGpuVramVisible(int gpuIndex) const {
    wchar_t key[32];
    swprintf_s(key, L"ShowGPU_%d_VRAM", gpuIndex);
    auto it = m_data.gpuToggles.find(key);
    if (it != m_data.gpuToggles.end()) return it->second;
    return true; // Default ON
}

void Settings::SetGpuVramVisible(int gpuIndex, bool visible) {
    wchar_t key[32];
    swprintf_s(key, L"ShowGPU_%d_VRAM", gpuIndex);
    m_data.gpuToggles[key] = visible;
}

bool Settings::IsMonitorFpsVisible(int monitorIndex) const {
    wchar_t key[32];
    swprintf_s(key, L"ShowMonitor_%d_FPS", monitorIndex + 1);
    auto it = m_data.monitorFpsToggles.find(key);
    if (it != m_data.monitorFpsToggles.end()) return it->second;
    return true; // Default ON
}

void Settings::SetMonitorFpsVisible(int monitorIndex, bool visible) {
    wchar_t key[32];
    swprintf_s(key, L"ShowMonitor_%d_FPS", monitorIndex + 1);
    m_data.monitorFpsToggles[key] = visible;
}

void Settings::SetRunAtStartup(bool enable) {
    m_data.runAtStartup = enable;
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    if (enable) {
        wchar_t cmd[1024];
        swprintf_s(cmd,
            L"schtasks /create /sc onlogon /rl highest /tn \"LellaVERSE System Monitor\" /tr \"\\\"%s\\\"\" /f",
            exePath);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        wchar_t cmd[512];
        swprintf_s(cmd, L"schtasks /delete /tn \"LellaVERSE System Monitor\" /f");

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

bool Settings::IsRunAtStartup() const {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    wchar_t cmd[512] = L"schtasks /query /tn \"LellaVERSE System Monitor\"";
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DWORD exitCode = 1;
        WaitForSingleObject(pi.hProcess, 2000);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

} // namespace LV
