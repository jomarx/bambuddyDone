#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

#include "buttons.h"
#include "config.h"
#include "net.h"
#include "secrets.h"
#include "ui.h"

static PrinterStatus slots[BUTTON_COUNT];

static uint8_t pollCursor = 0;
static uint32_t lastPollMs = 0;
static uint32_t lastScanMs = 0;
static uint32_t lastRenderMs = 0;
static uint32_t lastNameTryMs = 0;
static bool namesLoaded = false;

static uint32_t postCooldownUntil[BUTTON_COUNT] = {0};

static char bannerL1[24] = "";
static char bannerL2[24] = "";
static uint32_t bannerUntil = 0;

static void banner(const char *l1, const char *l2) {
  snprintf(bannerL1, sizeof(bannerL1), "%s", l1 ? l1 : "");
  snprintf(bannerL2, sizeof(bannerL2), "%s", l2 ? l2 : "");
  bannerUntil = millis() + BANNER_MS;
}

static const char *slotLabel(uint8_t i) {
  return slots[i].name[0] ? slots[i].name : "printer";
}

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void i2cScan() {
  Serial.println(F("I2C scan:"));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2cPresent(addr)) {
      Serial.printf("  0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println(F("  nothing found - check SDA/SCL and power"));
  if (!i2cPresent(OLED_ADDR)) Serial.printf("  WARNING: no OLED at 0x%02X\n", OLED_ADDR);
  if (!i2cPresent(PCF8574_ADDR))
    Serial.printf("  WARNING: no PCF8574 at 0x%02X\n", PCF8574_ADDR);
}

static const char *statusLine() {
  if (WiFi.status() != WL_CONNECTED) return "wifi down";
  if (!namesLoaded) return "bambuddy offline";
  if (netRateLimited()) return "rate limited";

  // Every fitted printer failing at once means the server, not the printers.
  bool anyValid = false;
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    if (slots[i].id >= 0 && slots[i].valid) anyValid = true;
  }
  if (!anyValid) return "bambuddy offline";
  return nullptr;
}

static void render() {
  const bool showBanner = (int32_t)(bannerUntil - millis()) > 0;
  uiRender(slots, statusLine(), showBanner ? bannerL1 : nullptr,
           showBanner ? bannerL2 : nullptr);
  lastRenderMs = millis();
}

static void handlePress(uint8_t i) {
  // Logged before any gating, including unfitted positions, so a wiring check
  // over serial sees every press the expander reports.
  Serial.printf("button %u (P%u) pressed\n", (unsigned)(i + 1), (unsigned)i);
  if (slots[i].id < 0) return;

  const uint32_t now = millis();
  const char *label = slotLabel(i);

  if (!slots[i].valid || (now - slots[i].lastOkMs) > STALE_AFTER_MS) {
    banner(label, "no status");
    return;
  }
  if (!slots[i].awaitingPlateClear) {
    banner(label, "nothing to clear");
    return;
  }
  if ((int32_t)(postCooldownUntil[i] - now) > 0) {
    banner(label, "wait...");
    return;
  }
  if (!netPostBudgetAvailable()) {
    banner(label, "rate limited");
    return;
  }

  // Draw BUSY before blocking on the request, so a press during an in-flight
  // call is visibly ignored rather than silently dropped.
  banner(label, "BUSY");
  render();

  char msg[24];
  const int code = netClearPlate(slots[i].id, msg, sizeof(msg));
  postCooldownUntil[i] = millis() + POST_COOLDOWN_MS;
  banner(label, msg);
  Serial.printf("clear-plate id=%d -> %d (%s)\n", slots[i].id, code, msg);

  if (code == 200) netFetchStatus(slots[i]);
  render();
}

static void pollNext() {
  // Round robin over fitted positions only.
  for (uint8_t n = 0; n < BUTTON_COUNT; n++) {
    const uint8_t i = (pollCursor + n) % BUTTON_COUNT;
    if (slots[i].id < 0) continue;
    pollCursor = (i + 1) % BUTTON_COUNT;
    netFetchStatus(slots[i]);
    return;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\nbambuddy plate box"));

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) slots[i].id = BUTTON_PRINTER_IDS[i];

  Wire.begin(PIN_SDA, PIN_SCL);
  i2cScan();

  uiBegin();
  Wire.setClock(400000);
  buttonsBegin();
  netBegin();

  uiSplash("connecting wifi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  const uint32_t deadline = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && (int32_t)(deadline - millis()) > 0) delay(200);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("ip "));
    Serial.println(WiFi.localIP());
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
  } else {
    Serial.println(F("wifi failed, continuing offline"));
  }

  namesLoaded = netFetchNames(slots);
  lastNameTryMs = millis();
  render();
}

void loop() {
  ArduinoOTA.handle();

  const uint32_t now = millis();

  if (buttonsInterruptPending() || (now - lastScanMs) >= BUTTON_SCAN_MS) {
    lastScanMs = now;
    const uint8_t edges = buttonsPoll();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (edges & (1 << i)) handlePress(i);
    }
  }

  if (!namesLoaded && (millis() - lastNameTryMs) >= NAME_RETRY_MS) {
    lastNameTryMs = millis();
    namesLoaded = netFetchNames(slots);
  }

  if (namesLoaded && (millis() - lastPollMs) >= POLL_INTERVAL_MS) {
    lastPollMs = millis();
    pollNext();
  }

  if ((millis() - lastRenderMs) >= RENDER_INTERVAL_MS) render();
}
