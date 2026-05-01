/*
 * ============================================================
 *  BER (Bearcats Electric Racing) — Driver Dashboard UI
 *  Target:  GC9A01A 240×240 round TFT display
 *  MCU:     ESP32 (any variant with SPI)
 *  Library: Adafruit_GC9A01A + Adafruit_GFX
 *
 *  Wiring (VSPI defaults — adjust to your hardware):
 *    GC9A01A CS   → GPIO 5
 *    GC9A01A DC   → GPIO 2
 *    GC9A01A RST  → GPIO 4
 *    GC9A01A SCL  → GPIO 18  (VSPI CLK)
 *    GC9A01A SDA  → GPIO 23  (VSPI MOSI)
 *    GC9A01A VCC  → 3.3 V
 *    GC9A01A GND  → GND
 *
 *  STAGE: Step 4 — hardcoded values, no CAN yet.
 *         Replace the "HARDCODED DATA" block with CAN reads
 *         once the bus is wired up (Step 5 → 6).
 * ============================================================
 */

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>

// ── Pin definitions ──────────────────────────────────────────
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ── Display constants ─────────────────────────────────────────
#define SCREEN_W  240
#define SCREEN_H  240
#define CX        120   // centre X
#define CY        120   // centre Y

// ── BER Colour palette ────────────────────────────────────────
//  GFX uses 16-bit RGB565
#define CLR_BG        0x0000  // Black
#define CLR_ACCENT    0xF800  // Bearcat Red
#define CLR_WHITE     0xFFFF
#define CLR_GREY      0x7BEF
#define CLR_YELLOW    0xFFE0
#define CLR_GREEN     0x07E0
#define CLR_ORANGE    0xFD20
#define CLR_CYAN      0x07FF

// ── Driver Mode strings (replace numbers from DBC) ───────────
//  Map whatever integer your BMS/ECU sends to one of these
const char* MODE_STRINGS[] = {
  "STANDBY",   // 0
  "DRIVE",     // 1
  "REGEN",     // 2
  "ENDURANCE", // 3
  "SPORT",     // 4
  "LIMP",      // 5 — fault / reduced power
};
#define NUM_MODES 6

// ── Data struct (populated by CAN later) ─────────────────────
struct DashData {
  float  soc_pct;       // 0.0 – 100.0
  float  speed_mph;     // vehicle speed
  float  ts_voltage;    // Tractive System voltage (V)
  float  glv_voltage;   // Grounded Low Voltage (V)
  uint8_t driver_mode;  // index into MODE_STRINGS[]
  bool   fault;         // generic fault flag
};

// ════════════════════════════════════════════════════════════
//  HARDCODED DATA  ← replace this block with CAN reads
// ════════════════════════════════════════════════════════════
DashData getData() {
  DashData d;
  d.soc_pct     = 73.5f;
  d.speed_mph   = 42.1f;
  d.ts_voltage  = 391.6f;
  d.glv_voltage = 12.4f;
  d.driver_mode = 1;      // "DRIVE"
  d.fault       = false;
  return d;
}
// ════════════════════════════════════════════════════════════

// ── Layout constants (all in pixels) ─────────────────────────
// The 240×240 circle gives us comfortable room for six zones:
//
//   ┌──────────────────────────────┐
//   │     [MODE]   top-centre      │   y ≈ 28
//   │   [SPEED]  large, centre     │   y ≈ 90
//   │    "mph"   sub-label         │   y ≈ 130
//   │  [SoC bar]  horizontal       │   y ≈ 155
//   │  [SoC %]   right of bar      │
//   │  [TS V]        [GLV V]       │   y ≈ 185
//   │  "TS VOLT"    "GLV VOLT"     │
//   └──────────────────────────────┘

// ── Helper: draw a centred string ────────────────────────────
void drawCentredString(const char* str, int16_t x, int16_t y,
                       uint8_t textSize, uint16_t colour) {
  tft.setTextSize(textSize);
  tft.setTextColor(colour);
  int16_t bx, by;
  uint16_t bw, bh;
  tft.getTextBounds(str, 0, 0, &bx, &by, &bw, &bh);
  tft.setCursor(x - (int16_t)(bw / 2), y - (int16_t)(bh / 2));
  tft.print(str);
}

// ── Helper: right-aligned string ─────────────────────────────
void drawRightString(const char* str, int16_t x, int16_t y,
                     uint8_t textSize, uint16_t colour) {
  tft.setTextSize(textSize);
  tft.setTextColor(colour);
  int16_t bx, by;
  uint16_t bw, bh;
  tft.getTextBounds(str, 0, 0, &bx, &by, &bw, &bh);
  tft.setCursor(x - (int16_t)bw, y - (int16_t)(bh / 2));
  tft.print(str);
}

// ── Helper: erase a rectangular region ───────────────────────
void clearRegion(int16_t x, int16_t y, uint16_t w, uint16_t h) {
  tft.fillRect(x, y, w, h, CLR_BG);
}

// ── SoC colour ramp ──────────────────────────────────────────
uint16_t socColour(float pct) {
  if (pct > 50.0f) return CLR_GREEN;
  if (pct > 20.0f) return CLR_YELLOW;
  return CLR_ORANGE;   // critical
}

// ── Mode badge colour ─────────────────────────────────────────
uint16_t modeColour(uint8_t mode) {
  switch (mode) {
    case 0: return CLR_GREY;    // STANDBY
    case 1: return CLR_GREEN;   // DRIVE
    case 2: return CLR_CYAN;    // REGEN
    case 3: return CLR_WHITE;   // ENDURANCE
    case 4: return CLR_YELLOW;  // SPORT
    case 5: return CLR_ORANGE;  // LIMP
    default: return CLR_GREY;
  }
}

// ── Draw static chrome (drawn once on boot) ───────────────────
void drawChrome() {
  // Outer ring — Bearcat Red
  tft.drawCircle(CX, CY, 118, CLR_ACCENT);
  tft.drawCircle(CX, CY, 117, CLR_ACCENT);

  // Divider line above voltage row
  tft.drawFastHLine(40, 170, 160, CLR_GREY);

  // Divider between TS and GLV columns
  tft.drawFastVLine(CX, 173, 50, CLR_GREY);

  // Static sub-labels
  drawCentredString("mph",      CX,   138, 1, CLR_GREY);
  drawCentredString("TS VOLT",  CX/2, 215, 1, CLR_GREY);
  drawCentredString("GLV VOLT", CX + CX/2, 215, 1, CLR_GREY);

  // "BER" watermark — very small, bottom of circle
  drawCentredString("BER", CX, 228, 1, CLR_ACCENT);
}

// ── Draw / update the Mode badge ─────────────────────────────
void drawMode(uint8_t mode) {
  clearRegion(40, 14, 160, 22);
  const char* label = (mode < NUM_MODES) ? MODE_STRINGS[mode] : "UNKNOWN";
  drawCentredString(label, CX, 24, 2, modeColour(mode));
}

// ── Draw / update Speed (large centre number) ─────────────────
void drawSpeed(float mph) {
  clearRegion(44, 50, 152, 80);
  char buf[8];
  dtostrf(mph, 4, 1, buf);
  drawCentredString(buf, CX, 95, 5, CLR_WHITE);
}

// ── Draw / update SoC bar + percentage ───────────────────────
#define SOC_BAR_X   40
#define SOC_BAR_Y  148
#define SOC_BAR_W  140
#define SOC_BAR_H   12

void drawSoC(float pct) {
  // Background track
  tft.drawRect(SOC_BAR_X, SOC_BAR_Y, SOC_BAR_W, SOC_BAR_H, CLR_GREY);
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, SOC_BAR_W - 2, SOC_BAR_H - 2, CLR_BG);

  // Fill
  uint16_t fillW = (uint16_t)((pct / 100.0f) * (SOC_BAR_W - 2));
  if (fillW > SOC_BAR_W - 2) fillW = SOC_BAR_W - 2;
  tft.fillRect(SOC_BAR_X + 1, SOC_BAR_Y + 1, fillW, SOC_BAR_H - 2, socColour(pct));

  // Percentage label right of bar
  char buf[8];
  snprintf(buf, sizeof(buf), "%4.1f%%", pct);
  clearRegion(SOC_BAR_X + SOC_BAR_W + 2, SOC_BAR_Y - 2, 52, 16);
  tft.setTextSize(1);
  tft.setTextColor(socColour(pct));
  tft.setCursor(SOC_BAR_X + SOC_BAR_W + 4, SOC_BAR_Y + 2);
  tft.print(buf);
}

// ── Draw / update TS voltage ──────────────────────────────────
void drawTSVoltage(float v) {
  clearRegion(10, 174, 108, 36);
  char buf[10];
  dtostrf(v, 5, 1, buf);
  strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  drawCentredString(buf, CX / 2, 193, 2, CLR_CYAN);
}

// ── Draw / update GLV voltage ─────────────────────────────────
void drawGLVVoltage(float v) {
  clearRegion(122, 174, 108, 36);
  char buf[10];
  dtostrf(v, 4, 1, buf);
  strncat(buf, "V", sizeof(buf) - strlen(buf) - 1);
  drawCentredString(buf, CX + CX / 2, 193, 2, CLR_YELLOW);
}

// ── Fault overlay (full screen flash) ────────────────────────
void drawFaultOverlay(bool fault) {
  if (fault) {
    tft.drawRect(10, 10, 220, 220, CLR_ORANGE);
    tft.drawRect(11, 11, 218, 218, CLR_ORANGE);
    drawCentredString("! FAULT !", CX, CY + 40, 2, CLR_ORANGE);
  }
}

// ─────────────────────────────────────────────────────────────
//  Full redraw helper (call on first paint and after fault clears)
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  Partial update — only repaint changed values, no flicker
// ─────────────────────────────────────────────────────────────
void partialUpdate(const DashData& prev, const DashData& curr) {
  if (curr.driver_mode != prev.driver_mode) drawMode(curr.driver_mode);
  if (curr.speed_mph   != prev.speed_mph)   drawSpeed(curr.speed_mph);
  if (curr.soc_pct     != prev.soc_pct)     drawSoC(curr.soc_pct);
  if (curr.ts_voltage  != prev.ts_voltage)  drawTSVoltage(curr.ts_voltage);
  if (curr.glv_voltage != prev.glv_voltage) drawGLVVoltage(curr.glv_voltage);

  // Fault is special — full redraw to clear or show overlay
  if (curr.fault != prev.fault) {
    if (curr.fault) drawFaultOverlay(true);
    else            fullRedraw(curr);   // clear fault, repaint clean
  }
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────
DashData prevData = {};

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(0);   // 0–3; adjust if your mount is rotated

  // Splash — show team name while CAN initialises
  tft.fillScreen(CLR_BG);
  tft.drawCircle(CX, CY, 118, CLR_ACCENT);
  tft.drawCircle(CX, CY, 117, CLR_ACCENT);
  drawCentredString("BEARCATS",   CX, CY - 16, 2, CLR_WHITE);
  drawCentredString("ELECTRIC",   CX, CY,       2, CLR_ACCENT);
  drawCentredString("RACING",     CX, CY + 16,  2, CLR_WHITE);
  delay(2000);

  // Initial paint with hardcoded (or first CAN) data
  DashData d = getData();
  fullRedraw(d);
  prevData = d;

  Serial.println("[BER] Dashboard ready.");
}

void loop() {
  // ── Step 5/6: replace getData() with a CAN read function ──
  //   e.g.:  DashData d = readFromCAN();
  DashData d = getData();

  partialUpdate(prevData, d);
  prevData = d;

  delay(100);   // ~10 Hz refresh — fast enough for in-car telemetry
}
