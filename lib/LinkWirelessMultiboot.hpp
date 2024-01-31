#ifndef LINK_WIRELESS_MULTIBOOT_H
#define LINK_WIRELESS_MULTIBOOT_H

// --------------------------------------------------------------------------
// A Wireless Multiboot tool to send small ROMs from a GBA to up to 4 slaves.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWirelessMultiboot* linkWirelessMultiboot =
//         new LinkWirelessMultiboot();
// - 2) Send the ROM:
//       LinkWirelessMultiboot::Result result = linkWirelessMultiboot->sendRom(
//         romBytes, // for current ROM, use: ((const void*)MEM_EWRAM)
//         romLength, // should be multiple of 0x10
//         []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, transfer will be canceled)
//         }
//       );
//       // `result` should be LinkWirelessMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include "LinkRawWireless.hpp"
#include "LinkWirelessOpenSDK.hpp"

// Enable logging (set `linkWirelessMultiboot->logger` and uncomment to enable)
#define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING  // TODO: DISABLE

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
#define LWMLOG(str) logger(str)
#else
#define LWMLOG(str)
#endif

#define LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xc0)
#define LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS 2
#define LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS 5
#define LINK_WIRELESS_MULTIBOOT_HEADER_SIZE 0xC0
#define LINK_WIRELESS_MULTIBOOT_SETUP_MAGIC 0x003F0120
#define LINK_WIRELESS_MULTIBOOT_GAME_ID_MULTIBOOT_FLAG 0b1000000000000000
#define LINK_WIRELESS_MULTIBOOT_FRAME_LINES 228
#define LINK_WIRELESS_MULTIBOOT_FRAMES_BEFORE_HANDSHAKE 20  // ~300ms

static volatile char LINK_WIRELESS_MULTIBOOT_VERSION[] =
    "LinkWirelessMultiboot/v6.2.0";

class LinkWirelessMultiboot {
  typedef void (*Logger)(std::string);

 public:
  Logger logger = [](std::string str) {};
  enum Result {
    SUCCESS,
    INVALID_SIZE,
    INVALID_PLAYERS,
    CANCELED,
    ADAPTER_NOT_DETECTED,
    FAILURE
  };

  template <typename F>
  Result sendRom(const u8* rom,
                 u32 romSize,
                 const char* gameName,
                 const char* userName,
                 const u16 gameId,
                 u8 players,
                 F cancel) {
    if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
      return INVALID_SIZE;  // TODO: Document no 0x10 boundary limit
    if (players < LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS ||
        players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS)
      return INVALID_PLAYERS;

    if (!activate())
      return ADAPTER_NOT_DETECTED;

    if (!initialize(gameName, userName, gameId, players))
      return FAILURE;

    LinkRawWireless::AcceptConnectionsResponse acceptResponse;
    while (link->playerCount() < players) {
      link->acceptConnections(acceptResponse);
      if (cancel(players)) {
        link->deactivate();
        return CANCELED;
      }
    }

    LWMLOG("connected!");  // TODO: MOVE HANDSHAKE TO WHILE
    linkRawWireless->wait(LINK_WIRELESS_MULTIBOOT_FRAME_LINES *
                          LINK_WIRELESS_MULTIBOOT_FRAMES_BEFORE_HANDSHAKE);

    // HANDSHAKE

    bool hasData = false;
    LinkRawWireless::ReceiveDataResponse response;
    while (!hasData) {
      if (!sendAndExpectData(toArray(), 0, 1, response))
        return FAILURE;
      hasData = response.dataSize > 0;
    }

    LWMLOG("data received");
    LinkWirelessOpenSDK::ClientSDKHeader clientHeader =
        parseClientHeader(response.data[0]);
    LWMLOG("client size: " + std::to_string(clientHeader.payloadSize));
    LWMLOG("n: " + std::to_string(clientHeader.n));
    LWMLOG("phase: " + std::to_string(clientHeader.phase));
    LWMLOG("ack: " + std::to_string(clientHeader.isACK));
    LWMLOG("slotState:" + std::to_string(clientHeader.slotState));

    LWMLOG("sending ACK");
  firstack:
    LinkWirelessOpenSDK::ServerSDKHeader serverHeader;
    serverHeader = createACKFor(clientHeader);
    u32 sndHeader = serializeServerHeader(serverHeader);

    if (!sendAndExpectData(toArray(sndHeader), 1, 3, response))
      return FAILURE;

    if (response.dataSize == 0) {
      goto firstack;
    }
    clientHeader = parseClientHeader(response.data[0]);
    if (clientHeader.n == 1)
      goto firstack;

    if (clientHeader.n == 2 && clientHeader.slotState == 1) {
      LWMLOG("N IS NOW 2, slotstate = 1");
    } else {
      LWMLOG("Error: weird packet");
      return FAILURE;
    }

  secondack:
    serverHeader = createACKFor(clientHeader);
    sndHeader = serializeServerHeader(serverHeader);
    if (!sendAndExpectData(toArray(sndHeader), 1, 3, response))
      return FAILURE;

    if (response.dataSize == 0) {
      goto secondack;
    }
    clientHeader = parseClientHeader(response.data[0]);
    if (clientHeader.n == 2 && clientHeader.slotState == 1)
      goto secondack;

    if (clientHeader.n == 1 && clientHeader.slotState == 2) {
      LWMLOG("NI STARTED");
    } else {
      LWMLOG("NI DIDN'T START");
      return FAILURE;
    }

    while (clientHeader.slotState > 0) {
      link->wait(228);
      serverHeader = createACKFor(clientHeader);
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(toArray(sndHeader), 1, 3, response))
        return FAILURE;

      clientHeader = parseClientHeader(response.data[0]);
    }

    LWMLOG("slotState IS NOW 0");

    // ROM START COMMAND
    bool didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 7;
      serverHeader.n = 1;
      serverHeader.phase = 0;
      serverHeader.slotState = 1;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(toArray(sndHeader, 0x54, 0x02), 3, 10, response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK == 1 && clientHeader.n == 1 &&
          clientHeader.phase == 0 && clientHeader.slotState == 1)
        didClientRespond = true;
    }

    LWMLOG("READY TO SEND ROM!");

    // ROM START
    u32 transferredBytes = 0;
    u32 n = 1;
    u32 phase = 0;
    // bool isRetry = false;
    u32 progress = 0;
    while (transferredBytes < romSize) {
      // isRetry = false;
    retry:
      auto sendBuffer = linkWirelessOpenSDK->createServerBuffer(
          rom, romSize, n, phase, 2, transferredBytes, 0b0001);
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(sendBuffer.data, sendBuffer.dataSize,
                             sendBuffer.totalByteCount, response)) {
        LWMLOG("SendData failed!");
        return FAILURE;
      }
      if (response.dataSize == 0) {
        // isRetry = true;
        goto retry;
      }

      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK && clientHeader.n == n &&
          clientHeader.phase == phase) {
        phase++;
        if (phase == 4) {
          phase = 0;
          n++;
          if (n == 4)
            n = 0;
        }
        transferredBytes += 84;
        u32 newProgress = transferredBytes * 100 / romSize;
        if (newProgress != progress) {
          progress = newProgress;
          LWMLOG("-> " + std::to_string(transferredBytes * 100 / romSize));
        }
      } else {
        // isRetry = true;
        goto retry;
      }
    }

    LWMLOG("SEND FINISHED! Confirming...");

    // ROM END COMMAND
    didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 0;
      serverHeader.n = 0;
      serverHeader.phase = 0;
      serverHeader.slotState = 3;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(toArray(sndHeader), 1, 3, response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK == 1 && clientHeader.n == 0 &&
          clientHeader.phase == 0 && clientHeader.slotState == 3)
        didClientRespond = true;
    }

    LWMLOG("Reconfirming...");

    // ROM END 2 COMMAND
    didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 0;
      serverHeader.n = 1;
      serverHeader.phase = 0;
      serverHeader.slotState = 0;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(toArray(sndHeader), 1, 3, response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      // if (clientHeader.slotState == 0)
      didClientRespond = true;
    }

    LWMLOG("SUCCESS!");

    // u32 diffs = 0;
    // for (u32 i = 0; i < romSize; i++) {
    //   if (rom[i] != bytes[i]) {
    //     LWMLOG("DIFF AT " + std::to_string(i) + ": " + link->toHex(bytes[i])
    //     +
    //            " vs " + link->toHex(rom[i]));
    //     diffs++;
    //   }
    //   if (diffs > 100)
    //     break;
    // }

    // LWMLOG("??");

    return SUCCESS;
  }

  ~LinkWirelessMultiboot() {
    delete link;
    delete linkWirelessOpenSDK;
  }

  LinkRawWireless* link = new LinkRawWireless();
  LinkWirelessOpenSDK* linkWirelessOpenSDK = new LinkWirelessOpenSDK();

  LinkWirelessOpenSDK::ClientSDKHeader lastACK;

 private:
  bool activate() {
    if (!link->activate()) {
      LWMLOG("! adapter not detected");
      return false;
    }
    LWMLOG("activated");

    return true;
  }

  bool initialize(const char* gameName,
                  const char* userName,
                  const u16 gameId,
                  u8 players) {
    if (!link->setup(players, LINK_WIRELESS_MULTIBOOT_SETUP_MAGIC)) {
      LWMLOG("! setup failed");
      return false;
    }
    LWMLOG("setup ok");

    if (!link->broadcast(
            gameName, userName,
            gameId | LINK_WIRELESS_MULTIBOOT_GAME_ID_MULTIBOOT_FLAG)) {
      LWMLOG("! broadcast failed");
      return false;
    }
    LWMLOG("broadcast data set");

    if (!link->startHost()) {
      LWMLOG("! start host failed");
      return false;
    }
    LWMLOG("host started");

    return true;
  }

  bool sendAndExpectData(
      std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> data,
      u32 dataSize,
      u32 _bytes,
      LinkRawWireless::ReceiveDataResponse& response) {
    LinkRawWireless::RemoteCommand remoteCommand;
    bool success = false;
    success = link->sendDataAndWait(data, dataSize, remoteCommand, _bytes);
    if (!success) {
      LWMLOG("senddatawait no");
      return false;
    }
    if (remoteCommand.commandId != 0x28) {
      LWMLOG("expected response 0x28");
      LWMLOG("but got " + link->toHex(remoteCommand.commandId));
      return false;
    }
    success = link->receiveData(response);
    if (!success) {
      LWMLOG("receive data failed");
      return false;
    }
    return true;
  }

  LinkWirelessOpenSDK::ServerSDKHeader createACKFor(
      LinkWirelessOpenSDK::ClientSDKHeader clientHeader) {
    LinkWirelessOpenSDK::ServerSDKHeader serverHeader;
    serverHeader.isACK = 1;
    serverHeader.targetSlots = 0b0001;  //  TODO: Implement
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.slotState = clientHeader.slotState;

    return serverHeader;
  }

  LinkWirelessOpenSDK::ClientSDKHeader parseClientHeader(u32 clientHeaderInt) {
    return linkWirelessOpenSDK->parseClientHeader(clientHeaderInt);
  }

  u32 serializeServerHeader(LinkWirelessOpenSDK::ServerSDKHeader serverHeader) {
    return linkWirelessOpenSDK->serializeServerHeader(serverHeader);
  }

  template <typename... Args>
  std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> toArray(
      Args... args) {
    return {static_cast<u32>(args)...};
  }
};

extern LinkWirelessMultiboot* linkWirelessMultiboot;

#undef LWMLOG

#endif  // LINK_WIRELESS_MULTIBOOT_H