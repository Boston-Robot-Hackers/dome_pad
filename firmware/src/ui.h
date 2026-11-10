#pragma once
// Shared look for all tabs: palette, layout constants and anti-aliased text helpers.
// Defined in main.cpp.

#include <Arduino.h>
#include <TFT_eSPI.h>

// Palette: dark surfaces, one accent for anything pressed or selected, red only for STOP.
constexpr uint16_t COLOR_BG = 0x0882;       // #0E1116
constexpr uint16_t COLOR_SURFACE = 0x1905;  // #1C2128 keys and panels
constexpr uint16_t COLOR_RAISED = 0x2987;   // #2D333B the selected tab
constexpr uint16_t COLOR_TEXT = 0xE77E;     // #E6EDF3
constexpr uint16_t COLOR_DIM = 0x8CB3;      // #8B949E
constexpr uint16_t COLOR_ACCENT = 0x2EB7;   // #2DD4BF teal
constexpr uint16_t COLOR_WARN = 0xF5E7;     // #F2BC3C amber
constexpr uint16_t COLOR_STOP = 0xFA89;     // #F85149 red, also errors

constexpr int RADIUS = 10;  // corner radius of keys

// 320x240 landscape: status line, tab bar, then the page.
constexpr int SCREEN_W = 320, SCREEN_H = 240;
constexpr int STATUS_H = 20;
constexpr int PAGE_Y = 58;

extern TFT_eSPI tft;
extern const uint8_t* const FONT_TEXT;    // Noto Sans Bold 15
extern const uint8_t* const FONT_NUMBER;  // Noto Sans Mono SemiCondensed Bold 20

void useFont(const uint8_t* font);
// Text anti-aliased against bg, which must be the colour already under it.
void drawText(const String& s, int x, int y, uint8_t datum, uint16_t color, uint16_t bg,
              const uint8_t* font = FONT_TEXT);
int textWidth(const String& s, const uint8_t* font = FONT_TEXT);
