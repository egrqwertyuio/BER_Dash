/*
 * ============================================================
 *  BER — Step 4 RECEIVER
 *  ESP32 + ST7789P3 1.9" 170x320 (TFT_eSPI) + MCP2515 (HSPI)
 *  Landscape orientation: 320 wide x 170 tall, USB on the left
 *
 *  Libraries:
 *    - TFT_eSPI by Bodmer  (screen — pins configured in User_Setup.h)
 *    - mcp_can by Cory J. Fowler (CAN)
 *
 *  CAN wiring (HSPI):
 *    MCP2515 SCK -> GPIO 14
 *    MCP2515 SI  -> GPIO 13
 *    MCP2515 SO  -> GPIO 12
 *    MCP2515 CS  -> GPIO 15
 *    MCP2515 INT -> GPIO  4
 *    MCP2515 VCC -> 5V
 *    MCP2515 GND -> GND  (must share GND with the sender board)
 *
 *  IMPORTANT: TFT_eSPI reads screen pins from User_Setup.h.
 *  Make sure the screen pins there do NOT clash with CAN pins
 *  14, 13, 12, 15, or 4.
 * ============================================================
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <mcp_can.h>

// ── Screen ───────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

#define SCREEN_W  320
#define SCREEN_H  170
#define CX        160
#define CY         85

// ── CAN pins (HSPI) ──────────────────────────────────────────
#define CAN_CS    15
#define CAN_INT    4
#define HSPI_SCK  14
#define HSPI_MISO 12
#define HSPI_MOSI 13

SPIClass hspi(HSPI);
MCP_CAN  CAN(&hspi, CAN_CS);

// ── Palette ──────────────────────────────────────────────────
#define CLR_BG     0x0000
#define CLR_ACCENT 0xF800
#define CLR_WHITE  0xFFFF
#define CLR_GREY   0x7BEF
#define CLR_YELLOW 0xFFE0
#define CLR_GREEN  0x07E0
#define CLR_ORANGE 0xFD20
#define CLR_CYAN   0x07FF

const char* MODE_STRINGS[] = {
  "STANDBY", "DRIVE", "REGEN", "ENDURANCE", "SPORT", "LIMP"
};
#define NUM_MODES 6

// ── CAN IDs ──────────────────────────────────────────────────
#define ID_SPEED   0x100
#define ID_SOC     0x101
#define ID_TS      0x102
#define ID_GLV     0x103
#define ID_MODE    0x104
#define ID_FAULT   0x105

struct DashData {
  float   speed_mph;
  float   soc_pct;
  float   ts_voltage;
  float   glv_voltage;
  uint8_t driver_mode;
  bool    fault;
};

DashData dashData = { 42.1f, 73.5f, 391.6f, 12.4f, 1, false };
DashData prevData = {};

// ════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════
uint16_t socColour(float pct) {
  if (pct > 50.0f) return CLR_GREEN;
  if (pct > 20.0f) return CLR_YELLOW;
  return CLR_ORANGE;
}

uint16_t modeColour(uint8_t mode) {
  switch (mode) {
    case 0: return CLR_GREY;
    case 1: return CLR_GREEN;
    case 2: return CLR_CYAN;
    case 3: return CLR_WHITE;
    case 4: return CLR_YELLOW;
    case 5: return CLR_ORANGE;
    default: return CLR_GREY;
  }
}

void clearRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRect(x, y, w, h, CLR_BG);
}

void drawCentredString(const char* str, int16_t x, int16_t y,
                       uint8_t font, uint16_t colour) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(colour, CLR_BG);
  tft.drawString(str, x, y, font);
}

// ════════════════════════════════════════════════════════════
//  Drawing
// ════════════════════════════════════════════════════════════
void drawChrome() {
  tft.fillRect(0, 0,           SCREEN_W, 4, CLR_ACCENT);
  tft.fillRect(0, SCREEN_H - 4, SCREEN_W, 4, CLR_ACCENT);
  tft.drawFastVLine(195, 30, 100, CLR_GREY);
  drawCentredString("mph", 95, 130, 2, CLR_GREY);
  drawCentredString("TS",  215, 50, 2, CLR_GREY);
  drawCentredString("GLV", 215, 95, 2, CLR_GREY);
  drawCentredString("SoC", 18, 152, 1, CLR_GREY);
  drawCentredString("BER", SCREEN_W - 20, SCREEN_H - 12, 1, CLR_ACCENT);
}

void drawMode(uint8_t mode) {
  clearRegion(0, 6, 160, 22);
  const char* label = (mode < NUM_MODES) ? MODE_STRINGS[mode] : "UNKNOWN";
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(modeColour(mode), CLR_BG);
  tft.drawString(label, 8, 17, 2);
}

void drawSpeed(float mph) {
  clearRegion(5, 35, 190, 80);
  char buf[8];
  dtostrf(mph, 4, 1, buf);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_WHITE, CLR_BG);
  // Font 7 = big 7-segment numeric. Falls back to font 6 if not enabled in TFT_eSPI.
  tft.drawString(buf, 95, 75, 7);
}

void drawTSVoltage(float v) {
  clearRegion(240, 40, 80, 22);
  char buf[10];
  dtostrf(v, 5, 1, buf);
  strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(CLR_CYAN, CLR_BG);
  tft.drawString(buf, 240, 50, 2);
}

void drawGLVVoltage(float v) {
  clearRegion(240, 85, 80, 22);
  char buf[10];
  dtostrf(v, 4, 1, buf);
  strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(CLR_YELLOW, CLR_BG);
  tft.drawString(buf, 240, 95, 2);
}

#define SOC_BAR_X   40
#define SOC_BAR_Y  144
#define SOC_BAR_W  220
#define SOC_BAR_H   14

void drawSoC(float pct) {
  tft.drawRect(SOC_BAR_X, SOC_BAR_Y, SOC_BAR_W, SOC_BAR_H, CLR_GREY);
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, SOC_BAR_W - 2, SOC_BAR_H - 2, CLR_BG);
  uint16_t fillW = (uint16_t)((pct / 100.0f) * (SOC_BAR_W - 2));
  if (fillW > SOC_BAR_W - 2) fillW = SOC_BAR_W - 2;
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, fillW, SOC_BAR_H - 2, socColour(pct));

  char buf[8];
  snprintf(buf, sizeof(buf), "%4.1f%%", pct);
  clearRegion(SOC_BAR_X + SOC_BAR_W + 4, SOC_BAR_Y - 2, 50, SOC_BAR_H + 4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(socColour(pct), CLR_BG);
  tft.drawString(buf, SOC_BAR_X + SOC_BAR_W + 6, SOC_BAR_Y + SOC_BAR_H / 2, 1);
}

void drawFaultOverlay(bool fault) {
  if (fault) {
    tft.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, CLR_ORANGE);
    tft.drawRect(3, 3, SCREEN_W - 6, SCREEN_H - 6, CLR_ORANGE);
    drawCentredString("! FAULT !", SCREEN_W - 60, 17, 2, CLR_ORANGE);
  }
}

void fullRedraw(const DashData& d) {
  tft.fillScreen(CLR_BG);
  drawChrome();
  drawMode(d.driver_mode);
  drawSpeed(d.speed_mph);
  drawSoC(d.soc_pct);
  drawTSVoltage(d.ts_voltage);
  drawGLVVoltage(d.glv_voltage);
  if (d.fault) drawFaultOverlay(true);
}

void partialUpdate(const DashData& prev, const DashData& curr) {
  if (curr.driver_mode != prev.driver_mode) drawMode(curr.driver_mode);
  if (curr.speed_mph   != prev.speed_mph)   drawSpeed(curr.speed_mph);
  if (curr.soc_pct     != prev.soc_pct)     drawSoC(curr.soc_pct);
  if (curr.ts_voltage  != prev.ts_voltage)  drawTSVoltage(curr.ts_voltage);
  if (curr.glv_voltage != prev.glv_voltage) drawGLVVoltage(curr.glv_voltage);
  if (curr.fault != prev.fault) {
    if (curr.fault) drawFaultOverlay(true);
    else            fullRedraw(curr);
  }
}

// ════════════════════════════════════════════════════════════
//  CAN
// ════════════════════════════════════════════════════════════
uint16_t readLE16(const uint8_t* d) {
  return (uint16_t)d[0] | ((uint16_t)d[1] << 8);
}

void handleCanFrame(uint32_t id, uint8_t len, const uint8_t* d) {
  switch (id) {
    case ID_SPEED: if (len >= 2) dashData.speed_mph   = readLE16(d) / 10.0f; break;
    case ID_SOC:   if (len >= 2) dashData.soc_pct     = readLE16(d) / 10.0f; break;
    case ID_TS:    if (len >= 2) dashData.ts_voltage  = readLE16(d) / 10.0f; break;
    case ID_GLV:   if (len >= 2) dashData.glv_voltage = readLE16(d) / 10.0f; break;
    case ID_MODE:  if (len >= 1) dashData.driver_mode = d[0] % NUM_MODES;    break;
    case ID_FAULT: if (len >= 1) dashData.fault       = (d[0] != 0);         break;
    default: return;
  }

  Serial.print("RX 0x"); Serial.print(id, HEX);
  Serial.print("  len="); Serial.print(len);
  Serial.print("  data=");
  for (uint8_t i = 0; i < len; i++) {
    if (d[i] < 0x10) Serial.print('0');
    Serial.print(d[i], HEX); Serial.print(' ');
  }
  Serial.println();
}

void pollCAN() {
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    uint32_t id;
    uint8_t  len;
    uint8_t  buf[8];
    if (CAN.readMsgBuf(&id, &len, buf) == CAN_OK) {
      handleCanFrame(id & 0x1FFFFFFF, len, buf);
    }
  }
}

// ════════════════════════════════════════════════════════════
//  setup / loop
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[BER Receiver] Boot");

  // Screen — uses pins from User_Setup.h
  tft.init();
  tft.setRotation(3);              // landscape, USB on left
  tft.fillScreen(CLR_BG);

  // Splash
  tft.fillRect(0, 0,           SCREEN_W, 4, CLR_ACCENT);
  tft.fillRect(0, SCREEN_H - 4, SCREEN_W, 4, CLR_ACCENT);
  drawCentredString("BEARCATS ELECTRIC RACING", CX, CY - 12, 2, CLR_WHITE);
  drawCentredString("Driver Dashboard",         CX, CY + 12, 2, CLR_ACCENT);

  // CAN bus on HSPI
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT);
  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("MCP2515 ready @ 500 kbps");

  delay(1500);
  fullRedraw(dashData);
  prevData = dashData;
}

void loop() {
  pollCAN();

  static uint32_t lastDraw = 0;
  uint32_t now = millis();
  if (now - lastDraw > 50) {
    partialUpdate(prevData, dashData);
    prevData = dashData;
    lastDraw = now;
  }
}
