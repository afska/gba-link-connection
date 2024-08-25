#ifndef C_BINDINGS_LINK_MOBILE_H
#define C_BINDINGS_LINK_MOBILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkMobileHandle;

#define C_LINK_MOBILE_MAX_USER_TRANSFER_LENGTH 254
#define C_LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH 255
#define C_LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH 32
#define C_LINK_MOBILE_MAX_LOGIN_ID_LENGTH 32
#define C_LINK_MOBILE_MAX_PASSWORD_LENGTH 32
#define C_LINK_MOBILE_MAX_DOMAIN_NAME_LENGTH 253
#define C_LINK_MOBILE_COMMAND_TRANSFER_BUFFER \
  (C_LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH + 4)
#define C_LINK_MOBILE_DEFAULT_TIMEOUT (60 * 10)
#define C_LINK_MOBILE_DEFAULT_TIMER_ID 3

typedef enum {
  C_LINK_MOBILE_STATE_NEEDS_RESET,
  C_LINK_MOBILE_STATE_PINGING,
  C_LINK_MOBILE_STATE_WAITING_TO_START,
  C_LINK_MOBILE_STATE_STARTING_SESSION,
  C_LINK_MOBILE_STATE_ACTIVATING_SIO32,
  C_LINK_MOBILE_STATE_WAITING_32BIT_SWITCH,
  C_LINK_MOBILE_STATE_READING_CONFIGURATION,
  C_LINK_MOBILE_STATE_SESSION_ACTIVE,
  C_LINK_MOBILE_STATE_CALL_REQUESTED,
  C_LINK_MOBILE_STATE_CALLING,
  C_LINK_MOBILE_STATE_CALL_ESTABLISHED,
  C_LINK_MOBILE_STATE_ISP_CALL_REQUESTED,
  C_LINK_MOBILE_STATE_ISP_CALLING,
  C_LINK_MOBILE_STATE_PPP_LOGIN,
  C_LINK_MOBILE_STATE_PPP_ACTIVE,
  C_LINK_MOBILE_STATE_SHUTDOWN_REQUESTED,
  C_LINK_MOBILE_STATE_ENDING_SESSION,
  C_LINK_MOBILE_STATE_WAITING_8BIT_SWITCH,
  C_LINK_MOBILE_STATE_SHUTDOWN
} C_LinkMobile_State;

typedef enum {
  C_LINK_MOBILE_ROLE_NO_P2P_CONNECTION,
  C_LINK_MOBILE_ROLE_CALLER,
  C_LINK_MOBILE_ROLE_RECEIVER
} C_LinkMobile_Role;

typedef enum {
  C_LINK_MOBILE_CONNECTION_TYPE_TCP,
  C_LINK_MOBILE_CONNECTION_TYPE_UDP
} C_LinkMobileConnectionType;

typedef struct {
  volatile bool completed;
  bool success;
  u8 data[254];
  u8 size;
} C_LinkMobileDataTransfer;

typedef struct {
  volatile bool completed;
  bool success;
  u8 ipv4[4];
} C_LinkMobileDNSQuery;

typedef struct {
  volatile bool completed;
  bool success;
  u8 connectionId;
} C_LinkMobileOpenConn;

typedef struct {
  volatile bool completed;
  bool success;
} C_LinkMobileCloseConn;

C_LinkMobileHandle C_LinkMobile_create(u32 timeout, u8 timerId);
void C_LinkMobile_destroy(C_LinkMobileHandle handle);

bool C_LinkMobile_isActive(C_LinkMobileHandle handle);
void C_LinkMobile_activate(C_LinkMobileHandle handle);
void C_LinkMobile_deactivate(C_LinkMobileHandle handle);
bool C_LinkMobile_shutdown(C_LinkMobileHandle handle);

bool C_LinkMobile_call(C_LinkMobileHandle handle, const char* phoneNumber);
bool C_LinkMobile_callISP(C_LinkMobileHandle handle,
                          const char* password,
                          const char* loginId);

bool C_LinkMobile_dnsQuery(C_LinkMobileHandle handle,
                           const char* domainName,
                           C_LinkMobileDNSQuery* result);
bool C_LinkMobile_openConnection(C_LinkMobileHandle handle,
                                 const u8* ip,
                                 u16 port,
                                 C_LinkMobileConnectionType type,
                                 C_LinkMobileOpenConn* result);
bool C_LinkMobile_closeConnection(C_LinkMobileHandle handle,
                                  u8 connectionId,
                                  C_LinkMobileConnectionType type,
                                  C_LinkMobileCloseConn* result);

bool C_LinkMobile_transfer(C_LinkMobileHandle handle,
                           C_LinkMobileDataTransfer dataToSend,
                           C_LinkMobileDataTransfer* result,
                           u8 connectionId);
bool C_LinkMobile_waitFor(C_LinkMobileHandle handle, void* asyncRequest);
bool C_LinkMobile_hangUp(C_LinkMobileHandle handle);

C_LinkMobile_State C_LinkMobile_getState(C_LinkMobileHandle handle);
C_LinkMobile_Role C_LinkMobile_getRole(C_LinkMobileHandle handle);

void C_LinkMobile_onVBlank(C_LinkMobileHandle handle);
void C_LinkMobile_onSerial(C_LinkMobileHandle handle);
void C_LinkMobile_onTimer(C_LinkMobileHandle handle);

extern C_LinkMobileHandle cLinkMobile;

inline void C_LINK_MOBILE_ISR_VBLANK() {
  C_LinkMobile_onVBlank(cLinkMobile);
}

inline void C_LINK_MOBILE_ISR_SERIAL() {
  C_LinkMobile_onSerial(cLinkMobile);
}

inline void C_LINK_MOBILE_ISR_TIMER() {
  C_LinkMobile_onTimer(cLinkMobile);
}

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_MOBILE_H
