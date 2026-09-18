#include "ui.h"

#include <U8g2lib.h>

#include "config.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static const uint8_t ROW_H = 8;
static const uint8_t SCREEN_W = 128;

void uiBegin() {
  u8g2.setI2CAddress(OLED_ADDR * 2);
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();
}

void uiSplash(const char *line) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  const int w = u8g2.getStrWidth(line);
  u8g2.drawStr((SCREEN_W - w) / 2, 28, line);
  u8g2.sendBuffer();
}

static void drawRow(uint8_t row, const PrinterStatus &s) {
  const uint8_t y = row * ROW_H;

  if (s.awaitingPlateClear) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, y, SCREEN_W, ROW_H);
    u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }

  char left[20];
  snprintf(left, sizeof(left), "%u %.12s", (unsigned)(row + 1),
           s.name[0] ? s.name : "?");
  u8g2.drawStr(0, y, left);

  const char *right = s.valid ? s.state : "OFFLINE";
  u8g2.drawStr(SCREEN_W - u8g2.getStrWidth(right), y, right);

  u8g2.setDrawColor(1);
}

static void drawBanner(const char *l1, const char *l2) {
  const uint8_t top = 20;
  const uint8_t h = 24;

  u8g2.setDrawColor(0);
  u8g2.drawBox(2, top, SCREEN_W - 4, h);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(2, top, SCREEN_W - 4, h);

  if (l1) u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(l1)) / 2, top + 4, l1);
  if (l2) u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(l2)) / 2, top + 13, l2);
}

void uiRender(const PrinterStatus *slots, const char *statusLine, const char *bannerL1,
              const char *bannerL2) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  int firstFreeRow = -1;
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    if (slots[i].id < 0) {
      if (firstFreeRow < 0) firstFreeRow = i;
      continue;
    }
    drawRow(i, slots[i]);
  }

  if (statusLine) {
    // Fall back to the last row when every button position is fitted.
    const uint8_t row = firstFreeRow >= 0 ? (uint8_t)firstFreeRow : (uint8_t)(BUTTON_COUNT - 1);
    const uint8_t y = row * ROW_H;
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, y, SCREEN_W, ROW_H);
    u8g2.setDrawColor(0);
    u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(statusLine)) / 2, y, statusLine);
    u8g2.setDrawColor(1);
  }

  if (bannerL1 || bannerL2) drawBanner(bannerL1, bannerL2);

  u8g2.sendBuffer();
}
