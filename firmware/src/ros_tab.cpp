// ROS tab. Connects to rosbridge over a WebSocket and shows how often each topic in topics[]
// publishes. For now that's /odom and /scan, as placeholders. Nothing else is subscribed, so
// rosbridge isn't asked to stream more than the ESP32 can read. The connection state is shown
// in the status bar (rosTabLink), not on the page.
//
// rosapi's topic list is fetched every TOPIC_REFRESH_MS. A topic is subscribed once it appears
// there, so rosbridge can infer its type; a topic missing from it shows "not published".
//
// Page layout (y): one row per topic from ROW_Y. Rows are redrawn only when what they show
// changes, to avoid flicker.

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

constexpr uint32_t TOPIC_REFRESH_MS = 10000;
constexpr uint32_t RATE_WINDOW_MS = 2000;
constexpr uint32_t ACTIVE_MS = 3000;   // a topic counts as active if it published this recently
constexpr uint32_t THROTTLE_MS = 50;   // rosbridge sends at most 20 messages/s per topic
constexpr uint32_t REDRAW_MS = 500;
constexpr uint32_t RECONNECT_MS = 3000;

// Layout.
constexpr int ROW_Y = PAGE_Y + 8, ROW_H = 26;
constexpr int LEFT = 12, RIGHT = SCREEN_W - 12;

struct Topic {
  const char* name;
  bool present;     // in the latest rosapi topic list
  bool subscribed;  // subscribe sent on this connection
  uint32_t count;   // messages in the current rate window
  float rate;
  uint32_t lastSeen;  // millis() of the last message, 0 if never
};

// The topics shown, with their publish rates. Placeholders until the real set is chosen.
Topic topics[] = {{"/odom"}, {"/scan"}};
constexpr int TOPIC_COUNT = sizeof(topics) / sizeof(topics[0]);

enum class Link { OFF, JOINING, ONLINE };

WebSocketsClient ws;
bool wsBegun = false;
bool connected = false;
Link wifiLink = Link::OFF;  // not "link": that's a POSIX function

uint32_t nextTopicRefresh = 0, windowStart = 0, lastRedraw = 0;

// What each row last showed, so unchanged rows aren't redrawn.
String shown[TOPIC_COUNT];

bool changed(int row, const String& key) {
  if (shown[row] == key) return false;
  shown[row] = key;
  return true;
}

bool hasWifi() { return WIFI_SSID[0] != '\0'; }
bool hasHost() { return ROSBRIDGE_HOST[0] != '\0'; }

// ---- rosbridge ----

Topic* findTopic(const char* name) {
  for (Topic& t : topics)
    if (strcmp(t.name, name) == 0) return &t;
  return nullptr;
}

// Forgets presence and subscriptions: rosbridge drops our subscriptions on disconnect.
void resetTopics() {
  for (Topic& t : topics) {
    t.present = t.subscribed = false;
    t.count = 0;
    t.rate = 0;
  }
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

// Marks which topics exist, and subscribes to any that just appeared.
void handleTopicList(JsonArray names) {
  for (Topic& t : topics) t.present = false;
  for (const char* name : names) {
    Topic* t = findTopic(name);
    if (!t) continue;
    t->present = true;
    if (!t->subscribed) {
      subscribe(t->name);
      t->subscribed = true;
    }
  }
}

void handleMessage(uint8_t* payload, size_t length) {
  // Only the envelope is needed: message contents aren't shown, so /scan's ranges are skipped.
  JsonDocument filter;
  filter["op"] = true;
  filter["topic"] = true;
  filter["id"] = true;
  filter["values"]["topics"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter))) return;

  const char* op = doc["op"] | "";
  if (strcmp(op, "service_response") == 0 && doc["id"] == "topics") {
    handleTopicList(doc["values"]["topics"]);
    return;
  }
  if (strcmp(op, "publish") != 0) return;
  if (Topic* t = findTopic(doc["topic"] | "")) {
    t->count++;
    t->lastSeen = millis();
  }
}

void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      connected = true;
      resetTopics();
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

void clearRow(int y, int h) { tft.fillRect(0, y, SCREEN_W, h, COLOR_BG); }

String rateText(const Topic& t) {
  if (!connected) return "-";
  if (!t.present) return "not published";
  if (!isActive(t)) return "-";
  if (t.rate == 0) return "...";  // active, but the first rate window isn't done
  if (t.rate >= 1000.0f / THROTTLE_MS - 0.5f) return String(1000 / THROTTLE_MS) + "+ Hz";
  return String(t.rate, 1) + " Hz";
}

// Dot (green active, amber seen earlier, grey never), topic name, rate on the right.
void drawTopicRow(int i) {
  const Topic& t = topics[i];
  int y = ROW_Y + i * ROW_H, cy = y + ROW_H / 2;
  String rate = rateText(t);
  uint16_t dot = isActive(t) ? COLOR_ACCENT : t.lastSeen ? COLOR_WARN : COLOR_RAISED;
  if (!changed(i, rate + "|" + dot)) return;
  clearRow(y, ROW_H);
  tft.fillSmoothCircle(LEFT + 3, cy, 3, dot, COLOR_BG);
  drawText(t.name, LEFT + 14, cy, ML_DATUM, COLOR_TEXT, COLOR_BG);
  drawText(rate, RIGHT, cy, MR_DATUM, isActive(t) ? COLOR_ACCENT : COLOR_DIM, COLOR_BG, FONT_NUMBER);
}

void drawUpdates() {
  for (int i = 0; i < TOPIC_COUNT; i++) drawTopicRow(i);
}

}  // namespace

RosLink rosTabLink() {
  if (wifiLink == Link::OFF) return RosLink::OFF;
  if (!hasWifi() || !hasHost()) return RosLink::UNCONFIGURED;
  if (wifiLink == Link::JOINING) return RosLink::JOINING;
  return connected ? RosLink::ONLINE : RosLink::CONNECTING;
}

void rosTabStart() {
  wifiLink = Link::JOINING;
  connected = false;
  resetTopics();
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
    for (Topic& t : topics) {
      t.rate = t.count * 1000.0f / elapsed;
      t.count = 0;
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
  drawUpdates();
}
