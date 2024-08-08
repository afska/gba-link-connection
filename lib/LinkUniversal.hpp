#ifndef LINK_UNIVERSAL_H
#define LINK_UNIVERSAL_H

// --------------------------------------------------------------------------
// A multiplayer connection for the Link Cable and the Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkUniversal* linkUniversal = new LinkUniversal();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
//       irq_add(II_TIMER2, LINK_UNIVERSAL_ISR_ACK_TIMER); // (*)
//       // optional, for `LinkWireless::asyncACKTimerId` -----^
// - 3) Initialize the library with:
//       linkUniversal->activate();
// - 4) Sync:
//       linkUniversal->sync();
//       // (put this line at the start of your game loop)
// - 5) Send/read messages by using:
//       bool isConnected = linkUniversal->isConnected();
//       u8 playerCount = linkUniversal->playerCount();
//       u8 currentPlayerId = linkUniversal->currentPlayerId();
//       linkUniversal->send(0x1234);
//       if (isConnected && linkUniversal->canRead(!currentPlayerId)) {
//         u16 message = linkUniversal->read(!currentPlayerId);
//         // ...
//       }
// --------------------------------------------------------------------------
// (*1) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//      That causes packet loss. You REALLY want to use libugba's instead.
//      (see examples)
// --------------------------------------------------------------------------
// (*2) For CABLE mode:
//      The hardware is very sensitive to timing. Make sure that
//      `LINK_CABLE_ISR_SERIAL()` is handled on time. That means:
//      Be careful with DMA usage (which stops the CPU), and write short
//      interrupt handlers (or activate nested interrupts by setting
//      `REG_IME=1` at the start of your handlers).
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - 0xFFFF and 0x0 are reserved values, so don't use them!
//   (they mean 'disconnected' and 'no data' respectively)
// --------------------------------------------------------------------------

#include "_link_common.hpp"

#include <cstdio>
#include "LinkCable.hpp"
#include "LinkWireless.hpp"

// Max players. Default = 5 (keep in mind that LinkCable's limit is 4)
#define LINK_UNIVERSAL_MAX_PLAYERS LINK_WIRELESS_MAX_PLAYERS

// Game ID Filter. Default = 0 (no filter)
#define LINK_UNIVERSAL_GAME_ID_FILTER 0

static volatile char LINK_UNIVERSAL_VERSION[] = "LinkUniversal/v7.0.0";

#define LINK_UNIVERSAL_DISCONNECTED LINK_CABLE_DISCONNECTED
#define LINK_UNIVERSAL_NO_DATA LINK_CABLE_NO_DATA

class LinkUniversal {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using s8 = signed char;

  static constexpr int MAX_ROOM_NUMBER = 32000;
  static constexpr int INIT_WAIT_FRAMES = 10;
  static constexpr int SWITCH_WAIT_FRAMES = 25;
  static constexpr int SWITCH_WAIT_FRAMES_RANDOM = 10;
  static constexpr int BROADCAST_SEARCH_WAIT_FRAMES = 10;
  static constexpr int SERVE_WAIT_FRAMES = 60;
  static constexpr int SERVE_WAIT_FRAMES_RANDOM = 30;

 public:
  enum State { INITIALIZING, WAITING, CONNECTED };
  enum Mode { LINK_CABLE, LINK_WIRELESS };
  enum Protocol {
    AUTODETECT,
    CABLE,
    WIRELESS_AUTO,
    WIRELESS_SERVER,
    WIRELESS_CLIENT
  };

  struct CableOptions {
    LinkCable::BaudRate baudRate;
    u32 timeout;
    u32 remoteTimeout;
    u16 interval;
    u8 sendTimerId;
  };

  struct WirelessOptions {
    bool retransmission;
    u32 maxPlayers;
    u32 timeout;
    u32 remoteTimeout;
    u16 interval;
    u8 sendTimerId;
    s8 asyncACKTimerId;
  };

  explicit LinkUniversal(Protocol protocol = AUTODETECT,
                         const char* gameName = "",
                         CableOptions cableOptions =
                             CableOptions{LinkCable::BaudRate::BAUD_RATE_1,
                                          LINK_CABLE_DEFAULT_TIMEOUT,
                                          LINK_CABLE_DEFAULT_REMOTE_TIMEOUT,
                                          LINK_CABLE_DEFAULT_INTERVAL,
                                          LINK_CABLE_DEFAULT_SEND_TIMER_ID},
                         WirelessOptions wirelessOptions =
                             WirelessOptions{
                                 true, LINK_UNIVERSAL_MAX_PLAYERS,
                                 LINK_WIRELESS_DEFAULT_TIMEOUT,
                                 LINK_WIRELESS_DEFAULT_REMOTE_TIMEOUT,
                                 LINK_WIRELESS_DEFAULT_INTERVAL,
                                 LINK_WIRELESS_DEFAULT_SEND_TIMER_ID,
                                 LINK_WIRELESS_DEFAULT_ASYNC_ACK_TIMER_ID},
                         int randomSeed = 123) {
    this->linkCable = new LinkCable(
        cableOptions.baudRate, cableOptions.timeout, cableOptions.remoteTimeout,
        cableOptions.interval, cableOptions.sendTimerId);
    this->linkWireless = new LinkWireless(
        wirelessOptions.retransmission, true,
        Link::_min(wirelessOptions.maxPlayers, LINK_UNIVERSAL_MAX_PLAYERS),
        wirelessOptions.timeout, wirelessOptions.remoteTimeout,
        wirelessOptions.interval, wirelessOptions.sendTimerId,
        wirelessOptions.asyncACKTimerId);

    this->config.protocol = protocol;
    this->config.gameName = gameName;
  }

  [[nodiscard]] bool isActive() { return isEnabled; }

  void activate() {
    reset();
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
    linkCable->deactivate();
    linkWireless->deactivate();
    resetState();
  }

  void setProtocol(Protocol protocol) { this->config.protocol = protocol; }
  [[nodiscard]] Protocol getProtocol() { return this->config.protocol; }

  [[nodiscard]] bool isConnected() { return state == CONNECTED; }

  [[nodiscard]] u8 playerCount() {
    return mode == LINK_CABLE ? linkCable->playerCount()
                              : linkWireless->playerCount();
  }

  [[nodiscard]] u8 currentPlayerId() {
    return mode == LINK_CABLE ? linkCable->currentPlayerId()
                              : linkWireless->currentPlayerId();
  }

  void sync() {
    if (!isEnabled)
      return;

    u16 keys = ~Link::_REG_KEYS & Link::_KEY_ANY;
    randomSeed += keys;
    randomSeed += Link::_REG_RCNT;
    randomSeed += Link::_REG_SIOCNT;

    if (mode == LINK_CABLE)
      linkCable->sync();

    switch (state) {
      case INITIALIZING: {
        waitCount++;
        if (waitCount > INIT_WAIT_FRAMES)
          start();
        break;
      };
      case WAITING: {
        if (mode == LINK_CABLE) {
          // Cable, waiting...
          if (isConnectedCable()) {
            state = CONNECTED;
            goto connected;
          }
        } else {
          // Wireless, waiting...
          if (isConnectedWireless()) {
            state = CONNECTED;
            goto connected;
          } else {
            if (!autoDiscoverWirelessConnections())
              waitCount = switchWait;
            if (isConnectedWireless())
              goto connected;
          }
        }

        waitCount++;
        if (waitCount > switchWait)
          toggleMode();

        break;
      }
      case CONNECTED: {
      connected:
        if (mode == LINK_CABLE) {
          // Cable, connected...
          if (!isConnectedCable()) {
            toggleMode();
            break;
          }

          receiveCableMessages();
        } else {
          // Wireless, connected...
          if (!isConnectedWireless()) {
            toggleMode();
            break;
          }

          receiveWirelessMessages();
        }

        break;
      }
    }
  }

  bool waitFor(u8 playerId) {
    return waitFor(playerId, []() { return false; });
  }

  template <typename F>
  bool waitFor(u8 playerId, F cancel) {
    sync();

    u8 timerId = mode == LINK_CABLE ? linkCable->config.sendTimerId
                                    : linkWireless->config.sendTimerId;

    while (isConnected() && !canRead(playerId) && !cancel()) {
      Link::_IntrWait(1, Link::_IRQ_SERIAL | Link::_TIMER_IRQ_IDS[timerId]);
      sync();
    }

    return isConnected() && canRead(playerId);
  }

  [[nodiscard]] bool canRead(u8 playerId) {
    return !incomingMessages[playerId].isEmpty();
  }

  u16 read(u8 playerId) { return incomingMessages[playerId].pop(); }

  [[nodiscard]] u16 peek(u8 playerId) {
    return incomingMessages[playerId].peek();
  }

  bool send(u16 data) {
    if (data == LINK_CABLE_DISCONNECTED || data == LINK_CABLE_NO_DATA)
      return false;

    if (mode == LINK_CABLE) {
      linkCable->send(data);
      return true;
    } else {
      return linkWireless->send(data);
    }
  }

  [[nodiscard]] State getState() { return state; }
  [[nodiscard]] Mode getMode() { return mode; }
  [[nodiscard]] LinkWireless::State getWirelessState() {
    return linkWireless->getState();
  }

  ~LinkUniversal() {
    delete linkCable;
    delete linkWireless;
  }

  [[nodiscard]] u32 _getWaitCount() { return waitCount; }
  [[nodiscard]] u32 _getSubWaitCount() { return subWaitCount; }

  void _onVBlank() {
    if (mode == LINK_CABLE)
      linkCable->_onVBlank();
    else
      linkWireless->_onVBlank();
  }

  void _onSerial() {
    if (mode == LINK_CABLE)
      linkCable->_onSerial();
    else
      linkWireless->_onSerial();
  }

  void _onTimer() {
    if (mode == LINK_CABLE)
      linkCable->_onTimer();
    else
      linkWireless->_onTimer();
  }

  void _onACKTimer() {
    if (mode == LINK_WIRELESS)
      linkWireless->_onACKTimer();
  }

  LinkCable* linkCable;
  LinkWireless* linkWireless;

 private:
  struct Config {
    Protocol protocol;
    const char* gameName;
  };

  LinkCable::U16Queue incomingMessages[LINK_UNIVERSAL_MAX_PLAYERS];
  Config config;
  State state = INITIALIZING;
  Mode mode = LINK_CABLE;
  u32 waitCount = 0;
  u32 switchWait = 0;
  u32 subWaitCount = 0;
  u32 serveWait = 0;
  int randomSeed = 0;
  volatile bool isEnabled = false;

  void receiveCableMessages() {
    static constexpr u32 MAX_PLAYERS =
        LINK_CABLE_MAX_PLAYERS < LINK_UNIVERSAL_MAX_PLAYERS
            ? LINK_CABLE_MAX_PLAYERS
            : LINK_UNIVERSAL_MAX_PLAYERS;

    for (u32 i = 0; i < MAX_PLAYERS; i++) {
      while (linkCable->canRead(i))
        incomingMessages[i].push(linkCable->read(i));
    }
  }

  void receiveWirelessMessages() {
    LinkWireless::Message messages[LINK_WIRELESS_QUEUE_SIZE];
    linkWireless->receive(messages);

    for (u32 i = 0; i < LINK_WIRELESS_QUEUE_SIZE; i++) {
      auto message = messages[i];
      if (message.packetId == LINK_WIRELESS_END)
        break;

      if (message.playerId < LINK_UNIVERSAL_MAX_PLAYERS)
        incomingMessages[message.playerId].push(message.data);
    }
  }

  bool autoDiscoverWirelessConnections() {
    switch (linkWireless->getState()) {
      case LinkWireless::State::NEEDS_RESET:
      case LinkWireless::State::AUTHENTICATED: {
        subWaitCount = 0;
        linkWireless->getServersAsyncStart();
        break;
      }
      case LinkWireless::State::SEARCHING: {
        waitCount = 0;
        subWaitCount++;

        if (subWaitCount >= BROADCAST_SEARCH_WAIT_FRAMES) {
          if (!tryConnectOrServeWirelessSession())
            return false;
        }
        break;
      }
      case LinkWireless::State::CONNECTING: {
        if (!linkWireless->keepConnecting())
          return false;

        break;
      }
      case LinkWireless::State::SERVING: {
        waitCount = 0;
        subWaitCount++;

        if (subWaitCount > serveWait)
          return false;

        break;
      }
      case LinkWireless::State::CONNECTED: {
        // (should not happen)
        break;
      }
    }

    return true;
  }

  bool tryConnectOrServeWirelessSession() {
    LinkWireless::Server servers[LINK_WIRELESS_MAX_SERVERS];
    if (!linkWireless->getServersAsyncEnd(servers))
      return false;

    u32 maxRandomNumber = 0;
    u32 serverIndex = 0;
    for (u32 i = 0; i < LINK_WIRELESS_MAX_SERVERS; i++) {
      auto server = servers[i];
      if (server.id == LINK_WIRELESS_END)
        break;

      if (!server.isFull() &&
          std::strcmp(server.gameName, config.gameName) == 0 &&
          (LINK_UNIVERSAL_GAME_ID_FILTER == 0 ||
           server.gameId == LINK_UNIVERSAL_GAME_ID_FILTER)) {
        u32 randomNumber = safeStoi(server.userName);
        if (randomNumber > maxRandomNumber && randomNumber < MAX_ROOM_NUMBER) {
          maxRandomNumber = randomNumber;
          serverIndex = i;
        }
      }
    }

    if (maxRandomNumber > 0 && config.protocol != WIRELESS_SERVER) {
      if (!linkWireless->connect(servers[serverIndex].id))
        return false;
    } else {
      if (config.protocol == WIRELESS_CLIENT)
        return false;

      subWaitCount = 0;
      serveWait = SERVE_WAIT_FRAMES + _qran_range(1, SERVE_WAIT_FRAMES_RANDOM);
      u32 randomNumber = _qran_range(1, MAX_ROOM_NUMBER);
      char randomNumberStr[6];
      std::snprintf(randomNumberStr, sizeof(randomNumberStr), "%d",
                    randomNumber);
      if (!linkWireless->serve(config.gameName, randomNumberStr,
                               LINK_UNIVERSAL_GAME_ID_FILTER > 0
                                   ? LINK_UNIVERSAL_GAME_ID_FILTER
                                   : LINK_WIRELESS_MAX_GAME_ID))
        return false;
    }

    return true;
  }

  bool isConnectedCable() { return linkCable->isConnected(); }
  bool isConnectedWireless() { return linkWireless->isConnected(); }

  void reset() {
    switch (config.protocol) {
      case AUTODETECT:
      case CABLE: {
        setMode(LINK_CABLE);
        break;
      }
      case WIRELESS_AUTO:
      case WIRELESS_SERVER:
      case WIRELESS_CLIENT: {
        setMode(LINK_WIRELESS);
        break;
      }
    }
  }

  void stop() {
    if (mode == LINK_CABLE)
      linkCable->deactivate();
    else
      linkWireless->deactivate();
  }

  void toggleMode() {
    switch (config.protocol) {
      case AUTODETECT: {
        setMode(mode == LINK_CABLE ? LINK_WIRELESS : LINK_CABLE);
        break;
      }
      case CABLE: {
        setMode(LINK_CABLE);
        break;
      }
      case WIRELESS_AUTO:
      case WIRELESS_SERVER:
      case WIRELESS_CLIENT: {
        setMode(LINK_WIRELESS);
        break;
      }
    }
  }

  void setMode(Mode mode) {
    stop();
    this->state = INITIALIZING;
    this->mode = mode;
    resetState();
  }

  void start() {
    if (mode == LINK_CABLE)
      linkCable->activate();
    else
      linkWireless->activate();

    state = WAITING;
    resetState();
  }

  void resetState() {
    waitCount = 0;
    switchWait = SWITCH_WAIT_FRAMES + _qran_range(1, SWITCH_WAIT_FRAMES_RANDOM);
    subWaitCount = 0;
    serveWait = 0;
    for (u32 i = 0; i < LINK_UNIVERSAL_MAX_PLAYERS; i++)
      incomingMessages[i].clear();
  }

  u32 safeStoi(const char* str) {
    u32 num = 0;

    while (*str != '\0') {
      char ch = *str;
      if (ch < '0' || ch > '9')
        return 0;
      num = num * 10 + (ch - '0');
      str++;
    }

    return num;
  }

  int _qran(void) {
    randomSeed = 1664525 * randomSeed + 1013904223;
    return (randomSeed >> 16) & 0x7FFF;
  }

  int _qran_range(int min, int max) {
    return (_qran() * (max - min) >> 15) + min;
  }
};

extern LinkUniversal* linkUniversal;

inline void LINK_UNIVERSAL_ISR_VBLANK() {
  linkUniversal->_onVBlank();
}

inline void LINK_UNIVERSAL_ISR_SERIAL() {
  linkUniversal->_onSerial();
}

inline void LINK_UNIVERSAL_ISR_TIMER() {
  linkUniversal->_onTimer();
}

inline void LINK_UNIVERSAL_ISR_ACK_TIMER() {
  linkUniversal->_onACKTimer();
}

#endif  // LINK_UNIVERSAL_H
