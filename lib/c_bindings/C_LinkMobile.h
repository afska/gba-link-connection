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

enum C_LinkMobile_Role {
  C_LINK_MOBILE_ROLE_NO_P2P_CONNECTION,
  C_LINK_MOBILE_ROLE_CALLER,
  C_LINK_MOBILE_ROLE_RECEIVER
};

enum C_LinkMobile_ConnectionType {
  C_LINK_MOBILE_CONNECTION_TYPE_TCP,
  C_LINK_MOBILE_CONNECTION_TYPE_UDP
};

typedef enum {
  C_LINK_MOBILE_ERROR_TYPE_NONE,
  C_LINK_MOBILE_ERROR_TYPE_ADAPTER_NOT_CONNECTED,
  C_LINK_MOBILE_ERROR_TYPE_PPP_LOGIN_FAILED,
  C_LINK_MOBILE_ERROR_TYPE_COMMAND_FAILED,
  C_LINK_MOBILE_ERROR_TYPE_WEIRD_RESPONSE,
  C_LINK_MOBILE_ERROR_TYPE_TIMEOUT,
  C_LINK_MOBILE_ERROR_TYPE_WTF
} C_LinkMobile_ErrorType;

typedef enum {
  C_LINK_MOBILE_COMMAND_RESULT_PENDING,
  C_LINK_MOBILE_COMMAND_RESULT_SUCCESS,
  C_LINK_MOBILE_COMMAND_RESULT_INVALID_DEVICE_ID,
  C_LINK_MOBILE_COMMAND_RESULT_INVALID_COMMAND_ACK,
  C_LINK_MOBILE_COMMAND_RESULT_INVALID_MAGIC_BYTES,
  C_LINK_MOBILE_COMMAND_RESULT_WEIRD_DATA_SIZE,
  C_LINK_MOBILE_COMMAND_RESULT_WRONG_CHECKSUM,
  C_LINK_MOBILE_COMMAND_RESULT_ERROR_CODE,
  C_LINK_MOBILE_COMMAND_RESULT_WEIRD_ERROR_CODE
} C_LinkMobile_CommandResult;

typedef struct {
  C_LinkMobile_ErrorType type;
  C_LinkMobile_State state;
  u8 cmdId;
  C_LinkMobile_CommandResult cmdResult;
  u8 cmdErrorCode;
  bool cmdIsSending;
  int reqType;
} C_LinkMobile_Error;

typedef struct {
  volatile bool completed;
  bool success;

  u8 ipv4[4];
} C_LinkMobile_DNSQuery;

typedef struct {
  volatile bool completed;
  bool success;

  u32 connectionId;
} C_LinkMobile_OpenConn;

typedef struct {
  volatile bool completed;
  bool success;
} C_LinkMobile_CloseConn;

typedef struct {
  volatile bool completed;
  bool success;

  u8 data[C_LINK_MOBILE_MAX_USER_TRANSFER_LENGTH];
  u8 size;
} C_LinkMobile_DataTransfer;

typedef struct {
  char magic[2];
  u8 registrationState;
  u8 _unused1_;
  u8 primaryDNS[4];
  u8 secondaryDNS[4];
  char loginId[10];
  u8 _unused2_[22];
  char email[24];
  u8 _unused3_[6];
  char smtpServer[20];
  char popServer[19];
  u8 _unused4_[5];
  u8 configurationSlot1[24];
  u8 configurationSlot2[24];
  u8 configurationSlot3[24];
  u8 checksumHigh;
  u8 checksumLow;

  char _ispNumber1[16 + 1];
} C_LinkMobile_ConfigurationData;

typedef enum {
  C_LINK_MOBILE_SIZE_32BIT,
  C_LINK_MOBILE_SIZE_8BIT
} C_LinkMobile_DataSize;

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
                           C_LinkMobile_DNSQuery* result);
bool C_LinkMobile_openConnection(C_LinkMobileHandle handle,
                                 const u8* ip,
                                 u16 port,
                                 C_LinkMobile_ConnectionType connectionType,
                                 C_LinkMobile_OpenConn* result);
bool C_LinkMobile_closeConnection(C_LinkMobileHandle handle,
                                  u8 connectionId,
                                  C_LinkMobile_ConnectionType connectionType,
                                  C_LinkMobile_CloseConn* result);
bool C_LinkMobile_transfer(C_LinkMobileHandle handle,
                           C_LinkMobile_DataTransfer dataToSend,
                           C_LinkMobile_DataTransfer* result,
                           u8 connectionId);

bool C_LinkMobile_waitFor(C_LinkMobileHandle handle, void* asyncRequest);
bool C_LinkMobile_hangUp(C_LinkMobileHandle handle);

bool C_LinkMobile_readConfiguration(
    C_LinkMobileHandle handle,
    C_LinkMobile_ConfigurationData* configurationData);

C_LinkMobile_State C_LinkMobile_getState(C_LinkMobileHandle handle);
C_LinkMobile_Role C_LinkMobile_getRole(C_LinkMobileHandle handle);
int C_LinkMobile_isConfigurationValid(C_LinkMobileHandle handle);
bool C_LinkMobile_isConnectedP2P(C_LinkMobileHandle handle);
bool C_LinkMobile_isConnectedPPP(C_LinkMobileHandle handle);
bool C_LinkMobile_isSessionActive(C_LinkMobileHandle handle);
bool C_LinkMobile_canShutdown(C_LinkMobileHandle handle);
C_LinkMobile_DataSize C_LinkMobile_getDataSize(C_LinkMobileHandle handle);
C_LinkMobile_Error C_LinkMobile_getError(C_LinkMobileHandle handle);

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
