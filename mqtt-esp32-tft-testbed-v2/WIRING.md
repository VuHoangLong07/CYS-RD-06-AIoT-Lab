# ESP32 and GMT130-V1.0 wiring

Unplug USB before wiring. Use the earlier known-good jumpers/breadboard.

| TFT pin label | ESP32 pin | Purpose |
|---|---|---|
| GND | GND | Common ground |
| VCC | 3V3 | Power for this module |
| SCK | GPIO18 | SPI clock |
| SDA | GPIO23 | SPI MOSI; this SDA label is not I2C |
| RES | GPIO22 | Display reset |
| DC | GPIO21 | SPI data/command |
| BLK | 3V3 | Backlight |

The TFT has no CS pin. Do not add a guessed CS wire; the constructor uses -1.
No MISO wire is required. Use board GPIO labels rather than assumed header
positions. Power the ESP32 by USB and power the TFT from its 3V3/GND pins.
Do not supply TFT data signals from a 5 V UNO header.

The preserved setup is:

```cpp
Adafruit_ST7789 tft(&SPI, -1, 21, 22);
SPI.begin(18, -1, 23, -1);
tft.init(240, 240, TFT_SPI_MODE);
tft.setSPISpeed(10000000);
tft.setRotation(0);
```

The current saved standalone test used SPI_MODE3. The older combined example
defaulted to SPI_MODE0. Both new sketches start with MODE3; if MODE0 is the
verified working mode on the actual screen, change both display_config.h copies.
Initialize and test the screen before Wi-Fi/MQTT so networking failures cannot
prevent the initial status screen from appearing.

## Default output

Commands update the FAN ON/OFF indication on the TFT. ACTUATOR_PIN=-1 means no
external GPIO load. The subscriber role is physical display output and logical
actuator-state feedback; it is not evidence that a motor or relay switched.

## Optional LED only

If using a verified ordinary LED:

| From | To |
|---|---|
| ESP32 GPIO4 | One end of a 1 kOhm resistor |
| Other resistor end | LED anode, usually the longer lead |
| LED cathode, usually short/flat-side lead | ESP32 GND |

Set ACTUATOR_PIN=4 and re-upload. Keep the series resistor. This optional pin
does not overlap the TFT pins. Do not attach a relay coil directly to a GPIO.
A relay is outside this build; any later relay exercise needs supervised,
module-specific low-voltage wiring.

## Checks before power

- Confirm VCC, BLK and GND against the actual TFT labels.
- Confirm SDA is on GPIO23, not an I2C SDA pin chosen from another tutorial.
- Confirm RES/DC are not reversed.
- Check breadboard rows and split power rails for shorts/intermittent contacts.
- No 5 V signal or external power supply is connected to this TFT circuit.

The diagram represents electrical nets, not exact board header positions.
Photograph the real wiring and labels as physical evidence.
