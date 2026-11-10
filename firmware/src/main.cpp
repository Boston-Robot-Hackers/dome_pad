// Yellow Pad, tabbed design: a touchscreen BLE gamepad for driving a ROS robot via teleop_twist_joy.
//
// Drive tab: holding an arrow moves the left stick in that direction and holds
// button 1 (the teleop enable / dead-man button). A, B and C hold buttons 2-4.
// Releasing stops. Settings tab: per-direction speeds in m/s and rad/s, saved in flash.
//
// The first design (arrows + speed panel in %) is in arrows_v1.cpp (env cyd_v1).

#include <Arduino.h>
#include <BleGamepad.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <sys/time.h>

// Anti-aliased fonts from TFT_eSPI's examples (Noto Sans, SIL Open Font License).
#include "fonts/NotoSansBold15.h"
#include "fonts/NotoSansMonoSCB20.h"
#include "ros_tab.h"
#include "ui.h"

// Touch controller is on its own SPI bus on the CYD.
constexpr int TOUCH_CS = 33;
constexpr int TOUCH_IRQ = 36;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_CLK = 25;

// Raw touch range, typical for the CYD. Adjust if taps land off-target.
constexpr int RAW_X_MIN = 200, RAW_X_MAX = 3700;
constexpr int RAW_Y_MIN = 240, RAW_Y_MAX = 3800;

constexpr int16_t AXIS_MIN = 0;
constexpr int16_t AXIS_MAX = 32767;
constexpr int16_t AXIS_CENTER = 16384;

// The robot's speed at full stick. Must match scale_linear.x and
// scale_angular.yaw in yellow_pad/config/yellow_pad.yaml.
constexpr int LINEAR_MAX = 1500;   // mm/s (1.5 m/s)
constexpr int ANGULAR_MAX = 1000;  // mrad/s (1.0 rad/s)

constexpr uint32_t LOOP_MS = 20;

// Power saving. 80 MHz is the lowest speed BLE supports, and plenty for touch and drawing.
// The backlight is a large part of the board's draw: 0 = off, 255 = full.
constexpr uint32_t CPU_MHZ = 80;
constexpr int BACKLIGHT_LEVEL = 128;

// After this long with no touch the backlight dims to DIM_LEVEL. The touch that
// brightens it again is ignored until the finger lifts, so a blind tap can't drive.
constexpr uint32_t DIM_AFTER_MS = 2 * 60 * 1000;
constexpr int DIM_LEVEL = 12;

// After this long with no touch the pad deep-sleeps: backlight and screen off, Bluetooth
// gone (the robot stops). A touch wakes it; it reboots and reconnects in a few seconds,
// and the waking touch is ignored until lifted.
constexpr uint32_t SLEEP_AFTER_MS = 10 * 60 * 1000;

// Runtime tracking, for measuring battery life. Minutes are saved to flash
// (namespace yellowpad_pwr) once a minute, so a power cut loses at most one.
// Holding the status bar for CHARGED_HOLD_MS marks the battery as charged.
constexpr uint32_t SAVE_EVERY_MS = 60 * 1000;
constexpr uint32_t CHARGED_HOLD_MS = 3000;
constexpr uint32_t LAST_RUN_SHOW_MS = 10 * 1000;

enum Dir { UP, DOWN, LEFT, RIGHT, DIR_COUNT, NONE = -1 };
enum Tab { DRIVE, ROS, SETTINGS, TAB_COUNT };
const char* const TAB_NAMES[TAB_COUNT] = {"Drive", "ROS", "Settings"};

constexpr int ACTION_COUNT = 3;
const char* const ACTION_LABELS[ACTION_COUNT] = {"A", "B", "C"};
const uint8_t ACTION_BUTTONS[ACTION_COUNT] = {BUTTON_2, BUTTON_3, BUTTON_4};

// A numeric setting, stored in thousandths of its unit (mm/s, mrad/s).
struct Setting {
  const char* label;
  const char* unit;
  const char* key;  // NVS key
  int step, max, fallback;
  int value;
};

// The first four are the speeds, indexed by Dir; their max is the full-stick speed.
// New settings go after them.
Setting settings[] = {
    {"Forward", "m/s", "up", 50, LINEAR_MAX, 150, 0},
    {"Back", "m/s", "down", 50, LINEAR_MAX, 150, 0},
    {"Left", "rad/s", "left", 100, ANGULAR_MAX, 500, 0},
    {"Right", "rad/s", "right", 100, ANGULAR_MAX, 500, 0},
};
constexpr int SETTING_COUNT = sizeof(settings) / sizeof(settings[0]);

struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// Tab touch areas, matching the segments drawTabs() draws. Layout constants are in ui.h.
const Rect TAB_RECTS[TAB_COUNT] = {{0, 20, 107, 36}, {107, 20, 106, 36}, {213, 20, 107, 36}};

// Drive page.
constexpr int PAD_CX = 92, PAD_CY = 149, KEY = 56, HALF = KEY / 2;
const Rect ARROW_RECTS[DIR_COUNT] = {
    {PAD_CX - HALF, PAD_CY - HALF - KEY, KEY, KEY},  // up
    {PAD_CX - HALF, PAD_CY + HALF, KEY, KEY},        // down
    {PAD_CX - HALF - KEY, PAD_CY - HALF, KEY, KEY},  // left
    {PAD_CX + HALF, PAD_CY - HALF, KEY, KEY},        // right
};
const Rect STOP_RECT = {PAD_CX - HALF, PAD_CY - HALF, KEY, KEY};
const Rect ACTION_RECTS[ACTION_COUNT] = {{196, 66, 116, 52}, {196, 124, 116, 52}, {196, 182, 116, 52}};

// Settings page. Rows fit four settings above the -/+ buttons; more will need paging.
// The -/+ buttons run to the screen edges; the drawn area is the touch area.
constexpr int ROW_Y = 60, ROW_H = 30;
const Rect MINUS_RECT = {0, 182, 158, 58};
const Rect PLUS_RECT = {162, 182, 158, 58};

const uint8_t* const FONT_TEXT = NotoSansBold15;
const uint8_t* const FONT_NUMBER = NotoSansMonoSCB20;

TFT_eSPI tft;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
BleGamepad gamepad("Yellow Pad", "pitosalas", 100);
Preferences prefs;

Tab tab = DRIVE;
int selected = 0;
Dir active = NONE;
int actionHeld = -1;
int adjustHeld = 0;  // -1 while the finger is on -, +1 on +
bool stopHeld = false;
bool wasTouched = false;
bool wasConnected = false;
uint32_t lastActivityMs = 0;
bool dimmed = false;
bool swallowTouch = true;  // a touch held at boot or when the screen woke; ignored until lifted

// Since-charge totals are the offset plus this session's minutes. The offset is the
// saved total at boot, or minus the session minutes when marked charged mid-session.
// A session runs from power-on to power loss, through any deep sleeps. RTC_DATA_ATTR
// values survive deep sleep and are reset on every other boot.
Preferences power;
RTC_DATA_ATTR int32_t chargeOffsetMin = 0;
RTC_DATA_ATTR int32_t dimOffsetMin = 0;
RTC_DATA_ATTR int32_t sleepOffsetMin = 0;
RTC_DATA_ATTR uint32_t lastRunMin = 0;
RTC_DATA_ATTR bool brownoutSeen = false;  // a brownout reset happened since the last charge
RTC_DATA_ATTR uint64_t sessionBaseMs = 0;   // session time before this boot, awake and asleep
RTC_DATA_ATTR uint32_t dimmedSessionMs = 0;
RTC_DATA_ATTR uint32_t sleptSessionMs = 0;
RTC_DATA_ATTR int64_t sleepStartUs = 0;     // RTC time when the last deep sleep began
bool wokeFromSleep = false;
uint32_t dimStartMs = 0;
uint32_t lastSaveMs = 0;
bool holdingStatus = false;
bool holdDone = false;
uint32_t holdStartMs = 0;
String shownRuntime;

Rect settingRect(int i) { return {8, static_cast<int16_t>(ROW_Y + i * ROW_H), 304, ROW_H - 2}; }

// Loading a font from flash is cheap, so switch freely; this skips reloading the current one.
void useFont(const uint8_t* font) {
  static const uint8_t* loaded = nullptr;
  if (font == loaded) return;
  if (loaded) tft.unloadFont();
  tft.loadFont(font);
  loaded = font;
}

void drawText(const String& s, int x, int y, uint8_t datum, uint16_t color, uint16_t bg, const uint8_t* font) {
  useFont(font);
  tft.setTextColor(color, bg);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y);
}

int textWidth(const String& s, const uint8_t* font) {
  useFont(font);
  return tft.textWidth(s);
}

// A key is a rounded panel inset from its touch rect, so neighbours have a gap between them.
void drawKey(const Rect& r, int inset, uint16_t fill) {
  tft.fillSmoothRoundRect(r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset, RADIUS, fill, COLOR_BG);
}

// A thin chevron pointing in direction d, centred on cx, cy.
void drawChevron(int cx, int cy, Dir d, uint16_t color, uint16_t bg) {
  const float s = 8, h = s / 2, w = 3;
  float tx = cx, ty = cy, ax = cx, ay = cy, bx = cx, by = cy;  // tip and the two arm ends
  switch (d) {
    case UP: ty -= h; ay = by = cy + h; ax -= s; bx += s; break;
    case DOWN: ty += h; ay = by = cy - h; ax -= s; bx += s; break;
    case LEFT: tx -= h; ax = bx = cx + h; ay -= s; by += s; break;
    case RIGHT: tx += h; ax = bx = cx - h; ay -= s; by += s; break;
    default: return;
  }
  tft.drawWideLine(ax, ay, tx, ty, w, color, bg);
  tft.drawWideLine(tx, ty, bx, by, w, color, bg);
}

void drawArrow(Dir d) {
  const Rect& r = ARROW_RECTS[d];
  bool on = active == d;
  uint16_t fill = on ? COLOR_ACCENT : COLOR_SURFACE;
  drawKey(r, 3, fill);
  drawChevron(r.x + r.w / 2, r.y + r.h / 2, d, on ? COLOR_BG : COLOR_TEXT, fill);
}

// A red ring with STOP inside; filled red while touched.
void drawStop() {
  const Rect& r = STOP_RECT;
  int cx = r.x + r.w / 2, cy = r.y + r.h / 2, rad = r.w / 2 - 3;
  tft.fillSmoothCircle(cx, cy, rad, COLOR_STOP, COLOR_BG);
  if (!stopHeld) tft.fillSmoothCircle(cx, cy, rad - 2, COLOR_BG, COLOR_STOP);
  drawText("STOP", cx, cy, MC_DATUM, stopHeld ? COLOR_BG : COLOR_STOP, stopHeld ? COLOR_STOP : COLOR_BG);
}

void drawAction(int i) {
  const Rect& r = ACTION_RECTS[i];
  bool on = actionHeld == i;
  uint16_t fill = on ? COLOR_ACCENT : COLOR_SURFACE;
  drawKey(r, 2, fill);
  drawText(ACTION_LABELS[i], r.x + r.w / 2, r.y + r.h / 2, MC_DATUM, on ? COLOR_BG : COLOR_TEXT, fill, FONT_NUMBER);
}

// Label on the left; value (monospaced) and unit on the right. The selected row sits on a
// panel with an accent bar.
void drawSettingRow(int i) {
  const Setting& s = settings[i];
  Rect r = settingRect(i);
  bool sel = selected == i;
  uint16_t bg = sel ? COLOR_SURFACE : COLOR_BG;
  tft.fillRect(r.x, r.y, r.w, r.h, COLOR_BG);
  if (sel) {
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 8, COLOR_SURFACE, COLOR_BG);
    tft.fillSmoothRoundRect(r.x + 6, r.y + 7, 3, r.h - 14, 1, COLOR_ACCENT, COLOR_SURFACE);
  }
  int cy = r.y + r.h / 2;
  drawText(s.label, r.x + 18, cy, ML_DATUM, sel ? COLOR_TEXT : COLOR_DIM, bg);
  int right = r.x + r.w - 12;
  drawText(s.unit, right, cy, MR_DATUM, COLOR_DIM, bg);
  drawText(String(s.value / 1000.0f, 2), right - textWidth(s.unit) - 6, cy, MR_DATUM,
           sel ? COLOR_ACCENT : COLOR_TEXT, bg, FONT_NUMBER);
}

// - (sign -1) or + (sign +1), drawn as anti-aliased strokes.
void drawAdjust(int sign) {
  const Rect& r = sign < 0 ? MINUS_RECT : PLUS_RECT;
  bool on = adjustHeld == sign;
  uint16_t fill = on ? COLOR_ACCENT : COLOR_SURFACE, glyph = on ? COLOR_BG : COLOR_TEXT;
  // The touch rects run to the screen edges; keep the drawn keys 8 px in from them.
  int x = r.x + (sign < 0 ? 8 : 4), y = r.y + 6, w = r.w - 12, h = r.h - 12;
  tft.fillSmoothRoundRect(x, y, w, h, RADIUS, fill, COLOR_BG);
  int cx = x + w / 2, cy = y + h / 2, half = 9;
  tft.drawWideLine(cx - half, cy, cx + half, cy, 3, glyph, fill);
  if (sign > 0) tft.drawWideLine(cx, cy - half, cx, cy + half, 3, glyph, fill);
}

uint32_t sessionMin() { return (sessionBaseMs + millis()) / 60000; }

uint32_t dimSessionMin() {
  uint32_t ms = dimmedSessionMs + (dimmed ? millis() - dimStartMs : 0);
  return ms / 60000;
}

uint32_t sleepSessionMin() { return sleptSessionMs / 60000; }

uint32_t sinceChargeMin() { return chargeOffsetMin + static_cast<int32_t>(sessionMin()); }
uint32_t dimSinceChargeMin() { return dimOffsetMin + static_cast<int32_t>(dimSessionMin()); }
uint32_t sleepSinceChargeMin() { return sleepOffsetMin + static_cast<int32_t>(sleepSessionMin()); }

// Microseconds from the RTC clock, which keeps counting through deep sleep.
int64_t rtcNowUs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

String formatHM(uint32_t minutes) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu:%02lu", static_cast<unsigned long>(minutes / 60),
           static_cast<unsigned long>(minutes % 60));
  return buf;
}

// Right side of the status bar: the previous run for a few seconds after boot, then
// running time since the last charge.
String runtimeText() {
  if (lastRunMin > 0 && !wokeFromSleep && millis() < LAST_RUN_SHOW_MS) return "Last run " + formatHM(lastRunMin);
  return "Run " + formatHM(sinceChargeMin());
}

// Runtime on the left (amber if a brownout was seen since charge); connection dot
// and state on the right.
void drawStatus(bool connected) {
  int cy = STATUS_H / 2 + 2;
  tft.fillRect(0, 0, 320, STATUS_H, COLOR_BG);
  shownRuntime = runtimeText();
  drawText(shownRuntime, 12, cy, ML_DATUM, brownoutSeen ? COLOR_WARN : COLOR_DIM, COLOR_BG);
  const char* state = connected ? "Connected" : "Waiting for Bluetooth";
  drawText(state, 308, cy, MR_DATUM, connected ? COLOR_TEXT : COLOR_DIM, COLOR_BG);
  tft.fillSmoothCircle(308 - textWidth(state) - 10, cy, 3, connected ? COLOR_ACCENT : COLOR_DIM, COLOR_BG);
}

// A segmented control: one rounded track with the selected tab raised inside it.
void drawTabs() {
  constexpr int TRACK_X = 8, TRACK_Y = 25, TRACK_W = 304, TRACK_H = 28;
  tft.fillRect(0, STATUS_H, 320, PAGE_Y - STATUS_H, COLOR_BG);
  tft.fillSmoothRoundRect(TRACK_X, TRACK_Y, TRACK_W, TRACK_H, TRACK_H / 2, COLOR_SURFACE, COLOR_BG);
  int segW = (TRACK_W - 6) / TAB_COUNT;
  for (int t = 0; t < TAB_COUNT; t++) {
    bool on = tab == t;
    int x = TRACK_X + 3 + t * segW;
    if (on) tft.fillSmoothRoundRect(x, TRACK_Y + 3, segW, TRACK_H - 6, (TRACK_H - 6) / 2, COLOR_RAISED, COLOR_SURFACE);
    drawText(TAB_NAMES[t], x + segW / 2, TRACK_Y + TRACK_H / 2, MC_DATUM, on ? COLOR_TEXT : COLOR_DIM,
             on ? COLOR_RAISED : COLOR_SURFACE);
  }
}

void drawPage() {
  tft.fillRect(0, PAGE_Y, 320, 240 - PAGE_Y, COLOR_BG);
  if (tab == DRIVE) {
    for (int d = 0; d < DIR_COUNT; d++) drawArrow(static_cast<Dir>(d));
    drawStop();
    for (int i = 0; i < ACTION_COUNT; i++) drawAction(i);
  } else if (tab == ROS) {
    rosTabDraw();
  } else {
    for (int i = 0; i < SETTING_COUNT; i++) drawSettingRow(i);
    drawAdjust(-1);
    drawAdjust(1);
  }
}

void drawAll() {
  tft.fillScreen(COLOR_BG);
  drawStatus(wasConnected);
  drawTabs();
  drawPage();
}

void setButton(uint8_t button, bool down) {
  if (down) {
    gamepad.press(button);
  } else {
    gamepad.release(button);
  }
}

void sendGamepad() {
  int16_t x = AXIS_CENTER, y = AXIS_CENTER;
  if (active != NONE) {
    const Setting& s = settings[active];
    int16_t offset = static_cast<int32_t>(AXIS_MAX - AXIS_CENTER) * s.value / s.max;
    // HID axis minimum is up/left; ROS joy reports that as +1 (forward / turn left).
    switch (active) {
      case UP: y = AXIS_CENTER - offset; break;
      case DOWN: y = AXIS_CENTER + offset; break;
      case LEFT: x = AXIS_CENTER - offset; break;
      case RIGHT: x = AXIS_CENTER + offset; break;
      default: break;
    }
  }
  gamepad.setX(x);
  gamepad.setY(y);
  setButton(BUTTON_1, active != NONE);
  for (int i = 0; i < ACTION_COUNT; i++) setButton(ACTION_BUTTONS[i], actionHeld == i);
  if (gamepad.isConnected()) gamepad.sendReport();
}

void setActive(Dir d) {
  if (d == active) return;
  Dir prev = active;
  active = d;
  if (prev != NONE) drawArrow(prev);
  if (active != NONE) drawArrow(active);
  sendGamepad();
}

void setActionHeld(int i) {
  if (i == actionHeld) return;
  int prev = actionHeld;
  actionHeld = i;
  if (prev >= 0) drawAction(prev);
  if (actionHeld >= 0) drawAction(actionHeld);
  sendGamepad();
}

void setAdjustHeld(int sign) {
  if (sign == adjustHeld) return;
  int prev = adjustHeld;
  adjustHeld = sign;
  if (prev != 0) drawAdjust(prev);
  if (adjustHeld != 0) drawAdjust(adjustHeld);
}

void setStopHeld(bool held) {
  if (held == stopHeld) return;
  stopHeld = held;
  drawStop();
}

void selectTab(Tab t) {
  if (t == tab) return;
  // Wi-Fi runs only while the ROS tab is open, so driving never shares the radio with it.
  if (tab == ROS) rosTabStop();
  tab = t;
  if (tab == ROS) rosTabStart();
  drawTabs();
  drawPage();
}

void selectSetting(int i) {
  if (i == selected) return;
  int prev = selected;
  selected = i;
  drawSettingRow(prev);
  drawSettingRow(selected);
}

void changeSetting(int steps) {
  Setting& s = settings[selected];
  int v = constrain(s.value + steps * s.step, 0, s.max);
  if (v == s.value) return;
  s.value = v;
  prefs.putInt(s.key, v);
  drawSettingRow(selected);
}

void setBacklight(bool bright) {
  if (bright && dimmed) dimmedSessionMs += millis() - dimStartMs;
  if (!bright && !dimmed) dimStartMs = millis();
  dimmed = !bright;
  analogWrite(TFT_BL, bright ? BACKLIGHT_LEVEL : DIM_LEVEL);
}

// Restarts the dim timer and brightens the screen. Returns true if it was dim.
bool noteActivity() {
  lastActivityMs = millis();
  if (!dimmed) return false;
  setBacklight(true);
  return true;
}

void dimIfIdle() {
  if (!dimmed && millis() - lastActivityMs >= DIM_AFTER_MS) setBacklight(false);
}

void saveRuntime() {
  power.putUInt("session_min", sessionMin());
  power.putUInt("charge_min", sinceChargeMin());
  power.putUInt("dim_min", dimSinceChargeMin());
  power.putUInt("sleep_min", sleepSinceChargeMin());
}

// Runs before the display, BLE or backlight start, so a brownout is recorded while the
// battery can still power a flash write. Restarts shorter than a minute (a dying
// battery restarts over and over) don't replace the last run. Waking from deep sleep
// continues the session: the totals are still in RTC memory.
void loadRuntime() {
  esp_reset_reason_t reason = esp_reset_reason();
  power.begin("yellowpad_pwr", false);
  if (reason == ESP_RST_DEEPSLEEP) {
    wokeFromSleep = true;
    int64_t sleptMs = max<int64_t>(0, (rtcNowUs() - sleepStartUs) / 1000);
    sessionBaseMs += sleptMs;
    sleptSessionMs += sleptMs;
    Serial.printf("Runtime: woke after %s asleep, since charge %s (asleep %s)\n",
                  formatHM(sleptMs / 60000).c_str(), formatHM(sinceChargeMin()).c_str(),
                  formatHM(sleepSinceChargeMin()).c_str());
    return;
  }
  uint32_t prevSession = power.getUInt("session_min", 0);
  if (prevSession > 0) power.putUInt("last_min", prevSession);
  if (reason == ESP_RST_BROWNOUT) power.putBool("brownout", true);
  power.putUInt("session_min", 0);

  lastRunMin = power.getUInt("last_min", 0);
  brownoutSeen = power.getBool("brownout", false);
  chargeOffsetMin = power.getUInt("charge_min", 0);
  dimOffsetMin = power.getUInt("dim_min", 0);
  sleepOffsetMin = power.getUInt("sleep_min", 0);
  Serial.printf("Runtime: reset reason %d, last run %s, since charge %s (dimmed %s, asleep %s)%s\n", reason,
                formatHM(lastRunMin).c_str(), formatHM(chargeOffsetMin).c_str(), formatHM(dimOffsetMin).c_str(),
                formatHM(sleepOffsetMin).c_str(), brownoutSeen ? ", brownout since charge" : "");
}

void markCharged() {
  chargeOffsetMin = -static_cast<int32_t>(sessionMin());
  dimOffsetMin = -static_cast<int32_t>(dimSessionMin());
  sleepOffsetMin = -static_cast<int32_t>(sleepSessionMin());
  brownoutSeen = false;
  power.putBool("brownout", false);
  saveRuntime();
  Serial.println("Runtime: marked charged");
  drawStatus(wasConnected);
}

void saveRuntimeIfDue() {
  if (millis() - lastSaveMs < SAVE_EVERY_MS) return;
  lastSaveMs = millis();
  saveRuntime();
  Serial.printf("Runtime: session %s, since charge %s (dimmed %s, asleep %s)\n", formatHM(sessionMin()).c_str(),
                formatHM(sinceChargeMin()).c_str(), formatHM(dimSinceChargeMin()).c_str(),
                formatHM(sleepSinceChargeMin()).c_str());
}

// Deep sleep after SLEEP_AFTER_MS without a touch. The touch controller's IRQ line
// (GPIO 36, low while touched) wakes the chip, which then boots from the start.
void sleepIfIdle() {
  if (millis() - lastActivityMs < SLEEP_AFTER_MS) return;
  if (touch.tirqTouched()) return;  // a finger is down; it would wake the chip at once
  Serial.println("Sleep: idle, going to deep sleep");
  if (tab == ROS) rosTabStop();

  if (dimmed) dimmedSessionMs += millis() - dimStartMs;
  dimmed = false;
  saveRuntime();
  sessionBaseMs += millis();
  sleepStartUs = rtcNowUs();

  // Backlight off, held low through sleep (GPIO 21 would otherwise float).
  ledcDetachPin(TFT_BL);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(TFT_BL));
  gpio_deep_sleep_hold_en();

  tft.writecommand(0x28);  // ILI9341 display off
  tft.writecommand(0x10);  // ILI9341 sleep in
  delay(10);

  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(TOUCH_IRQ), 0);
  Serial.flush();
  esp_deep_sleep_start();
}

// Holding the status bar for CHARGED_HOLD_MS marks the battery charged, once per hold.
void trackChargedHold(bool touched, int y) {
  if (!touched || y >= STATUS_H) {
    holdingStatus = false;
    holdDone = false;
    return;
  }
  if (!holdingStatus) {
    holdingStatus = true;
    holdStartMs = millis();
  }
  if (!holdDone && millis() - holdStartMs >= CHARGED_HOLD_MS) {
    holdDone = true;
    markCharged();
  }
}

bool readTouch(int& x, int& y) {
  if (!touch.tirqTouched() || !touch.touched()) return false;
  TS_Point p = touch.getPoint();
  x = constrain(map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 319), 0, 319);
  y = constrain(map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 239), 0, 239);
  return true;
}

Dir arrowAt(int x, int y) {
  for (int d = 0; d < DIR_COUNT; d++) {
    if (ARROW_RECTS[d].contains(x, y)) return static_cast<Dir>(d);
  }
  return NONE;
}

int actionAt(int x, int y) {
  for (int i = 0; i < ACTION_COUNT; i++) {
    if (ACTION_RECTS[i].contains(x, y)) return i;
  }
  return -1;
}

void handleTouch() {
  int x = 0, y = 0;
  bool touched = readTouch(x, y);
  bool newPress = touched && !wasTouched;
  wasTouched = touched;

  if (touched && noteActivity()) swallowTouch = true;
  if (!touched) swallowTouch = false;
  trackChargedHold(touched && !swallowTouch, y);

  if (!touched || swallowTouch) {
    setActive(NONE);
    setActionHeld(-1);
    setAdjustHeld(0);
    setStopHeld(false);
    return;
  }

  // On the Settings page, light up - or + while the finger is on it.
  if (tab == SETTINGS) {
    setAdjustHeld(MINUS_RECT.contains(x, y) ? -1 : PLUS_RECT.contains(x, y) ? 1 : 0);
  }

  // On the Drive page, motion and A/B/C follow the finger; sliding off releases.
  if (tab == DRIVE) {
    setActive(arrowAt(x, y));
    setActionHeld(actionAt(x, y));
    setStopHeld(STOP_RECT.contains(x, y));
  }

  if (!newPress) return;
  for (int t = 0; t < TAB_COUNT; t++) {
    if (TAB_RECTS[t].contains(x, y)) selectTab(static_cast<Tab>(t));
  }
  if (tab == ROS && y >= PAGE_Y) rosTabTouch(x, y);
  if (tab != SETTINGS) return;
  for (int i = 0; i < SETTING_COUNT; i++) {
    if (settingRect(i).contains(x, y)) selectSetting(i);
  }
  if (MINUS_RECT.contains(x, y)) changeSetting(-1);
  if (PLUS_RECT.contains(x, y)) changeSetting(1);
}

void setup() {
  // Release the backlight pin held low during deep sleep.
  gpio_hold_dis(static_cast<gpio_num_t>(TFT_BL));
  gpio_deep_sleep_hold_dis();
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.begin(115200);
  loadRuntime();

  // Own namespace: the first design stores percentages under the same keys.
  prefs.begin("yellowpad2", false);
  for (Setting& s : settings) s.value = constrain(prefs.getInt(s.key, s.fallback), 0, s.max);

  tft.init();
  tft.setRotation(1);
  analogWrite(TFT_BL, BACKLIGHT_LEVEL);  // tft.init() turned it fully on; PWM dims it
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  config.setButtonCount(4);
  config.setHatSwitchCount(0);
  config.setWhichAxes(true, true, false, false, false, false, false, false);
  config.setAxesMin(AXIS_MIN);
  config.setAxesMax(AXIS_MAX);
  gamepad.begin(&config);

  drawAll();
  lastActivityMs = millis();
}

void loop() {
  bool connected = gamepad.isConnected();
  if (connected != wasConnected) {
    wasConnected = connected;
    drawStatus(connected);
    noteActivity();                // show the status change
    if (connected) sendGamepad();  // start the host from a centered, disabled state
  }
  handleTouch();
  if (tab == ROS) rosTabLoop();
  dimIfIdle();
  sleepIfIdle();
  saveRuntimeIfDue();
  if (runtimeText() != shownRuntime) drawStatus(connected);
  delay(LOOP_MS);
}
