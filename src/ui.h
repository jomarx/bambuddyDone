#pragma once

#include "net.h"

void uiBegin();

// Single centred line, for boot and WiFi association.
void uiSplash(const char *line);

// One row per fitted button. A row is drawn inverted when that printer is
// awaiting a plate clear: the lit rows are the ones worth pressing.
// `statusLine` (may be null) is drawn on the first unfitted row.
// `bannerL1`/`bannerL2` (may be null) overlay a result box.
void uiRender(const PrinterStatus *slots, const char *statusLine, const char *bannerL1,
              const char *bannerL2);
