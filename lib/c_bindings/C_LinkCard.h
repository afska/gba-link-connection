#ifndef C_LINK_CARD_H
#define C_LINK_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkCardHandle;

#define C_LINK_CARD_SIZE 1998

typedef enum {
  C_LINK_CARD_CONNECTED_DEVICE_E_READER_USA,
  C_LINK_CARD_CONNECTED_DEVICE_E_READER_JAP,
  C_LINK_CARD_CONNECTED_DEVICE_DLC_LOADER,
  C_LINK_CARD_CONNECTED_DEVICE_WRONG_CONNECTION,
  C_LINK_CARD_CONNECTED_DEVICE_UNKNOWN_DEVICE
} C_LinkCard_ConnectedDevice;

typedef enum {
  C_LINK_CARD_SEND_RESULT_SUCCESS,
  C_LINK_CARD_SEND_RESULT_UNALIGNED,
  C_LINK_CARD_SEND_RESULT_INVALID_SIZE,
  C_LINK_CARD_SEND_RESULT_CANCELED,
  C_LINK_CARD_SEND_RESULT_WRONG_DEVICE,
  C_LINK_CARD_SEND_RESULT_FAILURE_DURING_TRANSFER
} C_LinkCard_SendResult;

typedef enum {
  C_LINK_CARD_RECEIVE_RESULT_SUCCESS,
  C_LINK_CARD_RECEIVE_RESULT_CANCELED,
  C_LINK_CARD_RECEIVE_RESULT_WRONG_DEVICE,
  C_LINK_CARD_RECEIVE_RESULT_BAD_CHECKSUM,
  C_LINK_CARD_RECEIVE_RESULT_UNEXPECTED_FAILURE,
  C_LINK_CARD_RECEIVE_RESULT_SCAN_ERROR
} C_LinkCard_ReceiveResult;

C_LinkCardHandle C_LinkCard_createDefault();
void C_LinkCard_destroy(C_LinkCardHandle handle);

C_LinkCard_ConnectedDevice C_LinkCard_getConnectedDevice(
    C_LinkCardHandle handle,
    bool (*cancel)());
C_LinkCard_SendResult C_LinkCard_sendLoader(C_LinkCardHandle handle,
                                            const u8* loader,
                                            u32 loaderSize,
                                            bool (*cancel)());
C_LinkCard_ReceiveResult C_LinkCard_receiveCard(C_LinkCardHandle handle,
                                                u8* card,
                                                bool (*cancel)());

#ifdef __cplusplus
}
#endif

#endif  // C_LINK_CARD_H
