#ifndef LINK_MOBILE_H
#define LINK_MOBILE_H

// --------------------------------------------------------------------------
// A high level driver for the GB Mobile Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkMobile* linkMobile = new LinkMobile();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_MOBILE_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_MOBILE_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_MOBILE_ISR_TIMER);
// - 3) Initialize the library with:
//       linkMobile->activate();
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#include "_link_common.hpp"

#include <cstring>
#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

// TODO: Remove
#include <functional>
#include <string>

#define LINK_MOBILE_BARRIER asm volatile("" ::: "memory")

#define LINK_MOBILE_RESET_IF_NEEDED \
  if (!isEnabled)                   \
    return false;                   \
  if (state == NEEDS_RESET)         \
    if (!reset())                   \
      return false;

static volatile char LINK_MOBILE_VERSION[] = "LinkMobile/v6.7.0";

void LINK_MOBILE_ISR_VBLANK();
void LINK_MOBILE_ISR_SERIAL();
void LINK_MOBILE_ISR_TIMER();

class LinkMobile {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int COMMAND_BEGIN_SESSION = 0x10;
  static constexpr int COMMAND_END_SESSION = 0x11;
  static constexpr int COMMAND_DIAL_TELEPHONE = 0x12;
  static constexpr int COMMAND_HANG_UP_TELEPHONE = 0x13;
  static constexpr int COMMAND_WAIT_FOR_TELEPHONE_CALL = 0x14;
  static constexpr int COMMAND_TRANSFER_DATA = 0x15;
  static constexpr int COMMAND_TELEPHONE_STATUS = 0x17;
  static constexpr int COMMAND_SIO32 = 0x18;
  static constexpr int COMMAND_READ_CONFIGURATION_DATA = 0x19;
  static constexpr int COMMAND_ISP_LOGIN = 0x21;
  static constexpr int COMMAND_ISP_LOGOUT = 0x22;
  static constexpr int COMMAND_OPEN_TCP_CONNECTION = 0x23;
  static constexpr int COMMAND_CLOSE_TCP_CONNECTION = 0x24;
  static constexpr int COMMAND_OPEN_UDP_CONNECTION = 0x25;
  static constexpr int COMMAND_CLOSE_UDP_CONNECTION = 0x26;
  static constexpr int COMMAND_DNS_QUERY = 0x28;
  static constexpr int COMMAND_ERROR_STATUS = 0x6E;

 public:
  std::function<void(std::string str)> debug;  // TODO: REMOVE

  enum State {
    NEEDS_RESET,
    AUTHENTICATED,
    SEARCHING,
    SERVING,
    CONNECTING,
    CONNECTED
  };

  enum Error {
    // User errors
    NONE = 0,
    WRONG_STATE = 1,
    // Communication errors
    // ...
  };

  explicit LinkMobile() {
    // TODO: Fill config
  }

  [[nodiscard]] bool isActive() { return isEnabled; }

  bool activate() {
    lastError = NONE;
    isEnabled = false;

    LINK_MOBILE_BARRIER;
    bool success = reset();
    LINK_MOBILE_BARRIER;

    isEnabled = true;
    return success;
  }

  bool deactivate() {
    activate();
    // bool success = sendCommand(LINK_MOBILE_COMMAND_BYE).success;

    lastError = NONE;
    isEnabled = false;
    resetState();
    stop();

    return true;
  }

  [[nodiscard]] State getState() { return state; }
  Error getLastError(bool clear = true) {
    Error error = lastError;
    if (clear)
      lastError = NONE;
    return error;
  }

  ~LinkMobile() { delete linkSPI; }

  void _onVBlank() {
    if (!isEnabled)
      return;
  }

  void _onSerial() {
    if (!isEnabled)
      return;
  }

  void _onTimer() {
    if (!isEnabled)
      return;
  }

  struct Config {
    // TODO: Define
  };

  Config config;

 private:
  struct MagicBytes {
    u8 magic1 = 0x99;
    u8 magic2 = 0x66;
  } __attribute__((packed));

  struct PacketHeader {
    u8 commandId;
    u8 _unused_;
    u8 dataSizeH;  // The Mobile Adapter discards any packets bigger than 255
                   // bytes, effectively forcing the high byte of the packet
                   // data length to be 0.
    u8 dataSizeL;
  } __attribute__((packed));

  struct PacketChecksum {
    // The Packet Checksum is simply the 16-bit sum of all previous header bytes
    // and all previous packet data bytes. It does not include the magic bytes.
    // The checksum is transmitted big-endian.
    u8 checksumH;
    u8 checksumL;
  } __attribute__((packed));

  struct AcknowledgementSignal {
    u8 deviceId;    // The first byte is the Device ID OR'ed with the value 0x80
    u8 commandAck;  // The second byte is 0x00 for the sender. The receiver
                    // transfers the Command ID from the Packet Header XOR'ed by
                    // 0x80.

    /* Device ID | OR Value | Device Type
    0x01		| 0x81		| Game Boy Advance
    0x08		| 0x88		| PDC Mobile Adapter (Blue)
    0x09		| 0x89		| cdmaOne Mobile Adapter (Yellow)
    0x0A		| 0x8A		| PHS Mobile Adapter (Green)
    0x0B		| 0x8B		| DDI Mobile Adapter (Red)
    */
  } __attribute__((packed));

  struct Command {
    MagicBytes magicBytes;
    PacketHeader packetHeader;
    u8 data[255];
    PacketChecksum packetChecksum;
    AcknowledgementSignal acknowledgementSignal;
  };

  LinkSPI* linkSPI = new LinkSPI();
  State state = NEEDS_RESET;
  Error lastError = NONE;
  volatile bool isEnabled = false;

  void copyName(char* target, const char* source, u32 length) {
    u32 len = std::strlen(source);

    for (u32 i = 0; i < length + 1; i++)
      if (i < len)
        target[i] = source[i];
      else
        target[i] = '\0';
  }

  bool reset() {
    resetState();
    stop();
    return start();
  }

  void resetState() { this->state = NEEDS_RESET; }

  void stop() {
    stopTimer();
    linkSPI->deactivate();
  }

  bool start() {
    startTimer();

    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                      LinkSPI::DataSize::SIZE_8BIT);

    if (!login())
      return false;

    // TODO: SWITCH TO 32BITS?

    state = AUTHENTICATED;

    return true;
  }

  void stopTimer() {
    // Link::_REG_TM[config.sendTimerId].cnt =
    //     Link::_REG_TM[config.sendTimerId].cnt & (~Link::_TM_ENABLE);
  }

  void startTimer() {
    // Link::_REG_TM[config.sendTimerId].start = -config.interval;
    // Link::_REG_TM[config.sendTimerId].cnt =
    //     Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
  }

  bool login() {
    // TODO: IMPLEMENT
    return false;
  }

  bool cmdTimeout(u32& lines, u32& vCount) {
    return timeout(100, lines, vCount);
  }

  bool timeout(u32 limit, u32& lines, u32& vCount) {
    if (Link::_REG_VCOUNT != vCount) {
      lines += Link::_max((int)Link::_REG_VCOUNT - (int)vCount, 0);
      vCount = Link::_REG_VCOUNT;
    }

    return lines > limit;
  }

  void wait(u32 verticalLines) {
    u32 count = 0;
    u32 vCount = Link::_REG_VCOUNT;

    while (count < verticalLines) {
      if (Link::_REG_VCOUNT != vCount) {
        count++;
        vCount = Link::_REG_VCOUNT;
      }
    };
  }

  template <typename F>
  void waitVBlanks(u32 vBlanks, F onVBlank) {
    u32 count = 0;
    u32 vCount = Link::_REG_VCOUNT;

    while (count < vBlanks) {
      if (Link::_REG_VCOUNT != vCount) {
        vCount = Link::_REG_VCOUNT;

        if (vCount == 160) {
          onVBlank();
          count++;
        }
      }
    };
  }

  u32 buildU32(u16 msB, u16 lsB) { return (msB << 16) | lsB; }
  u16 buildU16(u8 msB, u8 lsB) { return (msB << 8) | lsB; }
  u16 msB32(u32 value) { return value >> 16; }
  u16 lsB32(u32 value) { return value & 0xffff; }
  u8 msB16(u16 value) { return value >> 8; }
  u8 lsB16(u16 value) { return value & 0xff; }
};

extern LinkMobile* linkMobile;

inline void LINK_MOBILE_ISR_VBLANK() {
  linkMobile->_onVBlank();
}

inline void LINK_MOBILE_ISR_SERIAL() {
  linkMobile->_onSerial();
}

inline void LINK_MOBILE_ISR_TIMER() {
  linkMobile->_onTimer();
}

#endif  // LINK_MOBILE_H
