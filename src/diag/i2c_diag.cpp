// Standalone I2C wiring diagnostic, built only by the i2c_diag environment:
//   pio run -e i2c_diag -t upload && pio device monitor -b 115200
// Repeats every 5s so you can watch it change while you fix wiring.

#include <Arduino.h>
#include <Wire.h>

static const uint8_t PIN_D2 = 4;  // wired as SDA
static const uint8_t PIN_D1 = 5;  // wired as SCL

// Discharge the line, release it with no internal pull, and see if it comes
// back. An external pull-up (4.7k on the modules) recharges it in well under
// a microsecond; a floating wire holds the low charge. Without the discharge a
// floating wire can read high from charge left by the previous pull-up read.
static int countRecovers(uint8_t pin) {
  int high = 0;
  for (int i = 0; i < 20; i++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(100);
    pinMode(pin, INPUT);
    delayMicroseconds(20);
    high += digitalRead(pin);
  }
  return high;
}

static int countHighPulledUp(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  delay(5);
  int high = 0;
  for (int i = 0; i < 20; i++) {
    high += digitalRead(pin);
    delayMicroseconds(200);
  }
  return high;
}

// No-pull reading tells us what the *modules* are doing to the line; the
// pull-up reading separates a floating wire from one dragged to ground.
static void reportLine(const char *name, uint8_t pin) {
  const int noPull = countRecovers(pin);
  const int pullUp = countHighPulledUp(pin);

  const char *verdict;
  if (noPull == 20 && pullUp == 20) {
    verdict = "ok: module pull-up present, line connected and powered";
  } else if (pullUp == 20) {
    verdict = "FLOATING: nothing pulling it up, wire not reaching a module";
  } else if (pullUp == 0) {
    verdict = "HELD LOW: module unpowered (pull-up to dead VCC) or shorted to GND";
  } else {
    verdict = "UNSTABLE: intermittent contact, check this joint";
  }
  Serial.printf("  %s  no-pull %2d/20 high, pull-up %2d/20 high  -> %s\n", name, noPull,
                pullUp, verdict);
}

static uint8_t scan(uint8_t sda, uint8_t scl, bool quiet = false) {
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (!quiet) Serial.printf("      found 0x%02X\n", addr);
      found++;
    }
  }
  return found;
}

// Every GPIO that can safely carry I2C at runtime. TX/RX (gpio1/3) are the
// serial port; D0 (gpio16) cannot do open drain; D8 (gpio15) has a 10k
// pull-down on the board.
struct SweepPin {
  uint8_t gpio;
  const char *label;
  const char *expect;  // what a correctly built box shows on this pin
};

static const SweepPin SWEEP[] = {
    {5, "D1", "module pull-up (scl)"},
    {4, "D2", "module pull-up (sda)"},
    {0, "D3", "board 10k pull-up"},
    {2, "D4", "board 10k pull-up + led"},
    {14, "D5", "your 10k int pull-up"},
    {12, "D6", "floating"},
    {13, "D7", "floating"},
};
static const uint8_t SWEEP_N = sizeof(SWEEP) / sizeof(SWEEP[0]);

// Run only when the as-wired scan found nothing: tells you which pins the I2C
// wires actually landed on, or that the modules aren't on any of them.
static void sweep() {
  Serial.println(F("pin sweep (external pull-up = recovers after discharge):"));
  for (uint8_t i = 0; i < SWEEP_N; i++) {
    const int rec = countRecovers(SWEEP[i].gpio);
    Serial.printf("  %s/gpio%-2u  %-12s expected: %s\n", SWEEP[i].label, SWEEP[i].gpio,
                  rec == 20 ? "PULLED UP" : rec == 0 ? "no pull-up" : "unstable",
                  SWEEP[i].expect);
  }

  Serial.println(F("i2c scan across every pin pair:"));
  uint8_t hits = 0;
  for (uint8_t a = 0; a < SWEEP_N; a++) {
    for (uint8_t b = 0; b < SWEEP_N; b++) {
      if (a == b) continue;
      const uint8_t n = scan(SWEEP[a].gpio, SWEEP[b].gpio, true);
      if (n > 0) {
        Serial.printf("  sda=%s scl=%s -> %u device(s)\n", SWEEP[a].label, SWEEP[b].label, n);
        hits++;
      }
    }
  }
  if (hits == 0) Serial.println(F("  nothing on any pair: modules are not reachable from any pin"));
}

void setup() {
  Serial.begin(115200);
  delay(200);
}

void loop() {
  Serial.println(F("\n--- i2c diag ---"));
  Serial.println(F("line levels at idle:"));
  reportLine("D2/gpio4 (sda)", PIN_D2);
  reportLine("D1/gpio5 (scl)", PIN_D1);

  Serial.println(F("scan as wired (sda=D2, scl=D1):"));
  const uint8_t asWired = scan(PIN_D2, PIN_D1);
  Serial.printf("    %u device(s)\n", asWired);

  Serial.println(F("scan swapped (sda=D1, scl=D2):"));
  const uint8_t swapped = scan(PIN_D1, PIN_D2);
  Serial.printf("    %u device(s)\n", swapped);

  if (asWired == 0 && swapped > 0) Serial.println(F("=> SDA and SCL are swapped"));
  if (asWired == 0 && swapped == 0) sweep();

  delay(5000);
}
