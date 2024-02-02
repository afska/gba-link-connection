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
#define LINK_WIRELESS_MULTIBOOT_TRY(CALL) \
  if ((lastResult = CALL) != SUCCESS) {   \
    link->deactivate();                   \
    return lastResult;                    \
  }

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

  template <typename C>
  Result sendRom(const u8* rom,
                 u32 romSize,
                 const char* gameName,
                 const char* userName,
                 const u16 gameId,
                 u8 players,
                 C cancel) {
    if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
      return INVALID_SIZE;  // TODO: Document no 0x10 boundary limit
    if (players < LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS ||
        players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS)
      return INVALID_PLAYERS;

    LINK_WIRELESS_MULTIBOOT_TRY(activate())
    LINK_WIRELESS_MULTIBOOT_TRY(initialize(gameName, userName, gameId, players))
    LINK_WIRELESS_MULTIBOOT_TRY(waitForClients(players, cancel))

    LWMLOG("all players are connected");
    linkRawWireless->wait(LINK_WIRELESS_MULTIBOOT_FRAME_LINES);

    LWMLOG("rom start command...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        [this](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(
              linkWirelessOpenSDK->createServerBuffer(
                  LINK_WIRELESS_MULTIBOOT_CMD_START,
                  LINK_WIRELESS_MULTIBOOT_CMD_START_SIZE, {1, 0},
                  LinkWirelessOpenSDK::CommState::STARTING, 0, 0b0001),
              response);
        },
        [](LinkWirelessOpenSDK::ClientPacket packet) {
          auto header = packet.header;
          return header.isACK == 1 && header.n == 1 && header.phase == 0 &&
                 header.commState == LinkWirelessOpenSDK::CommState::STARTING;
        },
        cancel))

    LWMLOG("SENDING ROM!");
    LinkWirelessOpenSDK::ChildrenData childrenData;
    LinkWirelessOpenSDK::SequenceNumber sequence = {.n = 1, .phase = 0};
    u32 transferredBytes = 0;
    u32 progress = 0;
    while (transferredBytes < romSize) {
      // TODO: Check cancel
      auto sendBuffer = linkWirelessOpenSDK->createServerBuffer(
          rom, romSize, sequence, LinkWirelessOpenSDK::CommState::COMMUNICATING,
          transferredBytes, 0b0001);
      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(
          sendAndExpectData(sendBuffer.data, sendBuffer.dataSize,
                            sendBuffer.totalByteCount, response))
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto header = childrenData.responses[0].packets[i].header;
        if (header.isACK && header.sequence() == sequence) {
          sequence.inc();
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

    LWMLOG("confirming (1/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        [this](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(
              linkWirelessOpenSDK->createServerBuffer(
                  {}, 0, {0, 0}, LinkWirelessOpenSDK::CommState::ENDING, 0,
                  0b0001),
              response);
        },
        [](LinkWirelessOpenSDK::ClientPacket packet) {
          auto header = packet.header;
          return header.isACK == 1 && header.n == 0 && header.phase == 0 &&
                 header.commState == LinkWirelessOpenSDK::CommState::ENDING;
        },
        cancel))

    LWMLOG("confirming (2/2)...");
    LinkRawWireless::ReceiveDataResponse response;
    LINK_WIRELESS_MULTIBOOT_TRY(sendAndExpectData(
        linkWirelessOpenSDK->createServerBuffer(
            {}, 0, {1, 0}, LinkWirelessOpenSDK::CommState::OFF, 0, 0b0001),
        response))

    LWMLOG("SUCCESS!");

    return SUCCESS;
  }

  ~LinkWirelessMultiboot() {
    delete link;
    delete linkWirelessOpenSDK;
  }

  // TODO: CLEANUP
  LinkRawWireless* link = new LinkRawWireless();
  LinkWirelessOpenSDK* linkWirelessOpenSDK = new LinkWirelessOpenSDK();
  LinkWirelessOpenSDK::ClientSDKHeader lastValidHeader;

  Result lastResult;

 private:
  Result activate() {
    if (!link->activate()) {
      LWMLOG("! adapter not detected");
      return ADAPTER_NOT_DETECTED;
    }
    LWMLOG("activated");

    return SUCCESS;
  }

  Result initialize(const char* gameName,
                    const char* userName,
                    const u16 gameId,
                    u8 players) {
    if (!link->setup(players, LINK_WIRELESS_MULTIBOOT_SETUP_TX,
                     LINK_WIRELESS_MULTIBOOT_SETUP_WAIT_TIMEOUT,
                     LINK_WIRELESS_MULTIBOOT_SETUP_MAGIC)) {
      LWMLOG("! setup failed");
      return FAILURE;
    }
    LWMLOG("setup ok");

    if (!link->broadcast(
            gameName, userName,
            gameId | LINK_WIRELESS_MULTIBOOT_GAME_ID_MULTIBOOT_FLAG)) {
      LWMLOG("! broadcast failed");
      return FAILURE;
    }
    LWMLOG("broadcast data set");

    if (!link->startHost()) {
      LWMLOG("! start host failed");
      return FAILURE;
    }
    LWMLOG("host started");

    return SUCCESS;
  }

  template <typename C>
  Result waitForClients(u8 players, C cancel) {
    LinkRawWireless::AcceptConnectionsResponse acceptResponse;

    u32 currentPlayers = 1;
    while (link->playerCount() < players) {
      if (cancel(link->playerCount()))
        return CANCELED;

      link->acceptConnections(acceptResponse);

      if (link->playerCount() > currentPlayers) {
        currentPlayers = link->playerCount();

        u8 lastClientNumber = acceptResponse.connectedClientsSize - 1;
        LINK_WIRELESS_MULTIBOOT_TRY(handshakeClient(lastClientNumber, cancel))
      }
    }

    return SUCCESS;
  }

  template <typename C>
  Result handshakeClient(u8 clientNumber, C cancel) {
    LWMLOG("new client: " + std::to_string(clientNumber));
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        [this](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(toArray(), 0, 1, response);
        },
        [](LinkWirelessOpenSDK::ClientPacket packet) { return true; }, cancel))
    // initial packet received

    LWMLOG("handshake (1/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACK(
        clientNumber,
        [](LinkWirelessOpenSDK::ClientPacket packet) {
          auto header = packet.header;
          return header.n == 2 &&
                 header.commState == LinkWirelessOpenSDK::CommState::STARTING;
        },
        cancel))
    // n = 2, commState = 1

    LWMLOG("handshake (2/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACK(
        clientNumber,
        [](LinkWirelessOpenSDK::ClientPacket packet) {
          auto header = packet.header;
          return header.n == 1 &&
                 header.commState ==
                     LinkWirelessOpenSDK::CommState::COMMUNICATING;
        },
        cancel))
    // n = 1, commState = 2

    LWMLOG("receiving name...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACK(
        clientNumber,
        [this](LinkWirelessOpenSDK::ClientPacket packet) {
          this->lastValidHeader = packet.header;
          return packet.header.commState == LinkWirelessOpenSDK::CommState::OFF;
        },
        cancel))
    // commState = 0

    LWMLOG("draining queue...");
    bool hasFinished = false;
    while (!hasFinished) {
      if (cancel(link->playerCount()))
        return CANCELED;

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(sendAndExpectData(toArray(), 0, 1, response))
      auto childrenData = linkWirelessOpenSDK->getChildrenData(response);
      hasFinished = childrenData.responses[0].packetsSize == 0;
    }
    LWMLOG("client " + std::to_string(clientNumber) + " accepted");

    return SUCCESS;
  }

  template <typename V, typename C>
  Result exchangeACK(u8 clientNumber, V validatePacket, C cancel) {
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        [this, clientNumber](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(linkWirelessOpenSDK->createServerACKBuffer(
                                       lastValidHeader, clientNumber),
                                   response);
        },
        validatePacket, cancel))

    return SUCCESS;
  }

  template <typename F, typename V, typename C>
  Result exchangeData(F sendAction, V validatePacket, C cancel) {
    bool hasFinished = false;
    while (!hasFinished) {
      if (cancel(link->playerCount()))
        return CANCELED;

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(sendAction(response))
      auto childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[0].packetsSize; i++) {
        auto packet = childrenData.responses[0].packets[i];
        auto header = packet.header;
        if (validatePacket(packet)) {
          hasFinished = true;
          lastValidHeader = header;
          break;
        }
      }
    }

    return SUCCESS;
  }

  Result sendAndExpectData(LinkWirelessOpenSDK::SendBuffer<
                               LinkWirelessOpenSDK::ServerSDKHeader> sendBuffer,
                           LinkRawWireless::ReceiveDataResponse& response) {
    return sendAndExpectData(sendBuffer.data, sendBuffer.dataSize,
                             sendBuffer.totalByteCount, response);
  }

  Result sendAndExpectData(
      std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> data,
      u32 dataSize,
      u32 _bytes,
      LinkRawWireless::ReceiveDataResponse& response) {
    LinkRawWireless::RemoteCommand remoteCommand;
    bool success = false;

    success = link->sendDataAndWait(data, dataSize, remoteCommand, _bytes);
    if (!success) {
      LWMLOG("! sendDataAndWait failed");
      return FAILURE;
    }

    if (remoteCommand.commandId != 0x28) {
      LWMLOG("! expected EVENT 0x28");
      LWMLOG("! but got " + link->toHex(remoteCommand.commandId));
      return FAILURE;
    }

    if (remoteCommand.paramsSize > 0) {
      if (((remoteCommand.params[0] >> 8) & 0b0001) == 0) {
        // TODO: MULTIPLE CHILDREN
        LWMLOG("! child timeout");
        return FAILURE;
      }
    }

    success = link->receiveData(response);
    if (!success) {
      LWMLOG("! receiveData failed");
      return FAILURE;
    }

    return SUCCESS;
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