/*
 * BER — CAN Receiver DEBUG version
 * Reports MCP2515 state and error counters every 2 seconds,
 * plus prints any received frames.
 *
 * Wiring (HSPI):
 *   MCP2515 SCK -> GPIO 14
 *   MCP2515 SI  -> GPIO 13
 *   MCP2515 SO  -> GPIO 12
 *   MCP2515 CS  -> GPIO 15
 *   MCP2515 INT -> GPIO  4
 */

#include <SPI.h>
#include <mcp_can.h>

#define CAN_CS    15
#define CAN_INT    4
#define HSPI_SCK  14
#define HSPI_MISO 12
#define HSPI_MOSI 13

SPIClass hspi(HSPI);
MCP_CAN  CAN(&hspi, CAN_CS);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[BER Receiver DEBUG] Boot");

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
  Serial.println(modeResult == MCP2515_OK ? "  (OK)" : "  (FAIL!)");

  Serial.println("MCP2515 setup complete. Listening...");
}

void loop() {
  // Print received frames as they come in
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    uint32_t id;
    uint8_t  len;
    uint8_t  buf[8];
    if (CAN.readMsgBuf(&id, &len, buf) == CAN_OK) {
      Serial.print("RX 0x"); Serial.print(id & 0x1FFFFFFF, HEX);
      Serial.print("  len="); Serial.print(len);
      Serial.print("  data=");
      for (uint8_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX); Serial.print(' ');
      }
      Serial.println();
    }
  }

  // Every 2 seconds, print MCP2515 state and error counters
  static uint32_t lastReport = 0;
  uint32_t now = millis();
  if (now - lastReport > 2000) {
    lastReport = now;

    byte err   = CAN.getError();        // EFLG register
    byte tec   = CAN.errorCountTX();    // TX error counter
    byte rec   = CAN.errorCountRX();    // RX error counter

    Serial.print("STATUS  EFLG=0x"); Serial.print(err, HEX);
    Serial.print("  TEC=");           Serial.print(tec);
    Serial.print("  REC=");           Serial.print(rec);

    if (err & 0x01) Serial.print(" [RX0 OVR]");
    if (err & 0x02) Serial.print(" [RX1 OVR]");
    if (err & 0x04) Serial.print(" [TX WARN]");
    if (err & 0x08) Serial.print(" [RX WARN]");
    if (err & 0x10) Serial.print(" [TX PASSIVE]");
    if (err & 0x20) Serial.print(" [RX PASSIVE]");
    if (err & 0x40) Serial.print(" [BUS OFF]");
    Serial.println();
  }
}
