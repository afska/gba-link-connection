#include "../LinkWireless.hpp"

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM

#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL == 1
#define _LINK_SERIAL_ISR \
  LINK_CODE_IWRAM        \
  __attribute__((optimize(LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL_LEVEL)))
#else
#define _LINK_SERIAL_ISR
#endif

#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER == 1
#define _LINK_TIMER_ISR \
  LINK_CODE_IWRAM       \
  __attribute__((optimize(LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER_LEVEL)))
#else
#define _LINK_TIMER_ISR
#endif

_LINK_SERIAL_ISR void LinkWireless::_onSerial() {
  __onSerial();
}

_LINK_TIMER_ISR void LinkWireless::_onTimer() {
  __onTimer();
}

_LINK_SERIAL_ISR void LinkWireless::processMessage(u32 playerId,
                                                   u32 data,
                                                   u32& currentPacketId,
                                                   u32& playerBitMap,
                                                   int& playerBitMapCount) {
  _processMessage(playerId, data, currentPacketId, playerBitMap,
                  playerBitMapCount);
}

#endif
