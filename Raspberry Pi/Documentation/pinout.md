# V2 H8 - physical Raspberry Pi 40-pin header

Numbers below are **physical Raspberry Pi pins**, not BCM GPIO numbers and not
the EasyEDA H8 pad labels. Locate physical pin1 on the real connector before use;
the drawing shows numbering, not a promise about a photograph's orientation.

```text
              odd   even
  NC          1  o o  2    5V
  SDA1        3  o o  4    5V
  SCL1        5  o o  6    GND
  MCLK*       7  o o  8    NC
  GND         9  o o 10    NC
  NC         11  o o 12    BCK
  INT_N      13  o o 14    GND
  NC         15  o o 16    NC
  NC         17  o o 18    NC
  MOSI       19  o o 20    GND
  MISO       21  o o 22    NC
  SCLK       23  o o 24    CE0
  GND        25  o o 26    NC
  EEDATA     27  o o 28    EECLK
  NC         29  o o 30    GND
  NC         31  o o 32    NC
  NC         33  o o 34    GND
  LRCLK      35  o o 36    NC
  NC         37  o o 38    capture
  GND        39  o o 40    playback
```

| Physical pin | Pi signal | V2 net | Use |
| --- | --- | --- | --- |
| 1 | 3V3 | NC | Not connected on V2 |
| 2 | 5V | 5V | Regulated supply rail |
| 3 | BCM2 / SDA1 | I2C_SDA | Host control |
| 4 | 5V | 5V | Regulated supply rail |
| 5 | BCM3 / SCL1 | I2C_SCL | Host control |
| 6 | Ground | GND | Ground |
| 7 | BCM4 | MCLK | Keep input/high impedance in Pi kit |
| 8 | BCM14 | NC | Not connected on V2 |
| 9 | Ground | GND | Ground |
| 10 | BCM15 | NC | Not connected on V2 |
| 11 | BCM17 | NC | Not connected on V2 |
| 12 | BCM18 | BCK | Pi -> XMOS bit clock |
| 13 | BCM27 | PI_INT_N | Reserved input; not used by current Pi controller |
| 14 | Ground | GND | Ground |
| 15 | BCM22 | NC | Not connected on V2 |
| 16 | BCM23 | NC | Not connected on V2 |
| 17 | 3V3 | NC | Not connected on V2 |
| 18 | BCM24 | NC | Not connected on V2 |
| 19 | BCM10 / MOSI | PI_SPI_MOSI | SPI boot/control |
| 20 | Ground | GND | Ground |
| 21 | BCM9 / MISO | PI_SPI_MISO | SPI boot/control |
| 22 | BCM25 | NC | Not connected on V2 |
| 23 | BCM11 / SCLK | PI_SPI_CLK | SPI boot/control |
| 24 | BCM8 / CE0 | PI_SPI_CS_N | SPI chip select |
| 25 | Ground | GND | Ground |
| 26 | BCM7 / CE1 | NC | Not connected on V2 |
| 27 | BCM0 / ID_SD | EEDATA | Reserved HAT EEPROM |
| 28 | BCM1 / ID_SC | EECLK | Reserved HAT EEPROM |
| 29 | BCM5 | NC | Not connected on V2 |
| 30 | Ground | GND | Ground |
| 31 | BCM6 | NC | Not connected on V2 |
| 32 | BCM12 | NC | Not connected on V2 |
| 33 | BCM13 | NC | Not connected on V2 |
| 34 | Ground | GND | Ground |
| 35 | BCM19 | I2S_LRCK | Pi -> XMOS frame clock |
| 36 | BCM16 | NC | Not connected on V2 |
| 37 | BCM26 | NC | Not connected on V2 |
| 38 | BCM20 | I2S_DATA1 | XMOS -> Pi capture |
| 39 | Ground | GND | Ground |
| 40 | BCM21 | DIN_PRI | Pi -> XMOS playback |

NC means not connected to Voice DSP+, not globally unused on a running Pi.
GPIO logic is 3.3 V. Never drive 5 V/20 V into a GPIO. Pins1/17 still carry the
Pi's own 3.3 V when mounted, despite being NC on the Voice DSP+ side.
I2S is 48 kHz, two 32-bit slots, BCLK3.072 MHz; the application program is mono.
Do not configure GPIO4 as an output clock. Only one I2C/SPI master may own the
attached board: remove ZeroCore before Pi use, and do not run the standalone
USB firmware as another I2C master at the same time.

Source: V2.02 interface mapping and the tested
[Pi overlay](../Sources/host/v2/pi/voicedsp-v2-overlay.dts)/loader.
This footprint's CAD pad pairs are swapped
relative to physical Pi numbering: physical odd n maps to CAD n+1, even n to n-1.
Do not infer that rule for another footprint/revision. Standard Pi GPIO reference:
[Raspberry Pi documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio).
