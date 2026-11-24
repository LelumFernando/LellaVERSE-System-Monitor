#pragma once
// ═══════════════════════════════════════════════════════════════
// LellaVERSE System Monitor - Theme Palette Engine
// ═══════════════════════════════════════════════════════════════

#include "common.h"

namespace LV {

struct ThemePalette {
    DWORD background;      // ARGB base window background
    DWORD cardBackground;  // ARGB per-card / section container
    DWORD cardBorder;      // ARGB subtle card border
    DWORD primaryText;     // ARGB crisp readable text
    DWORD secondaryText;   // ARGB muted metric units / subtitles
    DWORD barBackground;   // ARGB progress bar background track
    DWORD separator;       // ARGB horizontal divider lines

    // Metric Accents
    DWORD netAccent;       // Network traffic
    DWORD pingGreen;       // Ping < 60 ms
    DWORD pingOrange;      // Ping 60 - 120 ms
    DWORD pingRed;         // Ping > 120 ms / timeout
    DWORD cpuAccent;       // CPU
    DWORD gpuAccent;       // GPU
    DWORD vramAccent;      // VRAM
    DWORD ramAccent;       // System RAM
    DWORD powerAccent;     // Power draw
    DWORD fpsAccent;       // FPS display
    DWORD warnTemp;        // Warning temp (> 75 C)
    DWORD critTemp;        // Critical temp (> 85 C)
};

inline ThemePalette GetThemePalette(AppTheme theme) {
    ThemePalette p{};

    // Universal alerts
    p.pingGreen  = 0xFF00E676; // Emerald Green (< 60 ms)
    p.pingOrange = 0xFFFF9100; // Orange (60 - 120 ms)
    p.pingRed    = 0xFFFF1744; // Red (> 120 ms / Timeout)
    p.warnTemp   = 0xFFFF9100; // Orange (> 75 C)
    p.critTemp   = 0xFFFF1744; // Red (> 85 C)

    switch (theme) {
    case THEME_NEON: // LellaVERSE Neon (Default)
        p.background     = 0xF00A0B10; // Pitch black with ~94% alpha
        p.cardBackground = 0x4D161924; // Translucent card plate
        p.cardBorder     = 0x3300F0FF; // Subtle neon cyan border
        p.primaryText    = 0xFFFFFFFF; // Pure white
        p.secondaryText  = 0xFF8E95A5; // Slate gray
        p.barBackground  = 0x33FFFFFF; // 20% white track
        p.separator      = 0x2400F0FF; // Faint neon divider
        p.netAccent      = 0xFF00F0FF; // Neon Cyan
        p.cpuAccent      = 0xFF00E5FF; // Electric Cyan
        p.gpuAccent      = 0xFFB829EA; // Vibrant Purple
        p.vramAccent     = 0xFFFFB300; // Amber
        p.ramAccent      = 0xFFB388FF; // Soft Violet
        p.powerAccent    = 0xFFFF8A65; // Coral
        p.fpsAccent      = 0xFF00F0FF; // Neon Cyan
        break;

    case THEME_STANDBY_DARK: // StandBy Dark Glass
        p.background     = 0xE8121418; // Charcoal ~90% alpha
        p.cardBackground = 0x38FFFFFF; // Glass card
        p.cardBorder     = 0x26FFFFFF; // Muted border
        p.primaryText    = 0xFFFFFFFF;
        p.secondaryText  = 0xFFA0A8B0;
        p.barBackground  = 0x30FFFFFF;
        p.separator      = 0x20FFFFFF;
        p.netAccent      = 0xFF38BDF8; // Sky blue
        p.cpuAccent      = 0xFF00E5FF; // Cyan
        p.gpuAccent      = 0xFF00E676; // Emerald green
        p.vramAccent     = 0xFFFFB300; // Amber
        p.ramAccent      = 0xFFB388FF; // Lavender
        p.powerAccent    = 0xFFFF8A65; // Warm coral
        p.fpsAccent      = 0xFF00E5FF; // Cyan
        break;

    case THEME_CRIMSON: // Crimson Gamer
        p.background     = 0xF20D090A; // Obsidian
        p.cardBackground = 0x4D241113; // Dark burgundy card
        p.cardBorder     = 0x4DFF1744; // Crimson border
        p.primaryText    = 0xFFFFFFFF;
        p.secondaryText  = 0xFFA8888E;
        p.barBackground  = 0x30FFFFFF;
        p.separator      = 0x33FF1744;
        p.netAccent      = 0xFFFF5252; // Salmon red
        p.cpuAccent      = 0xFFFF1744; // Neon Red
        p.gpuAccent      = 0xFFFF9100; // Vivid Amber
        p.vramAccent     = 0xFFFFC400; // Gold
        p.ramAccent      = 0xFFFF4081; // Pinkish red
        p.powerAccent    = 0xFFFF6E40; // Deep Orange
        p.fpsAccent      = 0xFFFF1744; // Neon Red
        break;

    case THEME_MATRIX: // Emerald Matrix
        p.background     = 0xF208100C; // Deep matrix slate
        p.cardBackground = 0x400E2618; // Translucent green plate
        p.cardBorder     = 0x4000E676; // Matrix border
        p.primaryText    = 0xFFE0FFE8; // Mint white
        p.secondaryText  = 0xFF709980; // Muted sage
        p.barBackground  = 0x30FFFFFF;
        p.separator      = 0x2B00E676;
        p.netAccent      = 0xFF00BFA5; // Teal
        p.cpuAccent      = 0xFF00E676; // Phosphor green
        p.gpuAccent      = 0xFF69F0AE; // Light green
        p.vramAccent     = 0xFFAEEA00; // Lime
        p.ramAccent      = 0xFF00E676; // Green
        p.powerAccent    = 0xFFFFD740; // Amber-gold
        p.fpsAccent      = 0xFF00E676; // Phosphor green
        break;

    case THEME_FROST: // Minimal Frost (Light theme)
        p.background     = 0xF0E8EEF5; // Frosted light ~94% alpha
        p.cardBackground = 0x80FFFFFF; // Bright translucent card
        p.cardBorder     = 0x4D0F172A; // Crisp slate border
        p.primaryText    = 0xFF0F172A; // Dark slate text
        p.secondaryText  = 0xFF64748B; // Cool muted gray
        p.barBackground  = 0x260F172A; // Dark track
        p.separator      = 0x260F172A;
        p.netAccent      = 0xFF0284C7; // Vivid deep sky
        p.cpuAccent      = 0xFF0891B2; // Cyan-slate
        p.gpuAccent      = 0xFF7C3AED; // Deep violet
        p.vramAccent     = 0xFFD97706; // Dark amber
        p.ramAccent      = 0xFF9333EA; // Purple
        p.powerAccent    = 0xFFEA580C; // Deep orange
        p.fpsAccent      = 0xFF0284C7; // Vivid sky
        break;
    }

    return p;
}

} // namespace LV
