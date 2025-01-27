#ifndef LINK_RAW_WIRELESS_H
#define LINK_RAW_WIRELESS_H

// --------------------------------------------------------------------------
// A low level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - There's one method for every supported Wireless Adapter command:
//   - `setup` = `0x17`
//   - `getSystemStatus` = `0x13`
//   - `broadcast` = `0x16`
//   - `startHost` = `0x19`
//   - `getSignalLevel` = `0x11`
//   - `getSlotStatus` = `0x14`
//   - `pollConnections` = `0x1A`
//   - `endHost` = `0x1B`
//   - `broadcastReadStart` = `0x1C`
//   - `broadcastReadPoll` = `0x1D`
//   - `broadcastReadEnd` = `0x1E`
//   - `connect` = `0x1F`
//   - `keepConnecting` = `0x20`
//   - `finishConnection` = `0x21`
//   - `sendData` = `0x24`
//   - `sendDataAndWait` = `0x25`
//   - `receiveData` = `0x26`
//   - `wait` = `0x27`
//   - `bye` = `0x3D`
// - Use `sendCommand(...)` to send arbitrary commands.
// - Use `sendCommandAsync(...)` to send arbitrary commands asynchronously.
//   - This requires setting `LINK_RAW_WIRELESS_ISR_SERIAL` as the `SERIAL`
//   interrupt handler.
//   - After calling this method, call `getAsyncState()` and
//   `getAsyncCommandResult()`.
//   - Do not call any other methods until the async state is `IDLE` again, or
//   the adapter will desync!
// - When sending arbitrary commands, the responses are not parsed. The
//   exceptions are SendData and ReceiveData, which have these helpers:
//   - `getSendDataHeaderFor(...)`
//   - `getReceiveDataResponse(...)`
// --------------------------------------------------------------------------
// considerations:
// - advanced usage only; if you're building a game, use `LinkWireless`!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

#ifndef LINK_RAW_WIRELESS_ENABLE_LOGGING
/**
 * @brief Enable logging.
 * \warning Set `linkRawWireless->logger` and uncomment to enable!
 * \warning This option #include`s std::string!
 */
// #define LINK_RAW_WIRELESS_ENABLE_LOGGING
#endif

LINK_VERSION_TAG LINK_RAW_WIRELESS_VERSION = "vLinkRawWireless/v8.0.0";

#define LINK_RAW_WIRELESS_MAX_PLAYERS 5
#define LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#define LINK_RAW_WIRELESS_MAX_GAME_ID 0x7FFF
#define LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 23
#define LINK_RAW_WIRELESS_BROADCAST_LENGTH 6
#define LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH \
  (1 + LINK_RAW_WIRELESS_BROADCAST_LENGTH)
#define LINK_RAW_WIRELESS_MAX_SERVERS              \
  (LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH / \
   LINK_RAW_WIRELESS_BROADCAST_LENGTH)

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
#include <string>
#define _LRWLOG_(str) logger(str)
#else
#define _LRWLOG_(str)
#endif

/**
 * @brief A low level driver for the GBA Wireless Adapter.
 */
class LinkRawWireless {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu8 = Link::vu8;

 public:
  static constexpr int PING_WAIT = 50;
  static constexpr int TRANSFER_WAIT = 15;
  static constexpr int MICRO_WAIT = 2;
#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  static constexpr int CMD_TIMEOUT = 228;
#else
  static constexpr int CMD_TIMEOUT = 15;
#endif
  static constexpr int LOGIN_STEPS = 9;
  static constexpr int COMMAND_HEADER_VALUE = 0x9966;
  static constexpr int RESPONSE_ACK = 0x80;
  static constexpr u32 DATA_REQUEST_VALUE = 0x80000000;
  static constexpr int SETUP_MAGIC = 0x003c0000;
  static constexpr int WAIT_STILL_CONNECTING = 0x01000000;
  static constexpr int COMMAND_HELLO = 0x10;
  static constexpr int COMMAND_SETUP = 0x17;
  static constexpr int COMMAND_SYSTEM_STATUS = 0x13;
  static constexpr int COMMAND_BROADCAST = 0x16;
  static constexpr int COMMAND_START_HOST = 0x19;
  static constexpr int COMMAND_SIGNAL_LEVEL = 0x11;
  static constexpr int COMMAND_SLOT_STATUS = 0x14;
  static constexpr int COMMAND_POLL_CONNECTIONS = 0x1A;
  static constexpr int COMMAND_END_HOST = 0x1B;
  static constexpr int COMMAND_BROADCAST_READ_START = 0x1C;
  static constexpr int COMMAND_BROADCAST_READ_POLL = 0x1D;
  static constexpr int COMMAND_BROADCAST_READ_END = 0x1E;
  static constexpr int COMMAND_CONNECT = 0x1F;
  static constexpr int COMMAND_IS_FINISHED_CONNECT = 0x20;
  static constexpr int COMMAND_FINISH_CONNECTION = 0x21;
  static constexpr int COMMAND_SEND_DATA = 0x24;
  static constexpr int COMMAND_SEND_DATA_AND_WAIT = 0x25;
  static constexpr int COMMAND_RECEIVE_DATA = 0x26;
  static constexpr int COMMAND_WAIT = 0x27;
  static constexpr int COMMAND_BYE = 0x3d;
  static constexpr int EVENT_WAIT_TIMEOUT = 0x27;
  static constexpr int EVENT_DATA_AVAILABLE = 0x28;
  static constexpr int EVENT_DISCONNECTED = 0x29;

  static constexpr u16 LOGIN_PARTS[] = {0x494E, 0x494E, 0x544E, 0x544E, 0x4E45,
                                        0x4E45, 0x4F44, 0x4F44, 0x8001};

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  typedef void (*Logger)(std::string);
  Logger logger = [](std::string str) {};
#endif

  enum class State {
    NEEDS_RESET = 0,
    AUTHENTICATED = 1,
    SEARCHING = 2,
    SERVING = 3,
    CONNECTING = 4,
    CONNECTED = 5
  };

  struct CommandResult {
    bool success = false;
    u8 commandId = 0;
    u32 data[LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
    u32 dataSize = 0;
  };

  struct Server {
    u16 id = 0;
    u16 gameId;
    char gameName[LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char userName[LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    u8 nextClientNumber;

    bool isFull() { return nextClientNumber == 0xFF; }
  };

  struct ConnectedClient {
    u16 deviceId = 0;
    u8 clientNumber = 0;
  };

  struct SystemStatusResponse {
    u16 deviceId = 0;
    u8 currentPlayerId = 0;
    State adapterState = State::AUTHENTICATED;
    bool isServerClosed = false;
  };

  struct SignalLevelResponse {
    u8 signalLevels[LINK_RAW_WIRELESS_MAX_PLAYERS] = {};
  };

  struct SlotStatusResponse {
    u8 nextClientNumber = 0;
    ConnectedClient connectedClients[LINK_RAW_WIRELESS_MAX_PLAYERS] = {};
    u32 connectedClientsSize = 0;
  };

  struct PollConnectionsResponse {
    ConnectedClient connectedClients[LINK_RAW_WIRELESS_MAX_PLAYERS] = {};
    u32 connectedClientsSize = 0;
  };

  struct BroadcastReadPollResponse {
    Server servers[LINK_RAW_WIRELESS_MAX_SERVERS] = {};
    u32 serversSize = 0;
  };

  enum class ConnectionPhase { STILL_CONNECTING, ERROR, SUCCESS };

  struct ConnectionStatus {
    ConnectionPhase phase = ConnectionPhase::STILL_CONNECTING;
    u8 assignedClientNumber = 0;
  };

  struct ReceiveDataResponse {
    u32 sentBytes[LINK_RAW_WIRELESS_MAX_PLAYERS];
    u32 data[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    u32 dataSize = 0;
  };

  enum class AsyncState { IDLE, WORKING, READY };

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   * Returns whether initialization was successful or not.
   */
  bool activate(bool _stopFirst = true) {
    LINK_READ_TAG(LINK_RAW_WIRELESS_VERSION);

    isEnabled = false;

    bool success = reset(_stopFirst);

    isEnabled = true;
    return success;
  }

  /**
   * @brief Restores the state from an existing connection on the Wireless
   * Adapter hardware. This is useful, for example, after a fresh launch of a
   * Multiboot game, to synchronize the library with the current state and
   * avoid a reconnection. Returns whether the restoration was successful.
   * On success, the state should be either `SERVING` or `CONNECTED`.
   * \warning This should be used as a replacement for `activate()`.
   */
  bool restoreExistingConnection() {
    isEnabled = false;

    _resetState();

    _LRWLOG_("setting SPI to 2Mbps");
    linkSPI.activate(LinkSPI::Mode::MASTER_2MBPS);

    _LRWLOG_("analyzing system status");
    SystemStatusResponse systemStatus;
    if (!getSystemStatus(systemStatus)) {
      deactivate();
      return false;
    }

    if (systemStatus.adapterState == State::SERVING) {
      _LRWLOG_("restoring SERVING state");

      SlotStatusResponse slotStatus;
      if (!getSlotStatus(slotStatus)) {
        deactivate();
        return false;
      }

      state = State::SERVING;
      sessionState.isServerClosed = systemStatus.isServerClosed;
    } else if (systemStatus.adapterState == State::CONNECTED) {
      _LRWLOG_("restoring CONNECTED state");
      state = State::CONNECTED;
    } else {
      _LRWLOG_("! invalid adapter state");
      deactivate();
      return false;
    }

    sessionState.currentPlayerId = systemStatus.currentPlayerId;
    _LRWLOG_("restored ok!");

    isEnabled = true;
    return true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    _resetState();
    stop();
  }

  /**
   * @brief Calls the Setup (`0x17`) command.
   * @param maxPlayers `(2~5)` Maximum players in hosted rooms. Clients should
   * set this to `0`.
   * @param maxTransmissions Number of transmissions before marking a player as
   * disconnected. `0` means infinite retransmissions.
   * @param waitTimeout Timeout of the *waiting commands*, in frames (16.6ms).
   * `0` means no timeout.
   * @param magic A part of the protocol that hasn't been reverse-engineered
   * yet. For now, it's magic (`0x003C0000`).
   */
  bool setup(u8 maxPlayers = LINK_RAW_WIRELESS_MAX_PLAYERS,
             u8 maxTransmissions = 4,
             u8 waitTimeout = 32,
             u32 magic = SETUP_MAGIC) {
    u32 config =
        (u32)(magic |
              (((LINK_RAW_WIRELESS_MAX_PLAYERS - maxPlayers) & 0b11) << 16) |
              (maxTransmissions << 8) | waitTimeout);
    u32 params[1] = {config};
    return sendCommand(COMMAND_SETUP, params, 1).success;
  }

  /**
   * @brief Calls the SystemStatus (`0x13`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool getSystemStatus(SystemStatusResponse& response) {
    auto result = sendCommand(COMMAND_SYSTEM_STATUS);

    if (!result.success || result.dataSize == 0) {
      if (result.dataSize == 0)
        _LRWLOG_("! empty response");
      _resetState();
      return false;
    }

    u32 status = result.data[0];

    response.deviceId = Link::lsB32(status);

    u8 slot = Link::lsB16(Link::msB32(status)) & 0b1111;
    response.currentPlayerId = slot == 0b0001   ? 1
                               : slot == 0b0010 ? 2
                               : slot == 0b0100 ? 3
                               : slot == 0b1000 ? 4
                                                : 0;

    u8 adapterState = Link::msB16(Link::msB32(status));
    response.isServerClosed = false;
    switch (adapterState) {
      case 1: {
        response.adapterState = State::SERVING;
        response.isServerClosed = true;
        break;
      }
      case 2: {
        response.adapterState = State::SERVING;
        break;
      }
      case 3: {
        response.adapterState = State::SEARCHING;
        break;
      }
      case 4: {
        response.adapterState = State::CONNECTING;
        break;
      }
      case 5: {
        response.adapterState = State::CONNECTED;
        break;
      }
      default: {
        response.adapterState = State::AUTHENTICATED;
        break;
      }
    }

    return true;
  }

  /**
   * @brief Calls the Broadcast (`0x16`) command.
   * @param gameName Game name. Maximum `14` characters + null terminator.
   * @param userName User name. Maximum `8` characters + null terminator.
   * @param gameId `(0 ~ 0x7FFF)` Game ID.
   */
  bool broadcast(const char* gameName = "",
                 const char* userName = "",
                 u16 gameId = LINK_RAW_WIRELESS_MAX_GAME_ID,
                 bool _validateNames = true) {
    if (_validateNames &&
        Link::strlen(gameName) > LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH) {
      _LRWLOG_("! game name too long");
      return false;
    }
    if (_validateNames &&
        Link::strlen(userName) > LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH) {
      _LRWLOG_("! user name too long");
      return false;
    }

    char finalGameName[LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char finalUserName[LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    copyName(finalGameName, gameName, LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH);
    copyName(finalUserName, userName, LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH);

    u32 params[LINK_RAW_WIRELESS_BROADCAST_LENGTH] = {
        Link::buildU32(Link::buildU16(finalGameName[1], finalGameName[0]),
                       gameId),
        Link::buildU32(Link::buildU16(finalGameName[5], finalGameName[4]),
                       Link::buildU16(finalGameName[3], finalGameName[2])),
        Link::buildU32(Link::buildU16(finalGameName[9], finalGameName[8]),
                       Link::buildU16(finalGameName[7], finalGameName[6])),
        Link::buildU32(Link::buildU16(finalGameName[13], finalGameName[12]),
                       Link::buildU16(finalGameName[11], finalGameName[10])),
        Link::buildU32(Link::buildU16(finalUserName[3], finalUserName[2]),
                       Link::buildU16(finalUserName[1], finalUserName[0])),
        Link::buildU32(Link::buildU16(finalUserName[7], finalUserName[6]),
                       Link::buildU16(finalUserName[5], finalUserName[4]))};
    bool success = sendCommand(COMMAND_BROADCAST, params,
                               LINK_RAW_WIRELESS_BROADCAST_LENGTH)
                       .success;

    if (!success) {
      _resetState();
      return false;
    }

    return true;
  }

  /**
   * @brief Calls the StartHost (`0x19`) command.
   * @param wait Whether the function should wait the recommended time or not.
   */
  bool startHost(bool wait = true) {
    bool success = sendCommand(COMMAND_START_HOST).success;

    if (!success) {
      _resetState();
      return false;
    }

    if (wait)
      Link::wait(TRANSFER_WAIT);

    _LRWLOG_("state = SERVING");
    state = State::SERVING;

    _LRWLOG_("server OPEN");
    sessionState.isServerClosed = false;

    return true;
  }

  /**
   * @brief Calls the SignalLevel (`0x11`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool getSignalLevel(SignalLevelResponse& response) {
    auto result = sendCommand(COMMAND_SIGNAL_LEVEL);

    if (!result.success || result.dataSize == 0) {
      if (result.dataSize == 0)
        _LRWLOG_("! empty response");
      _resetState();
      return false;
    }

    u32 levels = result.data[0];

    for (u32 i = 1; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++)
      response.signalLevels[i] = (levels >> ((i - 1) * 8)) & 0xFF;

    return true;
  }

  /**
   * @brief Calls the SlotStatus (`0x14`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool getSlotStatus(SlotStatusResponse& response) {
    auto result = sendCommand(COMMAND_SLOT_STATUS);

    if (!result.success) {
      _resetState();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.dataSize; i++) {
      if (i == 0) {
        response.nextClientNumber = (u8)Link::lsB32(result.data[i]);
      } else {
        response.connectedClients[response.connectedClientsSize++] =
            ConnectedClient{.deviceId = Link::lsB32(result.data[i]),
                            .clientNumber = (u8)Link::msB32(result.data[i])};
      }
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + response.connectedClientsSize;
    if (sessionState.playerCount != oldPlayerCount)
      _LRWLOG_("now: " + std::to_string(sessionState.playerCount) + " players");

    return true;
  }

  /**
   * @brief Calls the PollConnections (`0x1A`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool pollConnections(PollConnectionsResponse& response) {
    auto result = sendCommand(COMMAND_POLL_CONNECTIONS);

    if (!result.success) {
      _resetState();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.dataSize; i++) {
      response.connectedClients[response.connectedClientsSize++] =
          ConnectedClient{.deviceId = Link::lsB32(result.data[i]),
                          .clientNumber = (u8)Link::msB32(result.data[i])};
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + result.dataSize;
    if (sessionState.playerCount != oldPlayerCount)
      _LRWLOG_("now: " + std::to_string(sessionState.playerCount) + " players");

    return true;
  }

  /**
   * @brief Calls the EndHost (`0x1B`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool endHost(PollConnectionsResponse& response) {
    auto result = sendCommand(COMMAND_END_HOST);

    if (!result.success) {
      _resetState();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.dataSize; i++) {
      response.connectedClients[response.connectedClientsSize++] =
          ConnectedClient{.deviceId = Link::lsB32(result.data[i]),
                          .clientNumber = (u8)Link::msB32(result.data[i])};
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + result.dataSize;
    if (sessionState.playerCount != oldPlayerCount)
      _LRWLOG_("now: " + std::to_string(sessionState.playerCount) + " players");

    _LRWLOG_("server CLOSED");
    sessionState.isServerClosed = true;

    return true;
  }

  /**
   * @brief Calls the BroadcastReadStart (`0x1C`) command.
   */
  bool broadcastReadStart() {
    bool success = sendCommand(COMMAND_BROADCAST_READ_START).success;

    if (!success) {
      _resetState();
      return false;
    }

    _LRWLOG_("state = SEARCHING");
    state = State::SEARCHING;

    return true;
  }

  /**
   * @brief Calls the BroadcastReadPoll (`0x1D`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool broadcastReadPoll(BroadcastReadPollResponse& response) {
    auto result = sendCommand(COMMAND_BROADCAST_READ_POLL);
    bool success =
        result.success &&
        result.dataSize % LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH == 0;

    if (!success) {
      _resetState();
      return false;
    }

    u32 totalBroadcasts =
        result.dataSize / LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH;

    response.serversSize = 0;
    for (u32 i = 0; i < totalBroadcasts; i++) {
      u32 start = LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH * i;

      Server server;
      server.id = (u16)result.data[start];
      server.gameId = result.data[start + 1] & LINK_RAW_WIRELESS_MAX_GAME_ID;
      u32 gameI = 0, userI = 0;
      recoverName(server.gameName, gameI, result.data[start + 1], false);
      recoverName(server.gameName, gameI, result.data[start + 2]);
      recoverName(server.gameName, gameI, result.data[start + 3]);
      recoverName(server.gameName, gameI, result.data[start + 4]);
      recoverName(server.userName, userI, result.data[start + 5]);
      recoverName(server.userName, userI, result.data[start + 6]);
      server.gameName[gameI] = '\0';
      server.userName[userI] = '\0';
      server.nextClientNumber = (result.data[start] >> 16) & 0xFF;

      response.servers[response.serversSize++] = server;
    }

    return true;
  }

  /**
   * @brief Calls the BroadcastReadEnd (`0x1E`) command.
   */
  bool broadcastReadEnd() {
    bool success = sendCommand(COMMAND_BROADCAST_READ_END).success;

    if (!success) {
      _resetState();
      return false;
    }

    _LRWLOG_("state = AUTHENTICATED");
    state = State::AUTHENTICATED;

    return true;
  }

  /**
   * @brief Calls the Connect (`0x1F`) command.
   * @param serverId Device ID of the server.
   */
  bool connect(u16 serverId) {
    u32 params[1] = {serverId};
    bool success = sendCommand(COMMAND_CONNECT, params, 1).success;

    if (!success) {
      _resetState();
      return false;
    }

    _LRWLOG_("state = CONNECTING");
    state = State::CONNECTING;

    return true;
  }

  /**
   * @brief Calls the IsConnectionComplete (`0x20`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool keepConnecting(ConnectionStatus& response) {
    auto result = sendCommand(COMMAND_IS_FINISHED_CONNECT);
    if (!result.success || result.dataSize == 0) {
      if (result.dataSize == 0)
        _LRWLOG_("! empty response");
      _resetState();
      return false;
    }

    if (result.data[0] == WAIT_STILL_CONNECTING) {
      response.phase = ConnectionPhase::STILL_CONNECTING;
      return true;
    }

    u8 assignedPlayerId = 1 + (u8)Link::msB32(result.data[0]);
    if (assignedPlayerId >= LINK_RAW_WIRELESS_MAX_PLAYERS) {
      _LRWLOG_("! connection failed (1)");
      _resetState();
      response.phase = ConnectionPhase::ERROR;
      return false;
    }

    response.phase = ConnectionPhase::SUCCESS;
    response.assignedClientNumber = (u8)Link::msB32(result.data[0]);

    return true;
  }

  /**
   * @brief Calls the FinishConnection (`0x21`) command.
   */
  bool finishConnection() {
    auto result = sendCommand(COMMAND_FINISH_CONNECTION);
    if (!result.success || result.dataSize == 0) {
      if (result.dataSize == 0)
        _LRWLOG_("! empty response");
      _resetState();
      return false;
    }

    u16 status = Link::msB32(result.data[0]);
    if ((Link::msB16(status) & 1) == 1) {
      _LRWLOG_("! connection failed (2)");
      _resetState();
      return false;
    }

    u8 assignedPlayerId = 1 + (u8)status;
    sessionState.currentPlayerId = assignedPlayerId;
    _LRWLOG_("state = CONNECTED");
    state = State::CONNECTED;

    return true;
  }

  /**
   * @brief Calls the SendData (`0x24`) command.
   * @param data The values to be sent.
   * @param dataSize The number of 32-bit values in the `data` array.
   * @param _bytes The number of BYTES to send. If `0`, the method will use
   * `dataSize * 4` instead.
   */
  bool sendData(const u32* data, u32 dataSize, u32 _bytes = 0) {
    u32 bytes = _bytes == 0 ? dataSize * 4 : _bytes;
    u32 header = getSendDataHeaderFor(bytes);
    _LRWLOG_("using header " + toHex(header));

    u32 rawData[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    rawData[0] = header;
    for (u32 i = 0; i < dataSize; i++)
      rawData[i + 1] = data[i];

    bool success =
        sendCommand(COMMAND_SEND_DATA, rawData, 1 + dataSize).success;

    if (!success) {
      _resetState();
      return false;
    }

    return true;
  }

  /**
   * @brief Calls the SendDataAndWait (`0x25`) command.
   * @param data The values to be sent.
   * @param dataSize The number of 32-bit values in the `data` array.
   * @param remoteCommand A structure that will be filled with the remote
   * command from the adapter.
   * @param _bytes The number of BYTES to send. If `0`, the method will use
   * `dataSize * 4` instead.
   */
  bool sendDataAndWait(const u32* data,
                       u32 dataSize,
                       CommandResult& remoteCommand,
                       u32 _bytes = 0) {
    u32 bytes = _bytes == 0 ? dataSize * 4 : _bytes;
    u32 header = getSendDataHeaderFor(bytes);
    _LRWLOG_("using header " + toHex(header));

    u32 rawData[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    rawData[0] = header;
    for (u32 i = 0; i < dataSize; i++)
      rawData[i + 1] = data[i];

    if (!sendCommand(COMMAND_SEND_DATA_AND_WAIT, rawData, 1 + dataSize, true)
             .success) {
      _resetState();
      return false;
    }

    remoteCommand = receiveCommandFromAdapter();

    return remoteCommand.success;
  }

  /**
   * @brief Calls the ReceiveData (`0x26`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool receiveData(ReceiveDataResponse& response) {
    auto result = sendCommand(COMMAND_RECEIVE_DATA);
    return getReceiveDataResponse(result, response);
  }

  /**
   * @brief Calls the Wait (`0x27`) command.
   * @param remoteCommand A structure that will be filled with the remote
   * command from the adapter.
   */
  bool wait(CommandResult& remoteCommand) {
    if (!sendCommand(COMMAND_WAIT, {}, 0, true).success) {
      _resetState();
      return false;
    }

    remoteCommand = receiveCommandFromAdapter();

    return remoteCommand.success;
  }

  /**
   * @brief Calls the Bye (`3d`) command.
   */
  bool bye() { return sendCommand(COMMAND_BYE).success; }

  /**
   * Returns the header for the commands 0x24 and 0x25.
   * @param bytes The number of bytes of the command.
   */
  u32 getSendDataHeaderFor(u32 bytes) {
    return sessionState.currentPlayerId == 0
               ? bytes
               : (bytes << (3 + sessionState.currentPlayerId * 5));
  }

  /**
   * Returns the parsed response of a 0x26 command.
   * @param result The raw response returned by the command call.
   * @param response A structure that will be filled with the response data.
   */
  bool getReceiveDataResponse(CommandResult result,
                              ReceiveDataResponse& response) {
    for (u32 i = 0; i < result.dataSize; i++)
      response.data[i] = result.data[i];
    response.dataSize = result.dataSize;

    if (!result.success) {
      _resetState();
      return false;
    }

    for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++)
      response.sentBytes[i] = 0;

    if (response.dataSize > 0) {
      u32 header = response.data[0];
      for (u32 i = 1; i < response.dataSize; i++)
        response.data[i - 1] = response.data[i];
      response.dataSize--;
      response.sentBytes[0] = header & 0b1111111;
      response.sentBytes[1] = (header >> 8) & 0b11111;
      response.sentBytes[2] = (header >> 13) & 0b11111;
      response.sentBytes[3] = (header >> 18) & 0b11111;
      response.sentBytes[4] = (header >> 23) & 0b11111;
    }

    return true;
  }

  /**
   * @brief Calls an arbitrary command and returns the response.
   * @param type The ID of the command.
   * @param params The command parameters.
   * @param length The number of 32-bit values in the `params` array.
   * @param invertsClock Whether this command inverts the clock or not (Wait).
   * \warning If it `invertsClock`, call `receiveCommandFromAdapter()` on
   * finish.
   */
  CommandResult sendCommand(u8 type,
                            const u32* params = {},
                            u16 length = 0,
                            bool invertsClock = false) {
    CommandResult result;
    u32 command = buildCommand(type, length);
    u32 r;

    _LRWLOG_("sending command 0x" + toHex(command));
    if ((r = transfer(command)) != DATA_REQUEST_VALUE) {
      logExpectedButReceived(DATA_REQUEST_VALUE, r);
      return result;
    }

    u32 parameterCount = 0;
    for (u32 i = 0; i < length; i++) {
      u32 param = params[i];
      _LRWLOG_("sending param" + std::to_string(parameterCount) + ": 0x" +
               toHex(param));
      if ((r = transfer(param)) != DATA_REQUEST_VALUE) {
        logExpectedButReceived(DATA_REQUEST_VALUE, r);
        return result;
      }
      parameterCount++;
    }

    _LRWLOG_("sending response request");
    u32 response = transfer(DATA_REQUEST_VALUE);
    u16 header = Link::msB32(response);
    u16 data = Link::lsB32(response);
    u8 responses = Link::_min(Link::msB16(data),
                              LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH);
    u8 ack = Link::lsB16(data);

    if (header != COMMAND_HEADER_VALUE) {
      _LRWLOG_("! expected HEADER 0x9966");
      _LRWLOG_("! but received 0x" + toHex(header));
      return result;
    }
    if (ack != type + RESPONSE_ACK) {
      if (ack == 0xEE && responses == 1 && !invertsClock) {
        u8 __attribute__((unused)) code = (u8)transfer(DATA_REQUEST_VALUE);
        _LRWLOG_("! error received");
        _LRWLOG_(code == 1 ? "! invalid state" : "! unknown cmd");
      } else {
        _LRWLOG_("! expected ACK 0x" + toHex(type + RESPONSE_ACK));
        _LRWLOG_("! but received 0x" + toHex(ack));
      }
      return result;
    }
    _LRWLOG_("ack ok! " + std::to_string(responses) + " responses");

    if (!invertsClock) {
      for (u32 i = 0; i < responses; i++) {
        _LRWLOG_("response " + std::to_string(i + 1) + "/" +
                 std::to_string(responses) + ":");
        u32 responseData = transfer(DATA_REQUEST_VALUE);
        result.data[result.dataSize++] = responseData;
        _LRWLOG_("<< " + toHex(responseData));
      }
    }

    result.success = true;
    return result;
  }

  /**
   * @brief Inverts the clock and waits until the adapter sends a command.
   * Returns the remote command.
   */
  CommandResult receiveCommandFromAdapter() {
    CommandResult remoteCommand;

    _LRWLOG_("setting SPI to SLAVE");
    linkSPI.activate(LinkSPI::Mode::SLAVE);

    _LRWLOG_("WAITING for adapter cmd");
    u32 command = linkSPI.transfer(
        DATA_REQUEST_VALUE, []() { return false; }, false, true);
    if (!reverseAcknowledge()) {
      _resetState();
      return remoteCommand;
    }

    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    u16 header = Link::msB32(command);
    u16 data = Link::lsB32(command);
    u8 params = Link::msB16(data);
    u8 commandId = Link::lsB16(data);
    if (header != COMMAND_HEADER_VALUE) {
      _LRWLOG_("! expected HEADER 0x9966");
      _LRWLOG_("! but received 0x" + toHex(header));
      _resetState();
      return remoteCommand;
    }
    _LRWLOG_("received cmd: " + toHex(commandId) + " (" +
             std::to_string(params) + " params)");

    for (u32 i = 0; i < params; i++) {
      _LRWLOG_("param " + std::to_string(i + 1) + "/" + std::to_string(params) +
               ":");
      u32 paramData = linkSPI.transfer(
          DATA_REQUEST_VALUE,
          [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
          false, true);
      if (!reverseAcknowledge()) {
        _resetState();
        return remoteCommand;
      }
      remoteCommand.data[remoteCommand.dataSize++] = paramData;
      _LRWLOG_("<< " + toHex(paramData));
    }

    _LRWLOG_("sending ack");
    command = linkSPI.transfer(
        (COMMAND_HEADER_VALUE << 16) | ((commandId + RESPONSE_ACK) & 0xFF),
        [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); }, false,
        true);
    if (!reverseAcknowledge(true)) {
      _resetState();
      return remoteCommand;
    }

    if (command != DATA_REQUEST_VALUE) {
      _LRWLOG_("! expected CMD request");
      _LRWLOG_("! but received 0x" + toHex(command));
      _resetState();
      return remoteCommand;
    }

    _LRWLOG_("setting SPI to MASTER");
    linkSPI.activate(LinkSPI::Mode::MASTER_2MBPS);

    remoteCommand.success = true;
    remoteCommand.commandId = commandId;

    return remoteCommand;
  }

  /**
   * @brief Schedules an arbitrary command and returns the response. After
   * this, call `getAsyncState()` and `getAsyncCommandResult()`. Note that until
   * you retrieve the async response, next command requests will fail!
   * @param type The ID of the command.
   * @param params The command parameters.
   * @param length The number of 32-bit values in the `params` array.
   * @param invertsClock Whether this command inverts the clock or not (Wait).
   * \warning If it `invertsClock`, the command result will be the one sent by
   * the adapter.
   */
  bool sendCommandAsync(u8 type,
                        const u32* params = {},
                        u16 length = 0,
                        bool invertsClock = false,
                        bool _fromIRQ = false) {
    if (asyncState != AsyncState::IDLE)
      return false;

    asyncCommand.type = type;
    asyncCommand.invertsClock = invertsClock;
    asyncCommand.direction = AsyncCommand::Direction::SENDING;
    for (u32 i = 0; i < length; i++)
      asyncCommand.parameters[i] = params[i];
    asyncCommand.result = CommandResult{};
    asyncCommand.result.commandId = type;
    asyncCommand.state = AsyncCommand::State::PENDING;
    asyncCommand.step = AsyncCommand::Step::COMMAND_HEADER;
    asyncCommand.sentParameters = 0;
    asyncCommand.totalParameters = length;
    asyncCommand.receivedResponses = 0;
    asyncCommand.totalResponses = 0;
    asyncState = AsyncState::WORKING;

    u32 command = buildCommand(type, asyncCommand.totalParameters);

    _LRWLOG_("sending command 0x" + toHex(command));
    transferAsync(command, _fromIRQ);

    return true;
  }

  /**
   * @brief Returns the state of the last async command.
   * @return One of the enum values from `LinkRawWireless::AsyncState`.
   */
  [[nodiscard]] AsyncState getAsyncState() { return asyncState; }

  /**
   * @brief If the async state is `READY`, returns the result of the command and
   * switches the state back to `IDLE`. If not, returns an empty result.
   */
  [[nodiscard]] CommandResult getAsyncCommandResult() {
    if (asyncState != AsyncState::READY)
      return CommandResult{};

    CommandResult data = asyncCommand.result;
    asyncState = AsyncState::IDLE;
    return data;
  }

  /**
   * @brief Returns the maximum number of transferrable 32-bit values.
   * It's 23 for servers and 4 for clients.
   */
  [[nodiscard]] u32 getDeviceTransferLength() {
    return state == State::SERVING
               ? LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH
               : LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
  }

  /**
   * @brief Returns the current state.
   */
  [[nodiscard]] State getState() { return state; }

  /**
   * @brief Returns `true` if the player count is higher than `1`.
   */
  [[nodiscard]] bool isConnected() { return sessionState.playerCount > 1; }

  /**
   * @brief Returns `true` if the state is `SERVING` or `CONNECTED`.
   */
  [[nodiscard]] bool isSessionActive() {
    return state == State::SERVING || state == State::CONNECTED;
  }

  /**
   * @brief Returns `true` if the server was closed with `endHost()`.
   */
  [[nodiscard]] bool isServerClosed() { return sessionState.isServerClosed; }

  /**
   * @brief Returns the number of connected players.
   */
  [[nodiscard]] u8 playerCount() { return sessionState.playerCount; }

  /**
   * @brief Returns the current player ID.
   */
  [[nodiscard]] u8 currentPlayerId() { return sessionState.currentPlayerId; }

  /**
   * @brief Resets all the state.
   * \warning This is internal API!
   */
  void _resetState() {
    _LRWLOG_("state = NEEDS_RESET");
    state = State::NEEDS_RESET;
    asyncState = AsyncState::IDLE;
    sessionState.playerCount = 1;
    sessionState.currentPlayerId = 0;
    sessionState.isServerClosed = false;
  }

  /**
   * @brief Returns a pointer to the internal result of the last async command
   * and switches the state back to `IDLE`.
   * \warning This is internal API!
   */
  [[nodiscard]] CommandResult* _getAsyncCommandResultRef() {
    asyncState = AsyncState::IDLE;
    return &asyncCommand.result;
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  LINK_INLINE int _onSerial(bool _clockInversionSupport = true) {
    if (!isEnabled)
      return -1;

    linkSPI._onSerial(true);

    bool hasNewData = linkSPI.getAsyncState() == LinkSPI::AsyncState::READY;
    if (!hasNewData)
      return -2;
    u32 newData = linkSPI.getAsyncData();

    if (!isSessionActive() || asyncState != AsyncState::WORKING)
      return -3;

    if (asyncCommand.state == AsyncCommand::State::PENDING) {
      if (!_clockInversionSupport ||
          asyncCommand.direction == AsyncCommand::Direction::SENDING) {
        if (!acknowledge())
          return -4;
        sendAsyncCommand(newData, _clockInversionSupport);
      } else if (_clockInversionSupport) {
        if (!reverseAcknowledge(asyncCommand.step ==
                                AsyncCommand::Step::DATA_REQUEST))
          return -5;
        receiveAsyncCommand(newData);
      }

      if (asyncCommand.state == AsyncCommand::State::COMPLETED) {
        asyncState = AsyncState::READY;
        return 1;
      }
    }

    return 0;
  }

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  /**
   * @brief Converts `w` to an hexadecimal string.
   */
  template <typename I>
  [[nodiscard]] std::string toHex(I w, size_t hex_len = sizeof(I) << 1) {
    static const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
      rc[i] = digits[(w >> j) & 0x0F];
    return rc;
  }
#endif

  // -------------
  // Low-level API
  // -------------
  struct SessionState {
    vu8 playerCount = 1;
    vu8 currentPlayerId = 0;
    volatile bool isServerClosed = false;
  };
  SessionState sessionState;
  // ------------

 private:
  struct LoginMemory {
    u16 previousGBAData = 0xFFFF;
    u16 previousAdapterData = 0xFFFF;
  };

  struct AsyncCommand {
    enum class State { PENDING, COMPLETED };
    enum class Direction { SENDING, RECEIVING };

    enum class Step {
      COMMAND_HEADER,
      COMMAND_PARAMETERS,
      RESPONSE_REQUEST,
      DATA_REQUEST
    };  // (these are named from the sender's point of view)

    u8 type;
    bool invertsClock;
    Direction direction;
    u32 parameters[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    u32 responses[LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
    CommandResult result;
    State state;
    Step step;
    u32 sentParameters, totalParameters;
    u32 receivedResponses, totalResponses;
  };

  LinkSPI linkSPI;
  LinkGPIO linkGPIO;
  volatile State state = State::NEEDS_RESET;
  volatile AsyncState asyncState = AsyncState::IDLE;
  AsyncCommand asyncCommand;
  volatile bool isEnabled = false;

  void copyName(char* target, const char* source, u32 length) {
    u32 len = Link::strlen(source);

    for (u32 i = 0; i < length + 1; i++)
      if (i < len)
        target[i] = source[i];
      else
        target[i] = '\0';
  }

  void recoverName(char* name,
                   u32& nameCursor,
                   u32 word,
                   bool includeFirstTwoBytes = true) {
    u32 character = 0;
    if (includeFirstTwoBytes) {
      character = Link::lsB16(Link::lsB32(word));
      if (character > 0)
        name[nameCursor++] = character;
      character = Link::msB16(Link::lsB32(word));
      if (character > 0)
        name[nameCursor++] = character;
    }
    character = Link::lsB16(Link::msB32(word));
    if (character > 0)
      name[nameCursor++] = character;
    character = Link::msB16(Link::msB32(word));
    if (character > 0)
      name[nameCursor++] = character;
  }

  bool reset(bool stopFirst) {
    _resetState();
    if (stopFirst)
      stop();
    return start();
  }

  void stop() { linkSPI.deactivate(); }

  bool start() {
    pingAdapter();
    _LRWLOG_("setting SPI to 256Kbps");
    linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);

    if (!login())
      return false;

    Link::wait(TRANSFER_WAIT);

    _LRWLOG_("sending HELLO command");
    if (!sendCommand(COMMAND_HELLO).success)
      return false;

    _LRWLOG_("setting SPI to 2Mbps");
    linkSPI.activate(LinkSPI::Mode::MASTER_2MBPS);
    _LRWLOG_("state = AUTHENTICATED");
    state = State::AUTHENTICATED;

    return true;
  }

  void pingAdapter() {
    linkGPIO.reset();
    _LRWLOG_("setting SO as OUTPUT");
    linkGPIO.setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    _LRWLOG_("setting SD as OUTPUT");
    linkGPIO.setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    _LRWLOG_("setting SD = HIGH");
    linkGPIO.writePin(LinkGPIO::Pin::SD, true);
    Link::wait(PING_WAIT);
    _LRWLOG_("setting SD = LOW");
    linkGPIO.writePin(LinkGPIO::Pin::SD, false);
  }

  bool login() {
    LoginMemory memory;

    _LRWLOG_("sending initial login packet");
    if (!exchangeLoginPacket(LOGIN_PARTS[0], 0, memory))
      return false;

    for (u32 i = 0; i < LOGIN_STEPS; i++) {
      _LRWLOG_("sending login packet " + std::to_string(i + 1) + "/" +
               std::to_string(LOGIN_STEPS));
      if (!exchangeLoginPacket(LOGIN_PARTS[i], LOGIN_PARTS[i], memory))
        return false;
    }

    return true;
  }

  bool exchangeLoginPacket(u16 data,
                           u16 expectedResponse,
                           LoginMemory& memory) {
    u32 packet = Link::buildU32(~memory.previousAdapterData, data);
    u32 response = transfer(packet, false);

    if (Link::msB32(response) != expectedResponse ||
        Link::lsB32(response) != (u16)~memory.previousGBAData) {
      logExpectedButReceived(
          Link::buildU32(expectedResponse, (u16)~memory.previousGBAData),
          response);
      return false;
    }

    memory.previousGBAData = data;
    memory.previousAdapterData = expectedResponse;

    return true;
  }

  u32 buildCommand(u8 type, u8 length = 0) {
    return Link::buildU32(COMMAND_HEADER_VALUE, Link::buildU16(length, type));
  }

  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      Link::wait(TRANSFER_WAIT);

    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    u32 receivedData = linkSPI.transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, customAck);

    if (customAck && !acknowledge())
      return LINK_SPI_NO_DATA_32;

    return receivedData;
  }

  bool acknowledge() {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    linkSPI._setSOLow();
    while (!linkSPI._isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        _LRWLOG_("! ACK 1 failed. I put SO=LOW,");
        _LRWLOG_("! but SI didn't become HIGH.");
        return false;
      }
    }
    linkSPI._setSOHigh();
    while (linkSPI._isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        _LRWLOG_("! ACK 2 failed. I put SO=HIGH,");
        _LRWLOG_("! but SI didn't become LOW.");
        return false;
      }
    }
    linkSPI._setSOLow();

    return true;
  }

  bool reverseAcknowledge(bool isLastPart = false) {
    // `isLastPart` is required when there's no subsequent
    // `linkSPI.transfer(...)` call.
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    linkSPI._setSOLow();
    while (linkSPI._isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        _LRWLOG_("! RevAck0 failed. I put SO=LOW,");
        _LRWLOG_("! but SI didn't become LOW.");
        return false;
      }
    }

    linkSPI._setSOHigh();
    while (!linkSPI._isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        _LRWLOG_("! RevAck1 failed. I put SO=HIGH,");
        _LRWLOG_("! but SI didn't become HIGH.");
        return false;
      }
    }

    Link::wait(MICRO_WAIT);  // this wait is VERY important to avoid desyncs!
    // wait at least 40us; monitoring VCOUNT to avoid requiring a timer

    // (normally, this occurs on the next linkSPI.transfer(...) call)
    if (isLastPart) {
      linkSPI._setSOLow();
      while (linkSPI._isSIHigh()) {
        if (cmdTimeout(lines, vCount)) {
          _LRWLOG_("! RevAck2 failed. I put SO=LOW,");
          _LRWLOG_("! but SI didn't become LOW.");
          return false;
        }
      }
    }

    return true;
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

  LINK_INLINE void sendAsyncCommand(
      u32 newData,
      bool _clockInversionSupport = true) {  // (irq only)
    switch (asyncCommand.step) {
      case AsyncCommand::Step::COMMAND_HEADER: {
        if (newData != DATA_REQUEST_VALUE) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendParametersOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::COMMAND_PARAMETERS: {
        if (newData != DATA_REQUEST_VALUE) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendParametersOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::RESPONSE_REQUEST: {
        u16 header = Link::msB32(newData);
        u16 data = Link::lsB32(newData);
        u8 responses = Link::msB16(data);
        u8 ack = Link::lsB16(data);

        if (header != COMMAND_HEADER_VALUE ||
            ack != asyncCommand.type + RESPONSE_ACK ||
            responses > LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH) {
          if (header != COMMAND_HEADER_VALUE) {
            _LRWLOG_("! expected HEADER 0x9966");
            _LRWLOG_("! but received 0x" + toHex(header));
          }
          if (ack != asyncCommand.type + RESPONSE_ACK) {
            if (ack == 0xEE) {
              _LRWLOG_("! error received");
            } else {
              _LRWLOG_("! expected ACK 0x" +
                       toHex(asyncCommand.type + RESPONSE_ACK));
              _LRWLOG_("! but received 0x" + toHex(ack));
            }
          }

          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        _LRWLOG_("ack ok! " + std::to_string(responses) + " responses");

        asyncCommand.totalResponses = responses;
        asyncCommand.result.dataSize = responses;

        receiveResponseOrFinish(_clockInversionSupport);
        break;
      }
      case AsyncCommand::Step::DATA_REQUEST: {
        _LRWLOG_("response " +
                 std::to_string(asyncCommand.receivedResponses + 1) + "/" +
                 std::to_string(asyncCommand.totalResponses) + ":");
        _LRWLOG_("<< " + toHex(newData));

        asyncCommand.result.data[asyncCommand.receivedResponses] = newData;
        asyncCommand.receivedResponses++;

        receiveResponseOrFinish(_clockInversionSupport);
        break;
      }
      default: {
      }
    }
  }

  void sendParametersOrRequestResponse() {  // (irq only)
    if (asyncCommand.sentParameters < asyncCommand.totalParameters) {
      asyncCommand.step = AsyncCommand::Step::COMMAND_PARAMETERS;
      _LRWLOG_("sending param" + std::to_string(asyncCommand.sentParameters) +
               ": 0x" +
               toHex(asyncCommand.parameters[asyncCommand.sentParameters]));
      transferAsync(asyncCommand.parameters[asyncCommand.sentParameters], true);
      asyncCommand.sentParameters++;
    } else {
      _LRWLOG_("sending response request");
      asyncCommand.step = AsyncCommand::Step::RESPONSE_REQUEST;
      transferAsync(DATA_REQUEST_VALUE, true);
    }
  }

  void receiveResponseOrFinish(
      bool _clockInversionSupport = true) {  // (irq only)
    if (asyncCommand.receivedResponses < asyncCommand.totalResponses) {
      asyncCommand.step = AsyncCommand::Step::DATA_REQUEST;
      transferAsync(DATA_REQUEST_VALUE, true);
    } else {
      if (_clockInversionSupport && asyncCommand.invertsClock) {
        _LRWLOG_("setting SPI to SLAVE");
        linkSPI.activate(LinkSPI::Mode::SLAVE);

        asyncCommand.type = 0;
        asyncCommand.invertsClock = true;
        asyncCommand.direction = AsyncCommand::Direction::RECEIVING;
        asyncCommand.result = CommandResult{};
        asyncCommand.state = AsyncCommand::State::PENDING;
        asyncCommand.step = AsyncCommand::Step::COMMAND_HEADER;
        asyncCommand.sentParameters = 0;
        asyncCommand.totalParameters = 0;
        asyncCommand.receivedResponses = 0;
        asyncCommand.totalResponses = 0;

        _LRWLOG_("WAITING for adapter cmd");
        transferAsync(DATA_REQUEST_VALUE, true);
      } else {
        asyncCommand.result.success = true;
        asyncCommand.state = AsyncCommand::State::COMPLETED;
      }
    }
  }

  LINK_INLINE void receiveAsyncCommand(u32 newData) {  // (irq only)
    switch (asyncCommand.step) {
      case AsyncCommand::Step::COMMAND_HEADER: {
        u16 header = Link::msB32(newData);
        u16 data = Link::lsB32(newData);
        u8 params = Link::msB16(data);
        u8 commandId = Link::lsB16(data);

        if (header != COMMAND_HEADER_VALUE) {
          _LRWLOG_("! expected HEADER 0x9966");
          _LRWLOG_("! but received 0x" + toHex(header));
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }
        _LRWLOG_("received cmd: " + toHex(commandId) + " (" +
                 std::to_string(params) + " params)");

        asyncCommand.type = commandId;
        asyncCommand.result.commandId = asyncCommand.type;
        asyncCommand.result.dataSize = params;

        if (params > 0) {
          asyncCommand.step = AsyncCommand::Step::COMMAND_PARAMETERS;
          _LRWLOG_("param 1/" + std::to_string(params) + ":");
          transferAsync(DATA_REQUEST_VALUE, true);
        } else {
          acknowledgeRemoteCommand();
        }

        break;
      }
      case AsyncCommand::Step::COMMAND_PARAMETERS: {
        asyncCommand.result.data[asyncCommand.sentParameters++] = newData;

        _LRWLOG_("param " + std::to_string(asyncCommand.sentParameters + 1) +
                 "/" + std::to_string(asyncCommand.totalParameters) + ":");
        _LRWLOG_("<< " + toHex(newData));

        if (asyncCommand.sentParameters < asyncCommand.result.dataSize)
          transferAsync(DATA_REQUEST_VALUE, true);
        else
          acknowledgeRemoteCommand();

        break;
      }
      case AsyncCommand::Step::RESPONSE_REQUEST: {
        break;  // (unused)
      }
      case AsyncCommand::Step::DATA_REQUEST: {
        if (newData != DATA_REQUEST_VALUE) {
          _LRWLOG_("! expected CMD request");
          _LRWLOG_("! but received 0x" + toHex(newData));
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        _LRWLOG_("setting SPI to MASTER");
        linkSPI.activate(LinkSPI::Mode::MASTER_2MBPS);
        asyncCommand.result.success = true;
        asyncCommand.state = AsyncCommand::State::COMPLETED;
        asyncState = AsyncState::READY;
      }
    }
  }

  void acknowledgeRemoteCommand() {  // (irq only)
    _LRWLOG_("sending ack");
    asyncCommand.step = AsyncCommand::Step::DATA_REQUEST;
    u32 ack = (COMMAND_HEADER_VALUE << 16) |
              ((asyncCommand.type + RESPONSE_ACK) & 0xFF);
    transferAsync(ack, true);
  }

  void transferAsync(u32 data, bool fromIRQ) {
#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    if (fromIRQ)
      Link::_REG_IME = 0;
#endif
#endif

    linkSPI.transfer(data, []() { return false; }, true, true);
  }

  void logExpectedButReceived(u32 expected, u32 received) {
    _LRWLOG_("! expected 0x" + toHex(expected));
    _LRWLOG_("! but received 0x" + toHex(received));
  }
};

extern LinkRawWireless* linkRawWireless;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_RAW_WIRELESS_ISR_SERIAL() {
  linkRawWireless->_onSerial();
}

#undef _LRWLOG_

#endif  // LINK_RAW_WIRELESS_H
