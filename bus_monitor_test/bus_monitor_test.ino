/*
 * bus_monitor_test.ino
 * ────────────────────────────────────────────────────────────────────
 * Self-contained test for the W65C02S bus monitor.
 * No 65C02 required — the Arduino generates its own test signals.
 *
 * Hardware needed: ONE jumper wire  A0 → D2
 *
 * What it does:
 *   1. Drives a 1 Hz clock on A0  (loop it back to D2 = interrupt pin)
 *   2. On each clock HIGH, sets known values on D3–D13 (address A0–A10)
 *   3. Counts interrupts received and reports to Serial
 *   4. Verifies the cycle count matches the generated clock count
 *
 * Pass criteria:  "generated" and "received" counts stay in sync,
 *                 and the address bits read back match what was driven.
 *
 * Note: MCP23017 is NOT tested here (needs the real hardware).
 *       This test only validates the interrupt + GPIO capture path.
 */

#define CLOCK_OUT_PIN  A0   // drive test clock here  → jumper to D2
#define CLOCK_IN_PIN   2    // interrupt pin

// We drive address bits on D3–D13, then read them back
// via the same ring-buffer ISR used in the real sketch.

// ── Ring buffer (same as production sketch) ──────────────────────────
#define BUF_SIZE 16

struct Snapshot { uint8_t pd; uint8_t pb; };

volatile uint8_t g_wIdx = 0;
volatile uint8_t g_rIdx = 0;
Snapshot         g_buf[BUF_SIZE];

void onClockRise() {
    uint8_t next = (g_wIdx + 1) % BUF_SIZE;
    if (next != g_rIdx) {
        g_buf[g_wIdx] = { PIND, PINB };
        g_wIdx = next;
    }
}

// ── Test state ────────────────────────────────────────────────────────
uint32_t g_generated = 0;
uint32_t g_received  = 0;
uint16_t g_testAddr  = 0;     // increments each cycle so we have known data

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // A0 as clock output
    pinMode(CLOCK_OUT_PIN, OUTPUT);
    digitalWrite(CLOCK_OUT_PIN, LOW);

    // D3–D13 as outputs (we drive known address patterns)
    for (int p = 3; p <= 13; p++) pinMode(p, OUTPUT);

    // D2 as interrupt input
    pinMode(CLOCK_IN_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), onClockRise, RISING);

    Serial.println(F("=== Bus Monitor Self-Test ==="));
    Serial.println(F("Jumper required: A0 -> D2"));
    Serial.println(F(""));
    Serial.println(F("  Gen |  Rcv | Addr driven | Addr read  | Status"));
    Serial.println(F("------+------+-------------+------------+-------"));
}

static void driveAddress(uint16_t addr) {
    // Drive A0–A10 onto D3–D13
    // D3–D7 → A0–A4  (PORTD bits 3–7)
    uint8_t pd = (PORTD & 0x07) | ((addr & 0x1F) << 3);
    PORTD = pd;
    // D8–D13 → A5–A10 (PORTB bits 0–5)
    uint8_t pb = (PORTB & 0xC0) | ((addr >> 5) & 0x3F);
    PORTB = pb;
}

static uint16_t decodeAddr(const Snapshot &s) {
    return ((uint16_t)((s.pd >> 3) & 0x1F))
         | ((uint16_t)( s.pb       & 0x3F) << 5);
}

void loop() {
    // ── Generate one clock pulse per second ───────────────────────────
    static uint32_t lastTick = 0;
    uint32_t now = millis();

    if (now - lastTick >= 1000) {
        lastTick = now;
        g_generated++;

        // Set a known address pattern BEFORE the rising edge
        driveAddress(g_testAddr);

        // Rising edge → triggers ISR
        digitalWrite(CLOCK_OUT_PIN, HIGH);
        delayMicroseconds(100);
        digitalWrite(CLOCK_OUT_PIN, LOW);
    }

    // ── Drain ring buffer ─────────────────────────────────────────────
    if (g_rIdx == g_wIdx) return;

    Snapshot snap = g_buf[g_rIdx];
    g_rIdx = (g_rIdx + 1) % BUF_SIZE;
    g_received++;

    uint16_t addrRead = decodeAddr(snap);
    bool     ok       = (addrRead == g_testAddr);

    char buf[64];
    snprintf(buf, sizeof(buf), "%5lu | %4lu |     $%04X   |    $%04X   | %s",
             g_generated, g_received, g_testAddr, addrRead,
             ok ? "PASS" : "FAIL");
    Serial.println(buf);

    g_testAddr++;   // next cycle will drive a different address
}
