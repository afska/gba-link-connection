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
//         romBytes, // for current ROM, use: ((const u8*)MEM_EWRAM)
//         romLength, // in bytes
//         "Multiboot", // game name
//         "Test", // user name
//         0xffff, // game ID
//         2, // number of players
//         [](LinkWirelessMultiboot::MultibootProgress progress) {
//           // check progress.[state,connectedClients,percentage]
//
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, the transfer will be canceled)
//         }
//       );
//       // `result` should be LinkWirelessMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include "LinkRawWireless.hpp"
#include "LinkWirelessOpenSDK.hpp"

#ifndef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
/**
 * @brief Enable logging.
 * \warning Set `linkWirelessMultiboot->logger` and uncomment to enable!
 */
// #define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
#endif

static volatile char LINK_WIRELESS_MULTIBOOT_VERSION[] =
    "LinkWirelessMultiboot/v7.1.0";

#define LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xc0)
#define LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS 2
#define LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS 5
#define LINK_WIRELESS_MULTIBOOT_BARRIER asm volatile("" ::: "memory")
#define LINK_WIRELESS_MULTIBOOT_TRY(CALL) \
  LINK_WIRELESS_MULTIBOOT_BARRIER;        \
  if ((lastResult = CALL) != SUCCESS) {   \
    return finish(lastResult);            \
  }

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
#include <string>
#define _LWMLOG_(str) logger(str)
#else
#define _LWMLOG_(str)
#endif

/**
 * @brief A Wireless Multiboot tool to send small ROMs from a GBA to up to 4
 * slaves.
 */
class LinkWirelessMultiboot {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  using CommState = LinkWirelessOpenSDK::CommState;
  using Sequence = LinkWirelessOpenSDK::SequenceNumber;
  using ClientHeader = LinkWirelessOpenSDK::ClientSDKHeader;
  using ClientPacket = LinkWirelessOpenSDK::ClientPacket;
  using ChildrenData = LinkWirelessOpenSDK::ChildrenData;
  using SendBuffer =
      LinkWirelessOpenSDK::SendBuffer<LinkWirelessOpenSDK::ServerSDKHeader>;

  static constexpr int HEADER_SIZE = 0xC0;
  static constexpr int SETUP_MAGIC = 0x003c0000;
  static constexpr int SETUP_TX = 1;
  static constexpr int SETUP_WAIT_TIMEOUT = 32;
  static constexpr int GAME_ID_MULTIBOOT_FLAG = (1 << 15);
  static constexpr int FRAME_LINES = 228;
  static constexpr int MAX_INFLIGHT_PACKETS = 4;
  static constexpr u8 CMD_START[] = {0x00, 0x54, 0x00, 0x00, 0x00, 0x02, 0x00};
  static constexpr u8 CMD_START_SIZE = 7;
  static constexpr u8 BOOTLOADER_HANDSHAKE[][6] = {
      {0x00, 0x00, 0x52, 0x46, 0x55, 0x2d},
      {0x4d, 0x42, 0x2d, 0x44, 0x4c, 0x00}};
  static constexpr u8 BOOTLOADER_HANDSHAKE_SIZE = 6;
  static constexpr u8 ROM_HEADER_PATCH[] = {0x52, 0x46, 0x55, 0x2d, 0x4d, 0x42,
                                            0x4f, 0x4f, 0x54, 0x00, 0x00, 0x00};
  static constexpr u8 ROM_HEADER_PATCH_OFFSET = 4;
  static constexpr u8 ROM_HEADER_PATCH_SIZE = 12;

 public:
#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
  typedef void (*Logger)(std::string);
  Logger logger = [](std::string str) {};
#endif

  enum Result {
    SUCCESS,
    INVALID_SIZE,
    INVALID_PLAYERS,
    CANCELED,
    ADAPTER_NOT_DETECTED,
    BAD_HANDSHAKE,
    CLIENT_DISCONNECTED,
    FAILURE
  };
  enum State { STOPPED, INITIALIZING, WAITING, PREPARING, SENDING, CONFIRMING };

  struct MultibootProgress {
    State state = STOPPED;
    u32 connectedClients = 0;
    u32 percentage = 0;
    volatile bool* ready = nullptr;
  };

  /**
   * @brief Sends the `rom`. Once completed, the return value should be
   * `LinkWirelessMultiboot::Result::SUCCESS`.
   * @param rom A pointer to ROM data.
   * @param romSize Size of the ROM in bytes. It must be a number between
   * `448` and `262144`. It's recommended to use a ROM size that is a multiple
   * of `16`, as this also ensures compatibility with Multiboot via Link Cable.
   * @param gameName Game name. Maximum `14` characters + null terminator.
   * @param userName User name. Maximum `8` characters + null terminator.
   * @param gameId `(0 ~ 0x7FFF)` Game ID.
   * @param players The exact number of consoles that will download the ROM.
   * Once this number of players is reached, the code will start transmitting
   * the ROM bytes.
   * @param listener A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted. It receives a
   * `LinkWirelessMultiboot::MultibootProgress` object with details.
   * \warning You can start the transfer before the player count is reached by
   * running `*progress.ready = true;` in the `listener` callback.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename C>
  Result sendRom(const u8* rom,
                 u32 romSize,
                 const char* gameName,
                 const char* userName,
                 const u16 gameId,
                 u8 players,
                 C listener) {
    if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
      return INVALID_SIZE;
    if (players < LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS ||
        players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS)
      return INVALID_PLAYERS;

    resetState();

    _LWMLOG_("starting...");
    LINK_WIRELESS_MULTIBOOT_TRY(activate())
    progress.state = INITIALIZING;
    LINK_WIRELESS_MULTIBOOT_TRY(initialize(gameName, userName, gameId, players))

    _LWMLOG_("waiting for connections...");
    progress.state = WAITING;
    LINK_WIRELESS_MULTIBOOT_TRY(waitForClients(players, listener))

    _LWMLOG_("all players are connected");
    progress.state = PREPARING;

    _LWMLOG_("rom start command...");
    LINK_WIRELESS_MULTIBOOT_TRY(sendRomStartCommand(listener))

    _LWMLOG_("SENDING ROM!");
    progress.state = SENDING;
    LINK_WIRELESS_MULTIBOOT_TRY(sendRomBytes(rom, romSize, listener))

    progress.state = CONFIRMING;
    LINK_WIRELESS_MULTIBOOT_TRY(confirm(listener))

    _LWMLOG_("SUCCESS!");
    return finish(SUCCESS);
  }

  ~LinkWirelessMultiboot() {
    delete linkRawWireless;
    delete linkWirelessOpenSDK;
  }

  LinkRawWireless* linkRawWireless = new LinkRawWireless();
  LinkWirelessOpenSDK* linkWirelessOpenSDK = new LinkWirelessOpenSDK();

 private:
  struct PendingTransfer {
    u32 cursor;
    bool ack;
    bool isActive = false;
  };

  struct PendingTransferList {
    std::array<PendingTransfer, MAX_INFLIGHT_PACKETS> transfers;

    PendingTransfer* max(bool ack = false) {
      int maxCursor = -1;
      int maxI = -1;
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++) {
        if (transfers[i].isActive && (int)transfers[i].cursor > maxCursor &&
            (!ack || transfers[i].ack)) {
          maxCursor = transfers[i].cursor;
          maxI = i;
        }
      }
      return maxI > -1 ? &transfers[maxI] : nullptr;
    }

    PendingTransfer* minWithoutAck() {
      u32 minCursor = 0xffffffff;
      int minI = -1;
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++) {
        if (transfers[i].isActive && transfers[i].cursor < minCursor &&
            !transfers[i].ack) {
          minCursor = transfers[i].cursor;
          minI = i;
        }
      }
      return minI > -1 ? &transfers[minI] : nullptr;
    }

    void addIfNeeded(u32 newCursor) {
      auto maxTransfer = max();
      if (maxTransfer != nullptr && newCursor <= maxTransfer->cursor)
        return;

      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++) {
        if (!transfers[i].isActive) {
          transfers[i].cursor = newCursor;
          transfers[i].ack = false;
          transfers[i].isActive = true;
          break;
        }
      }
    }

    int ack(Sequence sequence) {
      int index = findIndex(sequence);
      if (index == -1)
        return -1;

      transfers[index].ack = true;

      auto maxAckTransfer = max(true);
      bool canUpdateCursor = maxAckTransfer != nullptr &&
                             isAckCompleteUpTo(maxAckTransfer->cursor);

      if (canUpdateCursor)
        cleanup();

      return canUpdateCursor ? maxAckTransfer->cursor + 1 : -1;
    }

    void cleanup() {
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++) {
        if (transfers[i].isActive && transfers[i].ack)
          transfers[i].isActive = false;
      }
    }

    bool isFull() { return size() == MAX_INFLIGHT_PACKETS; }

    u32 size() {
      u32 size = 0;
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++)
        if (transfers[i].isActive)
          size++;
      return size;
    }

   private:
    bool isAckCompleteUpTo(u32 cursor) {
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++)
        if (transfers[i].isActive && !transfers[i].ack &&
            transfers[i].cursor < cursor)
          return false;
      return true;
    }

    int findIndex(Sequence sequence) {
      for (u32 i = 0; i < MAX_INFLIGHT_PACKETS; i++) {
        if (transfers[i].isActive &&
            Sequence::fromPacketId(transfers[i].cursor) == sequence) {
          return i;
        }
      }

      return -1;
    }
  };

  struct Transfer {
    u32 cursor = 0;
    PendingTransferList pendingTransferList;

    u32 nextCursor(bool canSendInflightPackets) {
      u32 pendingCount = pendingTransferList.size();

      if (canSendInflightPackets && pendingCount > 0 &&
          pendingCount < MAX_INFLIGHT_PACKETS) {
        return pendingTransferList.max()->cursor + 1;
      } else {
        auto minWithoutAck = pendingTransferList.minWithoutAck();
        return minWithoutAck != nullptr ? minWithoutAck->cursor : cursor;
      }
    }

    void addIfNeeded(u32 newCursor) {
      if (newCursor >= cursor)
        pendingTransferList.addIfNeeded(newCursor);
    }

    u32 transferred() {
      return cursor * LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER;
    }

    Sequence sequence() { return Sequence::fromPacketId(cursor); }
  };

  MultibootProgress progress;
  volatile bool readyFlag = false;
  volatile Result lastResult;
  ClientHeader lastValidHeader;

  using TransferArray =
      std::array<Transfer, LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS - 1>;

  Result activate() {
    if (!linkRawWireless->activate()) {
      _LWMLOG_("! adapter not detected");
      return ADAPTER_NOT_DETECTED;
    }
    _LWMLOG_("activated");

    return SUCCESS;
  }

  Result initialize(const char* gameName,
                    const char* userName,
                    const u16 gameId,
                    u8 players) {
    if (!linkRawWireless->setup(players, SETUP_TX, SETUP_WAIT_TIMEOUT,
                                SETUP_MAGIC)) {
      _LWMLOG_("! setup failed");
      return FAILURE;
    }
    _LWMLOG_("setup ok");

    if (!linkRawWireless->broadcast(gameName, userName,
                                    gameId | GAME_ID_MULTIBOOT_FLAG)) {
      _LWMLOG_("! broadcast failed");
      return FAILURE;
    }
    _LWMLOG_("broadcast data set");

    if (!linkRawWireless->startHost()) {
      _LWMLOG_("! start host failed");
      return FAILURE;
    }
    _LWMLOG_("host started");

    return SUCCESS;
  }

  template <typename C>
  Result waitForClients(u8 players, C listener) {
    LinkRawWireless::AcceptConnectionsResponse acceptResponse;

    u32 currentPlayers = 1;
    while ((linkRawWireless->playerCount() < players && !readyFlag) ||
           linkRawWireless->playerCount() <= 1) {
      if (listener(progress))
        return finish(CANCELED);

      linkRawWireless->acceptConnections(acceptResponse);

      if (linkRawWireless->playerCount() > currentPlayers) {
        currentPlayers = linkRawWireless->playerCount();
        progress.connectedClients = currentPlayers - 1;

        u8 lastClientNumber =
            acceptResponse
                .connectedClients[acceptResponse.connectedClientsSize - 1]
                .clientNumber;
        LINK_WIRELESS_MULTIBOOT_TRY(handshakeClient(lastClientNumber, listener))
      }
    }

    readyFlag = true;

    return SUCCESS;
  }

  template <typename C>
  Result handshakeClient(u8 clientNumber, C listener) {
    ClientPacket handshakePackets[2];
    volatile bool hasReceivedName = false;

    _LWMLOG_("new client: " + std::to_string(clientNumber));
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        clientNumber,
        [this](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(toArray(), 0, 1, response);
        },
        [](ClientPacket packet) { return true; }, listener))
    // (initial client packet received)

    _LWMLOG_("handshake (1/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACKData(
        clientNumber,
        [](ClientPacket packet) {
          auto header = packet.header;
          return header.n == 2 && header.commState == CommState::STARTING;
        },
        listener))
    // (n = 2, commState = 1)

    _LWMLOG_("handshake (2/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACKData(
        clientNumber,
        [&handshakePackets](ClientPacket packet) {
          auto header = packet.header;
          bool isValid = header.n == 1 && header.phase == 0 &&
                         header.commState == CommState::COMMUNICATING;
          if (isValid)
            handshakePackets[0] = packet;
          return isValid;
        },
        listener))
    // (n = 1, commState = 2)

    _LWMLOG_("receiving name...");
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeACKData(
        clientNumber,
        [this, &handshakePackets, &hasReceivedName](ClientPacket packet) {
          auto header = packet.header;
          lastValidHeader = header;
          if (header.n == 1 && header.phase == 1 &&
              header.commState == CommState::COMMUNICATING) {
            handshakePackets[1] = packet;
            hasReceivedName = true;
          }
          return header.commState == CommState::OFF;
        },
        listener))
    // (commState = 0)

    _LWMLOG_("validating name...");
    for (u32 i = 0; i < 2; i++) {
      auto receivedPayload = handshakePackets[i].payload;
      auto expectedPayload = BOOTLOADER_HANDSHAKE[i];

      for (u32 j = 0; j < BOOTLOADER_HANDSHAKE_SIZE; j++) {
        if (!hasReceivedName || receivedPayload[j] != expectedPayload[j]) {
          _LWMLOG_("! bad payload");
          return finish(BAD_HANDSHAKE);
        }
      }
    }

    _LWMLOG_("draining queue...");
    volatile bool hasFinished = false;
    while (!hasFinished) {
      if (listener(progress))
        return finish(CANCELED);

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(sendAndExpectData(toArray(), 0, 1, response))
      auto childrenData = linkWirelessOpenSDK->getChildrenData(response);
      hasFinished = childrenData.responses[clientNumber].packetsSize == 0;
    }
    // (no more client packets)

    _LWMLOG_("client " + std::to_string(clientNumber) + " accepted");

    return SUCCESS;
  }

  template <typename C>
  Result sendRomStartCommand(C listener) {
    for (u32 i = 0; i < progress.connectedClients; i++) {
      LINK_WIRELESS_MULTIBOOT_TRY(exchangeNewData(
          i,
          linkWirelessOpenSDK->createServerBuffer(
              CMD_START, CMD_START_SIZE, {1, 0, CommState::STARTING}, 1 << i),
          listener))
    }

    return SUCCESS;
  }

  template <typename C>
  Result sendRomBytes(const u8* rom, u32 romSize, C listener) {
    ChildrenData childrenData;
    TransferArray transfers;
    u8 firstPagePatch[LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER];
    for (u32 i = 0; i < LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER; i++) {
      firstPagePatch[i] =
          i >= ROM_HEADER_PATCH_OFFSET &&
                  i < ROM_HEADER_PATCH_OFFSET + ROM_HEADER_PATCH_SIZE
              ? ROM_HEADER_PATCH[i - ROM_HEADER_PATCH_OFFSET]
              : rom[i];
    }

    progress.percentage = 0;
    u32 minClient = 0;

    while (transfers[minClient = findMinClient(transfers)].transferred() <
           romSize) {
      if (listener(progress))
        return finish(CANCELED);

      LINK_WIRELESS_MULTIBOOT_TRY(ensureAllClientsAreStillAlive())

      u32 cursor = findMinCursor(transfers);
      u32 offset = cursor * LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER;
      auto sequence = Sequence::fromPacketId(cursor);
      const u8* bufferToSend = cursor == 0 ? (const u8*)firstPagePatch : rom;

      auto sendBuffer = linkWirelessOpenSDK->createServerBuffer(
          bufferToSend, romSize, sequence, 0b1111, offset);

      for (u32 i = 0; i < progress.connectedClients; i++)
        transfers[i].addIfNeeded(cursor);

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(sendAndExpectData(sendBuffer, response))
      childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < progress.connectedClients; i++) {
        for (u32 j = 0; j < childrenData.responses[i].packetsSize; j++) {
          auto header = childrenData.responses[i].packets[j].header;

          if (header.isACK) {
            int newAckCursor =
                transfers[i].pendingTransferList.ack(header.sequence());
            if (newAckCursor > -1)
              transfers[i].cursor = newAckCursor;
          }
        }
      }

      u32 newPercentage = Link::_min(
          transfers[findMinClient(transfers)].transferred() * 100 / romSize,
          100);
      if (newPercentage != progress.percentage) {
        progress.percentage = newPercentage;
        _LWMLOG_("-> " + std::to_string(newPercentage));
      }
    }

    return SUCCESS;
  }

  u32 findMinClient(TransferArray& transfers) {
    u32 minTransferredBytes = 0xffffffff;
    u32 minClient = 0;

    for (u32 i = 0; i < progress.connectedClients; i++) {
      u32 transferred = transfers[i].transferred();
      if (transferred < minTransferredBytes) {
        minTransferredBytes = transferred;
        minClient = i;
      }
    }

    return minClient;
  }

  u32 findMinCursor(TransferArray& transfers) {
    u32 minNextCursor = 0xffffffff;

    bool canSendInflightPackets = true;
    for (u32 i = 0; i < progress.connectedClients; i++) {
      if (transfers[i].pendingTransferList.isFull())
        canSendInflightPackets = false;
    }

    for (u32 i = 0; i < progress.connectedClients; i++) {
      u32 nextCursor = transfers[i].nextCursor(canSendInflightPackets);
      if (nextCursor < minNextCursor)
        minNextCursor = nextCursor;
    }

    return minNextCursor;
  }

  template <typename C>
  Result confirm(C listener) {
    _LWMLOG_("confirming (1/2)...");
    for (u32 i = 0; i < progress.connectedClients; i++) {
      LINK_WIRELESS_MULTIBOOT_TRY(
          exchangeNewData(i,
                          linkWirelessOpenSDK->createServerBuffer(
                              {}, 0, {0, 0, CommState::ENDING}, 1 << i),
                          listener))
    }

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    _LWMLOG_("confirming (2/2)...");
    for (u32 i = 0; i < progress.connectedClients; i++) {
      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(
          sendAndExpectData(linkWirelessOpenSDK->createServerBuffer(
                                {}, 0, {1, 0, CommState::OFF}, 1 << i),
                            response))
    }

    return SUCCESS;
  }

  template <typename C>
  Result exchangeNewData(u8 clientNumber, SendBuffer sendBuffer, C listener) {
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        clientNumber,
        [this, &sendBuffer](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(sendBuffer, response);
        },
        [&sendBuffer](ClientPacket packet) {
          auto header = packet.header;
          return header.isACK == 1 &&
                 header.sequence() == sendBuffer.header.sequence();
        },
        listener))

    return SUCCESS;
  }

  template <typename V, typename C>
  Result exchangeACKData(u8 clientNumber, V validatePacket, C listener) {
    LINK_WIRELESS_MULTIBOOT_TRY(exchangeData(
        clientNumber,
        [this, clientNumber](LinkRawWireless::ReceiveDataResponse& response) {
          return sendAndExpectData(linkWirelessOpenSDK->createServerACKBuffer(
                                       lastValidHeader, clientNumber),
                                   response);
        },
        validatePacket, listener))

    return SUCCESS;
  }

  template <typename F, typename V, typename C>
  Result exchangeData(u8 clientNumber,
                      F sendAction,
                      V validatePacket,
                      C listener) {
    volatile bool hasFinished = false;
    while (!hasFinished) {
      if (listener(progress))
        return finish(CANCELED);

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY(sendAction(response))
      auto childrenData = linkWirelessOpenSDK->getChildrenData(response);

      for (u32 i = 0; i < childrenData.responses[clientNumber].packetsSize;
           i++) {
        auto packet = childrenData.responses[clientNumber].packets[i];
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

  Result sendAndExpectData(SendBuffer sendBuffer,
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
    volatile bool success = false;

    success =
        linkRawWireless->sendDataAndWait(data, dataSize, remoteCommand, _bytes);

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    if (!success) {
      _LWMLOG_("! sendDataAndWait failed");
      return FAILURE;
    }

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    if (remoteCommand.commandId != 0x28) {
      _LWMLOG_("! expected EVENT 0x28");
      _LWMLOG_("! but got " + toHex(remoteCommand.commandId));
      return FAILURE;
    }

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    if (remoteCommand.paramsSize > 0) {
      u8 expectedActiveChildren = 0;
      for (u32 i = 0; i < progress.connectedClients; i++)
        expectedActiveChildren |= 1 << i;
      u8 activeChildren =
          (remoteCommand.params[0] >> 8) & expectedActiveChildren;

      if (activeChildren != expectedActiveChildren) {
        _LWMLOG_("! client timeout [" + std::to_string(activeChildren) + "]");
        _LWMLOG_("! vs expected: [" + std::to_string(expectedActiveChildren) +
                 "]");
        return FAILURE;
      }
    }

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    success = linkRawWireless->receiveData(response);

    LINK_WIRELESS_MULTIBOOT_BARRIER;

    if (!success) {
      _LWMLOG_("! receiveData failed");
      return FAILURE;
    }

    return SUCCESS;
  }

  Result ensureAllClientsAreStillAlive() {
    LinkRawWireless::SlotStatusResponse slotStatusResponse;
    if (!linkRawWireless->getSlotStatus(slotStatusResponse))
      return FAILURE;

    if (slotStatusResponse.connectedClientsSize < progress.connectedClients)
      return CLIENT_DISCONNECTED;

    return SUCCESS;
  }

  Result finish(Result result) {
    linkRawWireless->deactivate();
    resetState();
    return result;
  }

  void resetState() {
    progress.state = STOPPED;
    progress.connectedClients = 0;
    progress.percentage = 0;
    progress.ready = &readyFlag;
    readyFlag = false;
  }

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
  template <typename I>
  std::string toHex(I w, size_t hex_len = sizeof(I) << 1) {
    static const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
      rc[i] = digits[(w >> j) & 0x0f];
    return rc;
  }
#endif

  template <typename... Args>
  std::array<u32, LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH> toArray(
      Args... args) {
    return {static_cast<u32>(args)...};
  }
};

extern LinkWirelessMultiboot* linkWirelessMultiboot;

#undef _LWMLOG_

#endif  // LINK_WIRELESS_MULTIBOOT_H
