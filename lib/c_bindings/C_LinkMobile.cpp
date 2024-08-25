#include "C_LinkMobile.h"
#include "../LinkMobile.hpp"

extern "C" {

C_LinkMobileHandle C_LinkMobile_create(u32 timeout, u8 timerId) {
  return new LinkMobile(timeout, timerId);
}

void C_LinkMobile_destroy(C_LinkMobileHandle handle) {
  delete static_cast<LinkMobile*>(handle);
}

bool C_LinkMobile_isActive(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->isActive();
}

void C_LinkMobile_activate(C_LinkMobileHandle handle) {
  static_cast<LinkMobile*>(handle)->activate();
}

void C_LinkMobile_deactivate(C_LinkMobileHandle handle) {
  static_cast<LinkMobile*>(handle)->deactivate();
}

bool C_LinkMobile_shutdown(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->shutdown();
}

bool C_LinkMobile_call(C_LinkMobileHandle handle, const char* phoneNumber) {
  return static_cast<LinkMobile*>(handle)->call(phoneNumber);
}

bool C_LinkMobile_callISP(C_LinkMobileHandle handle,
                          const char* password,
                          const char* loginId) {
  return static_cast<LinkMobile*>(handle)->callISP(password, loginId);
}

bool C_LinkMobile_dnsQuery(C_LinkMobileHandle handle,
                           const char* domainName,
                           C_LinkMobile_DNSQuery* result) {
  return static_cast<LinkMobile*>(handle)->dnsQuery(
      domainName, reinterpret_cast<LinkMobile::DNSQuery*>(result));
}

bool C_LinkMobile_openConnection(C_LinkMobileHandle handle,
                                 const u8* ip,
                                 u16 port,
                                 C_LinkMobile_ConnectionType connectionType,
                                 C_LinkMobile_OpenConn* result) {
  return static_cast<LinkMobile*>(handle)->openConnection(
      ip, port, static_cast<LinkMobile::ConnectionType>(connectionType),
      reinterpret_cast<LinkMobile::OpenConn*>(result));
}

bool C_LinkMobile_closeConnection(C_LinkMobileHandle handle,
                                  u8 connectionId,
                                  C_LinkMobile_ConnectionType connectionType,
                                  C_LinkMobile_CloseConn* result) {
  return static_cast<LinkMobile*>(handle)->closeConnection(
      connectionId, static_cast<LinkMobile::ConnectionType>(connectionType),
      reinterpret_cast<LinkMobile::CloseConn*>(result));
}

bool C_LinkMobile_transfer(C_LinkMobileHandle handle,
                           C_LinkMobile_DataTransfer dataToSend,
                           C_LinkMobile_DataTransfer* result,
                           u8 connectionId) {
  return static_cast<LinkMobile*>(handle)->transfer(
      *reinterpret_cast<LinkMobile::DataTransfer*>(&dataToSend),
      reinterpret_cast<LinkMobile::DataTransfer*>(result), connectionId);
}

bool C_LinkMobile_waitFor(C_LinkMobileHandle handle, void* asyncRequest) {
  return static_cast<LinkMobile*>(handle)->waitFor(
      static_cast<LinkMobile::AsyncRequest*>(asyncRequest));
}

bool C_LinkMobile_hangUp(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->hangUp();
}

bool C_LinkMobile_readConfiguration(
    C_LinkMobileHandle handle,
    C_LinkMobile_ConfigurationData* configurationData) {
  return static_cast<LinkMobile*>(handle)->readConfiguration(
      *reinterpret_cast<LinkMobile::ConfigurationData*>(configurationData));
}

C_LinkMobile_State C_LinkMobile_getState(C_LinkMobileHandle handle) {
  return static_cast<C_LinkMobile_State>(
      static_cast<LinkMobile*>(handle)->getState());
}

C_LinkMobile_Role C_LinkMobile_getRole(C_LinkMobileHandle handle) {
  return static_cast<C_LinkMobile_Role>(
      static_cast<LinkMobile*>(handle)->getRole());
}

int C_LinkMobile_isConfigurationValid(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->isConfigurationValid();
}

bool C_LinkMobile_isConnectedP2P(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->isConnectedP2P();
}

bool C_LinkMobile_isConnectedPPP(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->isConnectedPPP();
}

bool C_LinkMobile_isSessionActive(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->isSessionActive();
}

bool C_LinkMobile_canShutdown(C_LinkMobileHandle handle) {
  return static_cast<LinkMobile*>(handle)->canShutdown();
}

C_LinkMobile_DataSize C_LinkMobile_getDataSize(C_LinkMobileHandle handle) {
  return static_cast<C_LinkMobile_DataSize>(
      static_cast<LinkMobile*>(handle)->getDataSize());
}

C_LinkMobile_Error C_LinkMobile_getError(C_LinkMobileHandle handle) {
  LinkMobile::Error error = static_cast<LinkMobile*>(handle)->getError();
  return {static_cast<C_LinkMobile_ErrorType>(error.type),
          static_cast<C_LinkMobile_State>(error.state),
          error.cmdIsSending,
          error.cmdId,
          static_cast<C_LinkMobile_CommandResult>(error.cmdResult),
          error.cmdErrorCode,
          error.reqType};
}

void C_LinkMobile_onVBlank(C_LinkMobileHandle handle) {
  static_cast<LinkMobile*>(handle)->_onVBlank();
}

void C_LinkMobile_onSerial(C_LinkMobileHandle handle) {
  static_cast<LinkMobile*>(handle)->_onSerial();
}

void C_LinkMobile_onTimer(C_LinkMobileHandle handle) {
  static_cast<LinkMobile*>(handle)->_onTimer();
}
}
