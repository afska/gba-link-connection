#include "LinkWireless.hpp"

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
LINK_WIRELESS_CODE_IWRAM void LinkWireless::_onSerial() {
  __onSerial();
}
LINK_WIRELESS_CODE_IWRAM void LinkWireless::_onTimer() {
  __onTimer();
}
LINK_WIRELESS_CODE_IWRAM void LinkWireless::_onACKTimer() {
  __onACKTimer();
}
#endif