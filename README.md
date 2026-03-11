# W65C02S Bus Monitor

Captures address bus, data bus, and R/W signal from a WDC W65C02S CPU on every clock rising edge, and streams results to USB serial.

## Hardware

| 65C02 Signal | Arduino Pin | Notes |
|---|---|---|
| PHI2 (clock) | D2 | Interrupt (INT0) |
| RWB | A0 | HIGH=read, LOW=write |
| A0–A10 | D3–D13 | A0→D3 … A10→D13 |
| A11–A15 | MCP PA0–PA4 | |
| D0–D7 | MCP PB0–PB7 | |

**MCP23017 (Waveshare expansion board)**
- I2C address: `0x27` (A0/A1/A2 tied to VCC)
- SDA → Arduino A4, SCL → Arduino A5

> Max reliable CPU clock: ~5 kHz (I2C read takes ~100 µs per cycle)

## Setup

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

## Usage

```bash
make compile      # compile only
make upload       # upload only (skip compile)
make flash        # compile + upload
make read         # stream bus output to terminal
make test         # hardware-free self-test (mock mode, no wiring changes)
make flash-mcp-test  # validate MCP23017 wiring
```

## Serial output format

```
 Cycle  │  Timestamp               │  Boot ms  │  Address  │  Data  │  R/W
────────────────────────────────────────────────────────────────────────────
      1  │  2026-03-11 14:23:01.042 │      312  │  $FFFC    │  $00   │  R
      2  │  2026-03-11 14:23:01.543 │      813  │  $FFFD    │  $80   │  R
      3  │  2026-03-11 14:23:02.101 │     1371  │  $8000    │  $A9   │  R
```

## Project structure

```
bus_monitor/        Main Arduino sketch
bus_monitor_test/   Hardware loopback test (needs jumper A0→D2)
mcp_test/           MCP23017 I2C validation suite
read_serial.py      Rich terminal reader (run via make read)
run_tests.py        Mock-mode test runner (run via make test)
Makefile            All build/flash/test commands
```

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Data always `$00` | MCP not found — run `make flash-mcp-test` |
| MCP not found | Wrong I2C address — see I2C scanner below |
| Missing clock edges | CPU clock too fast for I2C (keep ≤ 5 kHz) |
| Garbled output | Wrong baud rate — set terminal to 115200 |

### Find MCP23017 I2C address

```cpp
#include <Wire.h>
void setup() {
    Serial.begin(115200); Wire.begin();
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { Serial.print("Found: 0x"); Serial.println(a, HEX); }
    }
}
void loop() {}
```
