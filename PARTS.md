# Parts list

For a 5-button box. The firmware supports 8 buttons with no changes beyond `BUTTON_PRINTER_IDS` in
[`src/config.h`](src/config.h), so buy 8 buttons now if the farm might grow — the expander pins are
already there and going back later means reopening the enclosure.

Peso figures are rough Shopee/Lazada ballparks, not quotes. Check before you cart.

## Required

| # | Part | Qty | Notes | ~₱ |
| --- | --- | --- | --- | --- |
| 1 | Wemos D1 mini (ESP8266, ESP-12F) | 1 | CH340 USB-serial version. USB-C or micro-USB both fine. | 120–200 |
| 2 | SSD1306 0.96" 128x64 OLED, **I2C** | 1 | Must be the **4-pin** module (VCC/GND/SCL/SDA). | 90–150 |
| 3 | PCF8574 I2C expander breakout | 1 | Plain breakout with P0–P7 on headers. **Not** an LCD backpack. | 50–100 |
| 4 | Momentary push button | 5 (8) | See sizing below. | 30–60 ea |
| 5 | 10kΩ resistor, 1/4W | 1 | Pull-up for the expander's open-drain `/INT`. | pack ~20 |
| 6 | Hookup wire, 22AWG stranded | ~2m | Two colours minimum: signal and GND. | 60–120 |
| 7 | Project enclosure | 1 | Sized to your buttons — see below. | 100–250 |
| 8 | 5V USB supply + cable | 1 | Any phone charger, 500mA is plenty. | 100–150 |

Ballpark for a 5-button build: **₱700–1,200.**

## Buying gotchas

These are the four ways this order goes wrong.

**The OLED must be I2C, not SPI.** A 7-pin SSD1306 module is SPI and will not work with this
firmware. Count the pins in the listing photo: you want 4. Listings often say "IIC" rather than
"I2C". Most are address `0x3C`; a few ship `0x3D`, which is a one-line change to `OLED_ADDR` in
`src/config.h` if the boot I2C scan shows it.

**The PCF8574 must be a breakout, not an LCD backpack.** The most common PCF8574 listing by far is
the "I2C LCD adapter" — a small board that solders onto a 1602/2004 LCD's 16-pin header. That chip
is the right one but its pins go to the LCD, not to headers you can wire buttons to. You want the
breakout that exposes P0–P7, A0/A1/A2, INT, VCC, GND on 0.1" pins.

**PCF8574 vs PCF8574A.** The plain part answers at `0x20`–`0x27`; the **A** variant answers at
`0x38`–`0x3F`. Either works — if you end up with an A, change `PCF8574_ADDR` to `0x38`. The boot
I2C scan prints what it actually found, so you don't have to guess.

**Tie A0/A1/A2 to GND.** Some breakouts have solder jumpers floating by default. Floating address
pins give you an intermittent device. Ground all three for `0x20`.

## Button sizing

You will be pressing these with a build plate in your hands, sometimes with gloves on.

- **24mm arcade buttons** — recommended. Big enough to hit without looking, snap into a 24mm hole,
  and come with screw-terminal microswitches so no soldering to the button itself.
- **16mm or 19mm metal panel-mount** — smarter looking, needs a stepped drill bit, and the cheap
  ones are often latching rather than momentary. **Confirm the listing says momentary / NO
  (normally open).** A latching button will half-work and confuse you for an hour.
- **12mm tactile with caps** — cheapest, fine for a bench prototype, too small and too stiff for a
  wall box you use every print.

Avoid anything sold as "self-locking" or "latching" — the firmware wants a clean momentary contact.
Illuminated buttons are tempting but their LEDs would need current the ESP8266 can't source
directly, and the OLED already tells you which printer is waiting.

## Optional

| Part | Why |
| --- | --- |
| Perfboard 7x9cm + 0.1" headers | Socket the D1 mini instead of soldering it down, so it's replaceable. |
| JST-XH 2-pin connectors, 8 pairs | Unplug buttons from the board when opening the lid. Worth it. |
| 100nF ceramic caps, 8 | Hardware debounce across each button. The firmware already debounces 25ms, so this is belt-and-braces only. |
| Label maker or printed overlay | The OLED shows printer names, but a label under each button beats reading a screen. |
| M3 standoffs + screws | Mounting the boards inside the enclosure. |

You do **not** need I2C pull-up resistors. Both the OLED and the PCF8574 breakout carry their own
(typically 4.7kΩ); two sets in parallel on a short bus is fine.

## Wiring recap

Full detail is in the [README](README.md). Pin assignments come from
[`src/config.h`](src/config.h):

```
D1 (GPIO5)  -> SCL   (OLED + PCF8574)
D2 (GPIO4)  -> SDA   (OLED + PCF8574)
D5 (GPIO14) -> PCF8574 /INT, with the 10k to 3V3
3V3, GND    -> OLED + PCF8574
PCF8574 A0/A1/A2 -> GND
PCF8574 P0..P7   -> button -> GND     (P0 is button 1)
```

Each button is two wires: one to its `Pn` pin, one to a shared GND bus. No resistors per button —
the PCF8574's internal pull-ups do that job.

## If you skip the expander

You can build a 5-button version with no PCF8574, wiring buttons straight to `D5`, `D6`, `D7`, `D3`
and `D4` with `INPUT_PULLUP`. It costs one part less and gives you zero headroom, and `D3`/`D4`
carry a boot constraint: a button held down at power-up on `D3` drops the ESP into flash mode.
It also needs changes to `src/buttons.cpp`, which currently only knows how to read the expander.
Not recommended, but it's the fallback if the PCF8574 order falls through.
