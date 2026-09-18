#pragma once

// Copy to src/secrets.h and fill in. secrets.h is gitignored.

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

// Bambuddy on the LAN, plain HTTP.
#define BAMBUDDY_HOST "192.168.1.50"
#define BAMBUDDY_PORT 8000

// Settings -> API Keys. Scope it to printers:read + printers:clear_plate only:
// this key sits in ESP8266 flash and is recoverable by anyone holding the board.
#define BAMBUDDY_API_KEY "bbd_xxxxxxxxxxxxxxxxxxxxxxxx"

#define OTA_HOSTNAME "plate-box"
#define OTA_PASSWORD "change-me"
