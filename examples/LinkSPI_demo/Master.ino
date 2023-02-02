#include <SPI.h>

// PIN 11  - YELLOW - SI  / MOSI
// PIN 12  - RED    - SO  / MISO
// PIN 13  - WHITE  - SC  / CLK
// PIN GND - BLUE   - GND / GND

void setup() {
  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(128);  // (16Mhz / 128 = 128Khz)
  SPI.begin();
  Serial.begin(9600);
}

uint32_t transfer(uint32_t value) {
  uint8_t a = (value >> 24) & 0xff;
  uint8_t b = (value >> 16) & 0xff;
  uint8_t c = (value >> 8) & 0xff;
  uint8_t d = (value >> 0) & 0xff;

  a = SPI.transfer(a);
  b = SPI.transfer(b);
  c = SPI.transfer(c);
  d = SPI.transfer(d);

  return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
         ((uint32_t)d << 0);
}

void loop() {
  uint32_t received = transfer(0xaabbccdd);  // (2864434397)

  Serial.println(received);

  delay(100);
}
