#include "net.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "secrets.h"

static WiFiClient wifiClient;

// Ring of POST timestamps, so we can refuse a press rather than earn a 429.
static uint32_t postTimes[POST_BUDGET_PER_MIN] = {0};
static uint8_t postCursor = 0;
static bool rateLimited = false;

void netBegin() {
  for (uint8_t i = 0; i < POST_BUDGET_PER_MIN; i++) postTimes[i] = 0;
  postCursor = 0;
  rateLimited = false;
}

bool netRateLimited() { return rateLimited; }

bool netPostBudgetAvailable() {
  const uint32_t oldest = postTimes[postCursor];
  if (oldest == 0) return true;
  return (millis() - oldest) >= 60000UL;
}

static void notePost() {
  postTimes[postCursor] = millis();
  postCursor = (postCursor + 1) % POST_BUDGET_PER_MIN;
}

static void copyTrunc(char *dst, size_t dstLen, const char *src) {
  if (!src) src = "";
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
}

static bool beginRequest(HTTPClient &http, const String &path) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!http.begin(wifiClient, BAMBUDDY_HOST, BAMBUDDY_PORT, path)) return false;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("X-API-Key", BAMBUDDY_API_KEY);
  http.addHeader("Accept", "application/json");
  return true;
}

bool netFetchNames(PrinterStatus *slots) {
  HTTPClient http;
  if (!beginRequest(http, "/api/v1/printers")) return false;

  const int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  // Called once at boot and the payload is small, so no stream filter here.
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;

  // Accept either a bare array or {"printers": [...]}.
  JsonArrayConst arr = doc.is<JsonArray>() ? doc.as<JsonArrayConst>()
                                           : doc["printers"].as<JsonArrayConst>();
  if (arr.isNull()) return false;

  for (JsonObjectConst p : arr) {
    const int id = p["id"] | -1;
    if (id < 0) continue;
    const char *name = p["name"] | "";
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (slots[i].id == id) copyTrunc(slots[i].name, sizeof(slots[i].name), name);
    }
  }
  return true;
}

bool netFetchStatus(PrinterStatus &slot) {
  if (slot.id < 0) return false;

  HTTPClient http;
  const String path = String("/api/v1/printers/") + slot.id + "/status";
  if (!beginRequest(http, path)) {
    slot.valid = false;
    return false;
  }

  const int code = http.GET();
  if (code != 200) {
    if (code == 429) rateLimited = true;
    http.end();
    slot.valid = false;
    return false;
  }

  // The status payload is large and the heap is not, so pull out only the two
  // fields the box actually renders and gates on.
  JsonDocument filter;
  filter["state"] = true;
  filter["awaiting_plate_clear"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    slot.valid = false;
    return false;
  }

  copyTrunc(slot.state, sizeof(slot.state), doc["state"] | "?");
  slot.awaitingPlateClear = doc["awaiting_plate_clear"] | false;
  slot.lastOkMs = millis();
  slot.valid = true;
  rateLimited = false;
  return true;
}

int netClearPlate(int printerId, char *msg, size_t msgLen) {
  HTTPClient http;
  const String path = String("/api/v1/printers/") + printerId + "/clear-plate";
  if (!beginRequest(http, path)) {
    copyTrunc(msg, msgLen, WiFi.status() == WL_CONNECTED ? "connect failed" : "wifi down");
    return -1000;
  }

  http.addHeader("Content-Length", "0");
  const int code = http.POST((uint8_t *)nullptr, 0);
  notePost();

  if (code <= 0) {
    copyTrunc(msg, msgLen, http.errorToString(code).c_str());
    http.end();
    return code;
  }

  if (code == 429) {
    rateLimited = true;
    copyTrunc(msg, msgLen, "rate limited");
    http.end();
    return code;
  }

  if (code != 200) {
    snprintf(msg, msgLen, "HTTP %d", code);
    http.end();
    return code;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (!err && doc["success"].is<bool>() && !(doc["success"] | false)) {
    copyTrunc(msg, msgLen, doc["message"] | "refused");
    return -1001;
  }

  copyTrunc(msg, msgLen, "CLEARED");
  rateLimited = false;
  return 200;
}
