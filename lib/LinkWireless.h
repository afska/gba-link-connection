#ifndef LINK_WIRELESS_H
#define LINK_WIRELESS_H

// --------------------------------------------------------------------------
// A high level driver for the GBA Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWireless* linkWireless = new LinkWireless();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_WIRELESS_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_WIRELESS_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_WIRELESS_ISR_TIMER);
// - 3) Initialize the library with:
//       linkWireless->activate();
// - 4) Start a server:
//       linkWireless->serve();
//
//       // `getState()` should return SERVING now...
//       // `currentPlayerId()` should return 0
//       // `playerCount()` should return the number of active consoles
// - 5) Connect to a server:
//       std::vector<LinkWireless::Server> servers;
//       linkWireless->getServers(servers);
//       if (servers.empty()) return;
//
//       linkWireless->connect(servers[0].id);
//       while (linkWireless->getState() == LinkWireless::State::CONNECTING)
//         linkWireless->keepConnecting();
//
//       // `getState()` should return CONNECTED now...
//       // `currentPlayerId()` should return 1, 2, 3, or 4 (the host is 0)
//       // `playerCount()` should return the number of active consoles
// - 6) Send data:
//       linkConnection->send(std::vector<u32>{1, 2, 3});
// - 7) Receive data:
//       auto messages = std::vector<LinkWireless::Message>{};
//       linkConnection->receive(messages);
//       if (messages.size() > 0) {
//         // ...
//       }
// - 8) Disconnect:
//       linkWireless->activate();
//       // (resets the adapter)
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That can cause packet loss. You might want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - servers can send up to 19 words of 32 bits at a time!
// - clients can send up to 3 words of 32 bits at a time!
// - if retransmission is on, these limits drop to 14 and 1!
// - don't send 0xFFFFFFFF, it's reserved for errors!
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <string>
#include <vector>
#include "LinkGPIO.h"
#include "LinkSPI.h"

#define LINK_WIRELESS_MAX_PLAYERS 5
#define LINK_WIRELESS_MIN_PLAYERS 2
#define LINK_WIRELESS_QUEUE_SIZE 30
#define LINK_WIRELESS_DEFAULT_TIMEOUT 5
#define LINK_WIRELESS_DEFAULT_REMOTE_TIMEOUT 25
#define LINK_WIRELESS_DEFAULT_INTERVAL 50
#define LINK_WIRELESS_DEFAULT_SEND_TIMER_ID 3
#define LINK_WIRELESS_BASE_FREQUENCY TM_FREQ_1024
#define LINK_WIRELESS_MSG_CONFIRMATION 0
#define LINK_WIRELESS_PING_WAIT 50
#define LINK_WIRELESS_TRANSFER_WAIT 15
#define LINK_WIRELESS_BROADCAST_SEARCH_WAIT_FRAMES 60
#define LINK_WIRELESS_CMD_TIMEOUT 100
#define LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH 20
#define LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#define LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 50
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
#define LINK_WIRELESS_BARRIER asm volatile("" ::: "memory")

#define LINK_WIRELESS_RESET_IF_NEEDED \
  if (!isEnabled)                     \
    return false;                     \
  if (state == NEEDS_RESET)           \
    if (!reset())                     \
      return false;

static volatile char LINK_WIRELESS_VERSION[] = "LinkWireless/v4.3.0";

void LINK_WIRELESS_ISR_VBLANK();
void LINK_WIRELESS_ISR_SERIAL();
void LINK_WIRELESS_ISR_TIMER();
const u16 LINK_WIRELESS_LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                         0x4e45, 0x4f44, 0x4f44, 0x8001};
const u16 LINK_WIRELESS_USER_MAX_SERVER_TRANSFER_LENGTHS[] = {19, 14};
const u32 LINK_WIRELESS_USER_MAX_CLIENT_TRANSFER_LENGTHS[] = {3, 1};
const u16 LINK_WIRELESS_TIMER_IRQ_IDS[] = {IRQ_TIMER0, IRQ_TIMER1, IRQ_TIMER2,
                                           IRQ_TIMER3};

class LinkWireless {
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
    INVALID_SEND_SIZE = 4,
    BUFFER_IS_FULL = 5,
    // Communication errors
    COMMAND_FAILED = 6,
    WEIRD_PLAYER_ID = 7,
    SEND_DATA_FAILED = 8,
    RECEIVE_DATA_FAILED = 9,
    BAD_CONFIRMATION = 10,
    BAD_MESSAGE = 11,
    ACKNOWLEDGE_FAILED = 12,
    TIMEOUT = 13,
    REMOTE_TIMEOUT = 14
  };

  struct Message {
    u32 _packetId = 0;

    u32 data[LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH];
    u32 dataSize = 0;

    u8 playerId = 0;
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
      u32 timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
      u32 remoteTimeout = LINK_WIRELESS_DEFAULT_REMOTE_TIMEOUT,
      u16 interval = LINK_WIRELESS_DEFAULT_INTERVAL,
      u8 sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID) {
    this->config.forwarding = forwarding;
    this->config.retransmission = retransmission;
    this->config.maxPlayers = maxPlayers;
    this->config.timeout = timeout;
    this->config.remoteTimeout = remoteTimeout;
    this->config.interval = interval;
    this->config.sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID;
  }

  bool isActive() { return isEnabled; }

  bool activate() {
    lastError = NONE;
    isEnabled = false;

    LINK_WIRELESS_BARRIER;
    bool success = reset();
    LINK_WIRELESS_BARRIER;

    isEnabled = true;
    return success;
  }

  void deactivate() {
    lastError = NONE;
    isEnabled = false;
    resetState();
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

    addData(buildU32(buildU16(gameName[1], gameName[0]), buildU16(0x02, 0x02)),
            true);
    addData(buildU32(buildU16(gameName[5], gameName[4]),
                     buildU16(gameName[3], gameName[2])));
    addData(buildU32(buildU16(gameName[9], gameName[8]),
                     buildU16(gameName[7], gameName[6])));
    addData(buildU32(buildU16(gameName[13], gameName[12]),
                     buildU16(gameName[11], gameName[10])));
    addData(buildU32(buildU16(userName[3], userName[2]),
                     buildU16(userName[1], userName[0])));
    addData(buildU32(buildU16(userName[7], userName[6]),
                     buildU16(userName[5], userName[4])));
    bool success = sendCommand(LINK_WIRELESS_COMMAND_BROADCAST, true).success &&
                   sendCommand(LINK_WIRELESS_COMMAND_START_HOST).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    wait(LINK_WIRELESS_TRANSFER_WAIT);
    state = SERVING;

    return true;
  }

  bool getServers(std::vector<Server>& servers) {
    return getServers(servers, []() {});
  }

  template <typename F>
  bool getServers(std::vector<Server>& servers, F onWait) {
    if (!getServersAsyncStart())
      return false;

    waitVBlanks(LINK_WIRELESS_BROADCAST_SEARCH_WAIT_FRAMES, onWait);

    if (!getServersAsyncEnd(servers))
      return false;

    return true;
  }

  bool getServersAsyncStart() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_START).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = SEARCHING;

    return true;
  }

  bool getServersAsyncEnd(std::vector<Server>& servers) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SEARCHING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result = sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_POLL);
    bool success1 =
        result.success &&
        result.responsesSize % LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH == 0;

    if (!success1) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    bool success2 =
        sendCommand(LINK_WIRELESS_COMMAND_BROADCAST_READ_END).success;

    if (!success2) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    u32 totalBroadcasts =
        result.responsesSize / LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH;

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

    state = AUTHENTICATED;

    return true;
  }

  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    addData(serverId, true);
    bool success = sendCommand(LINK_WIRELESS_COMMAND_CONNECT, true).success;

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
    if (!result1.success || result1.responsesSize == 0) {
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
    if (!result2.success || result2.responsesSize == 0 ||
        (u16)result2.responses[0] != assignedClientId) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    sessionState.currentPlayerId = assignedPlayerId;
    state = CONNECTED;

    return true;
  }

  bool send(std::vector<u32> data) { return send(&data[0], data.size()); }

  bool receive(std::vector<Message>& messages) {
    if (!isEnabled || state == NEEDS_RESET || !isSessionActive())
      return false;

    LINK_WIRELESS_BARRIER;
    isReadingMessages = true;
    LINK_WIRELESS_BARRIER;

    while (!sessionState.incomingMessages.isEmpty())
      messages.push_back(sessionState.incomingMessages.pop());

    LINK_WIRELESS_BARRIER;
    isReadingMessages = false;
    LINK_WIRELESS_BARRIER;

    return true;
  }

  State getState() { return state; }
  bool isConnected() { return sessionState.playerCount > 1; }
  bool isSessionActive() { return state == SERVING || state == CONNECTED; }
  u8 playerCount() { return sessionState.playerCount; }
  u8 currentPlayerId() { return sessionState.currentPlayerId; }
  bool canSend() { return !sessionState.outgoingMessages.isFull(); }
  u32 getPendingCount() { return sessionState.outgoingMessages.size(); }
  Error getLastError() {
    Error error = lastError;
    lastError = NONE;
    return error;
  }

  u32 _lastPacketId() { return sessionState.lastPacketId; }
  u32 _lastConfirmationFromClient1() {
    return sessionState.lastConfirmationFromClients[1];
  }
  u32 _lastPacketIdFromClient1() {
    return sessionState.lastPacketIdFromClients[1];
  }
  u32 _lastConfirmationFromServer() {
    return sessionState.lastConfirmationFromServer;
  }
  u32 _lastPacketIdFromServer() { return sessionState.lastPacketIdFromServer; }
  u32 _nextPendingPacketId() {
    return sessionState.outgoingMessages.isEmpty()
               ? 0
               : sessionState.outgoingMessages.peek()._packetId;
  }

  ~LinkWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

  void _onVBlank() {
    if (!isEnabled)
      return;

    if (!isSessionActive()) {
      copyState();
      return;
    }

    if (isConnected() && sessionState.frameRecvCount == 0)
      sessionState.recvTimeout++;

    sessionState.frameRecvCount = 0;
    sessionState.acceptCalled = false;

    copyState();
  }

  void _onSerial() {
    if (!isEnabled)
      return;

    linkSPI->_onSerial(true);

    bool hasNewData = linkSPI->getAsyncState() == LinkSPI::AsyncState::READY;
    if (hasNewData)
      if (!acknowledge()) {
        reset();
        lastError = ACKNOWLEDGE_FAILED;
        copyState();
        return;
      }
    u32 newData = linkSPI->getAsyncData();

    if (!isSessionActive()) {
      copyState();
      return;
    }

    if (asyncCommand.isActive) {
      if (asyncCommand.state == AsyncCommand::State::PENDING) {
        if (hasNewData)
          updateAsyncCommand(newData);
        else
          asyncCommand.state = AsyncCommand::State::COMPLETED;

        if (asyncCommand.state == AsyncCommand::State::COMPLETED)
          processAsyncCommand();
      }
    }

    copyState();
  }

  void _onTimer() {
    if (!isEnabled)
      return;

    if (!isSessionActive()) {
      copyState();
      return;
    }

    if (sessionState.recvTimeout >= config.timeout) {
      reset();
      lastError = TIMEOUT;
      copyState();
      return;
    }

    if (!asyncCommand.isActive)
      acceptConnectionsOrSendData();

    copyState();
  }

 private:
  struct Config {
    bool forwarding;
    bool retransmission;
    u8 maxPlayers;
    u32 timeout;
    u32 remoteTimeout;
    u32 interval;
    u32 sendTimerId;
  };

  class MessageQueue {
   public:
    void push(Message item) {
      if (isFull())
        return;

      rear = (rear + 1) % LINK_WIRELESS_QUEUE_SIZE;
      arr[rear] = item;
      count++;
    }

    Message pop() {
      if (isEmpty())
        return Message{};

      auto x = arr[front];
      front = (front + 1) % LINK_WIRELESS_QUEUE_SIZE;
      count--;

      return x;
    }

    Message peek() {
      if (isEmpty())
        return Message{};
      return arr[front];
    }

    template <typename F>
    void forEach(F action) {
      int currentFront = front;

      for (u32 i = 0; i < count; i++) {
        if (!action(arr[front]))
          return;
        currentFront = (currentFront + 1) % LINK_WIRELESS_QUEUE_SIZE;
      }
    }

    void clear() {
      while (!isEmpty())
        pop();
    }

    int size() { return count; }
    bool isEmpty() { return size() == 0; }
    bool isFull() { return size() == LINK_WIRELESS_QUEUE_SIZE; }

   private:
    Message arr[LINK_WIRELESS_QUEUE_SIZE];
    vs32 front = 0;
    vs32 rear = -1;
    vu32 count = 0;
  };

  struct SessionState {
    MessageQueue incomingMessages;      // read by user, write by irq&user
    MessageQueue outgoingMessages;      // read and write by irq
    MessageQueue tmpMessagesToReceive;  // read and write by irq
    MessageQueue tmpMessagesToSend;     // read by irq, write by user&irq
    u32 timeouts[LINK_WIRELESS_MAX_PLAYERS];
    u32 recvTimeout = 0;
    u32 frameRecvCount = 0;
    bool acceptCalled = false;

    u8 playerCount = 1;
    u8 currentPlayerId = 0;

    u32 lastPacketId = 0;
    u32 lastPacketIdFromServer = 0;
    u32 lastConfirmationFromServer = 0;
    u32 lastPacketIdFromClients[LINK_WIRELESS_MAX_PLAYERS];
    u32 lastConfirmationFromClients[LINK_WIRELESS_MAX_PLAYERS];
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

  struct LoginMemory {
    u16 previousGBAData = 0xffff;
    u16 previousAdapterData = 0xffff;
  };

  struct CommandResult {
    bool success = false;
    u32 responses[LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
    u32 responsesSize = 0;
  };

  struct AsyncCommand {
    enum State { PENDING, COMPLETED };

    enum Step {
      COMMAND_HEADER,
      COMMAND_PARAMETERS,
      RESPONSE_REQUEST,
      DATA_REQUEST
    };

    u8 type;
    u32 parameters[LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH];
    u32 responses[LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH];
    CommandResult result;
    State state;
    Step step;
    u32 sentParameters, totalParameters;
    u32 receivedResponses, totalResponses;
    bool isActive;
  };

  SessionState sessionState;
  AsyncCommand asyncCommand;
  Config config;
  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  u32 data[LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH];
  u32 dataSize = 0;
  volatile bool isReadingMessages = false;
  volatile bool isAddingMessage = false;
  volatile bool isPendingClearActive = false;
  Error lastError = NONE;
  bool isEnabled = false;

  bool send(u32* data, u32 dataSize) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive()) {
      lastError = WRONG_STATE;
      return false;
    }
    u32 maxTransferLength = state == SERVING
                                ? LINK_WIRELESS_USER_MAX_SERVER_TRANSFER_LENGTHS
                                      [config.retransmission]
                                : LINK_WIRELESS_USER_MAX_CLIENT_TRANSFER_LENGTHS
                                      [config.retransmission];
    if (dataSize == 0 || dataSize > maxTransferLength) {
      lastError = INVALID_SEND_SIZE;
      return false;
    }

    if (sessionState.outgoingMessages.isFull()) {
      lastError = BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId = sessionState.currentPlayerId;
    for (u32 i = 0; i < dataSize; i++)
      message.data[i] = data[i];
    message.dataSize = dataSize;

    LINK_WIRELESS_BARRIER;
    isAddingMessage = true;
    LINK_WIRELESS_BARRIER;

    sessionState.tmpMessagesToSend.push(message);

    LINK_WIRELESS_BARRIER;
    isAddingMessage = false;
    LINK_WIRELESS_BARRIER;

    return true;
  }

  void processAsyncCommand() {  // (irq only)
    if (!asyncCommand.result.success) {
      if (asyncCommand.type == LINK_WIRELESS_COMMAND_SEND_DATA)
        lastError = SEND_DATA_FAILED;
      else if (asyncCommand.type == LINK_WIRELESS_COMMAND_RECEIVE_DATA)
        lastError = RECEIVE_DATA_FAILED;
      else
        lastError = COMMAND_FAILED;

      reset();
      return;
    }

    asyncCommand.isActive = false;

    switch (asyncCommand.type) {
      case LINK_WIRELESS_COMMAND_ACCEPT_CONNECTIONS: {
        // Accept connections (end)
        sessionState.playerCount = 1 + asyncCommand.result.responsesSize;

        break;
      }
      case LINK_WIRELESS_COMMAND_SEND_DATA: {
        // Send data (end)
        // Receive data (start)
        sendCommandAsync(LINK_WIRELESS_COMMAND_RECEIVE_DATA);

        break;
      }
      case LINK_WIRELESS_COMMAND_RECEIVE_DATA: {
        // Receive data (end)
        if (asyncCommand.result.responsesSize == 0)
          break;

        sessionState.frameRecvCount++;
        sessionState.recvTimeout = 0;

        trackRemoteTimeouts();

        if (!addIncomingMessagesFromData(asyncCommand.result))
          return;

        if (!checkRemoteTimeouts()) {
          reset();
          lastError = REMOTE_TIMEOUT;
          return;
        }

        break;
      }
      default: {
      }
    }
  }

  void acceptConnectionsOrSendData() {  // (irq only)
    if (state == SERVING && !sessionState.acceptCalled &&
        sessionState.playerCount < config.maxPlayers) {
      // Accept connections (start)
      sendCommandAsync(LINK_WIRELESS_COMMAND_ACCEPT_CONNECTIONS);
      sessionState.acceptCalled = true;
    } else if (state == CONNECTED || isConnected()) {
      // Send data (start)
      sendPendingData();
    }
  }

  void sendPendingData() {  // (irq only)
    setDataFromOutgoingMessages();
    sendCommandAsync(LINK_WIRELESS_COMMAND_SEND_DATA, true);
    clearOutgoingMessagesIfNeeded();
  }

  void setDataFromOutgoingMessages() {  // (irq only)
    u32 maxTransferLength = getDeviceTransferLength();

    addData(0, true);

    if (config.retransmission)
      addConfirmations();
    else
      addPingMessageIfNeeded();

    sessionState.outgoingMessages.forEach(
        [this, maxTransferLength](Message message) {
          u8 size = message.dataSize;
          u32 header =
              buildMessageHeader(message.playerId, size, message._packetId);

          if (dataSize /* -1 (wireless header) + 1 (msg header) */ + size >
              maxTransferLength)
            return false;

          addData(header);
          for (u32 i = 0; i < size; i++)
            addData(message.data[i]);

          return true;
        });

    // (add wireless header)
    u32 bytes = (dataSize - 1) * 4;
    data[0] = sessionState.currentPlayerId == 0
                  ? bytes
                  : (1 << (3 + sessionState.currentPlayerId * 5)) * bytes;
  }

  bool addIncomingMessagesFromData(CommandResult& result) {  // (irq only)
    for (u32 i = 1; i < result.responsesSize; i++) {
      MessageHeaderSerializer serializer;
      serializer.asInt = result.responses[i];

      MessageHeader header = serializer.asStruct;
      u8 remotePlayerCount = LINK_WIRELESS_MIN_PLAYERS + header.clientCount;
      u8 remotePlayerId = header.playerId;
      u8 size = header.size;
      u32 packetId = header.packetId;

      if (i + size >= result.responsesSize) {
        reset();
        lastError = BAD_MESSAGE;
        return false;
      }

      sessionState.timeouts[0] = 0;
      sessionState.timeouts[remotePlayerId] = 0;

      if (state == SERVING) {
        if (config.retransmission &&
            packetId != LINK_WIRELESS_MSG_CONFIRMATION &&
            sessionState.lastPacketIdFromClients[remotePlayerId] > 0 &&
            packetId !=
                sessionState.lastPacketIdFromClients[remotePlayerId] + 1)
          goto skip;

        if (packetId != LINK_WIRELESS_MSG_CONFIRMATION)
          sessionState.lastPacketIdFromClients[remotePlayerId] = packetId;
      } else {
        if (config.retransmission &&
            packetId != LINK_WIRELESS_MSG_CONFIRMATION &&
            sessionState.lastPacketIdFromServer > 0 &&
            packetId != sessionState.lastPacketIdFromServer + 1)
          goto skip;

        sessionState.playerCount = remotePlayerCount;

        if (packetId != LINK_WIRELESS_MSG_CONFIRMATION)
          sessionState.lastPacketIdFromServer = packetId;
      }

      if (remotePlayerId == sessionState.currentPlayerId) {
      skip:
        i += size;
        continue;
      }

      if (size > 0) {
        Message message;
        message._packetId = packetId;
        for (u32 j = 0; j < size; j++)
          message.data[j] = result.responses[i + 1 + j];
        message.dataSize = size;
        message.playerId = remotePlayerId;

        if (config.retransmission &&
            packetId == LINK_WIRELESS_MSG_CONFIRMATION) {
          if (!handleConfirmation(message)) {
            reset();
            lastError = BAD_CONFIRMATION;
            return false;
          }
        } else {
          sessionState.tmpMessagesToReceive.push(message);
          forwardMessageIfNeeded(message);
        }

        i += size;
      }
    }

    return true;
  }

  void clearOutgoingMessagesIfNeeded() {  // (irq only)
    if (!config.retransmission)
      sessionState.outgoingMessages.clear();
  }

  void forwardMessageIfNeeded(Message& message) {  // (irq only)
    if (state == SERVING && config.forwarding && sessionState.playerCount > 2) {
      message._packetId = ++sessionState.lastPacketId;
      sessionState.outgoingMessages.push(message);
    }
  }

  void addPingMessageIfNeeded() {  // (irq only)
    if (sessionState.outgoingMessages.isEmpty()) {
      Message emptyMessage;
      emptyMessage.playerId = sessionState.currentPlayerId;
      emptyMessage._packetId = ++sessionState.lastPacketId;
      sessionState.outgoingMessages.push(emptyMessage);
    }
  }

  void addConfirmations() {  // (irq only)
    if (state == SERVING) {
      addData(buildConfirmationHeader(0));
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS - 1; i++)
        addData(sessionState.lastPacketIdFromClients[1 + i]);
    } else {
      addData(buildConfirmationHeader(sessionState.currentPlayerId));
      addData(sessionState.lastPacketIdFromServer);
    }
  }

  bool handleConfirmation(Message confirmation) {  // (irq only)
    if (confirmation.dataSize == 0)
      return false;

    bool isServerConfirmation = confirmation.playerId == 0;

    if (isServerConfirmation) {
      if (state != CONNECTED ||
          confirmation.dataSize != LINK_WIRELESS_MAX_PLAYERS - 1)
        return false;

      sessionState.lastConfirmationFromServer =
          confirmation.data[sessionState.currentPlayerId - 1];
      removeConfirmedMessages(sessionState.lastConfirmationFromServer);
    } else {
      if (state != SERVING || confirmation.dataSize != 1)
        return false;

      u32 confirmationData = confirmation.data[0];
      sessionState.lastConfirmationFromClients[confirmation.playerId] =
          confirmationData;

      u32 min = 0xffffffff;
      for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS - 1; i++) {
        u32 confirmationData = sessionState.lastConfirmationFromClients[1 + i];
        if (confirmationData > 0 && confirmationData < min)
          min = confirmationData;
      }
      if (min < 0xffffffff)
        removeConfirmedMessages(min);
    }

    return true;
  }

  void removeConfirmedMessages(u32 confirmation) {  // (irq only)
    while (!sessionState.outgoingMessages.isEmpty() &&
           sessionState.outgoingMessages.peek()._packetId <= confirmation)
      sessionState.outgoingMessages.pop();
  }

  u32 buildConfirmationHeader(u8 playerId) {  // (irq only)
    return buildMessageHeader(
        playerId, playerId == 0 ? LINK_WIRELESS_MAX_PLAYERS - 1 : 1, 0);
  }

  u32 buildMessageHeader(u8 playerId, u8 size, u32 packetId) {  // (irq only)
    MessageHeader header;
    header.clientCount = sessionState.playerCount - LINK_WIRELESS_MIN_PLAYERS;
    header.playerId = playerId;
    header.size = size;
    header.packetId = packetId;

    MessageHeaderSerializer serializer;
    serializer.asStruct = header;
    return serializer.asInt;
  }

  void trackRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < sessionState.playerCount; i++)
      if (i != sessionState.currentPlayerId)
        sessionState.timeouts[i]++;
  }

  bool checkRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < sessionState.playerCount; i++) {
      if ((i == 0 || state == SERVING) &&
          sessionState.timeouts[i] > config.remoteTimeout)
        return false;
    }

    return true;
  }

  u32 getDeviceTransferLength() {  // (irq only)
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

  void addData(u32 value, bool start = false) {
    if (start)
      dataSize = 0;
    data[dataSize] = value;
    dataSize++;
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
    this->sessionState.recvTimeout = 0;
    this->sessionState.frameRecvCount = 0;
    this->sessionState.acceptCalled = false;
    this->sessionState.lastPacketId = 0;
    this->sessionState.lastPacketIdFromServer = 0;
    this->sessionState.lastConfirmationFromServer = 0;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
      this->sessionState.timeouts[i] = 0;
      this->sessionState.lastPacketIdFromClients[i] = 0;
      this->sessionState.lastConfirmationFromClients[i] = 0;
    }
    this->asyncCommand.isActive = false;
    this->dataSize = 0;

    if (!isReadingMessages)
      this->sessionState.incomingMessages.clear();

    isPendingClearActive = true;
  }

  void stop() {
    stopTimer();

    linkSPI->deactivate();
  }

  bool start() {
    startTimer();

    pingAdapter();
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);

    if (!login())
      return false;

    wait(LINK_WIRELESS_TRANSFER_WAIT);

    if (!sendCommand(LINK_WIRELESS_COMMAND_HELLO).success)
      return false;

    addData(LINK_WIRELESS_SETUP_MAGIC, true);
    if (!sendCommand(LINK_WIRELESS_COMMAND_SETUP, true).success)
      return false;

    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
    state = AUTHENTICATED;

    return true;
  }

  void stopTimer() {
    REG_TM[config.sendTimerId].cnt =
        REG_TM[config.sendTimerId].cnt & (~TM_ENABLE);
  }

  void startTimer() {
    REG_TM[config.sendTimerId].start = -config.interval;
    REG_TM[config.sendTimerId].cnt =
        TM_ENABLE | TM_IRQ | LINK_WIRELESS_BASE_FREQUENCY;
  }

  void copyState() {  // (irq only)
    if (!isAddingMessage) {
      while (!sessionState.tmpMessagesToSend.isEmpty()) {
        bool shouldPop = !isSessionActive() || canSend();

        if (shouldPop) {
          auto message = sessionState.tmpMessagesToSend.pop();

          if (isSessionActive()) {
            message._packetId = ++sessionState.lastPacketId;
            sessionState.outgoingMessages.push(message);
          }
        }
      }

      if (isPendingClearActive) {
        sessionState.outgoingMessages.clear();
        isPendingClearActive = false;
      }
    }

    if (!isReadingMessages) {
      while (!sessionState.tmpMessagesToReceive.isEmpty()) {
        auto message = sessionState.tmpMessagesToReceive.pop();

        if (state == SERVING || state == CONNECTED)
          sessionState.incomingMessages.push(message);
      }
    }
  }

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

  CommandResult sendCommand(u8 type, bool withData = false) {
    CommandResult result;
    u32 command = buildCommand(type, withData ? (u16)dataSize : 0);

    if (transfer(command) != LINK_WIRELESS_DATA_REQUEST)
      return result;

    if (withData) {
      for (u32 i = 0; i < dataSize; i++) {
        if (transfer(data[i]) != LINK_WIRELESS_DATA_REQUEST)
          return result;
      }
    }

    u32 response = transfer(LINK_WIRELESS_DATA_REQUEST);
    u16 header = msB32(response);
    u16 data = lsB32(response);
    u8 responses = msB16(data);
    u8 ack = lsB16(data);

    if (header != LINK_WIRELESS_COMMAND_HEADER ||
        ack != type + LINK_WIRELESS_RESPONSE_ACK ||
        responses > LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH)
      return result;

    for (u32 i = 0; i < responses; i++)
      result.responses[i] = transfer(LINK_WIRELESS_DATA_REQUEST);
    result.responsesSize = responses;

    result.success = true;
    return result;
  }

  void sendCommandAsync(u8 type, bool withData = false) {  // (irq only)
    if (asyncCommand.isActive)
      return;

    asyncCommand.type = type;
    if (withData) {
      for (u32 i = 0; i < dataSize; i++)
        asyncCommand.parameters[i] = data[i];
    }
    asyncCommand.result.success = false;
    asyncCommand.state = AsyncCommand::State::PENDING;
    asyncCommand.step = AsyncCommand::Step::COMMAND_HEADER;
    asyncCommand.sentParameters = 0;
    asyncCommand.totalParameters = withData ? dataSize : 0;
    asyncCommand.receivedResponses = 0;
    asyncCommand.totalResponses = 0;
    asyncCommand.isActive = true;

    u32 command = buildCommand(type, asyncCommand.totalParameters);
    transferAsync(command);
  }

  void updateAsyncCommand(u32 newData) {  // (irq only)
    switch (asyncCommand.step) {
      case AsyncCommand::Step::COMMAND_HEADER: {
        if (newData != LINK_WIRELESS_DATA_REQUEST) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendAsyncCommandParameterOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::COMMAND_PARAMETERS: {
        if (newData != LINK_WIRELESS_DATA_REQUEST) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendAsyncCommandParameterOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::RESPONSE_REQUEST: {
        u16 header = msB32(newData);
        u16 data = lsB32(newData);
        u8 responses = msB16(data);
        u8 ack = lsB16(data);

        if (header != LINK_WIRELESS_COMMAND_HEADER ||
            ack != asyncCommand.type + LINK_WIRELESS_RESPONSE_ACK ||
            responses > LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        asyncCommand.totalResponses = responses;
        asyncCommand.result.responsesSize = responses;

        receiveAsyncCommandResponseOrFinish();
        break;
      }
      case AsyncCommand::Step::DATA_REQUEST: {
        asyncCommand.result.responses[asyncCommand.receivedResponses] = newData;
        asyncCommand.receivedResponses++;

        receiveAsyncCommandResponseOrFinish();
        break;
      }
    }
  }

  void sendAsyncCommandParameterOrRequestResponse() {  // (irq only)
    if (asyncCommand.sentParameters < asyncCommand.totalParameters) {
      asyncCommand.step = AsyncCommand::Step::COMMAND_PARAMETERS;
      transferAsync(asyncCommand.parameters[asyncCommand.sentParameters]);
      asyncCommand.sentParameters++;
    } else {
      asyncCommand.step = AsyncCommand::Step::RESPONSE_REQUEST;
      transferAsync(LINK_WIRELESS_DATA_REQUEST);
    }
  }

  void receiveAsyncCommandResponseOrFinish() {  // (irq only)
    if (asyncCommand.receivedResponses < asyncCommand.totalResponses) {
      asyncCommand.step = AsyncCommand::Step::DATA_REQUEST;
      transferAsync(LINK_WIRELESS_DATA_REQUEST);
    } else {
      asyncCommand.result.success = true;
      asyncCommand.state = AsyncCommand::State::COMPLETED;
    }
  }

  u32 buildCommand(u8 type, u8 length = 0) {
    return buildU32(LINK_WIRELESS_COMMAND_HEADER, buildU16(length, type));
  }

  void transferAsync(u32 data) {
    linkSPI->transfer(
        data, []() { return false; }, true, true);
  }

  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      wait(LINK_WIRELESS_TRANSFER_WAIT);

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
    u32 count = 0;
    u32 vCount = REG_VCOUNT;

    while (count < verticalLines) {
      if (REG_VCOUNT != vCount) {
        count += std::max((s32)REG_VCOUNT - (s32)vCount, 0);
        vCount = REG_VCOUNT;
      }
    };
  }

  template <typename F>
  void waitVBlanks(u32 vBlanks, F onVBlank) {
    u32 count = 0;
    u32 vCount = REG_VCOUNT;

    while (count < vBlanks) {
      if (REG_VCOUNT != vCount) {
        vCount = REG_VCOUNT;

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

extern LinkWireless* linkWireless;

inline void LINK_WIRELESS_ISR_VBLANK() {
  linkWireless->_onVBlank();
}

inline void LINK_WIRELESS_ISR_SERIAL() {
  linkWireless->_onSerial();
}

inline void LINK_WIRELESS_ISR_TIMER() {
  linkWireless->_onTimer();
}

#endif  // LINK_WIRELESS_H
