#ifndef LINK_CONNECTION_H
#define LINK_CONNECTION_H

#include <tonc_core.h>
#include <tonc_memmap.h>

#define LINK_MAX_PLAYERS 4
#define LINK_NO_DATA 0xffff
#define LINK_BIT_SLAVE 2
#define LINK_BIT_READY 3
#define LINK_BITS_PLAYER_ID 4
#define LINK_BIT_ERROR 6
#define LINK_BIT_START 7
#define LINK_BIT_MULTIPLAYER 13
#define LINK_BIT_IRQ 14

// A Link Cable connection for Multi-player mode.
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkConnection* linkConnection = new LinkConnection();
// - 2) Add the interrupt service routine:
//       irq_add(II_SERIAL, ISR_serial);
// - 3) Add to your update loop:
//       auto linkState = linkConnection->tick(data);
// - 4) Use `linkState` to process updates

void ISR_serial();

struct LinkState {
  u8 playerCount = 0;
  u8 currentPlayerId = 0;
  u16 data[4];

  bool isConnected() { return playerCount > 1; }

  bool hasData(u8 playerId) { return data[playerId] != LINK_NO_DATA; }

  void clear() {
    playerCount = 0;
    currentPlayerId = 0;
    for (u32 i = 0; i < LINK_MAX_PLAYERS; i++)
      data[i] = LINK_NO_DATA;
  }
};

class LinkConnection {
 public:
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  struct LinkState _linkState;

  LinkConnection() {
    REG_RCNT = 0;
    REG_SIOCNT = (u8)baudRate;
    this->setBitHigh(LINK_BIT_MULTIPLAYER);
    this->setBitHigh(LINK_BIT_IRQ);
  }

  LinkConnection(BaudRate baudRate) : LinkConnection() {
    this->baudRate = baudRate;
  }

  LinkState tick(u16 data) {
    REG_SIOMLT_SEND = data;

    if (!isBitHigh(LINK_BIT_READY) || isBitHigh(LINK_BIT_ERROR)) {
      _linkState.clear();
      return _linkState;
    }

    if (!isBitHigh(LINK_BIT_SLAVE)) {
      if (isWaiting) {
        if (!isBitHigh(LINK_BIT_START))
          isWaiting = false;
        return _linkState;
      }

      setBitHigh(LINK_BIT_START);
      isWaiting = true;
    }

    return _linkState;
  }

 private:
  BaudRate baudRate = BaudRate::BAUD_RATE_0;
  bool isWaiting = false;

  bool isBitHigh(u8 bit) { return (REG_SIOCNT & (1 << bit)) != 0; }
  void setBitHigh(u8 bit) { REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { REG_SIOCNT &= ~(1 << bit); }
};

extern LinkConnection* linkConnection;

inline void ISR_serial() {
  linkConnection->_linkState.playerCount = 0;
  linkConnection->_linkState.currentPlayerId =
      (REG_SIOCNT & (0b11 << LINK_BITS_PLAYER_ID)) >> LINK_BITS_PLAYER_ID;
  linkConnection->_linkState.data[0] = REG_SIOMULTI0;
  linkConnection->_linkState.data[1] = REG_SIOMULTI1;
  linkConnection->_linkState.data[2] = REG_SIOMULTI2;
  linkConnection->_linkState.data[3] = REG_SIOMULTI3;

  for (u32 i = 0; i < LINK_MAX_PLAYERS; i++)
    if (linkConnection->_linkState.hasData(i))
      linkConnection->_linkState.playerCount++;
}

#endif  // LINK_CONNECTION_H
