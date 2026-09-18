#include "buttons.h"

#include <Wire.h>

#include "config.h"

static volatile bool g_intFlag = false;

static uint8_t stableState = 0xFF;  // 1 = released, the PCF8574 idles high
static uint8_t lastRaw = 0xFF;
static uint32_t lastChangeMs = 0;

static void IRAM_ATTR onInt() { g_intFlag = true; }

void buttonsBegin() {
  // Writing all ones puts every quasi-bidirectional pin in input mode with its
  // weak internal pull-up, so buttons wire straight to GND.
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(0xFF);
  Wire.endTransmission();

  pinMode(PIN_PCF_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_PCF_INT), onInt, FALLING);
}

bool buttonsInterruptPending() {
  if (!g_intFlag) return false;
  g_intFlag = false;
  return true;
}

static bool readPort(uint8_t &val) {
  if (Wire.requestFrom(PCF8574_ADDR, (uint8_t)1) != 1) return false;
  if (!Wire.available()) return false;
  val = Wire.read();
  return true;
}

uint8_t buttonsPoll() {
  uint8_t raw;
  if (!readPort(raw)) return 0;

  const uint32_t now = millis();
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChangeMs = now;
    return 0;
  }
  if (now - lastChangeMs < DEBOUNCE_MS) return 0;

  // A press is a bit that was 1 (released) and is now 0 (pulled to GND).
  const uint8_t edges = stableState & ~raw;
  stableState = raw;
  return edges;
}
