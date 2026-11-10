// ROS tab. Connects to rosbridge over a WebSocket, lists topics via rosapi, subscribes to the
// lightweight ones and shows how often each publishes. /cmd_vel and the battery are pinned at
// the top with their values; the last two /rosout lines are at the bottom.
//
// Page layout (y): header 60-78, pinned rows 83-124, topic list 130-202, log 207-240.
// Rows are redrawn only when what they show changes, to avoid flicker.

#include "ros_tab.h"

#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

#include "secrets.h"
#include "ui.h"

#ifndef ROSBRIDGE_PORT
#define ROSBRIDGE_PORT 9090
#endif

namespace {

const char* const CMD_VEL_TOPIC = "/cmd_vel";
// sensor_msgs/BatteryState (voltage, percentage), or a std_msgs number (data, shown as volts).
const char* const BATTERY_TOPIC = "/battery_state";

constexpr uint32_t TOPIC_REFRESH_MS = 10000;
constexpr uint32_t RATE_WINDOW_MS = 2000;
constexpr uint32_t ACTIVE_MS = 3000;   // a topic counts as active if it published this recently
constexpr uint32_t STALE_MS = 5000;    // pinned values older than this are dimmed
constexpr uint32_t THROTTLE_MS = 50;   // rosbridge sends at most 20 messages/s per topic
constexpr uint32_t REDRAW_MS = 500;
constexpr uint32_t RECONNECT_MS = 3000;
constexpr int MAX_TOPICS = 40;

// Message types too big to stream to the ESP32 as JSON; they're listed but not watched.
const char* const HEAVY_TYPES[] = {
    "sensor_msgs/msg/Image",      "sensor_msgs/msg/CompressedImage",   "sensor_msgs/msg/PointCloud2",
    "sensor_msgs/msg/LaserScan",  "nav_msgs/msg/OccupancyGrid",        "map_msgs/msg/OccupancyGridUpdate",
    "visualization_msgs/msg/MarkerArray", "tf2_msgs/msg/TFMessage",
};
const char* const SKIPPED_TOPICS[] = {"/robot_description"};

// Layout.
constexpr int HEADER_Y = 60, HEADER_H = 19;
constexpr int PIN_Y = 83, PIN_H = 21;
constexpr int LIST_Y = 130, LIST_ROW_H = 18, LIST_ROWS = 4;
constexpr int LOG_Y = 207, LOG_ROW_H = 16, LOG_ROWS = 2;
constexpr int LEFT = 12, RIGHT = SCREEN_W - 12;

struct Topic {
  char name[48];
  bool watched;  // subscribed through rosbridge
  bool present;  // still in the latest topic list
  uint32_t count;
  float rate;
  uint32_t lastSeen;  // millis() of the last message, 0 if never
};

struct LogLine {
  char text[96];
  int level;  // rcl_interfaces/Log: 10 DEBUG, 20 INFO, 30 WARN, 40 ERROR, 50 FATAL
};

enum class Link { OFF, JOINING, ONLINE };

WebSocketsClient ws;
bool wsBegun = false;
bool connected = false;
Link wifiLink = Link::OFF;  // not "link": that's a POSIX function

Topic topics[MAX_TOPICS];
int topicCount = 0;
uint32_t nextTopicRefresh = 0, windowStart = 0, lastRedraw = 0;
int listPage = 0, listPages = 1;

struct {
  float linear, angular;
  uint32_t lastSeen;
} cmdVel = {};

struct {
  float voltage, percent;  // percent is 0-100
  bool hasVoltage, hasPercent;
  uint32_t lastSeen;
} battery = {};

LogLine logLines[LOG_ROWS] = {};  // [0] is the newest
int logCount = 0;

// What each row last showed, so unchanged rows aren't redrawn.
enum Slot { SLOT_HEADER, SLOT_CMD_VEL, SLOT_BATTERY, SLOT_LIST, SLOT_LOG = SLOT_LIST + LIST_ROWS,
            SLOT_COUNT = SLOT_LOG + LOG_ROWS };
String shown[SLOT_COUNT];

bool changed(int slot, const String& key) {
  if (shown[slot] == key) return false;
  shown[slot] = key;
  return true;
}

bool hasWifi() { return WIFI_SSID[0] != '\0'; }
bool hasHost() { return ROSBRIDGE_HOST[0] != '\0'; }

// ---- rosbridge ----

bool isHeavy(const char* name, const char* type) {
  for (const char* t : HEAVY_TYPES)
    if (strcmp(type, t) == 0) return true;
  for (const char* t : SKIPPED_TOPICS)
    if (strcmp(name, t) == 0) return true;
  return false;
}

Topic* findTopic(const char* name) {
  for (int i = 0; i < topicCount; i++)
    if (strcmp(topics[i].name, name) == 0) return &topics[i];
  return nullptr;
}

void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

void requestTopics() {
  JsonDocument doc;
  doc["op"] = "call_service";
  doc["service"] = "/rosapi/topics";
  doc["id"] = "topics";
  sendJson(doc);
}

void subscribe(const char* name) {
  JsonDocument doc;
  doc["op"] = "subscribe";
  doc["topic"] = name;
  doc["throttle_rate"] = THROTTLE_MS;
  doc["queue_length"] = 1;
  sendJson(doc);
}

// Merges a fresh topic list into the table, subscribing to any new lightweight topics.
void handleTopicList(JsonObject values) {
  for (int i = 0; i < topicCount; i++) topics[i].present = false;
  JsonArray names = values["topics"], types = values["types"];
  for (size_t i = 0; i < names.size(); i++) {
    const char* name = names[i];
    const char* type = types[i] | "";
    Topic* t = findTopic(name);
    if (!t) {
      if (topicCount == MAX_TOPICS) continue;
      t = &topics[topicCount++];
      memset(t, 0, sizeof *t);
      strlcpy(t->name, name, sizeof t->name);
      t->watched = !isHeavy(name, type);
      if (t->watched) subscribe(name);
    }
    t->present = true;
  }
}

void handleCmdVel(JsonObject msg) {
  JsonObject twist = msg["twist"].is<JsonObject>() ? msg["twist"].as<JsonObject>() : msg;  // TwistStamped
  cmdVel.linear = twist["linear"]["x"] | 0.0f;
  cmdVel.angular = twist["angular"]["z"] | 0.0f;
  cmdVel.lastSeen = millis();
}

void handleBattery(JsonObject msg) {
  float v = msg["voltage"].is<float>() ? msg["voltage"].as<float>() : msg["data"] | NAN;
  float p = msg["percentage"] | NAN;  // BatteryState uses 0-1; some drivers send 0-100
  battery.hasVoltage = !isnan(v);
  battery.voltage = v;
  battery.hasPercent = !isnan(p);
  battery.percent = p <= 1.0f ? p * 100 : p;
  battery.lastSeen = millis();
}

void handleLog(JsonObject msg) {
  const char* node = msg["name"] | "?";
  if (strcmp(node, "rosbridge_websocket") == 0) return;  // it logs every client, including us
  for (int i = LOG_ROWS - 1; i > 0; i--) logLines[i] = logLines[i - 1];
  snprintf(logLines[0].text, sizeof logLines[0].text, "%s: %s", node, msg["msg"] | "");
  logLines[0].level = msg["level"] | 20;
  if (logCount < LOG_ROWS) logCount++;
}

void handleMessage(uint8_t* payload, size_t length) {
  // Only keep the fields we use, so large messages don't need a large document.
  JsonDocument filter;
  filter["op"] = true;
  filter["topic"] = true;
  filter["id"] = true;
  filter["values"] = true;
  JsonObject m = filter["msg"].to<JsonObject>();
  for (const char* f : {"level", "name", "msg", "voltage", "percentage", "data"}) m[f] = true;
  m["linear"]["x"] = true;
  m["angular"]["z"] = true;
  m["twist"]["linear"]["x"] = true;
  m["twist"]["angular"]["z"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter))) return;

  const char* op = doc["op"] | "";
  if (strcmp(op, "service_response") == 0 && doc["id"] == "topics") {
    handleTopicList(doc["values"]);
    return;
  }
  if (strcmp(op, "publish") != 0) return;
  const char* name = doc["topic"] | "";
  if (Topic* t = findTopic(name)) {
    t->count++;
    t->lastSeen = millis();
  }
  JsonObject msg = doc["msg"];
  if (strcmp(name, CMD_VEL_TOPIC) == 0) handleCmdVel(msg);
  else if (strcmp(name, BATTERY_TOPIC) == 0) handleBattery(msg);
  else if (strcmp(name, "/rosout") == 0) handleLog(msg);
}

void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      connected = true;
      topicCount = 0;  // rosbridge forgets our subscriptions on disconnect, so start afresh
      nextTopicRefresh = 0;
      break;
    case WStype_DISCONNECTED:
      connected = false;
      break;
    case WStype_TEXT:
      handleMessage(payload, length);
      break;
    default:
      break;
  }
}

// ---- drawing ----

bool isActive(const Topic& t) { return t.lastSeen && millis() - t.lastSeen < ACTIVE_MS; }

// Display order: active topics by rate, then quiet watched ones, then unwatched, each alphabetical.
int rank(const Topic& t) { return isActive(t) ? 0 : t.watched ? 1 : 2; }

int compareTopics(const void* a, const void* b) {
  const Topic &x = **(const Topic* const*)a, &y = **(const Topic* const*)b;
  if (rank(x) != rank(y)) return rank(x) - rank(y);
  if (rank(x) == 0 && x.rate != y.rate) return x.rate > y.rate ? -1 : 1;
  return strcmp(x.name, y.name);
}

// Shortens s with "..." until it fits in maxW pixels.
String fit(String s, int maxW, const uint8_t* font = FONT_TEXT) {
  if (textWidth(s, font) <= maxW) return s;
  while (s.length() > 1 && textWidth(s + "...", font) > maxW) s.remove(s.length() - 1);
  return s + "...";
}

void clearRow(int y, int h) { tft.fillRect(0, y, SCREEN_W, h, COLOR_BG); }

void drawHeader(int presentCount, int activeCount) {
  String text, right;
  uint16_t dot = COLOR_DIM, color = COLOR_DIM;
  if (!hasWifi()) {
    text = "Set WIFI_SSID in secrets.h";
    dot = color = COLOR_STOP;
  } else if (wifiLink != Link::ONLINE) {
    text = String("Joining ") + WIFI_SSID + "...";
  } else if (!hasHost()) {
    text = "Set ROSBRIDGE_HOST in secrets.h";
    dot = color = COLOR_WARN;
  } else if (!connected) {
    text = String("Connecting to ") + ROSBRIDGE_HOST + "...";
    dot = COLOR_WARN;
  } else {
    text = String(presentCount) + " topics, " + activeCount + " active";
    right = listPages > 1 ? String(listPage + 1) + " / " + listPages : String(ROSBRIDGE_HOST);
    dot = COLOR_ACCENT;
    color = COLOR_TEXT;
  }
  if (!changed(SLOT_HEADER, text + "|" + right + "|" + dot + "|" + color)) return;
  int cy = HEADER_Y + HEADER_H / 2;
  clearRow(HEADER_Y, HEADER_H);
  tft.fillSmoothCircle(LEFT + 3, cy, 4, dot, COLOR_BG);
  drawText(text, LEFT + 14, cy, ML_DATUM, color, COLOR_BG);
  if (right.length()) drawText(right, RIGHT, cy, MR_DATUM, COLOR_DIM, COLOR_BG);
}

// Draws value-unit pairs right to left, ending at RIGHT; numbers monospaced, units dim.
void drawValues(const String* numbers, const char* const* units, int n, int cy, uint16_t color) {
  int x = RIGHT;
  for (int i = n - 1; i >= 0; i--) {
    drawText(units[i], x, cy, MR_DATUM, COLOR_DIM, COLOR_BG);
    x -= textWidth(units[i]) + 4;
    drawText(numbers[i], x, cy, MR_DATUM, color, COLOR_BG, FONT_NUMBER);
    x -= textWidth(numbers[i], FONT_NUMBER) + 14;
  }
}

void drawPinned(int slot, int y, const char* label, uint32_t lastSeen, const String* numbers,
                const char* const* units, int n) {
  bool fresh = lastSeen && millis() - lastSeen < STALE_MS;
  String key = String(lastSeen ? 1 : 0) + fresh;
  for (int i = 0; i < n; i++) key += "|" + numbers[i];
  if (!changed(slot, key)) return;
  int cy = y + PIN_H / 2;
  clearRow(y, PIN_H);
  drawText(label, LEFT, cy, ML_DATUM, COLOR_DIM, COLOR_BG);
  if (!lastSeen || n == 0) {
    drawText("-", RIGHT, cy, MR_DATUM, COLOR_DIM, COLOR_BG);
    return;
  }
  drawValues(numbers, units, n, cy, fresh ? COLOR_TEXT : COLOR_DIM);
}

void drawCmdVel() {
  const String numbers[] = {String(cmdVel.linear, 2), String(cmdVel.angular, 2)};
  const char* const units[] = {"m/s", "rad/s"};
  drawPinned(SLOT_CMD_VEL, PIN_Y, "cmd_vel", cmdVel.lastSeen, numbers, units, 2);
}

void drawBattery() {
  String numbers[2];
  const char* units[2];
  int n = 0;
  if (battery.hasVoltage) {
    numbers[n] = String(battery.voltage, 1);
    units[n++] = "V";
  }
  if (battery.hasPercent) {
    numbers[n] = String(static_cast<int>(battery.percent + 0.5f));
    units[n++] = "%";
  }
  drawPinned(SLOT_BATTERY, PIN_Y + PIN_H, "battery", battery.lastSeen, numbers, units, n);
}

void drawTopicRow(int i, const Topic* t) {
  int y = LIST_Y + i * LIST_ROW_H, cy = y + LIST_ROW_H / 2;
  String name, rate;
  uint16_t dot = COLOR_BG, rateColor = COLOR_DIM;
  if (t) {
    name = t->name;
    dot = isActive(*t) ? COLOR_ACCENT : t->lastSeen ? COLOR_WARN : COLOR_RAISED;
    if (!t->watched) {
      rate = "skip";
    } else if (!isActive(*t)) {
      rate = "-";
    } else if (t->rate == 0) {
      rate = "...";  // active, but the first rate window isn't done
    } else if (t->rate >= 1000.0f / THROTTLE_MS - 0.5f) {
      rate = String(1000 / THROTTLE_MS) + "+ Hz";
    } else {
      rate = String(t->rate, 1) + " Hz";
    }
    if (isActive(*t)) rateColor = COLOR_ACCENT;
  }
  if (!changed(SLOT_LIST + i, name + "|" + rate + "|" + dot)) return;
  clearRow(y, LIST_ROW_H);
  if (!t) return;
  tft.fillSmoothCircle(LEFT + 3, cy, 3, dot, COLOR_BG);
  drawText(rate, RIGHT, cy, MR_DATUM, rateColor, COLOR_BG);
  int nameW = RIGHT - textWidth(rate) - 12 - (LEFT + 14);
  drawText(fit(name, nameW), LEFT + 14, cy, ML_DATUM, t->watched ? COLOR_TEXT : COLOR_DIM, COLOR_BG);
}

void drawTopicList() {
  const Topic* present[MAX_TOPICS];
  int n = 0;
  for (int i = 0; i < topicCount; i++)
    if (topics[i].present) present[n++] = &topics[i];
  qsort(present, n, sizeof present[0], compareTopics);
  listPages = n > 0 ? (n + LIST_ROWS - 1) / LIST_ROWS : 1;
  if (listPage >= listPages) listPage = 0;
  for (int i = 0; i < LIST_ROWS; i++) {
    int k = listPage * LIST_ROWS + i;
    drawTopicRow(i, connected && k < n ? present[k] : nullptr);
  }
}

void drawLog() {
  for (int i = 0; i < LOG_ROWS; i++) {
    int y = LOG_Y + i * LOG_ROW_H, cy = y + LOG_ROW_H / 2;
    const LogLine* line = i < logCount ? &logLines[i] : nullptr;
    String text = line ? String(line->text) : (i == 0 ? "No log messages yet" : "");
    int level = line ? line->level : 0;
    if (!changed(SLOT_LOG + i, text + "|" + level)) continue;
    clearRow(y, LOG_ROW_H);
    if (!text.length()) continue;
    if (!line) {
      drawText(text, LEFT, cy, ML_DATUM, COLOR_DIM, COLOR_BG);
      continue;
    }
    uint16_t color = level >= 40 ? COLOR_STOP : level >= 30 ? COLOR_WARN : COLOR_DIM;
    const char* tag = level >= 50 ? "F" : level >= 40 ? "E" : level >= 30 ? "W" : level >= 20 ? "I" : "D";
    drawText(tag, LEFT, cy, ML_DATUM, color, COLOR_BG);
    drawText(fit(text, RIGHT - (LEFT + 16)), LEFT + 16, cy, ML_DATUM, level >= 30 ? color : COLOR_TEXT,
             COLOR_BG);
  }
}

void drawUpdates() {
  int present = 0, active = 0;
  for (int i = 0; i < topicCount; i++) {
    if (!topics[i].present) continue;
    present++;
    if (isActive(topics[i])) active++;
  }
  drawTopicList();  // first: it sets listPages, which the header shows
  drawHeader(present, active);
  drawCmdVel();
  drawBattery();
  drawLog();
}

void drawDivider(int y) { tft.drawFastHLine(LEFT, y, RIGHT - LEFT, COLOR_SURFACE); }

}  // namespace

void rosTabStart() {
  wifiLink = Link::JOINING;
  connected = false;
  topicCount = 0;
  listPage = 0;
  if (hasWifi()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void rosTabStop() {
  if (wsBegun) ws.disconnect();
  connected = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiLink = Link::OFF;
}

void rosTabLoop() {
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (wifiLink == Link::JOINING && wifiUp) wifiLink = Link::ONLINE;
  if (wifiLink == Link::ONLINE && !wifiUp) {
    wifiLink = Link::JOINING;
    connected = false;
  }

  if (wifiLink == Link::ONLINE && hasHost()) {
    if (!wsBegun) {
      ws.begin(ROSBRIDGE_HOST, ROSBRIDGE_PORT, "/");
      ws.onEvent(onEvent);
      ws.setReconnectInterval(RECONNECT_MS);
      wsBegun = true;
    }
    ws.loop();
    if (connected && millis() >= nextTopicRefresh) {
      nextTopicRefresh = millis() + TOPIC_REFRESH_MS;
      requestTopics();
    }
  }

  uint32_t elapsed = millis() - windowStart;
  if (elapsed >= RATE_WINDOW_MS) {
    for (int i = 0; i < topicCount; i++) {
      topics[i].rate = topics[i].count * 1000.0f / elapsed;
      topics[i].count = 0;
    }
    windowStart = millis();
  }

  if (millis() - lastRedraw >= REDRAW_MS) {
    lastRedraw = millis();
    drawUpdates();
  }
}

void rosTabDraw() {
  tft.fillRect(0, PAGE_Y, SCREEN_W, SCREEN_H - PAGE_Y, COLOR_BG);
  for (String& s : shown) s = "\x01";  // matches nothing, so every row redraws
  drawDivider(PIN_Y - 3);
  drawDivider(LIST_Y - 3);
  drawDivider(LOG_Y - 2);
  drawUpdates();
}

void rosTabTouch(int, int y) {
  if (y < LIST_Y || listPages <= 1) return;
  listPage = (listPage + 1) % listPages;
  drawUpdates();
}
