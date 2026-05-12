/*
 * BER — CAN Sender DEBUG version
 * Prints the result of every sendMsgBuf() call.
 * Strips the encoder so we just blast one frame per second
 * and can see whether TX actually succeeds.
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
  Serial.println("[BER Sender DEBUG] Boot");

  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT);

  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("MCP2515 ready @ 500 kbps");
  Serial.println("Sending one test frame per second...");
}

void loop() {
  static uint8_t counter = 0;
  uint8_t buf[2] = { counter, 0xAA };

  byte result = CAN.sendMsgBuf(0x100, 0, 2, buf);

  Serial.print("TX 0x100 data=");
  Serial.print(counter, HEX); Serial.print(" AA  -> ");

  switch (result) {
    case CAN_OK:               Serial.println("OK"); break;
    case CAN_FAILTX:           Serial.println("FAIL: CAN_FAILTX (no ACK from bus / no other node)"); break;
    case CAN_GETTXBFTIMEOUT:   Serial.println("FAIL: GETTXBFTIMEOUT"); break;
    case CAN_SENDMSGTIMEOUT:   Serial.println("FAIL: SENDMSGTIMEOUT"); break;
    default:
      Serial.print("FAIL: result code ");
      Serial.println(result);
      break;
  }

  counter++;
  delay(1000);
}
