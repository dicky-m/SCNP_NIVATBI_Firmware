#include "ads1256.h"

// int32_t initial;
void ADS_setup_pins(void)
{
    pinMode(ADS_CS_PIN, OUTPUT);
    pinMode(ADS_RDY_PIN, INPUT);
    pinMode(ADS_RST_PIN, OUTPUT);
}

void initADS() {
  attachInterrupt(ADS_RDY_PIN, DRDY_Interuppt, FALLING);
  digitalWrite(ADS_RST_PIN, LOW);  delay(1); 
  digitalWrite(ADS_RST_PIN, HIGH); delay(100);
  Reset(); delay(200); 
  SetRegisterValue(STATUS,B00110010);  //buffer diaktifkan kembali
  SetRegisterValue(ADCON, PGA_1);
  SetRegisterValue(DRATE, DR_30000);
}

int32_t read_ADC(byte ch) {
  int32_t x = 0;
  waitforDRDY();
//  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
//  digitalWrite(ADS_CS_PIN, LOW);
  SPI.transfer(WREG | MUX);
  SPI.transfer(0x00);
  SPI.transfer(ch);     delayMicroseconds(2);
  SPI.transfer(SYNC);   delayMicroseconds(5);
  SPI.transfer(WAKEUP); delayMicroseconds(1);
  SPI.transfer(RDATA);  delayMicroseconds(7);
  x |= SPI.transfer(NOPT); x <<= 8;
  x |= SPI.transfer(NOPT); x <<= 8;
  x |= SPI.transfer(NOPT);
  if (x > 0x7fffff) x -= 0x1000000;
  delayMicroseconds(1);
  return x;
}

volatile int DRDY_state = HIGH;

void waitforDRDY() {
  while (DRDY_state) {
    continue;
  }
  noInterrupts();
  DRDY_state = HIGH;
  interrupts();
}

void DRDY_Interuppt() {
  DRDY_state = LOW;
}

long GetRegisterValue(uint8_t regAdress) {
  uint8_t bufr;
  digitalWrite(ADS_CS_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(RREG | regAdress);
  SPI.transfer(0x00);
  delayMicroseconds(10);
  bufr = SPI.transfer(NOPT);
  delayMicroseconds(10);
  digitalWrite(ADS_CS_PIN, HIGH);
  SPI.endTransaction();
  return bufr;
}

void SendCMD(uint8_t cmd) {
  waitforDRDY();
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(ADS_CS_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(cmd);
  delayMicroseconds(10);
  digitalWrite(ADS_CS_PIN, HIGH);
  SPI.endTransaction();
}

void Reset() {
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(ADS_CS_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(RESET);
  delay(2);
  SPI.transfer(SDATAC);
  delayMicroseconds(100);
  digitalWrite(ADS_CS_PIN, HIGH);
  SPI.endTransaction();
}

void SetRegisterValue(uint8_t regAdress, uint8_t regValue) {
  uint8_t regValuePre = GetRegisterValue(regAdress);
  if (regValue != regValuePre) {
    delayMicroseconds(10);
    waitforDRDY();
    SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
    digitalWrite(ADS_CS_PIN, LOW);
    delayMicroseconds(10);
    SPI.transfer(WREG | regAdress);
    SPI.transfer(0x00);
    SPI.transfer(regValue);
    delayMicroseconds(10);
    digitalWrite(ADS_CS_PIN, HIGH);
    SPI.endTransaction();
  }
}
