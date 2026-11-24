#include "fps_monitor.h"
#include "settings.h"

namespace LV {

FpsMonitor& FpsMonitor::Get() {
    static FpsMonitor inst;
    return inst;
}

bool FpsMonitor::Init() {
    if (m_running) return true;

    QueryPerformanceFrequency(&m_qpcFreq);
    m_running = true;

    EnumerateActiveOutputs();
    return !m_monitors.empty();
}

void FpsMonitor::Shutdown() {
    m_running = false;

    for (auto& pMon : m_monitors) {
        if (pMon) {
            pMon->running = false;
            if (pMon->thread.joinable()) {
                pMon->thread.join();
            }
            if (pMon->pOutput) {
                pMon->pOutput->Release();
                pMon->pOutput = nullptr;
            }
            if (pMon->pAdapter) {
                pMon->pAdapter->Release();
                pMon->pAdapter = nullptr;
            }
        }
    }
    m_monitors.clear();
}

void FpsMonitor::EnumerateActiveOutputs() {
    IDXGIFactory1* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) {
        return;
    }

    IDXGIAdapter1* pAdapter = nullptr;
    UINT adapterIdx = 0;
    int monIdx = 0;

    while (pFactory->EnumAdapters1(adapterIdx, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 adDesc;
        pAdapter->GetDesc1(&adDesc);

        // Ignore software adapters
        if (!(adDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && adDesc.VendorId != 0x1414) {
            IDXGIOutput* pOutput = nullptr;
            UINT outIdx = 0;

            while (pAdapter->EnumOutputs(outIdx, &pOutput) != DXGI_ERROR_NOT_FOUND) {
                DXGI_OUTPUT_DESC outDesc;
                if (SUCCEEDED(pOutput->GetDesc(&outDesc)) && outDesc.AttachedToDesktop) {
                    auto pMon = std::make_unique<ActiveMonitorOutput>();
                    pMon->index = monIdx;
                    swprintf_s(pMon->label, L"Monitor %d", monIdx + 1);
                    wcsncpy_s(pMon->deviceName, outDesc.DeviceName, _TRUNCATE);

                    int w = outDesc.DesktopCoordinates.right - outDesc.DesktopCoordinates.left;
                    int h = outDesc.DesktopCoordinates.bottom - outDesc.DesktopCoordinates.top;
                    swprintf_s(pMon->resolution, L"%dx%d", w, h);

                    pMon->isPrimary = (outDesc.DesktopCoordinates.left == 0 && outDesc.DesktopCoordinates.top == 0);
                    pMon->hMonitor = outDesc.Monitor;
                    pMon->desktopCoordinates = outDesc.DesktopCoordinates;

                    pMon->pAdapter = pAdapter;
                    pMon->pAdapter->AddRef();
                    pMon->pOutput = pOutput;
                    pMon->pOutput->AddRef();

                    pMon->running = true;
                    pMon->enabled = Settings::Get().IsMonitorFpsVisible(monIdx);

                    // Launch dedicated lightweight capture worker
                    pMon->thread = std::thread(&FpsMonitor::MonitorWorkerThread, this, pMon.get());

                    m_monitors.push_back(std::move(pMon));
                    monIdx++;
                }
                pOutput->Release();
                outIdx++;
            }
        }
        pAdapter->Release();
        adapterIdx++;
    }

    pFactory->Release();
}

void FpsMonitor::UpdateToggles() {
    for (auto& pMon : m_monitors) {
        if (pMon) {
            pMon->enabled = Settings::Get().IsMonitorFpsVisible(pMon->index);
        }
    }
}

void FpsMonitor::MonitorWorkerThread(ActiveMonitorOutput* pMon) {
    if (!pMon || !pMon->pAdapter || !pMon->pOutput) return;

    // 1. Create D3D11 Device on owning adapter
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        pMon->pAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        0,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &pDevice,
        &featureLevel,
        &pContext
    );

    IDXGIOutput1* pOutput1 = nullptr;
    if (SUCCEEDED(hr)) {
        pMon->pOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&pOutput1);
    }

    IDXGIOutputDuplication* pDupl = nullptr;
    auto acquireDupl = [&]() {
        if (pDupl) {
            pDupl->Release();
            pDupl = nullptr;
        }
        if (pOutput1 && pDevice) {
            pOutput1->DuplicateOutput(pDevice, &pDupl);
        }
    };

    acquireDupl();

    uint64_t lastDwmFrame = 0;
    DWORD lastDuplAttempt = GetTickCount();

    while (pMon->running && m_running) {
        // Zero-waste: If user turned OFF this monitor's FPS, pause worker and consume 0% CPU
        if (!pMon->enabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        bool frameCaptured = false;

        // Path A: DXGI Desktop Duplication API (AcquireNextFrame with ~50ms timeout)
        if (pDupl) {
            DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
            IDXGIResource* pDesktopResource = nullptr;

            HRESULT duplHr = pDupl->AcquireNextFrame(50, &frameInfo, &pDesktopResource);

            if (SUCCEEDED(duplHr)) {
                // If a new frame was presented to the display
                if (frameInfo.TotalMetadataBufferSize > 0 || frameInfo.AccumulatedFrames > 0 || frameInfo.LastPresentTime.QuadPart > 0) {
                    LARGE_INTEGER now;
                    QueryPerformanceCounter(&now);

                    std::lock_guard<std::mutex> lock(pMon->mutex);
                    pMon->timestamps[pMon->head] = now.QuadPart;
                    pMon->head = (pMon->head + 1) % ActiveMonitorOutput::MAX_TS;
                    if (pMon->count < ActiveMonitorOutput::MAX_TS) {
                        pMon->count++;
                    }
                    frameCaptured = true;
                }

                if (pDesktopResource) {
                    pDesktopResource->Release();
                    pDesktopResource = nullptr;
                }
                pDupl->ReleaseFrame();
            } else if (duplHr == DXGI_ERROR_WAIT_TIMEOUT) {
                // Display is static / idle
                frameCaptured = false;
            } else if (duplHr == DXGI_ERROR_ACCESS_LOST) {
                // Display mode change or UAC prompt
                pDupl->Release();
                pDupl = nullptr;
                lastDuplAttempt = GetTickCount();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                pDupl->Release();
                pDupl = nullptr;
                lastDuplAttempt = GetTickCount();
            }
        } else {
            // Re-attempt duplication every 1000ms
            DWORD nowTick = GetTickCount();
            if (nowTick - lastDuplAttempt >= 1000) {
                lastDuplAttempt = nowTick;
                acquireDupl();
            }

            // Path B: DWM Composition Timing Fallback (dwmapi.dll)
            DWM_TIMING_INFO timing = {};
            timing.cbSize = sizeof(timing);
            if (SUCCEEDED(DwmGetCompositionTimingInfo(NULL, &timing))) {
                if (lastDwmFrame > 0 && timing.cFrameDisplayed > lastDwmFrame) {
                    uint64_t delta = timing.cFrameDisplayed - lastDwmFrame;
                    LARGE_INTEGER now;
                    QueryPerformanceCounter(&now);

                    std::lock_guard<std::mutex> lock(pMon->mutex);
                    for (uint64_t k = 0; k < delta && k < 4; ++k) {
                        pMon->timestamps[pMon->head] = now.QuadPart;
                        pMon->head = (pMon->head + 1) % ActiveMonitorOutput::MAX_TS;
                        if (pMon->count < ActiveMonitorOutput::MAX_TS) {
                            pMon->count++;
                        }
                    }
                    frameCaptured = true;
                }
                lastDwmFrame = timing.cFrameDisplayed;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    if (pDupl) {
        pDupl->Release();
        pDupl = nullptr;
    }
    if (pOutput1) {
        pOutput1->Release();
        pOutput1 = nullptr;
    }
    if (pContext) {
        pContext->Release();
        pContext = nullptr;
    }
    if (pDevice) {
        pDevice->Release();
        pDevice = nullptr;
    }
}

void FpsMonitor::Collect(std::vector<MonitorFpsInfo>& outMonitors) {
    outMonitors.clear();

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    int64_t nowQpc = now.QuadPart;
    int64_t oneSecAgoQpc = nowQpc - m_qpcFreq.QuadPart;

    for (size_t i = 0; i < m_monitors.size(); ++i) {
        auto* pMon = m_monitors[i].get();
        if (!pMon) continue;

        MonitorFpsInfo info;
        info.index = pMon->index;
        wcsncpy_s(info.label, pMon->label, _TRUNCATE);
        wcsncpy_s(info.name, pMon->deviceName, _TRUNCATE);
        wcsncpy_s(info.resolution, pMon->resolution, _TRUNCATE);
        info.isPrimary = pMon->isPrimary;
        info.showFps = Settings::Get().IsMonitorFpsVisible(pMon->index);

        if (!info.showFps) {
            info.fps = 0.0;
            outMonitors.push_back(info);
            continue;
        }

        int validCount = 0;
        int64_t newestQpc = 0;
        int64_t oldestQpc = 0;

        {
            std::lock_guard<std::mutex> lock(pMon->mutex);
            if (pMon->count > 0) {
                for (int j = 0; j < pMon->count; ++j) {
                    int idx = (pMon->head - 1 - j + ActiveMonitorOutput::MAX_TS) % ActiveMonitorOutput::MAX_TS;
                    int64_t ts = pMon->timestamps[idx];
                    if (ts >= oneSecAgoQpc) {
                        if (validCount == 0) newestQpc = ts;
                        oldestQpc = ts;
                        validCount++;
                    } else {
                        break;
                    }
                }
            }
        }

        if (validCount >= 2 && newestQpc > oldestQpc) {
            double deltaSec = (double)(newestQpc - oldestQpc) / (double)m_qpcFreq.QuadPart;
            if (deltaSec > 0.05) {
                info.fps = (double)(validCount - 1) / deltaSec;
            } else {
                info.fps = (double)validCount;
            }
        } else if (validCount == 1) {
            info.fps = 1.0;
        } else {
            info.fps = 0.0; // Clean idle: 0 FPS when display is static
        }

        pMon->currentFps.store(info.fps);
        outMonitors.push_back(info);
    }
}

} // namespace LV
