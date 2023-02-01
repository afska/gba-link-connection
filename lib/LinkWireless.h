#ifndef LINK_WIRELESS_H
#define LINK_WIRELESS_H

// --------------------------------------------------------------------------
// A high level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWireless* linkWireless = new LinkWireless();
// - 2) Initialize the library with:
//       linkWireless->activate();
// - 3) Start a server:
//       linkWireless->serve();
//       // `getState()` should return SERVING now...
//       // `getPlayerId()` should return 0
//       // `getPlayerCount()` should reflect the number of active consoles
//       // call `acceptConnections()` periodically
// - 4) Connect to a server:
//       std::vector<u16> serverIds;
//       linkWireless->getServerIds(serverIds);
//       linkWireless->connect(serverIds[0]);
//       while (linkWireless->getState() == LinkWireless::State::CONNECTING)
//         linkWireless->keepConnecting();
//       // `getState()` should return CONNECTED now...
//       // `getPlayerId()` should return 1, 2, 3, or 4 (the host is 0)
// - 5) Send data:
//       linkConnection->sendData(std::vector<u32>{1, 2, 3});
// - 6) Receive data:
//       std::vector<u32> data;
//       linkConnection->receiveData(data);
//       if (data.size() > 0) {
//         // ...
//       }
// - 7) Disconnect:
//       linkWireless->disconnect();
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <vector>
#include "LinkGPIO.h"
#include "LinkSPI.h"

#define LINK_WIRELESS_PING_WAIT 50
#define LINK_WIRELESS_TRANSFER_WAIT 15
#define LINK_WIRELESS_BROADCAST_SEARCH_WAIT ((160 + 68) * 60)
#define LINK_WIRELESS_TIMEOUT 100
#define LINK_WIRELESS_MAX_PLAYERS 5
#define LINK_WIRELESS_MAX_TRANSFER_LENGTH 20
#define LINK_WIRELESS_LOGIN_STEPS 9
#define LINK_WIRELESS_COMMAND_HEADER 0x9966
#define LINK_WIRELESS_RESPONSE_ACK 0x80
#define LINK_WIRELESS_DATA_REQUEST 0x80000000
#define LINK_WIRELESS_SETUP_MAGIC 0x003c0420
#define LINK_WIRELESS_STILL_CONNECTING 0x01000000
#define LINK_WIRELESS_BROADCAST_LENGTH 6
#define LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH \
  (1 + LINK_WIRELESS_BROADCAST_LENGTH)
#define LINK_WIRELESS_COMMAND_HELLO 0x10
#define LINK_WIRELESS_COMMAND_SETUP 0x17
#define LINK_WIRELESS_COMMAND_BROADCAST 0x16
#define LINK_WIRELESS_COMMAND_START_HOST 0x19
#define LINK_WIRELESS_COMMAND_ACCEPT_CONNECTIONS 0x1a
#define LINK_WIRELESS_COMMAND_BROADCAST_READ_START 0x1c
#define LINK_WIRELESS_COMMAND_BROADCAST_READ_END 0x1e
#define LINK_WIRELESS_COMMAND_CONNECT 0x1f
#define LINK_WIRELESS_COMMAND_IS_FINISHED_CONNECT 0x20
#define LINK_WIRELESS_COMMAND_FINISH_CONNECTION 0x21
#define LINK_WIRELESS_COMMAND_SEND_DATA 0x24
#define LINK_WIRELESS_COMMAND_RECEIVE_DATA 0x26
#define LINK_WIRELESS_COMMAND_DISCONNECT 0x30

#define LINK_WIRELESS_RESET_IF_NEEDED \
  if (state == NEEDS_RESET)           \
    reset();

const u16 LINK_WIRELESS_LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                         0x4e45, 0x4f44, 0x4f44, 0x8001};

class LinkWireless {
 public:
  enum State { NEEDS_RESET, AUTHENTICATED, SERVING, CONNECTING, CONNECTED };

  bool isActive() { return isEnabled; }

  bool activate() {
    bool success = reset();

    isEnabled = true;
    return success;
  }

  void deactivate() {
    isEnabled = false;
    stop();
  }

  bool serve() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED)
      return false;

    auto broadcast = std::vector<u32>{1, 2, 3, 4, 5, 6};
    bool success =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST, broadcast).success &&
        sendCommand(LINK_WIRELESS_COMMAND_START_HOST).success;

    if (!success) {
      reset();
      return false;
    }

    state = SERVING;

    return true;
  }

  bool acceptConnections() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING)
      return false;

    auto result = sendCommand(LINK_WIRELESS_COMMAND_ACCEPT_CONNECTIONS);

    if (!result.success) {
      reset();
      return false;
    }

    playerCount = 1 + result.responses.size();

    return true;
  }

  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED)
      return false;

    bool success =
        sendCommand(LINK_WIRELESS_COMMAND_CONNECT, std::vector<u32>{serverId})
            .success;

    if (!success) {
      reset();
      return false;
    }

    state = CONNECTING;

    return true;
  }

  bool keepConnecting() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != CONNECTING)
      return false;

    auto result1 = sendCommand(LINK_WIRELESS_COMMAND_IS_FINISHED_CONNECT);
    if (!result1.success || result1.responses.size() == 0) {
      reset();
      return false;
    }

    if (result1.responses[0] == LINK_WIRELESS_STILL_CONNECTING)
      return true;

    u8 assignedPlayerId = 1 + (u8)msB32(result1.responses[0]);
    u16 assignedClientId = (u16)result1.responses[0];

    if (assignedPlayerId >= LINK_WIRELESS_MAX_PLAYERS) {
      reset();
      return false;
    }

    auto result2 = sendCommand(LINK_WIRELESS_COMMAND_FINISH_CONNECTION);
    if (!result2.success || result2.responses.size() == 0 ||
        (u16)result2.responses[0] != assignedClientId) {
      reset();
      return false;
    }

    playerId = assignedPlayerId;
    state = CONNECTED;

    return true;
  }

  bool sendData(std::vector<u32> data) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if ((state != SERVING && state != CONNECTED) ||
        data.size() > LINK_WIRELESS_MAX_TRANSFER_LENGTH)
      return false;

    u32 bytes = data.size() * 4;
    auto dataWithHeader = std::vector<u32>{
        playerId == 0 ? bytes : (1 << (3 + playerId * 5)) * bytes};
    dataWithHeader.insert(dataWithHeader.end(), data.begin(), data.end());

    bool success =
        sendCommand(LINK_WIRELESS_COMMAND_SEND_DATA, dataWithHeader).success;

    if (!success) {
      reset();
      return false;
    }

    return true;
  }

  // TODO: CHECK MULTIPLE MESSAGES
  bool receiveData(std::vector<u32>& data) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED)
      return false;

    auto result = sendCommand(LINK_WIRELESS_COMMAND_RECEIVE_DATA);
    data = result.responses;

    if (!result.success) {
      reset();
      return false;
    }

    if (data.size() > 0)
      data.erase(data.begin());

    return true;
  }

  bool getServerIds(std::vector<u16>& serverIds) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED)
      return false;

    bool success1 =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_START).success;

    if (!success1) {
      reset();
      return false;
    }

    wait(LINK_WIRELESS_BROADCAST_SEARCH_WAIT);

    auto result = sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_END);
    bool success2 =
        result.success &&
        result.responses.size() % LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH == 0;

    if (!success2) {
      reset();
      return false;
    }

    u32 totalBroadcasts =
        result.responses.size() / LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH;

    for (u32 i = 0; i < totalBroadcasts; i++) {
      serverIds.push_back(
          (u16)result.responses[LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH * i]);
    }

    return true;
  }

  // TODO: TEST AND UNHARDCODE
  bool getSignalLevel(std::vector<u32>& levels) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED)
      return false;

    auto result = sendCommand(0x11);
    if (result.success)
      levels = result.responses;
    return result.success;
  }

  bool disconnect() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED)
      return false;

    bool success = sendCommand(LINK_WIRELESS_COMMAND_DISCONNECT).success;

    if (!success) {
      reset();
      return false;
    }

    reset();

    return true;
  }

  State getState() { return state; }
  u8 getPlayerId() { return playerId; }
  u8 getPlayerCount() { return playerCount; }

  ~LinkWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

 private:
  struct LoginMemory {
    u16 previousGBAData = 0xffff;
    u16 previousAdapterData = 0xffff;
  };

  struct CommandResult {
    bool success = false;
    std::vector<u32> responses = std::vector<u32>{};
  };

  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  u8 playerId = 0;
  u8 playerCount = 1;
  bool isEnabled = false;

  bool reset() {
    this->state = NEEDS_RESET;
    this->playerId = 0;
    this->playerCount = 1;

    stop();
    return start();
  }

  bool start() {
    pingAdapter();
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);

    if (!login())
      return false;

    wait(LINK_WIRELESS_TRANSFER_WAIT);

    if (!sendCommand(LINK_WIRELESS_COMMAND_HELLO).success)
      return false;

    if (!sendCommand(LINK_WIRELESS_COMMAND_SETUP,
                     std::vector<u32>{LINK_WIRELESS_SETUP_MAGIC})
             .success)
      return false;

    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
    state = AUTHENTICATED;

    return true;
  }

  void stop() { linkSPI->deactivate(); }

  void pingAdapter() {
    linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    linkGPIO->writePin(LinkGPIO::SD, true);
    wait(LINK_WIRELESS_PING_WAIT);
    linkGPIO->writePin(LinkGPIO::SD, false);
  }

  bool login() {
    LoginMemory memory;

    if (!exchangeLoginPacket(LINK_WIRELESS_LOGIN_PARTS[0], 0, memory))
      return false;

    for (u32 i = 0; i < LINK_WIRELESS_LOGIN_STEPS; i++) {
      if (!exchangeLoginPacket(LINK_WIRELESS_LOGIN_PARTS[i],
                               LINK_WIRELESS_LOGIN_PARTS[i], memory))
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

  CommandResult sendCommand(u8 type,
                            std::vector<u32> params = std::vector<u32>{}) {
    CommandResult result;
    u16 length = params.size();
    u32 command = buildCommand(type, length);

    if (transfer(command) != LINK_WIRELESS_DATA_REQUEST)
      return result;

    for (auto& param : params) {
      if (transfer(param) != LINK_WIRELESS_DATA_REQUEST)
        return result;
    }

    u32 response = transfer(LINK_WIRELESS_DATA_REQUEST);
    u16 header = msB32(response);
    u16 data = lsB32(response);
    u8 responses = msB16(data);
    u8 ack = lsB16(data);

    if (header != LINK_WIRELESS_COMMAND_HEADER)
      return result;
    if (ack != type + LINK_WIRELESS_RESPONSE_ACK)
      return result;

    for (u32 i = 0; i < responses; i++)
      result.responses.push_back(transfer(LINK_WIRELESS_DATA_REQUEST));

    result.success = true;
    return result;
  }

  u32 buildCommand(u8 type, u8 length = 0) {
    return buildU32(LINK_WIRELESS_COMMAND_HEADER, buildU16(length, type));
  }

  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      wait(LINK_WIRELESS_TRANSFER_WAIT);

    u32 lines = 0;
    u32 vCount = REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return timeout(lines, vCount); },
        false, customAck);

    lines = 0;
    vCount = REG_VCOUNT;
    if (customAck) {
      linkSPI->_setSOLow();
      while (!linkSPI->_isSIHigh())
        if (timeout(lines, vCount))
          return LINK_SPI_NO_DATA;
      linkSPI->_setSOHigh();
      while (linkSPI->_isSIHigh())
        if (timeout(lines, vCount))
          return LINK_SPI_NO_DATA;
      linkSPI->_setSOLow();
    }

    return receivedData;
  }

  bool timeout(u32& lines, u32& vCount) {
    if (REG_VCOUNT != vCount) {
      lines++;
      vCount = REG_VCOUNT;
    }

    return lines > LINK_WIRELESS_TIMEOUT;
  }

  void wait(u32 verticalLines) {
    u32 lines = 0;
    u32 vCount = REG_VCOUNT;

    while (lines < verticalLines) {
      if (REG_VCOUNT != vCount) {
        lines++;
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

extern LinkWireless* linkWireless;

#endif  // LINK_WIRELESS_H
