/*
 * ============================================================
 *  BER — Step 4 SENDER
 *  ESP32 + MCP2515 (HSPI) + KY-040 rotary encoder
 *
 *  Encoder click  -> cycle through fields to edit
 *  Encoder rotate -> adjust selected field
 *  CAN out         -> 500 kbps, IDs 0x100..0x105
 *
 *  Wiring (matches Step 3 diagram):
 *    MCP2515 SCK  -> GPIO 14
 *    MCP2515 SI   -> GPIO 13
 *    MCP2515 SO   -> GPIO 12
 *    MCP2515 CS   -> GPIO 15
 *    MCP2515 INT  -> GPIO  4
 *    MCP2515 VCC  -> 5V
 *    MCP2515 GND  -> GND
 *
 *    KY-040 CLK   -> GPIO 32
 *    KY-040 DT    -> GPIO 33
 *    KY-040 SW    -> GPIO 25
 *    KY-040 +     -> 3.3V
 *    KY-040 GND   -> GND
 * ============================================================
 */

#include <SPI.h>
#include <mcp_can.h>

// ── MCP2515 pins (HSPI) ──────────────────────────────────────
#define CAN_CS     15
#define CAN_INT     4
#define HSPI_SCK   14
#define HSPI_MISO  12
#define HSPI_MOSI  13

// We need a dedicated HSPI bus instance so we don't fight VSPI later.
SPIClass hspi(HSPI);
MCP_CAN  CAN(&hspi, CAN_CS);

// ── Encoder pins ─────────────────────────────────────────────
#define ENC_CLK  32
#define ENC_DT   33
#define ENC_SW   25

// ── CAN IDs ──────────────────────────────────────────────────
#define ID_SPEED   0x100
#define ID_SOC     0x101
#define ID_TS      0x102
#define ID_GLV     0x103
#define ID_MODE    0x104
#define ID_FAULT   0x105

// ── Local dash state (same struct as the receiver) ───────────
struct DashData {
  float   speed_mph;
  float   soc_pct;
  float   ts_voltage;
  float   glv_voltage;
  uint8_t driver_mode;
  bool    fault;
};

DashData dash = { 42.1f, 73.5f, 391.6f, 12.4f, 1, false };

// Which field the encoder currently edits
enum Field { F_SPEED, F_SOC, F_TS, F_GLV, F_MODE, F_FAULT, F_COUNT };
Field selected = F_SPEED;

const char* FIELD_NAMES[] = { "speed", "soc", "ts", "glv", "mode", "fault" };
#define NUM_MODES 6

// ── Encoder state ────────────────────────────────────────────
volatile int  encDelta    = 0;       // accumulated steps since last loop
volatile bool encPressed  = false;
volatile uint32_t lastBtnMs = 0;
int lastClk = HIGH;

void IRAM_ATTR onEncoderEdge() {
  int clk = digitalRead(ENC_CLK);
  int dt  = digitalRead(ENC_DT);
  // Trigger only on falling edge of CLK — 1 detent = 1 count
  if (clk == LOW && lastClk == HIGH) {
    encDelta += (dt == HIGH) ? +1 : -1;
  }
  lastClk = clk;
}

void IRAM_ATTR onButtonEdge() {
  uint32_t now = millis();
  if (now - lastBtnMs > 200) {  // debounce
    encPressed = true;
    lastBtnMs  = now;
  }
}

// ── CAN send helpers ─────────────────────────────────────────
void sendUint16(uint32_t id, uint16_t v) {
  uint8_t buf[2] = { (uint8_t)(v & 0xFF), (uint8_t)(v >> 8) };  // little-endian
  CAN.sendMsgBuf(id, 0, 2, buf);
}

void sendUint8(uint32_t id, uint8_t v) {
  uint8_t buf[1] = { v };
  CAN.sendMsgBuf(id, 0, 1, buf);
}

void txAll() {
  sendUint16(ID_SPEED, (uint16_t)(dash.speed_mph   * 10.0f));
  sendUint16(ID_SOC,   (uint16_t)(dash.soc_pct     * 10.0f));
  sendUint16(ID_TS,    (uint16_t)(dash.ts_voltage  * 10.0f));
  sendUint16(ID_GLV,   (uint16_t)(dash.glv_voltage * 10.0f));
  sendUint8 (ID_MODE,  dash.driver_mode);
  sendUint8 (ID_FAULT, dash.fault ? 1 : 0);
}

// ── Apply encoder delta to the selected field ─────────────────
void applyDelta(int d) {
  if (d == 0) return;
  switch (selected) {
    case F_SPEED: dash.speed_mph   = constrain(dash.speed_mph   + d * 1.0f, 0.0f, 200.0f); break;
    case F_SOC:   dash.soc_pct     = constrain(dash.soc_pct     + d * 1.0f, 0.0f, 100.0f); break;
    case F_TS:    dash.ts_voltage  = constrain(dash.ts_voltage  + d * 5.0f, 0.0f, 600.0f); break;
    case F_GLV:   dash.glv_voltage = constrain(dash.glv_voltage + d * 0.1f, 0.0f,  20.0f); break;
    case F_MODE:  {
                    int m = (int)dash.driver_mode + d;
                    while (m < 0) m += NUM_MODES;
                    dash.driver_mode = (uint8_t)(m % NUM_MODES);
                  } break;
    case F_FAULT: dash.fault = !dash.fault; break;
    default: break;
  }
}

void printStatus() {
  Serial.print("[edit:");
  Serial.print(FIELD_NAMES[selected]);
  Serial.print("] speed=");  Serial.print(dash.speed_mph, 1);
  Serial.print(" soc=");     Serial.print(dash.soc_pct, 1);
  Serial.print(" ts=");      Serial.print(dash.ts_voltage, 1);
  Serial.print(" glv=");     Serial.print(dash.glv_voltage, 1);
  Serial.print(" mode=");    Serial.print(dash.driver_mode);
  Serial.print(" fault=");   Serial.println(dash.fault ? 1 : 0);
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[BER Sender] Boot");

  // Encoder pins
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  lastClk = digitalRead(ENC_CLK);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), onEncoderEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_SW),  onButtonEdge,  FALLING);

  // HSPI bus for CAN
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT);

  // 500 kbps @ 8 MHz crystal — note MCP_8MHZ, NOT MCP_16MHZ
  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("MCP2515 ready @ 500 kbps");

  printStatus();
  Serial.println("Rotate to change value, click to cycle field.");
}

void loop() {
  // Capture encoder activity atomically
  noInterrupts();
  int  d  = encDelta;       encDelta   = 0;
  bool bp = encPressed;     encPressed = false;
  interrupts();

  bool changed = false;

  if (bp) {
    selected = (Field)((selected + 1) % F_COUNT);
    changed = true;
  }

  if (d != 0) {
    applyDelta(d);
    changed = true;
  }

  // Send everything any time something changed (cheap on a quiet bus)
  static uint32_t lastTx = 0;
  uint32_t now = millis();
  if (changed || (now - lastTx > 200)) {  // also heartbeat at 5 Hz
    txAll();
    lastTx = now;
    if (changed) printStatus();
  }

  delay(5);
}
