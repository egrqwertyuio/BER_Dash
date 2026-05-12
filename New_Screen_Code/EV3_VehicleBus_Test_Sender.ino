/*
 * EV3 Vehicle Bus - Test Sender
 *
 * Sends simulated EV3_Vehicle_Bus.dbc frames so the dash display can be
 * verified without the real vehicle bus.
 *
 * MCP2515 wiring (HSPI):
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
MCP_CAN CAN(&hspi, CAN_CS);

uint32_t lastSendMs = 0;
uint16_t tick = 0;

void putU16(uint8_t *buf, uint8_t byteIndex, uint16_t value) {
  buf[byteIndex] = (uint8_t)(value & 0xFF);
  buf[byteIndex + 1] = (uint8_t)(value >> 8);
}

void putS16(uint8_t *buf, uint8_t byteIndex, int16_t value) {
  putU16(buf, byteIndex, (uint16_t)value);
}

void setBit(uint8_t *buf, uint8_t bit, bool value) {
  uint8_t byteIndex = bit / 8;
  uint8_t mask = 1 << (bit % 8);
  if (value) buf[byteIndex] |= mask;
  else buf[byteIndex] &= ~mask;
}

uint8_t encodeBmsSoc(float pct) {
  pct = constrain(pct, 0.0f, 100.0f);
  return (uint8_t)(pct / 0.392156863f + 0.5f);
}

uint8_t encodeBmsCurrent(float amps) {
  amps = constrain(amps, 0.0f, 200.0f);
  return (uint8_t)(amps / 0.78125f + 0.5f);
}

uint8_t encodeCellVolt(float volts) {
  volts = constrain(volts, 0.0f, 5.0f);
  return (uint8_t)(volts / 0.019607843f + 0.5f);
}

uint8_t encodeCellTemp(float tempC) {
  tempC = constrain(tempC, 0.0f, 150.0f);
  return (uint8_t)(tempC / 0.588235294f + 0.5f);
}

void printByteHex(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

void sendFrame(uint16_t id, uint8_t *buf) {
  byte result = CAN.sendMsgBuf(id, 0, 8, buf);

  Serial.print("TX 0x");
  Serial.print(id, HEX);
  Serial.print(" data=");
  for (uint8_t i = 0; i < 8; i++) {
    printByteHex(buf[i]);
    Serial.print(' ');
  }
  Serial.print("-> ");
  Serial.println(result == CAN_OK ? "OK" : "FAIL");
}

void sendVehicleBusFrames() {
  uint8_t buf[8] = {0};
  uint8_t phase = tick % 100;

  bool demoFault = (tick % 80) >= 65;
  bool r2d = (tick % 40) >= 8;
  bool precharge = (tick % 50) >= 5;
  bool regen = (tick % 60) >= 30;
  uint8_t driveMode = (tick / 25) % 6;
  uint8_t appsPct = phase;
  int16_t torqueCmd = (int16_t)((int)appsPct * 4 - (regen ? 60 : 0));
  uint16_t powerKw = (uint16_t)((appsPct * 80UL) / 100UL);
  uint16_t lvAdc = 124;  // Displayed as 12.4 V by receiver.
  uint8_t battSoc = 92 - (tick % 50);

  // 0x2 ECU_FAULTS
  memset(buf, 0, 8);
  setBit(buf, 4, demoFault); // OVER_POWER
  setBit(buf, 7, demoFault); // ANY_FAULT
  sendFrame(0x2, buf);

  // 0x3 MC_FAULTS
  memset(buf, 0, 8);
  setBit(buf, 2, demoFault);  // Fault34_OvrVolt
  setBit(buf, 25, demoFault); // ANY_MC_FAULT
  sendFrame(0x3, buf);

  // 0x4 APPS_Info
  memset(buf, 0, 8);
  putU16(buf, 0, 800 + appsPct * 10);
  putU16(buf, 2, 820 + appsPct * 10);
  buf[4] = appsPct;
  putS16(buf, 5, torqueCmd);
  sendFrame(0x4, buf);

  // 0x5 Sensors_Info
  memset(buf, 0, 8);
  putU16(buf, 0, 300 + appsPct * 4);
  buf[2] = 34 + (tick % 8);
  buf[3] = 36 + (tick % 7);
  buf[4] = 38 + (tick % 6);
  setBit(buf, 40, r2d);
  setBit(buf, 41, (tick % 20) < 5);
  setBit(buf, 42, (tick % 30) < 8);
  setBit(buf, 43, appsPct < 8);
  sendFrame(0x5, buf);

  // 0x6 Internal_States
  memset(buf, 0, 8);
  putU16(buf, 0, powerKw);
  putU16(buf, 2, lvAdc);
  buf[4] = battSoc;
  setBit(buf, 40, true);       // InitFinished
  setBit(buf, 41, precharge);  // PrechargeComplete
  setBit(buf, 42, r2d);        // R2D_Active
  buf[5] |= (driveMode & 0x07) << 3; // Drive_Mode bits 43..45
  setBit(buf, 46, regen);      // Regen_Enabled
  sendFrame(0x6, buf);

  // 0x7 BMS_Info
  memset(buf, 0, 8);
  buf[0] = encodeBmsSoc((float)battSoc);
  buf[1] = encodeBmsCurrent(20.0f + appsPct * 1.2f);
  buf[2] = encodeCellVolt(4.08f);
  buf[3] = encodeCellTemp(36.0f + (tick % 12));
  buf[4] = encodeCellVolt(3.72f);
  buf[5] = encodeCellTemp(30.0f + (tick % 8));
  buf[6] = 80 - (tick % 25);
  sendFrame(0x7, buf);

  tick++;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[EV3 Vehicle Bus Test Sender] Boot");

  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT);

  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("MCP2515 init failed, retrying...");
    delay(500);
  }

  CAN.setMode(MCP_NORMAL);
  Serial.println("MCP2515 ready @ 500 kbps");
}

void loop() {
  uint32_t now = millis();
  if (now - lastSendMs >= 250) {
    lastSendMs = now;
    sendVehicleBusFrames();
  }
}
