#ifndef C_BINDINGS_LINK_SPI_H
#define C_BINDINGS_LINK_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkSPIHandle;

#define C_LINK_SPI_NO_DATA_32 0xffffffff
#define C_LINK_SPI_NO_DATA_8 0xff
#define C_LINK_SPI_NO_DATA LINK_SPI_NO_DATA_32

typedef enum {
  C_MODE_SLAVE,
  C_MODE_MASTER_256KBPS,
  C_MODE_MASTER_2MBPS
} C_LinkSPI_Mode;

typedef enum { C_SIZE_32BIT, C_SIZE_8BIT } C_LinkSPI_DataSize;

typedef enum {
  C_ASYNC_STATE_IDLE,
  C_ASYNC_STATE_WAITING,
  C_ASYNC_STATE_READY
} C_LinkSPI_AsyncState;

C_LinkSPIHandle C_LinkSPI_create();
void C_LinkSPI_destroy(C_LinkSPIHandle handle);

bool C_LinkSPI_isActive(C_LinkSPIHandle handle);
void C_LinkSPI_activate(C_LinkSPIHandle handle,
                        C_LinkSPI_Mode mode,
                        C_LinkSPI_DataSize dataSize);
void C_LinkSPI_deactivate(C_LinkSPIHandle handle);

u32 C_LinkSPI_transfer(C_LinkSPIHandle handle, u32 data);
u32 C_LinkSPI_transferWithCancel(C_LinkSPIHandle handle,
                                 u32 data,
                                 bool (*cancel)());

void C_LinkSPI_transferAsync(C_LinkSPIHandle handle, u32 data);
void C_LinkSPI_transferAsyncWithCancel(C_LinkSPIHandle handle,
                                       u32 data,
                                       bool (*cancel)());

C_LinkSPI_AsyncState C_LinkSPI_getAsyncState(C_LinkSPIHandle handle);
u32 C_LinkSPI_getAsyncData(C_LinkSPIHandle handle);

C_LinkSPI_Mode C_LinkSPI_getMode(C_LinkSPIHandle handle);
C_LinkSPI_DataSize C_LinkSPI_getDataSize(C_LinkSPIHandle handle);

void C_LinkSPI_setWaitModeActive(C_LinkSPIHandle handle, bool isActive);
bool C_LinkSPI_isWaitModeActive(C_LinkSPIHandle handle);

void C_LinkSPI_onSerial(C_LinkSPIHandle handle, bool customAck);

extern C_LinkSPIHandle cLinkSPI;

inline void C_LINK_SPI_ISR_SERIAL() {
  C_LinkSPI_onSerial(cLinkSPI, false);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_SPI_H
