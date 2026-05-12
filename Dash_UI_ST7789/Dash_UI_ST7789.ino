/*
 * ============================================================
 *  BER (Bearcats Electric Racing) — Driver Dashboard UI
 *  Target:  ST7789 170×320 TFT display
 *  MCU:     ESP32 (VSPI)
 *  Library: Adafruit_ST7789 + Adafruit_GFX
 *
 *  Wiring:
 *    ST7789 GND  -> ESP32 GND
 *    ST7789 VCC  -> ESP32 3.3V
 *    ST7789 SCL  -> GPIO 18   (VSPI CLK)
 *    ST7789 SDA  -> GPIO 23   (VSPI MOSI)
 *    ST7789 RES  -> GPIO 22
 *    ST7789 DC   -> GPIO 21
 *    ST7789 BLK  -> GPIO 17   (or tie to 3.3V for always-on)
 *    ST7789 CS   -> GPIO  5   (or GND if no CS pin)
 * ============================================================
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ── Pin definitions ──────────────────────────────────────────
#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_BL    17

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

// ── Display constants ─────────────────────────────────────────
// Physical panel: 170 wide × 320 tall (portrait)
#define SCREEN_W  170
#define SCREEN_H  320
#define CX         85   // centre X
#define CY         160  // center Y
// Vertical zone centres — laid out across the full 320px height
#define ZONE_MODE_Y     30
#define ZONE_SPEED_Y   120
#define ZONE_UNIT_Y    165
#define ZONE_SOC_Y     205
#define ZONE_VOLT_Y    265
#define ZONE_LABEL_Y   290
#define TS_X            43   // left column centre
#define GLV_X          128   // right column centre

// ── BER Colour palette (RGB565) ──────────────────────────────
#define CLR_BG        0x0000
#define CLR_ACCENT    0xF800
#define CLR_WHITE     0xFFFF
#define CLR_GREY      0x7BEF
#define CLR_YELLOW    0xFFE0
#define CLR_GREEN     0x07E0
#define CLR_ORANGE    0xFD20
#define CLR_CYAN      0x07FF

// ── Driver Mode strings ──────────────────────────────────────
const char* MODE_STRINGS[] = {
  "STANDBY", "DRIVE", "REGEN", "ENDURANCE", "SPORT", "LIMP"
};
#define NUM_MODES 6

// ── Data struct ───────────────────────────────────────────────
struct DashData {
  float   soc_pct;
  float   speed_mph;
  float   ts_voltage;
  float   glv_voltage;
  uint8_t driver_mode;
  bool    fault;
};

DashData dashData = { 73.5f, 42.1f, 391.6f, 12.4f, 1, false };

String serialLine;

// ════════════════════════════════════════════════════════════
//  Serial control (test without CAN)
// ════════════════════════════════════════════════════════════
void printSerialHelp() {
  Serial.println();
  Serial.println("Serial controls:");
  Serial.println("  speed <mph>    e.g. speed 42.1");
  Serial.println("  soc <percent>  e.g. soc 73.5");
  Serial.println("  ts <volts>     e.g. ts 391.6");
  Serial.println("  glv <volts>    e.g. glv 12.4");
  Serial.println("  mode <0-5>     0=STANDBY 1=DRIVE 2=REGEN 3=ENDURANCE 4=SPORT 5=LIMP");
  Serial.println("  fault <0|1>    fault 1 shows overlay, 0 clears");
  Serial.println("  show           print current values");
  Serial.println("  help           print this list");
  Serial.println();
}

float clampFloat(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void printDashData() {
  Serial.print("speed=");  Serial.print(dashData.speed_mph, 1);
  Serial.print(" soc=");   Serial.print(dashData.soc_pct, 1);
  Serial.print(" ts=");    Serial.print(dashData.ts_voltage, 1);
  Serial.print(" glv=");   Serial.print(dashData.glv_voltage, 1);
  Serial.print(" mode=");  Serial.print(dashData.driver_mode);
  Serial.print("(");
  Serial.print(dashData.driver_mode < NUM_MODES ? MODE_STRINGS[dashData.driver_mode] : "?");
  Serial.print(") fault="); Serial.println(dashData.fault ? 1 : 0);
}

void handleSerialCommand(String line) {
  line.trim(); line.toLowerCase();
  if (line.length() == 0) return;
  int si = line.indexOf(' ');
  String cmd = (si >= 0) ? line.substring(0, si) : line;
  String arg = (si >= 0) ? line.substring(si + 1) : "";
  arg.trim();

  if      (cmd == "help" || cmd == "?") { printSerialHelp(); }
  else if (cmd == "show")  { printDashData(); }
  else if (cmd == "speed") { dashData.speed_mph   = clampFloat(arg.toFloat(), 0, 999); printDashData(); }
  else if (cmd == "soc")   { dashData.soc_pct     = clampFloat(arg.toFloat(), 0, 100); printDashData(); }
  else if (cmd == "ts")    { dashData.ts_voltage  = clampFloat(arg.toFloat(), 0, 999); printDashData(); }
  else if (cmd == "glv")   { dashData.glv_voltage = clampFloat(arg.toFloat(), 0,  99); printDashData(); }
  else if (cmd == "mode")  { dashData.driver_mode = (uint8_t)clampFloat(arg.toFloat(), 0, NUM_MODES-1); printDashData(); }
  else if (cmd == "fault") { dashData.fault = (arg.toInt() != 0); printDashData(); }
  else { Serial.print("Unknown: "); Serial.println(cmd); printSerialHelp(); }
}

void readSerialControls() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length() > 0) {
        handleSerialCommand(serialLine);
        serialLine = "";
      }
    } else {
      serialLine += c;
      if (serialLine.length() > 64) {
        serialLine = "";
        Serial.println("Command too long.");
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
//  Drawing helpers
// ════════════════════════════════════════════════════════════
void drawCentredString(const char* str, int16_t x, int16_t y,
                       uint8_t sz, uint16_t colour) {
  tft.setTextSize(sz);
  tft.setTextColor(colour);
  int16_t bx, by; uint16_t bw, bh;
  tft.getTextBounds(str, 0, 0, &bx, &by, &bw, &bh);
  tft.setCursor(x - (int16_t)(bw / 2), y - (int16_t)(bh / 2));
  tft.print(str);
}

void clearRegion(int16_t x, int16_t y, uint16_t w, uint16_t h) {
  tft.fillRect(x, y, w, h, CLR_BG);
}

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

// ── Static chrome (drawn once) ────────────────────────────────
void drawChrome() {
  // Top accent bar
  tft.fillRect(0, 0, SCREEN_W, 6, CLR_ACCENT);
  // Bottom accent bar
  tft.fillRect(0, SCREEN_H - 6, SCREEN_W, 6, CLR_ACCENT);

  // Divider above voltage row
  tft.drawFastHLine(10, ZONE_VOLT_Y - 16, SCREEN_W - 20, CLR_GREY);
  // Divider between TS and GLV columns
  tft.drawFastVLine(CX, ZONE_VOLT_Y - 12, 50, CLR_GREY);

  // Divider below mode
  tft.drawFastHLine(10, ZONE_MODE_Y + 18, SCREEN_W - 20, CLR_GREY);

  // Static sub-labels
  drawCentredString("mph",      CX,    ZONE_UNIT_Y,  2, CLR_GREY);
  drawCentredString("TS VOLT",  TS_X,  ZONE_LABEL_Y, 1, CLR_GREY);
  drawCentredString("GLV VOLT", GLV_X, ZONE_LABEL_Y, 1, CLR_GREY);

  // Watermark
  drawCentredString("BER", CX, SCREEN_H - 12, 1, CLR_ACCENT);
}

// ── Dynamic zones ─────────────────────────────────────────────
void drawMode(uint8_t mode) {
  clearRegion(0, ZONE_MODE_Y - 10, SCREEN_W, 20);
  const char* label = (mode < NUM_MODES) ? MODE_STRINGS[mode] : "UNKNOWN";
  drawCentredString(label, CX, ZONE_MODE_Y, 2, modeColour(mode));
}

void drawSpeed(float mph) {
  clearRegion(0, 60, SCREEN_W, 100);
  char buf[8];
  dtostrf(mph, 4, 1, buf);
  drawCentredString(buf, CX, ZONE_SPEED_Y, 6, CLR_WHITE);
}

// SoC bar — full width of the screen with some margin
#define SOC_BAR_X   10
#define SOC_BAR_Y  190
#define SOC_BAR_W  150
#define SOC_BAR_H   14

void drawSoC(float pct) {
  tft.drawRect(SOC_BAR_X, SOC_BAR_Y, SOC_BAR_W, SOC_BAR_H, CLR_GREY);
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, SOC_BAR_W - 2, SOC_BAR_H - 2, CLR_BG);
  uint16_t fillW = (uint16_t)((pct / 100.0f) * (SOC_BAR_W - 2));
  if (fillW > SOC_BAR_W - 2) fillW = SOC_BAR_W - 2;
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, fillW, SOC_BAR_H - 2, socColour(pct));

  // Percentage to the right of bar
  char buf[8]; snprintf(buf, sizeof(buf), "%4.1f%%", pct);
  clearRegion(SOC_BAR_X + SOC_BAR_W + 2, SOC_BAR_Y - 1, 40, SOC_BAR_H + 2);
  tft.setTextSize(1); tft.setTextColor(socColour(pct));
  tft.setCursor(SOC_BAR_X + SOC_BAR_W + 4, SOC_BAR_Y + 3);
  tft.print(buf);
}

void drawTSVoltage(float v) {
  clearRegion(0, ZONE_VOLT_Y - 4, CX, 30);
  char buf[10]; dtostrf(v, 5, 1, buf); strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  drawCentredString(buf, TS_X, ZONE_VOLT_Y + 10, 2, CLR_CYAN);
}

void drawGLVVoltage(float v) {
  clearRegion(CX, ZONE_VOLT_Y - 4, CX, 30);
  char buf[10]; dtostrf(v, 4, 1, buf); strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  drawCentredString(buf, GLV_X, ZONE_VOLT_Y + 10, 2, CLR_YELLOW);
}

void drawFaultOverlay(bool fault) {
  if (fault) {
    tft.drawRect(4, 4, SCREEN_W - 8, SCREEN_H - 8, CLR_ORANGE);
    tft.drawRect(5, 5, SCREEN_W - 10, SCREEN_H - 10, CLR_ORANGE);
    drawCentredString("! FAULT !", CX, CY, 2, CLR_ORANGE);
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
//  setup / loop
// ════════════════════════════════════════════════════════════
DashData prevData = {};

void setup() {
  Serial.begin(115200);

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

  // 170×320 — pass the actual panel dimensions here.
  // SPI_MODE2 works for most 170×320 modules; try SPI_MODE3 if you get
  // a white screen or inverted colours on first boot.
  tft.init(170, 320, SPI_MODE3);
  tft.setSPISpeed(40000000);
  tft.setRotation(3);   // 0 = portrait, USB at bottom. Try 2 if it's upside-down.
  tft.invertDisplay(false);  // flip to true if colours look wrong

  // Splash
  tft.fillScreen(CLR_BG);
  tft.fillRect(0, 0, SCREEN_W, 6, CLR_ACCENT);
  tft.fillRect(0, SCREEN_H - 6, SCREEN_W, 6, CLR_ACCENT);
  drawCentredString("BEARCATS", CX, CY - 20, 2, CLR_WHITE);
  drawCentredString("ELECTRIC", CX, CY,      2, CLR_ACCENT);
  drawCentredString("RACING",   CX, CY + 20, 2, CLR_WHITE);
  delay(2000);

  DashData d = dashData;
  fullRedraw(d);
  prevData = d;

  Serial.println("[BER] Dashboard ready.");
  printSerialHelp();
}

void loop() {
  readSerialControls();
  partialUpdate(prevData, dashData);
  prevData = dashData;
  delay(100);
}
