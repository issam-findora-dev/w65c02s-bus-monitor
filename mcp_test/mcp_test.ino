/*
 * mcp_test.ino  –  MCP23017 validation suite
 * ──────────────────────────────────────────────────────────────────────
 * Runs a series of tests against the MCP23017 and reports PASS/FAIL
 * over Serial at 115200 baud.  No external wiring changes needed.
 *
 * Tests
 * ─────
 *  1. I2C presence       – device responds at address 0x20
 *  2. IODIRA write/read  – register survives a round-trip
 *  3. IODIRB write/read  – same for Port B
 *  4. GPPUA write/read   – pull-up register round-trip
 *  5. GPPUB write/read   – pull-up register round-trip
 *  6. Port A pull-up     – with pull-ups enabled, floating pins read 0xFF
 *  7. Port B pull-up     – same for Port B (data bus lines)
 *  8. Port A no pull-up  – with pull-ups off, floating pins read 0x00
 *  9. Port B no pull-up  – same for Port B
 * 10. Port independence  – writing Port A does not corrupt Port B
 */

#include <Wire.h>

#define MCP_ADDR   0x20

// ── Register map (IOCON.BANK = 0, factory default) ──────────────────
#define REG_IODIRA  0x00
#define REG_IODIRB  0x01
#define REG_GPPUA   0x0C
#define REG_GPPUB   0x0D
#define REG_GPIOA   0x12
#define REG_GPIOB   0x13

// ── Helpers ─────────────────────────────────────────────────────────
static void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MCP_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(MCP_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MCP_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

static void resetMCP() {
    // Restore factory defaults: all inputs, no pull-ups
    writeReg(REG_IODIRA, 0xFF);
    writeReg(REG_IODIRB, 0xFF);
    writeReg(REG_GPPUA,  0x00);
    writeReg(REG_GPPUB,  0x00);
}

// ── Test runner ──────────────────────────────────────────────────────
static uint8_t passed = 0;
static uint8_t failed = 0;

static void check(const char *name, bool ok) {
    Serial.print(F("  "));
    Serial.print(ok ? F("[PASS] ") : F("[FAIL] "));
    Serial.println(name);
    ok ? passed++ : failed++;
}

static void checkVal(const char *name, uint8_t got, uint8_t expected) {
    bool ok = (got == expected);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  (got $%02X, expected $%02X)", name, got, expected);
    check(buf, ok);
}

// ── Tests ────────────────────────────────────────────────────────────

void testPresence() {
    Serial.println(F("\n[ 1 ] I2C presence"));
    Wire.beginTransmission(MCP_ADDR);
    uint8_t err = Wire.endTransmission();
    check("MCP23017 responds at 0x20", err == 0);
    if (err != 0) {
        Serial.print(F("      endTransmission error code: "));
        Serial.println(err);
        Serial.println(F("      Check: VCC/GND, SDA/SCL pull-ups (4.7k to 5V), RESET tied high."));
    }
}

void testRegisterRoundtrip() {
    Serial.println(F("\n[ 2 ] IODIRA write / read-back"));
    uint8_t patterns[] = { 0xFF, 0x00, 0xAA, 0x55, 0xF0, 0x0F };
    for (uint8_t p : patterns) {
        writeReg(REG_IODIRA, p);
        uint8_t got = readReg(REG_IODIRA);
        char name[32];
        snprintf(name, sizeof(name), "IODIRA = $%02X", p);
        checkVal(name, got, p);
    }
    writeReg(REG_IODIRA, 0xFF);   // restore

    Serial.println(F("\n[ 3 ] IODIRB write / read-back"));
    for (uint8_t p : patterns) {
        writeReg(REG_IODIRB, p);
        uint8_t got = readReg(REG_IODIRB);
        char name[32];
        snprintf(name, sizeof(name), "IODIRB = $%02X", p);
        checkVal(name, got, p);
    }
    writeReg(REG_IODIRB, 0xFF);
}

void testPullUpRegisters() {
    Serial.println(F("\n[ 4 ] GPPUA write / read-back"));
    writeReg(REG_GPPUA, 0xFF);
    checkVal("GPPUA = $FF", readReg(REG_GPPUA), 0xFF);
    writeReg(REG_GPPUA, 0x00);
    checkVal("GPPUA = $00", readReg(REG_GPPUA), 0x00);

    Serial.println(F("\n[ 5 ] GPPUB write / read-back"));
    writeReg(REG_GPPUB, 0xFF);
    checkVal("GPPUB = $FF", readReg(REG_GPPUB), 0xFF);
    writeReg(REG_GPPUB, 0x00);
    checkVal("GPPUB = $00", readReg(REG_GPPUB), 0x00);
}

void testFloatingPins() {
    // With pull-ups ON, disconnected pins should read 0xFF
    Serial.println(F("\n[ 6 ] Port A floating pins with pull-ups ON  → expect $FF"));
    writeReg(REG_IODIRA, 0xFF);
    writeReg(REG_GPPUA,  0xFF);
    delay(1);
    checkVal("GPIOA (pull-ups on)", readReg(REG_GPIOA), 0xFF);

    Serial.println(F("\n[ 7 ] Port B floating pins with pull-ups ON  → expect $FF"));
    writeReg(REG_IODIRB, 0xFF);
    writeReg(REG_GPPUB,  0xFF);
    delay(1);
    checkVal("GPIOB (pull-ups on)", readReg(REG_GPIOB), 0xFF);

    // With pull-ups OFF, disconnected pins float → typically 0x00
    Serial.println(F("\n[ 8 ] Port A floating pins with pull-ups OFF → expect $00"));
    writeReg(REG_GPPUA, 0x00);
    delay(1);
    checkVal("GPIOA (pull-ups off)", readReg(REG_GPIOA), 0x00);

    Serial.println(F("\n[ 9 ] Port B floating pins with pull-ups OFF → expect $00"));
    writeReg(REG_GPPUB, 0x00);
    delay(1);
    checkVal("GPIOB (pull-ups off)", readReg(REG_GPIOB), 0x00);
}

void testPortIndependence() {
    Serial.println(F("\n[10 ] Port A / B independence"));
    resetMCP();

    // Write distinct patterns to each direction register
    writeReg(REG_IODIRA, 0xAA);
    writeReg(REG_IODIRB, 0x55);
    checkVal("IODIRA after writing both", readReg(REG_IODIRA), 0xAA);
    checkVal("IODIRB after writing both", readReg(REG_IODIRB), 0x55);

    // Overwrite A, B must be unchanged
    writeReg(REG_IODIRA, 0xFF);
    checkVal("IODIRB unchanged after modifying A", readReg(REG_IODIRB), 0x55);

    resetMCP();
}

// ── Setup / main ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);

    Wire.begin();
    Wire.setClock(400000L);

    Serial.println(F("========================================"));
    Serial.println(F("  MCP23017 Validation Suite"));
    Serial.println(F("  I2C addr: 0x20  |  SCL: A5  SDA: A4"));
    Serial.println(F("========================================"));

    testPresence();

    // Only continue if device is reachable
    Wire.beginTransmission(MCP_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println(F("\nDevice not found — skipping remaining tests."));
        Serial.println(F("\n========================================"));
        Serial.println(F("  RESULT: FAIL (device unreachable)"));
        Serial.println(F("========================================"));
        return;
    }

    resetMCP();
    testRegisterRoundtrip();
    testPullUpRegisters();
    testFloatingPins();
    testPortIndependence();
    resetMCP();

    Serial.println(F("\n========================================"));
    char summary[48];
    snprintf(summary, sizeof(summary), "  RESULT: %u passed, %u failed",
             passed, failed);
    if (failed == 0) Serial.println(F("  RESULT: ALL PASSED"));
    else             Serial.println(summary);
    Serial.println(F("========================================"));
}

void loop() {}
