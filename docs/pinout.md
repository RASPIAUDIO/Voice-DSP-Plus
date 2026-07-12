# Voice DSP+ Pinout

## Main Devices

| Device | Interface | Address |
| --- | --- | --- |
| TLV320DAC3101 codec | I2C | `0x18` |
| PCAL6408APWJ expander | I2C | `0x20` |
| XVF3800 host control | I2C, Raspberry Pi mode | `0x2c` |

## XMOS Signals

| Function | XMOS signal |
| --- | --- |
| I2C SCL | `X0D37`, package pin 44 |
| I2C SDA | `X0D38`, package pin 45 |
| DAC reset | `X0D29`, GPO 8C bit 3 |
| Red LED | `X0D32`, GPO 8C bit 6, active low |
| Green LED | `X0D33`, GPO 8C bit 7, active low |
| User button | `X1D09`, active low |
| WS2812B data | `X1D13` |
| Microphone data 0..3 | `X0D40..X0D43` |

## PCAL6408A

| Pin | Signal | Use |
| --- | --- | --- |
| `P0` | `XVF_RST_N` | Reset-related control |
| `P1` | `INT_N` | Interrupt input |
| `P2` | `DAC_RST_N` | Shared net; direct XMOS reset remains owner |
| `P3` | `BOOT_SEL` | Boot selection |
| `P4` | `AMP_CTRL1` | Speaker amplifier mode |
| `P5` | `JACK_DET` | `1` when jack is inserted |
| `P6` | `MIC_DETECT` | `1` SQUARE, `0` LINE |
| `P7` | `AMP_CTRL2` | Speaker amplifier mode |

## Raspberry Pi 40-pin Header

| Header pin | Raspberry Pi signal | Voice DSP+ signal |
| --- | --- | --- |
| 3 | GPIO2 / SDA1 | `PI_I2C_SDA` |
| 5 | GPIO3 / SCL1 | `PI_I2C_SCL` |
| 7 | GPIO4 | `PI_MCLK` |
| 11 | GPIO17 | `PI_INT_N` |
| 12 | GPIO18 / PCM_CLK | `PI_I2S_BCLK` |
| 19 | GPIO10 / SPI0_MOSI | `PI_SPI_MOSI` |
| 21 | GPIO9 / SPI0_MISO | `PI_SPI_MISO` |
| 23 | GPIO11 / SPI0_SCLK | `PI_SPI_CLK` |
| 24 | GPIO8 / SPI0_CE0 | `PI_SPI_CS_N` |
| 27 | GPIO0 / ID_SD | HAT EEPROM data |
| 28 | GPIO1 / ID_SC | HAT EEPROM clock |
| 35 | GPIO19 / PCM_FS | `PI_I2S_LRCK` |
| 38 | GPIO20 / PCM_DIN | `PI_I2S_DOUT` from Voice DSP+ |
| 40 | GPIO21 / PCM_DOUT | `PI_I2S_DIN` to Voice DSP+ |

The Raspberry Pi is I2S master in the supported HAT profile. Do not connect
external drivers to these buses while the board is powered.
