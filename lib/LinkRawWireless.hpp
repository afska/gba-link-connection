#ifndef LINK_RAW_WIRELESS_H
#define LINK_RAW_WIRELESS_H

// --------------------------------------------------------------------------
// A low level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// - Advanced usage only (check out the documentation).
// - If you're building a game, use `LinkWireless`.
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <tonc_math.h>
#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

#include <string>
#include <vector>

#define LINK_RAW_WIRELESS_MAX_PLAYERS 5
#define LINK_RAW_WIRELESS_MIN_PLAYERS 2
#define LINK_RAW_WIRELESS_END 0
#define LINK_RAW_WIRELESS_PING_WAIT 50
#define LINK_RAW_WIRELESS_TRANSFER_WAIT 15
#define LINK_RAW_WIRELESS_CMD_TIMEOUT 100
#define LINK_RAW_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#define LINK_RAW_WIRELESS_MAX_GAME_ID 0x7fff
#define LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_RAW_WIRELESS_LOGIN_STEPS 9
#define LINK_RAW_WIRELESS_COMMAND_HEADER 0x9966
#define LINK_RAW_WIRELESS_RESPONSE_ACK 0x80
#define LINK_RAW_WIRELESS_DATA_REQUEST 0x80000000
#define LINK_RAW_WIRELESS_SETUP_MAGIC 0x003c0420
#define LINK_RAW_WIRELESS_SETUP_MAX_PLAYERS_BIT 16
#define LINK_RAW_WIRELESS_STILL_CONNECTING 0x01000000
#define LINK_RAW_WIRELESS_BROADCAST_LENGTH 6
#define LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH \
  (1 + LINK_RAW_WIRELESS_BROADCAST_LENGTH)
#define LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 22
#define LINK_RAW_WIRELESS_COMMAND_HELLO 0x10
#define LINK_RAW_WIRELESS_COMMAND_SETUP 0x17
#define LINK_RAW_WIRELESS_COMMAND_BROADCAST 0x16
#define LINK_RAW_WIRELESS_COMMAND_START_HOST 0x19
#define LINK_RAW_WIRELESS_COMMAND_ACCEPT_CONNECTIONS 0x1a
#define LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_START 0x1c
#define LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_POLL 0x1d
#define LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_END 0x1e
#define LINK_RAW_WIRELESS_COMMAND_CONNECT 0x1f
#define LINK_RAW_WIRELESS_COMMAND_IS_FINISHED_CONNECT 0x20
#define LINK_RAW_WIRELESS_COMMAND_FINISH_CONNECTION 0x21
#define LINK_RAW_WIRELESS_COMMAND_SEND_DATA 0x24
#define LINK_RAW_WIRELESS_COMMAND_RECEIVE_DATA 0x26
#define LINK_RAW_WIRELESS_COMMAND_BYE 0x3d

#define LINK_RAW_WIRELESS_RESET_IF_NEEDED \
  if (!isEnabled)                         \
    return false;                         \
  if (state == NEEDS_RESET)               \
    if (!reset())                         \
      return false;

static volatile char LINK_RAW_WIRELESS_VERSION[] = "LinkRawWireless/v6.0.3";

const u16 LINK_RAW_WIRELESS_LOGIN_PARTS[] = {
    0x494e, 0x494e, 0x544e, 0x544e, 0x4e45, 0x4e45, 0x4f44, 0x4f44, 0x8001};
const u16 LINK_RAW_WIRELESS_TIMER_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1,
                                               IRQ_TIMER2, IRQ_TIMER3};

class LinkRawWireless {
 public:
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
    GAME_NAME_TOO_LONG = 2,
    USER_NAME_TOO_LONG = 3,
    // Communication errors
    COMMAND_FAILED = 5,
    WEIRD_PLAYER_ID = 6,
    ACKNOWLEDGE_FAILED = 9,
  };

  struct Server {
    u16 id = 0;
    u16 gameId;
    std::string gameName;
    std::string userName;
    u8 nextClientNumber;

    bool isFull() { return nextClientNumber == 0xff; }
  };

  bool isActive() { return isEnabled; }

  bool activate() {
    lastError = NONE;
    isEnabled = false;

    bool success = reset();

    isEnabled = true;
    return success;
  }

  bool deactivate() {
    bool success = sendCommand(LINK_RAW_WIRELESS_COMMAND_BYE).success;

    lastError = NONE;
    isEnabled = false;
    resetState();
    stop();

    return success;
  }

  bool serve(std::string gameName = "",
             std::string userName = "",
             u16 gameId = LINK_RAW_WIRELESS_MAX_GAME_ID) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED &&
        state != SERVING) {  // TODO: REMOVE THESE CHECKS
      lastError = WRONG_STATE;
      return false;
    }
    if (gameName.length() > LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = GAME_NAME_TOO_LONG;
      return false;
    }
    if (userName.length() > LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = USER_NAME_TOO_LONG;
      return false;
    }
    gameName.append(LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH - gameName.length(),
                    0);
    userName.append(LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH - userName.length(),
                    0);

    auto broadcast =
        std::vector<u32>{buildU32(buildU16(gameName[1], gameName[0]),
                                  gameId & LINK_RAW_WIRELESS_MAX_GAME_ID),
                         buildU32(buildU16(gameName[5], gameName[4]),
                                  buildU16(gameName[3], gameName[2])),
                         buildU32(buildU16(gameName[9], gameName[8]),
                                  buildU16(gameName[7], gameName[6])),
                         buildU32(buildU16(gameName[13], gameName[12]),
                                  buildU16(gameName[11], gameName[10])),
                         buildU32(buildU16(userName[3], userName[2]),
                                  buildU16(userName[1], userName[0])),
                         buildU32(buildU16(userName[7], userName[6]),
                                  buildU16(userName[5], userName[4]))};

    bool success =
        sendCommand(LINK_RAW_WIRELESS_COMMAND_BROADCAST, broadcast).success;

    if (state != SERVING) {
      success =
          success && sendCommand(LINK_RAW_WIRELESS_COMMAND_START_HOST).success;
    }

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    wait(LINK_RAW_WIRELESS_TRANSFER_WAIT);
    state = SERVING;

    return true;
  }

  bool acceptConnections(std::string& connectedIds) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING)
      return false;

    auto result = sendCommand(0x14);

    if (!result.success) {
      reset();
      return false;
    }

    connectedIds = "";
    for (auto& response : result.responses) {
      connectedIds += std::to_string(response) + ", ";
    }

    sessionState.playerCount = 1 + result.responses.size();

    return true;
  }

  bool getServersAsyncStart() {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success =
        sendCommand(LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_START).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = SEARCHING;

    return true;
  }

  bool getServersAsyncEnd(std::vector<Server>& servers) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != SEARCHING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result = sendCommand(LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_POLL);
    bool success1 =
        result.success &&
        result.responses.size() % LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH ==
            0;

    if (!success1) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    bool success2 =
        sendCommand(LINK_RAW_WIRELESS_COMMAND_BROADCAST_READ_END).success;

    if (!success2) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    u32 totalBroadcasts =
        result.responses.size() / LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH;

    for (u32 i = 0; i < totalBroadcasts; i++) {
      u32 start = LINK_RAW_WIRELESS_BROADCAST_RESPONSE_LENGTH * i;

      Server server;
      server.id = (u16)result.responses[start];
      server.gameId =
          result.responses[start + 1] & LINK_RAW_WIRELESS_MAX_GAME_ID;
      recoverName(server.gameName, result.responses[start + 1], false);
      recoverName(server.gameName, result.responses[start + 2]);
      recoverName(server.gameName, result.responses[start + 3]);
      recoverName(server.gameName, result.responses[start + 4]);
      recoverName(server.userName, result.responses[start + 5]);
      recoverName(server.userName, result.responses[start + 6]);
      server.nextClientNumber = (result.responses[start] >> 16) & 0xff;

      servers.push_back(server);
    }

    state = AUTHENTICATED;

    return true;
  }

  bool connect(u16 serverId) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success = sendCommand(LINK_RAW_WIRELESS_COMMAND_CONNECT,
                               std::vector<u32>{serverId})
                       .success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = CONNECTING;

    return true;
  }

  bool keepConnecting() {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != CONNECTING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result1 = sendCommand(LINK_RAW_WIRELESS_COMMAND_IS_FINISHED_CONNECT);
    if (!result1.success || result1.responses.size() == 0) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    if (result1.responses[0] == LINK_RAW_WIRELESS_STILL_CONNECTING)
      return true;

    u8 assignedPlayerId = 1 + (u8)msB32(result1.responses[0]);
    if (assignedPlayerId >= LINK_RAW_WIRELESS_MAX_PLAYERS) {
      reset();
      lastError = WEIRD_PLAYER_ID;
      return false;
    }

    auto result2 = sendCommand(LINK_RAW_WIRELESS_COMMAND_FINISH_CONNECTION);
    if (!result2.success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    sessionState.currentPlayerId = assignedPlayerId;
    state = CONNECTED;

    return true;
  }

  u32 getDeviceTransferLength() {
    return state == SERVING ? LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH
                            : LINK_RAW_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
  }

  State getState() { return state; }
  bool isConnected() { return sessionState.playerCount > 1; }
  bool isSessionActive() { return state == SERVING || state == CONNECTED; }
  u8 playerCount() { return sessionState.playerCount; }
  u8 currentPlayerId() { return sessionState.currentPlayerId; }
  Error getLastError(bool clear = true) {
    Error error = lastError;
    if (clear)
      lastError = NONE;
    return error;
  }

  ~LinkRawWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

 private:
  struct SessionState {
    u8 playerCount = 1;
    u8 currentPlayerId = 0;
    u8 maxPlayers = LINK_RAW_WIRELESS_MAX_PLAYERS;
  };

  struct LoginMemory {
    u16 previousGBAData = 0xffff;
    u16 previousAdapterData = 0xffff;
  };

  struct CommandResult {
    bool success = false;
    std::vector<u32> responses = std::vector<u32>{};
  };

  SessionState sessionState;
  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  Error lastError = NONE;
  volatile bool isEnabled = false;

  bool sendData(std::vector<u32> data) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if ((state != SERVING && state != CONNECTED) || data.size() == 0 ||
        data.size() > LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH)
      return false;

    u32 bytes = data.size() * 4;
    u32 header = sessionState.currentPlayerId == 0
                     ? bytes
                     : (bytes << (3 + sessionState.currentPlayerId * 5));
    data.insert(data.begin(), header);

    bool success =
        sendCommand(LINK_RAW_WIRELESS_COMMAND_SEND_DATA, data).success;

    if (!success) {
      reset();
      // TODO: ERRORS?
      return false;
    }

    return true;
  }

  bool receiveData(std::vector<u32>& data) {
    LINK_RAW_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED)
      return false;

    auto result = sendCommand(LINK_RAW_WIRELESS_COMMAND_RECEIVE_DATA);
    data = result.responses;

    if (!result.success) {
      reset();
      // TODO: ERRORS?
      return false;
    }

    if (data.size() > 0)  // TODO: DON'T STRIP HEADER?
      data.erase(data.begin());

    return true;
  }

  void recoverName(std::string& name,
                   u32 word,
                   bool includeFirstTwoBytes = true) {
    u32 character = 0;
    if (includeFirstTwoBytes) {
      character = lsB16(lsB32(word));
      if (character > 0)
        name.push_back(character);
      character = msB16(lsB32(word));
      if (character > 0)
        name.push_back(character);
    }
    character = lsB16(msB32(word));
    if (character > 0)
      name.push_back(character);
    character = msB16(msB32(word));
    if (character > 0)
      name.push_back(character);
  }

  bool reset() {
    resetState();
    stop();
    return start();
  }

  void resetState() {
    this->state = NEEDS_RESET;
    this->sessionState.playerCount = 1;
    this->sessionState.currentPlayerId = 0;
  }

  void stop() { linkSPI->deactivate(); }

  bool start() {
    pingAdapter();
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);

    if (!login())
      return false;

    wait(LINK_RAW_WIRELESS_TRANSFER_WAIT);

    if (!sendCommand(LINK_RAW_WIRELESS_COMMAND_HELLO).success)
      return false;

    if (!setup())
      return false;

    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
    state = AUTHENTICATED;

    return true;
  }

  void pingAdapter() {
    linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    linkGPIO->writePin(LinkGPIO::SD, true);
    wait(LINK_RAW_WIRELESS_PING_WAIT);
    linkGPIO->writePin(LinkGPIO::SD, false);
  }

  bool login() {
    LoginMemory memory;

    if (!exchangeLoginPacket(LINK_RAW_WIRELESS_LOGIN_PARTS[0], 0, memory))
      return false;

    for (u32 i = 0; i < LINK_RAW_WIRELESS_LOGIN_STEPS; i++) {
      if (!exchangeLoginPacket(LINK_RAW_WIRELESS_LOGIN_PARTS[i],
                               LINK_RAW_WIRELESS_LOGIN_PARTS[i], memory))
        return false;
    }

    return true;
  }

  bool exchangeLoginPacket(u16 data,
                           u16 expectedResponse,
                           LoginMemory& memory) {
    u32 packet = buildU32(~memory.previousAdapterData, data);
    u32 response = transfer(packet, false);

    if (msB32(response) != expectedResponse ||
        lsB32(response) != (u16)~memory.previousGBAData)
      return false;

    memory.previousGBAData = data;
    memory.previousAdapterData = expectedResponse;

    return true;
  }

  bool setup(u8 maxPlayers = LINK_RAW_WIRELESS_MAX_PLAYERS) {
    return sendCommand(
               LINK_RAW_WIRELESS_COMMAND_SETUP,
               std::vector<u32>{
                   (u32)(LINK_RAW_WIRELESS_SETUP_MAGIC |
                         (((LINK_RAW_WIRELESS_MAX_PLAYERS - maxPlayers) & 0b11)
                          << LINK_RAW_WIRELESS_SETUP_MAX_PLAYERS_BIT))})
        .success;
  }

  CommandResult sendCommand(u8 type,
                            std::vector<u32> params = std::vector<u32>{}) {
    CommandResult result;
    u16 length = params.size();
    u32 command = buildCommand(type, length);

    if (transfer(command) != LINK_RAW_WIRELESS_DATA_REQUEST)
      return result;

    for (auto& param : params) {
      if (transfer(param) != LINK_RAW_WIRELESS_DATA_REQUEST)
        return result;
    }

    u32 response = transfer(LINK_RAW_WIRELESS_DATA_REQUEST);
    u16 header = msB32(response);
    u16 data = lsB32(response);
    u8 responses = msB16(data);
    u8 ack = lsB16(data);

    if (header != LINK_RAW_WIRELESS_COMMAND_HEADER)
      return result;
    if (ack != type + LINK_RAW_WIRELESS_RESPONSE_ACK)
      return result;

    for (u32 i = 0; i < responses; i++)
      result.responses.push_back(transfer(LINK_RAW_WIRELESS_DATA_REQUEST));

    result.success = true;
    return result;
  }

  u32 buildCommand(u8 type, u8 length = 0) {
    return buildU32(LINK_RAW_WIRELESS_COMMAND_HEADER, buildU16(length, type));
  }

  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      wait(LINK_RAW_WIRELESS_TRANSFER_WAIT);

    u32 lines = 0;
    u32 vCount = REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, customAck);

    if (customAck && !acknowledge())
      return LINK_SPI_NO_DATA;

    return receivedData;
  }

  bool acknowledge() {
    u32 lines = 0;
    u32 vCount = REG_VCOUNT;

    linkSPI->_setSOLow();
    while (!linkSPI->_isSIHigh())
      if (cmdTimeout(lines, vCount))
        return false;
    linkSPI->_setSOHigh();
    while (linkSPI->_isSIHigh())
      if (cmdTimeout(lines, vCount))
        return false;
    linkSPI->_setSOLow();

    return true;
  }

  bool cmdTimeout(u32& lines, u32& vCount) {
    return timeout(LINK_RAW_WIRELESS_CMD_TIMEOUT, lines, vCount);
  }

  bool timeout(u32 limit, u32& lines, u32& vCount) {
    if (REG_VCOUNT != vCount) {
      lines += max((int)REG_VCOUNT - (int)vCount, 0);
      vCount = REG_VCOUNT;
    }

    return lines > limit;
  }

  void wait(u32 verticalLines) {
    u32 count = 0;
    u32 vCount = REG_VCOUNT;

    while (count < verticalLines) {
      if (REG_VCOUNT != vCount) {
        count++;
        vCount = REG_VCOUNT;
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

extern LinkRawWireless* linkRawWireless;

#endif  // LINK_RAW_WIRELESS_H
