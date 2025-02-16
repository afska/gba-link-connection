#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "def.h"

// This protocol is only meant to be used by homebrew games using
// gba-link-connection's LinkCard library and its loader. It's highly inspired
// by `4-e`, but not compatible with it since SMA4 sends the cards from offset
// `0x72`, and this sends them from offset `0x4E` (no-individual-titles format).
// That said, you could make it compatible by changing `CARD_OFFSET` and
// `CARD_SIZE` to the commented values.

#define CARD_OFFSET /*0x72*/ 0x4E
#define CARD_SIZE /*2112*/ 2076

#define HANDSHAKE_1 0xfbfb
#define HANDSHAKE_2 0x5841
#define HANDSHAKE_3 0x4534

#define GAME_REQUEST 0xecec

#define GAME_ANIMATING 0xf3f3
#define EREADER_ANIMATING 0xf2f2
#define EREADER_READY 0xf1f1

#define GAME_READY 0xefef

#define EREADER_CANCEL 0xf7f7

#define EREADER_SEND_READY 0xf9f9
#define GAME_RECEIVE_READY 0xfefe

#define EREADER_SEND_START 0xfdfd
#define EREADER_SEND_END 0xfcfc

#define GAME_RECEIVE_OK 0xf5f5
#define GAME_RECEIVE_ERROR 0xf4f4

#define EREADER_SIO_END 0xf3f3
#define GAME_SIO_END 0xf1f1

#endif
