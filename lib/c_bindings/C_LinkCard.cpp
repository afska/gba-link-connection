#include "C_LinkCard.h"
#include "../LinkCard.hpp"

extern "C" {
C_LinkCardHandle C_LinkCard_createDefault() {
  return new LinkCard();
}

void C_LinkCard_destroy(C_LinkCardHandle handle) {
  delete static_cast<LinkCard*>(handle);
}

C_LinkCard_ConnectedDevice C_LinkCard_getConnectedDevice(
    C_LinkCardHandle handle,
    bool (*cancel)()) {
  auto device = static_cast<LinkCard*>(handle)->getConnectedDevice(cancel);
  return static_cast<C_LinkCard_ConnectedDevice>(device);
}

C_LinkCard_SendResult C_LinkCard_sendLoader(C_LinkCardHandle handle,
                                            const u8* loader,
                                            u32 loaderSize,
                                            bool (*cancel)()) {
  auto result =
      static_cast<LinkCard*>(handle)->sendLoader(loader, loaderSize, cancel);
  return static_cast<C_LinkCard_SendResult>(result);
}

C_LinkCard_ReceiveResult C_LinkCard_receiveCard(C_LinkCardHandle handle,
                                                u8* card,
                                                bool (*cancel)()) {
  auto result = static_cast<LinkCard*>(handle)->receiveCard(card, cancel);
  return static_cast<C_LinkCard_ReceiveResult>(result);
}
}
