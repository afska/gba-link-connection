#ifndef LINK_MOBILE_H
#define LINK_MOBILE_H

// --------------------------------------------------------------------------
// A high level driver for the Mobile Adapter GB.
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

static volatile char LINK_MOBILE_VERSION[] = "LinkMobile/v7.0.0";

#define LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH 255
#define LINK_MOBILE_DEFAULT_TIMER_ID 3

class LinkMobile {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int FRAME_LINES = 228;
  static constexpr int PING_WAIT = FRAME_LINES * 7;
  static constexpr int CMD_TIMEOUT = FRAME_LINES * 5;
  static constexpr int ADAPTER_WAITING = 0xD2;
  static constexpr int GBA_WAITING = 0x4B;
  static constexpr int OR_VALUE = 0x80;
  static constexpr int COMMAND_MAGIC_VALUE1 = 0x99;
  static constexpr int COMMAND_MAGIC_VALUE2 = 0x66;
  static constexpr int DEVICE_GBA = 0x1;
  static constexpr int DEVICE_ADAPTER_BLUE = 0x8;
  static constexpr int DEVICE_ADAPTER_YELLOW = 0x9;
  static constexpr int DEVICE_ADAPTER_GREEN = 0xA;
  static constexpr int DEVICE_ADAPTER_RED = 0xB;
  static constexpr int CONFIGURATION_DATA_SIZE = 192;
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
  static constexpr int COMMAND_ERROR_STATUS = 0x6E | OR_VALUE;

  static constexpr int LOGIN_PARTS_SIZE = 8;
  static constexpr u8 LOGIN_PARTS[] = {0x4e, 0x49, 0x4e, 0x54,
                                       0x45, 0x4e, 0x44, 0x4f};
  static constexpr u8 WAIT_TICKS[] = {4, 8};

  static constexpr int SUPPORTED_DEVICES_SIZE = 4;
  static constexpr u8 SUPPORTED_DEVICES[] = {
      DEVICE_ADAPTER_BLUE, DEVICE_ADAPTER_YELLOW, DEVICE_ADAPTER_GREEN,
      DEVICE_ADAPTER_RED};

 public:
  std::function<void(std::string str)> debug;  // TODO: REMOVE

  enum State {
    NEEDS_RESET,
    AUTHENTICATING,
    SESSION_ACTIVE,
  };

  enum Error {
    // User errors
    NONE = 0,
    WRONG_STATE = 1,
    // Communication errors
    // ...
  };

  struct ConfigurationData {
    char magic[2];
    bool isRegistering;
    u8 _unused1_;
    u8 primaryDNS[4];
    u8 secondaryDNS[4];
    char loginID[10];
    u8 _unused2_[22];
    char email[24];
    u8 _unused3_[6];
    char smtpServer[20];
    char popServer[19];
    u8 _unused4_[5];
    u8 configurationSlot1[24];
    u8 configurationSlot2[24];
    u8 configurationSlot3[24];
    u8 checksumHigh;
    u8 checksumLow;
  } __attribute__((packed));

  explicit LinkMobile(u8 timerId = LINK_MOBILE_DEFAULT_TIMER_ID) {
    this->config.timerId = timerId;
  }

  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief ...
   *
   * \warning Blocks the system for ~15 frames!
   */
  bool activate() {
    lastError = NONE;
    isEnabled = false;

    LINK_MOBILE_BARRIER;
    bool success = reset();
    LINK_MOBILE_BARRIER;

    if (!success) {
      deactivate(false);
      return false;
    }

    isEnabled = true;
    return true;
  }

  bool deactivate(bool logoutFirst = true) {
    bool success = true;

    if (logoutFirst) {
      activate();
      success = logout();
    }

    lastError = NONE;
    isEnabled = false;
    resetState();
    stop();

    return success;
  }

  bool readConfiguration(ConfigurationData& configurationData) {
    static constexpr u8 CONFIGURATION_DATA_CHUNK = CONFIGURATION_DATA_SIZE / 2;
    u8* configurationDataBytes = (u8*)&configurationData;

    addData(0, true);
    addData(CONFIGURATION_DATA_CHUNK);
    auto response1 = sendCommandWithResponse(
        buildCommand(COMMAND_READ_CONFIGURATION_DATA, true));
    if (response1.result != CommandResult::SUCCESS ||
        response1.command.header.size != CONFIGURATION_DATA_CHUNK + 1)
      return false;

    addData(CONFIGURATION_DATA_CHUNK, true);
    addData(CONFIGURATION_DATA_CHUNK);
    auto response2 = sendCommandWithResponse(
        buildCommand(COMMAND_READ_CONFIGURATION_DATA, true));
    if (response2.result != CommandResult::SUCCESS ||
        response2.command.header.size != CONFIGURATION_DATA_CHUNK + 1)
      return false;

    u8* response1Bytes = (u8*)&response1.command.data.bytes;
    u8* response2Bytes = (u8*)&response2.command.data.bytes;
    for (u32 i = 0; i < CONFIGURATION_DATA_CHUNK; i++)
      configurationDataBytes[i] = response1Bytes[1 + i];
    for (u32 i = 0; i < CONFIGURATION_DATA_CHUNK; i++)
      configurationDataBytes[CONFIGURATION_DATA_CHUNK + i] =
          response2Bytes[1 + i];

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

    linkSPI->_onSerial();
    if (linkSPI->getAsyncState() != LinkSPI::AsyncState::READY) {
      mgbalog("NOT READY");
      return;
    }
    u32 newData = linkSPI->getAsyncData();

    if (state == NEEDS_RESET)
      return;

    if (asyncCommand.isActive) {
      if (asyncCommand.state == AsyncCommand::State::PENDING) {
        if (isSIO32Mode()) {
          // TODO: IMPLEMENT
          mgbalog("sio32");
        } else {
          if (asyncCommand.direction == AsyncCommand::Direction::SENDING)
            sendAsyncCommandSIO8(newData);
          else
            receiveAsyncCommandSIO8(newData);
        }

        if (asyncCommand.state == AsyncCommand::State::COMPLETED)
          processAsyncCommand();
      }
    }
  }

  void _onTimer() {
    if (!isEnabled || !asyncCommand.isActive || !asyncCommand.isWaiting)
      return;

    linkSPI->transferAsync(asyncCommand.pendingData);
    stopTimer();
    asyncCommand.isWaiting = false;
  }

  struct Config {
    u32 timerId;
  };

  Config config;

 private:
  struct MagicBytes {
    u8 magic1 = COMMAND_MAGIC_VALUE1;
    u8 magic2 = COMMAND_MAGIC_VALUE2;
  } __attribute__((packed));

  struct PacketData {
    u8 bytes[LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH];
  } __attribute__((packed));

  struct PacketHeader {
    u8 commandId;
    u8 _unused_ = 0;
    u8 _unusedSizeHigh_ = 0;
    u8 size;

    u16 sum() { return commandId + _unused_ + _unusedSizeHigh_ + size; }
  } __attribute__((packed));

  struct PacketChecksum {
    u8 high;
    u8 low;
  } __attribute__((packed));

  struct Command {
    MagicBytes magicBytes;
    PacketHeader header;
    PacketData data;
    PacketChecksum checksum;
  };

  enum CommandResult {
    PENDING,
    SUCCESS,
    NOT_WAITING,
    INVALID_DEVICE_ID,
    INVALID_COMMAND_ACK,
    INVALID_MAGIC_BYTES,
    WEIRD_DATA_SIZE,
    WRONG_CHECKSUM,
    ERROR,
    UNEXPECTED_RESPONSE,
    TIMEOUT
  };

  struct CommandResponse {
    CommandResult result = CommandResult::PENDING;
    Command command;
  };

  struct AsyncCommand {
    enum State { PENDING, COMPLETED };
    enum Direction { SENDING, RECEIVING };

    volatile State state;
    volatile CommandResult result;
    volatile u32 transferred;  // (in SIO32: words; in SIO8: bytes)
    Command cmd;
    volatile Direction direction;
    volatile u16 expectedChecksum;
    volatile u32 pendingData;
    volatile bool isWaiting;
    volatile bool isActive = false;

    void reset() {
      state = AsyncCommand::State::PENDING;
      result = CommandResult::PENDING;
      transferred = 0;
      cmd = Command{};
      direction = AsyncCommand::Direction::SENDING;
      expectedChecksum = 0;
      pendingData = 0;
      isWaiting = false;
      isActive = true;
    }

    void fail(CommandResult result) {
      result = CommandResult::NOT_WAITING;
      state = AsyncCommand::State::COMPLETED;
    }
  };

  static constexpr u32 PREAMBLE_SIZE =
      sizeof(MagicBytes) + sizeof(PacketHeader);
  static constexpr u32 CHECKSUM_SIZE = sizeof(PacketChecksum);

  AsyncCommand asyncCommand;
  LinkSPI* linkSPI = new LinkSPI();
  State state = NEEDS_RESET;
  PacketData nextCommandData;
  u32 nextCommandDataSize = 0;
  Error lastError = NONE;
  volatile bool isEnabled = false;

  void processAsyncCommand() {
    asyncCommand.isActive = false;
    // TODO: IMPLEMENT
    mgbalog("PROCESSED! %d", asyncCommand.result);
  }

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

  void resetState() {
    this->asyncCommand.isActive = false;
    this->state = NEEDS_RESET;
  }

  void stop() {
    stopTimer();
    linkSPI->deactivate();
  }

  bool start() {
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                      LinkSPI::DataSize::SIZE_8BIT);

    pingAdapter();

    LINK_MOBILE_BARRIER;
    state = AUTHENTICATING;
    isEnabled = true;
    LINK_MOBILE_BARRIER;

    logout();

    if (!login())
      return false;

    // TODO: SWITCH TO 32BITS?

    state = SESSION_ACTIVE;

    return true;
  }

  void stopTimer() {
    Link::_REG_TM[config.timerId].cnt =
        Link::_REG_TM[config.timerId].cnt & (~Link::_TM_ENABLE);
  }

  void startTimer(u16 interval) {
    Link::_REG_TM[config.timerId].start = -interval;
    Link::_REG_TM[config.timerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
  }

  void pingAdapter() {
    transfer(0);
    wait(PING_WAIT);
  }

  bool login() {
    for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++)
      addData(LOGIN_PARTS[i], i == 0);
    auto command = buildCommand(COMMAND_BEGIN_SESSION, true);

    auto response = sendCommandWithResponse(command);
    if (response.result != CommandResult::SUCCESS)
      return false;

    if (response.command.header.size != LOGIN_PARTS_SIZE)
      return false;
    for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++) {
      if (response.command.data.bytes[i] != LOGIN_PARTS[i])
        return false;
    }

    return true;
  }

  bool logout() {
    auto command = buildCommand(COMMAND_END_SESSION, true);
    auto response = sendCommandWithResponse(command);
    return response.result == CommandResult::SUCCESS;
  }

  CommandResponse sendCommandWithResponse(Command command) {
    CommandResponse response;

    sendCommandAsync(command);
    waitUntilAsyncCommandFinishes();
    if (asyncCommand.result != CommandResult::SUCCESS) {
      response.result = asyncCommand.result;
      return response;
    }

    receiveCommandAsync();
    waitUntilAsyncCommandFinishes();
    if (asyncCommand.result != CommandResult::SUCCESS) {
      response.result = asyncCommand.result;
      return response;
    }

    response.result = asyncCommand.result;
    response.command = asyncCommand.cmd;

    auto remoteCommand = response.command.header.commandId;
    if (remoteCommand == COMMAND_ERROR_STATUS) {
      if (response.command.header.size != 2 ||
          response.command.data.bytes[0] != command.header.commandId) {
        response.result = CommandResult::UNEXPECTED_RESPONSE;
        return response;
      } else {
        response.result = CommandResult::ERROR;
        return response;
      }
    }

    if (remoteCommand != (command.header.commandId | OR_VALUE)) {
      response.result = CommandResult::UNEXPECTED_RESPONSE;
      return response;
    }

    return response;
  }

  void waitUntilAsyncCommandFinishes() {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    while (asyncCommand.isActive)
      if (cmdTimeout(lines, vCount)) {
        asyncCommand.isActive = false;
        asyncCommand.state = AsyncCommand::State::COMPLETED;
        asyncCommand.result = CommandResult::TIMEOUT;
        break;
      }
  }

  bool sendCommandAsync(Command command) {
    if (asyncCommand.isActive)
      return false;

    asyncCommand.reset();
    asyncCommand.cmd = command;

    if (isSIO32Mode()) {
      transferAsync(*((u32*)&command));  // TODO: CHECK ENDIANNESS
      asyncCommand.transferred += 4;
    } else
      advance(command.magicBytes.magic1);

    return true;
  }

  bool receiveCommandAsync() {
    if (asyncCommand.isActive)
      return false;

    asyncCommand.reset();
    asyncCommand.direction = AsyncCommand::Direction::RECEIVING;

    transferAsync(GBA_WAITING);

    return true;
  }

  void sendAsyncCommandSIO8(u32 newData) {
    const u8* commandBytes = (const u8*)&asyncCommand.cmd;
    u32 mainSize = PREAMBLE_SIZE + asyncCommand.cmd.header.size;

    bool isAcknowledgement =
        asyncCommand.transferred >= mainSize + CHECKSUM_SIZE + 1;
    if (!isAcknowledgement && newData != ADAPTER_WAITING)
      return asyncCommand.fail(CommandResult::NOT_WAITING);

    if (asyncCommand.transferred < mainSize) {
      // Magic Bytes + Packet Header + Packet Data
      advance(commandBytes[asyncCommand.transferred]);
    } else if (asyncCommand.transferred < mainSize + CHECKSUM_SIZE) {
      // Packet Checksum
      commandBytes += PREAMBLE_SIZE + LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH;
      advance(commandBytes[asyncCommand.transferred - mainSize]);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      advance(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance(0);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 2) {
      // Acknowledgement Signal (3)
      if (newData != (asyncCommand.cmd.header.commandId ^ OR_VALUE))
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.result = CommandResult::SUCCESS;
      asyncCommand.state = AsyncCommand::State::COMPLETED;
    }
  }

  void receiveAsyncCommandSIO8(u32 newData) {
    u8* commandBytes = (u8*)&asyncCommand.cmd;
    u32 mainSize = PREAMBLE_SIZE + asyncCommand.cmd.header.size;

    if (asyncCommand.transferred == 0) {
      // Magic Bytes (1)
      if (newData == ADAPTER_WAITING)
        return transferAsync(GBA_WAITING);
      if (newData != COMMAND_MAGIC_VALUE1)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      advance(GBA_WAITING);
    } else if (asyncCommand.transferred == 1) {
      // Magic Bytes (1)
      if (newData != COMMAND_MAGIC_VALUE2)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      advance(GBA_WAITING);
    } else if (asyncCommand.transferred < PREAMBLE_SIZE) {
      // Packet Header
      commandBytes[asyncCommand.transferred] = newData;
      if (asyncCommand.cmd.header._unusedSizeHigh_ != 0)
        return asyncCommand.fail(CommandResult::WEIRD_DATA_SIZE);
      advance(GBA_WAITING);
      if (asyncCommand.transferred == PREAMBLE_SIZE)
        asyncCommand.expectedChecksum = asyncCommand.cmd.header.sum();
    } else if (asyncCommand.transferred < mainSize) {
      // Packet Data
      commandBytes[asyncCommand.transferred] = newData;
      asyncCommand.expectedChecksum += newData;
      advance(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize) {
      // Packet Checksum (1)
      if (newData != msB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize + 1) {
      // Packet Checksum (2)
      if (newData != lsB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance(asyncCommand.cmd.header.commandId ^ OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (newData != 0)
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.result = CommandResult::SUCCESS;
      asyncCommand.state = AsyncCommand::State::COMPLETED;
    }
  }

  CommandResult acknowledge(u8 localCommandAck, u8 remoteCommandAck) {
    if (!isSupportedAdapter(transfer(DEVICE_GBA | OR_VALUE)))
      return CommandResult::INVALID_DEVICE_ID;
    if (transfer(localCommandAck) != remoteCommandAck)
      return CommandResult::INVALID_COMMAND_ACK;

    return CommandResult::SUCCESS;
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
    command.header.commandId = type;
    command.header._unused_ = 0;
    command.header._unusedSizeHigh_ = 0;
    command.header.size = withData ? (u8)nextCommandDataSize : 0;
    if (withData)
      command.data = nextCommandData;
    u16 checksum = command.header.sum();
    for (u32 i = 0; i < command.header.size; i++)
      checksum += command.data.bytes[i];
    command.checksum.high = msB16(checksum);
    command.checksum.low = lsB16(checksum);

    return command;
  }

  void advance(u32 data) {
    transferAsync(data);
    asyncCommand.transferred++;
  }

  void transferAsync(u32 data) {
    asyncCommand.isWaiting = true;
    asyncCommand.pendingData = data;
    startTimer(WAIT_TICKS[isSIO32Mode()]);
  }

  u32 transfer(u32 data) {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); });

    return receivedData;
  }

  bool isSIO32Mode() {
    return linkSPI->getDataSize() == LinkSPI::DataSize::SIZE_32BIT;
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
