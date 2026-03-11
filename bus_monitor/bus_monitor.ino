/*
 * W65C02S Bus Monitor
 * ──────────────────────────────────────────────────────────────────────
 * Captures address + data bus on every PHI2 rising edge and streams
 * the result over USB-Serial at 115 200 baud.
 *
 * Wiring
 * ──────
 *  65C02 signal  │  Arduino / MCP pin
 * ───────────────┼───────────────────────────────────────────────────
 *  PHI2 (clk)   │  Arduino D2  (INT0)
 *  A0            │  Arduino D3
 *  A1            │  Arduino D4
 *  A2            │  Arduino D5
 *  A3            │  Arduino D6
 *  A4            │  Arduino D7
 *  A5            │  Arduino D8
 *  A6            │  Arduino D9
 *  A7            │  Arduino D10
 *  A8            │  Arduino D11
 *  A9            │  Arduino D12
 *  A10           │  Arduino D13
 *  A11           │  MCP23017 PA0
 *  A12           │  MCP23017 PA1
 *  A13           │  MCP23017 PA2
 *  A14           │  MCP23017 PA3
 *  A15           │  MCP23017 PA4
 *  D0            │  MCP23017 PB0
 *  D1            │  MCP23017 PB1
 *  D2            │  MCP23017 PB2
 *  D3            │  MCP23017 PB3
 *  D4            │  MCP23017 PB4
 *  D5            │  MCP23017 PB5
 *  D6            │  MCP23017 PB6
 *  D7            │  MCP23017 PB7
 *  MCP SDA       │  Arduino A4
 *  MCP SCL       │  Arduino A5
 *  MCP A0/A1/A2  │  GND  (I2C address = 0x20)
 *  MCP RESET     │  3.3 V or 5 V (tie high)
 *
 * Timing note
 * ───────────
 * Reading the MCP23017 over I2C at 400 kHz takes ~70–100 µs, so this
 * monitor works reliably only when PHI2 ≤ ~5 kHz.  At manual / 1 Hz
 * step speeds every edge is captured perfectly.
 *
 * Mock mode
 * ─────────
 * Uncomment MOCK_HW below to run a full self-test with no hardware or
 * wiring changes needed.  The ISR, ring buffer, address/data decode,
 * and serial output are all exercised using software-generated data.
 */

// #define MOCK_HW   // ← uncomment to enable hardware mock / self-test

#include <Wire.h>

// ── Clock ──────────────────────────────────────────────────────────────
#define CLOCK_PIN   2          // PHI2 → INT0 (only pin 2 or 3 on Uno)

// ── MCP23017 ───────────────────────────────────────────────────────────
#define MCP_ADDR    0x20
#define MCP_IODIRA  0x00
#define MCP_IODIRB  0x01
#define MCP_GPIOA   0x12
#define MCP_GPIOB   0x13

// ── Ring buffer ────────────────────────────────────────────────────────
#define BUF_SIZE 16

struct Snapshot {
    uint8_t pd;   // PIND at edge  (A0–A4  in bits 7:3)
    uint8_t pb;   // PINB at edge  (A5–A10 in bits 5:0)
};

volatile uint8_t  g_wIdx = 0;
volatile uint8_t  g_rIdx = 0;
Snapshot          g_buf[BUF_SIZE];
uint32_t          g_cycle = 0;

// ── Mock state (compiled in only when MOCK_HW is set) ─────────────────
#ifdef MOCK_HW
static volatile uint8_t mock_PIND = 0;
static volatile uint8_t mock_PINB = 0;
static uint8_t          mock_mcpA = 0;   // A11–A15
static uint8_t          mock_mcpB = 0;   // D0–D7
static uint32_t         mock_expected_addr = 0;
static uint8_t          mock_expected_data = 0;
static uint32_t         mock_pass = 0;
static uint32_t         mock_fail = 0;
#define SAMPLE_PIND() mock_PIND
#define SAMPLE_PINB() mock_PINB
#else
#define SAMPLE_PIND() PIND
#define SAMPLE_PINB() PINB
#endif

// ── ISR ────────────────────────────────────────────────────────────────
void onClockRise() {
    uint8_t next = (g_wIdx + 1) % BUF_SIZE;
    if (next != g_rIdx) {
        g_buf[g_wIdx] = { SAMPLE_PIND(), SAMPLE_PINB() };
        g_wIdx = next;
    }
}

// ── MCP read (real or mocked) ──────────────────────────────────────────
static void mcpReadTwo(uint8_t startReg, uint8_t *a, uint8_t *b) {
#ifdef MOCK_HW
    (void)startReg;
    *a = mock_mcpA;
    *b = mock_mcpB;
#else
    Wire.beginTransmission(MCP_ADDR);
    Wire.write(startReg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MCP_ADDR, (uint8_t)2);
    *a = Wire.available() ? Wire.read() : 0;
    *b = Wire.available() ? Wire.read() : 0;
#endif
}

// ── Setup ──────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);

#ifdef MOCK_HW
    Serial.println(F("=== MOCK MODE — no hardware needed ==="));
    Serial.println(F("Driving known addr/data patterns and verifying decode."));
    Serial.println(F(""));
    Serial.println(F("  Cycle | Expected addr | Expected data | Got addr | Got data | Result"));
    Serial.println(F("--------+---------------+---------------+----------+----------+-------"));
    // No pins or I2C to configure — fire a software tick every second
#else
    for (int p = 3; p <= 13; p++) pinMode(p, INPUT);
    pinMode(CLOCK_PIN, INPUT);

    Wire.begin();
    Wire.setClock(400000L);

    Wire.beginTransmission(MCP_ADDR);
    Wire.write(MCP_IODIRA);
    Wire.write(0xFF);
    Wire.write(0xFF);
    Wire.endTransmission();

    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), onClockRise, RISING);
    Serial.println(F("READY"));
#endif
}

// ── Mock tick: inject one fake clock edge with known data ──────────────
#ifdef MOCK_HW
static void mockTick() {
    static uint16_t counter = 0;

    // Build a known 16-bit address and 8-bit data from the counter
    uint16_t addr = counter;                  // 0x0000 → 0xFFFF
    uint8_t  data = (uint8_t)(counter & 0xFF);

    // Encode addr into mock pin registers (same layout as real hardware)
    //   A0–A4  → PIND bits 7:3
    //   A5–A10 → PINB bits 5:0
    //   A11–A15 → mock_mcpA bits 4:0
    mock_PIND  = (uint8_t)((addr & 0x1F) << 3);        // A0–A4
    mock_PINB  = (uint8_t)((addr >> 5)   & 0x3F);      // A5–A10
    mock_mcpA  = (uint8_t)((addr >> 11)  & 0x1F);      // A11–A15
    mock_mcpB  = data;                                  // D0–D7

    mock_expected_addr = addr;
    mock_expected_data = data;

    // Fire the ISR directly — no interrupt pin needed
    onClockRise();

    counter++;
}
#endif

// ── Loop ───────────────────────────────────────────────────────────────
void loop() {
#ifdef MOCK_HW
    // Generate one fake edge per second
    static uint32_t lastTick = 0;
    if (millis() - lastTick >= 1000) {
        lastTick = millis();
        mockTick();
    }
#endif

    if (g_rIdx == g_wIdx) return;

    Snapshot snap = g_buf[g_rIdx];
    g_rIdx = (g_rIdx + 1) % BUF_SIZE;
    g_cycle++;

    uint16_t addrLow =
        ((uint16_t)((snap.pd >> 3) & 0x1F))
      | ((uint16_t)( snap.pb       & 0x3F) << 5);

    uint8_t mcpA, mcpB;
    mcpReadTwo(MCP_GPIOA, &mcpA, &mcpB);

    uint16_t address = addrLow | ((uint16_t)(mcpA & 0x1F) << 11);
    uint8_t  data    = mcpB;

#ifdef MOCK_HW
    bool ok = (address == mock_expected_addr) && (data == mock_expected_data);
    ok ? mock_pass++ : mock_fail++;

    char buf[80];
    snprintf(buf, sizeof(buf),
             "%7lu |        $%04X |           $%02X |    $%04X |       $%02X | %s  [P:%lu F:%lu]",
             g_cycle,
             (unsigned int)mock_expected_addr, mock_expected_data,
             (unsigned int)address, data,
             ok ? "PASS" : "FAIL",
             mock_pass, mock_fail);
    Serial.println(buf);
#else
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu,%lu,%04X,%02X",
             (unsigned long)g_cycle, millis(), address, data);
    Serial.println(buf);
#endif
}
