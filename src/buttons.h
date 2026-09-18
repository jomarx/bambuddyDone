#pragma once

#include <Arduino.h>

void buttonsBegin();

// Returns a bitmask of buttons that transitioned released -> pressed since the
// last call. Bit 0 is PCF8574 P0. Returns 0 when nothing changed, when the read
// failed, or while the debounce window is still open.
uint8_t buttonsPoll();

// True when the expander pulled /INT low since we last looked, i.e. something
// moved and we should read the port even if the scan interval has not elapsed.
bool buttonsInterruptPending();
