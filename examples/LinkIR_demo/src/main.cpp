// (0) Include the header
#include "../../../lib/LinkIR.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

void sendNECSignal();
void receiveNECSignal();
void sendGeneric38kHzSignal();
void receiveGeneric38kHzSignal();
void monitor();

// (1) Create a LinkIR instance
LinkIR* linkIR = new LinkIR();

bool isConnected = false;

void init() {
  Common::initTTE();

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, LINK_IR_ISR_SERIAL);
  interrupt_add(INTR_TIMER2, []() {});
  interrupt_add(INTR_TIMER3, []() {});

  // (3) Initialize the library
  isConnected = linkIR->activate();
}

int main() {
  init();

  bool a = true, b = true, left = true, right = true, select = true;

  while (true) {
    std::string output = "LinkIR_demo (v8.0.0)\n\n";

    output += std::string("IR adapter: ") +
              (isConnected ? "DETECTED" : "not detected");

    output += "\n\nA = Send NEC signal";
    output += "\nB = Receive NEC signal";
    output += "\n\nRIGHT = Send 38kHz signal";
    output += "\nLEFT = Receive 38kHz / copy";
    output += "\n\nSELECT = monitor";

    if (Common::didPress(KEY_A, a))
      sendNECSignal();
    if (Common::didPress(KEY_B, b))
      receiveNECSignal();
    if (Common::didPress(KEY_RIGHT, right))
      sendGeneric38kHzSignal();
    if (Common::didPress(KEY_LEFT, left))
      receiveGeneric38kHzSignal();
    if (Common::didPress(KEY_SELECT, select))
      monitor();

    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}

void sendNECSignal() {
  // (4) Send NEC signals
  Common::log("Sending...");

  // Addr=0x04, Cmd=0x08
  linkIR->sendNEC(0x04, 0x08);

  Common::log("Sent!\n\nPress DOWN");
  Common::waitForKey(KEY_DOWN);
}

void receiveNECSignal() {
  // (5) Receive NEC signals
  Common::log("Receiving...");

  u8 address, command;
  if (linkIR->receiveNEC(address, command, 1000000)) {
    Common::log("NEC signal detected!\n\nAddress: " + std::to_string(address) +
                "\nCommand: " + std::to_string(command) + "\n\nPress DOWN");
  } else {
    Common::log("No NEC signal detected!\n\nPress DOWN");
  }

  Common::waitForKey(KEY_DOWN);
}

void sendGeneric38kHzSignal() {
  // (6) Send 38kHz signals
  Common::log("Sending...");

  // Example with NEC signal Addr=0x04, Cmd=0x03
  linkIR->send((u16[]){
      // leader
      9000, 4500,
      // address 0x04, LSB first (bits: 0,0,1,0,0,0,0,0)
      560, 560,   // bit0: 0
      560, 560,   // bit1: 0
      560, 1690,  // bit2: 1
      560, 560,   // bit3: 0
      560, 560,   // bit4: 0
      560, 560,   // bit5: 0
      560, 560,   // bit6: 0
      560, 560,   // bit7: 0
      // inverted address 0xFB, LSB first (bits: 1,1,0,1,1,1,1,1)
      560, 1690,  // bit0: 1
      560, 1690,  // bit1: 1
      560, 560,   // bit2: 0
      560, 1690,  // bit3: 1
      560, 1690,  // bit4: 1
      560, 1690,  // bit5: 1
      560, 1690,  // bit6: 1
      560, 1690,  // bit7: 1
      // command 0x03, LSB first (bits: 1,1,0,0,0,0,0,0)
      560, 1690,  // bit0: 1
      560, 1690,  // bit1: 1
      560, 560,   // bit2: 0
      560, 560,   // bit3: 0
      560, 560,   // bit4: 0
      560, 560,   // bit5: 0
      560, 560,   // bit6: 0
      560, 560,   // bit7: 0
      // inverted command 0xFC, LSB first (bits: 0,0,1,1,1,1,1,1)
      560, 560,   // bit0: 0
      560, 560,   // bit1: 0
      560, 1690,  // bit2: 1
      560, 1690,  // bit3: 1
      560, 1690,  // bit4: 1
      560, 1690,  // bit5: 1
      560, 1690,  // bit6: 1
      560, 1690,  // bit7: 1
                  // final burst
      560,
      // terminator
      0});

  Common::log("Sent!\n\nPress DOWN");
  Common::waitForKey(KEY_DOWN);
}

void receiveGeneric38kHzSignal() {
  // (7) Receive 38kHz signals
  Common::log("Receiving...");

  u16 pulses[3000] = {};
  bool didReceive = false;
  if (linkIR->receive(pulses, 3000, 1000000)) {
    didReceive = true;
    std::string received;
    u32 i = 0;
    for (i = 0; pulses[i] != 0; i++) {
      if (i > 0)
        received += ", ";
      received += std::to_string(pulses[i]);
    }
    received =
        "Press START to retransmit\n" + std::to_string(i) + " // " + received;
    Common::log(received);
  } else {
    Common::log("No signal detected!\n\nPress START");
  }

  Common::waitForKey(KEY_START);

  if (didReceive) {
    linkIR->send(pulses);
    Common::log("Sent!\n\nPress DOWN");
    Common::waitForKey(KEY_DOWN);
  }
}

void monitor() {
  const u32 WIDTH = 29;
  const u32 SPEED = 3;
  const u8 ADDR = 0x04;
  const u8 CMD_LEFT = 0x07;
  const u8 CMD_RIGHT = 0x06;

  int x = 0;
  int direction = 1;
  std::string output = "";
  u32 count = 0;
  bool b = true;

  while (true) {
    if (Common::didPress(KEY_B, b))
      return;

    u8 address = 0, command = 0;
    if (linkIR->receiveNEC(address, command, 10000) && address == ADDR) {
      if (command == CMD_LEFT) {
        count = 0;
        direction = -1;
      }
      if (command == CMD_RIGHT) {
        count = 0;
        direction = 1;
      }
    }

    output = "";

    count++;
    if (count > SPEED) {
      x += direction;
      if (x > (int)WIDTH)
        x = WIDTH;
      if (x < 0)
        x = 0;
      count = 0;
    }

    for (int i = 0; i < x; i++)
      output += " ";
    output += "x";

    VBlankIntrWait();
    Common::log(output);
  }
}
