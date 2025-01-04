#ifndef LINK_RAW_WIRELESS_H
#define LINK_RAW_WIRELESS_H

// --------------------------------------------------------------------------
// A low level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// - Advanced usage only!
// - If you're building a game, use `LinkWireless`.
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include <array>
#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

/**
 * @brief Enable logging.
 * \warning Set `linkRawWireless->logger` and uncomment to enable!
 */
// #define LINK_RAW_WIRELESS_ENABLE_LOGGING

static volatile char LINK_RAW_WIRELESS_VERSION[] = "LinkRawWireless/v7.1.0";

#define LINK_RAW_WIRELESS_MAX_PLAYERS 5
#define LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#define LINK_RAW_WIRELESS_MAX_GAME_ID 0x7fff
#define LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 23

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
#include <string>
#define LRWLOG(str) logger(str)
#else
#define LRWLOG(str)
#endif

/**
 * @brief A low level driver for the GBA Wireless Adapter.
 * \warning Advanced usage only!
 * \warning If you're building a game, use `LinkWireless`.
 */
class LinkRawWireless {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

 public:
  static constexpr int PING_WAIT = 50;
  static constexpr int TRANSFER_WAIT = 15;
  static constexpr int CMD_TIMEOUT = 10;
  static constexpr int LOGIN_STEPS = 9;
  static constexpr int COMMAND_HEADER = 0x9966;
  static constexpr int RESPONSE_ACK = 0x80;
  static constexpr u32 DATA_REQUEST = 0x80000000;
  static constexpr int SETUP_MAGIC = 0x003c0000;
  static constexpr int WAIT_STILL_CONNECTING = 0x01000000;
  static constexpr int BROADCAST_LENGTH = 6;
  static constexpr int BROADCAST_RESPONSE_LENGTH = 1 + BROADCAST_LENGTH;
  static constexpr int MAX_SERVERS =
      LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH / BROADCAST_RESPONSE_LENGTH;
  static constexpr int COMMAND_HELLO = 0x10;
  static constexpr int COMMAND_SETUP = 0x17;
  static constexpr int COMMAND_BROADCAST = 0x16;
  static constexpr int COMMAND_START_HOST = 0x19;
  static constexpr int COMMAND_SYSTEM_STATUS = 0x13;
  static constexpr int COMMAND_SLOT_STATUS = 0x14;
  static constexpr int COMMAND_ACCEPT_CONNECTIONS = 0x1a;
  static constexpr int COMMAND_END_HOST = 0x1b;
  static constexpr int COMMAND_BROADCAST_READ_START = 0x1c;
  static constexpr int COMMAND_BROADCAST_READ_POLL = 0x1d;
  static constexpr int COMMAND_BROADCAST_READ_END = 0x1e;
  static constexpr int COMMAND_CONNECT = 0x1f;
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

  static constexpr u16 LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                        0x4e45, 0x4f44, 0x4f44, 0x8001};

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  typedef void (*Logger)(std::string);
  Logger logger = [](std::string str) {};
#endif

  enum State {
    NEEDS_RESET,
    AUTHENTICATED,
    SEARCHING,
    SERVING,
    CONNECTING,
    CONNECTED
  };

  struct CommandResult {
    bool success = false;
    u32 responses[LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
    u32 responsesSize = 0;
  };

  struct RemoteCommand {
    bool success = false;
    u8 commandId = 0;
    u32 params[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    u32 paramsSize = 0;
  };

  struct Server {
    u16 id = 0;
    u16 gameId;
    char gameName[LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char userName[LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    u8 nextClientNumber;

    bool isFull() { return nextClientNumber == 0xff; }
  };

  struct ConnectedClient {
    u16 deviceId = 0;
    u8 clientNumber = 0;
  };

  struct SystemStatusResponse {
    u16 deviceId = 0;
    u8 currentPlayerId = 0;
    State adapterState = AUTHENTICATED;
    bool isServerClosed = false;
  };

  struct SlotStatusResponse {
    u8 nextClientNumber = 0;
    std::array<ConnectedClient, LINK_RAW_WIRELESS_MAX_PLAYERS>
        connectedClients = {};
    u32 connectedClientsSize = 0;
  };

  struct AcceptConnectionsResponse {
    std::array<ConnectedClient, LINK_RAW_WIRELESS_MAX_PLAYERS>
        connectedClients = {};
    u32 connectedClientsSize = 0;
  };

  struct BroadcastReadPollResponse {
    std::array<Server, MAX_SERVERS> servers;
    u32 serversSize = 0;
  };

  enum ConnectionPhase { STILL_CONNECTING, ERROR, SUCCESS };

  struct ConnectionStatus {
    ConnectionPhase phase = STILL_CONNECTING;
    u8 assignedClientNumber = 0;
  };

  struct ReceiveDataResponse {
    u32 sentBytes[LINK_RAW_WIRELESS_MAX_PLAYERS];
    u32 data[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
    u32 dataSize = 0;
  };

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   * Returns whether initialization was successful or not.
   */
  bool activate() {
    isEnabled = false;

    bool success = reset(true);

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

    resetState();

    LRWLOG("setting SPI to 2Mbps");
    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);

    SystemStatusResponse systemStatus;
    if (!getSystemStatus(systemStatus)) {
      deactivate();
      return false;
    }

    if (systemStatus.adapterState == SERVING) {
      LRWLOG("restoring SERVING state");

      SlotStatusResponse slotStatus;
      if (!getSlotStatus(slotStatus)) {
        deactivate();
        return false;
      }

      state = SERVING;
      sessionState.isServerClosed = systemStatus.isServerClosed;
    } else if (systemStatus.adapterState == CONNECTED) {
      LRWLOG("restoring CONNECTED state");
      state = CONNECTED;
    } else {
      LRWLOG("! invalid adapter state");
      deactivate();
      return false;
    }

    sessionState.currentPlayerId = systemStatus.currentPlayerId;

    isEnabled = true;
    return true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    resetState();
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
   * yet. For now, it's magic (`0x003c0000`).
   */
  bool setup(u8 maxPlayers = LINK_RAW_WIRELESS_MAX_PLAYERS,
             u8 maxTransmissions = 4,
             u8 waitTimeout = 32,
             u32 magic = SETUP_MAGIC) {
    u32 config =
        (u32)(magic |
              (((LINK_RAW_WIRELESS_MAX_PLAYERS - maxPlayers) & 0b11) << 16) |
              (maxTransmissions << 8) | waitTimeout);
    return sendCommand(COMMAND_SETUP, {config}, 1).success;
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
        LINK_STRLEN(gameName) > LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH) {
      LRWLOG("! game name too long");
      return false;
    }
    if (_validateNames &&
        LINK_STRLEN(userName) > LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH) {
      LRWLOG("! user name too long");
      return false;
    }

    char finalGameName[LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char finalUserName[LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    copyName(finalGameName, gameName, LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH);
    copyName(finalUserName, userName, LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH);

    bool success =
        sendCommand(
            COMMAND_BROADCAST,
            {buildU32(buildU16(finalGameName[1], finalGameName[0]), gameId),
             buildU32(buildU16(finalGameName[5], finalGameName[4]),
                      buildU16(finalGameName[3], finalGameName[2])),
             buildU32(buildU16(finalGameName[9], finalGameName[8]),
                      buildU16(finalGameName[7], finalGameName[6])),
             buildU32(buildU16(finalGameName[13], finalGameName[12]),
                      buildU16(finalGameName[11], finalGameName[10])),
             buildU32(buildU16(finalUserName[3], finalUserName[2]),
                      buildU16(finalUserName[1], finalUserName[0])),
             buildU32(buildU16(finalUserName[7], finalUserName[6]),
                      buildU16(finalUserName[5], finalUserName[4]))},
            BROADCAST_LENGTH)
            .success;

    if (!success) {
      reset();
      return false;
    }

    return true;
  }

  /**
   * @brief Calls the StartHost (`0x19`) command.
   */
  bool startHost() {
    bool success = sendCommand(COMMAND_START_HOST).success;

    if (!success) {
      reset();
      return false;
    }

    Link::wait(TRANSFER_WAIT);
    LRWLOG("state = SERVING");
    state = SERVING;

    LRWLOG("server OPEN");
    sessionState.isServerClosed = false;

    return true;
  }

  /**
   * @brief Calls the SystemStatus (`0x13`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool getSystemStatus(SystemStatusResponse& response) {
    auto result = sendCommand(COMMAND_SYSTEM_STATUS);

    if (!result.success || result.responsesSize != 1) {
      reset();
      return false;
    }

    u32 status = result.responses[0];

    response.deviceId = lsB32(status);

    u8 slot = lsB16(msB32(status)) & 0b1111;
    response.currentPlayerId = slot == 0b0001   ? 1
                               : slot == 0b0010 ? 2
                               : slot == 0b0100 ? 3
                               : slot == 0b1000 ? 4
                                                : 0;

    u8 adapterState = msB16(msB32(status));
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
   * @brief Calls the SlotStatus (`0x14`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool getSlotStatus(SlotStatusResponse& response) {
    auto result = sendCommand(COMMAND_SLOT_STATUS);

    if (!result.success) {
      reset();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.responsesSize; i++) {
      if (i == 0) {
        response.nextClientNumber = (u8)lsB32(result.responses[i]);
      } else {
        response.connectedClients[response.connectedClientsSize++] =
            ConnectedClient{.deviceId = lsB32(result.responses[i]),
                            .clientNumber = (u8)msB32(result.responses[i])};
      }
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + response.connectedClientsSize;
    if (sessionState.playerCount != oldPlayerCount)
      LRWLOG("now: " + std::to_string(sessionState.playerCount) + " players");

    return true;
  }

  /**
   * @brief Calls the AcceptConnections (`0x1a`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool acceptConnections(AcceptConnectionsResponse& response) {
    auto result = sendCommand(COMMAND_ACCEPT_CONNECTIONS);

    if (!result.success) {
      reset();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.responsesSize; i++) {
      response.connectedClients[response.connectedClientsSize++] =
          ConnectedClient{.deviceId = lsB32(result.responses[i]),
                          .clientNumber = (u8)msB32(result.responses[i])};
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + result.responsesSize;
    if (sessionState.playerCount != oldPlayerCount)
      LRWLOG("now: " + std::to_string(sessionState.playerCount) + " players");

    return true;
  }

  /**
   * @brief Calls the EndHost (`0x1b`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool endHost(AcceptConnectionsResponse& response) {
    auto result = sendCommand(COMMAND_END_HOST);

    if (!result.success) {
      reset();
      return false;
    }

    response.connectedClientsSize = 0;
    for (u32 i = 0; i < result.responsesSize; i++) {
      response.connectedClients[response.connectedClientsSize++] =
          ConnectedClient{.deviceId = lsB32(result.responses[i]),
                          .clientNumber = (u8)msB32(result.responses[i])};
    }

    u8 oldPlayerCount = sessionState.playerCount;
    sessionState.playerCount = 1 + result.responsesSize;
    if (sessionState.playerCount != oldPlayerCount)
      LRWLOG("now: " + std::to_string(sessionState.playerCount) + " players");

    LRWLOG("server CLOSED");
    sessionState.isServerClosed = true;

    return true;
  }

  /**
   * @brief Calls the BroadcastRead1 (`0x1c`) command.
   */
  bool broadcastReadStart() {
    bool success = sendCommand(COMMAND_BROADCAST_READ_START).success;

    if (!success) {
      reset();
      return false;
    }

    LRWLOG("state = SEARCHING");
    state = SEARCHING;

    return true;
  }

  /**
   * @brief Calls the BroadcastRead2 (`0x1d`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool broadcastReadPoll(BroadcastReadPollResponse& response) {
    auto result = sendCommand(COMMAND_BROADCAST_READ_POLL);
    bool success =
        result.success && result.responsesSize % BROADCAST_RESPONSE_LENGTH == 0;

    if (!success) {
      reset();
      return false;
    }

    u32 totalBroadcasts = result.responsesSize / BROADCAST_RESPONSE_LENGTH;

    response.serversSize = 0;
    for (u32 i = 0; i < totalBroadcasts; i++) {
      u32 start = BROADCAST_RESPONSE_LENGTH * i;

      Server server;
      server.id = (u16)result.responses[start];
      server.gameId =
          result.responses[start + 1] & LINK_RAW_WIRELESS_MAX_GAME_ID;
      u32 gameI = 0, userI = 0;
      recoverName(server.gameName, gameI, result.responses[start + 1], false);
      recoverName(server.gameName, gameI, result.responses[start + 2]);
      recoverName(server.gameName, gameI, result.responses[start + 3]);
      recoverName(server.gameName, gameI, result.responses[start + 4]);
      recoverName(server.userName, userI, result.responses[start + 5]);
      recoverName(server.userName, userI, result.responses[start + 6]);
      server.gameName[gameI] = '\0';
      server.userName[userI] = '\0';
      server.nextClientNumber = (result.responses[start] >> 16) & 0xff;

      response.servers[response.serversSize++] = server;
    }

    return true;
  }

  /**
   * @brief Calls the BroadcastRead3 (`0x1e`) command.
   */
  bool broadcastReadEnd() {
    bool success = sendCommand(COMMAND_BROADCAST_READ_END).success;

    if (!success) {
      reset();
      return false;
    }

    LRWLOG("state = AUTHENTICATED");
    state = AUTHENTICATED;

    return true;
  }

  /**
   * @brief Calls the Connect (`0x1f`) command.
   * @param serverId Device ID of the server.
   */
  bool connect(u16 serverId) {
    bool success = sendCommand(COMMAND_CONNECT, {serverId}, 1).success;

    if (!success) {
      reset();
      return false;
    }

    LRWLOG("state = CONNECTING");
    state = CONNECTING;

    return true;
  }

  /**
   * @brief Calls the IsFinishedConnect (`0x20`) command.
   * @param response A structure that will be filled with the response data.
   */
  bool keepConnecting(ConnectionStatus& response) {
    auto result = sendCommand(COMMAND_IS_FINISHED_CONNECT);
    if (!result.success || result.responsesSize == 0) {
      if (result.responsesSize == 0)
        LRWLOG("! empty response");
      reset();
      return false;
    }

    if (result.responses[0] == WAIT_STILL_CONNECTING) {
      response.phase = STILL_CONNECTING;
      return true;
    }

    u8 assignedPlayerId = 1 + (u8)msB32(result.responses[0]);
    if (assignedPlayerId >= LINK_RAW_WIRELESS_MAX_PLAYERS) {
      LRWLOG("! connection failed (1)");
      reset();
      response.phase = ERROR;
      return false;
    }

    response.phase = SUCCESS;
    response.assignedClientNumber = (u8)msB32(result.responses[0]);

    return true;
  }

  /**
   * @brief Calls the FinishConnection (`0x21`) command.
   */
  bool finishConnection() {
    auto result = sendCommand(COMMAND_FINISH_CONNECTION);
    if (!result.success || result.responsesSize == 0) {
      if (result.responsesSize == 0)
        LRWLOG("! empty response");
      reset();
      return false;
    }

    u16 status = msB32(result.responses[0]);
    if ((msB16(status) & 1) == 1) {
      LRWLOG("! connection failed (2)");
      reset();
      return false;
    }

    u8 assignedPlayerId = 1 + (u8)status;
    sessionState.currentPlayerId = assignedPlayerId;
    LRWLOG("state = CONNECTED");
    state = CONNECTED;

    return true;
  }

  /**
   * @brief Calls the SendData (`0x24`) command.
   * @param data The values to be sent.
   * @param dataSize The number of 32-bit values in the `data` array.
   * @param _bytes The number of BYTES to send. If `0`, the method will use
   * `dataSize * 4` instead.
   */
  bool sendData(
      std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> data,
      u32 dataSize,
      u32 _bytes = 0) {
    u32 bytes = _bytes == 0 ? dataSize * 4 : _bytes;
    u32 header = sessionState.currentPlayerId == 0
                     ? bytes
                     : (bytes << (3 + sessionState.currentPlayerId * 5));
    for (u32 i = dataSize; i > 0; i--)
      data[i] = data[i - 1];
    data[0] = header;
    dataSize++;
    LRWLOG("using header " + toHex(header));

    bool success = sendCommand(COMMAND_SEND_DATA, data, dataSize).success;

    if (!success) {
      reset();
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
  bool sendDataAndWait(
      std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> data,
      u32 dataSize,
      RemoteCommand& remoteCommand,
      u32 _bytes = 0) {
    u32 bytes = _bytes == 0 ? dataSize * 4 : _bytes;
    u32 header = sessionState.currentPlayerId == 0
                     ? bytes
                     : (bytes << (3 + sessionState.currentPlayerId * 5));
    for (u32 i = dataSize; i > 0; i--)
      data[i] = data[i - 1];
    data[0] = header;
    dataSize++;
    LRWLOG("using header " + toHex(header));

    if (!sendCommand(COMMAND_SEND_DATA_AND_WAIT, data, dataSize, true)
             .success) {
      reset();
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
    for (u32 i = 0; i < result.responsesSize; i++)
      response.data[i] = result.responses[i];
    response.dataSize = result.responsesSize;

    if (!result.success) {
      reset();
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
   * @brief Calls the Wait (`0x27`) command.
   * @param remoteCommand A structure that will be filled with the remote
   * command from the adapter.
   */
  bool wait(RemoteCommand& remoteCommand) {
    if (!sendCommand(COMMAND_WAIT, {}, 0, true).success) {
      reset();
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
   * @brief Calls an arbitrary command and returns the response.
   * @param type The ID of the command.
   * @param params The command parameters.
   * @param length The number of 32-bit values in the `params` array.
   * @param invertsClock Whether this command inverts the clock or not (Wait).
   */
  CommandResult sendCommand(
      u8 type,
      std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> params =
          {},
      u16 length = 0,
      bool invertsClock = false) {
    CommandResult result;
    u32 command = buildCommand(type, length);
    u32 r;

    LRWLOG("sending command 0x" + toHex(command));
    if ((r = transfer(command)) != DATA_REQUEST) {
      logExpectedButReceived(DATA_REQUEST, r);
      return result;
    }

    u32 parameterCount = 0;
    for (u32 i = 0; i < length; i++) {
      u32 param = params[i];
      LRWLOG("sending param" + std::to_string(parameterCount) + ": 0x" +
             toHex(param));
      if ((r = transfer(param)) != DATA_REQUEST) {
        logExpectedButReceived(DATA_REQUEST, r);
        return result;
      }
      parameterCount++;
    }

    LRWLOG("sending response request");
    u32 response = invertsClock
                       ? transferAndStartClockInversionACK(DATA_REQUEST)
                       : transfer(DATA_REQUEST);
    u16 header = msB32(response);
    u16 data = lsB32(response);
    u8 responses = msB16(data);
    u8 ack = lsB16(data);

    if (header != COMMAND_HEADER) {
      LRWLOG("! expected HEADER 0x9966");
      LRWLOG("! but received 0x" + toHex(header));
      return result;
    }
    if (ack != type + RESPONSE_ACK) {
      if (ack == 0xee && responses == 1 && !invertsClock) {
        u8 __attribute__((unused)) code = (u8)transfer(DATA_REQUEST);
        LRWLOG("! error received");
        LRWLOG(code == 1 ? "! invalid state" : "! unknown cmd");
      } else {
        LRWLOG("! expected ACK 0x" + toHex(type + RESPONSE_ACK));
        LRWLOG("! but received 0x" + toHex(ack));
      }
      return result;
    }
    LRWLOG("ack ok! " + std::to_string(responses) + " responses");

    if (!invertsClock) {
      for (u32 i = 0; i < responses; i++) {
        LRWLOG("response " + std::to_string(i + 1) + "/" +
               std::to_string(responses) + ":");
        u32 responseData = transfer(DATA_REQUEST);
        result.responses[result.responsesSize++] = responseData;
        LRWLOG("<< " + toHex(responseData));
      }
    }

    result.success = true;
    return result;
  }

  /**
   * @brief Inverts the clock and waits until the adapter sends a command.
   * Returns the remote command.
   */
  RemoteCommand receiveCommandFromAdapter() {
    RemoteCommand remoteCommand;

    LRWLOG("setting SPI to SLAVE");
    linkSPI->activate(LinkSPI::Mode::SLAVE);

    LRWLOG("WAITING for adapter cmd");
    u32 command =
        linkSPI->transfer(DATA_REQUEST, []() { return false; }, false, true);
    if (!reverseAcknowledge()) {
      linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
      reset();
      return remoteCommand;
    }

    u16 header = msB32(command);
    u16 data = lsB32(command);
    u8 params = msB16(data);
    u8 commandId = lsB16(data);
    if (header != COMMAND_HEADER) {
      LRWLOG("! expected HEADER 0x9966");
      LRWLOG("! but received 0x" + toHex(header));
      linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
      reset();
      return remoteCommand;
    }
    LRWLOG("received cmd: " + toHex(commandId) + " (" + std::to_string(params) +
           " params)");

    for (u32 i = 0; i < params; i++) {
      LRWLOG("param " + std::to_string(i + 1) + "/" + std::to_string(params) +
             ":");
      u32 paramData =
          linkSPI->transfer(DATA_REQUEST, []() { return false; }, false, true);
      if (!reverseAcknowledge()) {
        linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
        reset();
        return remoteCommand;
      }
      remoteCommand.params[remoteCommand.paramsSize++] = paramData;
      LRWLOG("<< " + toHex(paramData));
    }

    Link::wait(TRANSFER_WAIT);

    LRWLOG("sending ack");
    command = linkSPI->transfer(
        (COMMAND_HEADER << 16) | ((commandId + RESPONSE_ACK) & 0xff),
        []() { return false; }, false, true);
    if (!reverseAcknowledge(true)) {
      linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
      reset();
      return remoteCommand;
    }

    if (command != DATA_REQUEST) {
      LRWLOG("! expected CMD request");
      LRWLOG("! but received 0x" + toHex(command));
      linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
      reset();
      return remoteCommand;
    }

    LRWLOG("setting SPI to MASTER");
    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);

    Link::wait(TRANSFER_WAIT);

    remoteCommand.success = true;
    remoteCommand.commandId = commandId;

    return remoteCommand;
  }

  /**
   * @brief Returns the maximum number of transferrable 32-bit values.
   * It's 23 for servers and 4 for clients.
   */
  [[nodiscard]] u32 getDeviceTransferLength() {
    return state == SERVING ? LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH
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
    return state == SERVING || state == CONNECTED;
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

  ~LinkRawWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

  struct SessionState {
    u8 playerCount = 1;
    u8 currentPlayerId = 0;
    bool isServerClosed = false;
  };

  struct LoginMemory {
    u16 previousGBAData = 0xffff;
    u16 previousAdapterData = 0xffff;
  };

  SessionState sessionState;
  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  volatile bool isEnabled = false;

  /**
   * @brief Copies a null-terminated `source` string to a `target` destination
   * (up to `length` characters).
   * @param target Target string.
   * @param source Source string.
   * @param length Number of characters.
   */
  void copyName(char* target, const char* source, u32 length) {
    u32 len = LINK_STRLEN(source);

    for (u32 i = 0; i < length + 1; i++)
      if (i < len)
        target[i] = source[i];
      else
        target[i] = '\0';
  }

  /**
   * @brief Recovers parts of the `name` of a Wireless Adapter room.
   * @param name Target string.
   * @param nameCursor Current position within `name`.
   * @param word Current value.
   * @param includeFirstTwoBytes Whether the first two bytes from `word` should
   * be used.
   */
  void recoverName(char* name,
                   u32& nameCursor,
                   u32 word,
                   bool includeFirstTwoBytes = true) {
    u32 character = 0;
    if (includeFirstTwoBytes) {
      character = lsB16(lsB32(word));
      if (character > 0)
        name[nameCursor++] = character;
      character = msB16(lsB32(word));
      if (character > 0)
        name[nameCursor++] = character;
    }
    character = lsB16(msB32(word));
    if (character > 0)
      name[nameCursor++] = character;
    character = msB16(msB32(word));
    if (character > 0)
      name[nameCursor++] = character;
  }

  /**
   * @brief Resets the adapter
   * @param initialize Whether it's an initialization (first time) or not.
   */
  bool reset(bool initialize = false) {
    resetState();
    if (initialize)
      stop();
    return initialize && start();
  }

  /**
   * @brief Resets all the state.
   */
  void resetState() {
    LRWLOG("state = NEEDS_RESET");
    this->state = NEEDS_RESET;
    this->sessionState.playerCount = 1;
    this->sessionState.currentPlayerId = 0;
    this->sessionState.isServerClosed = false;
  }

  /**
   * @brief Stops the communication.
   */
  void stop() { linkSPI->deactivate(); }

  /**
   * @brief Starts the communication.
   */
  bool start() {
    pingAdapter();
    LRWLOG("setting SPI to 256Kbps");
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);

    if (!login())
      return false;

    Link::wait(TRANSFER_WAIT);

    LRWLOG("sending HELLO command");
    if (!sendCommand(COMMAND_HELLO).success)
      return false;

    LRWLOG("setting SPI to 2Mbps");
    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
    LRWLOG("state = AUTHENTICATED");
    state = AUTHENTICATED;

    return true;
  }

  /**
   * @brief Sends the signal to reset the adapter.
   */
  void pingAdapter() {
    linkGPIO->reset();
    LRWLOG("setting SO as OUTPUT");
    linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    LRWLOG("setting SD as OUTPUT");
    linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    LRWLOG("setting SD = HIGH");
    linkGPIO->writePin(LinkGPIO::Pin::SD, true);
    Link::wait(PING_WAIT);
    LRWLOG("setting SD = LOW");
    linkGPIO->writePin(LinkGPIO::Pin::SD, false);
  }

  /**
   * @brief Sends the login sequence to the adapter.
   */
  bool login() {
    LoginMemory memory;

    LRWLOG("sending initial login packet");
    if (!exchangeLoginPacket(LOGIN_PARTS[0], 0, memory))
      return false;

    for (u32 i = 0; i < LOGIN_STEPS; i++) {
      LRWLOG("sending login packet " + std::to_string(i + 1) + "/" +
             std::to_string(LOGIN_STEPS));
      if (!exchangeLoginPacket(LOGIN_PARTS[i], LOGIN_PARTS[i], memory))
        return false;
    }

    return true;
  }

  /**
   * @brief Exchanges part of the login sequence with the adapter.
   * @param data The value to be sent.
   * @param expectedResponse The expected response.
   * @param memory A structure that holds memory of the previous values.
   */
  bool exchangeLoginPacket(u16 data,
                           u16 expectedResponse,
                           LoginMemory& memory) {
    u32 packet = buildU32(~memory.previousAdapterData, data);
    u32 response = transfer(packet, false);

    if (msB32(response) != expectedResponse ||
        lsB32(response) != (u16)~memory.previousGBAData) {
      logExpectedButReceived(
          buildU32(expectedResponse, (u16)~memory.previousGBAData), response);
      return false;
    }

    memory.previousGBAData = data;
    memory.previousAdapterData = expectedResponse;

    return true;
  }

  /**
   * @brief Builds a 32-bit value representing the command.
   * @param type The ID of the command.
   * @param length The number of 32-bit values that will be sent.
   */
  u32 buildCommand(u8 type, u8 length = 0) {
    return buildU32(COMMAND_HEADER, buildU16(length, type));
  }

  /**
   * @brief Transfers `data` via SPI and performs the adapter's ACK procedure.
   * Returns the received value.
   * @param data The value to be sent.
   * @param customAck Whether the adapter's ACK procedure should be used or not.
   */
  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      Link::wait(TRANSFER_WAIT);

    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, customAck);

    if (customAck && !acknowledge())
      return LINK_SPI_NO_DATA_32;

    return receivedData;
  }

  /**
   * @brief Transfers `data` via SPI and performs the inverted adapter's ACK
   * procedure. Returns the received value.
   * @param data The value to be sent.
   */
  u32 transferAndStartClockInversionACK(u32 data) {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, true);

    if (!reverseAcknowledgeStart())
      return LINK_SPI_NO_DATA_32;

    return receivedData;
  }

  /**
   * @brief Performs the adapter's ACK procedure.
   */
  bool acknowledge() {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    linkSPI->_setSOLow();
    while (!linkSPI->_isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        LRWLOG("! ACK 1 failed. I put SO=LOW,");
        LRWLOG("! but SI didn't become HIGH.");
        return false;
      }
    }
    linkSPI->_setSOHigh();
    while (linkSPI->_isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        LRWLOG("! ACK 2 failed. I put SO=HIGH,");
        LRWLOG("! but SI didn't become LOW.");
        return false;
      }
    }
    linkSPI->_setSOLow();

    return true;
  }

  /**
   * @brief Starts performing the inverted adapter's ACK procedure.
   */
  bool reverseAcknowledgeStart() {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    linkSPI->_setSOLow();
    Link::wait(1);
    linkSPI->_setSOHigh();
    while (linkSPI->_isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        LRWLOG("! Rev0 failed. I put SO=HIGH,");
        LRWLOG("! but SI didn't become LOW.");
        return false;
      }
    }
    linkSPI->_setSOLow();

    return true;
  }

  /**
   * @brief Performs the inverted adapter's ACK procedure.
   * @param isLastPart Whether it's the last part of the procedure or not.
   * \warning `isLastPart` is required when there's no subsequent
   * `linkSPI->transfer(...)` call.
   */
  bool reverseAcknowledge(bool isLastPart = false) {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

    linkSPI->_setSOLow();
    while (linkSPI->_isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        LRWLOG("! RevAck0 failed. I put SO=LOW,");
        LRWLOG("! but SI didn't become LOW.");
        return false;
      }
    }

    linkSPI->_setSOHigh();
    while (!linkSPI->_isSIHigh()) {
      if (cmdTimeout(lines, vCount)) {
        LRWLOG("! RevAck1 failed. I put SO=HIGH,");
        LRWLOG("! but SI didn't become HIGH.");
        return false;
      }
    }
    // (normally, this occurs on the next linkSPI->transfer(...) call)
    if (isLastPart) {
      linkSPI->_setSOLow();
      while (linkSPI->_isSIHigh()) {
        if (cmdTimeout(lines, vCount)) {
          LRWLOG("! RevAck2 failed. I put SO=LOW,");
          LRWLOG("! but SI didn't become LOW.");
          return false;
        }
      }
    }

    return true;
  }

  /**
   * @brief Evaluates a timeout defined by `CMD_TIMEOUT`.
   * @param lines A line counter that will be updated.
   * @param vCount Starting `VCOUNT`.
   */
  bool cmdTimeout(u32& lines, u32& vCount) {
    return timeout(CMD_TIMEOUT, lines, vCount);
  }

  /**
   * @brief Evaluates a timeout defined by `limit`.
   * @param limit Maximum number of lines to wait.
   * @param lines A line counter that will be updated.
   * @param vCount Starting `VCOUNT`.
   */
  bool timeout(u32 limit, u32& lines, u32& vCount) {
    if (Link::_REG_VCOUNT != vCount) {
      lines += Link::_max((int)Link::_REG_VCOUNT - (int)vCount, 0);
      vCount = Link::_REG_VCOUNT;
    }

    return lines > limit;
  }

  /**
   * @brief Logs an error message (expected vs received).
   * @param expected The expected number.
   * @param received The received number.
   */
  void logExpectedButReceived(u32 expected, u32 received) {
    LRWLOG("! expected 0x" + toHex(expected));
    LRWLOG("! but received 0x" + toHex(received));
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
      rc[i] = digits[(w >> j) & 0x0f];
    return rc;
  }
#endif

  /**
   * @brief Builds a u32 number from `msB` and `lsB`
   */
  [[nodiscard]] u32 buildU32(u16 msB, u16 lsB) { return (msB << 16) | lsB; }

  /**
   * @brief Builds a u16 number from `msB` and `lsB`
   */
  [[nodiscard]] u16 buildU16(u8 msB, u8 lsB) { return (msB << 8) | lsB; }

  /**
   * @brief Returns the higher 16 bits of `value`.
   */
  [[nodiscard]] u16 msB32(u32 value) { return value >> 16; }

  /**
   * @brief Returns the lower 16 bits of `value`.
   */
  [[nodiscard]] u16 lsB32(u32 value) { return value & 0xffff; }

  /**
   * @brief Returns the higher 8 bits of `value`.
   */
  [[nodiscard]] u8 msB16(u16 value) { return value >> 8; }

  /**
   * @brief Returns the lower 8 bits of `value`.
   */
  [[nodiscard]] u8 lsB16(u16 value) { return value & 0xff; }
};

extern LinkRawWireless* linkRawWireless;

#undef LRWLOG

#endif  // LINK_RAW_WIRELESS_H
