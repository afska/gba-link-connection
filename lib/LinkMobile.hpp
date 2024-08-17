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

/**
 * @brief ...
 */
#define LINK_MOBILE_QUEUE_SIZE 10

static volatile char LINK_MOBILE_VERSION[] = "LinkMobile/v7.0.0";

#define LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH 254
#define LINK_MOBILE_MAX_PHONE_NUMBER_SIZE 32
#define LINK_MOBILE_COMMAND_TRANSFER_BUFFER \
  (LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH + 4)
#define LINK_MOBILE_DEFAULT_TIMEOUT 480
#define LINK_MOBILE_DEFAULT_TIMER_ID 3
#define LINK_MOBILE_BARRIER asm volatile("" ::: "memory")

#if LINK_ENABLE_DEBUG_LOGS != 0
#define _LMLOG_(...) Link::log(__VA_ARGS__)
#else
#define _LMLOG_(...)
#endif

class LinkMobile {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int INIT_WAIT_FRAMES = 7;
  static constexpr int INIT_TIMEOUT_FRAMES = 30;
  static constexpr int PING_FREQUENCY_FRAMES = 60;
  static constexpr int ADAPTER_WAITING = 0xD2;
  static constexpr u32 ADAPTER_WAITING_32BIT = 0xD2D2D2D2;
  static constexpr int GBA_WAITING = 0x4B;
  static constexpr u32 GBA_WAITING_32BIT = 0x4B4B4B4B;
  static constexpr int OR_VALUE = 0x80;
  static constexpr int COMMAND_MAGIC_VALUE1 = 0x99;
  static constexpr int COMMAND_MAGIC_VALUE2 = 0x66;
  static constexpr int DEVICE_GBA = 0x1;
  static constexpr int DEVICE_ADAPTER_BLUE = 0x8;
  static constexpr int DEVICE_ADAPTER_YELLOW = 0x9;
  static constexpr int DEVICE_ADAPTER_GREEN = 0xA;
  static constexpr int DEVICE_ADAPTER_RED = 0xB;
  static constexpr int ACK_SENDER = 0;
  static constexpr int CONFIGURATION_DATA_SIZE = 192;
  static constexpr int CONFIGURATION_DATA_CHUNK = CONFIGURATION_DATA_SIZE / 2;
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

  static constexpr u8 WAIT_TICKS[] = {4, 8};
  static constexpr int LOGIN_PARTS_SIZE = 8;
  static constexpr u8 LOGIN_PARTS[] = {0x4e, 0x49, 0x4e, 0x54,
                                       0x45, 0x4e, 0x44, 0x4f};
  static constexpr int SUPPORTED_DEVICES_SIZE = 4;
  static constexpr u8 SUPPORTED_DEVICES[] = {
      DEVICE_ADAPTER_BLUE, DEVICE_ADAPTER_YELLOW, DEVICE_ADAPTER_GREEN,
      DEVICE_ADAPTER_RED};
  static constexpr u8 DIAL_PHONE_FIRST_BYTE[] = {0, 2, 1, 1};

 public:
  enum State {
    NEEDS_RESET,
    PINGING,
    WAITING_TO_START,
    STARTING_SESSION,
    ACTIVATING_SIO32,
    WAITING_32BIT_SWITCH,
    READING_CONFIGURATION,
    SESSION_ACTIVE,
    CALL_REQUESTED,
    CALLING,
    CALL_ESTABLISHED,
    SESSION_ACTIVE_ISP,
    SHUTDOWN_REQUESTED,
    ENDING_SESSION,
    WAITING_8BIT_SWITCH,
    SHUTDOWN
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
    ERROR_CODE,
    WEIRD_ERROR_CODE,
    TIMEOUT  // TODO: USE (for `UserRequest`s)
  };

  enum Role { NOT_CONNECTED, CALLER, RECEIVER };

  struct Error {
    enum Type {
      NONE,
      ADAPTER_NOT_CONNECTED,
      COMMAND_FAILED,
      WEIRD_RESPONSE,
      BAD_CONFIGURATION_CHECKSUM
    };

    Error::Type type = Error::Type::NONE;
    State state = State::NEEDS_RESET;
    u8 cmdId = 0;
    CommandResult cmdResult = CommandResult::PENDING;
    u8 cmdErrorCode = 0;
    bool cmdIsSending = false;
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

  struct DataTransfer {
    u8 data[LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH] = {};
    u8 size = 0;
    bool completed = false;
  };

  explicit LinkMobile(u32 timeout = LINK_MOBILE_DEFAULT_TIMEOUT,
                      u8 timerId = LINK_MOBILE_DEFAULT_TIMER_ID) {
    this->config.timeout = timeout;
    this->config.timerId = timerId;
  }

  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief ...
   */
  void activate() {
    error = {};

    LINK_MOBILE_BARRIER;
    isEnabled = false;
    LINK_MOBILE_BARRIER;

    resetState();
    stop();

    LINK_MOBILE_BARRIER;
    isEnabled = true;
    LINK_MOBILE_BARRIER;

    start();
  }

  bool shutdown() {
    if (!canShutdown() || userRequests.isFull())
      return false;

    userRequests.syncPush(UserRequest{.type = UserRequest::Type::SHUTDOWN});
    return true;
  }

  void deactivate() {
    error = {};
    isEnabled = false;
    resetState();
    stop();
  }

  bool call(const char* phoneNumber) {
    if (state != SESSION_ACTIVE || userRequests.isFull())
      return false;

    auto request = UserRequest{.type = UserRequest::Type::CALL};
    copyString(request.phoneNumber, phoneNumber,
               LINK_MOBILE_MAX_PHONE_NUMBER_SIZE);
    userRequests.syncPush(request);
    return true;
  }

  /**
   * @brief ...
   * @param data The value to be sent.
   */
  bool transfer(DataTransfer dataToSend, DataTransfer* receivedData) {
    if (state != CALL_ESTABLISHED || userRequests.isFull())
      return false;

    receivedData->completed = false;
    auto request = UserRequest{.type = UserRequest::Type::TRANSFER,
                               .send = {.data = {}, .size = dataToSend.size},
                               .receive = receivedData,
                               .commandSent = false};
    for (u32 i = 0; i < dataToSend.size; i++)
      request.send.data[i] = dataToSend.data[i];
    userRequests.syncPush(request);
    return true;
  }

  bool hangUp() {
    if (state != CALL_ESTABLISHED || userRequests.isFull())
      return false;

    userRequests.syncPush(UserRequest{.type = UserRequest::Type::HANG_UP});
    return true;
  }

  bool readConfiguration(ConfigurationData& configurationData) {
    if (!isSessionActive())
      return false;

    configurationData = adapterConfiguration.fields;
    return true;
  }

  [[nodiscard]] State getState() { return state; }

  [[nodiscard]] Role getRole() { return role; }

  /**
   * @brief Returns `true` if the session is active.
   */
  [[nodiscard]] bool isSessionActive() {
    return state >= SESSION_ACTIVE && state <= SHUTDOWN_REQUESTED;
  }

  /**
   * @brief
   */
  [[nodiscard]] bool isConnected() { return state == CALL_ESTABLISHED; }

  [[nodiscard]] bool canShutdown() {
    return isSessionActive() && state != SHUTDOWN_REQUESTED;
  }

  [[nodiscard]] LinkSPI::DataSize getDataSize() {
    return linkSPI->getDataSize();
  }

  [[nodiscard]] Error getError() { return error; }

  ~LinkMobile() { delete linkSPI; }

  void _onVBlank() {
    if (!isEnabled)
      return;

    if (shouldAbortOnStateTimeout()) {
      timeoutStateFrames++;
      if (timeoutStateFrames >= INIT_TIMEOUT_FRAMES)
        return abort(Error::Type::ADAPTER_NOT_CONNECTED);
    }

    pingFrameCount++;
    if (pingFrameCount >= PING_FREQUENCY_FRAMES && isSessionActive() &&
        !asyncCommand.isActive) {
      pingFrameCount = 0;
      cmdTelephoneStatus();
    }

    processUserRequests();
    processNewFrame();
  }

  void _onSerial() {
    if (!isEnabled)
      return;

    linkSPI->_onSerial();
    u32 newData = linkSPI->getAsyncData();

    if (asyncCommand.isActive) {
      if (asyncCommand.state == AsyncCommand::State::PENDING) {
        if (isSIO32Mode()) {
          if (asyncCommand.direction == AsyncCommand::Direction::SENDING)
            sendAsyncCommandSIO32(newData);
          else
            receiveAsyncCommandSIO32(newData);
        } else {
          if (asyncCommand.direction == AsyncCommand::Direction::SENDING)
            sendAsyncCommandSIO8(newData);
          else
            receiveAsyncCommandSIO8(newData);
        }

        if (asyncCommand.state == AsyncCommand::State::COMPLETED) {
          asyncCommand.isActive = false;
          processAsyncCommand();
        }
      }
    } else {
      processLoosePacket(newData);
    }
  }

  void _onTimer() {
    if (!isEnabled || !hasPendingTransfer)
      return;

    linkSPI->transferAsync(pendingTransfer);
    stopTimer();
    hasPendingTransfer = false;
  }

  struct Config {
    u32 timeout;
    u32 timerId;
  };

  /**
   * @brief LinkMobile configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  enum AdapterType { BLUE, YELLOW, GREEN, RED, UNKNOWN };

  struct UserRequest {
    enum Type { CALL, TRANSFER, HANG_UP, SHUTDOWN };

    Type type;
    char phoneNumber[LINK_MOBILE_MAX_PHONE_NUMBER_SIZE + 1];
    DataTransfer send;
    DataTransfer* receive;
    bool commandSent;
  };

  union AdapterConfiguration {
    ConfigurationData fields;
    char bytes[CONFIGURATION_DATA_SIZE];

    bool isValid() { return calculatedChecksum() == reportedChecksum(); }

    u16 calculatedChecksum() {
      u16 result = 0;
      for (u32 i = 0; i < CONFIGURATION_DATA_SIZE - 2; i++)
        result += bytes[i];
      return result;
    }

    u16 reportedChecksum() {
      return buildU16(fields.checksumHigh, fields.checksumLow);
    }
  };

  struct MagicBytes {
    u8 magic1 = COMMAND_MAGIC_VALUE1;
    u8 magic2 = COMMAND_MAGIC_VALUE2;
  } __attribute__((packed));

  struct PacketData {
    u8 bytes[LINK_MOBILE_COMMAND_TRANSFER_BUFFER] = {};
  } __attribute__((packed));

  struct PacketHeader {
    u8 commandId = 0;
    u8 _unused_ = 0;
    u8 _unusedSizeHigh_ = 0;
    u8 size = 0;

    u16 sum() { return commandId + _unused_ + _unusedSizeHigh_ + size; }
    u8 pureCommandId() { return commandId & (~OR_VALUE); }
  } __attribute__((packed));

  struct PacketChecksum {
    u8 high = 0;
    u8 low = 0;
  } __attribute__((packed));

  struct Command {
    MagicBytes magicBytes;
    PacketHeader header;
    PacketData data;
    PacketChecksum checksum;
  };

  struct CommandResponse {
    CommandResult result = CommandResult::PENDING;
    Command command;
  };

  struct AsyncCommand {
    enum State { PENDING, COMPLETED };
    enum Direction { SENDING, RECEIVING };

    State state;
    CommandResult result;
    u32 transferred;
    Command cmd;
    Direction direction;
    u16 expectedChecksum;
    u8 errorCommandId;
    u8 errorCode;
    bool isActive = false;

    void reset() {
      state = AsyncCommand::State::PENDING;
      result = CommandResult::PENDING;
      transferred = 0;
      cmd = Command{};
      direction = AsyncCommand::Direction::SENDING;
      expectedChecksum = 0;
      errorCommandId = 0;
      errorCode = 0;
      isActive = false;
    }

    u8 relatedCommandId() {
      return result == CommandResult::ERROR_CODE ? errorCommandId
                                                 : cmd.header.pureCommandId();
    }

    bool respondsTo(u8 commandId) {
      return direction == AsyncCommand::Direction::RECEIVING &&
             (result == CommandResult::ERROR_CODE
                  ? errorCommandId == commandId
                  : cmd.header.commandId == (commandId | OR_VALUE));
    }

    void finish() {
      if (cmd.header.commandId == COMMAND_ERROR_STATUS) {
        if (cmd.header.size != 2) {
          result = CommandResult::WEIRD_ERROR_CODE;
        } else {
          result = CommandResult::ERROR_CODE;
          errorCommandId = cmd.data.bytes[0];
          errorCode = cmd.data.bytes[1];
        }
      } else {
        result = CommandResult::SUCCESS;
      }

      state = AsyncCommand::State::COMPLETED;
    }

    void fail(CommandResult _result) {
      result = _result;
      state = AsyncCommand::State::COMPLETED;
    }
  };

  static constexpr u32 PREAMBLE_SIZE =
      sizeof(MagicBytes) + sizeof(PacketHeader);
  static constexpr u32 CHECKSUM_SIZE = sizeof(PacketChecksum);

  using RequestQueue = Link::Queue<UserRequest, LINK_MOBILE_QUEUE_SIZE, false>;

  RequestQueue userRequests;
  AdapterConfiguration adapterConfiguration;
  AsyncCommand asyncCommand;
  u32 waitFrames = 0;
  u32 timeoutStateFrames = 0;
  u32 pingFrameCount = 0;
  Role role = Role::NOT_CONNECTED;
  LinkSPI* linkSPI = new LinkSPI();
  State state = NEEDS_RESET;
  PacketData nextCommandData;
  u32 nextCommandDataSize = 0;
  bool hasPendingTransfer = false;
  u32 pendingTransfer = 0;
  AdapterType adapterType = AdapterType::UNKNOWN;
  Error error = {};
  volatile bool isEnabled = false;

  void processUserRequests() {
    if (!userRequests.canMutate() || userRequests.isEmpty())
      return;

    if (!isSessionActive()) {
      userRequests.clear();
      return;
    }

    auto request = userRequests.peek();

    switch (request.type) {
      case UserRequest::Type::CALL: {
        if (state != SESSION_ACTIVE && state != CALL_REQUESTED) {
          userRequests.pop();
          return;
        }
        if (state != CALL_REQUESTED)
          setState(CALL_REQUESTED);

        if (!asyncCommand.isActive) {
          setState(CALLING);
          cmdDialTelephone(request.phoneNumber);
          userRequests.pop();
        }
        break;
      }
      case UserRequest::Type::TRANSFER: {
        if (state != CALL_ESTABLISHED) {
          userRequests.pop();
          return;
        }
        if (!asyncCommand.isActive && !request.commandSent) {
          cmdTransferData(0xff, request.send.data, request.send.size);
          request.commandSent = true;
        }
        break;
      }
      case UserRequest::Type::HANG_UP: {
        if (state != CALL_ESTABLISHED) {
          userRequests.pop();
          return;
        }
        if (!asyncCommand.isActive)
          cmdHangUpTelephone();
        break;
      }
      case UserRequest::Type::SHUTDOWN: {
        if (state != SHUTDOWN_REQUESTED)
          setState(SHUTDOWN_REQUESTED);

        if (!asyncCommand.isActive) {
          setState(ENDING_SESSION);
          cmdEndSession();
          userRequests.pop();
        }
        break;
      }
      default: {
      }
    }
  }

  void processNewFrame() {
    switch (state) {
      case WAITING_TO_START: {
        waitFrames--;

        if (waitFrames == 0) {
          setState(STARTING_SESSION);
          cmdBeginSession();
        }
        break;
      }
      case WAITING_32BIT_SWITCH: {
        waitFrames--;

        if (waitFrames == 0) {
          linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                            LinkSPI::DataSize::SIZE_32BIT);
          setState(READING_CONFIGURATION);
          cmdReadConfigurationData(0, CONFIGURATION_DATA_CHUNK);
        }
        break;
      }
      case SESSION_ACTIVE: {
        if (!asyncCommand.isActive)
          cmdWaitForTelephoneCall();

        break;
      }
      case WAITING_8BIT_SWITCH: {
        waitFrames--;

        if (waitFrames == 0) {
          linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                            LinkSPI::DataSize::SIZE_8BIT);
          error = {};
          setState(SHUTDOWN);
        }
        break;
      }
      default: {
      }
    }
  }

  void processAsyncCommand() {
    if (asyncCommand.result != CommandResult::SUCCESS) {
      if (shouldAbortOnCommandFailure())
        return abort(Error::Type::COMMAND_FAILED);
      else
        abort(Error::Type::COMMAND_FAILED, false);  // (log the error)
    }

    _LMLOG_("%s $%X [%d]",
            asyncCommand.direction == AsyncCommand::Direction::SENDING ? ">!"
                                                                       : "<!",
            asyncCommand.cmd.header.pureCommandId(),
            asyncCommand.cmd.header.size);

    if (asyncCommand.direction == AsyncCommand::Direction::SENDING) {
      receiveCommandAsync();
      return;
    }

    if (asyncCommand.respondsTo(COMMAND_TELEPHONE_STATUS)) {
      if (asyncCommand.cmd.header.size != 3)
        return abort(Error::Type::WEIRD_RESPONSE);
      if (state == CALL_ESTABLISHED) {
        if (!isBitHigh(asyncCommand.cmd.data.bytes[0], 2)) {
          // (call terminated)
          setState(SESSION_ACTIVE);
        }
      }
      return;
    }

    switch (state) {
      case STARTING_SESSION: {
        if (!asyncCommand.respondsTo(COMMAND_BEGIN_SESSION))
          return;
        if (asyncCommand.cmd.header.size != LOGIN_PARTS_SIZE)
          return abort(Error::Type::WEIRD_RESPONSE);

        for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++) {
          if (asyncCommand.cmd.data.bytes[i] != LOGIN_PARTS[i])
            return abort(Error::Type::WEIRD_RESPONSE);
        }

        setState(ACTIVATING_SIO32);
        cmdSIO32(true);
        break;
      }
      case ACTIVATING_SIO32: {
        if (asyncCommand.respondsTo(COMMAND_RESET)) {
          // If the adapter responds to a 0x16 instead of 0x18,
          // it's libmobile telling us that SIO32 is not supported.
          // In that case, we continue using SIO8.
          setState(READING_CONFIGURATION);
          cmdReadConfigurationData(0, CONFIGURATION_DATA_CHUNK);
          return;
        }
        if (!asyncCommand.respondsTo(COMMAND_SIO32))
          return;

        setState(WAITING_32BIT_SWITCH);
        waitFrames = INIT_WAIT_FRAMES;
        break;
      }
      case READING_CONFIGURATION: {
        if (!asyncCommand.respondsTo(COMMAND_READ_CONFIGURATION_DATA))
          return;

        u32 offset = asyncCommand.cmd.data.bytes[0];
        u32 sizeWithOffsetByte = asyncCommand.cmd.header.size;
        if (asyncCommand.result != CommandResult::SUCCESS ||
            sizeWithOffsetByte != CONFIGURATION_DATA_CHUNK + 1 ||
            (offset != 0 && offset != CONFIGURATION_DATA_CHUNK))
          return abort(Error::Type::WEIRD_RESPONSE);

        for (u32 i = 0; i < CONFIGURATION_DATA_CHUNK; i++)
          adapterConfiguration.bytes[offset + i] =
              asyncCommand.cmd.data.bytes[1 + i];

        if (offset == CONFIGURATION_DATA_CHUNK &&
            !adapterConfiguration.isValid())
          return abort(Error::Type::BAD_CONFIGURATION_CHECKSUM);

        if (offset == 0)
          cmdReadConfigurationData(CONFIGURATION_DATA_CHUNK,
                                   CONFIGURATION_DATA_CHUNK);
        else
          setState(SESSION_ACTIVE);
        break;
      }
      case SESSION_ACTIVE: {
        if (asyncCommand.respondsTo(COMMAND_WAIT_FOR_TELEPHONE_CALL)) {
          if (asyncCommand.result == CommandResult::SUCCESS) {
            setState(CALL_ESTABLISHED);
            role = Role::RECEIVER;
          } else {
            // (no call received)
          }
        }
        break;
      }
      case CALLING: {
        if (!asyncCommand.respondsTo(COMMAND_DIAL_TELEPHONE))
          return;

        if (asyncCommand.result == CommandResult::SUCCESS) {
          setState(CALL_ESTABLISHED);
          role = Role::CALLER;
        } else {
          // (call terminated)
          setState(SESSION_ACTIVE);
        }
        break;
      }
      case CALL_ESTABLISHED: {
        if (asyncCommand.respondsTo(COMMAND_HANG_UP_TELEPHONE)) {
          setState(SESSION_ACTIVE);
          return;
        }

        if (!asyncCommand.respondsTo(COMMAND_TRANSFER_DATA))
          return;

        auto request = userRequests.peek();

        if (asyncCommand.result == CommandResult::SUCCESS) {
          if (asyncCommand.cmd.header.size == 0)
            return abort(Error::Type::WEIRD_RESPONSE);

          if (request.type == UserRequest::TRANSFER) {
            u32 size = asyncCommand.cmd.header.size - 1;
            for (u32 i = 0; i < size; i++)
              request.receive->data[i] = asyncCommand.cmd.data.bytes[1 + i];
            request.receive->size = size;
            request.receive->completed = true;
            userRequests.pop();
          }
        } else {
          setState(SESSION_ACTIVE);
        }
      }
      case ENDING_SESSION: {
        if (!asyncCommand.respondsTo(COMMAND_END_SESSION))
          return;

        setState(WAITING_8BIT_SWITCH);
        waitFrames = INIT_WAIT_FRAMES;
        break;
      }
      default: {
      }
    }
  }

  void processLoosePacket(u32 newData) {
    switch (state) {
      case PINGING: {
        setState(WAITING_TO_START);
        waitFrames = INIT_WAIT_FRAMES;
        break;
      }
      default: {
      }
    }
  }

  void cmdBeginSession() {
    for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++)
      addData(LOGIN_PARTS[i], i == 0);
    sendCommandAsync(buildCommand(COMMAND_BEGIN_SESSION, true));
  }

  void cmdEndSession() { sendCommandAsync(buildCommand(COMMAND_END_SESSION)); }

  void cmdDialTelephone(char* phoneNumber) {
    addData(DIAL_PHONE_FIRST_BYTE[adapterType], true);
    for (u32 i = 0; i < std::strlen(phoneNumber); i++)
      addData(phoneNumber[i]);
    sendCommandAsync(buildCommand(COMMAND_DIAL_TELEPHONE, true));
  }

  void cmdHangUpTelephone() {
    sendCommandAsync(buildCommand(COMMAND_HANG_UP_TELEPHONE, true));
  }

  void cmdWaitForTelephoneCall() {
    sendCommandAsync(buildCommand(COMMAND_WAIT_FOR_TELEPHONE_CALL));
  }

  void cmdTransferData(u8 connectionId, const u8* data, u8 size) {
    addData(connectionId, true);
    for (u32 i = 0; i < size; i++)
      addData(data[i]);
    sendCommandAsync(buildCommand(COMMAND_TRANSFER_DATA, true));
  }

  void cmdTelephoneStatus() {
    sendCommandAsync(buildCommand(COMMAND_TELEPHONE_STATUS, true));
  }

  void cmdSIO32(bool enabled) {
    addData(enabled, true);
    sendCommandAsync(buildCommand(COMMAND_SIO32, true));
  }

  void cmdReadConfigurationData(u8 offset, u8 size) {
    addData(offset, true);
    addData(CONFIGURATION_DATA_CHUNK);
    sendCommandAsync(buildCommand(COMMAND_READ_CONFIGURATION_DATA, true));
  }

  bool shouldAbortOnStateTimeout() {
    return state > NEEDS_RESET && state < SESSION_ACTIVE;
  }

  bool shouldAbortOnCommandFailure() {
    u8 commandId = asyncCommand.relatedCommandId();
    return asyncCommand.direction == AsyncCommand::Direction::SENDING ||
           (commandId != COMMAND_WAIT_FOR_TELEPHONE_CALL &&
            commandId != COMMAND_DIAL_TELEPHONE);
  }

  void addData(u8 value, bool start = false) {
    if (start) {
      nextCommandDataSize = 0;
      nextCommandData = PacketData{};
    }
    nextCommandData.bytes[nextCommandDataSize] = value;
    nextCommandDataSize++;
  }

  void copyString(char* target, const char* source, u32 length) {
    u32 len = std::strlen(source);

    for (u32 i = 0; i < length + 1; i++)
      if (i < len)
        target[i] = source[i];
      else
        target[i] = '\0';
  }

  void setState(State newState) {
    role = Role::NOT_CONNECTED;
    State oldState = state;
    state = newState;
    timeoutStateFrames = 0;
    pingFrameCount = 0;
    _LMLOG_("!! new state: %d -> %d", oldState, newState);
    (void)oldState;
  }

  void abort(Error::Type errorType, bool fatal = true) {
    auto newError = Error{.type = errorType,
                          .state = state,
                          .cmdId = asyncCommand.relatedCommandId(),
                          .cmdResult = asyncCommand.result,
                          .cmdErrorCode = asyncCommand.errorCode,
                          .cmdIsSending = asyncCommand.direction ==
                                          AsyncCommand::Direction::SENDING};

    _LMLOG_(
        "!! %s:\n  error: %d\n  cmdId: %s$%X\n  cmdResult: %d\n  "
        "cmdErrorCode: %d",
        fatal ? "aborted" : "failed", newError.type,
        newError.cmdIsSending ? ">" : "<", newError.cmdId, newError.cmdResult,
        newError.cmdErrorCode);
    (void)newError;

    if (fatal) {
      error = newError;
      resetState();
      stop();
    }
  }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    setState(NEEDS_RESET);

    this->adapterConfiguration = AdapterConfiguration{};
    this->userRequests.clear();
    this->asyncCommand.reset();
    this->waitFrames = 0;
    this->timeoutStateFrames = 0;
    this->role = Role::NOT_CONNECTED;
    this->nextCommandDataSize = 0;
    this->hasPendingTransfer = false;
    this->pendingTransfer = 0;
    this->adapterType = AdapterType::UNKNOWN;

    userRequests.syncClear();
  }

  void stop() {
    stopTimer();
    linkSPI->deactivate();
  }

  void start() {
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                      LinkSPI::DataSize::SIZE_8BIT);

    setState(PINGING);
    transferAsync(0);
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

  void sendCommandAsync(Command command) {
    _LMLOG_(">> $%X [%d] (...)", command.header.commandId, command.header.size);
    asyncCommand.reset();
    asyncCommand.cmd = command;
    asyncCommand.isActive = true;

    if (isSIO32Mode())  // Magic+Header
      advance32(buildU32(command.magicBytes.magic1, command.magicBytes.magic2,
                         command.header.commandId, command.header._unused_));
    else  // Magic Bytes (1)
      advance8(command.magicBytes.magic1);
  }

  void receiveCommandAsync() {
    _LMLOG_("<< ...");
    asyncCommand.reset();
    asyncCommand.direction = AsyncCommand::Direction::RECEIVING;
    asyncCommand.isActive = true;

    if (isSIO32Mode())
      transferAsync(GBA_WAITING_32BIT);
    else
      transferAsync(GBA_WAITING);
  }

  void sendAsyncCommandSIO8(u32 newData) {
    const u8* commandBytes = (const u8*)&asyncCommand.cmd;
    u32 mainSize = PREAMBLE_SIZE + asyncCommand.cmd.header.size;

    bool isAcknowledgement =
        asyncCommand.transferred >= mainSize + CHECKSUM_SIZE + 1;
    if (!isAcknowledgement && newData != ADAPTER_WAITING) {
      _LMLOG_("!! not waiting: %X", newData);
      return asyncCommand.fail(CommandResult::NOT_WAITING);
    }

    if (asyncCommand.transferred < mainSize) {
      // Magic Bytes (2) + Packet Header + Packet Data
      advance8(commandBytes[asyncCommand.transferred]);
    } else if (asyncCommand.transferred < mainSize + CHECKSUM_SIZE) {
      // Packet Checksum
      commandBytes += PREAMBLE_SIZE + LINK_MOBILE_COMMAND_TRANSFER_BUFFER;
      advance8(commandBytes[asyncCommand.transferred - mainSize]);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      advance8(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance8(ACK_SENDER);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 2) {
      // Acknowledgement Signal (3)
      if (newData != (asyncCommand.cmd.header.commandId ^ OR_VALUE))
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
    }
  }

  void sendAsyncCommandSIO32(u32 newData) {
    u32 dataSize = asyncCommand.cmd.header.size;
    u32 alignment = dataSize % 4;
    u32 padding = alignment != 0 ? 4 - alignment : 0;
    u32 mainSize = PREAMBLE_SIZE + dataSize + padding;

    bool isAcknowledgement = asyncCommand.transferred >= mainSize;
    if (!isAcknowledgement && newData != ADAPTER_WAITING &&
        newData != ADAPTER_WAITING_32BIT) {
      _LMLOG_("!! not waiting: %X", newData);
      return asyncCommand.fail(CommandResult::NOT_WAITING);
    }

    if (asyncCommand.transferred == 4) {
      // Header+Data || Header+Checksum
      advance32(dataSize > 0
                    ? buildU32(asyncCommand.cmd.header._unusedSizeHigh_,
                               asyncCommand.cmd.header.size,
                               asyncCommand.cmd.data.bytes[0],
                               asyncCommand.cmd.data.bytes[1])
                    : buildU32(asyncCommand.cmd.header._unusedSizeHigh_,
                               asyncCommand.cmd.header.size,
                               asyncCommand.cmd.checksum.high,
                               asyncCommand.cmd.checksum.low));
    } else if (asyncCommand.transferred < mainSize) {
      // Data || Data+Checksum
      u32 transferredDataCount = asyncCommand.transferred - PREAMBLE_SIZE;
      u32 pendingDataCount = (dataSize + padding) - transferredDataCount;
      advance32(
          pendingDataCount > 2
              ? buildU32(asyncCommand.cmd.data.bytes[transferredDataCount],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 1],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 2],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 3])
              : buildU32(asyncCommand.cmd.data.bytes[transferredDataCount],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 1],
                         asyncCommand.cmd.checksum.high,
                         asyncCommand.cmd.checksum.low));
    } else if (asyncCommand.transferred < mainSize + 4) {
      // Acknowledgement Signal (1)
      advance32(buildU32(DEVICE_GBA | OR_VALUE, ACK_SENDER, 0, 0));
    } else {
      // Acknowledgement Signal (2)
      u16 ackData = msB32(newData);
      if (!isSupportedAdapter(msB16(ackData)))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      if (lsB16(ackData) != (asyncCommand.cmd.header.commandId ^ OR_VALUE))
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
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
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == 1) {
      // Magic Bytes (1)
      if (newData != COMMAND_MAGIC_VALUE2)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred < PREAMBLE_SIZE) {
      // Packet Header
      commandBytes[asyncCommand.transferred] = newData;
      if (asyncCommand.cmd.header._unusedSizeHigh_ != 0 ||
          asyncCommand.cmd.header.size >
              LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH)
        return asyncCommand.fail(CommandResult::WEIRD_DATA_SIZE);
      advance8(GBA_WAITING);
      if (asyncCommand.transferred == PREAMBLE_SIZE)
        asyncCommand.expectedChecksum = asyncCommand.cmd.header.sum();
    } else if (asyncCommand.transferred < mainSize) {
      // Packet Data
      commandBytes[asyncCommand.transferred] = newData;
      asyncCommand.expectedChecksum += newData;
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize) {
      // Packet Checksum (1)
      if (newData != msB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize + 1) {
      // Packet Checksum (2)
      if (newData != lsB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance8(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance8(asyncCommand.cmd.header.commandId ^ OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (newData != ACK_SENDER)
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
    }
  }

  void receiveAsyncCommandSIO32(u32 newData) {
    u32 dataSize = asyncCommand.cmd.header.size;
    u32 alignment = dataSize % 4;
    u32 padding = alignment != 0 ? 4 - alignment : 0;
    u32 mainSize = PREAMBLE_SIZE + dataSize + padding;

    if (asyncCommand.transferred == 0) {
      // Magic+Header
      if (newData == ADAPTER_WAITING || newData == ADAPTER_WAITING_32BIT)
        return transferAsync(GBA_WAITING_32BIT);
      u16 magic = msB32(newData);
      u16 firstHalfHeader = lsB32(newData);
      if (msB16(magic) != COMMAND_MAGIC_VALUE1 ||
          lsB16(magic) != COMMAND_MAGIC_VALUE2)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      asyncCommand.cmd.header.commandId = msB16(firstHalfHeader);
      asyncCommand.cmd.header._unused_ = lsB16(firstHalfHeader);
      advance32(GBA_WAITING_32BIT);
    } else if (asyncCommand.transferred == 4) {
      // Header+Data || Header+Checksum
      u16 secondHalfHeader = msB32(newData);
      asyncCommand.cmd.header._unusedSizeHigh_ = msB16(secondHalfHeader);
      asyncCommand.cmd.header.size = lsB16(secondHalfHeader);
      if (asyncCommand.cmd.header._unusedSizeHigh_ != 0 ||
          asyncCommand.cmd.header.size >
              LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH)
        return asyncCommand.fail(CommandResult::WEIRD_DATA_SIZE);
      asyncCommand.expectedChecksum = asyncCommand.cmd.header.sum();
      if (asyncCommand.cmd.header.size > 0) {
        u16 firstData = lsB32(newData);
        u8 b0 = msB16(firstData), b1 = lsB16(firstData);
        asyncCommand.cmd.data.bytes[0] = b0;
        asyncCommand.cmd.data.bytes[1] = b1;
        asyncCommand.expectedChecksum += b0 + b1;
      } else {
        u16 checksum = lsB32(newData);
        if (checksum != asyncCommand.expectedChecksum)
          return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
        asyncCommand.cmd.checksum.high = msB16(checksum);
        asyncCommand.cmd.checksum.low = lsB16(checksum);
      }
      advance32(GBA_WAITING_32BIT);
    } else if (asyncCommand.transferred < mainSize) {
      // Data || Data+Checksum
      u32 transferredDataCount = asyncCommand.transferred - PREAMBLE_SIZE;
      u32 pendingDataCount = (dataSize + padding) - transferredDataCount;
      if (pendingDataCount > 2) {
        u16 dataHigh = msB32(newData);
        u16 dataLow = lsB32(newData);
        u8 b0 = msB16(dataHigh), b1 = lsB16(dataHigh), b2 = msB16(dataLow),
           b3 = lsB16(dataLow);
        asyncCommand.cmd.data.bytes[transferredDataCount] = b0;
        asyncCommand.cmd.data.bytes[transferredDataCount + 1] = b1;
        asyncCommand.cmd.data.bytes[transferredDataCount + 2] = b2;
        asyncCommand.cmd.data.bytes[transferredDataCount + 3] = b3;
        asyncCommand.expectedChecksum += b0 + b1 + b2 + b3;
        advance32(GBA_WAITING_32BIT);
      } else {
        u16 lastData = msB32(newData);
        u8 b0 = msB16(lastData), b1 = lsB16(lastData);
        asyncCommand.cmd.data.bytes[transferredDataCount] = b0;
        asyncCommand.cmd.data.bytes[transferredDataCount + 1] = b1;
        asyncCommand.expectedChecksum += b0 + b1;
        u16 checksum = lsB32(newData);
        if (checksum != asyncCommand.expectedChecksum)
          return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
        asyncCommand.cmd.checksum.high = msB16(checksum);
        asyncCommand.cmd.checksum.low = lsB16(checksum);
        advance32(buildU32(DEVICE_GBA | OR_VALUE,
                           asyncCommand.cmd.header.commandId ^ OR_VALUE, 0, 0));
      }
    } else {
      // Acknowledgement Signal
      u32 ackData = msB32(newData);
      if (!isSupportedAdapter(msB16(ackData)) || lsB16(ackData) != ACK_SENDER)
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      asyncCommand.finish();
    }
  }

  bool isSupportedAdapter(u8 ack) {
    for (u32 i = 0; i < SUPPORTED_DEVICES_SIZE; i++) {
      if ((SUPPORTED_DEVICES[i] | OR_VALUE) == ack) {
        if (adapterType == AdapterType::UNKNOWN)
          adapterType = static_cast<AdapterType>(i);

        return true;
      }
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

  void advance8(u32 data) {
    transferAsync(data);
    asyncCommand.transferred++;
  }

  void advance32(u32 data) {
    transferAsync(data);
    asyncCommand.transferred += 4;
  }

  void transferAsync(u32 data) {
    hasPendingTransfer = true;
    pendingTransfer = data;
    startTimer(WAIT_TICKS[isSIO32Mode()]);
  }

  bool isSIO32Mode() {
    return linkSPI->getDataSize() == LinkSPI::DataSize::SIZE_32BIT;
  }

  static u32 buildU32(u8 msB, u8 byte2, u8 byte3, u8 lsB) {
    return ((msB & 0xFF) << 24) | ((byte2 & 0xFF) << 16) |
           ((byte3 & 0xFF) << 8) | (lsB & 0xFF);
  }
  static u16 buildU16(u8 msB, u8 lsB) { return (msB << 8) | lsB; }
  static u16 msB32(u32 value) { return value >> 16; }
  static u16 lsB32(u32 value) { return value & 0xffff; }
  static u8 msB16(u16 value) { return value >> 8; }
  static u8 lsB16(u16 value) { return value & 0xff; }
  bool isBitHigh(u8 byte, u8 bit) { return (byte >> bit) & 1; }
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

#undef _LMLOG_

#endif  // LINK_MOBILE_H
