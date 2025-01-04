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

#include "LinkRawWireless.hpp"

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

static volatile char LINK_WIRELESS_VERSION[] = "LinkWireless/v7.1.0";

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

#define LINK_WIRELESS_RESET_IF_NEEDED                             \
  if (!isEnabled)                                                 \
    return false;                                                 \
  if (linkRawWireless->state == LinkWireless::State::NEEDS_RESET) \
    if (!reset())                                                 \
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
  static constexpr int BROADCAST_SEARCH_WAIT_FRAMES = 60;

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

  using State = LinkRawWireless::State;

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
    stopTimer();
    startTimer();

    if (!linkRawWireless->restoreExistingConnection() ||
        linkRawWireless->sessionState.playerCount > config.maxPlayers) {
      deactivate();
      return false;
    }

    isEnabled = true;
    return true;
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
      success = linkRawWireless->bye();
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
    if (linkRawWireless->state != LinkWireless::State::AUTHENTICATED &&
        linkRawWireless->state != LinkWireless::State::SERVING)
      return badRequest(WRONG_STATE);
    if (LINK_STRLEN(gameName) > LINK_WIRELESS_MAX_GAME_NAME_LENGTH)
      return badRequest(GAME_NAME_TOO_LONG);
    if (LINK_STRLEN(userName) > LINK_WIRELESS_MAX_USER_NAME_LENGTH)
      return badRequest(USER_NAME_TOO_LONG);

    isSendingSyncCommand = true;
    if (asyncCommand.isActive)
      return badRequest(BUSY_TRY_AGAIN);

    if (linkRawWireless->state != LinkWireless::State::SERVING) {
      if (!setup(config.maxPlayers))
        return abort(COMMAND_FAILED);
    }

    bool success =
        linkRawWireless->broadcast(gameName, userName, gameId, false);

    if (linkRawWireless->state != LinkWireless::State::SERVING)
      success = success && linkRawWireless->startHost();

    if (!success)
      return abort(COMMAND_FAILED);

    LINK_WIRELESS_BARRIER;
    isSendingSyncCommand = false;
    LINK_WIRELESS_BARRIER;

    return true;
  }

  /**
   * @brief Closes the server while keeping the session active, to prevent new
   * users from joining the room.
   * \warning Closing the server can fail if the adapter is busy. In that case,
   * this will return `false` and `getLastError()` will be `BUSY_TRY_AGAIN`.
   */
  bool closeServer() {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless->state != LinkWireless::State::SERVING ||
        linkRawWireless->sessionState.isServerClosed)
      return badRequest(WRONG_STATE);

    isSendingSyncCommand = true;
    if (asyncCommand.isActive)
      return badRequest(BUSY_TRY_AGAIN);

    LinkRawWireless::AcceptConnectionsResponse response;
    bool success = linkRawWireless->endHost(response);

    if (!success)
      return abort(COMMAND_FAILED);

    LINK_WIRELESS_BARRIER;
    isSendingSyncCommand = false;
    LINK_WIRELESS_BARRIER;

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
    if (linkRawWireless->state != LinkWireless::State::AUTHENTICATED)
      return badRequest(WRONG_STATE);

    bool success = linkRawWireless->broadcastReadStart();

    if (!success)
      return abort(COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Fills the `servers` array with all the currently broadcasting
   * servers. Changes the state to `AUTHENTICATED` again.
   * @param servers The array to be filled with data.
   */
  bool getServersAsyncEnd(Server servers[]) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless->state != LinkWireless::State::SEARCHING)
      return badRequest(WRONG_STATE);

    LinkRawWireless::BroadcastReadPollResponse response;
    bool success1 = linkRawWireless->broadcastReadPoll(response);

    if (!success1)
      return abort(COMMAND_FAILED);

    bool success2 = linkRawWireless->broadcastReadEnd();

    if (!success2)
      return abort(COMMAND_FAILED);

    auto foundServers = response.servers;
    for (u32 i = 0; i < response.serversSize; i++) {
      Server server;
      server.id = foundServers[i].id;
      server.gameId = foundServers[i].gameId;
      LINK_MEMCPY(server.gameName, foundServers[i].gameName,
                  LINK_WIRELESS_MAX_GAME_NAME_LENGTH + 1);
      LINK_MEMCPY(server.userName, foundServers[i].userName,
                  LINK_WIRELESS_MAX_USER_NAME_LENGTH + 1);
      u8 nextClientNumber = foundServers[i].nextClientNumber;
      server.currentPlayerCount =
          nextClientNumber == 0xff ? 0 : 1 + nextClientNumber;
      servers[i] = server;
    }

    return true;
  }

  /**
   * @brief Starts a connection with `serverId` and changes the state to
   * `CONNECTING`.
   * @param serverId Device ID of the server.
   */
  bool connect(u16 serverId) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (linkRawWireless->state != LinkWireless::State::AUTHENTICATED)
      return badRequest(WRONG_STATE);

    bool success = linkRawWireless->connect(serverId);

    if (!success)
      return abort(COMMAND_FAILED);

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
    if (linkRawWireless->state != LinkWireless::State::CONNECTING)
      return badRequest(WRONG_STATE);

    LinkRawWireless::ConnectionStatus response;
    bool success1 = linkRawWireless->keepConnecting(response);

    if (!success1)
      return abort(COMMAND_FAILED);

    if (response.phase == LinkRawWireless::ConnectionPhase::STILL_CONNECTING)
      return true;
    else if (response.phase == LinkRawWireless::ConnectionPhase::ERROR)
      return abort(COMMAND_FAILED);

    auto success2 = linkRawWireless->finishConnection();
    if (!success2)
      return abort(COMMAND_FAILED);

    return true;
  }

  /**
   * @brief Enqueues `data` to be sent to other nodes.
   * @param data The value to be sent.
   */
  bool send(u16 data, int _author = -1) {
    LINK_WIRELESS_RESET_IF_NEEDED
    if (!isSessionActive())
      return badRequest(WRONG_STATE);

    if (!_canAddNewMessage()) {
      if (_author < 0)
        lastError = BUFFER_IS_FULL;
      return false;
    }

    Message message;
    message.playerId =
        _author >= 0 ? _author : linkRawWireless->sessionState.currentPlayerId;
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
    if (!isSessionActive())
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
  [[nodiscard]] State getState() { return linkRawWireless->state; }

  /**
   * @brief Returns `true` if the player count is higher than `1`.
   */
  [[nodiscard]] bool isConnected() {
    return linkRawWireless->sessionState.playerCount > 1;
  }

  /**
   * @brief Returns `true` if the state is `SERVING` or `CONNECTED`.
   */
  [[nodiscard]] bool isSessionActive() {
    return linkRawWireless->state == LinkWireless::State::SERVING ||
           linkRawWireless->state == LinkWireless::State::CONNECTED;
  }

  /**
   * @brief Returns `true` if the server was closed with `closeServer()`.
   */
  [[nodiscard]] bool isServerClosed() {
    return linkRawWireless->sessionState.isServerClosed;
  }

  /**
   * @brief Returns the number of connected players.
   */
  [[nodiscard]] u8 playerCount() {
    return linkRawWireless->sessionState.playerCount;
  }

  /**
   * @brief Returns the current player ID.
   */
  [[nodiscard]] u8 currentPlayerId() {
    return linkRawWireless->sessionState.currentPlayerId;
  }

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

  /**
   * @brief Restarts the send timer without disconnecting.
   */
  void resetTimer() {
    if (!isEnabled)
      return;

    stopTimer();
    startTimer();
  }

  ~LinkWireless() { delete linkRawWireless; }

  /**
   * @brief Returns whether it's running an async command or not.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _hasActiveAsyncCommand() { return asyncCommand.isActive; }

  /**
   * @brief Returns whether there's room for sending messages or not.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _canSend() {
    return !sessionState.outgoingMessages.isFull();
  }

  /**
   * @brief Returns whether there's room for scheduling new messages or not.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _canAddNewMessage() {
    return !sessionState.newOutgoingMessages.isFull();
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
    if (sessionState.recvTimeout >= config.timeout)
      return (void)abort(TIMEOUT);

#ifndef LINK_WIRELESS_TWO_PLAYERS_ONLY
    trackRemoteTimeouts();
    if (!checkRemoteTimeouts())
      return (void)abort(REMOTE_TIMEOUT);
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

    auto linkSPI = linkRawWireless->linkSPI;
    linkSPI->_onSerial(true);

    bool hasNewData = linkSPI->getAsyncState() == LinkSPI::AsyncState::READY;
    if (hasNewData) {
      if (!linkRawWireless->acknowledge())
        return (void)abort(ACKNOWLEDGE_FAILED);
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
    u32 timeout;     // can be changed in realtime
    u16 interval;    // can be changed in realtime, but call `resetTimer()`
    u8 sendTimerId;  // can be changed in realtime, but call `resetTimer()`
  };

  /**
   * @brief LinkWireless configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

  /**
   * @brief The internal `LinkRawWireless` instance.
   */
  LinkRawWireless* linkRawWireless = new LinkRawWireless();

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
    bool sendReceiveLatch = false;  // true = send ; false = receive
    bool shouldWaitForServer = false;
#endif

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

  using CommandResult = LinkRawWireless::CommandResult;

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
    if (linkRawWireless->state == LinkWireless::State::SERVING &&
        config.forwarding && linkRawWireless->sessionState.playerCount > 2)
      send(message.data, message.playerId);
  }
#endif

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  void irqEnd() {
    Link::_REG_IME = 0;
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
      return (void)abort(asyncCommand.type == LinkRawWireless::COMMAND_SEND_DATA
                             ? SEND_DATA_FAILED
                         : asyncCommand.type ==
                                 LinkRawWireless::COMMAND_RECEIVE_DATA
                             ? RECEIVE_DATA_FAILED
                             : COMMAND_FAILED);
    }

    asyncCommand.isActive = false;

    switch (asyncCommand.type) {
      case LinkRawWireless::COMMAND_ACCEPT_CONNECTIONS: {
        // AcceptConnections (end)
        linkRawWireless->sessionState.playerCount = Link::_min(
            1 + asyncCommand.result.responsesSize, config.maxPlayers);

        break;
      }
      case LinkRawWireless::COMMAND_SEND_DATA: {
        // SendData (end)

#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
        if (linkRawWireless->state == LinkWireless::State::CONNECTED)
          sessionState.shouldWaitForServer = true;
        sessionState.sendReceiveLatch = !sessionState.sendReceiveLatch;
#else
        if (linkRawWireless->state == LinkWireless::State::SERVING) {
          // ReceiveData (start)
          sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
        }
#endif

        break;
      }
      case LinkRawWireless::COMMAND_RECEIVE_DATA: {
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
        if (linkRawWireless->state == LinkWireless::State::CONNECTED) {
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
    if (linkRawWireless->state == LinkWireless::State::SERVING &&
        !linkRawWireless->sessionState.isServerClosed &&
        !sessionState.acceptCalled &&
        linkRawWireless->sessionState.playerCount < config.maxPlayers) {
      // AcceptConnections (start)
      if (sendCommandAsync(LinkRawWireless::COMMAND_ACCEPT_CONNECTIONS))
        sessionState.acceptCalled = true;
    } else if (linkRawWireless->state == LinkWireless::State::CONNECTED ||
               isConnected()) {
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
      bool shouldReceive =
          !sessionState.sendReceiveLatch || sessionState.shouldWaitForServer;
#else
      bool shouldReceive =
          linkRawWireless->state == LinkWireless::State::CONNECTED;
#endif

      if (shouldReceive) {
        // ReceiveData (start)
        sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
      } else {
        // SendData (start)
        sendPendingData();
      }
    }
  }

  void sendPendingData() {  // (irq only)
    copyOutgoingState();
    int lastPacketId = setDataFromOutgoingMessages();
    if (sendCommandAsync(LinkRawWireless::COMMAND_SEND_DATA, true))
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
        linkRawWireless->sessionState.currentPlayerId == 0
            ? bytes
            : bytes << (3 + linkRawWireless->sessionState.currentPlayerId * 5);

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
    if (linkRawWireless->state == LinkWireless::State::SERVING) {
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

      linkRawWireless->sessionState.playerCount = remotePlayerCount;

      if (!isConfirmation)
        message.packetId = ++sessionState.lastPacketIdFromServer;
    }

    bool isMessageFromCurrentPlayer =
        !isConfirmation &&
        message.playerId == linkRawWireless->sessionState.currentPlayerId;

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
      pingMessage.playerId = linkRawWireless->sessionState.currentPlayerId;
      pingMessage.data = MSG_PING;
      sessionState.outgoingMessages.push(pingMessage);
      sessionState.pingSent = true;
    }
  }

  void addConfirmations() {  // (irq only)
    if (linkRawWireless->state == LinkWireless::State::SERVING) {
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
      u16 header = buildConfirmationHeader(
          linkRawWireless->sessionState.currentPlayerId, confirmationData);
      u32 rawMessage = buildU32(header, confirmationData & 0xffff);
      addAsyncData(rawMessage);
    }
  }

  bool handleConfirmation(Message confirmation) {  // (irq only)
    u32 confirmationData = (confirmation.packetId << 16) | confirmation.data;

    if (linkRawWireless->state == LinkWireless::State::CONNECTED) {
      if (confirmation.playerId == 0 &&
          !sessionState.didReceiveLastPacketIdFromServer) {
        sessionState.lastPacketIdFromServer = confirmationData;
        sessionState.didReceiveLastPacketIdFromServer = true;
      } else if (confirmation.playerId ==
                 linkRawWireless->sessionState.currentPlayerId) {
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
    header.clientCount =
        linkRawWireless->sessionState.playerCount - LINK_WIRELESS_MIN_PLAYERS;
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
    for (u32 i = 0; i < linkRawWireless->sessionState.playerCount; i++) {
      if (i != linkRawWireless->sessionState.currentPlayerId &&
          !sessionState.msgFlags[i])
        sessionState.msgTimeouts[i]++;
      sessionState.msgFlags[i] = false;
    }
  }

  bool checkRemoteTimeouts() {  // (irq only)
    for (u32 i = 0; i < linkRawWireless->sessionState.playerCount; i++) {
      if ((i == 0 || linkRawWireless->state == LinkWireless::State::SERVING) &&
          sessionState.msgTimeouts[i] > config.timeout)
        return false;
    }

    return true;
  }
#endif

  u32 getDeviceTransferLength() {  // (irq only)
    return linkRawWireless->state == LinkWireless::State::SERVING
               ? LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH
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
    if (sessionState.incomingMessages.isReading())
      return;

    while (!sessionState.newIncomingMessages.isEmpty()) {
      auto message = sessionState.newIncomingMessages.pop();
      sessionState.incomingMessages.push(message);
    }
  }

  u32 newPacketId() {  // (irq only)
    return ++sessionState.lastPacketId;
  }

  void addAsyncData(u32 value, bool start = false) {
    if (start)
      nextAsyncCommandDataSize = 0;
    nextAsyncCommandData[nextAsyncCommandDataSize] = value;
    nextAsyncCommandDataSize++;
  }

  bool badRequest(Error error) {
    isSendingSyncCommand = false;
    lastError = error;
    return false;
  }

  bool abort(Error error) {
    reset();
    lastError = error;
    return false;
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
    linkRawWireless->resetState();

    this->asyncCommand.isActive = false;
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    QUICK_SEND = 0;
    QUICK_RECEIVE = 0;
#endif
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
    this->nextAsyncCommandDataSize = 0;

    this->sessionState.incomingMessages.syncClear();
    this->sessionState.outgoingMessages.clear();

    this->sessionState.newIncomingMessages.clear();
    this->sessionState.newOutgoingMessages.syncClear();

    isSendingSyncCommand = false;
  }

  void stop() {
    stopTimer();
    linkRawWireless->stop();
  }

  bool start() {
    startTimer();

    if (!linkRawWireless->start())
      return false;

    if (!setup())
      return false;

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

  bool setup(u8 maxPlayers = LINK_WIRELESS_MAX_PLAYERS) {
    return linkRawWireless->setup(maxPlayers);
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

    u32 command =
        linkRawWireless->buildCommand(type, asyncCommand.totalParameters);
    transferAsync(command);

    return true;
  }

  void updateAsyncCommand(u32 newData) {  // (irq only)
    switch (asyncCommand.step) {
      case AsyncCommand::Step::COMMAND_HEADER: {
        if (newData != LinkRawWireless::DATA_REQUEST) {
          asyncCommand.state = AsyncCommand::State::COMPLETED;
          return;
        }

        sendAsyncCommandParameterOrRequestResponse();
        break;
      }
      case AsyncCommand::Step::COMMAND_PARAMETERS: {
        if (newData != LinkRawWireless::DATA_REQUEST) {
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

        if (header != LinkRawWireless::COMMAND_HEADER ||
            ack != asyncCommand.type + LinkRawWireless::RESPONSE_ACK ||
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
      transferAsync(LinkRawWireless::DATA_REQUEST);
    }
  }

  void receiveAsyncCommandResponseOrFinish() {  // (irq only)
    if (asyncCommand.receivedResponses < asyncCommand.totalResponses) {
      asyncCommand.step = AsyncCommand::Step::DATA_REQUEST;
      transferAsync(LinkRawWireless::DATA_REQUEST);
    } else {
      asyncCommand.result.success = true;
      asyncCommand.state = AsyncCommand::State::COMPLETED;
    }
  }

  void transferAsync(u32 data) {  // (irq only)
#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
    Link::_REG_IME = 0;
#endif
#endif

    linkRawWireless->linkSPI->transfer(
        data, []() { return false; }, true, true);
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
 * @brief TIMER interrupt handler.
 */
inline void LINK_WIRELESS_ISR_TIMER() {
  linkWireless->_onTimer();
}

#endif  // LINK_WIRELESS_H
