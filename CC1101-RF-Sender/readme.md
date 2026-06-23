# CC1101 RF Signal Sender (ESP32-C3 Super Mini)

Sub-GHz RF transmitter built from an ESP32-C3 Super Mini and a CC1101
breakout module. Lets you transmit raw OOK pulse trains (e.g. captured
remote/garage-door/sensor signals) or hex byte payloads over Serial commands.

> For authorized testing of your own devices only (garage doors, remotes,
> sensors you own) — RF transmission on licensed/shared bands may be
> regulated in your country.

## Wiring

| CC1101 | ESP32-C3 Super Mini |
|--------|----------------------|
| VCC    | 3.3V                 |
| GND    | GND                  |
| CE     | GPIO 20              |
| CNS    | GPIO 21              |
| SCK    | GPIO 4               |
| MOSI   | GPIO 6               |
| MISO   | GPIO 5               |

`CE` is the CC1101's `GDO0` pin — used both for SPI status during normal
operation and for bit-banged OOK output by the `RAW` command. `CNS` is the
SPI chip-select (`CSN`).

## Setup

1. Arduino IDE -> Boards Manager -> install **esp32** (Espressif).
2. Select board: **ESP32C3 Dev Module** (or "ESP32-C3 Super Mini" if your
   board package provides it).
3. Library Manager -> install **SmartRC-CC1101-Driver-Lib** (provides
   `ELECHOUSE_CC1101_SRC_DRV.h`).
4. Wire the module per the table above, flash `CC1101_RF_Sender.ino`.
5. Open Serial Monitor at **115200 baud**, line ending = Newline.

## Commands

```
FREQ <mhz>              Set carrier frequency, e.g. FREQ 433.92
MOD OOK|FSK             Set modulation
RATE <kbps>             Set data rate, e.g. RATE 4.8
SEND <hex> [repeats]    Transmit a hex byte payload, e.g. SEND AABBCC 5
RAW <us,us,...> [reps]  Bit-bang a raw OOK pulse train (durations in
                         microseconds, alternating ON/OFF, starting ON)
                         e.g. RAW 320,320,640,320 3
NOISE <mhz> <ms> [minUs] [maxUs]
                         Emit randomized OOK noise for contained
                         interference testing. See "Faraday box
                         interference testing" below before using this.
STATUS                  Print current frequency/modulation/rate
```

## Faraday box interference testing

`NOISE` is a broadband disruptor, not a normal transmit command — it exists
only to test how a device of yours reacts to interference. **Only ever run
it with the device under test and the CC1101's antenna fully enclosed in a
shielded box/bag.** Operating it over the air is a jammer, which is illegal
to operate in virtually every jurisdiction regardless of intent.

```
NOISE 433.92 2000        # 2s of noise at 433.92MHz, default pulse range
NOISE 433.92 5000 20 100 # 5s, faster/denser pulses (20-100us)
```

Recommended workflow: place the receiver under test and the CC1101 antenna
in the same sealed enclosure, run `NOISE`, and observe whether/when the
receiver loses lock — then stop and remove the enclosure before any other
transmit command.

### Example: replay a captured fixed-code remote

```
FREQ 433.92
MOD OOK
RAW 320,320,640,320,320,640,640,320 5
```

### Example: send a raw byte payload at 868 MHz, 2-FSK

```
FREQ 868.3
MOD FSK
RATE 9.6
SEND DEADBEEF 1
```
