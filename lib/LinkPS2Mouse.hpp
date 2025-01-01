#ifndef LINK_PS2_MOUSE_H
#define LINK_PS2_MOUSE_H

// --------------------------------------------------------------------------
// A PS/2 Mouse Adapter for the GBA.
// (Based on https://github.com/kristopher/PS2-Mouse-Arduino, MIT license)
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkPS2Mouse* linkPS2Mouse = new LinkPS2Mouse();
// - 2) Add the required interrupt service routines:
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
// considerations:
// - `activate()` or `report(...)` could freeze the system if not connected!
// - detecting timeouts using interrupts is the user's responsibility!
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

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

static volatile char LINK_PS2_MOUSE_VERSION[] = "LinkPS2Mouse/v7.1.0";

#define LINK_PS2_MOUSE_LEFT_CLICK 0b001
#define LINK_PS2_MOUSE_RIGHT_CLICK 0b010
#define LINK_PS2_MOUSE_MIDDLE_CLICK 0b100

/**
 * @brief A PS/2 Mouse Adapter for the GBA.
 */
class LinkPS2Mouse {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using s16 = signed short;

  static constexpr int RCNT_GPIO = 0b1000000000000000;
  static constexpr int SI_DIRECTION = 0b1000000;
  static constexpr int SO_DIRECTION = 0b10000000;
  static constexpr int SI_DATA = 0b100;
  static constexpr int SO_DATA = 0b1000;
  static constexpr int TO_TICKS = 17;

  LinkPS2Mouse() = delete;

 public:
  /**
   * @brief Constructs a new LinkPS2Mouse object.
   * @param waitTimerId `(0~3)` GBA Timer used for delays.
   */
  explicit LinkPS2Mouse(u8 waitTimerId) { this->waitTimerId = waitTimerId; }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   * \warning Could freeze the system if nothing is connected!
   * \warning Detect timeouts using timer interrupts!
   */
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

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;

    Link::_REG_RCNT = RCNT_GPIO;
    Link::_REG_SIOCNT = 0;
  }

  /**
   * @brief Fills the `data` int array with a report. The first int contains
   * *clicks* that you can check against the bitmasks
   * `LINK_PS2_MOUSE_LEFT_CLICK`, `LINK_PS2_MOUSE_MIDDLE_CLICK`, and
   * `LINK_PS2_MOUSE_RIGHT_CLICK`. The second int is the *X movement*, and the
   * third int is the *Y movement*.
   * @param data The array to be filled with data.
   */
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
      for (u32 i = 8; i < 16; i++)
        x |= 1 << i;
    return x;
  }

  s16 readMovementY(int status) {
    s16 y = readByte();
    if ((status & (1 << 5)) != 0)
      // negative
      for (u32 i = 8; i < 16; i++)
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
    for (u32 i = 0; i < 8; i++) {
      data |= readBit() << i;
    }
    readBit();  // parity bit
    readBit();  // stop bit should be 1
    setClockLow();

    return data;
  }

  volatile bool readBit() {
    while (getClock())
      ;
    volatile bool bit = getData();
    while (!getClock())
      ;
    return bit;
  }

  void waitMilliseconds(u16 milliseconds) {
    u16 ticksOf1024Cycles = milliseconds * TO_TICKS;
    Link::_REG_TM[waitTimerId].start = -ticksOf1024Cycles;
    Link::_REG_TM[waitTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | Link::_TM_FREQ_1024;
    Link::_IntrWait(1, Link::_TIMER_IRQ_IDS[waitTimerId]);
    Link::_REG_TM[waitTimerId].cnt = 0;
  }

  void waitMicroseconds(u16 microseconds) {
    u16 cycles = microseconds * TO_TICKS;
    Link::_REG_TM[waitTimerId].start = -cycles;
    Link::_REG_TM[waitTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | Link::_TM_FREQ_1;
    Link::_IntrWait(1, Link::_TIMER_IRQ_IDS[waitTimerId]);
    Link::_REG_TM[waitTimerId].cnt = 0;
  }

  volatile bool getClock() {
    Link::_REG_RCNT &= ~SI_DIRECTION;
    return (Link::_REG_RCNT & SI_DATA) >> 0;
  }
  volatile bool getData() {
    Link::_REG_RCNT &= ~SO_DIRECTION;
    return (Link::_REG_RCNT & SO_DATA) >> 1;
  }

  void setClockHigh() {
    Link::_REG_RCNT |= SI_DIRECTION;
    Link::_REG_RCNT |= SI_DATA;
  }

  void setClockLow() {
    Link::_REG_RCNT |= SI_DIRECTION;
    Link::_REG_RCNT &= ~SI_DATA;
  }

  void setDataHigh() {
    Link::_REG_RCNT |= SO_DIRECTION;
    Link::_REG_RCNT |= SO_DATA;
  }

  void setDataLow() {
    Link::_REG_RCNT |= SO_DIRECTION;
    Link::_REG_RCNT &= ~SO_DATA;
  }
};

extern LinkPS2Mouse* linkPS2Mouse;

#endif  // LINK_PS2_MOUSE_H
