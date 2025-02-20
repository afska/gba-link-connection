#include "def.h"
#include "erapi.h"
#include "link.h"
#include "protocol.h"

// Japanese strings are encoded as Shift-JIS byte arrays.

#ifdef LANGUAGE_JAP
/* "ゲームを待っています" */
const u8 MSG_WAITING_GAME[] = {0x83, 0x51, 0x81, 0x5b, 0x83, 0x80, 0x82,
                               0xf0, 0x91, 0xd2, 0x82, 0xc1, 0x82, 0xc4,
                               0x82, 0xa2, 0x82, 0xdc, 0x82, 0xb7, 0x00};

/* "カードをスキャンしてください" */
const u8 MSG_SCAN_CARD[] = {0x83, 0x4a, 0x81, 0x5b, 0x83, 0x68, 0x82, 0xf0,
                            0x83, 0x58, 0x83, 0x4c, 0x83, 0x83, 0x83, 0x93,
                            0x82, 0xb5, 0x82, 0xc4, 0x82, 0xad, 0x82, 0xbe,
                            0x82, 0xb3, 0x82, 0xa2, 0x00};

/* "転送中" */
const u8 MSG_TRANSFERRING[] = {0x93, 0x5d, 0x91, 0x97, 0x92, 0x86, 0x00};

/* "カード送信済み" */
const u8 MSG_CARD_SENT[] = {0x83, 0x4a, 0x81, 0x5b, 0x83, 0x68, 0x91, 0x97,
                            0x90, 0x4d, 0x8d, 0xcf, 0x82, 0xdd, 0x00};

/* "エラー" */
const u8 MSG_ERROR[] = {0x83, 0x47, 0x83, 0x89, 0x81, 0x5b, 0x00};

/* "エーを押して再試行" */
const u8 MSG_PRESS_A_TRY_AGAIN[] = {0x83, 0x47, 0x81, 0x5b, 0x82, 0xf0, 0x89,
                                    0x9f, 0x82, 0xb5, 0x82, 0xc4, 0x8d, 0xc4,
                                    0x8e, 0x8e, 0x8d, 0x73, 0x00};

/* "ビーを押してキャンセル" */
const u8 MSG_PRESS_B_CANCEL[] = {0x83, 0x72, 0x81, 0x5b, 0x82, 0xf0, 0x89, 0x9f,
                                 0x82, 0xb5, 0x82, 0xc4, 0x83, 0x4c, 0x83, 0x83,
                                 0x83, 0x93, 0x83, 0x5a, 0x83, 0x8b, 0x00};
#else
const char* MSG_WAITING_GAME = "Waiting for game...";
const char* MSG_SCAN_CARD = "Scan a card!";
const char* MSG_TRANSFERRING = "Transferring...";
const char* MSG_CARD_SENT = "Card sent!";
const char* MSG_ERROR = "Error!";
const char* MSG_PRESS_A_TRY_AGAIN = "Press A to try again";
const char* MSG_PRESS_B_CANCEL = "Press B to cancel";
#endif

#define CARD_BUFFER_SIZE 2100
#define SCAN_SUCCESS 6
#define POST_TRANSFER_WAIT 60

extern int __end[];

ERAPI_HANDLE_REGION region;
u8 card[CARD_BUFFER_SIZE];
const u16 palette[] = {0x0000, 0xFFFF};

void print(const char* text);
bool cancel();
bool tryAgain();
void reset();

int main() {
  // init
  ERAPI_FadeIn(1);
  ERAPI_InitMemory((ERAPI_RAM_END - (u32)__end) >> 10);
  ERAPI_SetBackgroundMode(0);

  // palette
  ERAPI_SetBackgroundPalette(&palette[0], 0x00, 0x02);

  // region & text
  region = ERAPI_CreateRegion(0, 0, 1, 1, 28, 10);
  ERAPI_SetTextColor(region, 0x01, 0x00);

  // background
  ERAPI_LoadBackgroundSystem(3, 20);

  // loop
  while (1) {
    if (cancel())
      break;

    // init loader
    reset();

    // "Waiting for game..."
    print(MSG_WAITING_GAME);

    // handshake with game
    if (!sendAndExpect(HANDSHAKE_1, HANDSHAKE_1, cancel))
      continue;
    if (!sendAndExpect(HANDSHAKE_2, HANDSHAKE_2, cancel))
      continue;
    if (!sendAndExpect(HANDSHAKE_3, HANDSHAKE_3, cancel))
      continue;

    // wait for card request
    u16 cardRequest = sendAndReceiveExcept(HANDSHAKE_3, HANDSHAKE_3, cancel);
    if (cardRequest != GAME_REQUEST)
      goto error;

    // confirm card request
    if (!sendAndExpect(GAME_ANIMATING, EREADER_ANIMATING, cancel))
      continue;
    if (!send(EREADER_ANIMATING, cancel))
      continue;

    // "Scan a card!"
    print(MSG_SCAN_CARD);

    // scan card
    while (true) {
      if (cancel())
        goto abort;
      if (!sendAndExpect(EREADER_READY, GAME_READY, cancel))
        goto abort;
      u32 resultCode = ERAPI_ScanDotCode((u32)card);
      if (resultCode == SCAN_SUCCESS) {
        break;
      } else
        goto error;
    }

    // "Transferring..."
    print(MSG_TRANSFERRING);

    // transfer start
    if (!sendAndExpect(EREADER_SEND_READY, GAME_RECEIVE_READY, cancel))
      goto error;
    if (!send(EREADER_SEND_START, cancel))
      goto error;

    // transfer
    u32 checksum = 0;
    for (u32 o = CARD_OFFSET; o < CARD_SIZE; o += 2) {
      u16 block = *(u16*)(card + o);
      if (!send(block, cancel))
        goto error;
      checksum += block;
    }
    if (!send(checksum & 0xffff, cancel))
      goto error;
    if (!send(checksum >> 16, cancel))
      goto error;
    if (!send(EREADER_SEND_END, cancel))
      goto error;

    // "Card sent!"
    print(MSG_CARD_SENT);
    for (u32 i = 0; i < POST_TRANSFER_WAIT; i++)
      ERAPI_RenderFrame(1);

    continue;

  abort:
    send(EREADER_CANCEL, cancel);
    send(EREADER_ANIMATING, cancel);
    send(EREADER_SIO_END, cancel);
    continue;

  error:
    // "Error!"
    ERAPI_ClearRegion(region);
    ERAPI_DrawText(region, 0, 0, MSG_ERROR);
    ERAPI_DrawText(region, 0, 16, MSG_PRESS_A_TRY_AGAIN);
    ERAPI_RenderFrame(1);

    while (!tryAgain())
      ERAPI_RenderFrame(1);

    send(EREADER_CANCEL, cancel);
    send(EREADER_ANIMATING, cancel);
    send(EREADER_SIO_END, cancel);
  }

  setGeneralPurposeMode();

  // exit
  return ERAPI_EXIT_TO_MENU;
}

void print(const char* text) {
  ERAPI_ClearRegion(region);
  ERAPI_DrawText(region, 0, 0, text);
  ERAPI_DrawText(region, 0, 16, MSG_PRESS_B_CANCEL);
  ERAPI_RenderFrame(1);
}

bool cancel() {
  u32 keys = ERAPI_GetKeyStateRaw();
  return (keys & ERAPI_KEY_B) != 0;
}

bool tryAgain() {
  u32 keys = ERAPI_GetKeyStateRaw();
  return (keys & ERAPI_KEY_A) != 0;
}

void reset() {
  setGeneralPurposeMode();
  setMultiPlayMode(3);  // 3 = 115200 bps

  for (u32 i = 0; i < CARD_BUFFER_SIZE; i++)
    ((vu8*)card)[i] = 0;
}
