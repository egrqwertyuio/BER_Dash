/*
 * Minimal ST7789 diagnostic
 * Cycles through RED, GREEN, BLUE every second.
 * If you see colors, the screen + wiring work and our app code has a bug.
 * If you see nothing, the screen, wiring, or driver chip is the problem.
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_BL    17

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ST7789 diagnostic starting...");

  // Backlight on
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Manual reset pulse — don't trust the library's reset on flaky boards
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH); delay(10);
  digitalWrite(TFT_RST, LOW);  delay(10);
  digitalWrite(TFT_RST, HIGH); delay(120);

  // Slow SPI to rule out signal-integrity issues
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(170, 320);            // no SPI_MODE arg — use library default
  tft.setSPISpeed(10000000);     // 10 MHz, slow and reliable
  tft.setRotation(0);

  Serial.println("Init done. If you see colors, SPI works.");
}

void loop() {
  tft.fillScreen(0xF800);  // RED
  Serial.println("RED");
  delay(1000);
  tft.fillScreen(0x07E0);  // GREEN
  Serial.println("GREEN");
  delay(1000);
  tft.fillScreen(0x001F);  // BLUE
  Serial.println("BLUE");
  delay(1000);
}
