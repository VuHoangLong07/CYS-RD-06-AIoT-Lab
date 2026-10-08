# Wiring map: two UNO R4 WiFi boards and ABX00102

Unplug USB before making connections. Label boards #1 sensor and #2 actuator.

## Board #1: Modulino Distance ABX00102

Use the supplied intact four-wire Qwiic cable. One end goes into the UNO R4
WiFi's Qwiic socket; the other goes into either Qwiic socket on ABX00102. The
keyed connector provides the correct mapping. Do not force it backwards.

| Electrical net | UNO R4 WiFi Qwiic socket | ABX00102 Qwiic socket |
|---|---|---|
| Supply | 3.3 V | 3V3 |
| Ground | GND | GND |
| I2C data | SDA, Wire1 | SDA |
| I2C clock | SCL, Wire1 | SCL |

This is not a left-to-right connector pin order. Do not cut the cable or infer
signal assignments from wire colour. Use the keyed sockets and printed labels.
The module's extra GPIO1/XSHUT pads are not needed for this exercise. Leave its
unused Qwiic socket and extra pads unconnected.

The Qwiic connection supplies power and data together. No extra jumper to the
UNO 5 V pin, no DHT22 pull-up, and no loose A4/A5 wires are needed. ABX00102 is
a 3.3 V module with default I2C address 0x29. The R4 WiFi's Qwiic bus is Wire1;
the A4/A5 header bus is a different interface. The supplied sketches call
Modulino.begin(), which selects Wire1 for this board.

Use a USB-C data cable to power/program the UNO. Keep the sensor opening clear
and point it at a flat opaque target, initially 100-500 mm away.

## Board #2: built-in L LED (default)

No external LED wires are required. Both LED sketches use:

```cpp
const int LED_PIN = LED_BUILTIN;
```

Observe the small L LED, not the power indicator or the 12x8 LED matrix.
HIGH means ON and LOW means OFF in these sketches.

## Optional external LED

Only use this option if you have an LED and a 1 kOhm resistor. Change LED_PIN
to 7 in the LED test and actuator sketches, then re-upload.

| Connection | From | To |
|---|---|---|
| Drive | Board #2 D7 | One end of a 1 kOhm resistor |
| Current-limited drive | Other resistor end | LED anode (+), usually longer lead |
| Return | LED cathode (-), usually short / flat-side lead | Board #2 GND |

The resistor is in series, and its orientation does not matter. The anode and
cathode must be correct. Never connect an LED directly to the GPIO. For a red
LED with about 2 V drop, (5 V - 2 V) / 1000 Ohm is about 3 mA.

## Power and communication

Power each UNO from its own USB-C cable. No inter-board signal wire or shared
breadboard supply rail is required; communication goes through Wi-Fi, Mosquitto
and Node-RED. Do not connect two 5 V outputs together. The module's Qwiic supply
is 3.3 V even though the UNO's ordinary digital GPIO uses 5 V logic.

## Before power-up

- Cable is in the board's Qwiic socket and module's Qwiic socket.
- Module has no connection to 5 V or the other board's supply.
- For external LED: correct D7, local GND, polarity and 1 kOhm series resistor.
- No bare relay coil is connected to a GPIO.

The wiring-diagram files show electrical connections, not exact header positions.
Add photographs of your real board labels/cable/LED as evidence.

Sources: https://docs.arduino.cc/resources/datasheets/ABX00102-datasheet.pdf ;
https://docs.arduino.cc/hardware/uno-r4-wifi/ ;
https://github.com/arduino-libraries/Arduino_Modulino/blob/main/src/Modulino.h
