#pragma once

#include <Arduino.h>

// Button index -> Bambuddy printer id, in PCF8574 pin order: index 0 is P0,
// index 7 is P7. Use -1 for a position with no button fitted.
// Get the ids from: GET /api/v1/printers
static const int BUTTON_PRINTER_IDS[8] = {1, 2, 3, 4, 5, -1, -1, -1};

static const uint8_t BUTTON_COUNT = 8;

// I2C
static const uint8_t PIN_SDA = 4;   // D2
static const uint8_t PIN_SCL = 5;   // D1
static const uint8_t PCF8574_ADDR = 0x20;
static const uint8_t OLED_ADDR = 0x3C;

// PCF8574 /INT, open drain and active low.
static const uint8_t PIN_PCF_INT = 14;  // D5

static const uint32_t DEBOUNCE_MS = 25;
static const uint32_t BUTTON_SCAN_MS = 5;

// One status GET at a time, this far apart. Five printers means each row
// refreshes every 7.5s and we spend ~40 req/min against a 100/min read budget.
static const uint32_t POLL_INTERVAL_MS = 1500;

// A status older than this is not trustworthy enough to gate a press on.
static const uint32_t STALE_AFTER_MS = 30000;

static const uint32_t POST_COOLDOWN_MS = 5000;  // per printer
static const uint8_t POST_BUDGET_PER_MIN = 8;   // documented control limit is 10/min

static const uint32_t BANNER_MS = 1500;
static const uint16_t HTTP_TIMEOUT_MS = 3000;
static const uint32_t RENDER_INTERVAL_MS = 100;
static const uint32_t NAME_RETRY_MS = 15000;
