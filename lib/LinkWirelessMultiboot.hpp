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
#define LINK_WIRELESS_MULTIBOOT_SETUP_MAGIC 0x003F0000
#define LINK_WIRELESS_MULTIBOOT_SETUP_TX 1
#define LINK_WIRELESS_MULTIBOOT_SETUP_WAIT_TIMEOUT 32
#define LINK_WIRELESS_MULTIBOOT_GAME_ID_MULTIBOOT_FLAG 0b1000000000000000
#define LINK_WIRELESS_MULTIBOOT_FRAME_LINES 228

const u8 LINK_WIRELESS_MULTIBOOT_CMD_START[] = {0x00, 0x54, 0x00, 0x00,
                                                0x00, 0x02, 0x00};
const u8 LINK_WIRELESS_MULTIBOOT_CMD_START_SIZE = 7;

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

    LWMLOG("new client");  // TODO: MOVE HANDSHAKE TO WHILE

    bool hasData = false;
    LinkWirelessOpenSDK::ChildrenData childrenData;
    LinkWirelessOpenSDK::ClientSDKHeader lastValidHeader;

    // HANDSHAKE
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(toArray(), 0, 1, response))
        return FAILURE;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);
      hasData = childrenData.responses[0].packetsSize > 0;
    }
    LWMLOG("handshake received");
    LWMLOG("sending ACK");
    lastValidHeader = childrenData.responses[0]
                          .packets[childrenData.responses[0].packetsSize - 1]
                          .header;

    // ACK 1
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerACKBuffer(lastValidHeader),
              response))
        return FAILURE;

      if (response.dataSize == 0)
        continue;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.n == 2 &&
            header.commState == LinkWirelessOpenSDK::CommState::STARTING) {
          hasData = true;
          lastValidHeader = header;
          break;
        }
      }
    }
    LWMLOG("N IS NOW 2, commState = 1");

    // ACK 2
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerACKBuffer(lastValidHeader),
              response))
        return FAILURE;

      if (response.dataSize == 0)
        continue;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.n == 1 &&
            header.commState == LinkWirelessOpenSDK::CommState::COMMUNICATING) {
          hasData = true;
          lastValidHeader = header;
          break;
        }
      }
    }
    LWMLOG("NI STARTED");

    // RECEIVE NAME
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerACKBuffer(lastValidHeader),
              response))
        return FAILURE;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);
      lastValidHeader = childrenData.responses[0]
                            .packets[childrenData.responses[0].packetsSize - 1]
                            .header;

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.commState == LinkWirelessOpenSDK::CommState::OFF) {
          hasData = true;
          break;
        }
      }
    }
    LWMLOG("commState IS NOW 0");

    // WAITING DATA STOP
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(toArray(), 0, 1, response))
        return FAILURE;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);
      hasData = childrenData.responses[0].packetsSize == 0;
    }
    LWMLOG("ready to send start command");

    linkRawWireless->wait(LINK_WIRELESS_MULTIBOOT_FRAME_LINES);

    // ROM START COMMAND
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerBuffer(
                  LINK_WIRELESS_MULTIBOOT_CMD_START,
                  LINK_WIRELESS_MULTIBOOT_CMD_START_SIZE, 1, 0,
                  LinkWirelessOpenSDK::CommState::STARTING, 0, 0b0001),
              response))
        return FAILURE;

      if (response.dataSize == 0)
        continue;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.isACK == 1 && header.n == 1 && header.phase == 0 &&
            header.commState == LinkWirelessOpenSDK::CommState::STARTING) {
          hasData = true;
          break;
        }
      }
    }
    LWMLOG("READY TO SEND ROM!");

    // ROM START
    u32 transferredBytes = 0;
    u32 n = 1;
    u32 phase = 0;
    u32 progress = 0;
    while (transferredBytes < romSize) {
      auto sendBuffer = linkWirelessOpenSDK->createServerBuffer(
          rom, romSize, n, phase, LinkWirelessOpenSDK::CommState::COMMUNICATING,
          transferredBytes, 0b0001);
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(sendBuffer.data, sendBuffer.dataSize,
                             sendBuffer.totalByteCount, response)) {
        LWMLOG("SendData failed!");
        return FAILURE;
      }
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.isACK && header.n == n && header.phase == phase) {
          phase++;
          if (phase == 4) {
            phase = 0;
            n++;
            if (n == 4)
              n = 0;
          }
          transferredBytes += sendBuffer.header.payloadSize;
          u32 newProgress = transferredBytes * 100 / romSize;
          if (newProgress != progress) {
            progress = newProgress;
            LWMLOG("-> " + std::to_string(transferredBytes * 100 / romSize));
          }
          break;
        }
      }
    }
    LWMLOG("SEND FINISHED! Confirming...");

    // ROM END COMMAND
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerBuffer(
                  {}, 0, 0, 0, LinkWirelessOpenSDK::CommState::ENDING, 0,
                  0b0001),
              response))
        return FAILURE;
      if (response.dataSize == 0)
        continue;
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.isACK == 1 && header.n == 0 && header.phase == 0 &&
            header.commState == LinkWirelessOpenSDK::CommState::ENDING) {
          hasData = true;
          break;
        }
      }
    }
    LWMLOG("Reconfirming...");

    // ROM END 2 COMMAND
    hasData = false;
    while (!hasData) {
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(
              linkWirelessOpenSDK->createServerBuffer(
                  {}, 0, 1, 0, LinkWirelessOpenSDK::CommState::OFF, 0, 0b0001),
              response))
        return FAILURE;
      hasData = true;
    }

    LWMLOG("SUCCESS!");

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
    if (!link->setup(players, LINK_WIRELESS_MULTIBOOT_SETUP_TX,
                     LINK_WIRELESS_MULTIBOOT_SETUP_WAIT_TIMEOUT,
                     LINK_WIRELESS_MULTIBOOT_SETUP_MAGIC)) {
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

  bool sendAndExpectData(LinkWirelessOpenSDK::SendBuffer<
                             LinkWirelessOpenSDK::ServerSDKHeader> sendBuffer,
                         LinkRawWireless::ReceiveDataResponse& response) {
    return sendAndExpectData(sendBuffer.data, sendBuffer.dataSize,
                             sendBuffer.totalByteCount, response);
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
      LWMLOG("! sendDataAndWait failed");
      return false;
    }
    if (remoteCommand.commandId != 0x28) {
      LWMLOG("! expected EVENT 0x28");
      LWMLOG("! but got " + link->toHex(remoteCommand.commandId));
      return false;
    }
    if (remoteCommand.paramsSize > 0) {
      if (((remoteCommand.params[0] >> 8) & 0b0001) == 0) {
        // TODO: MULTIPLE CHILDREN
        LWMLOG("! child timeout");
        return false;
      }
    }
    success = link->receiveData(response);
    if (!success) {
      LWMLOG("! receiveData failed");
      return false;
    }
    return true;
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