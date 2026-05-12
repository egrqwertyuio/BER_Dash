/*
 * ============================================================
 *  BER (Bearcats Electric Racing) - EV3 Vehicle Bus Dash
 *  Target:  ST7789 170x320 TFT display
 *  CAN DBC: EV3_Vehicle_Bus.dbc
 *  Library: Adafruit_ST7789 + Adafruit_GFX + Cory Fowler mcp_can
 *
 *  ST7789 wiring:
 *    GND -> GND, VCC -> 3.3V, SCL -> GPIO 18, SDA -> GPIO 23
 *    RES -> GPIO 22, DC -> GPIO 21, BLK -> GPIO 17, CS -> GPIO 5
 *
 *  MCP2515 wiring (HSPI):
 *    SCK -> GPIO 14, SI -> GPIO 13, SO -> GPIO 12, CS -> GPIO 15, INT -> GPIO 4
 * ============================================================
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <mcp_can.h>

#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_BL    17

#define CAN_CS    15
#define CAN_INT    4
#define HSPI_SCK  14
#define HSPI_MISO 12
#define HSPI_MOSI 13

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
SPIClass hspi(HSPI);
MCP_CAN CAN(&hspi, CAN_CS);

#define SCREEN_W 170
#define SCREEN_H 320

#define CLR_BG        0x0000
#define CLR_PANEL     0x1082
#define CLR_ACCENT    0xF800
#define CLR_RED_DIM   0x7800
#define CLR_WHITE     0xFFFF
#define CLR_GREY      0x7BEF
#define CLR_DARKGREY  0x39E7
#define CLR_GREEN     0x07E0
#define CLR_YELLOW    0xFFE0
#define CLR_ORANGE    0xFD20

const char* DRIVE_MODES[] = { "STBY", "DRIVE", "REGEN", "ENDUR", "SPORT", "LIMP" };

struct VehicleState {
  bool ecuFault;
  bool mcFault;
  uint16_t ecuFaultBits;
  uint32_t mcFaultBits;

  uint16_t apps0;
  uint16_t apps1;
  uint8_t appsPct;
  int16_t torqueCmd;

  uint16_t bpsRaw;
  uint8_t water1C;
  uint8_t water2C;
  uint8_t water3C;
  bool r2dButton;
  bool prog1;
  bool prog2;
  bool brakePressed;

  uint16_t powerKw;
  float lvVoltage;
  uint8_t battSoc;
  bool initFinished;
  bool prechargeComplete;
  bool r2dActive;
  uint8_t driveMode;
  bool regenEnabled;

  float bmsSoc;
  float bmsCurrent;
  float bmsMaxCellV;
  float bmsMaxCellTempC;
  float bmsMinCellV;
  float bmsMinCellTempC;
  uint8_t bmsPowerLimitKw;
};

VehicleState state = {};
uint32_t rxCount = 0;
uint32_t lastRxMs = 0;
uint32_t lastDrawMs = 0;

uint16_t getU16(const uint8_t *buf, uint8_t byteIndex) {
  return ((uint16_t)buf[byteIndex]) | ((uint16_t)buf[byteIndex + 1] << 8);
}

int16_t getS16(const uint8_t *buf, uint8_t byteIndex) {
  return (int16_t)getU16(buf, byteIndex);
}

bool getBit(const uint8_t *buf, uint8_t bit) {
  return (buf[bit / 8] & (1 << (bit % 8))) != 0;
}

uint32_t getBitsLE(const uint8_t *buf, uint8_t startBit, uint8_t length) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < length; i++) {
    if (getBit(buf, startBit + i)) value |= 1UL << i;
  }
  return value;
}

void drawText(const char* text, int16_t x, int16_t y, uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawRightText(const char* text, int16_t rightX, int16_t y, uint8_t size, uint16_t color) {
  int16_t bx, by;
  uint16_t bw, bh;
  tft.setTextSize(size);
  tft.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
  drawText(text, rightX - bw, y, size, color);
}

void drawCenterText(const char* text, int16_t cx, int16_t y, uint8_t size, uint16_t color) {
  int16_t bx, by;
  uint16_t bw, bh;
  tft.setTextSize(size);
  tft.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
  drawText(text, cx - bw / 2, y, size, color);
}

void drawLabelValue(int y, const char* label, const char* value, uint16_t valueColor = CLR_WHITE) {
  drawText(label, 8, y, 1, CLR_GREY);
  drawRightText(value, SCREEN_W - 8, y, 1, valueColor);
}

void drawSmallStatus(int x, int y, const char* label, bool on, uint16_t onColor = CLR_ACCENT) {
  tft.drawRect(x, y, 36, 15, on ? onColor : CLR_DARKGREY);
  drawCenterText(label, x + 18, y + 4, 1, on ? CLR_WHITE : CLR_GREY);
}

uint16_t faultColor() {
  if (state.ecuFault || state.mcFault) return CLR_ACCENT;
  return CLR_GREEN;
}

void decodeFrame(uint32_t id, const uint8_t *buf, uint8_t len) {
  if (len < 8) return;

  switch (id) {
    case 0x2:
      state.ecuFaultBits = getU16(buf, 0);
      state.ecuFault = getBit(buf, 7) || getBit(buf, 9) || getBit(buf, 6);
      break;

    case 0x3:
      state.mcFaultBits = getBitsLE(buf, 0, 26);
      state.mcFault = getBit(buf, 25);
      break;

    case 0x4:
      state.apps0 = getU16(buf, 0);
      state.apps1 = getU16(buf, 2);
      state.appsPct = buf[4];
      state.torqueCmd = getS16(buf, 5);
      break;

    case 0x5:
      state.bpsRaw = getU16(buf, 0);
      state.water1C = buf[2];
      state.water2C = buf[3];
      state.water3C = buf[4];
      state.r2dButton = getBit(buf, 40);
      state.prog1 = getBit(buf, 41);
      state.prog2 = getBit(buf, 42);
      state.brakePressed = getBit(buf, 43);
      break;

    case 0x6:
      state.powerKw = getU16(buf, 0);
      state.lvVoltage = getU16(buf, 2) * 0.1f;
      state.battSoc = buf[4];
      state.initFinished = getBit(buf, 40);
      state.prechargeComplete = getBit(buf, 41);
      state.r2dActive = getBit(buf, 42);
      state.driveMode = (uint8_t)getBitsLE(buf, 43, 3);
      state.regenEnabled = getBit(buf, 46);
      break;

    case 0x7:
      state.bmsSoc = buf[0] * 0.392156863f;
      state.bmsCurrent = buf[1] * 0.78125f;
      state.bmsMaxCellV = buf[2] * 0.019607843f;
      state.bmsMaxCellTempC = buf[3] * 0.588235294f;
      state.bmsMinCellV = buf[4] * 0.019607843f;
      state.bmsMinCellTempC = buf[5] * 0.588235294f;
      state.bmsPowerLimitKw = buf[6];
      break;
  }
}

void readCANFrames() {
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    uint32_t id;
    uint8_t len;
    uint8_t buf[8];
    if (CAN.readMsgBuf(&id, &len, buf) == CAN_OK) {
      uint32_t cleanId = id & 0x1FFFFFFF;
      decodeFrame(cleanId, buf, len);
      rxCount++;
      lastRxMs = millis();
    }
  }
}

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, 28, CLR_ACCENT);
  drawText("BER EV3", 8, 8, 2, CLR_WHITE);
  drawRightText((lastRxMs && millis() - lastRxMs < 1000) ? "CAN" : "NO CAN", SCREEN_W - 8, 10, 1,
                (lastRxMs && millis() - lastRxMs < 1000) ? CLR_WHITE : CLR_YELLOW);
}

void drawSocBar(int x, int y, int w, int h, float pct) {
  pct = constrain(pct, 0.0f, 100.0f);
  tft.drawRect(x, y, w, h, CLR_GREY);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, CLR_PANEL);
  uint16_t color = (pct > 50.0f) ? CLR_GREEN : (pct > 20.0f ? CLR_YELLOW : CLR_ACCENT);
  int fillW = (int)((pct / 100.0f) * (w - 2));
  tft.fillRect(x + 1, y + 1, fillW, h - 2, color);
}

void drawDashboard() {
  char buf[32];
  tft.fillScreen(CLR_BG);
  drawHeader();

  const char* mode = (state.driveMode < 6) ? DRIVE_MODES[state.driveMode] : "?";
  drawCenterText(mode, SCREEN_W / 2, 38, 2, state.r2dActive ? CLR_WHITE : CLR_GREY);
  drawSmallStatus(8, 62, "INIT", state.initFinished, CLR_GREEN);
  drawSmallStatus(48, 62, "PRE", state.prechargeComplete, CLR_GREEN);
  drawSmallStatus(88, 62, "R2D", state.r2dActive, CLR_GREEN);
  drawSmallStatus(128, 62, "RGN", state.regenEnabled, CLR_ORANGE);

  snprintf(buf, sizeof(buf), "%.0f%%", state.bmsSoc > 0.1f ? state.bmsSoc : (float)state.battSoc);
  drawText("SOC", 8, 88, 1, CLR_GREY);
  drawRightText(buf, SCREEN_W - 8, 82, 3, CLR_WHITE);
  drawSocBar(8, 112, SCREEN_W - 16, 12, state.bmsSoc > 0.1f ? state.bmsSoc : (float)state.battSoc);

  snprintf(buf, sizeof(buf), "%u%%", state.appsPct);
  drawLabelValue(132, "APPS", buf);
  snprintf(buf, sizeof(buf), "%d Nm", state.torqueCmd);
  drawLabelValue(146, "TORQUE", buf);
  snprintf(buf, sizeof(buf), "%u kW", state.powerKw);
  drawLabelValue(160, "POWER", buf);
  snprintf(buf, sizeof(buf), "%.1f V", state.lvVoltage);
  drawLabelValue(174, "LV", buf);

  snprintf(buf, sizeof(buf), "%.0f A / %u kW", state.bmsCurrent, state.bmsPowerLimitKw);
  drawLabelValue(194, "BMS I/LIM", buf);
  snprintf(buf, sizeof(buf), "%.2f / %.2f V", state.bmsMinCellV, state.bmsMaxCellV);
  drawLabelValue(208, "CELL V", buf);
  snprintf(buf, sizeof(buf), "%.0f / %.0f C", state.bmsMinCellTempC, state.bmsMaxCellTempC);
  drawLabelValue(222, "CELL T", buf);
  snprintf(buf, sizeof(buf), "%u %u %u C", state.water1C, state.water2C, state.water3C);
  drawLabelValue(236, "WATER", buf);

  snprintf(buf, sizeof(buf), "%u", state.bpsRaw);
  drawLabelValue(256, "BPS RAW", buf, state.brakePressed ? CLR_ORANGE : CLR_WHITE);
  snprintf(buf, sizeof(buf), "%s %s %s", state.brakePressed ? "BRK" : "---",
           state.r2dButton ? "R2D" : "---", state.prog1 || state.prog2 ? "PRG" : "---");
  drawLabelValue(270, "INPUTS", buf, state.brakePressed ? CLR_ORANGE : CLR_WHITE);

  uint16_t fc = faultColor();
  snprintf(buf, sizeof(buf), "ECU %04X  MC %08lX", state.ecuFaultBits, (unsigned long)state.mcFaultBits);
  drawText("FAULTS", 8, 292, 1, CLR_GREY);
  drawRightText((state.ecuFault || state.mcFault) ? "ACTIVE" : "CLEAR", SCREEN_W - 8, 292, 1, fc);
  drawCenterText(buf, SCREEN_W / 2, 306, 1, fc);
}

void printMcpErrorFlags(byte err) {
  if (err == 0) {
    Serial.print(" [OK]");
    return;
  }
  if (err & 0x01) Serial.print(" [ERROR WARN]");
  if (err & 0x02) Serial.print(" [RX WARN]");
  if (err & 0x04) Serial.print(" [TX WARN]");
  if (err & 0x08) Serial.print(" [RX PASSIVE]");
  if (err & 0x10) Serial.print(" [TX PASSIVE]");
  if (err & 0x20) Serial.print(" [TX BUS OFF]");
  if (err & 0x40) Serial.print(" [RX0 OVR]");
  if (err & 0x80) Serial.print(" [RX1 OVR]");
}

void reportCANStatus() {
  static uint32_t lastReport = 0;
  uint32_t now = millis();
  if (now - lastReport < 2000) return;
  lastReport = now;

  byte err = CAN.getError();
  Serial.print("STATUS EFLG=0x");
  Serial.print(err, HEX);
  Serial.print(" TEC=");
  Serial.print(CAN.errorCountTX());
  Serial.print(" REC=");
  Serial.print(CAN.errorCountRX());
  Serial.print(" RX=");
  Serial.print(rxCount);
  printMcpErrorFlags(err);
  Serial.println();
}

void setupDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(10);
  digitalWrite(TFT_RST, HIGH);
  delay(120);

  tft.init(170, 320, SPI_MODE3);
  tft.setSPISpeed(40000000);
  tft.setRotation(3);
  tft.invertDisplay(false);
}

void setupCAN() {
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT);

  byte beginResult = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  Serial.print("CAN.begin() returned: ");
  Serial.println(beginResult == CAN_OK ? "OK" : "FAIL");

  while (beginResult != CAN_OK) {
    Serial.println("MCP2515 init failed, retrying...");
    delay(500);
    beginResult = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  }

  byte modeResult = CAN.setMode(MCP_NORMAL);
  Serial.print("CAN.setMode(NORMAL) returned: ");
  Serial.print(modeResult);
  Serial.println(modeResult == MCP2515_OK ? " (OK)" : " (FAIL)");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[EV3 Vehicle Bus Dash Receiver] Boot");
  setupCAN();
  setupDisplay();

  tft.fillScreen(CLR_BG);
  drawCenterText("BEARCATS", SCREEN_W / 2, 122, 2, CLR_WHITE);
  drawCenterText("ELECTRIC", SCREEN_W / 2, 150, 2, CLR_ACCENT);
  drawCenterText("RACING", SCREEN_W / 2, 178, 2, CLR_WHITE);
  delay(1200);

  drawDashboard();
}

void loop() {
  readCANFrames();
  reportCANStatus();

  uint32_t now = millis();
  if (now - lastDrawMs >= 250) {
    lastDrawMs = now;
    drawDashboard();
  }
}
