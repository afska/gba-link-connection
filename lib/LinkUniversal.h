#ifndef LINK_UNIVERSAL_H
#define LINK_UNIVERSAL_H

// --------------------------------------------------------------------------
//  A multiplayer connection for the Link Cable and the Wireless Adapter.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkUniversal* linkUniversal = new LinkUniversal();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
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
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That can cause packet loss. You might want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// `send(...)` restrictions:
// - 0xFFFF and 0x0 are reserved values, so don't use them!
//   (they mean 'disconnected' and 'no data' respectively)
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <queue>
#include <vector>
#include "LinkCable.h"
#include "LinkWireless.h"

#define LINK_UNIVERSAL_MAX_PLAYERS LINK_CABLE_MAX_PLAYERS
#define LINK_UNIVERSAL_DISCONNECTED LINK_CABLE_DISCONNECTED
#define LINK_UNIVERSAL_NO_DATA LINK_CABLE_NO_DATA
#define LINK_UNIVERSAL_BUFFER_SIZE LINK_WIRELESS_QUEUE_SIZE
#define LINK_UNIVERSAL_MAX_ROOM_NUMBER 32000
#define LINK_UNIVERSAL_INIT_WAIT_FRAMES 10
#define LINK_UNIVERSAL_SWITCH_WAIT_FRAMES 25
#define LINK_UNIVERSAL_SWITCH_WAIT_FRAMES_RANDOM 10
#define LINK_UNIVERSAL_BROADCAST_SEARCH_WAIT_FRAMES 10
#define LINK_UNIVERSAL_SERVE_WAIT_FRAMES 60
#define LINK_UNIVERSAL_SERVE_WAIT_FRAMES_RANDOM 30

static volatile char LINK_UNIVERSAL_VERSION[] = "LinkUniversal/v5.0.0";

void LINK_UNIVERSAL_ISR_VBLANK();
void LINK_UNIVERSAL_ISR_SERIAL();
void LINK_UNIVERSAL_ISR_TIMER();

class LinkUniversal {
 public:
  enum State { INITIALIZING, WAITING, CONNECTED };
  enum Mode { LINK_CABLE, LINK_WIRELESS };
  enum Protocol { AUTODETECT, CABLE, WIRELESS };

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
  };

  explicit LinkUniversal(
      Protocol protocol = AUTODETECT,
      std::string gameName = "",
      CableOptions cableOptions =
          CableOptions{
              LinkCable::BaudRate::BAUD_RATE_1, LINK_CABLE_DEFAULT_TIMEOUT,
              LINK_CABLE_DEFAULT_REMOTE_TIMEOUT, LINK_CABLE_DEFAULT_INTERVAL,
              LINK_CABLE_DEFAULT_SEND_TIMER_ID},
      WirelessOptions wirelessOptions = WirelessOptions{
          true, LINK_WIRELESS_MAX_PLAYERS, LINK_WIRELESS_DEFAULT_TIMEOUT,
          LINK_WIRELESS_DEFAULT_REMOTE_TIMEOUT, LINK_WIRELESS_DEFAULT_INTERVAL,
          LINK_WIRELESS_DEFAULT_SEND_TIMER_ID}) {
    this->linkCable =
        new LinkCable(cableOptions.baudRate, cableOptions.timeout,
                      cableOptions.remoteTimeout, LINK_UNIVERSAL_BUFFER_SIZE,
                      cableOptions.interval, cableOptions.sendTimerId);
    this->linkWireless = new LinkWireless(
        wirelessOptions.retransmission, true, wirelessOptions.maxPlayers,
        wirelessOptions.timeout, wirelessOptions.remoteTimeout,
        wirelessOptions.interval, wirelessOptions.sendTimerId);

    this->config.protocol = protocol;
    this->config.gameName = gameName;
  }

  bool isActive() { return isEnabled; }

  void activate() {
    reset();
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;

    linkCable->deactivate();
    linkWireless->deactivate();
  }

  bool isConnected() { return state == CONNECTED; }

  u8 playerCount() {
    return mode == LINK_CABLE ? linkCable->playerCount()
                              : linkWireless->playerCount();
  }

  u8 currentPlayerId() {
    return mode == LINK_CABLE ? linkCable->currentPlayerId()
                              : linkWireless->currentPlayerId();
  }

  void sync() {
    if (!isEnabled)
      return;

    u16 keys = ~REG_KEYS & KEY_ANY;
    __qran_seed += keys;
    __qran_seed += REG_RCNT;
    __qran_seed += REG_SIOCNT;

    switch (state) {
      case INITIALIZING: {
        waitCount++;
        if (waitCount > LINK_UNIVERSAL_INIT_WAIT_FRAMES)
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

    if (mode == LINK_CABLE)
      linkCable->consume();
  }

  bool canRead(u8 playerId) { return !incomingMessages[playerId].empty(); }

  u16 read(u8 playerId) {
    return LINK_CABLE_QUEUE_POP(incomingMessages[playerId]);
  }

  void send(u16 data) {
    if (data == LINK_CABLE_DISCONNECTED || data == LINK_CABLE_NO_DATA)
      return;

    if (mode == LINK_CABLE)
      linkCable->send(data);
    else
      linkWireless->send(std::vector<u32>{data});
  }

  State getState() { return state; }
  Mode getMode() { return mode; }
  LinkWireless::State getWirelessState() { return linkWireless->getState(); }

  ~LinkUniversal() {
    delete linkCable;
    delete linkWireless;
  }

  u32 _getWaitCount() { return waitCount; }
  u32 _getSubWaitCount() { return subWaitCount; }

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

 private:
  struct Config {
    Protocol protocol;
    std::string gameName;
  };

  std::queue<u16> incomingMessages[LINK_UNIVERSAL_MAX_PLAYERS];
  LinkCable* linkCable;
  LinkWireless* linkWireless;
  Config config;
  State state = INITIALIZING;
  Mode mode = LINK_CABLE;
  u32 waitCount = 0;
  u32 switchWait = 0;
  u32 subWaitCount = 0;
  u32 serveWait = 0;
  bool isEnabled = false;

  void receiveCableMessages() {
    for (u32 i = 0; i < LINK_UNIVERSAL_MAX_PLAYERS; i++) {
      while (linkCable->canRead(i))
        push(incomingMessages[i], linkCable->read(i));
    }
  }

  void receiveWirelessMessages() {
    std::vector<LinkWireless::Message> messages;
    linkWireless->receive(messages);

    for (auto& message : messages)
      push(incomingMessages[message.playerId], (u16)message.data[0]);
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

        if (subWaitCount >= LINK_UNIVERSAL_BROADCAST_SEARCH_WAIT_FRAMES) {
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
    std::vector<LinkWireless::Server> servers;
    if (!linkWireless->getServersAsyncEnd(servers))
      return false;

    u32 maxRandomNumber = 0;
    u32 serverIndex = 0;
    for (u32 i = 0; i < servers.size(); i++) {
      auto server = servers[i];
      u32 randomNumber = std::stoi(server.userName);

      if (server.gameName == config.gameName &&
          randomNumber > maxRandomNumber) {
        maxRandomNumber = randomNumber;
        serverIndex = i;
      }
    }

    if (maxRandomNumber > 0) {
      if (!linkWireless->connect(servers[serverIndex].id))
        return false;
    } else {
      subWaitCount = 0;
      serveWait = LINK_UNIVERSAL_SERVE_WAIT_FRAMES +
                  qran_range(1, LINK_UNIVERSAL_SERVE_WAIT_FRAMES_RANDOM);
      u32 randomNumber = qran_range(1, LINK_UNIVERSAL_MAX_ROOM_NUMBER);
      if (!linkWireless->serve(config.gameName, std::to_string(randomNumber)))
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
      case WIRELESS: {
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
      case WIRELESS: {
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
    switchWait = LINK_UNIVERSAL_SWITCH_WAIT_FRAMES +
                 qran_range(1, LINK_UNIVERSAL_SWITCH_WAIT_FRAMES_RANDOM);
    subWaitCount = 0;
    serveWait = 0;
    for (u32 i = 0; i < LINK_UNIVERSAL_MAX_PLAYERS; i++)
      LINK_CABLE_QUEUE_CLEAR(incomingMessages[i]);
  }

  void push(std::queue<u16>& q, u16 value) {
    if (q.size() >= LINK_UNIVERSAL_BUFFER_SIZE)
      LINK_CABLE_QUEUE_POP(q);

    q.push(value);
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

#endif  // LINK_UNIVERSAL_H
