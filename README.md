# bambuddy-plate-box

A wall/bench box of buttons that acknowledges plate clears on a
[Bambuddy](https://github.com/maziggy/bambuddy) print farm, so you can confirm at the machine
instead of walking back to a screen.

Bambuddy gates its queue behind a manual plate-clear confirmation: when a print finishes or fails
with something queued behind it, the scheduler waits until someone clicks "Clear Plate & Start
Next". This box is that click, one button per printer, plus an OLED that shows which printers are
actually waiting on you.

ESP8266, direct REST to Bambuddy over the LAN. No shim, no broker, no cloud.

## Hardware

| Part | Notes |
| --- | --- |
| Wemos D1 mini (ESP8266) | Any ESP8266 with I2C broken out |
| SSD1306 0.96" 128x64 I2C OLED | Address `0x3C` |
| PCF8574 I2C expander | Address `0x20`, 8 button inputs |
| Up to 8 momentary buttons | One per printer |
| 10k resistor | Pull-up for the expander's open-drain `/INT` |
| USB 5V supply | Always on |

The expander is what makes "5+" work. Once the OLED takes I2C, a bare ESP8266 has exactly five
safe button pins and no headroom: `D5`/`D6`/`D7` are clean, `D3`/`D4` work but must be unpressed at
boot (holding `D3` at power-up enters flash mode), `D8` must be low at boot, and `D0` has no
internal pull-up or interrupt. The PCF8574 shares the OLED's two pins, has weak pull-ups on all
eight inputs so buttons wire straight to GND, and leaves every other GPIO free.

### Wiring

```
D1 (GPIO5)  -> SCL   (OLED + PCF8574)
D2 (GPIO4)  -> SDA   (OLED + PCF8574)
D5 (GPIO14) -> PCF8574 /INT, 10k pull-up to 3V3
3V3, GND    -> OLED + PCF8574
PCF8574 A0/A1/A2 -> GND        (address 0x20)
PCF8574 P0..P7   -> button -> GND
```

## Bambuddy setup

1. **Settings -> Workflow -> Queue & Dispatch -> Require plate-clear confirmation** must be on.
   Without it `awaiting_plate_clear` never goes true and the box has nothing to show or do.
2. **Settings -> API Keys**: create a key scoped to `printers:read` and `printers:clear_plate`
   only. It lives in ESP8266 flash and is recoverable by anyone holding the board, so keep its
   blast radius to "read printer state and acknowledge plates".
3. Note each printer's numeric `id` from `GET /api/v1/printers/`.

## Configure

```
cp src/secrets.example.h src/secrets.h     # gitignored
```

Fill in WiFi, the Bambuddy host/port, and the API key. Then map buttons to printers in
`src/config.h`, in PCF8574 pin order (index 0 is P0), using `-1` for positions with no button:

```c
static const int BUTTON_PRINTER_IDS[8] = {1, 2, 3, 4, 5, -1, -1, -1};
```

Printer *names* are not configured. The box calls `GET /api/v1/printers/` at boot and hydrates a
label per id, so renaming a printer in Bambuddy follows through on the next reboot. A configured id
that Bambuddy doesn't know renders as `?`.

The trailing slash matters: a real instance 404s on `GET /api/v1/printers` and answers
`/api/v1/printers/`. The box tries the slash first and falls back, so either route shape works. The
response is parsed through an ArduinoJson filter that keeps only `id` and `name` -- the list also
carries every printer's LAN `access_code`, serial number and IP, and none of that should sit in a
device anyone can walk off with.

## Build and flash

```
pio run -t upload && pio device monitor -b 115200
```

Subsequent flashes can go over the air — the box runs `ArduinoOTA` under `OTA_HOSTNAME`.

A bare `pio run` builds only the real firmware (`default_envs = d1_mini`).

### Wiring diagnostic

If the boot log's I2C scan finds nothing, flash the diagnostic instead:

```
pio run -e i2c_diag -t upload && pio device monitor -b 115200
```

Every 5 seconds it reports, for each usable GPIO, whether an external pull-up is present — it
discharges the line and checks whether it recovers, so a floating wire can't pass as connected. It
then scans the bus as wired, swapped, and across every pin pair. That separates swapped SDA/SCL,
wires landed on the wrong pins, unpowered modules, and dead header joints. D3 and D4 should always
read pulled up from the D1 mini's own onboard resistors — if they don't, distrust the rest.

Flash the real firmware back with `pio run -t upload` once it finds `0x3C` and `0x20`.

## How it behaves

```
1 X1C-Left     FINISH      <- inverted: awaiting plate clear, press me
2 X1C-Right   RUNNING
3 A1-Mini        IDLE
4 P1S          FINISH      <- inverted
5 A1          OFFLINE
  bambuddy offline         <- status line, on the first unfitted row
```

**Inverted rows are the ones worth pressing.** Pressing a non-inverted row sends no request at all
— it flashes `nothing to clear` and returns. That gating is the whole reason the box polls status
rather than just firing blind.

Polling is round-robin: one `GET /api/v1/printers/{id}/status` every 1.5s across fitted positions,
parsed with an ArduinoJson stream filter that keeps only `state` and `awaiting_plate_clear` (the
full payload is large and the heap is not). Five printers means each row refreshes every 7.5s at
about 40 req/min, against Bambuddy's documented 100/min read budget.

Writes are bounded on both ends: a 5s per-printer cooldown, and a token bucket of 8 POSTs/min
against the documented 10/min control limit. A 429 flips the status line to `rate limited`.

### Known limitations

- A second button pressed *during* an in-flight HTTP call can be dropped. The OLED shows `BUSY` on
  the active row so the operator knows to wait. Not worth an async HTTP rewrite for a box one
  person uses at a time.
- Plain HTTP, no TLS. Bambuddy is on the LAN and BearSSL is heavy on an ESP8266 — same trust
  boundary as the Bambuddy web UI itself.
- The API key is recoverable from flash. Mitigated by the two-scope key above.

## Reference

- [Bambuddy API reference](https://wiki.bambuddy.cool/reference/api/)
- [Print queue / plate-clear workflow](https://wiki.bambuddy.cool/features/print-queue/)
- [API keys & scopes](https://wiki.bambuddy.cool/features/api-keys/)
