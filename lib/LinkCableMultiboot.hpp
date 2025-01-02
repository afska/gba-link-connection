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

static volatile char LINK_CABLE_MULTIBOOT_VERSION[] =
    "LinkCableMultiboot/v7.1.0";

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
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int MIN_ROM_SIZE = 0x100 + 0xc0;
  static constexpr int MAX_ROM_SIZE = 256 * 1024;
  static constexpr int FRAME_LINES = 228;
  static constexpr int WAIT_BEFORE_RETRY = FRAME_LINES * 4;
  static constexpr int DETECTION_TRIES = 16;
  static constexpr int CLIENTS = 3;
  static constexpr int CLIENT_NO_DATA = 0xff;
  static constexpr int HANDSHAKE = 0x6200;
  static constexpr int HANDSHAKE_RESPONSE = 0x7200;
  static constexpr int CONFIRM_CLIENTS = 0x6100;
  static constexpr int SEND_PALETTE = 0x6300;
  static constexpr int HANDSHAKE_DATA = 0x11;
  static constexpr int CONFIRM_HANDSHAKE_DATA = 0x6400;
  static constexpr int ACK_RESPONSE = 0x73;
  static constexpr int HEADER_SIZE = 0xC0;
  static constexpr auto MAX_BAUD_RATE = LinkRawCable::BaudRate::BAUD_RATE_3;
  static constexpr u8 CLIENT_IDS[] = {0b0010, 0b0100, 0b1000};

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
   * @param rom A pointer to ROM data.
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
    this->_mode = mode;
    if (romSize < MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > MAX_ROM_SIZE)
      return INVALID_SIZE;
    if ((romSize % 0x10) != 0)
      return INVALID_SIZE;

  retry:
    deactivate();

    // (*) instead of 1/16s, waiting a random number of frames works better
    Link::wait(WAIT_BEFORE_RETRY + FRAME_LINES * _qran_range(1, 30));

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

    deactivate();

    // 10. Upon return, r0 will be either 0 for success, or 1 for failure. If
    // successful, all clients have received the multiboot program successfully
    // and are now executing it - you can begin either further data transfer or
    // a multiplayer game from here.
    return result == 1 ? FAILURE_DURING_TRANSFER : SUCCESS;
  }

  ~LinkCableMultiboot() {
    delete linkRawCable;
    delete linkSPI;
  }

 private:
  LinkRawCable* linkRawCable = new LinkRawCable();
  LinkSPI* linkSPI = new LinkSPI();
  TransferMode _mode;
  int randomSeed = 123;

  enum PartialResult { NEEDS_RETRY, FINISHED, ABORTED };

  struct Responses {
    u16 d[CLIENTS];
  };

  template <typename F>
  PartialResult detectClients(Link::_MultiBootParam& multiBootParameters,
                              F cancel) {
    // 2. Initiate a multiplayer communication session, using either Normal mode
    // for a single client or MultiPlay mode for multiple clients.
    activate();

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
              auto clientId = value & 0xf;
              if (clientId == CLIENT_IDS[i]) {
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
    transfer(CONFIRM_CLIENTS | multiBootParameters.client_bit, cancel);

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

      bool success = validateResponse(response, [&remaining](u32 i, u16 value) {
        u8 clientId = CLIENT_IDS[i];
        u16 expectedValue = (remaining << 8) | clientId;
        return value == expectedValue;
      });

      if (!success)
        return NEEDS_RETRY;

      remaining--;
    }

    // 6. Send 0x6200, followed by 0x620Y again.
    transfer(HANDSHAKE, cancel);
    if (cancel())
      return ABORTED;
    transfer(HANDSHAKE | multiBootParameters.client_bit, cancel);
    if (cancel())
      return ABORTED;

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

      success =
          validateResponse(response, [&multiBootParameters](u32 i, u16 value) {
            if ((value >> 8) == ACK_RESPONSE) {
              multiBootParameters.client_data[i] = value & 0xff;
              return true;
            }
            return false;
          });

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
    multiBootParameters.handshake_data =
        (HANDSHAKE_DATA + multiBootParameters.client_data[0] +
         multiBootParameters.client_data[1] +
         multiBootParameters.client_data[2]) %
        256;

    u16 data = CONFIRM_HANDSHAKE_DATA | multiBootParameters.handshake_data;
    auto response = transfer(data, cancel);
    if (cancel())
      return ABORTED;

    return (response.data[1] >> 8) == ACK_RESPONSE ? FINISHED : NEEDS_RETRY;
  }

  template <typename F>
  bool validateResponse(Response response, F check) {
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
      auto response16bit = linkRawCable->transfer(data, cancel);
      for (u32 i = 0; i < LINK_RAW_CABLE_MAX_PLAYERS; i++)
        response.data[i] = response16bit.data[i];
      response.playerId = response16bit.playerId;
      return response;
    } else {
      Response response = {
          .data = {LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED,
                   LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED}};
      response.data[1] = linkSPI->transfer(data, cancel) >> 16;
      response.playerId = 0;
      return response;
    }
  }

 public:
  void activate() {
    if (_mode == TransferMode::MULTI_PLAY)
      linkRawCable->activate(MAX_BAUD_RATE);
    else
      linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);
  }

  void deactivate() {
    if (_mode == TransferMode::MULTI_PLAY)
      linkRawCable->deactivate();
    else
      linkSPI->deactivate();
  }

  Result error(Result error) {
    deactivate();
    return error;
  }

  int _qran() {
    randomSeed = 1664525 * randomSeed + 1013904223;
    return (randomSeed >> 16) & 0x7FFF;
  }

  int _qran_range(int min, int max) {
    return (_qran() * (max - min) >> 15) + min;
  }
};

extern LinkCableMultiboot* linkCableMultiboot;

#endif  // LINK_CABLE_MULTIBOOT_H
