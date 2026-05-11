/* 
Simply prints hello world on a SSD1306, showcasing a basic SPI setup
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI:
#define OLED_MOSI  23
#define OLED_CLK   18
#define OLED_DC    5
#define OLED_CS    17
#define OLED_RESET 16
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);



void setup() {
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
   Serial.println(F("SSD1306 allocation failed"));
   for(;;); // Don't proceed, loop forever
  }

  // set pins
  pinMode(OLED_MOSI,OUTPUT);
  pinMode(OLED_CLK,OUTPUT);
  pinMode(OLED_DC,OUTPUT);
  pinMode(OLED_RESET,OUTPUT);
  pinMode(OLED_CS,OUTPUT);
  
  digitalWrite(OLED_RESET, HIGH); 
  digitalWrite(OLED_CS, LOW); 
  digitalWrite(OLED_DC, HIGH); 


  // Clear the buffer.
  display.clearDisplay();
  
  // Display Text
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 28);
  display.println("Hello world!");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
}