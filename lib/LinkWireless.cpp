#include "LinkWireless.hpp"

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
LINK_WIRELESS_CODE_IWRAM void LINK_WIRELESS_ISR_SERIAL() {
  linkWireless->_onSerial();
}
LINK_WIRELESS_CODE_IWRAM void LINK_WIRELESS_ISR_TIMER() {
  linkWireless->_onTimer();
}
LINK_WIRELESS_CODE_IWRAM void LINK_WIRELESS_ISR_ACK_TIMER() {
  linkWireless->_onACKTimer();
}
#endif