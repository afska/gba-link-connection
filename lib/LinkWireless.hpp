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
//       LinkWireless::Server servers[LINK_WIRELESS_MAX_SERVERS];
//       linkWireless->getServers(servers);
//       if (servers[0].id == LINK_WIRELESS_END) return;
//
//       linkWireless->connect(servers[0].id);
//       while (linkWireless->getState() == LinkWireless::State::CONNECTING)
//         linkWireless->keepConnecting();
//
//       // `getState()` should return CONNECTED now...
//       // `currentPlayerId()` should return 1, 2, 3, or 4 (the host is 0)
//       // `playerCount()` should return the number of active consoles
// - 6) Send data:
//       linkWireless->send(0x1234);
// - 7) Receive data:
//       LinkWireless::Message messages[LINK_WIRELESS_QUEUE_SIZE];
//       linkWireless->receive(messages);
//       if (messages[0].packetId != LINK_WIRELESS_END) {
//         // ...
//       }
// - 8) Disconnect:
//       linkWireless->activate();
//       // (resets the adapter)
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - 0xFFFF is a reserved value, so don't use it!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include <cstring>
#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

// #include <string>
// #include <functional>

#ifndef LINK_WIRELESS_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing messages the queues can
 * store at max). The default value is `30`, which seems fine for most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `960` bytes. There's a double-buffered incoming queue and a
 * double-buffered outgoing queue (to avoid data races).
 * \warning You can approximate the usage with `LINK_WIRELESS_QUEUE_SIZE * 32`.
 */
#define LINK_WIRELESS_QUEUE_SIZE 30
#endif

#ifndef LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
/**
 * @brief Max server transfer length per timer tick. Must be in the range
 * `[6;20]`. The default value is `20`, but you might want to set it a bit lower
 * to reduce CPU usage.
 */
#define LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH 20
#endif

#ifndef LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH
/**
 * @brief Max client transfer length per timer tick. Must be in the range
 * `[2;4]`. The default value is `4`. Changing this is not recommended, it's
 * already too low.
 */
#define LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH 4
#endif

#ifndef LINK_WIRELESS_PUT_ISR_IN_IWRAM
/**
 * @brief Put Interrupt Service Routines (ISR) in IWRAM (uncomment to enable).
 * This can significantly improve performance due to its faster access, but it's
 * disabled by default to conserve IWRAM space, which is limited.
 * \warning If you enable this, make sure that `LinkWireless.cpp` gets compiled!
 * For example, in a Makefile-based project, verify that the file is in your
 * `SRCDIRS` list.
 */
// #define LINK_WIRELESS_PUT_ISR_IN_IWRAM
#endif

#ifndef LINK_WIRELESS_ENABLE_NESTED_IRQ
/**
 * @brief Allow LINK_WIRELESS_ISR_* functions to be interrupted (uncomment to
 * enable).
 * This can be useful, for example, if your audio engine requires calling a
 * VBlank handler with precise timing.
 * \warning This won't produce any effect if `LINK_WIRELESS_PUT_ISR_IN_IWRAM` is
 * disabled.
 */
// #define LINK_WIRELESS_ENABLE_NESTED_IRQ
#endif

#ifndef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
/**
 * @brief Use send/receive latch (uncomment to enable).
 * This makes it alternate between sends and receives on each timer tick
 * (instead of doing both things). Enabling it will introduce some latency but
 * also reduce overall CPU usage.
 */
// #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
#endif

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
/**
 * @brief Optimize the library for two players (uncomment to enable).
 * This will make the code smaller and use less CPU. It will also let you
 * "misuse" 5 bits from the packet header to send small packets really fast
 * (e.g. pressed keys) without confirmation, using the `QUICK_SEND` and
 * `QUICK_RECEIVE` properties.
 */
// #define LINK_WIRELESS_TWO_PLAYERS_ONLY
#endif

static volatile char LINK_WIRELESS_VERSION[] = "LinkWireless/v7.0.0";

#define LINK_WIRELESS_MAX_PLAYERS 5
#define LINK_WIRELESS_MIN_PLAYERS 2
#define LINK_WIRELESS_END 0
#define LINK_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH 22
#define LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH 30
#define LINK_WIRELESS_BROADCAST_LENGTH 6
#define LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH \
  (1 + LINK_WIRELESS_BROADCAST_LENGTH)
#define LINK_WIRELESS_MAX_SERVERS              \
  (LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH / \
   LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH)
#define LINK_WIRELESS_MAX_GAME_ID 0x7fff
#define LINK_WIRELESS_MAX_GAME_NAME_LENGTH 14
#define LINK_WIRELESS_MAX_USER_NAME_LENGTH 8
#define LINK_WIRELESS_DEFAULT_TIMEOUT 10
#define LINK_WIRELESS_DEFAULT_INTERVAL 50
#define LINK_WIRELESS_DEFAULT_SEND_TIMER_ID 3
#define LINK_WIRELESS_BARRIER asm volatile("" ::: "memory")
#define LINK_WIRELESS_CODE_IWRAM \
  __attribute__((section(".iwram"), target("arm"), noinline))
#define LINK_WIRELESS_ALWAYS_INLINE inline __attribute__((always_inline))

#define LINK_WIRELESS_RESET_IF_NEEDED \
  if (!isEnabled)                     \
    return false;                     \
  if (state == NEEDS_RESET)           \
    if (!reset())                     \
      return false;

/**
 * @brief A high level driver for the GBA Wireless Adapter.
 */
class LinkWireless {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using vu32 = volatile unsigned int;
  using vs32 = volatile signed int;
  using s8 = signed char;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
  static constexpr int PACKET_ID_BITS = 5;
#else
  static constexpr int PACKET_ID_BITS = 6;
#endif
  static constexpr int MAX_PACKET_IDS = (1 << PACKET_ID_BITS);
  static constexpr int PACKET_ID_MASK = (MAX_PACKET_IDS - 1);
  static constexpr int MSG_PING = 0xffff;
  static constexpr int PING_WAIT = 50;
  static constexpr int TRANSFER_WAIT = 15;
  static constexpr int BROADCAST_SEARCH_WAIT_FRAMES = 60;
  static constexpr int CMD_TIMEOUT = 10;
  static constexpr int LOGIN_STEPS = 9;
  static constexpr int COMMAND_HEADER_VALUE = 0x9966;
  static constexpr int RESPONSE_ACK_VALUE = 0x80;
  static constexpr u32 DATA_REQUEST_VALUE = 0x80000000;
  static constexpr int SETUP_MAGIC = 0x003c0420;
  static constexpr int SETUP_MAX_PLAYERS_BIT = 16;
  static constexpr int WAIT_STILL_CONNECTING = 0x01000000;
  static constexpr int COMMAND_HELLO = 0x10;
  static constexpr int COMMAND_SETUP = 0x17;
  static constexpr int COMMAND_BROADCAST = 0x16;
  static constexpr int COMMAND_START_HOST = 0x19;
  static constexpr int COMMAND_ACCEPT_CONNECTIONS = 0x1a;
  static constexpr int COMMAND_BROADCAST_READ_START = 0x1c;
  static constexpr int COMMAND_BROADCAST_READ_POLL = 0x1d;
  static constexpr int COMMAND_BROADCAST_READ_END = 0x1e;
  static constexpr int COMMAND_CONNECT = 0x1f;
  static constexpr int COMMAND_IS_FINISHED_CONNECT = 0x20;
  static constexpr int COMMAND_FINISH_CONNECTION = 0x21;
  static constexpr int COMMAND_SEND_DATA = 0x24;
  static constexpr int COMMAND_RECEIVE_DATA = 0x26;
  static constexpr int COMMAND_BYE = 0x3d;
  static constexpr u16 LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                        0x4e45, 0x4f44, 0x4f44, 0x8001};

 public:
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
  u32 QUICK_SEND = 0;
  u32 QUICK_RECEIVE = 0;
#endif

// std::function<void(std::string str)> debug;
// #define PROFILING_ENABLED
#ifdef PROFILING_ENABLED
  u32 lastVBlankTime = 0;
  u32 lastSerialTime = 0;
  u32 lastTimerTime = 0;
  u32 lastFrameSerialIRQs = 0;
  u32 lastFrameTimerIRQs = 0;
  u32 serialIRQCount = 0;
  u32 timerIRQCount = 0;
#endif

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
    BUFFER_IS_FULL = 4,
    // Communication errors
    COMMAND_FAILED = 5,
    CONNECTION_FAILED = 6,
    SEND_DATA_FAILED = 7,
    RECEIVE_DATA_FAILED = 8,
    ACKNOWLEDGE_FAILED = 9,
    TIMEOUT = 10,
    REMOTE_TIMEOUT = 11,
    BUSY_TRY_AGAIN = 12,
  };

  struct Message {
    u32 packetId = 0;

    u16 data;
    u8 playerId = 0;
  };

  struct Server {
    u16 id = 0;
    u16 gameId;
    char gameName[LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char userName[LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    u8 currentPlayerCount;

    bool isFull() { return currentPlayerCount == 0; }
  };

  /**
   * @brief Constructs a new LinkWireless object.
   * @param forwarding If `true`, the server forwards all messages to the
   * clients. Otherwise, clients only see messages sent from the server
   * (ignoring other peers).
   * @param retransmission If `true`, the library handles retransmission for
   * you, so there should be no packet loss.
   * @param maxPlayers Maximum number of allowed players. If your game only
   * supports -for example- two players, set this to `2` as it will make
   * transfers faster.
   * @param timeout Number of *frames* without receiving *any* data to reset the
   * connection.
   * @param interval Number of *1024-cycle ticks* (61.04μs) between transfers
   * *(50 = 3.052ms)*. It's the interval of Timer #`sendTimerId`. Lower values
   * will transfer faster but also consume more CPU.
   * @param sendTimerId GBA Timer to use for sending.
   * \warning You can use `Link::perFrame(...)` to convert from *packets per
   * frame* to *interval values*.
   */
  explicit LinkWireless(bool forwarding = true,
                        bool retransmission = true,
                        u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS,
                        u32 timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                        u16 interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                        u8 sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID) {
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    maxPlayers = 2;
#endif

    this->config.forwarding = forwarding;
    this->config.retransmission = retransmission;
    this->config.maxPlayers = maxPlayers;
    this->config.timeout = timeout;
    this->config.interval = interval;
    this->config.sendTimerId = sendTimerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library. When an adapter is connected, it changes the
   * state to `AUTHENTICATED`. It can also be used to disconnect or reset the
   * adapter.
   */
  bool activate() {
    lastError = NONE;
    isEnabled = false;

    LINK_WIRELESS_BARRIER;
    bool success = reset();
    LINK_WIRELESS_BARRIER;

    isEnabled = true;
    return success;
  }

  /**
   * @brief Puts the adapter into a low consumption mode and then deactivates
   * the library. It returns a boolean indicating whether the transition to low
   * consumption mode was successful.
   * @param turnOff Whether the library should put the adapter in the low
   * consumption mode or not before deactivation. Defaults to `true`.
   */
  bool deactivate(bool turnOff = true) {
    bool success = true;

    if (turnOff) {
      activate();
      success = sendCommand(COMMAND_BYE).success;
    }

    lastError = NONE;
    isEnabled = false;
    resetState();
    stop();

    return success;
  }

  /**
   * @brief Starts broadcasting a server and changes the state to `SERVING`. You
   * can optionally provide data that games will be able to read. If the adapter
   * is already serving, this method only updates the broadcast data.
   * @param gameName Game name. Maximum `14` characters + null terminator.
   * @param userName User name. Maximum `8` characters + null terminator.
   * @param gameId `(0 ~ 0x7FFF)` Game ID.
   * \warning Updating broadcast data while serving can fail if the adapter is
   * busy. In that case, this will return `false` and `getLastError()` will be
   * `BUSY_TRY_AGAIN`.
   */
  bool serve(const char* gameName = "",
             const char* userName = "",
             u16 gameId = LINK_WIRELESS_MAX_GAME_ID) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED && state != SERVING) {
      lastError = WRONG_STATE;
      return false;
    }
    if (std::strlen(gameName) > LINK_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = GAME_NAME_TOO_LONG;
      return false;
    }
    if (std::strlen(userName) > LINK_WIRELESS_MAX_GAME_NAME_LENGTH) {
      lastError = USER_NAME_TOO_LONG;
      return false;
    }

    isSendingSyncCommand = true;
    if (asyncCommand.isActive) {
      lastError = BUSY_TRY_AGAIN;
      isSendingSyncCommand = false;
      return false;
    }

    char finalGameName[LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1];
    char finalUserName[LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1];
    copyName(finalGameName, gameName, LINK_WIRELESS_MAX_GAME_NAME_LENGTH);
    copyName(finalUserName, userName, LINK_WIRELESS_MAX_USER_NAME_LENGTH);

    if (state != SERVING)
      setup(config.maxPlayers);

    addData(buildU32(buildU16(finalGameName[1], finalGameName[0]),
                     gameId & LINK_WIRELESS_MAX_GAME_ID),
            true);
    addData(buildU32(buildU16(finalGameName[5], finalGameName[4]),
                     buildU16(finalGameName[3], finalGameName[2])));
    addData(buildU32(buildU16(finalGameName[9], finalGameName[8]),
                     buildU16(finalGameName[7], finalGameName[6])));
    addData(buildU32(buildU16(finalGameName[13], finalGameName[12]),
                     buildU16(finalGameName[11], finalGameName[10])));
    addData(buildU32(buildU16(finalUserName[3], finalUserName[2]),
                     buildU16(finalUserName[1], finalUserName[0])));
    addData(buildU32(buildU16(finalUserName[7], finalUserName[6]),
                     buildU16(finalUserName[5], finalUserName[4])));

    bool success = sendCommand(COMMAND_BROADCAST, true).success;

    if (state != SERVING)
      success = success && sendCommand(COMMAND_START_HOST).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    wait(TRANSFER_WAIT);
    state = SERVING;

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  bool getServers(Server servers[]) {
    return getServers(servers, []() {});
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers.
   * @param servers The array to be filled with data.
   * @param onWait A function which will be invoked each time VBlank starts.
   * \warning This action takes 1 second to complete.
   * \warning For an async version, see `getServersAsyncStart()`.
   */
  template <typename F>
  bool getServers(Server servers[], F onWait) {
    if (!getServersAsyncStart())
      return false;

    waitVBlanks(BROADCAST_SEARCH_WAIT_FRAMES, onWait);

    if (!getServersAsyncEnd(servers))
      return false;

    return true;
  }

  /**
   * @brief Starts looking for broadcasting servers and changes the state to
   * `SEARCHING`. After this, call `getServersAsyncEnd(...)` 1 second later.
   */
  bool getServersAsyncStart() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    bool success = sendCommand(COMMAND_BROADCAST_READ_START).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = SEARCHING;

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers. Changes the state to `AUTHENTICATED` again.
   * @param servers The array to be filled with data.
   */
  bool getServersAsyncEnd(Server servers[]) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != SEARCHING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result = sendCommand(COMMAND_BROADCAST_READ_POLL);
    bool success1 =
        result.success &&
        result.responsesSize % LINK_WIRELESS_BROADCAST_RESPONSE_LENGTH == 0;

    if (!success1) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    bool success2 = sendCommand(COMMAND_BROADCAST_READ_END).success;

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
      server.gameId = result.responses[start + 1] & LINK_WIRELESS_MAX_GAME_ID;
      u32 gameI = 0, userI = 0;
      recoverName(server.gameName, gameI, result.responses[start + 1], false);
      recoverName(server.gameName, gameI, result.responses[start + 2]);
      recoverName(server.gameName, gameI, result.responses[start + 3]);
      recoverName(server.gameName, gameI, result.responses[start + 4]);
      recoverName(server.userName, userI, result.responses[start + 5]);
      recoverName(server.userName, userI, result.responses[start + 6]);
      server.gameName[gameI] = '\0';
      server.userName[userI] = '\0';
      u8 connectedClients = (result.responses[start] >> 16) & 0xff;
      server.currentPlayerCount =
          connectedClients == 0xff ? 0 : (1 + connectedClients);

      servers[i] = server;
    }

    state = AUTHENTICATED;

    return true;
  }

  /**
   * @brief Starts a connection with `serverId` and changes the state to
   * `CONNECTING`.
   * @param serverId Device ID of the server.
   */
  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != AUTHENTICATED) {
      lastError = WRONG_STATE;
      return false;
    }

    addData(serverId, true);
    bool success = sendCommand(COMMAND_CONNECT, true).success;

    if (!success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    state = CONNECTING;

    return true;
  }

  /**
   * @brief When connecting, this needs to be called until the state is
   * `CONNECTED`. It assigns a player ID. Keep in mind that `isConnected()` and
   * `playerCount()` won't be updated until the first message from the server
   * arrives.
   */
  bool keepConnecting() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (state != CONNECTING) {
      lastError = WRONG_STATE;
      return false;
    }

    auto result1 = sendCommand(COMMAND_IS_FINISHED_CONNECT);
    if (!result1.success || result1.responsesSize == 0) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    if (result1.responses[0] == WAIT_STILL_CONNECTING)
      return true;

    u8 assignedPlayerId = 1 + (u8)msB32(result1.responses[0]);
    if (assignedPlayerId >= LINK_WIRELESS_MAX_PLAYERS) {
      reset();
      lastError = CONNECTION_FAILED;
      return false;
    }

    auto result2 = sendCommand(COMMAND_FINISH_CONNECTION);
    if (!result2.success) {
      reset();
      lastError = COMMAND_FAILED;
      return false;
    }

    sessionState.currentPlayerId = assignedPlayerId;
    state = CONNECTED;

    return true;
  }

  /**
   * @brief Enqueues `data` to be sent to other nodes.
   * @param data The value to be sent.
   */
  bool send(u16 data, int _author = -1) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive()) {
      lastError = WRONG_STATE;
      return false;
    }

    if (!_canSend()) {
      if (_author < 0)
        lastError = BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId = _author >= 0 ? _author : sessionState.currentPlayerId;
    message.data = data;

    sessionState.newOutgoingMessages.syncPush(message);

    return true;
  }

  /**
   * @brief Fills the `messages` array with incoming messages, forwarding if
   * needed.
   * @param messages The array to be filled with data.
   */
  bool receive(Message messages[]) {
    if (!isEnabled || state == NEEDS_RESET || !isSessionActive())
      return false;

    LINK_WIRELESS_BARRIER;
    sessionState.incomingMessages.startReading();
    LINK_WIRELESS_BARRIER;

    u32 i = 0;
    while (!sessionState.incomingMessages.isEmpty()) {
      auto message = sessionState.incomingMessages.pop();
      messages[i] = message;
#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
      forwardMessageIfNeeded(message);
#endif
      i++;
    }

    LINK_WIRELESS_BARRIER;
    sessionState.incomingMessages.stopReading();
    LINK_WIRELESS_BARRIER;

    return true;
  }

  /**
   * @brief Returns the current state.
   * @return One of the enum values from `LinkWireless::State`.
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
   * @brief Returns the number of connected players.
   */
  [[nodiscard]] u8 playerCount() { return sessionState.playerCount; }

  /**
   * @brief Returns the current player ID.
   */
  [[nodiscard]] u8 currentPlayerId() { return sessionState.currentPlayerId; }

  /**
   * @brief If one of the other methods returns `false`, you can inspect this to
   * know the cause. After this call, the last error is cleared if `clear` is
   * `true` (default behavior).
   * @param clear Whether it should clear the error or not.
   */
  Error getLastError(bool clear = true) {
    Error error = lastError;
    if (clear)
      lastError = NONE;
    return error;
  }

  ~LinkWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

  /**
   * @brief Returns whether it's running an async command or not.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _hasActiveAsyncCommand() { return asyncCommand.isActive; }

  /**
   * @brief Returns whether there's room for new outgoing messages or not.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _canSend() {
    return !sessionState.outgoingMessages.isFull();
  }

  /**
   * @brief Returns the number of pending outgoing messages.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _getPendingCount() {
    return sessionState.outgoingMessages.size();
  }

  /**
   * @brief Returns the last packet ID.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketId() { return sessionState.lastPacketId; }

  /**
   * @brief Returns the last confirmation received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastConfirmationFromClient1() {
    return sessionState.lastConfirmationFromClients[1];
  }

  /**
   * @brief Returns the last packet ID received from player ID 1.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketIdFromClient1() {
    return sessionState.lastPacketIdFromClients[1];
  }

  /**
   * @brief Returns the last confirmation received from the server.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastConfirmationFromServer() {
    return sessionState.lastConfirmationFromServer;
  }

  /**
   * @brief Returns the last packet ID received from the server.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _lastPacketIdFromServer() {
    return sessionState.lastPacketIdFromServer;
  }

  /**
   * @brief Returns the next pending packet ID.
   * \warning This is internal API!
   */
  [[nodiscard]] u32 _nextPendingPacketId() {
    return sessionState.outgoingMessages.isEmpty()
               ? 0
               : sessionState.outgoingMessages.peek().packetId;
  }

  /**
   * @brief This method is called by the VBLANK interrupt handler.
   * \warning This is internal API!
   */
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  __attribute__((noinline)) void _onVBlank() {
#else
  void _onVBlank() {
#endif
    if (!isEnabled)
      return;

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    if (interrupt) {
      pendingVBlank = true;
      return;
    }
#endif
#endif

#ifdef PROFILING_ENABLED
    profileStart();
#endif

    if (!isSessionActive())
      return;

    if (isConnected() && !sessionState.recvFlag)
      sessionState.recvTimeout++;
    if (sessionState.recvTimeout >= config.timeout) {
      reset();
      lastError = TIMEOUT;
      return;
    }

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
    trackRemoteTimeouts();
    if (!checkRemoteTimeouts()) {
      reset();
      lastError = REMOTE_TIMEOUT;
      return;
    }
#endif

    sessionState.recvFlag = false;
    sessionState.acceptCalled = false;
    sessionState.pingSent = false;

#ifdef PROFILING_ENABLED
    lastVBlankTime = profileStop();
    lastFrameSerialIRQs = serialIRQCount;
    lastFrameTimerIRQs = timerIRQCount;
    serialIRQCount = 0;
    timerIRQCount = 0;
#endif
  }

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
  void _onSerial();
  void _onTimer();
#else
  void _onSerial() { __onSerial(); }
  void _onTimer() { __onTimer(); }
#endif

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  LINK_WIRELESS_ALWAYS_INLINE void __onSerial() {
    if (!isEnabled)
      return;

#ifdef PROFILING_ENABLED
    profileStart();
#endif

    linkSPI->_onSerial(true);

    bool hasNewData = linkSPI->getAsyncState() == LinkSPI::AsyncState::READY;
    if (hasNewData) {
      if (!acknowledge()) {
        reset();
        lastError = ACKNOWLEDGE_FAILED;
        return;
      }
    } else
      return;
    u32 newData = linkSPI->getAsyncData();

    if (!isSessionActive())
      return;

    if (asyncCommand.isActive) {
      if (asyncCommand.state == AsyncCommand::State::PENDING) {
        updateAsyncCommand(newData);

        if (asyncCommand.state == AsyncCommand::State::COMPLETED)
          processAsyncCommand();
      }
    }

#ifdef PROFILING_ENABLED
    lastSerialTime = profileStop();
    serialIRQCount++;
#endif
  }

  /**
   * @brief This method is called by the TIMER interrupt handler.
   * \warning This is internal API!
   */
  LINK_WIRELESS_ALWAYS_INLINE void __onTimer() {
    if (!isEnabled)
      return;

#ifdef PROFILING_ENABLED
    profileStart();
#endif

    if (!isSessionActive())
      return;

    if (!asyncCommand.isActive)
      acceptConnectionsOrTransferData();

#ifdef PROFILING_ENABLED
    lastTimerTime = profileStop();
    timerIRQCount++;
#endif
  }

  struct Config {
    bool forwarding;
    bool retransmission;
    u8 maxPlayers;
    u32 timeout;
    u32 interval;
    u32 sendTimerId;
  };

  /**
   * @brief LinkWireless configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  using MessageQueue = Link::Queue<Message, LINK_WIRELESS_QUEUE_SIZE, false>;

  struct SessionState {
    MessageQueue incomingMessages;     // read by user, write by irq&user
    MessageQueue outgoingMessages;     // read and write by irq
    MessageQueue newIncomingMessages;  // read and write by irq
    MessageQueue newOutgoingMessages;  // read by irq, write by user&irq

    u32 recvTimeout = 0;                         // (~= LinkCable::IRQTimeout)
    u32 msgTimeouts[LINK_WIRELESS_MAX_PLAYERS];  // (~= LinkCable::msgTimeouts)
    bool recvFlag = false;                       // (~= LinkCable::IRQFlag)
    bool msgFlags[LINK_WIRELESS_MAX_PLAYERS];    // (~= LinkCable::msgFlags)

    bool acceptCalled = false;
    bool pingSent = false;
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
    bool sendReceiveLatch = false;
    bool shouldWaitForServer = false;
#endif

    u8 playerCount = 1;
    u8 currentPlayerId = 0;

    bool didReceiveLastPacketIdFromServer = false;
    u32 lastPacketId = 0;
    u32 lastPacketIdFromServer = 0;
    u32 lastConfirmationFromServer = 0;
    u32 lastPacketIdFromClients[LINK_WIRELESS_MAX_PLAYERS];
    u32 lastConfirmationFromClients[LINK_WIRELESS_MAX_PLAYERS];
  };

  struct MessageHeader {
    unsigned int partialPacketId : PACKET_ID_BITS;
    unsigned int isConfirmation : 1;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    unsigned int playerId : 1;
    unsigned int quickData : 5;
#else
    unsigned int playerId : 3;
    unsigned int clientCount : 2;
#endif
    unsigned int dataChecksum : 4;
  };

  union MessageHeaderSerializer {
    MessageHeader asStruct;
    u16 asInt;
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
    u32 parameters[LINK_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
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
  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  State state = NEEDS_RESET;
  u32 nextCommandData[LINK_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
  u32 nextCommandDataSize = 0;
  u32 nextAsyncCommandData[LINK_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
  u32 nextAsyncCommandDataSize = 0;
  volatile bool isSendingSyncCommand = false;
  Error lastError = NONE;
  volatile bool isEnabled = false;

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  volatile bool interrupt = false, pendingVBlank = false;
#endif
#endif

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
  void forwardMessageIfNeeded(Message& message) {
    if (state == SERVING && config.forwarding && sessionState.playerCount > 2)
      send(message.data, message.playerId);
  }
#endif

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  void irqEnd() {
    interrupt = false;
    LINK_WIRELESS_BARRIER;
    if (pendingVBlank) {
      _onVBlank();
      pendingVBlank = false;
    }
  }
#endif
#endif

  void processAsyncCommand() {  // (irq only)
    if (!asyncCommand.result.success) {
      if (asyncCommand.type == COMMAND_SEND_DATA)
        lastError = SEND_DATA_FAILED;
      else if (asyncCommand.type == COMMAND_RECEIVE_DATA)
        lastError = RECEIVE_DATA_FAILED;
      else
        lastError = COMMAND_FAILED;

      reset();
      return;
    }

    asyncCommand.isActive = false;

    switch (asyncCommand.type) {
      case COMMAND_ACCEPT_CONNECTIONS: {
        // AcceptConnections (end)
        sessionState.playerCount = Link::_min(
            1 + asyncCommand.result.responsesSize, config.maxPlayers);

        break;
      }
      case COMMAND_SEND_DATA: {
        // SendData (end)

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        if (state == CONNECTED)
          sessionState.shouldWaitForServer = true;
        sessionState.sendReceiveLatch = !sessionState.sendReceiveLatch;
#else
        if (state == SERVING) {
          // ReceiveData (start)
          sendCommandAsync(COMMAND_RECEIVE_DATA);
        }
#endif

        break;
      }
      case COMMAND_RECEIVE_DATA: {
        // ReceiveData (end)

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        sessionState.sendReceiveLatch =
            sessionState.shouldWaitForServer || !sessionState.sendReceiveLatch;
#endif

        if (asyncCommand.result.responsesSize == 0)
          break;

        sessionState.recvFlag = true;
        sessionState.recvTimeout = 0;

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        sessionState.shouldWaitForServer = false;
#endif

        addIncomingMessagesFromData(asyncCommand.result);

#ifndef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        if (state == CONNECTED) {
          // SendData (start)
          sendPendingData();
        }
#endif

        break;
      }
      default: {
      }
    }
  }

  void acceptConnectionsOrTransferData() {  // (irq only)
    if (state == SERVING && !sessionState.acceptCalled &&
        sessionState.playerCount < config.maxPlayers) {
      // AcceptConnections (start)
      if (sendCommandAsync(COMMAND_ACCEPT_CONNECTIONS))
        sessionState.acceptCalled = true;
    } else if (state == CONNECTED || isConnected()) {
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
      bool shouldReceive =
          !sessionState.sendReceiveLatch || sessionState.shouldWaitForServer;
#else
      bool shouldReceive = state == CONNECTED;
#endif

      if (shouldReceive) {
        // ReceiveData (start)
        sendCommandAsync(COMMAND_RECEIVE_DATA);
      } else {
        // SendData (start)
        sendPendingData();
      }
    }
  }

  void sendPendingData() {  // (irq only)
    copyOutgoingState();
    int lastPacketId = setDataFromOutgoingMessages();
    if (sendCommandAsync(COMMAND_SEND_DATA, true))
      clearOutgoingMessagesIfNeeded(lastPacketId);
  }

  int setDataFromOutgoingMessages() {  // (irq only)
    u32 maxTransferLength = getDeviceTransferLength();

    addAsyncData(0, true);

    if (config.retransmission)
      addConfirmations();
    else
      addPingMessageIfNeeded();

    int lastPacketId = -1;

    sessionState.outgoingMessages.forEach([this, maxTransferLength,
                                           &lastPacketId](Message message) {
      u16 header = buildMessageHeader(message.playerId, message.packetId,
                                      buildChecksum(message.data));
      u32 rawMessage = buildU32(header, message.data);

      if (nextAsyncCommandDataSize /* -1 (wireless header) + 1 (rawMessage) */ >
          maxTransferLength)
        return false;

      addAsyncData(rawMessage);
      lastPacketId = message.packetId;

      return true;
    });

    // (add wireless header)
    u32 bytes = (nextAsyncCommandDataSize - 1) * 4;
    nextAsyncCommandData[0] =
        sessionState.currentPlayerId == 0
            ? bytes
            : bytes << (3 + sessionState.currentPlayerId * 5);

    return lastPacketId;
  }

  void addIncomingMessagesFromData(CommandResult& result) {  // (irq only)
    for (u32 i = 1; i < result.responsesSize; i++) {
      u32 rawMessage = result.responses[i];
      u16 headerInt = msB32(rawMessage);
      u16 data = lsB32(rawMessage);

      MessageHeaderSerializer serializer;
      serializer.asInt = headerInt;

      MessageHeader header = serializer.asStruct;
      u32 partialPacketId = header.partialPacketId;
      bool isConfirmation = header.isConfirmation;
      u8 remotePlayerId = Link::_min(header.playerId, config.maxPlayers - 1);
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
      QUICK_RECEIVE = header.quickData;
      u8 remotePlayerCount = 2;
#else
      u8 remotePlayerCount = LINK_WIRELESS_MIN_PLAYERS + header.clientCount;
#endif
      u32 checksum = header.dataChecksum;
      bool isPing = data == MSG_PING;

      sessionState.msgTimeouts[0] = 0;
      sessionState.msgTimeouts[remotePlayerId] = 0;
      sessionState.msgFlags[0] = true;
      sessionState.msgFlags[remotePlayerId] = true;

      if (checksum != buildChecksum(data))
        continue;

      Message message;
      message.packetId = partialPacketId;
      message.data = data;
      message.playerId = remotePlayerId;

      if (!acceptMessage(message, isConfirmation, remotePlayerCount) || isPing)
        continue;

      if (config.retransmission && isConfirmation) {
        if (!handleConfirmation(message))
          continue;
      } else {
        sessionState.newIncomingMessages.push(message);
      }
    }
    copyIncomingState();
  }

  bool acceptMessage(Message& message,
                     bool isConfirmation,
                     u32 remotePlayerCount) {  // (irq only)
    if (state == SERVING) {
      u32 expectedPacketId =
          (sessionState.lastPacketIdFromClients[message.playerId] + 1) %
          MAX_PACKET_IDS;

      if (config.retransmission && !isConfirmation &&
          message.packetId != expectedPacketId)
        return false;

      if (!isConfirmation)
        message.packetId =
            ++sessionState.lastPacketIdFromClients[message.playerId];
    } else {
      u32 expectedPacketId =
          (sessionState.lastPacketIdFromServer + 1) % MAX_PACKET_IDS;

      if (config.retransmission && !isConfirmation &&
          message.packetId != expectedPacketId)
        return false;

      sessionState.playerCount = remotePlayerCount;

      if (!isConfirmation)
        message.packetId = ++sessionState.lastPacketIdFromServer;
    }

    bool isMessageFromCurrentPlayer =
        !isConfirmation && message.playerId == sessionState.currentPlayerId;

    return !isMessageFromCurrentPlayer;
  }

  void clearOutgoingMessagesIfNeeded(int lastPacketId) {  // (irq only)
    if (!config.retransmission && lastPacketId > -1)
      removeConfirmedMessages(lastPacketId);
  }

  void addPingMessageIfNeeded() {  // (irq only)
    if (sessionState.outgoingMessages.isEmpty() && !sessionState.pingSent) {
      Message pingMessage;
      pingMessage.packetId = newPacketId();
      pingMessage.playerId = sessionState.currentPlayerId;
      pingMessage.data = MSG_PING;
      sessionState.outgoingMessages.push(pingMessage);
      sessionState.pingSent = true;
    }
  }

  void addConfirmations() {  // (irq only)
    if (state == SERVING) {
#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
      if (config.maxPlayers > 2 &&
          (sessionState.lastPacketIdFromClients[1] == 0 ||
           sessionState.lastPacketIdFromClients[2] == 0 ||
           sessionState.lastPacketIdFromClients[3] == 0 ||
           sessionState.lastPacketIdFromClients[4] == 0)) {
        u32 lastPacketId = sessionState.lastPacketId;
        u16 header = buildConfirmationHeader(0, lastPacketId);
        u32 rawMessage = buildU32(header, lastPacketId & 0xffff);
        addAsyncData(rawMessage);
      }
#endif

      for (int i = 0; i < config.maxPlayers - 1; i++) {
        u32 confirmationData = sessionState.lastPacketIdFromClients[1 + i];
        u16 header = buildConfirmationHeader(1 + i, confirmationData);
        u32 rawMessage = buildU32(header, confirmationData & 0xffff);
        addAsyncData(rawMessage);
      }
    } else {
      u32 confirmationData = sessionState.lastPacketIdFromServer;
      u16 header = buildConfirmationHeader(sessionState.currentPlayerId,
                                           confirmationData);
      u32 rawMessage = buildU32(header, confirmationData & 0xffff);
      addAsyncData(rawMessage);
    }
  }

  bool handleConfirmation(Message confirmation) {  // (irq only)
    u32 confirmationData = (confirmation.packetId << 16) | confirmation.data;

    if (state == CONNECTED) {
      if (confirmation.playerId == 0 &&
          !sessionState.didReceiveLastPacketIdFromServer) {
        sessionState.lastPacketIdFromServer = confirmationData;
        sessionState.didReceiveLastPacketIdFromServer = true;
      } else if (confirmation.playerId == sessionState.currentPlayerId) {
        handleServerConfirmation(confirmationData);
      } else {
        return false;
      }
    } else {
      handleClientConfirmation(confirmationData, confirmation.playerId);
    }

    return true;
  }

  void handleServerConfirmation(u32 confirmationData) {  // (irq only)
    sessionState.lastConfirmationFromServer = confirmationData;
    removeConfirmedMessages(confirmationData);
  }

  void handleClientConfirmation(u32 confirmationData,
                                u8 playerId) {  // (irq only)
    sessionState.lastConfirmationFromClients[playerId] = confirmationData;

    u32 min = 0xffffffff;
    for (int i = 0; i < config.maxPlayers - 1; i++) {
      u32 _confirmationData = sessionState.lastConfirmationFromClients[1 + i];
      if (_confirmationData > 0 && _confirmationData < min)
        min = _confirmationData;
    }
    if (min < 0xffffffff)
      removeConfirmedMessages(min);
  }

  void removeConfirmedMessages(u32 confirmationData) {  // (irq only)
    while (!sessionState.outgoingMessages.isEmpty() &&
           sessionState.outgoingMessages.peek().packetId <= confirmationData)
      sessionState.outgoingMessages.pop();
  }

  u16 buildConfirmationHeader(u8 playerId,
                              u32 confirmationData) {  // (irq only)
    // confirmation messages "repurpose" some message header fields:
    //     packetId => high 6 bits of confirmation
    //     data     => low 16 bits of confirmation
    u8 highPart = (confirmationData >> 16) & PACKET_ID_MASK;
    u16 lowPart = confirmationData & 0xffff;
    return buildMessageHeader(playerId, highPart, buildChecksum(lowPart), true);
  }

  u16 buildMessageHeader(u8 playerId,
                         u32 packetId,
                         u8 dataChecksum,
                         bool isConfirmation = false) {  // (irq only)
    MessageHeader header;
    header.partialPacketId = packetId % MAX_PACKET_IDS;
    header.isConfirmation = isConfirmation;
    header.playerId = playerId;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    header.quickData = QUICK_SEND;
#else
    header.clientCount = sessionState.playerCount - LINK_WIRELESS_MIN_PLAYERS;
#endif
    header.dataChecksum = dataChecksum;

    MessageHeaderSerializer serializer;
    serializer.asStruct = header;
    return serializer.asInt;
  }

  u32 buildChecksum(u16 data) {  // (irq only)
    // (hamming weight)
    return __builtin_popcount(data) % 16;
  }

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
  void trackRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < sessionState.playerCount; i++) {
      if (i != sessionState.currentPlayerId && !sessionState.msgFlags[i])
        sessionState.msgTimeouts[i]++;
      sessionState.msgFlags[i] = false;
    }
  }

  bool checkRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < sessionState.playerCount; i++) {
      if ((i == 0 || state == SERVING) &&
          sessionState.msgTimeouts[i] > config.timeout)
        return false;
    }

    return true;
  }
#endif

  u32 getDeviceTransferLength() {  // (irq only)
    return state == SERVING ? LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
                            : LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH;
  }

  void copyOutgoingState() {  // (irq only)
    if (sessionState.newOutgoingMessages.isWriting())
      return;

    while (!sessionState.newOutgoingMessages.isEmpty()) {
      if (!_canSend())
        break;

      auto message = sessionState.newOutgoingMessages.pop();
      message.packetId = newPacketId();
      sessionState.outgoingMessages.push(message);
    }
  }

  void copyIncomingState() {  // (irq only)
    if (sessionState.newIncomingMessages.isReading())
      return;

    while (!sessionState.newIncomingMessages.isEmpty()) {
      auto message = sessionState.newIncomingMessages.pop();
      sessionState.incomingMessages.push(message);
    }
  }

  u32 newPacketId() {  // (irq only)
    return ++sessionState.lastPacketId;
  }

  void addData(u32 value, bool start = false) {
    if (start)
      nextCommandDataSize = 0;
    nextCommandData[nextCommandDataSize] = value;
    nextCommandDataSize++;
  }

  void addAsyncData(u32 value, bool start = false) {
    if (start)
      nextAsyncCommandDataSize = 0;
    nextAsyncCommandData[nextAsyncCommandDataSize] = value;
    nextAsyncCommandDataSize++;
  }

  void copyName(char* target, const char* source, u32 length) {
    u32 len = std::strlen(source);

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

  bool reset() {
    bool wasEnabled = isEnabled;

    LINK_WIRELESS_BARRIER;
    isEnabled = false;
    LINK_WIRELESS_BARRIER;

    resetState();
    stop();
    bool success = start();

    if (!success)
      stop();

    LINK_WIRELESS_BARRIER;
    isEnabled = wasEnabled;
    LINK_WIRELESS_BARRIER;

    return success;
  }

  void resetState() {
    this->state = NEEDS_RESET;
    this->asyncCommand.isActive = false;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    QUICK_SEND = 0;
    QUICK_RECEIVE = 0;
#endif
    this->sessionState.playerCount = 1;
    this->sessionState.currentPlayerId = 0;
    this->sessionState.recvFlag = false;
    this->sessionState.recvTimeout = 0;
    this->sessionState.acceptCalled = false;
    this->sessionState.pingSent = false;
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
    this->sessionState.sendReceiveLatch = false;
    this->sessionState.shouldWaitForServer = false;
#endif
    this->sessionState.didReceiveLastPacketIdFromServer = false;
    this->sessionState.lastPacketId = 0;
    this->sessionState.lastPacketIdFromServer = 0;
    this->sessionState.lastConfirmationFromServer = 0;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++) {
      this->sessionState.msgTimeouts[i] = 0;
      this->sessionState.msgFlags[i] = 0;
      this->sessionState.lastPacketIdFromClients[i] = 0;
      this->sessionState.lastConfirmationFromClients[i] = 0;
    }
    this->nextCommandDataSize = 0;
    this->nextAsyncCommandDataSize = 0;

    this->sessionState.incomingMessages.syncClear();
    this->sessionState.outgoingMessages.clear();

    this->sessionState.newIncomingMessages.clear();
    this->sessionState.newOutgoingMessages.syncClear();

    isSendingSyncCommand = false;
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

    wait(TRANSFER_WAIT);

    if (!sendCommand(COMMAND_HELLO).success)
      return false;

    if (!setup())
      return false;

    linkSPI->activate(LinkSPI::Mode::MASTER_2MBPS);
    state = AUTHENTICATED;

    return true;
  }

  void stopTimer() {
    Link::_REG_TM[config.sendTimerId].cnt =
        Link::_REG_TM[config.sendTimerId].cnt & (~Link::_TM_ENABLE);
  }

  void startTimer() {
    Link::_REG_TM[config.sendTimerId].start = -config.interval;
    Link::_REG_TM[config.sendTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
  }

  void pingAdapter() {
    linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    linkGPIO->writePin(LinkGPIO::SD, true);
    wait(PING_WAIT);
    linkGPIO->writePin(LinkGPIO::SD, false);
  }

  bool login() {
    LoginMemory memory;

    if (!exchangeLoginPacket(LOGIN_PARTS[0], 0, memory))
      return false;

    for (u32 i = 0; i < LOGIN_STEPS; i++) {
      if (!exchangeLoginPacket(LOGIN_PARTS[i], LOGIN_PARTS[i], memory))
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

  bool setup(u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS) {
    addData(SETUP_MAGIC | (((LINK_WIRELESS_MAX_PLAYERS - maxPlayers) & 0b11)
                           << SETUP_MAX_PLAYERS_BIT),
            true);
    return sendCommand(COMMAND_SETUP, true).success;
  }

  CommandResult sendCommand(u8 type, bool withData = false) {
    CommandResult result;
    u32 command = buildCommand(type, withData ? (u16)nextCommandDataSize : 0);

    if (transfer(command) != DATA_REQUEST_VALUE) {
      isSendingSyncCommand = false;
      return result;
    }

    if (withData) {
      for (u32 i = 0; i < nextCommandDataSize; i++) {
        if (transfer(nextCommandData[i]) != DATA_REQUEST_VALUE) {
          isSendingSyncCommand = false;
          return result;
        }
      }
    }

    u32 response = transfer(DATA_REQUEST_VALUE);
    u16 header = msB32(response);
    u16 data = lsB32(response);
    u8 responses = msB16(data);
    u8 ack = lsB16(data);

    if (header != COMMAND_HEADER_VALUE || ack != type + RESPONSE_ACK_VALUE ||
        responses > LINK_WIRELESS_MAX_COMMAND_RESPONSE_LENGTH) {
      isSendingSyncCommand = false;
      return result;
    }

    for (u32 i = 0; i < responses; i++)
      result.responses[i] = transfer(DATA_REQUEST_VALUE);
    result.responsesSize = responses;

    result.success = true;

    LINK_WIRELESS_BARRIER;
    isSendingSyncCommand = false;
    LINK_WIRELESS_BARRIER;

    return result;
  }

  bool sendCommandAsync(u8 type, bool withData = false) {  // (irq only)
    if (asyncCommand.isActive || isSendingSyncCommand)
      return false;

    asyncCommand.type = type;
    if (withData) {
      for (u32 i = 0; i < nextAsyncCommandDataSize; i++)
        asyncCommand.parameters[i] = nextAsyncCommandData[i];
    }
    asyncCommand.result.success = false;
    asyncCommand.state = AsyncCommand::State::PENDING;
    asyncCommand.step = AsyncCommand::Step::COMMAND_HEADER;
    asyncCommand.sentParameters = 0;
    asyncCommand.totalParameters = withData ? nextAsyncCommandDataSize : 0;
    asyncCommand.receivedResponses = 0;
    asyncCommand.totalResponses = 0;
    asyncCommand.isActive = true;

    u32 command = buildCommand(type, asyncCommand.totalParameters);
    transferAsync(command);

    return true;
  }

  void updateAsyncCommand(u32 newData) {  // (irq only)
    switch (asyncCommand.step) {
      case AsyncCommand::Step::COMMAND_HEADER: {
        if (newData != DATA_REQUEST_VALUE) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendAsyncCommandParameterOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::COMMAND_PARAMETERS: {
        if (newData != DATA_REQUEST_VALUE) {
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

        if (header != COMMAND_HEADER_VALUE ||
            ack != asyncCommand.type + RESPONSE_ACK_VALUE ||
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
      default: {
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
      transferAsync(DATA_REQUEST_VALUE);
    }
  }

  void receiveAsyncCommandResponseOrFinish() {  // (irq only)
    if (asyncCommand.receivedResponses < asyncCommand.totalResponses) {
      asyncCommand.step = AsyncCommand::Step::DATA_REQUEST;
      transferAsync(DATA_REQUEST_VALUE);
    } else {
      asyncCommand.result.success = true;
      asyncCommand.state = AsyncCommand::State::COMPLETED;
    }
  }

  u32 buildCommand(u8 type, u8 length = 0) {
    return buildU32(COMMAND_HEADER_VALUE, buildU16(length, type));
  }

  void transferAsync(u32 data) {  // (irq only)
#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    Link::_REG_IME = 0;
#endif
#endif

    linkSPI->transfer(data, []() { return false; }, true, true);
  }

  u32 transfer(u32 data, bool customAck = true) {
    if (!customAck)
      wait(TRANSFER_WAIT);

    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;
    u32 receivedData = linkSPI->transfer(
        data, [this, &lines, &vCount]() { return cmdTimeout(lines, vCount); },
        false, customAck);

    if (customAck && !acknowledge())
      return LINK_SPI_NO_DATA_32;

    return receivedData;
  }

  bool acknowledge() {
    u32 lines = 0;
    u32 vCount = Link::_REG_VCOUNT;

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

#ifdef PROFILING_ENABLED
  void profileStart() {
    Link::_REG_TM1CNT_L = 0;
    Link::_REG_TM2CNT_L = 0;

    Link::_REG_TM1CNT_H = 0;
    Link::_REG_TM2CNT_H = 0;

    Link::_REG_TM2CNT_H = Link::_TM_ENABLE | Link::_TM_CASCADE;
    Link::_REG_TM1CNT_H = Link::_TM_ENABLE | Link::_TM_FREQ_1;
  }

  u32 profileStop() {
    Link::_REG_TM1CNT_H = 0;
    Link::_REG_TM2CNT_H = 0;

    return (Link::_REG_TM1CNT_L | (Link::_REG_TM2CNT_L << 16));
  }

 public:
  u32 toMs(u32 cycles) {
    // CPU Frequency * time per frame = cycles per frame
    // 16780000 * (1/60) ~= 279666
    return (cycles * 1000) / (279666 * 60);
  }
#endif
};

extern LinkWireless* linkWireless;

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_WIRELESS_ISR_VBLANK() {
  linkWireless->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_WIRELESS_ISR_SERIAL() {
  linkWireless->_onSerial();
}

/**
 * @brief TIMER interrupt handler used for sending.
 */
inline void LINK_WIRELESS_ISR_TIMER() {
  linkWireless->_onTimer();
}

#endif  // LINK_WIRELESS_H
