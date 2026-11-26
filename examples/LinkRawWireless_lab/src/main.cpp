// LinkRawWireless_lab
//
// A simple program to generate synthetic traffic, mostly for investigative
// purposes. It serves as a playground where we can create certain situations
// (ie. one side doesn't respond or does slowly) and see how the RFU responds.
// Useful situations allow us to understand the frame structure (ie. header)
// as well as some behaviours (ie. retransmits, keep-alive, etc).
//
// Every packet has some key data as well as some "dummy" data:
//   bytes 0..3  a sequence number (little-endian), +1 for every SEND
//   byte n      (sequence * 7 + n) & 0xFF, for n >= 4
//
// Using this structure we can infer which data was sent and how, what
// happens with duplicated or missing packets, etc.

#include "../../../lib/LinkRawWireless.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

#define GAME_NAME "LinkRawLab"
#define CHECK_EVERY_FRAMES 30  // how often we ask who is (still) connected
#define LEN(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

LinkRawWireless* linkRawWireless = nullptr;
volatile u32 vblankCount = 0;

// menu options go here

#define NEVER 0
#define ON_NEW_DATA 0xFFFF  // send right after new data arrives
#define NONSTOP 0           // receive in a busy loop
#define SEND 0x24
#define SEND_AND_WAIT 0x25

void strRole(char* out, u32 value) {
  std::strcpy(out, value == 0 ? "host" : "client");
}

void strU(char* out, u32 value) {
  std::sprintf(out, "%lu", (unsigned long)value);
}

void strHex02(char* out, u32 value) {
  std::sprintf(out, "0x%02lx", (unsigned long)value);
}

void strBytecnt(char* out, u32 value) {
  std::sprintf(out, "%lu bytes", (unsigned long)value);
}

void strFramecnt(char* out, u32 value) {
  if (value == 0)
    std::strcpy(out, "none");
  else
    std::sprintf(out, "%lu frames", (unsigned long)value);
}

void strSendInterval(char* out, u32 value) {
  if (value == NEVER)
    std::strcpy(out, "never");
  else if (value == ON_NEW_DATA)
    std::strcpy(out, "on new data");
  else
    std::sprintf(out, "every %lu fr", (unsigned long)value);
}

void strReceiveInterval(char* out, u32 value) {
  if (value == NONSTOP)
    std::strcpy(out, "nonstop");
  else
    std::sprintf(out, "every %lu fr", (unsigned long)value);
}

void strCommand(char* out, u32 value) {
  std::strcpy(out, value == SEND ? "0x24 send" : "0x25 send+wait");
}

// default value goes in the first position of the array
static const u32 ROLES[] = {0, 1};
static const u32 MAX_CLIENTS[] = {4, 3, 2, 1};
static const u32 MAX_TRANSMISSIONS[] = {4, 0, 1, 2, 3, 8, 128, 254, 255};
static const u32 WAIT_TIMEOUTS[] = {32, 0, 1, 4, 8, 16, 60};
// Magic bits 16..23: 0x3c is what most games sends.
// Lowest 2 bits indicate the max number of clients.
static const u32 MAGIC_BYTE2[] = {0x3c, 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14,
                                  0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30,
                                  0x34, 0x38, 0x40, 0x7c, 0xbc, 0xfc};
// Magic bits 24..31: 0x00 in most games.
static const u32 MAGIC_BYTE3[] = {0x00, 0x01, 0x02, 0x03,
                                  0x04, 0x08, 0xf0, 0xff};
static const u32 HOST_SIZES[] = {87, 0,  1,  4,  8,  16, 20, 21,
                                 24, 40, 52, 55, 56, 60, 72, 84};
static const u32 CLIENT_SIZES[] = {16, 0, 1, 2, 4, 8, 12, 14};
static const u32 SEND_INTERVALS[] = {1, 2,  3,  4,     6,
                                     8, 16, 60, NEVER, ON_NEW_DATA};
static const u32 SEND_COMMANDS[] = {SEND, SEND_AND_WAIT};
static const u32 RECEIVE_INTERVALS[] = {1, 2, 4, 8, 16, NONSTOP};

struct Setting {
  const char* name;
  const u32* values;
  u32 count;
  void (*describe)(char* out, u32 value);
  u32 selected;
};

enum {
  ROLE,
  MAX_CLIENT_COUNT,
  MAX_TX,
  WAIT_TIMEOUT,
  MAGIC_2,
  MAGIC_3,
  HOST_SIZE,
  CLIENT_SIZE,
  SEND_INTERVAL,
  SEND_COMMAND,
  RECEIVE_INTERVAL,
  SETTING_COUNT
};

// (same order as the enum above)
static Setting settings[SETTING_COUNT] = {
    {"Role", ROLES, LEN(ROLES), strRole},
    {"Max clients", MAX_CLIENTS, LEN(MAX_CLIENTS), strU},
    {"Max tx", MAX_TRANSMISSIONS, LEN(MAX_TRANSMISSIONS), strU},
    {"Wait timeout", WAIT_TIMEOUTS, LEN(WAIT_TIMEOUTS), strFramecnt},
    {"Magic byte 2", MAGIC_BYTE2, LEN(MAGIC_BYTE2), strHex02},
    {"Magic byte 3", MAGIC_BYTE3, LEN(MAGIC_BYTE3), strHex02},
    {"Host packet", HOST_SIZES, LEN(HOST_SIZES), strBytecnt},
    {"Client packet", CLIENT_SIZES, LEN(CLIENT_SIZES), strBytecnt},
    {"Send", SEND_INTERVALS, LEN(SEND_INTERVALS), strSendInterval},
    {"Send with", SEND_COMMANDS, LEN(SEND_COMMANDS), strCommand},
    {"Receive", RECEIVE_INTERVALS, LEN(RECEIVE_INTERVALS), strReceiveInterval},
};

// The current value of a setting
#define SETTING(INDEX) (settings[INDEX].values[settings[INDEX].selected])

#define isHost() (SETTING(ROLE) == 0)

u32 packetSize() {
  return isHost() ? SETTING(HOST_SIZE) : SETTING(CLIENT_SIZE);
}

static char screen[1024];
static u32 screenLength = 0;

// Starts a new screen
void clearScreen() {
  screen[0] = '\0';
  screenLength = 0;
}

// Adds text to the screen, printf style
void print(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  screenLength += std::vsnprintf(screen + screenLength,
                                 sizeof(screen) - screenLength, format, args);
  va_end(args);
}

void showScreen() {
  Common::log(screen);
}

// Keys that went down since the last call
u16 previousKeys = 0;
u16 pressedKeys() {
  u16 keys = ~REG_KEYS & KEY_ANY;
  u16 pressed = keys & ~previousKeys;
  previousKeys = keys;
  return pressed;
}

void showError(const char* message) {
  print("\n\n%s [state %d]\n\n(DOWN = ok)", message,
        (int)linkRawWireless->getState());
  showScreen();
  while (!(pressedKeys() & KEY_DOWN))
    VBlankIntrWait();
  linkRawWireless->activate();
}

// What we know about the packets coming from one peer.
struct PeerStats {
  bool seenAny;
  u32 lastSequence;
  u32 lastSize;
  u32 newPackets;  // a sequence number we had not seen before
  u32 repeats;     // the same sequence number again
  u32 skipped;     // sequence numbers that never arrived
  u32 damaged;     // content doesn't match the pattern
  u32 tooShort;    // under 4 bytes: no sequence number to check
};

struct {
  u16 deviceId;
  u16 hostId;  // (client only)
  u8 slot;     // (client only)
  const char* status;

  u32 frame;
  u32 sequence;
  u32 sent;
  u32 sendFailures;
  u32 receives;  // ReceiveData calls that returned something

  // SendDataAndWait: which event ended the wait, and how long it took
  u32 dataEvents;        // 0x28
  u32 disconnectEvents;  // 0x29
  u32 timeoutEvents;     // 0x27
  u32 lastWaitMicros;

  // (host) who is connected, and what each slot sent us
  LinkRawWireless::PollConnectionsResponse clients;
  PeerStats fromClient[4];
  u32 selectedSlot;
  bool isServerClosed;

  // (client) what the host sent us
  PeerStats fromHost;
} session;

void fillPacket(Link::u32* data, u32 size, u32 sequence) {
  u8* bytes = (u8*)data;
  for (u32 j = 0; j < size; j++)
    bytes[j] = (u8)(sequence * 7 + j);
  if (size >= 4)
    data[0] = sequence;
}

// Checks one received packet. Returns true if it was new.
bool checkPacket(const u8* bytes, u32 size, PeerStats* stats) {
  stats->lastSize = size;
  if (size < 4) {
    stats->tooShort++;
    return true;
  }

  u32 sequence =
      bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
  for (u32 j = 4; j < size; j++) {
    if (bytes[j] != (u8)(sequence * 7 + j)) {
      stats->damaged++;
      break;
    }
  }

  bool isNew = !stats->seenAny || sequence != stats->lastSequence;
  if (isNew) {
    stats->newPackets++;
    if (stats->seenAny && sequence > stats->lastSequence)
      stats->skipped += sequence - stats->lastSequence - 1;
  } else {
    stats->repeats++;
  }

  stats->seenAny = true;
  stats->lastSequence = sequence;
  return isNew;
}

void sendPacket() {
  Link::u32 data[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
  u32 size = packetSize();
  u32 words = (size + 3) / 4;
  fillPacket(data, size, session.sequence);

  bool success;
  if (SETTING(SEND_COMMAND) == SEND) {
    success = linkRawWireless->sendData(data, words, size);
  } else {
    // Send, then let the adapter hold the clock until something happens:
    // new data (0x28), a disconnection (0x29) or a timeout (0x27)
    LinkRawWireless::CommandResult event;
    Common::profileStart();
    success = linkRawWireless->sendDataAndWait(data, words, event, size);
    session.lastWaitMicros = Common::profileStop() / 17;  // ~16.78 MHz

    if (success) {
      if (event.commandId == 0x28)
        session.dataEvents++;
      else if (event.commandId == 0x29)
        session.disconnectEvents++;
      else if (event.commandId == 0x27)
        session.timeoutEvents++;
    }
  }

  if (success) {
    session.sent++;
    session.sequence++;
  } else {
    session.sendFailures++;
  }
}

void receivePackets() {
  LinkRawWireless::ReceiveDataResponse response;
  if (!linkRawWireless->receiveData(response) || response.dataSize == 0)
    return;
  session.receives++;

  const u8* bytes = (const u8*)response.data;
  bool gotSomethingNew = false;

  if (isHost()) {
    // The data of all clients comes back to back, in slot order.
    // sentBytes[1..4] says how much belongs to slots 0..3.
    u32 offset = 0;
    for (u32 slot = 0; slot < 4; slot++) {
      u32 size = response.sentBytes[1 + slot];
      if (size == 0)
        continue;
      if (checkPacket(bytes + offset, size, &session.fromClient[slot]))
        gotSomethingNew = true;
      offset += size;
    }
  } else {
    u32 size = response.sentBytes[0];
    if (size > 0)
      gotSomethingNew = checkPacket(bytes, size, &session.fromHost);
  }

  if (gotSomethingNew && SETTING(SEND_INTERVAL) == ON_NEW_DATA)
    sendPacket();
}

bool setUpAdapter() {
  u32 maxPlayers = 1 + SETTING(MAX_CLIENT_COUNT);
  u32 magic = (SETTING(MAGIC_3) << 24) | (SETTING(MAGIC_2) << 16);

  return linkRawWireless->activate() &&
         linkRawWireless->setup(maxPlayers, SETTING(MAX_TX),
                                SETTING(WAIT_TIMEOUT), magic);
}

bool readDeviceId() {
  LinkRawWireless::SystemStatusResponse status;
  if (!linkRawWireless->getSystemStatus(status))
    return false;

  session.deviceId = status.deviceId;
  if (status.currentPlayerId > 0)
    session.slot = status.currentPlayerId - 1;
  return true;
}

void printStats(const char* from, const PeerStats* stats) {
  print("%-7s%5lu%4lu%4lu%4lu%4lu\n", from, (unsigned long)stats->newPackets,
        (unsigned long)stats->repeats, (unsigned long)stats->skipped,
        (unsigned long)stats->damaged, (unsigned long)stats->lastSize);
}

void printSession() {
  char text[32];
  clearScreen();

  if (isHost())
    print("HOST %04x\n\n", session.deviceId);
  else
    print("CLIENT %04x slot %u of %04x\n\n", session.deviceId, session.slot,
          session.hostId);

  strSendInterval(text, SETTING(SEND_INTERVAL));
  print("send %lu bytes, %s\n", (unsigned long)packetSize(), text);
  print("sent %lu, failed %lu\n", (unsigned long)session.sent,
        (unsigned long)session.sendFailures);
  if (SETTING(SEND_COMMAND) == SEND_AND_WAIT) {
    print("wait: data %lu disc %lu tmo %lu\n", (unsigned long)session.dataEvents,
          (unsigned long)session.disconnectEvents,
          (unsigned long)session.timeoutEvents);
    print("  last wait %lu us\n", (unsigned long)session.lastWaitMicros);
  }
  strReceiveInterval(text, SETTING(RECEIVE_INTERVAL));
  print("receive %s (%lu)\n\n", text, (unsigned long)session.receives);

  print("from     new rep skp bad len\n");
  if (isHost()) {
    for (u32 slot = 0; slot < 4; slot++) {
      u16 id = 0;
      for (u32 i = 0; i < session.clients.connectedClientsSize; i++) {
        if (session.clients.connectedClients[i].clientNumber == slot)
          id = session.clients.connectedClients[i].deviceId;
      }
      char from[8];
      if (id)
        std::sprintf(from, "%c%lu %04x",
                     slot == session.selectedSlot ? '>' : ' ',
                     (unsigned long)slot, id);
      else
        std::sprintf(from, "%c%lu ----",
                     slot == session.selectedSlot ? '>' : ' ',
                     (unsigned long)slot);
      printStats(from, &session.fromClient[slot]);
    }
  } else {
    printStats("host", &session.fromHost);
  }

  print("\nframe %lu  %s\n\n", (unsigned long)session.frame, session.status);
  if (isHost())
    print("L/R slot  A kick  START close\n");
  print("SELECT = stop");
}

void waitForNextFrame() {
  if (SETTING(RECEIVE_INTERVAL) == NONSTOP) {
    // Keep asking the adapter for data until the frame is over
    u32 frame = vblankCount;
    while (vblankCount == frame)
      receivePackets();
  } else {
    VBlankIntrWait();
  }
}

#define CHECK_ERRORS(MESSAGE)                                               \
  if (!success ||                                                           \
      linkRawWireless->getState() == LinkRawWireless::State::NEEDS_RESET) { \
    showError(MESSAGE);                                                     \
    goto endSession;                                                        \
  }

int main() {
  Common::initTTE();

  linkRawWireless = new LinkRawWireless();

  interrupt_init();
  interrupt_add(INTR_VBLANK, [] { vblankCount++; });
  interrupt_add(INTR_SERIAL, LINK_RAW_WIRELESS_ISR_SERIAL);

  u32 opt = 0;

  while (true) {
    u16 pressed = pressedKeys();
    Setting* current = &settings[opt];

    if (pressed & KEY_UP)
      opt = (opt + SETTING_COUNT - 1) % SETTING_COUNT;
    if (pressed & KEY_DOWN)
      opt = (opt + 1) % SETTING_COUNT;
    if (pressed & KEY_LEFT)
      current->selected =
          (current->selected + current->count - 1) % current->count;
    if (pressed & KEY_RIGHT)
      current->selected = (current->selected + 1) % current->count;

    clearScreen();
    print("LinkRawWireless_lab (v8.0.5)\n\n");
    for (u32 i = 0; i < SETTING_COUNT; i++) {
      char value[32];
      settings[i].describe(value, SETTING(i));
      print("%c%-14s%s\n", i == opt ? '>' : ' ', settings[i].name, value);
    }
    print("\nUP/DOWN/LEFT/RIGHT = change\nA = start");
    showScreen();

    if (!(pressed & KEY_A)) {
      VBlankIntrWait();
      continue;
    }

    // A pressed, start hosting or connecting!
    session = {};
    session.status = "";
    bool success = true;

    if (isHost()) {
      clearScreen();
      print("Hosting...\n\n(SELECT = cancel)");
      showScreen();

      static char gameName[LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH + 1] =
          GAME_NAME;
      static char userName[LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH + 1] = "Host";
      success = setUpAdapter() &&
                linkRawWireless->broadcast(gameName, userName) &&
                linkRawWireless->startHost() && readDeviceId();
      CHECK_ERRORS("Serve failed :(")

      do {
        if (pressedKeys() & KEY_SELECT)
          goto endSession;

        VBlankIntrWait();
        success = linkRawWireless->pollConnections(session.clients);
        CHECK_ERRORS("Accept failed :(")
      } while (session.clients.connectedClientsSize == 0);
    } else {
      // we just find hosts and connect to the first one that
      // has the right GAME_NAME
      success = setUpAdapter() && linkRawWireless->broadcastReadStart();
      CHECK_ERRORS("Search failed :(")

      for (u32 frame = 1; session.hostId == 0; frame++) {
        if (pressedKeys() & KEY_SELECT)
          goto endSession;

        clearScreen();
        print("Searching%.*s\n\n(SELECT = cancel)", (int)(1 + (frame / 10) % 3),
              "...");
        showScreen();
        VBlankIntrWait();

        if (frame % 60 != 0)
          continue;

        LinkRawWireless::BroadcastReadPollResponse found;
        success = linkRawWireless->broadcastReadPoll(found);
        CHECK_ERRORS("Search failed :(")
        for (u32 i = 0; i < found.serversSize; i++) {
          if (std::strcmp(found.servers[i].gameName, GAME_NAME) == 0) {
            session.hostId = found.servers[i].id;
            break;
          }
        }
      }

      success = linkRawWireless->broadcastReadEnd() &&
                linkRawWireless->connect(session.hostId);
      CHECK_ERRORS("Connect failed 1 :(")

      LinkRawWireless::ConnectionStatus status;
      do {
        VBlankIntrWait();
        success = linkRawWireless->keepConnecting(status) &&
                  status.phase != LinkRawWireless::ConnectionPhase::ERROR;
        CHECK_ERRORS("Connect failed 2 :(")
      } while (status.phase ==
               LinkRawWireless::ConnectionPhase::STILL_CONNECTING);

      success = linkRawWireless->finishConnection() && readDeviceId();
      CHECK_ERRORS("Connect failed 3 :(")
    }

    // main loop, keeps receiving and sending packets according to config.
    while (true) {
      CHECK_ERRORS("Error :(")
      u16 keys = pressedKeys();
      session.frame++;

      // Check if data must be sent and send it
      u32 sendEvery = SETTING(SEND_INTERVAL);
      if (sendEvery != NEVER && sendEvery != ON_NEW_DATA &&
          session.frame % sendEvery == 0)
        sendPacket();

      // Same with receive
      u32 receiveEvery = SETTING(RECEIVE_INTERVAL);
      if (receiveEvery != NONSTOP && session.frame % receiveEvery == 0)
        receivePackets();

      // Who is (still) connected?
      if (session.frame % CHECK_EVERY_FRAMES == 0) {
        if (isHost()) {
          if (session.isServerClosed) {
            // if the server is closed, we can't use `pollConnections(...)`, so
            // we use `getSlotStatus(...)` instead
            LinkRawWireless::SlotStatusResponse status;
            success = linkRawWireless->getSlotStatus(status);
            if (success) {
              session.clients.connectedClientsSize =
                  status.connectedClientsSize;
              for (u32 i = 0; i < status.connectedClientsSize; i++)
                session.clients.connectedClients[i] =
                    status.connectedClients[i];
            }
          } else {
            success = linkRawWireless->pollConnections(session.clients);
          }
        } else {
          LinkRawWireless::SystemStatusResponse status;
          success = linkRawWireless->getSystemStatus(status) &&
                    status.adapterState == LinkRawWireless::State::CONNECTED;
        }
      }

      if (keys & KEY_SELECT)
        goto endSession;

      if (isHost()) {
        // L/R allows to pick a client slot, A disconnects it, START closes
        // server
        u32 slot = session.selectedSlot;
        if (keys & KEY_L)
          session.selectedSlot = (slot + 3) % 4;
        if (keys & KEY_R)
          session.selectedSlot = (slot + 1) % 4;
        if (keys & KEY_A) {
          bool kicked = linkRawWireless->disconnectClient(slot == 0, slot == 1,
                                                          slot == 2, slot == 3);
          session.status = kicked ? "kicked!" : "can't kick";
        }
        if ((keys & KEY_START) && !session.isServerClosed) {
          session.isServerClosed = linkRawWireless->endHost(session.clients);
          session.status = session.isServerClosed ? "closed!" : "can't close";
        }
      }

      // update screen every now and then
      waitForNextFrame();
      if ((session.frame & 7) == 0) {
        printSession();
        showScreen();
      }
    }

  endSession:
    linkRawWireless->deactivate();
  }

  return 0;
}
