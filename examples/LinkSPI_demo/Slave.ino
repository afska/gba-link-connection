#include <SPI.h>

// PIN 11  - RED    - SO  / MOSI
// PIN 12  - YELLOW - SI  / MISO
// PIN 13  - WHITE  - SC  / CLK
// PIN GND - BLUE   - GND / GND

void setup() {
  pinMode(MISO, OUTPUT);
  SPI.setDataMode(SPI_MODE3);
  SPCR |= _BV(SPE);

  Serial.begin(9600);
}

uint8_t transferByte(uint8_t value) {
  SPDR = value;
  while (!(SPSR & _BV(SPIF)))
    ;
  return SPDR;
}

uint32_t transfer(uint32_t value) {
  uint8_t a = (value >> 24) & 0xFF;
  uint8_t b = (value >> 16) & 0xFF;
  uint8_t c = (value >> 8) & 0xFF;
  uint8_t d = (value >> 0) & 0xFF;

  a = transferByte(a);
  b = transferByte(b);
  c = transferByte(c);
  d = transferByte(d);

  return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
         ((uint32_t)d << 0);
}

void loop() {
  uint32_t received = transfer(0xAABBCCDD);  // (2864434397)

  Serial.println(received);
}
