# LellaVERSE System Monitor (Universal Edition)

[![Vibe Coded](https://img.shields.io/badge/Vibe%20Coded-%E2%9C%A8%20100%25-blueviolet?style=for-the-badge)](https://github.com/lellaverse)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20x64-0078D6?style=for-the-badge&logo=windows)](https://microsoft.com)
[![Architecture](https://img.shields.io/badge/Architecture-Native%20C%2B%2B17%20%2F%20Win32-00599C?style=for-the-badge&logo=c%2B%2B)](https://visualstudio.microsoft.com)
[![Footprint](https://img.shields.io/badge/Binary%20Size-329%20KB-00E676?style=for-the-badge)](#performance-benchmarks)

A minimalist, high-performance desktop hardware monitoring overlay engineered for Windows 10 & 11 x64. Built using pure Win32, C++17, and GDI+ per-pixel alpha blending, with responsive auto-resizing, 5 built-in visual themes, and hardware telemetry across Intel, AMD, and NVIDIA systems.

---

## Highlights

- **Universal Hardware Support:** Auto-adapts to Intel Core, AMD Ryzen, NVIDIA GeForce/RTX, AMD Radeon, and Intel Arc/Iris/UHD GPUs.
- **Dynamic Auto-Layout & Responsive Resizing:** Free border dragging with adaptive layout modes:
  - **Standard Mode (240px–360px):** Vertical cards with capacity progress bars.
  - **Narrow Mode (< 240px):** Ultra-compact single-line telemetry rows.
  - **Wide HUD Mode (Width > Height * 1.8):** Streamer-style horizontal top/bottom bar.
- **5 Custom Themes:**
  1. *LellaVERSE Neon* (Default: Pitch black `#0A0B10`, Neon Cyan `#00F0FF`, and Vibrant Purple `#B829EA`)
  2. *StandBy Dark Glass* (Muted charcoal glass `#121418` with component accents)
  3. *Crimson Gamer* (Obsidian `#0D090A` with Crimson Red `#FF1744` & Amber)
  4. *Emerald Matrix* (Slate `#08100C` with Phosphor Green `#00E676`)
  5. *Minimal Frost* (Frosted translucent light glass `#E8EEF5` with dark typography)
- **Primary Monitor Frame Rate (ETW):** Event Tracing for Windows (`Microsoft-Windows-DXGI` & `DxgKrnl`) tracking foreground game frame presentation rates, with adapter-independent primary display detection.
- **Network Bandwidth & ICMP Ping:** Real-time upload/download tracking via IP Helper API (`GetIfTable2`) and round-trip ping latency to `1.1.1.1` (fallback `8.8.8.8`) with green/orange/red color coding.
- **Zero External DLLs:** 100% statically linked runtime (`/MT`).

---

## Measured Benchmarks

| Metric | Target Specification | Measured Result |
|---|---|---|
| **Executable Size** | 2.0 – 4.0 MB | **329 KB** (336,896 bytes) |
| **Installer Size** | < 10 MB | **3.14 MB** |
| **Runtime Memory (Working Set)** | < 50 MB | **~24 MB** |
| **CPU Utilization** | < 1.0% | **~0.2%** (1000 ms poll rate) |
| **GPU 3D Engine Load** | 0.0% | **0.0%** (GDI+ 2D software blit) |
| **Loose DLL Dependencies** | 0 | **0** |

---

## Telemetry Pipeline

1. **Network Bandwidth:** `GetIfTable2` summing non-loopback active physical adapters over high-resolution QPC intervals.
2. **Ping Latency:** ICMP echo (`IcmpSendEcho`) to `1.1.1.1` (fallback `8.8.8.8`) on a background worker thread.
3. **Processor (CPU):** PDH queries (`% Processor Utility` / `% Processor Time`) + `CallNtPowerInformation` for core frequencies. Package temperature and power wattage via kernel MSR access.
4. **Graphics (GPU):**
   - **NVIDIA:** Dynamic runtime loading of `nvml.dll` for utilization, temperature, core clock, VRAM, and power draw.
   - **AMD Radeon / Intel Arc:** DXGI 1.4 `IDXGIAdapter3::QueryVideoMemoryInfo` for dedicated VRAM and Windows PDH counters for 3D engine utilization.
5. **Memory (RAM & VRAM):** `GlobalMemoryStatusEx` for physical RAM and dedicated VRAM allocation tracking with capacity bars.
6. **Total Power:** Aggregated CPU Package + GPU board power draw.
7. **Primary Monitor FPS:** Low-overhead ETW presentation event consumer session filtered to the foreground application.

---

## Controls & Interaction

- **Left-Click + Drag:** Move overlay across screens (when unlocked).
- **Edge / Corner Drag:** Smooth window resize (`WM_NCHITTEST`).
- **Right-Click / Tray Icon Menu:**
  - Lock / Unlock Position
  - Click-Through Mode (`WS_EX_TRANSPARENT`)
  - Always on Top toggle
  - Themes (5 styles)
  - Visible Metrics toggles (Network, Ping, CPU, GPU, VRAM, RAM, Power, FPS)
  - Opacity selector (100%, 85%, 70%, 55%, 40%, 25%)
  - Refresh Rate (500ms, 1000ms, 2000ms)
  - Select Display
  - Reset Position
  - Run at Windows Startup (Elevated Task Scheduler registration)
  - Exit

---

## Building from Source

### Requirements
- Windows 10/11 x64
- Visual Studio 2022 Build Tools (with C++ Desktop workload)
- Inno Setup 6 (optional, for installer creation)

### Build Commands
Run from Developer Command Prompt or execute `scripts\build.bat`:

```cmd
cd LELLAVERSE_SYSTEM_MONITOR
scripts\build.bat
```

To compile the setup installer:
```cmd
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\setup.iss
```
Outputs:
- Standalone EXE: `release\LellaVERSE_System_Monitor.exe`
- Windows Installer: `release\LellaVERSE_System_Monitor_Setup.exe`

---

## License

Copyright (c) 2026 Lelum Anuradha Fernando / LellaVERSE. Released under the MIT License.
