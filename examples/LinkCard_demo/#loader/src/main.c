#include "def.h"
#include "erapi.h"
#include "link.h"
#include "protocol.h"

// Enable this to simulate failed scans with ⮜ and successful scans with ➤
#define DEBUG_MODE 0

// Enable this to display error codes
#define DISPLAY_ERROR_CODES 1

// Japanese strings are encoded as Shift-JIS byte arrays.
#ifdef REGION_JAP
#if DISPLAY_ERROR_CODES == 1
/* "０１２３４５６７８９" */
const u8 MSG_NUMBERS[] = {0x82, 0x4f, 0x82, 0x50, 0x82, 0x51, 0x82,
                          0x52, 0x82, 0x53, 0x82, 0x54, 0x82, 0x55,
                          0x82, 0x56, 0x82, 0x57, 0x82, 0x58, 0x00};
#endif

#ifdef LANGUAGE_ENG
const u8 MSG_WAITING_GAME[] = {
    0x82, 0x76, 0x82, 0x60, 0x82, 0x68, 0x82, 0x73, 0x82, 0x68, 0x82,
    0x6d, 0x82, 0x66, 0x81, 0x40, 0x82, 0x65, 0x82, 0x6e, 0x82, 0x71,
    0x81, 0x40, 0x82, 0x66, 0x82, 0x60, 0x82, 0x6c, 0x82, 0x64, 0x00};
const u8 MSG_SCAN_CARD[] = {
    0x82, 0x6f, 0x82, 0x6b, 0x82, 0x64, 0x82, 0x60, 0x82, 0x72, 0x82,
    0x64, 0x81, 0x40, 0x82, 0x72, 0x82, 0x62, 0x82, 0x60, 0x82, 0x6d,
    0x81, 0x40, 0x82, 0x78, 0x82, 0x6e, 0x82, 0x74, 0x82, 0x71, 0x81,
    0x40, 0x82, 0x62, 0x82, 0x60, 0x82, 0x71, 0x82, 0x63, 0x00};
const u8 MSG_TRANSFERRING[] = {0x82, 0x73, 0x82, 0x71, 0x82, 0x60, 0x82,
                               0x6d, 0x82, 0x72, 0x82, 0x65, 0x82, 0x64,
                               0x82, 0x71, 0x82, 0x71, 0x82, 0x68, 0x82,
                               0x6d, 0x82, 0x66, 0x00};
const u8 MSG_CARD_SENT[] = {0x82, 0x62, 0x82, 0x60, 0x82, 0x71, 0x82,
                            0x63, 0x81, 0x40, 0x82, 0x72, 0x82, 0x64,
                            0x82, 0x6d, 0x82, 0x73, 0x00};
const u8 MSG_ERROR[] = {0x82, 0x64, 0x82, 0x71, 0x82, 0x71,
                        0x82, 0x6e, 0x82, 0x71, 0x00};
const u8 MSG_PRESS_B_CANCEL[] = {
    0x82, 0x6f, 0x82, 0x71, 0x82, 0x64, 0x82, 0x72, 0x82, 0x72, 0x81, 0x40,
    0x82, 0x61, 0x81, 0x40, 0x82, 0x73, 0x82, 0x6e, 0x81, 0x40, 0x82, 0x62,
    0x82, 0x60, 0x82, 0x6d, 0x82, 0x62, 0x82, 0x64, 0x82, 0x6b, 0x00};
#else
const u8 MSG_WAITING_GAME[] = {0x83, 0x51, 0x81, 0x5b, 0x83, 0x80, 0x82,
                               0xf0, 0x91, 0xd2, 0x82, 0xc1, 0x82, 0xc4,
                               0x82, 0xa2, 0x82, 0xdc, 0x82, 0xb7, 0x00};
const u8 MSG_SCAN_CARD[] = {0x83, 0x4a, 0x81, 0x5b, 0x83, 0x68, 0x82, 0xf0,
                            0x83, 0x58, 0x83, 0x4c, 0x83, 0x83, 0x83, 0x93,
                            0x82, 0xb5, 0x82, 0xc4, 0x82, 0xad, 0x82, 0xbe,
                            0x82, 0xb3, 0x82, 0xa2, 0x00};
const u8 MSG_TRANSFERRING[] = {0x93, 0x5d, 0x91, 0x97, 0x92, 0x86, 0x00};
const u8 MSG_CARD_SENT[] = {0x83, 0x4a, 0x81, 0x5b, 0x83, 0x68, 0x91, 0x97,
                            0x90, 0x4d, 0x8d, 0xcf, 0x82, 0xdd, 0x00};
const u8 MSG_ERROR[] = {0x83, 0x47, 0x83, 0x89, 0x81, 0x5b, 0x00};
const u8 MSG_PRESS_B_CANCEL[] = {0x83, 0x72, 0x81, 0x5b, 0x82, 0xf0, 0x89, 0x9f,
                                 0x82, 0xb5, 0x82, 0xc4, 0x83, 0x4c, 0x83, 0x83,
                                 0x83, 0x93, 0x83, 0x5a, 0x83, 0x8b, 0x00};
#endif
#else
const char* MSG_WAITING_GAME = "Waiting for game...";
const char* MSG_SCAN_CARD = "Scan a card!";
const char* MSG_TRANSFERRING = "Transferring...";
const char* MSG_CARD_SENT = "Card sent!";
const char* MSG_ERROR = "Error!";
const char* MSG_PRESS_B_CANCEL = "Press B to cancel";
#endif

#define CARD_BUFFER_SIZE 2100
#define SCAN_SUCCESS 6
#define POST_TRANSFER_WAIT 60

extern int __end[];

ERAPI_HANDLE_REGION region;
u8 card[CARD_BUFFER_SIZE];
const u16 palette[] = {0x0000, 0xFFFF};
u32 previousKeys = 0;

#if DISPLAY_ERROR_CODES == 1
void codeToString(char* buf, int num) {
#ifdef REGION_JAP
  const u8* digits = MSG_NUMBERS;
  int temp[5];
  int len = 0;
  do {
    temp[len++] = num % 10;
    num /= 10;
  } while (num && len < 5);

  u8* p = (u8*)buf;
  for (int i = len - 1; i >= 0; --i) {
    int idx = temp[i] * 2;
    *p++ = digits[idx];
    *p++ = digits[idx + 1];
  }
  *p = 0;
#else
  char temp[6];
  int pos = 0;
  do {
    temp[pos++] = '0' + (num % 10);
    num /= 10;
  } while (num && pos < 5);
  int j = 0;
  while (pos)
    buf[j++] = temp[--pos];
  buf[j] = '\0';
#endif
}
#endif

void print(const char* text, bool canCancel);
bool cancel();
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
    u32 errorCode = 0;

    // init loader
    reset();

    // "Waiting for game..."
    print(MSG_WAITING_GAME, false);

    // handshake with game
    if (!sendAndExpect(HANDSHAKE_1, HANDSHAKE_1, cancel))
      continue;
    if (!sendAndExpect(HANDSHAKE_2, HANDSHAKE_2, cancel))
      continue;
    if (!sendAndExpect(HANDSHAKE_3, HANDSHAKE_3, cancel))
      continue;

    // wait for card request
    u16 cardRequest = sendAndReceiveExcept(HANDSHAKE_3, HANDSHAKE_3, cancel);
    if (cardRequest != GAME_REQUEST) {
      errorCode = 1;
      goto error;
    }

    // confirm card request
    if (!sendAndExpect(GAME_ANIMATING, EREADER_ANIMATING, cancel))
      continue;
    if (!send(EREADER_ANIMATING, cancel))
      continue;

    // scan card
    if (!sendAndExpect(EREADER_READY, GAME_READY, cancel)) {
      errorCode = 2;
      goto error;
    }

    // "Scan a card!"
    print(MSG_SCAN_CARD, false);

#if DEBUG_MODE == 1
    u32 resultCode = 0;
    while (true) {
      u32 debugKeys = ERAPI_GetKeyStateRaw();
      if ((debugKeys & ERAPI_KEY_LEFT) != 0) {
        resultCode = SCAN_SUCCESS - 1;
        break;
      }
      if ((debugKeys & ERAPI_KEY_RIGHT) != 0) {
        resultCode = SCAN_SUCCESS;
        const char msg[] = "HelloWorld";
        const u32 msgLen = sizeof(msg) - 1;
        const u32 byteCount = CARD_BUFFER_SIZE - CARD_OFFSET;
        for (u32 i = 0; i < byteCount; i++)
          card[CARD_OFFSET + i] = i == byteCount - 1 ? '!' : msg[i % msgLen];
        break;
      }
    }
#else
    u32 resultCode = ERAPI_ScanDotCode((u32)card);
#endif

    if (resultCode != SCAN_SUCCESS) {
      errorCode = 3;
      goto error;
    }

    // "Transferring..."
    print(MSG_TRANSFERRING, true);

    // transfer start
    if (!sendAndExpect(EREADER_SEND_READY, GAME_RECEIVE_READY, cancel)) {
      errorCode = 4;
      goto error;
    }
    if (!send(EREADER_SEND_START, cancel)) {
      errorCode = 5;
      goto error;
    }

    // transfer
    u32 checksum = 0;
    for (u32 o = CARD_OFFSET; o < CARD_SIZE; o += 2) {
      u16 block = *(u16*)(card + o);
      if (!send(block, cancel)) {
        errorCode = 6;
        goto error;
      }
      checksum += block;
    }
    if (!send(checksum & 0xffff, cancel)) {
      errorCode = 7;
      goto error;
    }
    if (!send(checksum >> 16, cancel)) {
      errorCode = 8;
      goto error;
    }
    if (!send(EREADER_SEND_END, cancel)) {
      errorCode = 9;
      goto error;
    }

    // "Card sent!"
    print(MSG_CARD_SENT, false);
    for (u32 i = 0; i < POST_TRANSFER_WAIT; i++)
      ERAPI_RenderFrame(1);

    continue;

  error:
    // "Error!"
    ERAPI_ClearRegion(region);
    ERAPI_DrawText(region, 0, 0, MSG_ERROR);
#if DISPLAY_ERROR_CODES == 1
    char errorCodeStr[11];
    codeToString(errorCodeStr, errorCode);
    ERAPI_DrawText(region, 0, 16, errorCodeStr);
#else
    ERAPI_DrawText(region, 0, 16, MSG_WAITING_GAME);
#endif
    ERAPI_RenderFrame(1);

    send(EREADER_CANCEL, cancel);
    send(EREADER_CANCEL, cancel);
    send(EREADER_ANIMATING, cancel);
    send(EREADER_SIO_END, cancel);
  }

  setGeneralPurposeMode();

  // exit
  return ERAPI_EXIT_TO_MENU;
}

void print(const char* text, bool canCancel) {
  ERAPI_ClearRegion(region);
  ERAPI_DrawText(region, 0, 0, text);
  if (canCancel)
    ERAPI_DrawText(region, 0, 16, MSG_PRESS_B_CANCEL);
  ERAPI_RenderFrame(1);
}

bool cancel() {
  u32 keys = ERAPI_GetKeyStateRaw();
  bool isPressed =
      (previousKeys & ERAPI_KEY_B) == 0 && (keys & ERAPI_KEY_B) != 0;
  previousKeys = keys;
  return isPressed;
}

void reset() {
  setGeneralPurposeMode();
  setMultiPlayMode(3);  // 3 = 115200 bps

  for (u32 i = 0; i < CARD_BUFFER_SIZE; i++)
    ((vu8*)card)[i] = 0;
}
