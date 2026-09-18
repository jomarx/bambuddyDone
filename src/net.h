#pragma once

#include <Arduino.h>
#include "config.h"

struct PrinterStatus {
  int id = -1;                     // -1 = button position not fitted
  char name[16] = "";              // hydrated from GET /printers
  char state[12] = "";             // IDLE / PREPARE / RUNNING / FINISH / FAILED ...
  bool awaitingPlateClear = false;
  bool valid = false;              // last status fetch succeeded
  uint32_t lastOkMs = 0;
};

void netBegin();

// GET /api/v1/printers, fills slots[].name for every fitted position.
bool netFetchNames(PrinterStatus *slots);

// GET /api/v1/printers/{id}/status. On failure marks the slot invalid.
bool netFetchStatus(PrinterStatus &slot);

// POST /api/v1/printers/{id}/clear-plate.
// Returns the HTTP status, or a negative HTTPClient error. `msg` gets a short
// human-readable result either way.
int netClearPlate(int printerId, char *msg, size_t msgLen);

// True when a clear-plate POST is allowed by our own token bucket.
bool netPostBudgetAvailable();

// Set when the server answered 429 or reported an exhausted rate limit.
bool netRateLimited();
