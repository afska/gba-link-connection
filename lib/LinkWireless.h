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
//       std::vector<LinkWireless::Server> servers;
//       linkWireless->getServers(servers);
//       linkWireless->connect(servers[0].id);
//       while (linkWireless->getState() == LinkWireless::State::CONNECTING)
//         linkWireless->keepConnecting();
//       // `getState()` should return CONNECTED now...
//       // `getPlayerId()` should return 1, 2, 3, or 4 (the host is 0)
// - 5) Send data:
//       linkConnection->send(std::vector<u32>{1, 2, 3});
// - 6) Receive data:
//       std::vector<LinkWireless::Message> messages;
//       linkConnection->receive(messages);
//       if (messages.size() > 0) {
//         // ...
//       }
// - 7) Disconnect:
//       linkWireless->disconnect();
// --------------------------------------------------------------------------
// restrictions:
// - servers can send up to 19 words of 32 bits at a time!
// - clients can send up to 3 words of 32 bits at a time!
// - if retransmission is on, these limits drop to 14 and 1!
// - you can workaround these limits by doing multiple exchanges with
// receive(messages, times)!
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <algorithm>
#include <string>
#include <vector>
#include "LinkGPIO.h"
#include "LinkSPI.h"

#define LINK_WIRELESS_DEFAULT_MSG_TIMEOUT 5
#define LINK_WIRELESS_DEFAULT_MULTIRECEIVE_TIMEOUT ((160 + 68) * 5)
#define LINK_WIRELESS_DEFAULT_BUFFER_SIZE 30
#define LINK_WIRELESS_MSG_CONFIRMATION 0
#define LINK_WIRELESS_PING_WAIT 50
#define LINK_WIRELESS_TRANSFER_WAIT 15
#define LINK_WIRELESS_BROADCAST_SEARCH_WAIT ((160 + 68) * 60)
#define LINK_WIRELESS_CMD_TIMEOUT 100
#define LINK_WIRELESS_MIN_PLAYERS 2
#define LINK_WIRELESS_MAX_PLAYERS 5
#define LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH 20
#define LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
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
#define LINK_WIRELESS_COMMAND_BROADCAST_READ_POLL 0x1d
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

static volatile char LINK_WIRELESS_VERSION[] = "LinkWireless/v4.2.0";

const u16 LINK_WIRELESS_LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                         0x4e45, 0x4f44, 0x4f44, 0x8001};
const u16 LINK_WIRELESS_USER_MAX_SERVER_TRANSFER_LENGTHS[] = {19, 14};
const u32 LINK_WIRELESS_USER_MAX_CLIENT_TRANSFER_LENGTHS[] = {3, 1};

class LinkWireless {
 public:
  enum State { NEEDS_RESET, AUTHENTICATED, SERVING, CONNECTING, CONNECTED };

  enum Error {
    // User errors
    NONE = 0,
    WRONG_STATE = 1,
    GAME_NAME_TOO_LONG = 2,
    USER_NAME_TOO_LONG = 3,
    INVALID_SEND_SIZE = 4,
    BUFFER_IS_FULL = 5,
    RETRANSMISSION_IS_OFF = 6,
    // Communication errors
    COMMAND_FAILED = 7,
    WEIRD_PLAYER_ID = 8,
    SEND_DATA_FAILED = 9,
    RECEIVE_DATA_FAILED = 10,
    BAD_CONFIRMATION = 11,
    BAD_MESSAGE = 12,
    MAX_PLAYERS_LIMIT_REACHED = 13,
    TIMEOUT = 14
  };

  struct Message {
    u8 playerId = 0;
    std::vector<u32> data = std::vector<u32>{};

    u32 _packetId = 0;
  };

  struct Server {
    u16 id;
    std::string gameName;
    std::string userName;
  };

  explicit LinkWireless(
      bool forwarding = true,
      bool retransmission = true,
      u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS,
      u32 msgTimeout = LINK_WIRELESS_DEFAULT_MSG_TIMEOUT,
      u32 multiReceiveTimeout = LINK_WIRELESS_DEFAULT_MULTIRECEIVE_TIMEOUT,
      u32 bufferSize = LINK_WIRELESS_DEFAULT_BUFFER_SIZE) {
    this->forwarding = forwarding;
    this->retransmission = retransmission;
    this->maxPlayers = maxPlayers;
    this->msgTimeout = msgTimeout;
    this->multiReceiveTimeout = multiReceiveTimeout;
    this->bufferSize = bufferSize;
  }

  bool isActive() { return isEnabled; }

  bool activate() {
    lastError = NONE;
    bool success = reset();

    isEnabled = true;
    return success;
  }

  void deactivate() {
    lastError = NONE;
    isEnabled = false;
    stop();
  }

  bool serve(std::string gameName = "", std::string userName = "") {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }
    if (gameName.length() > LINK_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = GAME_NAME_TOO_LONG;
      return false;
    }
    if (userName.length() > LINK_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = USER_NAME_TOO_LONG;
      return false;
    }
    gameName.append(LINK_WIRELESS_MAX_GAME_NAME_LENGTH - gameName.length(), 0);
    userName.append(LINK_WIRELESS_MAX_USER_NAME_LENGTH - userName.length(), 0);

    auto broadcast = std::vector<u32>{
        buildU32(buildU16(gameName[1], gameName[0]), buildU16(0x02, 0x02)),
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
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST, broadcast).success &&
        sendCommand(LINK_WIRELESS_COMMAND_START_HOST).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = SERVING;

    return true;
  }

  bool acceptConnections() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result = sendCommand(LINK_WIRELESS_COMMAND_ACCEPT_CONNECTIONS);

    if (!result.success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    playerCount = 1 + result.responses.size();

    if (playerCount > maxPlayers) {
      disconnect();
      lastError = MAX_PLAYERS_LIMIT_REACHED;
      return false;
    }

    return true;
  }

  bool getServers(std::vector<Server>& servers) {
    return getServers(servers, []() {});
  }

  template <typename F>
  bool getServers(std::vector<Server>& servers, F onWait) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success1 =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_START).success;

    if (!success1) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    wait(LINK_WIRELESS_BROADCAST_SEARCH_WAIT, onWait);

    auto result = sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_POLL);
    bool success2 =
        result.success &&
        result.responses.size() % LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH == 0;

    if (!success2) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    bool success3 =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_END).success;

    if (!success3) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    u32 totalBroadcasts =
        result.responses.size() / LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH;

    for (u32 i = 0; i < totalBroadcasts; i++) {
      u32 start = LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH * i;

      Server server;
      server.id = (u16)result.responses[start];
      recoverName(server.gameName, result.responses[start + 1], false);
      recoverName(server.gameName, result.responses[start + 2]);
      recoverName(server.gameName, result.responses[start + 3]);
      recoverName(server.gameName, result.responses[start + 4]);
      recoverName(server.userName, result.responses[start + 5]);
      recoverName(server.userName, result.responses[start + 6]);

      servers.push_back(server);
    }

    return true;
  }

  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success =
        sendCommand(LINK_WIRELESS_COMMAND_CONNECT, std::vector<u32>{serverId})
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
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != CONNECTING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result1 = sendCommand(LINK_WIRELESS_COMMAND_IS_FINISHED_CONNECT);
    if (!result1.success || result1.responses.size() == 0) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    if (result1.responses[0] == LINK_WIRELESS_STILL_CONNECTING)
      return true;

    u8 assignedPlayerId = 1 + (u8)msB32(result1.responses[0]);
    u16 assignedClientId = (u16)result1.responses[0];

    if (assignedPlayerId >= LINK_WIRELESS_MAX_PLAYERS) {
      reset();
      lastError = WEIRD_PLAYER_ID;
      return false;
    }

    auto result2 = sendCommand(LINK_WIRELESS_COMMAND_FINISH_CONNECTION);
    if (!result2.success || result2.responses.size() == 0 ||
        (u16)result2.responses[0] != assignedClientId) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    playerId = assignedPlayerId;
    state = CONNECTED;

    return true;
  }

  bool send(std::vector<u32> data, int _author = -1) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED) {
      lastError = WRONG_STATE;
      return false;
    }
    u32 maxTransferLength =
        state == SERVING
            ? LINK_WIRELESS_USER_MAX_SERVER_TRANSFER_LENGTHS[retransmission]
            : LINK_WIRELESS_USER_MAX_CLIENT_TRANSFER_LENGTHS[retransmission];
    if (data.size() == 0 || data.size() > maxTransferLength) {
      lastError = INVALID_SEND_SIZE;
      return false;
    }

    if (outgoingMessages.size() >= bufferSize) {
      lastError = BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId = _author < 0 ? playerId : _author;
    message.data = data;
    message._packetId = ++lastPacketId;

    outgoingMessages.push_back(message);

    return true;
  }

  bool receive(std::vector<Message>& messages, bool _enableTimeouts = true) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED) {
      lastError = WRONG_STATE;
      return false;
    }

    if (!sendPendingMessages()) {
      lastError = SEND_DATA_FAILED;
      return false;
    }

    std::vector<u32> words;
    if (!receiveData(words)) {
      lastError = RECEIVE_DATA_FAILED;
      return false;
    }

    if (_enableTimeouts)
      trackTimeouts();

    u32 startIndex = messages.size();

    for (u32 i = 0; i < words.size(); i++) {
      MessageHeaderSerializer serializer;
      serializer.asInt = words[i];

      MessageHeader header = serializer.asStruct;
      u8 remotePlayerCount = LINK_WIRELESS_MIN_PLAYERS + header.clientCount;
      u8 remotePlayerId = header.playerId;
      u8 size = header.size;
      u32 packetId = header.packetId;

      if (i + size >= words.size()) {
        reset();
        lastError = BAD_MESSAGE;
        return false;
      }

      timeouts[0] = 0;
      timeouts[remotePlayerId] = 0;

      if (state == SERVING) {
        if (retransmission && packetId != LINK_WIRELESS_MSG_CONFIRMATION &&
            lastPacketIdFromClients[remotePlayerId] > 0 &&
            packetId != lastPacketIdFromClients[remotePlayerId] + 1)
          goto skip;

        if (packetId != LINK_WIRELESS_MSG_CONFIRMATION)
          lastPacketIdFromClients[remotePlayerId] = packetId;
      } else {
        if (retransmission && packetId != LINK_WIRELESS_MSG_CONFIRMATION &&
            lastPacketIdFromServer > 0 &&
            packetId != lastPacketIdFromServer + 1)
          goto skip;

        playerCount = remotePlayerCount;

        if (packetId != LINK_WIRELESS_MSG_CONFIRMATION)
          lastPacketIdFromServer = packetId;
      }

      if (remotePlayerId == playerId) {
      skip:
        i += size;
        continue;
      }

      if (size > 0) {
        Message message;
        message.playerId = remotePlayerId;
        for (u32 j = 0; j < size; j++)
          message.data.push_back(words[i + 1 + j]);
        message._packetId = packetId;

        if (retransmission && packetId == LINK_WIRELESS_MSG_CONFIRMATION) {
          if (!handleConfirmation(message)) {
            reset();
            lastError = BAD_CONFIRMATION;
            return false;
          }
        } else {
          messages.push_back(message);
        }

        i += size;
      }
    }

    if (_enableTimeouts && !checkTimeouts())
      return false;

    if (state == SERVING && forwarding && playerCount > 2) {
      for (u32 i = startIndex; i < messages.size(); i++) {
        auto message = messages[i];
        send(message.data, message.playerId);
      }
    }

    return true;
  }

  bool receive(std::vector<Message>& messages, u32 times) {
    return receive(messages, times, []() { return false; });
  }

  template <typename F>
  bool receive(std::vector<Message>& messages, u32 times, F cancel) {
    if (!retransmission) {
      lastError = RETRANSMISSION_IS_OFF;
      return false;
    }

    u32 successfulExchanges = 0;
    trackTimeouts();

    u32 lines = 0;
    u32 vCount = REG_VCOUNT;
    while (successfulExchanges < times) {
      if (cancel())
        return true;

      if (timeout(multiReceiveTimeout, lines, vCount)) {
        lastError = TIMEOUT;
        disconnect();
        return false;
      }

      if (!receive(messages, false))
        return false;

      if (didReceiveAnyBytes)
        successfulExchanges++;
    }

    if (!checkTimeouts()) {
      lastError = TIMEOUT;
      disconnect();
      return false;
    }

    return true;
  }

  bool disconnect() {
    LINK_WIRELESS_RESET_IF_NEEDED

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
  bool canSend() { return outgoingMessages.size() < bufferSize; }
  u32 getPendingCount() { return outgoingMessages.size(); }
  bool didReceiveBytes() { return didReceiveAnyBytes; }
  Error getLastError() {
    Error error = lastError;
    lastError = NONE;
    return error;
  }

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

  struct MessageHeader {
    unsigned int packetId : 22;
    unsigned int size : 5;
    unsigned int playerId : 3;
    unsigned int clientCount : 2;
  };

  union MessageHeaderSerializer {
    MessageHeader asStruct;
    u32 asInt;
  };

  bool forwarding;
  bool retransmission;
  u8 maxPlayers;
  u32 msgTimeout;
  u32 multiReceiveTimeout;
  u32 bufferSize;
  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  u8 playerId = 0;
  u8 playerCount = 1;
  std::vector<Message> outgoingMessages;
  u32 lastPacketId = 0;
  u32 lastPacketIdFromServer = 0;
  u32 lastConfirmationFromServer = 0;
  u32 lastPacketIdFromClients[LINK_WIRELESS_MAX_PLAYERS];
  u32 lastConfirmationFromClients[LINK_WIRELESS_MAX_PLAYERS];
  u32 timeouts[LINK_WIRELESS_MAX_PLAYERS];
  bool didReceiveAnyBytes = false;
  Error lastError = NONE;
  bool isEnabled = false;

  bool sendPendingMessages() {
    if (outgoingMessages.empty() && !retransmission) {
      Message emptyMessage;
      emptyMessage.playerId = playerId;
      emptyMessage._packetId = ++lastPacketId;
      outgoingMessages.push_back(emptyMessage);
    }

    u32 maxTransferLength = getDeviceTransferLength();
    std::vector<u32> words;

    if (retransmission)
      addConfirmations(words);

    for (auto& message : outgoingMessages) {
      u8 size = message.data.size();
      u32 header =
          buildMessageHeader(message.playerId, size, message._packetId);

      if (words.size() + 1 + size > maxTransferLength)
        break;

      words.push_back(header);
      words.insert(words.end(), message.data.begin(), message.data.end());
    }

    if (!sendData(words))
      return false;

    if (!retransmission)
      outgoingMessages.clear();

    return true;
  }

  void trackTimeouts() {
    for (u32 i = 0; i < playerCount; i++)
      if (i != playerId)
        timeouts[i]++;
  }

  bool checkTimeouts() {
    for (u32 i = 0; i < playerCount; i++) {
      if ((i == 0 || state == SERVING) && timeouts[i] > msgTimeout) {
        lastError = TIMEOUT;
        disconnect();
        return false;
      }
    }

    return true;
  }

  void addConfirmations(std::vector<u32>& words) {
    if (state == SERVING) {
      words.push_back(buildConfirmationHeader(0));
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS - 1; i++)
        words.push_back(lastPacketIdFromClients[1 + i]);
    } else {
      words.push_back(buildConfirmationHeader(playerId));
      words.push_back(lastPacketIdFromServer);
    }
  }

  bool handleConfirmation(Message confirmation) {
    if (confirmation.data.size() == 0)
      return false;

    bool isServerConfirmation = confirmation.playerId == 0;

    if (isServerConfirmation) {
      if (state != CONNECTED ||
          confirmation.data.size() != LINK_WIRELESS_MAX_PLAYERS - 1)
        return false;

      lastConfirmationFromServer = confirmation.data[playerId - 1];
      removeConfirmedMessages(lastConfirmationFromServer);
    } else {
      if (state != SERVING || confirmation.data.size() != 1)
        return false;

      u32 confirmationData = confirmation.data[0];
      lastConfirmationFromClients[confirmation.playerId] = confirmationData;

      u32 min = 0xffffffff;
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS - 1; i++) {
        u32 confirmationData = lastConfirmationFromClients[1 + i];
        if (confirmationData > 0 && confirmationData < min)
          min = confirmationData;
      }
      removeConfirmedMessages(min);
    }

    return true;
  }

  void removeConfirmedMessages(u32 confirmation) {
    outgoingMessages.erase(
        std::remove_if(outgoingMessages.begin(), outgoingMessages.end(),
                       [confirmation](Message it) {
                         return it._packetId <= confirmation;
                       }),
        outgoingMessages.end());
  }

  u32 buildConfirmationHeader(u8 playerId) {
    return buildMessageHeader(
        playerId, playerId == 0 ? LINK_WIRELESS_MAX_PLAYERS - 1 : 1, 0);
  }

  u32 buildMessageHeader(u8 playerId, u8 size, u32 packetId) {
    MessageHeader header;
    header.clientCount = playerCount - LINK_WIRELESS_MIN_PLAYERS;
    header.playerId = playerId;
    header.size = size;
    header.packetId = packetId;

    MessageHeaderSerializer serializer;
    serializer.asStruct = header;
    return serializer.asInt;
  }

  bool sendData(std::vector<u32> data) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED) {
      lastError = WRONG_STATE;
      return false;
    }
    if (data.size() == 0 || data.size() > getDeviceTransferLength()) {
      lastError = INVALID_SEND_SIZE;
      return false;
    }

    u32 bytes = data.size() * 4;
    u32 header = playerId == 0 ? bytes : (1 << (3 + playerId * 5)) * bytes;
    data.insert(data.begin(), header);

    bool success = sendCommand(LINK_WIRELESS_COMMAND_SEND_DATA, data).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    return true;
  }

  bool receiveData(std::vector<u32>& data) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SERVING && state != CONNECTED) {
      lastError = WRONG_STATE;
      return false;
    }

    this->didReceiveAnyBytes = false;

    auto result = sendCommand(LINK_WIRELESS_COMMAND_RECEIVE_DATA);
    data = result.responses;

    if (!result.success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    if (data.size() > 0) {
      data.erase(data.begin());
      this->didReceiveAnyBytes = true;
    }

    return true;
  }

  u32 getDeviceTransferLength() {
    return state == SERVING ? LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
                            : LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
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
    this->state = NEEDS_RESET;
    this->playerId = 0;
    this->playerCount = 1;
    this->outgoingMessages = std::vector<Message>{};
    this->lastPacketId = 0;
    this->lastPacketIdFromServer = 0;
    this->lastConfirmationFromServer = 0;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
      this->lastPacketIdFromClients[i] = 0;
      this->lastConfirmationFromClients[i] = 0;
      this->timeouts[i] = 0;
    }
    this->didReceiveAnyBytes = false;

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
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, customAck);

    lines = 0;
    vCount = REG_VCOUNT;
    if (customAck) {
      linkSPI->_setSOLow();
      while (!linkSPI->_isSIHigh())
        if (cmdTimeout(lines, vCount))
          return LINK_SPI_NO_DATA;
      linkSPI->_setSOHigh();
      while (linkSPI->_isSIHigh())
        if (cmdTimeout(lines, vCount))
          return LINK_SPI_NO_DATA;
      linkSPI->_setSOLow();
    }

    return receivedData;
  }

  bool cmdTimeout(u32& lines, u32& vCount) {
    return timeout(LINK_WIRELESS_CMD_TIMEOUT, lines, vCount);
  }

  bool timeout(u32 limit, u32& lines, u32& vCount) {
    if (REG_VCOUNT != vCount) {
      lines += std::max((s32)REG_VCOUNT - (s32)vCount, 0);
      vCount = REG_VCOUNT;
    }

    return lines > limit;
  }

  void wait(u32 verticalLines) {
    wait(verticalLines, []() {});
  }

  template <typename F>
  void wait(u32 verticalLines, F onVBlank) {
    u32 lines = 0;
    u32 vCount = REG_VCOUNT;

    while (lines < verticalLines) {
      if (REG_VCOUNT != vCount) {
        lines += std::max((s32)REG_VCOUNT - (s32)vCount, 0);
        vCount = REG_VCOUNT;

        if (REG_VCOUNT == 160)
          onVBlank();
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
