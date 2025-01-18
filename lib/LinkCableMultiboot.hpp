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

// --- Borrowed from Lorenzoone's gba_mem_viewer. ---
#define MULTIBOOT_INIT_VALID_VALUE 0x91

#define FPS 60
#define MAX_FINAL_HANDSHAKE_ATTEMPTS (FPS * 5)
#define MAX_PALETTE_ATTEMPTS 128
#define MULTIBOOT_CHOSEN_PALETTE_VALUE 0x81

// In the end, it's not needed...?!
#define MULTIBOOT_WAIT_TIME_MUS (36 - 36)

#define CRCC_NORMAL_START 0xC387
#define CRCC_MULTI_START 0xFFF8

#define CRCC_NORMAL_XOR 0xC37B
#define CRCC_MULTI_XOR 0xA517

#define DATA_NORMAL_XOR 0x43202F2F
#define DATA_MULTI_XOR 0x6465646F

#define MULTIBOOT_MAX_SIZE 0x3FF40  // sin 0xC0
#define MAX_NUM_SLAVES 3

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

  // lol
  static inline void VBlankIntrWait() {
    while (Link::_REG_VCOUNT >= 160)
      ;  // wait till VDraw
    while (Link::_REG_VCOUNT < 160)
      ;  // wait till VBlank
  };

  static inline void calc_crc_data_u32(u32 read_data,
                                       u32* crcC_ptr,
                                       u8 is_normal) {
    u32 tmp_crcC = *crcC_ptr;
    u32 xor_val = CRCC_NORMAL_XOR;
    if (!is_normal)
      xor_val = CRCC_MULTI_XOR;
    for (int j = 0; j < 32; j++) {
      u8 bit = (tmp_crcC ^ read_data) & 1;
      read_data >>= 1;
      tmp_crcC >>= 1;
      if (bit)
        tmp_crcC ^= xor_val;
    }
    *crcC_ptr = tmp_crcC;
  }

  struct multiboot_fixed_data {
    u16* data;
    u32 size;
    u32 crcC_normal;
    u32 crcC_multi;
    u8 crcC_normal_init;
    u8 crcC_multi_init;
    u8 init;
  };

  struct multiboot_dynamic_data {
    u8 is_normal;
    u32 crcB;
    u32 seed;
    u8* token_data;
    u8 client_mask;
  };

  enum MULTIBOOT_RESULTS {
    MB_SUCCESS,
    MB_NO_INIT_SYNC,
    MB_WRONG_ANSWER,
    MB_HEADER_ISSUE,
    MB_PALETTE_FAILURE,
    MB_SWI_FAILURE,
    MB_NOT_INIT,
    MB_TOO_BIG,
    MB_FINAL_HANDSHAKE_FAILURE,
    MB_CRC_FAILURE,
    MB_SEND_FAILURE
  };

  static MULTIBOOT_RESULTS multiboot_init(u16* data,
                                          u16* end,
                                          multiboot_fixed_data* mb_data) {
    mb_data->init = 0;
    mb_data->data = data;
    mb_data->size = (((((u32)end) + 15) >> 4) << 4) - ((u32)data);
    if (mb_data->size > MULTIBOOT_MAX_SIZE)
      return MB_TOO_BIG;

    mb_data->crcC_normal_init = 0;
    mb_data->crcC_multi_init = 0;
    mb_data->init = MULTIBOOT_INIT_VALID_VALUE;
    return MB_SUCCESS;
  }

  // esto ya lo tengo creo
  static inline u8 received_data_same_as_value(u8 client_mask,
                                               u16 wanted_value,
                                               u16 mask,
                                               u16* response) {
    for (int i = 0; i < MAX_NUM_SLAVES; i++) {
      u8 client_bit = 1 << (i + 1);

      if ((client_mask & client_bit) && ((response[i] & mask) != wanted_value))
        return 0;  // bad
    }
    return 1;  // good
  }

  static inline u8 received_data_same_as_value_client_bit(u8 client_mask,
                                                          u16 wanted_value,
                                                          u16* response) {
    for (int i = 0; i < MAX_NUM_SLAVES; i++) {
      u8 client_bit = 1 << (i + 1);

      if ((client_mask & client_bit) &&
          (response[i] != (wanted_value | client_bit)))
        return 0;
    }
    return 1;
  }

  void multiboot_send(u32 data, bool isNormal, u16* recvData) {
    Response response = transfer(data, []() { return false; });
    for (u32 i = 0; i < MAX_NUM_SLAVES; i++)
      recvData[i] = response.data[i + 1];
  }

  MULTIBOOT_RESULTS multiboot_main_transfer(
      multiboot_fixed_data* mb_data,
      multiboot_dynamic_data* mb_dyn_data) {
    u16 recv_data[MAX_NUM_SLAVES];
    u32* u32_data = (u32*)mb_data->data;
    Link::wait(228 * 4);
    // delay_cycles(CLOCK_CYCLES_PER_MS((1000 + 15) / 16));

    multiboot_send((mb_data->size - 0x190) >> 2, mb_dyn_data->is_normal,
                   recv_data);
    for (int i = 0; i < MAX_NUM_SLAVES; i++) {
      u8 contribute = 0xFF;
      u8 client_bit = 1 << (i + 1);

      if (mb_dyn_data->client_mask & client_bit)
        contribute = recv_data[i] & 0xFF;
      mb_dyn_data->crcB |= contribute << (8 * (i + 1));
    }

    if (mb_dyn_data->is_normal) {
      if (!mb_data->crcC_normal_init)
        mb_data->crcC_normal = CRCC_NORMAL_START;
      for (u32 i = 0xC0 >> 2; i < (mb_data->size >> 2); i++) {
        mb_dyn_data->seed = (mb_dyn_data->seed * 0x6F646573) + 1;
        multiboot_send(u32_data[i] ^ (0xFE000000 - (i << 2)) ^
                           mb_dyn_data->seed ^ DATA_NORMAL_XOR,
                       mb_dyn_data->is_normal, recv_data);
        if (!received_data_same_as_value(mb_dyn_data->client_mask, i << 2,
                                         0xFFFF, recv_data))
          return MB_SEND_FAILURE;
        if (!mb_data->crcC_normal_init)
          calc_crc_data_u32(u32_data[i], &mb_data->crcC_normal,
                            mb_dyn_data->is_normal);
      }
      mb_data->crcC_normal_init = 1;
    } else {
      if (!mb_data->crcC_multi_init)
        mb_data->crcC_multi = CRCC_MULTI_START;
      for (u32 i = 0xC0 >> 2; i < (mb_data->size >> 2); i++) {
        mb_dyn_data->seed = (mb_dyn_data->seed * 0x6F646573) + 1;
        multiboot_send((u32_data[i] ^ (0xFE000000 - (i << 2)) ^
                        mb_dyn_data->seed ^ DATA_MULTI_XOR) &
                           0xFFFF,
                       mb_dyn_data->is_normal, recv_data);
        if (!received_data_same_as_value(mb_dyn_data->client_mask, i << 2,
                                         0xFFFF, recv_data))
          return MB_SEND_FAILURE;
        multiboot_send((u32_data[i] ^ (0xFE000000 - (i << 2)) ^
                        mb_dyn_data->seed ^ DATA_MULTI_XOR) >>
                           16,
                       mb_dyn_data->is_normal, recv_data);
        if (!received_data_same_as_value(mb_dyn_data->client_mask, (i << 2) + 2,
                                         0xFFFF, recv_data))
          return MB_SEND_FAILURE;
        if (!mb_data->crcC_multi_init)
          calc_crc_data_u32(u32_data[i], &mb_data->crcC_multi,
                            mb_dyn_data->is_normal);
      }
      mb_data->crcC_multi_init = 1;
    }

    multiboot_send(0x0065, mb_dyn_data->is_normal, recv_data);

    return MB_SUCCESS;
  }

  enum MULTIBOOT_RESULTS multiboot_normal(u16* data,
                                          u16* end,
                                          multiboot_fixed_data* mb_data,
                                          int is_normal) {
    u16 response[MAX_NUM_SLAVES];
    int attempts, sends, halves;
    u8 answers[MAX_NUM_SLAVES] = {0xFF, 0xFF, 0xFF};
    u8 handshake;
    u8 sendMask;
    u32 attempt_counter;
    const u8 palette = MULTIBOOT_CHOSEN_PALETTE_VALUE;
    struct multiboot_dynamic_data mb_dyn_data;
    mb_dyn_data.is_normal = is_normal;
    mb_dyn_data.client_mask = 0;

    if (mb_data->init != MULTIBOOT_INIT_VALID_VALUE)
      multiboot_init(data, end, mb_data);

    start();
    // if (mb_dyn_data.is_normal)
    //   init_sio_normal(SIO_MASTER, SIO_32);
    // else
    //   init_sio_multi(SIO_MASTER);

    for (attempts = 0; attempts < 128; attempts++) {
      for (sends = 0; sends < 16; sends++) {
        multiboot_send(0x6200, mb_dyn_data.is_normal, response);

        for (int i = 0; i < MAX_NUM_SLAVES; i++)
          if ((response[i] & 0xFFF0) == 0x7200) {
            mb_dyn_data.client_mask |= response[i] & 0xF;
          }
      }

      if (mb_dyn_data.client_mask)
        break;
      else
        VBlankIntrWait();
    }

    if (!mb_dyn_data.client_mask) {
      return MB_NO_INIT_SYNC;
    }

    multiboot_send(0x6100 | mb_dyn_data.client_mask, mb_dyn_data.is_normal,
                   response);
    if (!received_data_same_as_value_client_bit(mb_dyn_data.client_mask, 0x7200,
                                                response))
      return MB_WRONG_ANSWER;

    for (halves = 0; halves < 0x60; ++halves) {
      multiboot_send(mb_data->data[halves], mb_dyn_data.is_normal, response);
      if (!received_data_same_as_value_client_bit(
              mb_dyn_data.client_mask, (0x60 - halves) << 8, response))
        return MB_HEADER_ISSUE;
    }

    multiboot_send(0x6200, mb_dyn_data.is_normal, response);
    if (!received_data_same_as_value_client_bit(mb_dyn_data.client_mask, 0,
                                                response))
      return MB_WRONG_ANSWER;

    multiboot_send(0x6200 | mb_dyn_data.client_mask, mb_dyn_data.is_normal,
                   response);
    if (!received_data_same_as_value_client_bit(mb_dyn_data.client_mask, 0x7200,
                                                response))
      return MB_WRONG_ANSWER;

    sendMask = mb_dyn_data.client_mask;
    attempt_counter = 0;

    while (sendMask) {
      multiboot_send(0x6300 | palette, mb_dyn_data.is_normal, response);

      for (int i = 0; i < MAX_NUM_SLAVES; i++) {
        u8 client_bit = 1 << (i + 1);

        if ((mb_dyn_data.client_mask & client_bit) &&
            ((response[i] & 0xFF00) == 0x7300)) {
          answers[i] = response[i] & 0xFF;
          sendMask &= ~client_bit;
        }
      }
      attempt_counter++;

      if ((attempt_counter == MAX_PALETTE_ATTEMPTS) && sendMask)
        return MB_PALETTE_FAILURE;
    }

    mb_dyn_data.seed = palette;
    handshake = 0x11;
    for (int i = 0; i < MAX_NUM_SLAVES; i++) {
      handshake += answers[i];
      mb_dyn_data.seed |= answers[i] << (8 * (i + 1));
    }

    handshake &= 0xFF;
    multiboot_send(0x6400 | handshake, mb_dyn_data.is_normal, response);
    if (!received_data_same_as_value(mb_dyn_data.client_mask, 0x7300, 0xFF00,
                                     response))
      return MB_WRONG_ANSWER;
    mb_dyn_data.crcB = handshake;

    // print_multiboot_mid_process(1);
    // prepare_flush();
    VBlankIntrWait();

    // This is slower before caching the fixed crcC, but it works even in
    // vram/ovram...
    enum MULTIBOOT_RESULTS result =
        multiboot_main_transfer(mb_data, &mb_dyn_data);
    if (result != MB_SUCCESS)
      return result;

    u32 crcC_final = mb_data->crcC_normal & 0xFFFF;
    if (!mb_dyn_data.is_normal)
      crcC_final = mb_data->crcC_multi & 0xFFFF;
    calc_crc_data_u32(mb_dyn_data.crcB, &crcC_final, mb_dyn_data.is_normal);

    attempt_counter = 0;
    u8 done = 0;

    while (!done) {
      multiboot_send(0x0065, mb_dyn_data.is_normal, response);
      done = 1;
      if (!received_data_same_as_value(mb_dyn_data.client_mask, 0x0075, 0xFFFF,
                                       response))
        done = 0;
      VBlankIntrWait();
      attempt_counter += 1;

      if (attempt_counter == MAX_FINAL_HANDSHAKE_ATTEMPTS)
        return MB_FINAL_HANDSHAKE_FAILURE;
    }
    multiboot_send(0x0066, mb_dyn_data.is_normal, response);
    multiboot_send(crcC_final & 0xFFFF, mb_dyn_data.is_normal, response);

    if (!received_data_same_as_value(mb_dyn_data.client_mask, crcC_final,
                                     0xFFFF, response))
      return MB_CRC_FAILURE;

    return MB_SUCCESS;
  }

  // --- Lorenzoone ---

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
    if (romSize < MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > MAX_ROM_SIZE)
      return INVALID_SIZE;
    if ((romSize % 0x10) != 0)
      return INVALID_SIZE;

    // sio_stop_irq_slave();
    // irqDisable(IRQ_SERIAL);
    multiboot_fixed_data mb_data;
    u8* romEnd = (u8*)rom + romSize;
    auto res = multiboot_normal((u16*)rom, (u16*)romEnd, &mb_data,
                                mode == TransferMode::SPI);
    return res == MULTIBOOT_RESULTS::MB_SUCCESS
               ? Result::SUCCESS
               : Result::FAILURE_DURING_TRANSFER;

  retry:
    stop();

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
