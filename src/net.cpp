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

enum class NamesResult { HttpFailed, NoPrinters, Applied };

// One attempt at the list route. `wrapped` picks the filter shape: an
// ArduinoJson filter has to match the payload's structure, so an array-shaped
// filter matches nothing against {"printers": [...]} and the two shapes cannot
// share one filter. Boot-only, so retrying beats guessing.
static NamesResult fetchNamesOnce(PrinterStatus *slots, const char *path, bool wrapped) {
  HTTPClient http;
  if (!beginRequest(http, path)) return NamesResult::HttpFailed;

  const int code = http.GET();
  if (code != 200) {
    http.end();
    return NamesResult::HttpFailed;
  }

  // Filtered, and not for heap reasons alone: the list carries each printer's
  // LAN access_code, plus serial_number and ip_address. Keeping only id and
  // name means the access codes never land in device RAM at all.
  JsonDocument filter;
  JsonObject entry = wrapped ? filter["printers"].to<JsonArray>().add<JsonObject>()
                             : filter.to<JsonArray>().add<JsonObject>();
  entry["id"] = true;
  entry["name"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return NamesResult::HttpFailed;

  JsonArrayConst arr =
      wrapped ? doc["printers"].as<JsonArrayConst>() : doc.as<JsonArrayConst>();
  if (arr.isNull() || arr.size() == 0) return NamesResult::NoPrinters;

  for (JsonObjectConst p : arr) {
    const int id = p["id"] | -1;
    if (id < 0) continue;
    const char *name = p["name"] | "";
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (slots[i].id == id) copyTrunc(slots[i].name, sizeof(slots[i].name), name);
    }
  }
  return NamesResult::Applied;
}

bool netFetchNames(PrinterStatus *slots) {
  // The trailing slash is not cosmetic: on a real instance GET /api/v1/printers
  // 404s while GET /api/v1/printers/ returns 200. Try the slash first and keep
  // the bare form as a fallback -- a 404 here leaves namesLoaded false, and
  // status polling is gated on that, so the whole box goes dead.
  static const char *const PATHS[] = {"/api/v1/printers/", "/api/v1/printers"};

  for (uint8_t p = 0; p < 2; p++) {
    for (uint8_t w = 0; w < 2; w++) {
      const NamesResult r = fetchNamesOnce(slots, PATHS[p], w == 1);
      if (r == NamesResult::Applied) return true;
      // Route answered, both shapes parsed, no printers in it: Bambuddy is
      // reachable and there is nothing left to hydrate.
      if (r == NamesResult::NoPrinters && w == 1) return true;
    }
  }
  return false;
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
