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
//         0xFFFF, // game ID
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
// - 3) (Optional) Send ROMs asynchronously:
//       LinkWirelessMultiboot::Async* linkWirelessMultibootAsync =
//         new LinkWirelessMultiboot::Async("Multiboot", "Test");
//       interrupt_init();
//       interrupt_add(INTR_VBLANK, LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_VBLANK);
//       interrupt_add(INTR_SERIAL, LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_SERIAL);
//       interrupt_add(INTR_TIMER3, LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_TIMER);
//       bool success = linkWirelessMultibootAsync->sendRom(
//         romBytes, romLength
//       );
//       if (success) {
//         // (monitor `playerCount()` and `getPercentage()`)
//         if (!linkWirelessMultibootAsync->isSending()) {
//           auto result = linkWirelessMultibootAsync->getResult();
//           // `result` should be
//           // LinkWirelessMultiboot::Async::GeneralResult::SUCCESS
//         }
//       }
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
 * \warning This option #includes std::string!
 */
// #define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
#endif

#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
/**
 * @brief Disable nested IRQs (uncomment to enable).
 * In the async version, SERIAL IRQs can be interrupted (once they clear their
 * time-critical needs) by default, which helps prevent issues with audio
 * engines. However, if something goes wrong, you can disable this behavior.
 */
// #define LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
#endif

LINK_VERSION_TAG LINK_WIRELESS_MULTIBOOT_VERSION =
    "vLinkWirelessMultiboot/v8.0.1";

#define LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xC0)
#define LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS 2
#define LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS 5
#define LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_INTERVAL 50
#define LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_TIMER_ID 3
#define LINK_WIRELESS_MULTIBOOT_TRY(CALL)       \
  LINK_BARRIER;                                 \
  if ((lastResult = CALL) != Result::SUCCESS) { \
    return finish(lastResult);                  \
  }
#define LINK_WIRELESS_MULTIBOOT_TRY_SUB(CALL)   \
  LINK_BARRIER;                                 \
  if ((lastResult = CALL) != Result::SUCCESS) { \
    return lastResult;                          \
  }

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
#include <string>
#define _LWMLOG_(str) logger(str)
#else
#define _LWMLOG_(str)
#endif

/**
 * @brief A Multiboot tool to send small ROMs from a GBA to up to 4 slaves via
 * GBA Wireless Adapter.
 */
class LinkWirelessMultiboot {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;

  using CommState = LinkWirelessOpenSDK::CommState;
  using Sequence = LinkWirelessOpenSDK::SequenceNumber;
  using ClientHeader = LinkWirelessOpenSDK::ClientSDKHeader;
  using ClientPacket = LinkWirelessOpenSDK::ClientPacket;
  using ChildrenData = LinkWirelessOpenSDK::ChildrenData;
  using SendBuffer =
      LinkWirelessOpenSDK::SendBuffer<LinkWirelessOpenSDK::ServerSDKHeader>;

  static constexpr int HEADER_SIZE = 0xC0;
  static constexpr int SETUP_TX = 1;
  static constexpr int GAME_ID_MULTIBOOT_FLAG = 1 << 15;
  static constexpr int FRAME_LINES = 228;
  static constexpr int MAX_INFLIGHT_PACKETS = 4;
  static constexpr int FINAL_CONFIRMS = 3;
  static constexpr u8 CMD_START[] = {0x00, 0x54, 0x00, 0x00, 0x00, 0x02, 0x00};
  static constexpr int CMD_START_SIZE = 7;
  static constexpr u8 BOOTLOADER_HANDSHAKE[][6] = {
      {0x00, 0x00, 0x52, 0x46, 0x55, 0x2D},
      {0x4D, 0x42, 0x2D, 0x44, 0x4C, 0x00}};
  static constexpr int BOOTLOADER_HANDSHAKE_SIZE = 6;
  static constexpr u8 ROM_HEADER_PATCH[] = {0x52, 0x46, 0x55, 0x2D, 0x4D, 0x42,
                                            0x4F, 0x4F, 0x54, 0x00, 0x00, 0x00};
  static constexpr int ROM_HEADER_PATCH_OFFSET = 4;
  static constexpr int ROM_HEADER_PATCH_SIZE = 12;

 public:
#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
  typedef void (*Logger)(std::string);
  Logger logger = [](std::string str) {};
#endif

  enum class State {
    STOPPED = 0,
    INITIALIZING = 1,
    LISTENING = 2,
    PREPARING = 3,
    SENDING = 4,
    CONFIRMING = 5
  };

  enum class Result {
    SUCCESS = 0,
    INVALID_SIZE = 1,
    INVALID_PLAYERS = 2,
    CANCELED = 3,
    ADAPTER_NOT_DETECTED = 4,
    BAD_HANDSHAKE = 5,
    CLIENT_DISCONNECTED = 6,
    FAILURE = 7
  };

  struct MultibootProgress {
    State state = State::STOPPED;
    u8 connectedClients = 0;
    u8 percentage = 0;
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
   * @param players The number of consoles that will download the ROM.
   * Once this number of players is reached, the code will start transmitting
   * the ROM bytes.
   * @param listener A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted. It receives a
   * `LinkWirelessMultiboot::MultibootProgress` object with details.
   * @param keepConnectionAlive If `true`, the adapter won't be reset after a
   * successful transfer, so users can continue the session using
   * `LinkWireless::restoreExistingConnection()`.
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
                 C listener,
                 bool keepConnectionAlive = false) {
    LINK_READ_TAG(LINK_WIRELESS_MULTIBOOT_VERSION);

    if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE ||
        romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
      return Result::INVALID_SIZE;
    if (players < LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS ||
        players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS)
      return Result::INVALID_PLAYERS;

    resetState();

    _LWMLOG_("starting...");
    LINK_WIRELESS_MULTIBOOT_TRY(activate())
    progress.state = State::INITIALIZING;
    LINK_WIRELESS_MULTIBOOT_TRY(initialize(gameName, userName, gameId, players))

    _LWMLOG_("waiting for connections...");
    progress.state = State::LISTENING;
    LINK_WIRELESS_MULTIBOOT_TRY(waitForClients(players, listener))

    _LWMLOG_("all players are connected");
    progress.state = State::PREPARING;

    _LWMLOG_("rom start command...");
    LINK_WIRELESS_MULTIBOOT_TRY(sendRomStartCommand(listener))

    _LWMLOG_("SENDING ROM!");
    progress.state = State::SENDING;
    LINK_WIRELESS_MULTIBOOT_TRY(sendRomBytes(rom, romSize, listener))

    progress.state = State::CONFIRMING;
    LINK_WIRELESS_MULTIBOOT_TRY(confirm(listener))

    _LWMLOG_("SUCCESS!");
    return finish(Result::SUCCESS, keepConnectionAlive);
  }

  /**
   * @brief Turns off the adapter and deactivates the library. It returns a
   * boolean indicating whether the transition to low consumption mode was
   * successful.
   */
  bool reset() {
    bool success = linkRawWireless.bye();
    linkRawWireless.deactivate();
    resetState();
    return success;
  }

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  /**
   * @brief Sets a logger function.
   * \warning This is internal API!
   */
  void _setLogger(LinkRawWireless::Logger logger) {
    linkRawWireless.logger = logger;
  }
#endif

 private:
  LinkRawWireless linkRawWireless;
  LinkWirelessOpenSDK linkWirelessOpenSDK;
  MultibootProgress progress;
  volatile bool readyFlag = false;
  volatile Result lastResult;
  ClientHeader lastValidHeader;

  Result activate() {
    if (!linkRawWireless.activate()) {
      _LWMLOG_("! adapter not detected");
      return Result::ADAPTER_NOT_DETECTED;
    }
    _LWMLOG_("activated");

    return Result::SUCCESS;
  }

  Result initialize(const char* gameName,
                    const char* userName,
                    const u16 gameId,
                    u8 players) {
    if (!linkRawWireless.setup(players, SETUP_TX)) {
      _LWMLOG_("! setup failed");
      return Result::FAILURE;
    }
    _LWMLOG_("setup ok");

    if (!linkRawWireless.broadcast(gameName, userName,
                                   gameId | GAME_ID_MULTIBOOT_FLAG)) {
      _LWMLOG_("! broadcast failed");
      return Result::FAILURE;
    }
    _LWMLOG_("broadcast data set");

    if (!linkRawWireless.startHost()) {
      _LWMLOG_("! start host failed");
      return Result::FAILURE;
    }
    _LWMLOG_("host started");

    return Result::SUCCESS;
  }

  template <typename C>
  Result waitForClients(u8 players, C listener) {
    LinkRawWireless::PollConnectionsResponse pollResponse;

    u32 currentPlayers = 1;
    while ((linkRawWireless.playerCount() < players && !readyFlag) ||
           linkRawWireless.playerCount() <= 1) {
      if (listener(progress))
        return Result::CANCELED;

      if (!linkRawWireless.pollConnections(pollResponse))
        return Result::FAILURE;

      if (linkRawWireless.playerCount() > currentPlayers) {
        currentPlayers = linkRawWireless.playerCount();
        progress.connectedClients = currentPlayers - 1;

        u8 lastClientNumber =
            pollResponse.connectedClients[pollResponse.connectedClientsSize - 1]
                .clientNumber;
        LINK_WIRELESS_MULTIBOOT_TRY_SUB(
            handshakeClient(lastClientNumber, listener))
      }
    }

    readyFlag = true;

    if (!linkRawWireless.endHost(pollResponse))
      return Result::FAILURE;

    return Result::SUCCESS;
  }

  template <typename C>
  Result handshakeClient(u8 clientNumber, C listener) {
    ClientPacket handshakePackets[2] = {ClientPacket{}, ClientPacket{}};
    bool hasReceivedName = false;

    _LWMLOG_("new client: " + std::to_string(clientNumber));
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeAndValidate(
        clientNumber,
        [this](LinkRawWireless::ReceiveDataResponse& response) {
          return exchange({}, 0, 1, response);
        },
        [](ClientPacket packet) { return true; }, listener))
    // (initial client packet received)

    _LWMLOG_("handshake (1/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeACKData(
        clientNumber,
        [](ClientPacket packet) {
          auto header = packet.header;
          return header.n == 2 && header.commState == CommState::STARTING;
        },
        listener))
    // (n = 2, commState = 1)

    _LWMLOG_("handshake (2/2)...");
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeACKData(
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
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeACKData(
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
    if (!validateName(handshakePackets, hasReceivedName)) {
      _LWMLOG_("! bad payload");
      return Result::BAD_HANDSHAKE;
    }

    _LWMLOG_("draining queue...");
    bool hasFinished = false;
    while (!hasFinished) {
      if (listener(progress))
        return Result::CANCELED;

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchange({}, 0, 1, response))
      auto childrenData = linkWirelessOpenSDK.getChildrenData(response);
      hasFinished = childrenData.responses[clientNumber].packetsSize == 0;
    }
    // (no more client packets)

    _LWMLOG_("client " + std::to_string(clientNumber) + " accepted");

    return Result::SUCCESS;
  }

  template <typename C>
  Result sendRomStartCommand(C listener) {
    for (u32 i = 0; i < progress.connectedClients; i++) {
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeNewData(
          i,
          linkWirelessOpenSDK.createServerBuffer(
              CMD_START, CMD_START_SIZE, {1, 0, CommState::STARTING}, 1 << i),
          listener))
    }

    return Result::SUCCESS;
  }

  template <typename C>
  Result sendRomBytes(const u8* rom, u32 romSize, C listener) {
    u8 firstPagePatch[LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER];
    generateFirstPagePatch(rom, firstPagePatch);
    progress.percentage = 0;

    LinkWirelessOpenSDK::MultiTransfer<MAX_INFLIGHT_PACKETS> multiTransfer(
        &linkWirelessOpenSDK);
    multiTransfer.configure(romSize, progress.connectedClients);

    while (!multiTransfer.hasFinished()) {
      if (listener(progress))
        return Result::CANCELED;

      LINK_WIRELESS_MULTIBOOT_TRY_SUB(ensureAllClientsAreStillAlive())

      auto sendBuffer = multiTransfer.createNextSendBuffer(
          multiTransfer.getCursor() == 0 ? (const u8*)firstPagePatch : rom);

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchange(sendBuffer, response))

      u8 newPercentage = multiTransfer.processResponse(response);
      progress.percentage = newPercentage;
    }

    return Result::SUCCESS;
  }

  template <typename C>
  Result confirm(C listener) {
    _LWMLOG_("confirming (1/2)...");
    for (u32 i = 0; i < progress.connectedClients; i++) {
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(
          exchangeNewData(i,
                          linkWirelessOpenSDK.createServerBuffer(
                              {}, 0, {0, 0, CommState::ENDING}, 1 << i),
                          listener))
    }

    _LWMLOG_("confirming (2/2)...");
    for (u32 i = 0; i < FINAL_CONFIRMS; i++) {
      LinkRawWireless::ReceiveDataResponse response;
      auto sendBuffer = linkWirelessOpenSDK.createServerBuffer(
          {}, 0, {1, 0, CommState::OFF}, 0b1111);
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchange(sendBuffer, response))
    }

    return Result::SUCCESS;
  }

  template <typename C>
  Result exchangeNewData(u8 clientNumber, SendBuffer sendBuffer, C listener) {
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeAndValidate(
        clientNumber,
        [this, &sendBuffer](LinkRawWireless::ReceiveDataResponse& response) {
          return exchange(sendBuffer, response);
        },
        [&sendBuffer](ClientPacket packet) {
          auto header = packet.header;
          return header.isACK == 1 &&
                 header.sequence() == sendBuffer.header.sequence();
        },
        listener))

    return Result::SUCCESS;
  }

  template <typename V, typename C>
  Result exchangeACKData(u8 clientNumber, V validatePacket, C listener) {
    LINK_WIRELESS_MULTIBOOT_TRY_SUB(exchangeAndValidate(
        clientNumber,
        [this, clientNumber](LinkRawWireless::ReceiveDataResponse& response) {
          auto sendBuffer = linkWirelessOpenSDK.createServerACKBuffer(
              lastValidHeader, clientNumber);
          return exchange(sendBuffer, response);
        },
        validatePacket, listener))

    return Result::SUCCESS;
  }

  template <typename F, typename V, typename C>
  Result exchangeAndValidate(u8 clientNumber,
                             F sendAction,
                             V validatePacket,
                             C listener) {
    while (true) {
      if (listener(progress))
        return Result::CANCELED;

      LinkRawWireless::ReceiveDataResponse response;
      LINK_WIRELESS_MULTIBOOT_TRY_SUB(sendAction(response))
      auto childrenData = linkWirelessOpenSDK.getChildrenData(response);

      if (isDataValid(clientNumber, childrenData, lastValidHeader,
                      validatePacket))
        break;
    }

    return Result::SUCCESS;
  }

  Result exchange(SendBuffer& sendBuffer,
                  LinkRawWireless::ReceiveDataResponse& response) {
    return exchange(sendBuffer.data, sendBuffer.dataSize,
                    sendBuffer.totalByteCount, response);
  }

  Result exchange(const u32* data,
                  u32 dataSize,
                  u32 _bytes,
                  LinkRawWireless::ReceiveDataResponse& response) {
    LinkRawWireless::CommandResult remoteCommand;
    bool success = false;

    success =
        linkRawWireless.sendDataAndWait(data, dataSize, remoteCommand, _bytes);

    if (!success) {
      _LWMLOG_("! sendDataAndWait failed");
      return Result::FAILURE;
    }

    if (remoteCommand.commandId != LinkRawWireless::EVENT_DATA_AVAILABLE) {
      _LWMLOG_("! expected EVENT 0x28");
      _LWMLOG_("! but got " + toHex(remoteCommand.commandId));
      return Result::FAILURE;
    }

    if (remoteCommand.dataSize > 0 &&
        !areAllConnected(&remoteCommand, progress.connectedClients)) {
      _LWMLOG_("! client timeout");
      return Result::CLIENT_DISCONNECTED;
    }

    success = linkRawWireless.receiveData(response);

    if (!success) {
      _LWMLOG_("! receiveData failed");
      return Result::FAILURE;
    }

    return Result::SUCCESS;
  }

  Result ensureAllClientsAreStillAlive() {
    LinkRawWireless::SlotStatusResponse slotStatusResponse;
    if (!linkRawWireless.getSlotStatus(slotStatusResponse))
      return Result::FAILURE;

    if (slotStatusResponse.connectedClientsSize < progress.connectedClients)
      return Result::CLIENT_DISCONNECTED;

    return Result::SUCCESS;
  }

  Result finish(Result result, bool keepConnectionAlive = false) {
    if (result != Result::SUCCESS || !keepConnectionAlive)
      linkRawWireless.bye();
    linkRawWireless.deactivate();
    resetState();
    return result;
  }

  void resetState() {
    LINK_BARRIER;
    progress.state = State::STOPPED;
    progress.connectedClients = 0;
    progress.percentage = 0;
    progress.ready = &readyFlag;
    readyFlag = false;
    lastValidHeader = ClientHeader{};
    LINK_BARRIER;
  }

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
  template <typename I>
  std::string toHex(I w, size_t hex_len = sizeof(I) << 1) {
    static const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
      rc[i] = digits[(w >> j) & 0x0F];
    return rc;
  }
#endif

  static bool validateName(ClientPacket* handshakePackets,
                           bool hasReceivedName) {
    for (u32 i = 0; i < 2; i++) {
      auto receivedPayload = handshakePackets[i].payload;
      auto expectedPayload = BOOTLOADER_HANDSHAKE[i];

      for (u32 j = 0; j < BOOTLOADER_HANDSHAKE_SIZE; j++) {
        if (!hasReceivedName || receivedPayload[j] != expectedPayload[j])
          return false;
      }
    }

    return true;
  }

  static void generateFirstPagePatch(const u8* rom, u8* firstPagePatch) {
    for (u32 i = 0; i < LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER; i++) {
      firstPagePatch[i] =
          i >= ROM_HEADER_PATCH_OFFSET &&
                  i < ROM_HEADER_PATCH_OFFSET + ROM_HEADER_PATCH_SIZE
              ? ROM_HEADER_PATCH[i - ROM_HEADER_PATCH_OFFSET]
              : rom[i];
    }
  }

  template <typename V>
  static bool isDataValid(u8 clientNumber,
                          ChildrenData& childrenData,
                          ClientHeader& lastReceivedHeader,
                          V validatePacket) {
    for (u32 i = 0; i < childrenData.responses[clientNumber].packetsSize; i++) {
      auto packet = childrenData.responses[clientNumber].packets[i];
      auto header = packet.header;
      if (validatePacket(packet)) {
        lastReceivedHeader = header;
        return true;
      }
    }
    return false;
  }

  static bool areAllConnected(LinkRawWireless::CommandResult* remoteCommand,
                              u32 connectedClients) {
    u8 expectedActiveChildren = 0;
    for (u32 i = 0; i < connectedClients; i++)
      expectedActiveChildren |= 1 << i;
    u8 activeChildren = (remoteCommand->data[0] >> 8) & expectedActiveChildren;

    return activeChildren == expectedActiveChildren;
  }

 public:
  /**
   * @brief [Asynchronous version] A Multiboot tool to send small ROMs from a
   * GBA to up to 4 slaves via GBA Wireless Adapter.
   */
  class Async : Link::AsyncMultiboot {
   private:
    using ServerHeader = LinkWirelessOpenSDK::ServerSDKHeader;

    static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
    static constexpr int FPS = 60;
    static constexpr int MAX_IRQ_TIMEOUT_FRAMES = FPS * 5;
    static constexpr int START_WAIT_FRAMES = 2;

   public:
#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
    Logger logger = [](std::string str){};
#endif

    using GeneralResult = Link::AsyncMultiboot::Result;

    enum class State {
      STOPPED = 0,
      INITIALIZING = 1,
      STARTING = 2,
      LISTENING = 3,
      HANDSHAKING_CLIENT_STEP1 = 4,
      HANDSHAKING_CLIENT_STEP2 = 5,
      HANDSHAKING_CLIENT_STEP3 = 6,
      HANDSHAKING_CLIENT_STEP4 = 7,
      HANDSHAKING_CLIENT_STEP5 = 8,
      ENDING_HOST = 9,
      SENDING_ROM_START_COMMAND = 10,
      ENSURING_CLIENTS_ALIVE = 11,
      SENDING_ROM_PART = 12,
      CONFIRMING_STEP1 = 13,
      CONFIRMING_STEP2 = 14,
    };

    enum class Result {
      NONE = -1,
      SUCCESS = 0,
      INVALID_SIZE = 1,
      INVALID_PLAYERS = 2,
      ADAPTER_NOT_DETECTED = 3,
      INIT_FAILURE = 4,
      BAD_HANDSHAKE = 5,
      CLIENT_DISCONNECTED = 6,
      FAILURE = 7,
      IRQ_TIMEOUT = 8
    };

    /**
     * @brief Constructs a new LinkWirelessMultiboot::Async object.
     * @param gameName Game name. Maximum `14` characters + null terminator.
     * @param userName User name. Maximum `8` characters + null terminator.
     * @param gameId `(0 ~ 0x7FFF)` The Game ID to be broadcasted.
     * @param players The number of consoles that will download the ROM.
     * Once this number of players is reached, the code will start transmitting
     * the ROM bytes, unless `waitForReadySignal` is `true`.
     * @param waitForReadySignal Whether the code should wait for a
     * `markReady()` call to start the actual transfer.
     * @param keepConnectionAlive If `true`, the adapter won't be reset after
     * a successful transfer, so users can continue the session using
     * `LinkWireless::restoreExistingConnection()`.
     * @param interval Number of *1024-cycle ticks* (61.04μs) between transfers
     * *(50 = 3.052ms)*. It's the interval of Timer #`timerId`. Lower values
     * will transfer faster but also consume more CPU. Some audio players
     * require precise interrupt timing to avoid crashes! Use a minimum of 30.
     * @param timerId `(0~3)` GBA Timer to use for waiting.
     */
    explicit Async(
        const char* gameName = "",
        const char* userName = "",
        u16 gameId = LINK_RAW_WIRELESS_MAX_GAME_ID,
        u8 players = 5,
        bool waitForReadySignal = false,
        bool keepConnectionAlive = false,
        u16 interval = LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_INTERVAL,
        u8 timerId = LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_TIMER_ID)
        : multiTransfer(&linkWirelessOpenSDK) {
      config.gameName = gameName;
      config.userName = userName;
      config.gameId = gameId;
      config.players = players;
      config.waitForReadySignal = waitForReadySignal;
      config.keepConnectionAlive = keepConnectionAlive;
      config.interval = interval;
      config.timerId = timerId;
    }

    /**
     * @brief Sends the `rom`. Once completed, `getState()` should return
     * `LinkWirelessMultiboot::Async::State::STOPPED` and `getResult()` should
     * return `LinkWirelessMultiboot::Async::GeneralResult::SUCCESS`. Returns
     * `false` if there's a pending transfer or the data is invalid.
     * @param rom A pointer to ROM data.
     * @param romSize Size of the ROM in bytes. It must be a number between
     * `448` and `262144`. It's recommended to use a ROM size that is a multiple
     * of `16`, as this also ensures compatibility with Multiboot via Link
     * Cable.
     */
    bool sendRom(const u8* rom, u32 romSize) override {
      if (state != State::STOPPED)
        return false;

      if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE ||
          romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE) {
        result = Result::INVALID_SIZE;
        return false;
      }
      if (config.players < LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS ||
          config.players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS) {
        result = Result::INVALID_PLAYERS;
        return false;
      }

      stop();

      fixedData.rom = rom;
      fixedData.romSize = romSize;
      fixedData.gameName = config.gameName;
      fixedData.userName = config.userName;
      fixedData.gameId = config.gameId;
      fixedData.players = config.players;
      fixedData.waitForReadySignal = config.waitForReadySignal;
      fixedData.keepConnectionAlive = config.keepConnectionAlive;
      fixedData.timerId = config.timerId;
      generateFirstPagePatch(rom, fixedData.firstPagePatch);

      _LWMLOG_("starting...");
      state = State::INITIALIZING;
      if (!linkRawWireless.activate()) {
        _LWMLOG_("! adapter not detected");
        stop(Result::ADAPTER_NOT_DETECTED);
        return false;
      }
      _LWMLOG_("activated");

      if (!linkRawWireless.setup(fixedData.players, SETUP_TX) ||
          !linkRawWireless.broadcast(
              fixedData.gameName, fixedData.userName,
              fixedData.gameId | GAME_ID_MULTIBOOT_FLAG) ||
          !linkRawWireless.startHost(false)) {
        _LWMLOG_("! init failed");
        stop(Result::INIT_FAILURE);
        return false;
      }
      _LWMLOG_("host started");

      state = State::STARTING;

      return true;
    }

    /**
     * @brief Turns off the adapter and deactivates the library, canceling the
     * in-progress transfer, if any. It returns a boolean indicating whether
     * the transition to low consumption mode was successful.
     * \warning Never call this method inside an interrupt handler!
     */
    bool reset() override { return stop(); }

    /**
     * @brief Returns whether there's an active transfer or not.
     */
    [[nodiscard]] bool isSending() override { return state != State::STOPPED; }

    /**
     * @brief Returns the current state.
     */
    [[nodiscard]] State getState() { return state; }

    /**
     * @brief Returns the result of the last operation. After this
     * call, the result is cleared if `clear` is `true` (default behavior).
     * @param clear Whether it should clear the result or not.
     */
    Link::AsyncMultiboot::Result getResult(bool clear = true) override {
      auto detailedResult = getDetailedResult(clear);
      switch (detailedResult) {
        case Result::NONE:
          return Link::AsyncMultiboot::Result::NONE;
        case Result::SUCCESS:
          return Link::AsyncMultiboot::Result::SUCCESS;
        case Result::INVALID_SIZE:
        case Result::INVALID_PLAYERS:
          return Link::AsyncMultiboot::Result::INVALID_DATA;
        case Result::ADAPTER_NOT_DETECTED:
        case Result::INIT_FAILURE:
          return Link::AsyncMultiboot::Result::INIT_FAILED;
        default:
          return Link::AsyncMultiboot::Result::FAILURE;
      }
    }

    /**
     * @brief Returns the detailed result of the last operation. After this
     * call, the result is cleared if `clear` is `true` (default behavior).
     * @param clear Whether it should clear the result or not.
     */
    Result getDetailedResult(bool clear = true) {
      Result _result = result;
      if (clear)
        result = Result::NONE;
      return _result;
    }

    /**
     * @brief Returns the number of connected players (`1~5`).
     */
    [[nodiscard]] u8 playerCount() override {
      return 1 + dynamicData.connectedClients;
    }

    /**
     * @brief Returns the completion percentage (0~100).
     */
    [[nodiscard]] u8 getPercentage() override {
      if (state == State::STOPPED || fixedData.romSize == 0)
        return 0;

      return dynamicData.percentage;
    }

    /**
     * @brief Returns whether the ready mark is active or not.
     */
    [[nodiscard]] bool isReady() override { return dynamicData.ready; }

    /**
     * @brief Marks the transfer as ready.
     */
    void markReady() override {
      if (state == State::STOPPED)
        return;

      dynamicData.ready = true;
    }

    /**
     * @brief This method is called by the VBLANK interrupt handler.
     * \warning This is internal API!
     */
    void _onVBlank() {
      if (state == State::STOPPED)
        return;

      processNewFrame();
    }

    /**
     * @brief This method is called by the SERIAL interrupt handler.
     * \warning This is internal API!
     */
    void _onSerial() {
      if (state == State::STOPPED || interrupt)
        return;

#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
      interrupt = true;
#endif
      if (linkRawWireless._onSerial() > 0) {
        auto response = linkRawWireless._getAsyncCommandResultRef();
#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
        Link::_REG_IME = 1;
#endif
        processResponse(response);
      }
#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
      interrupt = false;
#endif
    }

    /**
     * @brief This method is called by the TIMER interrupt handler.
     * \warning This is internal API!
     */
    void _onTimer() {
      if (state != State::SENDING_ROM_PART || interrupt)
        return;

      state = State::ENSURING_CLIENTS_ALIVE;
      checkClientsAlive();
      stopTimer();
    }

    struct Config {
      const char* gameName;
      const char* userName;
      u16 gameId;
      u8 players;
      bool waitForReadySignal;
      bool keepConnectionAlive;
      u16 interval;
      u8 timerId;
    };

    /**
     * @brief LinkWirelessMultiboot::Async configuration.
     * \warning `deactivate()` first, change the config, and `activate()` again!
     */
    Config config;

   private:
    enum class SendState { NOT_SENDING, SEND_AND_WAIT, RECEIVE };

    struct MultibootFixedData {
      const u8* rom = nullptr;
      u32 romSize = 0;
      const char* gameName = nullptr;
      const char* userName = nullptr;
      u16 gameId = 0;
      u8 players = 0;
      bool waitForReadySignal = false;
      bool keepConnectionAlive = false;
      u32 interval = LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_INTERVAL;
      u8 timerId = LINK_WIRELESS_MULTIBOOT_ASYNC_DEFAULT_TIMER_ID;

      u8 firstPagePatch[LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER] = {};
    };

    struct HandshakeClientData {
      ClientPacket packets[2] = {ClientPacket{}, ClientPacket{}};
      bool didReceiveName = false;
    };

    struct MultibootDynamicData {
      u32 irqTimeout = 0;
      u32 wait = 0;
      u32 frameTransfers = 0;

      u8 currentClient = 0;
      HandshakeClientData handshakeClient = HandshakeClientData{};
      u32 percentage = 0;
      u32 confirmationTry = 0;

      ClientHeader lastReceivedHeader = ClientHeader{};
      ServerHeader lastSentHeader = ServerHeader{};

      bool ready = false;
      u8 connectedClients = 0;
    };

    LinkRawWireless linkRawWireless;
    LinkWirelessOpenSDK linkWirelessOpenSDK;
    SendState sendState;
    MultibootFixedData fixedData;
    MultibootDynamicData dynamicData;
    LinkWirelessOpenSDK::MultiTransfer<MAX_INFLIGHT_PACKETS> multiTransfer;
    volatile State state = State::STOPPED;
    volatile Result result = Result::NONE;
#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
    volatile bool interrupt = false;
#endif

    void processNewFrame() {
      dynamicData.frameTransfers = 0;
      dynamicData.irqTimeout++;
      if (dynamicData.irqTimeout >= MAX_IRQ_TIMEOUT_FRAMES) {
#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
        if (!interrupt)
          stop(Result::IRQ_TIMEOUT);
#endif
        return;
      }

      switch (state) {
        case State::STARTING: {
          dynamicData.wait++;
          if (dynamicData.wait >= START_WAIT_FRAMES) {
            state = State::LISTENING;
            startOrKeepListening();
          }
          break;
        }
        default: {
        }
      }
    }

    void processResponse(LinkRawWireless::CommandResult* response) {
      dynamicData.irqTimeout = 0;

      if (sendState == SendState::SEND_AND_WAIT) {
        if (!response->success ||
            response->commandId != LinkRawWireless::EVENT_DATA_AVAILABLE)
          return (void)stop(Result::FAILURE);

        if (response->dataSize > 0 &&
            !areAllConnected(response, dynamicData.connectedClients))
          return (void)stop(Result::CLIENT_DISCONNECTED);

        receiveAsync();
        return;
      } else if (sendState == SendState::RECEIVE) {
        if (!response->success)
          return (void)stop(Result::FAILURE);

        sendState = SendState::NOT_SENDING;
      }

      switch (state) {
        case State::LISTENING: {
          if (!response->success)
            return (void)stop(Result::FAILURE);

          LINK_BARRIER;
          u32 newConnectedClients = response->dataSize;
          linkRawWireless.sessionState.playerCount = 1 + newConnectedClients;
          LINK_BARRIER;

          if (newConnectedClients > dynamicData.connectedClients) {
            dynamicData.connectedClients = newConnectedClients;
            u8 lastClientNumber =
                (u8)Link::msB32(response->data[response->dataSize - 1]);
            _LWMLOG_("new client: " + std::to_string(lastClientNumber));

            state = State::HANDSHAKING_CLIENT_STEP1;
            startHandshakeWith(lastClientNumber);
          } else {
            state = State::STARTING;
            dynamicData.wait = START_WAIT_FRAMES - 1;
          }
          break;
        }
        case State::HANDSHAKING_CLIENT_STEP1: {
          u8 currentClient = dynamicData.currentClient;

          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isDataValid(currentClient, childrenData,
                           dynamicData.lastReceivedHeader,
                           [](ClientPacket packet) { return true; }))
            return (void)startHandshakeWith(currentClient);

          _LWMLOG_("handshake (1/2)...");
          state = State::HANDSHAKING_CLIENT_STEP2;
          sendACKData(currentClient);
          break;
        }
        case State::HANDSHAKING_CLIENT_STEP2: {
          u8 currentClient = dynamicData.currentClient;

          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isDataValid(currentClient, childrenData,
                           dynamicData.lastReceivedHeader,
                           [](ClientPacket packet) {
                             auto header = packet.header;
                             return header.n == 2 &&
                                    header.commState == CommState::STARTING;
                           }))
            return (void)sendACKData(currentClient);

          _LWMLOG_("handshake (2/2)...");
          state = State::HANDSHAKING_CLIENT_STEP3;
          sendACKData(currentClient);
          break;
        }
        case State::HANDSHAKING_CLIENT_STEP3: {
          u8 currentClient = dynamicData.currentClient;

          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isDataValid(
                  currentClient, childrenData, dynamicData.lastReceivedHeader,
                  [this](ClientPacket packet) {
                    auto header = packet.header;
                    bool isValid = header.n == 1 && header.phase == 0 &&
                                   header.commState == CommState::COMMUNICATING;
                    if (isValid)
                      dynamicData.handshakeClient.packets[0] = packet;
                    return isValid;
                  }))
            return (void)sendACKData(currentClient);

          _LWMLOG_("receiving name...");
          state = State::HANDSHAKING_CLIENT_STEP4;
          sendACKData(currentClient);
          break;
        }
        case State::HANDSHAKING_CLIENT_STEP4: {
          u8 currentClient = dynamicData.currentClient;

          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isDataValid(
                  currentClient, childrenData, dynamicData.lastReceivedHeader,
                  [this](ClientPacket packet) {
                    auto header = packet.header;
                    dynamicData.lastReceivedHeader = header;
                    if (header.n == 1 && header.phase == 1 &&
                        header.commState == CommState::COMMUNICATING) {
                      dynamicData.handshakeClient.packets[1] = packet;
                      dynamicData.handshakeClient.didReceiveName = true;
                    }
                    return header.commState == CommState::OFF;
                  }))
            return (void)sendACKData(currentClient);

          _LWMLOG_("validating name...");
          if (!validateName(dynamicData.handshakeClient.packets,
                            dynamicData.handshakeClient.didReceiveName)) {
            _LWMLOG_("! bad payload");
            return (void)stop(Result::BAD_HANDSHAKE);
          }

          _LWMLOG_("draining queue...");
          state = State::HANDSHAKING_CLIENT_STEP5;
          exchangeAsync({}, 0, 1);
          break;
        }
        case State::HANDSHAKING_CLIENT_STEP5: {
          u8 currentClient = dynamicData.currentClient;

          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          bool hasFinished =
              childrenData.responses[currentClient].packetsSize == 0;
          if (!hasFinished)
            return (void)exchangeAsync({}, 0, 1);

          _LWMLOG_("client " + std::to_string(currentClient) + " accepted");

          startOrKeepListening();
          break;
        }
        case State::ENDING_HOST: {
          if (!response->success)
            return (void)stop(Result::FAILURE);

          _LWMLOG_("rom start command...");
          dynamicData.currentClient = 0;
          state = State::SENDING_ROM_START_COMMAND;
          sendRomStartCommand();
          break;
        }
        case State::SENDING_ROM_START_COMMAND: {
          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isValidAcknowledge(childrenData))
            return (void)sendRomStartCommand();

          dynamicData.currentClient++;
          if (dynamicData.currentClient < dynamicData.connectedClients)
            return (void)sendRomStartCommand();

          _LWMLOG_("SENDING ROM!");
          state = State::ENSURING_CLIENTS_ALIVE;
          multiTransfer.configure(fixedData.romSize,
                                  dynamicData.connectedClients);
          checkClientsAlive();
          break;
        }
        case State::ENSURING_CLIENTS_ALIVE: {
          if (!response->success)
            return (void)stop(Result::FAILURE);

          if (response->dataSize - 1 < dynamicData.connectedClients)
            return (void)stop(Result::CLIENT_DISCONNECTED);

          state = State::SENDING_ROM_PART;
          sendRomPart();
          break;
        }
        case State::SENDING_ROM_PART: {
          LinkRawWireless::ReceiveDataResponse receiveDataResponse;
          if (!response->success || !linkRawWireless.getReceiveDataResponse(
                                        *response, receiveDataResponse))
            return (void)stop(Result::FAILURE);

          u8 newPercentage = multiTransfer.processResponse(receiveDataResponse);
          dynamicData.percentage = newPercentage;

          dynamicData.frameTransfers++;
          startTimer();
          break;
        }
        case State::CONFIRMING_STEP1: {
          ChildrenData childrenData;
          if (!parseResponse(response, childrenData))
            return (void)stop(Result::FAILURE);

          if (!isValidAcknowledge(childrenData))
            return (void)sendConfirmation1();

          dynamicData.currentClient++;
          if (dynamicData.currentClient < dynamicData.connectedClients)
            return (void)sendConfirmation1();

          _LWMLOG_("confirming (2/2)...");
          state = State::CONFIRMING_STEP2;
          dynamicData.confirmationTry = 0;
          sendConfirmation2();
          break;
        }
        case State::CONFIRMING_STEP2: {
          if (!response->success)
            return (void)stop(Result::FAILURE);

          dynamicData.confirmationTry++;
          if (dynamicData.confirmationTry < FINAL_CONFIRMS)
            return (void)sendConfirmation2();

          _LWMLOG_("SUCCESS!");
          stop(Result::SUCCESS);
          break;
        }
        default: {
        }
      }
    }

    void pollConnections() {
      sendCommandAsync(LinkRawWireless::COMMAND_POLL_CONNECTIONS);
    }

    void startHandshakeWith(u8 clientNumber) {
      dynamicData.currentClient = clientNumber;
      dynamicData.handshakeClient = HandshakeClientData{};

      exchangeAsync({}, 0, 1);
    };

    void startOrKeepListening() {
      if (linkRawWireless.playerCount() <= 1 ||
          (fixedData.waitForReadySignal && !dynamicData.ready) ||
          (linkRawWireless.playerCount() < fixedData.players &&
           !dynamicData.ready)) {
        state = State::LISTENING;
        return (void)pollConnections();
      }

      dynamicData.ready = true;

      _LWMLOG_("all players are connected");
      state = State::ENDING_HOST;
      sendCommandAsync(LinkRawWireless::COMMAND_END_HOST);
    }

    void sendRomStartCommand() {
      u8 clientNumber = dynamicData.currentClient;

      auto sendBuffer = linkWirelessOpenSDK.createServerBuffer(
          CMD_START, CMD_START_SIZE, {1, 0, CommState::STARTING},
          1 << clientNumber);
      sendNewData(sendBuffer);
    }

    void checkClientsAlive() {
      if (multiTransfer.hasFinished()) {
        _LWMLOG_("confirming (1/2)...");
        state = State::CONFIRMING_STEP1;
        dynamicData.currentClient = 0;
        sendConfirmation1();
        return;
      }

      sendCommandAsync(LinkRawWireless::COMMAND_SLOT_STATUS);
    }

    void sendRomPart() {
      auto sendBuffer = multiTransfer.createNextSendBuffer(
          multiTransfer.getCursor() == 0 ? (const u8*)fixedData.firstPagePatch
                                         : fixedData.rom);
      exchangeAsync(sendBuffer);
    }

    void sendConfirmation1() {
      auto sendBuffer = linkWirelessOpenSDK.createServerBuffer(
          {}, 0, {0, 0, CommState::ENDING}, 1 << dynamicData.currentClient);
      sendNewData(sendBuffer);
    }

    void sendConfirmation2() {
      auto sendBuffer = linkWirelessOpenSDK.createServerBuffer(
          {}, 0, {1, 0, CommState::OFF}, 0b1111);
      sendNewData(sendBuffer);
    }

    bool parseResponse(LinkRawWireless::CommandResult* response,
                       ChildrenData& childrenData) {
      LinkRawWireless::ReceiveDataResponse receiveDataResponse;
      if (!response->success || !linkRawWireless.getReceiveDataResponse(
                                    *response, receiveDataResponse))
        return false;
      childrenData = linkWirelessOpenSDK.getChildrenData(receiveDataResponse);
      return true;
    }

    bool isValidAcknowledge(ChildrenData& childrenData) {
      return isDataValid(
          dynamicData.currentClient, childrenData,
          dynamicData.lastReceivedHeader, [this](ClientPacket packet) {
            auto header = packet.header;
            return header.isACK == 1 &&
                   header.sequence() == dynamicData.lastSentHeader.sequence();
          });
    }

    void sendNewData(SendBuffer& sendBuffer) {
      dynamicData.lastSentHeader = sendBuffer.header;
      exchangeAsync(sendBuffer);
    }

    void sendACKData(u8 clientNumber) {
      auto ackBuffer = linkWirelessOpenSDK.createServerACKBuffer(
          dynamicData.lastReceivedHeader, clientNumber);
      exchangeAsync(ackBuffer);
    }

    void exchangeAsync(SendBuffer& sendBuffer) {
      exchangeAsync(sendBuffer.data, sendBuffer.dataSize,
                    sendBuffer.totalByteCount);
    }

    void exchangeAsync(const u32* data, u32 dataSize, u32 _bytes) {
      u32 rawData[LINK_RAW_WIRELESS_MAX_COMMAND_TRANSFER_LENGTH];
      rawData[0] = linkRawWireless.getSendDataHeaderFor(_bytes);
      for (u32 i = 0; i < dataSize; i++)
        rawData[1 + i] = data[i];

      sendState = SendState::SEND_AND_WAIT;
      sendCommandAsync(LinkRawWireless::COMMAND_SEND_DATA_AND_WAIT, rawData,
                       1 + dataSize, true);
    }

    void receiveAsync() {
      sendState = SendState::RECEIVE;
      sendCommandAsync(LinkRawWireless::COMMAND_RECEIVE_DATA);
    }

    void stopTimer() {
      Link::_REG_TM[config.timerId].cnt =
          Link::_REG_TM[config.timerId].cnt & (~Link::_TM_ENABLE);
    }

    void startTimer() {
      Link::_REG_TM[config.timerId].start = -config.interval;
      Link::_REG_TM[config.timerId].cnt =
          Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
    }

    void sendCommandAsync(u8 type,
                          const u32* params = {},
                          u16 length = 0,
                          bool invertsClock = false) {
#ifndef LINK_WIRELESS_MULTIBOOT_ASYNC_DISABLE_NESTED_IRQ
      Link::_REG_IME = 0;
#endif
      linkRawWireless.sendCommandAsync(type, params, length, invertsClock);
    }

    void resetState(Result newResult = Result::NONE) {
      LINK_BARRIER;
      state = State::STOPPED;
      result = newResult;
      sendState = SendState::NOT_SENDING;
      fixedData = MultibootFixedData{};
      dynamicData = MultibootDynamicData{};
      LINK_BARRIER;
    }

    bool stop(Result newResult = Result::NONE) {
      bool keepConnectionAlive = fixedData.keepConnectionAlive;
      resetState(newResult);
      stopTimer();

      bool success = true;
      if (newResult != Result::SUCCESS || !keepConnectionAlive)
        success = linkRawWireless.bye();
      linkRawWireless.deactivate();
      return success;
    }
  };
};

extern LinkWirelessMultiboot* linkWirelessMultiboot;
extern LinkWirelessMultiboot::Async* linkWirelessMultibootAsync;

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_VBLANK() {
  linkWirelessMultibootAsync->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_SERIAL() {
  linkWirelessMultibootAsync->_onSerial();
}

/**
 * @brief TIMER interrupt handler.
 */
inline void LINK_WIRELESS_MULTIBOOT_ASYNC_ISR_TIMER() {
  linkWirelessMultibootAsync->_onTimer();
}

#undef _LWMLOG_

#endif  // LINK_WIRELESS_MULTIBOOT_H
