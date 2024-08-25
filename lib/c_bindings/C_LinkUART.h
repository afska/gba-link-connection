#ifndef C_BINDINGS_LINK_UART_H
#define C_BINDINGS_LINK_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkUARTHandle;

typedef enum {
  C_BAUD_RATE_0,  // 9600 bps
  C_BAUD_RATE_1,  // 38400 bps
  C_BAUD_RATE_2,  // 57600 bps
  C_BAUD_RATE_3   // 115200 bps
} C_LinkUART_BaudRate;

typedef enum { C_SIZE_7_BITS, C_SIZE_8_BITS } C_LinkUART_DataSize;

typedef enum { C_PARITY_NO, C_PARITY_EVEN, C_PARITY_ODD } C_LinkUART_Parity;

C_LinkUARTHandle C_LinkUART_create();
void C_LinkUART_destroy(C_LinkUARTHandle handle);

bool C_LinkUART_isActive(C_LinkUARTHandle handle);
void C_LinkUART_activate(C_LinkUARTHandle handle,
                         C_LinkUART_BaudRate baudRate,
                         C_LinkUART_DataSize dataSize,
                         C_LinkUART_Parity parity,
                         bool useCTS);
void C_LinkUART_deactivate(C_LinkUARTHandle handle);

void C_LinkUART_sendLine(C_LinkUARTHandle handle, const char* string);
void C_LinkUART_sendLineWithCancel(C_LinkUARTHandle handle,
                                   const char* string,
                                   bool (*cancel)());

bool C_LinkUART_readLine(C_LinkUARTHandle handle, char* string, u32 limit);
bool C_LinkUART_readLineWithCancel(C_LinkUARTHandle handle,
                                   char* string,
                                   bool (*cancel)(),
                                   u32 limit);

void C_LinkUART_send(C_LinkUARTHandle handle,
                     const u8* buffer,
                     u32 size,
                     u32 offset);
u32 C_LinkUART_read(C_LinkUARTHandle handle, u8* buffer, u32 size, u32 offset);

bool C_LinkUART_canRead(C_LinkUARTHandle handle);
bool C_LinkUART_canSend(C_LinkUARTHandle handle);
u32 C_LinkUART_availableForRead(C_LinkUARTHandle handle);
u32 C_LinkUART_availableForSend(C_LinkUARTHandle handle);

u8 C_LinkUART_readByte(C_LinkUARTHandle handle);
void C_LinkUART_sendByte(C_LinkUARTHandle handle, u8 data);

void C_LinkUART_onSerial(C_LinkUARTHandle handle);

extern C_LinkUARTHandle cLinkUART;

inline void C_LINK_SPI_ISR_SERIAL() {
  C_LinkUART_onSerial(cLinkUART);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_UART_H
