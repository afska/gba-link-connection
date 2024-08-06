#ifndef LINK_PS2_MOUSE_H
#define LINK_PS2_MOUSE_H

// --------------------------------------------------------------------------
// A PS/2 Mouse Adapter for the GBA.
// (Based on https://github.com/kristopher/PS2-Mouse-Arduino, MIT license)
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkPS2Mouse* linkPS2Mouse = new LinkPS2Mouse();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_TIMER2, NULL);
// - 3) Initialize the library with:
//       linkPS2Mouse->activate();
// - 4) Get a report:
//       int data[3];
//       linkPS2Mouse->report(data);
//       if ((data[0] & LINK_PS2_MOUSE_LEFT_CLICK) != 0)
//         ; // handle LEFT click
//       data[1] // X movement
//       data[2] // Y movement
// --------------------------------------------------------------------------
//  ____________
// |   Pinout   |
// |PS/2 --- GBA|
// |------------|
// |CLOCK -> SI |
// |DATA --> SO |
// |VCC ---> VCC|
// |GND ---> GND|
// --------------------------------------------------------------------------

#include <tonc_bios.h>
#include <tonc_core.h>

#define LINK_PS2_MOUSE_LEFT_CLICK 0b001
#define LINK_PS2_MOUSE_RIGHT_CLICK 0b010
#define LINK_PS2_MOUSE_MIDDLE_CLICK 0b100

#define LINK_PS2_MOUSE_SI_DIRECTION 0b1000000
#define LINK_PS2_MOUSE_SO_DIRECTION 0b10000000
#define LINK_PS2_MOUSE_SI_DATA 0b100
#define LINK_PS2_MOUSE_SO_DATA 0b1000
#define LINK_PS2_MOUSE_TO_TICKS 17

const u16 LINK_PS2_MOUSE_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1, IRQ_TIMER2,
                                      IRQ_TIMER3};

static volatile char LINK_PS2_MOUSE_VERSION[] = "LinkPS2Mouse/v7.0.0";

class LinkPS2Mouse {
 public:
  explicit LinkPS2Mouse(u8 waitTimerId) { this->waitTimerId = waitTimerId; }

  bool isActive() { return isEnabled; }

  void activate() {
    deactivate();

    setClockHigh();
    setDataHigh();
    waitMilliseconds(20);
    write(0xff);            // send reset to the mouse
    readByte();             // read ack byte
    waitMilliseconds(20);   // not sure why this needs the delay
    readByte();             // blank
    readByte();             // blank
    waitMilliseconds(20);   // not sure why this needs the delay
    enableDataReporting();  // tell the mouse to start sending data
    waitMicroseconds(100);

    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;

    REG_RCNT = 0b1000000000000000;  // General Purpose Mode
    REG_SIOCNT = 0;                 // Unused
  }

  void report(int (&data)[3]) {
    write(0xeb);                       // send read data
    readByte();                        // read ack byte
    data[0] = readByte();              // status bit
    data[1] = readMovementX(data[0]);  // X movement packet
    data[2] = readMovementY(data[0]);  // Y movement packet
  }

 private:
  u8 waitTimerId;
  volatile bool isEnabled = false;

  void enableDataReporting() {
    write(0xf4);  // send enable data reporting
    readByte();   // read ack byte
  }

  s16 readMovementX(int status) {
    s16 x = readByte();
    if ((status & (1 << 4)) != 0)
      // negative
      for (int i = 8; i < 16; i++)
        x |= 1 << i;
    return x;
  }

  s16 readMovementY(int status) {
    s16 y = readByte();
    if ((status & (1 << 5)) != 0)
      // negative
      for (int i = 8; i < 16; i++)
        y |= 1 << i;
    return y;
  }

  void write(u8 data) {
    u8 parity = 1;
    setDataHigh();
    setClockHigh();
    waitMicroseconds(300);
    setClockLow();
    waitMicroseconds(300);
    setDataLow();
    waitMicroseconds(10);
    setClockHigh();  // (start bit)
    while (getClock())
      ;  // wait for mouse to take control of clock
    // clock is low, and we are clear to send data
    for (u32 i = 0; i < 8; i++) {
      if (data & 0x01)
        setDataHigh();
      else
        setDataLow();
      // wait for clock cycle
      while (!getClock())
        ;
      while (getClock())
        ;
      parity = parity ^ (data & 0x01);
      data = data >> 1;
    }
    // parity
    if (parity)
      setDataHigh();
    else
      setDataLow();
    while (!getClock())
      ;
    while (getClock())
      ;
    setDataHigh();
    waitMicroseconds(50);
    while (getClock())
      ;
    while (!getClock() || !getData())
      ;             // wait for mouse to switch modes
    setClockLow();  // put a hold on the incoming data.
  }

  u8 readByte() {
    u8 data = 0;
    setClockHigh();
    setDataHigh();
    waitMicroseconds(50);
    while (getClock())
      ;
    while (!getClock())
      ;  // eat start bit
    for (int i = 0; i < 8; i++) {
      data |= readBit() << i;
    }
    readBit();  // parity bit
    readBit();  // stop bit should be 1
    setClockLow();

    return data;
  }

  bool readBit() {
    while (getClock())
      ;
    bool bit = getData();
    while (!getClock())
      ;
    return bit;
  }

  void waitMilliseconds(u16 milliseconds) {
    u16 ticksOf1024Cycles = milliseconds * LINK_PS2_MOUSE_TO_TICKS;
    REG_TM[waitTimerId].start = -ticksOf1024Cycles;
    REG_TM[waitTimerId].cnt = TM_ENABLE | TM_IRQ | TM_FREQ_1024;
    IntrWait(1, LINK_PS2_MOUSE_IRQ_IDS[waitTimerId]);
    REG_TM[waitTimerId].cnt = 0;
  }

  void waitMicroseconds(u16 microseconds) {
    u16 cycles = microseconds * LINK_PS2_MOUSE_TO_TICKS;
    REG_TM[waitTimerId].start = -cycles;
    REG_TM[waitTimerId].cnt = TM_ENABLE | TM_IRQ | TM_FREQ_1;
    IntrWait(1, LINK_PS2_MOUSE_IRQ_IDS[waitTimerId]);
    REG_TM[waitTimerId].cnt = 0;
  }

  bool getClock() {
    REG_RCNT &= ~LINK_PS2_MOUSE_SI_DIRECTION;
    return (REG_RCNT & LINK_PS2_MOUSE_SI_DATA) >> 0;
  }
  bool getData() {
    REG_RCNT &= ~LINK_PS2_MOUSE_SO_DIRECTION;
    return (REG_RCNT & LINK_PS2_MOUSE_SO_DATA) >> 1;
  }

  void setClockHigh() {
    REG_RCNT |= LINK_PS2_MOUSE_SI_DIRECTION;
    REG_RCNT |= LINK_PS2_MOUSE_SI_DATA;
  }

  void setClockLow() {
    REG_RCNT |= LINK_PS2_MOUSE_SI_DIRECTION;
    REG_RCNT &= ~LINK_PS2_MOUSE_SI_DATA;
  }

  void setDataHigh() {
    REG_RCNT |= LINK_PS2_MOUSE_SO_DIRECTION;
    REG_RCNT |= LINK_PS2_MOUSE_SO_DATA;
  }

  void setDataLow() {
    REG_RCNT |= LINK_PS2_MOUSE_SO_DIRECTION;
    REG_RCNT &= ~LINK_PS2_MOUSE_SO_DATA;
  }
};

extern LinkPS2Mouse* linkPS2Mouse;

#endif  // LINK_PS2_MOUSE_H