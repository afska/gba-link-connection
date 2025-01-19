#ifndef LINK_CABLE_MULTIBOOT_H
#define LINK_CABLE_MULTIBOOT_H

// --------------------------------------------------------------------------
// A Multiboot tool to send small programs from one GBA to up to 3 slaves.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCableMultiboot* linkCableMultiboot = new LinkCableMultiboot();
// - 2) Send the ROM:
//       LinkCableMultiboot::Result result = linkCableMultiboot->sendRom(
//         romBytes, // for current ROM, use: ((const u8*)MEM_EWRAM)
//         romLength, // in bytes, should be multiple of 0x10
//         []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, the transfer will be canceled)
//         }
//       );
//       // `result` should be LinkCableMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------
// considerations:
// - stop DMA before sending the ROM! (you might need to stop your audio player)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "LinkRawCable.hpp"
#include "LinkSPI.hpp"

#ifndef LINK_CABLE_MULTIBOOT_PALETTE_DATA
/**
 * @brief Palette data (controls how the logo is displayed).
 * Format: 0b1CCCDSS1, where C=color, D=direction, S=speed.
 * Default: 0b10010011
 */
#define LINK_CABLE_MULTIBOOT_PALETTE_DATA 0b10010011
#endif

LINK_VERSION_TAG LINK_CABLE_MULTIBOOT_VERSION = "vLinkCableMultiboot/v8.0.0";

#define LINK_CABLE_MULTIBOOT_TRY(CALL)   \
  partialResult = CALL;                  \
  if (partialResult == ABORTED)          \
    return error(CANCELED);              \
  else if (partialResult == NEEDS_RETRY) \
    goto retry;

/**
 * @brief A Multiboot tool to send small programs from one GBA to up to 3
 * slaves.
 */
class LinkCableMultiboot {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;

  static constexpr int MIN_ROM_SIZE = 0x100 + 0xc0;
  static constexpr int MAX_ROM_SIZE = 256 * 1024;
  static constexpr int FRAME_LINES = 228;
  static constexpr int INITIAL_WAIT_MIN_FRAMES = 4;
  static constexpr int INITIAL_WAIT_MAX_RANDOM_FRAMES = 30;
  static constexpr int INITIAL_WAIT_MIN_LINES =
      FRAME_LINES * INITIAL_WAIT_MIN_FRAMES;
  static constexpr int DETECTION_TRIES = 16;
  static constexpr int CLIENTS = 3;
  static constexpr int CLIENT_NO_DATA = 0xff;
  static constexpr int HANDSHAKE = 0x6200;
  static constexpr int HANDSHAKE_RESPONSE = 0x7200;
  static constexpr int CONFIRM_CLIENTS = 0x6100;
  static constexpr int SEND_PALETTE = 0x6300;
  static constexpr int HANDSHAKE_DATA = 0x11;
  static constexpr int CONFIRM_HANDSHAKE_DATA = 0x6400;
  static constexpr int ACK_RESPONSE = 0x7300;
  static constexpr int ACK_RESPONSE_MASK = 0xff00;
  static constexpr int HEADER_SIZE = 0xC0;
  static constexpr auto MAX_BAUD_RATE = LinkRawCable::BaudRate::BAUD_RATE_3;

  struct Response {
    u32 data[LINK_RAW_CABLE_MAX_PLAYERS];
    int playerId = -1;  // (-1 = unknown)
  };

 public:
  enum Result { SUCCESS, INVALID_SIZE, CANCELED, FAILURE_DURING_TRANSFER };

  enum TransferMode {
    SPI = 0,
    MULTI_PLAY = 1
  };  // (used in SWI call, do not swap)

  /**
   * @brief Sends the `rom`. Once completed, the return value should be
   * `LinkCableMultiboot::Result::SUCCESS`.
   * @param rom A pointer to ROM data. Must be 4-byte aligned.
   * @param romSize Size of the ROM in bytes. It must be a number between `448`
   * and `262144`, and a multiple of `16`.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * @param mode Either `TransferMode::MULTI_PLAY` for GBA cable (default value)
   * or `TransferMode::SPI` for GBC cable.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  Result sendRom(const u8* rom,
                 u32 romSize,
                 F cancel,
                 TransferMode mode = TransferMode::MULTI_PLAY) {
    LINK_READ_TAG(LINK_CABLE_MULTIBOOT_VERSION);

    this->_mode = mode;
    if (romSize < MIN_ROM_SIZE || romSize > MAX_ROM_SIZE ||
        (romSize % 0x10) != 0)
      return INVALID_SIZE;

  retry:
    stop();

    // (*) instead of 1/16s, waiting a random number of frames works better
    Link::wait(INITIAL_WAIT_MIN_LINES +
               FRAME_LINES *
                   Link::_qran_range(1, INITIAL_WAIT_MAX_RANDOM_FRAMES));

    // 1. Prepare a "Multiboot Parameter Structure" in RAM.
    PartialResult partialResult = NEEDS_RETRY;
    Link::_MultiBootParam multiBootParameters;
    multiBootParameters.client_data[0] = CLIENT_NO_DATA;
    multiBootParameters.client_data[1] = CLIENT_NO_DATA;
    multiBootParameters.client_data[2] = CLIENT_NO_DATA;
    multiBootParameters.palette_data = LINK_CABLE_MULTIBOOT_PALETTE_DATA;
    multiBootParameters.client_bit = 0;
    multiBootParameters.boot_srcp = (u8*)rom + HEADER_SIZE;
    multiBootParameters.boot_endp = (u8*)rom + romSize;

    LINK_CABLE_MULTIBOOT_TRY(detectClients(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(sendHeader(multiBootParameters, rom, cancel))
    LINK_CABLE_MULTIBOOT_TRY(sendPalette(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(confirmHandshakeData(multiBootParameters, cancel))

    // 9. Call SWI 0x25, with r0 set to the address of the multiboot parameter
    // structure and r1 set to the communication mode (0 for normal, 1 for
    // MultiPlay).
    int result = Link::_MultiBoot(&multiBootParameters, (int)_mode);

    stop();

    // 10. Upon return, r0 will be either 0 for success, or 1 for failure. If
    // successful, all clients have received the multiboot program successfully
    // and are now executing it - you can begin either further data transfer or
    // a multiplayer game from here.
    return result == 1 ? FAILURE_DURING_TRANSFER : SUCCESS;
  }

 private:
  LinkRawCable linkRawCable;
  LinkSPI linkSPI;
  TransferMode _mode;

  enum PartialResult { NEEDS_RETRY, FINISHED, ABORTED };

  template <typename F>
  PartialResult detectClients(Link::_MultiBootParam& multiBootParameters,
                              F cancel) {
    // 2. Initiate a multiplayer communication session, using either Normal mode
    // for a single client or MultiPlay mode for multiple clients.
    start();

    // 3. Send the word 0x6200 repeatedly until all detected clients respond
    // with 0x720X, where X is their client number (1-3). If they fail to do
    // this after 16 tries, delay 1/16s and go back to step 2. (*)
    bool success = false;
    for (u32 t = 0; t < DETECTION_TRIES; t++) {
      auto response = transfer(HANDSHAKE, cancel);
      if (cancel())
        return ABORTED;

      multiBootParameters.client_bit = 0;

      success =
          validateResponse(response, [&multiBootParameters](u32 i, u16 value) {
            if ((value & 0xfff0) == HANDSHAKE_RESPONSE) {
              u8 clientId = value & 0xf;
              u8 expectedClientId = 1 << (i + 1);
              if (clientId == expectedClientId) {
                multiBootParameters.client_bit |= clientId;
                return true;
              }
            }
            return false;
          });

      if (success)
        break;
    }

    if (!success)
      return NEEDS_RETRY;

    // 4. Fill in client_bit in the multiboot parameter structure (with
    // bits 1-3 set according to which clients responded). Send the word
    // 0x610Y, where Y is that same set of set bits.
    auto response =
        transfer(CONFIRM_CLIENTS | multiBootParameters.client_bit, cancel);

    // The clients should respond 0x7200.
    if (!isResponseSameAsValueWithClientBit(
            response, multiBootParameters.client_bit, HANDSHAKE_RESPONSE))
      return NEEDS_RETRY;

    return FINISHED;
  }

  template <typename F>
  PartialResult sendHeader(Link::_MultiBootParam& multiBootParameters,
                           const u8* rom,
                           F cancel) {
    // 5. Send the cartridge header, 16 bits at a time, in little endian order.
    // After each 16-bit send, the clients will respond with 0xNN0X, where NN is
    // the number of words remaining and X is the client number. (Note that if
    // transferring in the single-client 32-bit mode, you still need to send
    // only 16 bits at a time).
    u16* headerOut = (u16*)rom;
    u32 remaining = HEADER_SIZE / 2;
    while (remaining > 0) {
      auto response = transfer(*(headerOut++), cancel);
      if (cancel())
        return ABORTED;

      if (!isResponseSameAsValueWithClientBit(
              response, multiBootParameters.client_bit, remaining << 8))
        return NEEDS_RETRY;

      remaining--;
    }

    // 6. Send 0x6200, followed by 0x620Y again.
    // The clients should respond 0x000Y and 0x720Y.
    Response response;
    response = transfer(HANDSHAKE, cancel);
    if (cancel())
      return ABORTED;
    if (!isResponseSameAsValueWithClientBit(response,
                                            multiBootParameters.client_bit, 0))
      return NEEDS_RETRY;
    response = transfer(HANDSHAKE | multiBootParameters.client_bit, cancel);
    if (cancel())
      return ABORTED;
    if (!isResponseSameAsValueWithClientBit(
            response, multiBootParameters.client_bit, HANDSHAKE_RESPONSE))
      return NEEDS_RETRY;

    return FINISHED;
  }

  template <typename F>
  PartialResult sendPalette(Link::_MultiBootParam& multiBootParameters,
                            F cancel) {
    // 7. Send 0x63PP repeatedly, where PP is the palette_data you have picked
    // earlier. Do this until the clients respond with 0x73CC, where CC is a
    // random byte. Store these bytes in client_data in the parameter structure.
    auto data = SEND_PALETTE | LINK_CABLE_MULTIBOOT_PALETTE_DATA;

    bool success = false;
    for (u32 i = 0; i < DETECTION_TRIES; i++) {
      auto response = transfer(data, cancel);
      if (cancel())
        return ABORTED;

      u8 sendMask = multiBootParameters.client_bit;
      success = validateResponse(
                    response,
                    [&multiBootParameters, &sendMask](u32 i, u16 value) {
                      u8 clientBit = 1 << (i + 1);
                      if ((multiBootParameters.client_bit & clientBit) &&
                          ((value & ACK_RESPONSE_MASK) == ACK_RESPONSE)) {
                        multiBootParameters.client_data[i] = value & 0xff;
                        sendMask &= ~clientBit;
                        return true;
                      }
                      return false;
                    }) &&
                sendMask == 0;

      if (success)
        break;
    }

    if (!success)
      return NEEDS_RETRY;

    return FINISHED;
  }

  template <typename F>
  PartialResult confirmHandshakeData(Link::_MultiBootParam& multiBootParameters,
                                     F cancel) {
    // 8. Calculate the handshake_data byte and store it in the parameter
    // structure. This should be calculated as 0x11 + the sum of the three
    // client_data bytes. Send 0x64HH, where HH is the handshake_data.
    // The clients should respond 0x77GG, where GG is something unimportant.
    multiBootParameters.handshake_data =
        (HANDSHAKE_DATA + multiBootParameters.client_data[0] +
         multiBootParameters.client_data[1] +
         multiBootParameters.client_data[2]) %
        256;

    u16 data = CONFIRM_HANDSHAKE_DATA | multiBootParameters.handshake_data;
    auto response = transfer(data, cancel);
    if (cancel())
      return ABORTED;
    if (!isResponseSameAsValue(response, multiBootParameters.client_bit,
                               ACK_RESPONSE, ACK_RESPONSE_MASK))
      return NEEDS_RETRY;

    return FINISHED;
  }

  static bool isResponseSameAsValue(Response response,
                                    u8 clientMask,
                                    u16 wantedValue,
                                    u16 mask) {
    return validateResponse(
        response, [&clientMask, &wantedValue, &mask](u32 i, u32 value) {
          u8 clientBit = 1 << (i + 1);
          bool isInvalid =
              (clientMask & clientBit) && ((value & mask) != wantedValue);
          return !isInvalid;
        });
  }

  static bool isResponseSameAsValueWithClientBit(Response response,
                                                 u8 clientMask,
                                                 u32 wantedValue) {
    return validateResponse(
        response, [&clientMask, &wantedValue](u32 i, u32 value) {
          u8 clientBit = 1 << (i + 1);
          bool isInvalid =
              (clientMask & clientBit) && (value != (wantedValue | clientBit));
          return !isInvalid;
        });
  }

  template <typename F>
  static bool validateResponse(Response response, F check) {
    u32 count = 0;
    for (u32 i = 0; i < CLIENTS; i++) {
      auto value = response.data[1 + i];
      if (value == LINK_RAW_CABLE_DISCONNECTED) {
        // Note that throughout this process, any clients that are not
        // connected will always respond with 0xFFFF - be sure to ignore them.
        continue;
      }

      if (!check(i, value))
        return false;
      count++;
    }

    return count > 0;
  }

  template <typename F>
  Response transfer(u32 data, F cancel) {
    if (_mode == TransferMode::MULTI_PLAY) {
      Response response;
      auto response16bit = linkRawCable.transfer(data, cancel);
      for (u32 i = 0; i < LINK_RAW_CABLE_MAX_PLAYERS; i++)
        response.data[i] = response16bit.data[i];
      response.playerId = response16bit.playerId;
      return response;
    } else {
      Response response = {
          .data = {LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED,
                   LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED}};
      response.data[1] = linkSPI.transfer(data, cancel) >> 16;
      response.playerId = 0;
      return response;
    }
  }

  void start() {
    if (_mode == TransferMode::MULTI_PLAY)
      linkRawCable.activate(MAX_BAUD_RATE);
    else
      linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);
  }

  void stop() {
    if (_mode == TransferMode::MULTI_PLAY)
      linkRawCable.deactivate();
    else
      linkSPI.deactivate();
  }

  Result error(Result error) {
    stop();
    return error;
  }

 public:
  /*
  class Async {
   public:
    enum State {
      STOPPED,
      WAITING,
      DETECTING_CLIENTS,
      DETECTING_CLIENTS_END,
      SENDING_HEADER
    };

    enum Result {
      NONE,
      SUCCESS,
      INVALID_SIZE,
      CANCELED,
      NO_INIT_SYNC,
      WRONG_ANSWER,
      HEADER_ISSUE,
      PALETTE_FAILURE,
      NOT_INIT,
      FINAL_HANDSHAKE_FAILURE,
      CRC_FAILURE,
      SEND_FAILURE
    };

    bool sendRom(const u8* rom,
                 u32 romSize,
                 TransferMode mode = TransferMode::MULTI_PLAY) {
      if (state != STOPPED)
        return false;

      if (romSize < MIN_ROM_SIZE || romSize > MAX_ROM_SIZE ||
          (romSize % 0x10) != 0)
        return INVALID_SIZE;

      resetState();
      initFixedData(rom, romSize);
      dynamicData.transferMode = mode;

      startMultibootSend();

      return true;
    }

    void _onVBlank() {
      if (state == STOPPED)
        return;

      processNewFrame();
    }

    void _onSerial() {
      if (state == STOPPED)
        return;

      Response response = getAsyncResponse();
      processResponse(response);
    }

   private:
    struct MultibootFixedData {
      const u16* data = nullptr;
      u32 size = 0;
      u32 crcCNormal = 0;
      u32 crcCMulti = 0;
      u8 crcCNormalInit = 0;
      u8 crcCMultiInit = 0;
    };

    struct MultibootDynamicData {
      TransferMode transferMode = TransferMode::MULTI_PLAY;
      u32 crcB = 0;
      u32 seed = 0;
      u8* tokenData = nullptr;
      u8 clientMask = 0;

      u32 waitFrames = 0;
      u32 wait = 0;
      u32 retry = 0;
    };

    LinkRawCable linkRawCable;
    LinkSPI linkSPI;
    MultibootFixedData fixedData;
    MultibootDynamicData dynamicData;
    volatile State state = STOPPED;
    volatile Result result = NONE;

    void processNewFrame() {
      switch (state) {
        case WAITING: {
          dynamicData.wait++;
          if (dynamicData.wait >= dynamicData.waitFrames) {
            state = DETECTING_CLIENTS;
            start();
            transferAsync(HANDSHAKE);
          }
          break;
        }
      }
    }

    void processResponse(Response response) {
      switch (state) {
        case DETECTING_CLIENTS: {
          dynamicData.clientMask = 0;

          bool success = validateResponse(response, [this](u32 i, u16 value) {
            if ((value & 0xfff0) == HANDSHAKE_RESPONSE) {
              auto clientId = value & 0xf;
              auto expectedClientId = 1 << (i + 1);
              if (clientId == expectedClientId) {
                dynamicData.clientMask |= clientId;
                return true;
              }
            }
            return false;
          });

          if (success) {
            state = DETECTING_CLIENTS_END;
            transferAsync(CONFIRM_CLIENTS | dynamicData.clientMask);
          } else {
            dynamicData.retry++;
            if (dynamicData.retry >= DETECTION_TRIES) {
              startMultibootSend();
              return;
            }

            transferAsync(HANDSHAKE);
          }

          break;
        }
        case DETECTING_CLIENTS_END: {
          if (!isResponseSameAsValueWithClientBit(
                  response, dynamicData.clientMask, HANDSHAKE_RESPONSE)) {
            startMultibootSend();
            return;
          }

          state = SENDING_HEADER;
          break;
        }
      }
    }

    void initFixedData(const u8* rom, u32 romSize) {
      const u16* start = (u16*)rom;
      const u16* end = (u16*)(rom + romSize);

      fixedData.data = start;
      fixedData.size = (u32)end - (u32)start;
    }

    void startMultibootSend() {
      state = WAITING;
      stop();

      dynamicData = MultibootDynamicData{};
      dynamicData.waitFrames = INITIAL_WAIT_MIN_FRAMES +
                               Link::_qran_range(1,
  INITIAL_WAIT_MAX_RANDOM_FRAMES);
    }

    void resetState() {
      state = STOPPED;
      result = NONE;
      fixedData = MultibootFixedData{};
      dynamicData = MultibootDynamicData{};
    }

    Response getAsyncResponse() {
      Response response = {
          .data = {LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED,
                   LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED}};

      if (dynamicData.transferMode == TransferMode::MULTI_PLAY) {
        linkRawCable._onSerial();
        auto response16bit = linkRawCable.getAsyncData();
        for (u32 i = 0; i < LINK_RAW_CABLE_MAX_PLAYERS; i++)
          response.data[i] = response16bit.data[i];
        response.playerId = response16bit.playerId;
      } else {
        linkSPI._onSerial();
        response.data[1] = linkSPI.getAsyncData() >> 16;
        response.playerId = 0;
      }

      return response;
    }

    void transferAsync(u32 data) {
      if (dynamicData.transferMode == TransferMode::MULTI_PLAY)
        linkRawCable.transferAsync(data);
      else
        linkSPI.transferAsync(data);
    }

    void start() {
      if (dynamicData.transferMode == TransferMode::MULTI_PLAY)
        linkRawCable.activate(MAX_BAUD_RATE);
      else
        linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);
    }

    void stop() {
      if (dynamicData.transferMode == TransferMode::MULTI_PLAY)
        linkRawCable.deactivate();
      else
        linkSPI.deactivate();
    }
  };
  */
};

extern LinkCableMultiboot* linkCableMultiboot;

#endif  // LINK_CABLE_MULTIBOOT_H
