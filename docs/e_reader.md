- 🌎 **Original file**: https://github.com/mattieb/4-e/blob/3332a11accb628a283b914ba1e003f5ea8dedbe3/NOTES.md 🌎
- ✏️ **Updates**: Added **Transfer** section.

# Notes

## Connection

The game and e-Reader (specifically, the game's custom e-Reader
dotcode scanner, which is separately sent by the game to the e-Reader
and saved there for future use) communicate over [SIO multiplayer
mode](https://problemkaputt.de/gbatek.htm#siomultiplayermode).

The game is player 0 and master; the e-Reader is player 1.

Baud rate is 115200.

As master, the game drives the serial clock. The e-Reader waits for
the game to be ready before sending any data. Having our program
wait for serial interrupts before sending data appears to work very
reliably.

## ✏️ Transfer

Here's how games transfer their _custom e-Reader dotcode scanner_ (aka _"DLC Loader"_):

- Goes to MULTI mode and sends `0xFEFE`
- Sends `0xFEFE` again and expects `0xCCC0`
  * On the Japanese e-Reader, every `0xCCC0` becomes `0xCCD0`
- Sends `0xCCC0` and expects `0xCCC0`
- Switches to NORMAL32, waits 1 frame, and sends the card length as a 32-bit number (hi part is probably always `0x0000`, since there's not enough flash memory in the e-Reader)
- Sends the loader bytes in 4-byte chunks in a _fire and forget_ way
- Sends `0x00000000`
- Sends a 32-bit checksum, which is the sum of all previous 32-bit values
- Switches to MULTI mode again, waits 1 frame
- Sends `0xCCC0` and expects `0xCCC0`
- Sends `0xCCC0` and expects `0x1`
- All sends need a small wait or otherwise it'll work on mGBA but not on hardware (waiting for two VCOUNT changes works, but it's probably just 36μs)

## Card scanning protocol

When the e-Reader detects that the connection has been established,
it starts the protocol:

1.  The e-Reader sends FBFB until the game replies FBFB.

2.  The e-Reader sends 5841 until the game replies 5841.

3.  The e-Reader sends 4534 until the game replies 4534.

4.  Next, the game takes control, sending one of the following:

    -   ECEC to request a demo card.

    -   EDED to request a power-up card.

    -   EEEE to request a level card.

    The e-Reader replies F3F3. Both the game and the e-Reader
    continue to send F3F3 and the request message while Lakitu flies
    off the game screen.

5.  The game sends F2F2 and the e-Reader replies F2F2.  Both the
    game and the e-Reader continue to send F2F2 while Lakitu flies onto
    the e-Reader screen.

6.  The e-Reader sends F1F1.

7.  The game begins sending one of the following, expecting a scan to begin:

    -   EFEF when ready to receive a demo card.

    -   F0F0 when ready to receive a power-up card.

    -   FAFA when ready to receive a level card.

8.  The e-Reader can cancel at this point by sending F7F7, if B is
    pressed.  It will then start [shutdown](#shutdown-protocol).

9.  Once the e-Reader is ready to send, it sends F9F9.  The game
    begins sending FEFE repeatedly.

10. The e-Reader sends FDFD.  The game stops sending, but is still
    responsible for the serial clock.

11. The e-Reader sends the contents of the card, minus the 114-byte
    header, in two-byte packets.  They appear byteswapped in mGBA
    logs (e.g. bytes "CE CF" in the decoded card appear as "CFCE")
    but, in code, should be loaded as they appear (e.g. "0xcecf").

12. While transmitting, the e-Reader calculates a checksum by adding
    each 16-bit packet value (e.g. "0xcecf" as in the last example)
    to a 32-bit value that was initialized as zero. At the conclusion
    of the card, this checksum is transmitted into two 16-bit
    packets; least-significant first, most-significant second.

13. The e-Reader sends FCFC to end the card transmission.

14. If there was an transmission error (e.g. checksum failure), the
    game sends F4F4. _(Next steps are currently undocumented.)_

15. The game sends F5F5 to acknowledge a successful transmission,
    and the e-Reader begins [shutdown](#shutdown-protocol).

## Shutdown protocol

The e-Reader will do a clean shutdown of the communication session
in certain circumstances.

1.  The e-Reader sends F2F2 while Lakitu flies off the screen.

2.  The e-Reader sends F3F3 as its last packet, then stops transmitting.

3.  The game then sends F1F1 as its last packet, then stops
    transmitting.

At the conclusion of this protocol, the e-Reader is ready to restart
communications [from the beginning](#card-scanning-protocol) as if
it has just been started.

## Formats

### raw

.raw files are the complete binary data of the e-Reader card, with
error-correction applied, which can be rendered as dotcodes.

[nedcenc](https://github.com/Lymia/nedclib) can translate between
.raw and [.bin](#bin) files.

### bin

.bin files are the complete binary data of the e-Reader card, without
error-correction applied. The e-Reader will decode cards to this
internally.

| Offset | Size | Value |
| ------ | ---- | ----- |
| 0      | 114  | Card header |
| 114    | 1998 | [Card contents](#card-contents) |

The e-Reader requires the card header to be intact and correct. 4-e
will ignore the card header entirely, seeking 114 bytes into the
.bin file.

### sav

A Super Mario Advance 4 Flash save can "save" up to 32 levels, and
an additional in-progress level that, when beaten, moves to one of
the saved level slots.

| Offset | Size | Value |
| ------ | ---- | ----- |
| 800    | 1998 | In-progress [level contents](#level-contents) |
| 24592  | 1998 | Saved level 1 contents |
| 26624  | 1998 | Saved level 2 contents |
| 28688  | 1998 | Saved level 3 contents |
| 30720  | 1998 | Saved level 4 contents |
| 32784  | 1998 | Saved level 5 contents |
| 34816  | 1998 | Saved level 6 contents |
| 36880  | 1998 | Saved level 7 contents |
| 38912  | 1998 | Saved level 8 contents |
| 40976  | 1998 | Saved level 9 contents |
| 43008  | 1998 | Saved level 10 contents |
| 45072  | 1998 | Saved level 11 contents |
| 47104  | 1998 | Saved level 12 contents |
| 49168  | 1998 | Saved level 13 contents |
| 51200  | 1998 | Saved level 14 contents |
| 53264  | 1998 | Saved level 15 contents |
| 55296  | 1998 | Saved level 16 contents |
| 57360  | 1998 | Saved level 17 contents |
| 59392  | 1998 | Saved level 18 contents |
| 61456  | 1998 | Saved level 19 contents |
| 63488  | 1998 | Saved level 20 contents |
| 67584  | 1998 | In-progress level contents backup copy |
| 90128  | 1998 | Saved level 21 contents |
| 92160  | 1998 | Saved level 22 contents |
| 94224  | 1998 | Saved level 23 contents |
| 96256  | 1998 | Saved level 24 contents |
| 98320  | 1998 | Saved level 25 contents |
| 100352 | 1998 | Saved level 26 contents |
| 102416 | 1998 | Saved level 27 contents |
| 104448 | 1998 | Saved level 28 contents |
| 106512 | 1998 | Saved level 29 contents |
| 108544 | 1998 | Saved level 30 contents |
| 110608 | 1998 | Saved level 31 contents |
| 112640 | 1998 | Saved level 32 contents |

The saved level offsets seem a bit arbitrary but, in fact, follow
a pattern:

-   For slots 1 to 19, start at offset 24576, and for each slot,
    move 2048 bytes forward. (24576, 26624, 28672, 30702, etc.)

-   For slots 20 to 32, start at offset 90112, and for each slot, 
    move 2048 bytes forward. (90112, 92160, 94208, 96256, etc.)

-   For odd-numbered slots (1, 3, etc.), the contents start an
    additional 16 bytes in. (24592, 28688, etc.)

### Card contents

Card contents are transmitted in their entirety during the contents
step of the [card scanning protocol](#card-scanning-protocol).

They are always 1998 bytes long.

#### Level contents

When the [card contents](#card-contents) is a level:

| Offset | Size | Value |
| ------ | ---- | ----- |
| 0      | 1    | Level id |
| 1      | 7    | Bytes: 00 ff ff ff ff ff 00
| 8      | 1990 | [lcmp](#lcmp) |

The level id matches the last part of the [e-Card
number](https://www.mariowiki.com/Super_Mario_Advance_4:_Super_Mario_Bros._3_e-Reader_cards);
for example, Classic World 1-1 is numbered 07-A001 and has level
id is 1, and Airship's Revenge is numbered 07-A051 and has level
id is 51.

Official level ids 1-5 are "Classic" levels, 11-40 standard levels,
and 50-53 promotional levels. Re-using official level ids may cause
these levels to be overwritten.

While it's apparently technically possible to use any level id, at
least one
[report](https://discord.com/channels/884534133496365157/884534133496365160/1223234994907119667)
on the Smaghetti Discord suggests level ids higher than 72 (decimal)
will "mess up the level list".

### lcmp

.lcmp files are the portion of a level card that contains the
compressed level data itself, compressed using what is known as
"ASR0" compression.

| Offset | Size | Value |
| ------ | ---- | ----- |
| 0      | 4    | `"ASR0"` |
| 4      | 1986 | Compressed level data |

.lcmp files can be converted to [level contents](#level-contents),
given a level id.

.lcmp files can be decompressed into [.level](#level) files and
compressed again with sma4comp.

### level

.level files are the uncompressed level data.
