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
                           C_LinkMobileDNSQuery* result) {
  return static_cast<LinkMobile*>(handle)->dnsQuery(
      domainName, reinterpret_cast<LinkMobile::DNSQuery*>(result));
}

bool C_LinkMobile_openConnection(C_LinkMobileHandle handle,
                                 const u8* ip,
                                 u16 port,
                                 C_LinkMobileConnectionType type,
                                 C_LinkMobileOpenConn* result) {
  return static_cast<LinkMobile*>(handle)->openConnection(
      ip, port, static_cast<LinkMobile::ConnectionType>(type),
      reinterpret_cast<LinkMobile::OpenConn*>(result));
}

bool C_LinkMobile_closeConnection(C_LinkMobileHandle handle,
                                  u8 connectionId,
                                  C_LinkMobileConnectionType type,
                                  C_LinkMobileCloseConn* result) {
  return static_cast<LinkMobile*>(handle)->closeConnection(
      connectionId, static_cast<LinkMobile::ConnectionType>(type),
      reinterpret_cast<LinkMobile::CloseConn*>(result));
}

bool C_LinkMobile_transfer(C_LinkMobileHandle handle,
                           C_LinkMobileDataTransfer dataToSend,
                           C_LinkMobileDataTransfer* result,
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

C_LinkMobile_State C_LinkMobile_getState(C_LinkMobileHandle handle) {
  return static_cast<C_LinkMobile_State>(
      static_cast<LinkMobile*>(handle)->getState());
}

C_LinkMobile_Role C_LinkMobile_getRole(C_LinkMobileHandle handle) {
  return static_cast<C_LinkMobile_Role>(
      static_cast<LinkMobile*>(handle)->getRole());
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
