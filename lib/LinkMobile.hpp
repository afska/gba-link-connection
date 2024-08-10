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
#include "_link_tonc_mgba.h"

#define LINK_MOBILE_BARRIER asm volatile("" ::: "memory")

#define LINK_MOBILE_RESET_IF_NEEDED \
  if (!isEnabled)                   \
    return false;                   \
  if (state == NEEDS_RESET)         \
    if (!reset())                   \
      return false;

static volatile char LINK_MOBILE_VERSION[] = "LinkMobile/v6.7.0";

#define LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH 255

class LinkMobile {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int FRAME_LINES = 228;
  static constexpr int PING_WAIT = FRAME_LINES * 7;
  static constexpr int CMD_TIMEOUT = 100;
  static constexpr int ADAPTER_WAITING = 0xD2;
  static constexpr int GBA_WAITING = 0x4B;
  static constexpr int OR_VALUE = 0x80;
  static constexpr int LOGIN_STEPS = 8;
  static constexpr int COMMAND_HEADER_VALUE1 = 0x99;
  static constexpr int COMMAND_HEADER_VALUE2 = 0x66;
  static constexpr int DEVICE_GBA = 0x1;
  static constexpr int DEVICE_ADAPTER_BLUE = 0x8;
  static constexpr int DEVICE_ADAPTER_YELLOW = 0x9;
  static constexpr int DEVICE_ADAPTER_GREEN = 0xA;
  static constexpr int DEVICE_ADAPTER_RED = 0xB;
  static constexpr int COMMAND_BEGIN_SESSION = 0x10;
  static constexpr int COMMAND_END_SESSION = 0x11;
  static constexpr int COMMAND_DIAL_TELEPHONE = 0x12;
  static constexpr int COMMAND_HANG_UP_TELEPHONE = 0x13;
  static constexpr int COMMAND_WAIT_FOR_TELEPHONE_CALL = 0x14;
  static constexpr int COMMAND_TRANSFER_DATA = 0x15;
  static constexpr int COMMAND_RESET = 0x16;
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
  static constexpr u8 LOGIN_PARTS[] = {0x4e, 0x49, 0x4e, 0x54,
                                       0x45, 0x4e, 0x44, 0x4f};
  static constexpr int SUPPORTED_DEVICES_SIZE = 4;
  static constexpr u8 SUPPORTED_DEVICES[] = {
      DEVICE_ADAPTER_BLUE, DEVICE_ADAPTER_YELLOW, DEVICE_ADAPTER_GREEN,
      DEVICE_ADAPTER_RED};

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
    u8 magic1 = COMMAND_HEADER_VALUE1;
    u8 magic2 = COMMAND_HEADER_VALUE2;

    bool isValid() {
      return magic1 == COMMAND_HEADER_VALUE1 && magic2 == COMMAND_HEADER_VALUE2;
    }
  } __attribute__((packed));

  struct PacketData {
    u8 bytes[LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH];
  } __attribute__((packed));

  struct PacketHeader {
    u8 commandId;
    u8 _unused_ = 0;
    u8 _unusedDataSizeH_ = 0;  // The Mobile Adapter discards any packets bigger
                               // than 255 bytes, effectively forcing the high
                               // byte of the packet data length to be 0.
    u8 dataSizeL;

    u16 sum() { return commandId + _unused_ + _unusedDataSizeH_ + dataSizeL; }
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
    PacketData packetData;
    PacketChecksum packetChecksum;
    AcknowledgementSignal acknowledgementSignal;
  };

  struct CommandResponse {
    bool success = false;
    Command command;
  };

  static constexpr u32 HEADER_SIZE = sizeof(MagicBytes) + sizeof(PacketHeader);
  static constexpr u32 CHECKSUM_SIZE = sizeof(PacketChecksum);

  LinkSPI* linkSPI = new LinkSPI();
  State state = NEEDS_RESET;
  PacketData nextCommandData;
  u32 nextCommandDataSize = 0;
  volatile bool isSendingSyncCommand = false;
  Error lastError = NONE;
  volatile bool isEnabled = false;

  void addData(u8 value, bool start = false) {
    if (start)
      nextCommandDataSize = 0;
    nextCommandData.bytes[nextCommandDataSize] = value;
    nextCommandDataSize++;
  }

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

    pingAdapter();

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

  void pingAdapter() {
    transfer(0);
    wait(PING_WAIT);
  }

  bool login() {
    for (u32 i = 0; i < LOGIN_STEPS; i++)
      addData(LOGIN_PARTS[i], i == 0);
    auto command = buildCommand(COMMAND_BEGIN_SESSION, true);

    LINK_MOBILE_BARRIER;
    isSendingSyncCommand = true;
    LINK_MOBILE_BARRIER;

    sendCommand(command);

    LINK_MOBILE_BARRIER;
    isSendingSyncCommand = false;
    LINK_MOBILE_BARRIER;

    return false;
  }

  CommandResponse sendCommand(Command command) {
    CommandResponse response;

    mgbalog("----------");
    mgbalog("sending command: %d (%d bytes).", command.packetHeader.commandId,
            command.packetHeader.dataSizeL);

    // COMMAND
    const u8* commandBytes = (const u8*)&command;
    for (u32 i = 0; i < HEADER_SIZE + command.packetHeader.dataSizeL; i++) {
      u8 r;
      if ((r = transfer(commandBytes[i])) != ADAPTER_WAITING) {
        mgbalog("! not waiting: %d", r);
        return response;
      }
    }
    mgbalog("header + data sent");
    commandBytes += HEADER_SIZE + LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH;
    for (u32 i = 0; i < CHECKSUM_SIZE; i++) {
      u8 r;
      if ((r = transfer(commandBytes[i])) != ADAPTER_WAITING) {
        mgbalog("! not waiting: %d (i = %d)", r, i);
        return response;
      }
    }
    mgbalog("checksum sent");
    if (!exchangeACK(0, command.packetHeader.commandId ^ OR_VALUE)) {
      mgbalog("! invalid ack");
      return response;
    }
    mgbalog("COMMAND SENT!");

    // wait for response????
    u8 rr;
    while ((rr = transfer(GBA_WAITING)) == ADAPTER_WAITING) {
      mgbalog("waiting %d", rr);
    }
    mgbalog("AND NOW %d", rr);
    if (rr != COMMAND_HEADER_VALUE1) {
      mgbalog("invalid magic response 1! %d", rr);
      return response;
    }
    rr = transfer(GBA_WAITING);
    if (rr != COMMAND_HEADER_VALUE2) {
      mgbalog("invalid magic response 2! %d", rr);
      return response;
    }

    u8* responseCommandBytes = (u8*)&response.command;
    for (u32 i = 2; i < HEADER_SIZE; i++)
      responseCommandBytes[i] = transfer(GBA_WAITING);
    responseCommandBytes += HEADER_SIZE;
    // TODO: VALIDATE high bytes = 0?
    mgbalog("header received, %d bytes",
            response.command.packetHeader.dataSizeL);
    for (u32 i = 0; i < response.command.packetHeader.dataSizeL; i++)
      responseCommandBytes[i] = transfer(GBA_WAITING);
    responseCommandBytes += LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH;
    mgbalog("bytes received");
    for (u32 i = 0; i < CHECKSUM_SIZE; i++)
      responseCommandBytes[i] = transfer(GBA_WAITING);
    mgbalog("checksum received");

    // VALIDATE CHECKSUM
    u32 rChecksum = response.command.packetHeader.sum();
    for (u32 i = 0; i < response.command.packetHeader.dataSizeL; i++)
      rChecksum += response.command.packetData.bytes[i];
    if (msB16(rChecksum) != response.command.packetChecksum.checksumH) {
      mgbalog("wrong checksum H: expected %d but got %d", msB16(rChecksum),
              response.command.packetChecksum.checksumH);
      return response;
    }
    if (lsB16(rChecksum) != response.command.packetChecksum.checksumL) {
      mgbalog("wrong checksum L: expected %d but got %d", lsB16(rChecksum),
              response.command.packetChecksum.checksumL);
      return response;
    }

    if (!exchangeACK(response.command.packetHeader.commandId ^ OR_VALUE, 0)) {
      mgbalog("! invalid ack response");
      return response;
    }

    mgbalog("I think everything worked? %d",
            response.command.packetHeader.dataSizeL);

    return response;
  }

  bool exchangeACK(u8 localCommandAck, u8 remoteCommandAck) {
    if (!isSupportedAdapter(transfer(DEVICE_GBA | OR_VALUE)))
      return false;
    if (transfer(localCommandAck) != remoteCommandAck)
      return false;

    return true;
  }

  bool isSupportedAdapter(u8 ack) {
    for (u32 i = 0; i < SUPPORTED_DEVICES_SIZE; i++) {
      if ((SUPPORTED_DEVICES[i] | OR_VALUE) == ack)
        return true;
    }

    return false;
  }

  Command buildCommand(u8 type, bool withData = false) {
    Command command;
    command.packetHeader.commandId = type;
    command.packetHeader._unused_ = 0;
    command.packetHeader._unusedDataSizeH_ = 0;
    command.packetHeader.dataSizeL = withData ? (u8)nextCommandDataSize : 0;
    if (withData)
      command.packetData = nextCommandData;
    u16 checksum = command.packetHeader.sum();
    for (u32 i = 0; i < command.packetHeader.dataSizeL; i++)
      checksum += command.packetData.bytes[i];
    command.packetChecksum.checksumH = msB16(checksum);
    command.packetChecksum.checksumL = lsB16(checksum);
    command.acknowledgementSignal.deviceId = DEVICE_GBA | OR_VALUE;
    command.acknowledgementSignal.commandAck = 0;

    return command;
  }

  u32 transfer(u32 data) {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    // mgbalog("SENDING %d", data);
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); });

    return receivedData;
  }

  bool cmdTimeout(u32& lines, u32& vCount) {
    return timeout(CMD_TIMEOUT, lines, vCount);
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
