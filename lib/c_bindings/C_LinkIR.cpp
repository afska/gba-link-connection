#include "C_LinkIR.h"
#include "../LinkIR.hpp"

extern "C" {

C_LinkIRHandle C_LinkIR_createDefault() {
  return new LinkIR();
}

C_LinkIRHandle C_LinkIR_create(u8 primaryTimerId, u8 secondaryTimerId) {
  return new LinkIR(primaryTimerId, secondaryTimerId);
}

void C_LinkIR_destroy(C_LinkIRHandle handle) {
  delete static_cast<LinkIR*>(handle);
}

bool C_LinkIR_activate(C_LinkIRHandle handle) {
  return static_cast<LinkIR*>(handle)->activate();
}

void C_LinkIR_deactivate(C_LinkIRHandle handle) {
  static_cast<LinkIR*>(handle)->deactivate();
}

void C_LinkIR_sendNEC(C_LinkIRHandle handle, u8 address, u8 command) {
  static_cast<LinkIR*>(handle)->sendNEC(address, command);
}

bool C_LinkIR_receiveNEC(C_LinkIRHandle handle,
                         u8* address,
                         u8* command,
                         u32 startTimeout) {
  u8 addr = 0, cmd = 0;
  bool result =
      static_cast<LinkIR*>(handle)->receiveNEC(addr, cmd, startTimeout);
  *address = addr;
  *command = cmd;
  return result;
}

bool C_LinkIR_parseNEC(C_LinkIRHandle handle,
                       u16 pulses[],
                       u8* address,
                       u8* command) {
  u8 addr = 0, cmd = 0;
  bool result = static_cast<LinkIR*>(handle)->parseNEC(pulses, addr, cmd);
  *address = addr;
  *command = cmd;
  return result;
}

void C_LinkIR_send(C_LinkIRHandle handle, u16 pulses[]) {
  static_cast<LinkIR*>(handle)->send(pulses);
}

bool C_LinkIR_receive(C_LinkIRHandle handle,
                      u16 pulses[],
                      u32 maxEntries,
                      u32 startTimeout,
                      u32 signalTimeout) {
  return static_cast<LinkIR*>(handle)->receive(pulses, maxEntries, startTimeout,
                                               signalTimeout);
}

void C_LinkIR_setLight(C_LinkIRHandle handle, bool on) {
  static_cast<LinkIR*>(handle)->setLight(on);
}

bool C_LinkIR_isEmittingLight(C_LinkIRHandle handle) {
  return static_cast<LinkIR*>(handle)->isEmittingLight();
}

bool C_LinkIR_isDetectingLight(C_LinkIRHandle handle) {
  return static_cast<LinkIR*>(handle)->isDetectingLight();
}

C_LinkIR_Config C_LinkIR_getConfig(C_LinkIRHandle handle) {
  C_LinkIR_Config config;
  auto instance = static_cast<LinkIR*>(handle);
  config.primaryTimerId = instance->config.primaryTimerId;
  config.secondaryTimerId = instance->config.secondaryTimerId;
  return config;
}

void C_LinkIR_setConfig(C_LinkIRHandle handle, C_LinkIR_Config config) {
  auto instance = static_cast<LinkIR*>(handle);
  instance->config.primaryTimerId = config.primaryTimerId;
  instance->config.secondaryTimerId = config.secondaryTimerId;
}

void C_LinkIR_onSerial(C_LinkIRHandle handle) {
  static_cast<LinkIR*>(handle)->_onSerial();
}
}
