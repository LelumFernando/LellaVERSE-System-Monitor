#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Settings Management
// Supports dynamic per-GPU & per-Monitor FPS persistent toggles
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

class Settings {
public:
    static Settings& Get();

    void Load();
    void Save();

    AppSettings& Data() { return m_data; }
    const AppSettings& Data() const { return m_data; }

    void SetRunAtStartup(bool enable);
    bool IsRunAtStartup() const;

    // Per-GPU Toggles
    void PruneStaleGpuToggles(int physicalGpuCount);
    bool IsGpuUsageVisible(int gpuIndex) const;
    void SetGpuUsageVisible(int gpuIndex, bool visible);

    bool IsGpuVramVisible(int gpuIndex) const;
    void SetGpuVramVisible(int gpuIndex, bool visible);

    // Per-Monitor FPS Toggles
    bool IsMonitorFpsVisible(int monitorIndex) const;
    void SetMonitorFpsVisible(int monitorIndex, bool visible);

private:
    Settings() = default;
    std::wstring GetSettingsFilePath() const;

    AppSettings m_data;
};

} // namespace LV
