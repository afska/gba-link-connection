🌎 From: https://shonumi.github.io/dandocs.html#magb 🌎

# Mobile Adapter GB

- [General Hardware Information](#mobile-adapter-gb--general-hardware-information)
- [Compatible Games](#mobile-adapter-gb--compatible-games)
- [Protocol - Packet Format](#mobile-adapter-gb--protocol---packet-format)
- [Protocol - Flow of Communication](#mobile-adapter-gb--protocol---flow-of-communication)
- [Protocol - Commands](#mobile-adapter-gb--protocol---commands)
- [Configuration Data](#mobile-adapter-gb--protocol---configuration-data)

## \[Mobile Adapter GB\] : General Hardware Information

The Mobile Adapter GB was an accessory designed to allow the Game Boy Color, and later the Game Boy Advance, to connect online via cellular networks in Japan. Released on January 27, 2001, it supported a limited number of games before service was shutdown on December 14, 2002. Many of the compatible games supported features such on mail clients, downloadable bonus content, player-versus-player modes, and even online tournaments. It represented Nintendo's first official attempt at online gaming for its handhelds.

- The Mobile Adapter is a small device that essentially allows a Japanese phone to connect to the Game Boy's link port
- Model number is CGB-005
- Officially released with 3 different versions of the Mobile Adapter. Each featured distinct colors to work with different type of phones
- Each Mobile Adapter came packaged with a cartridge called the Mobile Trainer to help configure and setup the device
- Servers were formally hosted at gameboy.datacenter.ne.jp

Below, the Mobile Adapter variants are explained in further detail:

Blue -> Used to connect PDC phones.
Yellow -> Used to connect cdmaOne phones.
Red -> Used to connect DDI phones.
Green -> Would have been used to connect PHS phones, but this version was never released.

## \[Mobile Adapter GB\] : Compatible Games

There are currently 22 known games that are compatible with the Mobile Adapter:

Game Boy Color : 6 Total

- Game Boy Wars 3
- Hello Kitty: Happy House
- Mobile Golf
- Mobile Trainer
- Net de Get Minigames @ 100
- Pocket Monsters Crystal Version

Game Boy Advance : 16 Total

- All-Japan GT Championship
- Daisenryaku For Game Boy Advance
- Doraemon: Midori no Wakusei Doki Doki Daikyuushuutsu!
- Exciting Bass
- EX Monopoly
- JGTO Licensed: Golfmaster Mobile
- Kinniku Banzuke ~Kongou-kun no Daibouken!~
- Mail de Cute
- Mario Kart Advance
- Mobile Pro Baseball: Control Baton
- Monster Guardians
- Morita Shougi Advance
- Napoleon
- Play Novel: Silent Hill
- Starcom: Star Communicator
- Zero-Tours

Two games were planned but later cancelled: **beatmaniaGB Net Jam** for the GBC and **Horse Racing Creating Derby** for the GBA.

The GBA game Yu-Gi-Oh! Duel Monsters 5 Expert 1 contains code for the Mobile Adapter, but despite being built with the library it does not appear to use it. This functionality may have been planned and later abandoned.

## \[Mobile Adapter GB\] : Protocol - Packet Format

On the GBC, the Mobile Adapter operates using the fastest available setting (64KB/s) by setting Bits 0 and 1 of the SC register (0xFF02) high. It also uses an internal clock for all transfers. Communication is comparable to that of the Game Boy Printer, where the Game Boy sends packets with header, data, command, and checksum sections. On the GBA, the Mobile Adapter operates in NORMAL8 mode using a shift clock of 256KHz. Below is a chart breaking down the Mobile Adapter packet format used by the Game Boy or Mobile Adapter when acting as the sender. For response data sent by the receiver, refer to the next section.

```
-------------------------------------------------
Section   | Length
-------------------------------------------------
Magic Bytes : 0x99 0x66 | 2 bytes
Packet Header  | 4 bytes
Packet Data  | 0-254 bytes
Packet Checksum  | 2 bytes
Acknowledgement Signal | 2 bytes
-------------------------------------------------


-------------------------------------------------
Packet Header
-------------------------------------------------
Byte 1   | Command ID
Byte 2   | Unknown/Unused (0x00)
Byte 3   | High byte of Packet Data length
Byte 4   | Low byte of Packet Data length
-------------------------------------------------


-------------------------------------------------
Packet Data
-------------------------------------------------
Bytes 0-254  | Arbitrary data
-------------------------------------------------


-------------------------------------------------
Packet Checksum
-------------------------------------------------
Byte 1   | High byte of 16-bit sum
Byte 2   | Low byte of 16-bit sum
-------------------------------------------------


-------------------------------------------------
Acknowledgement Signal
-------------------------------------------------
Byte 1   | Device ID
Byte 2   | Command ID
-------------------------------------------------
```

The magic bytes are simply a pair of bytes used to identify the start of a Mobile Adapter packet.

Packet Data is arbitrary data and varies in length and content. On the Game Boy Color, it has a maximum size of 254 bytes. This restriction may be applied via software and appears to come from the fact that the Packet Data and Packet Checksum are lumped together, thus their total lengths must not exceed 256 bytes. Attempting to send more than 254 bytes of packet data causes communications errors in all supported GBC games. Evidence suggests GBA games can use Bytes 3 and 4 of the Packet Header to specify Packet Data size (possibly up to 64KB). The Mobile Adapter discards any packets bigger than 255 bytes, effectively forcing the high byte of the packet data length to be 0.

Data greater than the maximum packet length may be broken up into multiple packets, however. For example, when sending a large binary file such as an image or executable code, multiple packets are transferred from the Mobile Adapter to the Game Boy while the TCP transfer is ongoing.

The Packet Checksum is simply the 16-bit sum of all previous header bytes and all previous packet data bytes. It does not include the magic bytes. The checksum is transmitted big-endian.

After the checksum, a simple 2-byte Acknowledgement Signal is sent. The first byte is the Device ID OR'ed with the value 0x80. The second byte is 0x00 for the sender. The receiver transfers the Command ID from the Packet Header XOR'ed by 0x80. This essentially confirms what role the Game Boy is acting in. If it is the receiver, it is expecting to read information from the Packet Data from the Mobile Adapter. If it is the sender, it is pushing information from its own Packet Data to the Mobile Adapter. For example, with Command 0x19, the Game Boy is explicitly requesting data from the adapter, and with Command 0x1A the Game Boy is explicitly sending data to the adapter.

The Command ID byte in the Acknowledgement Signal may also be used for the receiver to indicate an error. If the checksum verification fails, the receiving side will send error 0xF1. This causes the sender to immediately re-attempt sending the packet up to 4 times. If the command isn't implemented/supported by the receiving Mobile Adapter, error 0xF0 will be sent. Error 0xF2 indicates an internal error, such as the Mobile Adapter's TCP/telephone transfer buffer being full.

The device ID determines what kind of hardware each side is communicating with. Below are the possible values and their meaning:

```
-------------------------------------------------
Device ID | OR Value | Device Type
-------------------------------------------------
0x00  | 0x80  | Game Boy Color
0x01  | 0x81  | Game Boy Advance
0x08  | 0x88  | PDC Mobile Adapter (Blue)
0x09  | 0x89  | cdmaOne Mobile Adapter (Yellow)
0x0A  | 0x8A  | PHS Mobile Adapter (Green)
0x0B  | 0x8B  | DDI Mobile Adapter (Red)
-------------------------------------------------
```

## \[Mobile Adapter GB\] : Protocol - Flow of Communication

Even though the protocol effectively enables 2-way communication between the Game Boy and a remote server, the handheld is expected to oversee all transmissions to the adapter itself. That is to say, the typical "master-slave" model often used for Game Boy serial I/O still applies in some sense. Once the server starts responding, the Game Boy has to continually initiate another transfer to the adapter (setting Bit 7 of 0xFF02 high) to keep reading any additional bytes that were sent.

It is up to the game software itself to handle secondary protocols (such as HTTP, POP3, or SMTP) which involve one side specifically acting as the sender or receiver. For example, after opening a TCP connection to an HTTP server and issuing the 0x15 command (Data Transfer), the software will determine whether the Game Boy is acting as a sender (making an HTTP request) or a receiver (receiving an HTTP response). Generally, this goes back and forth. The Game Boy sends information via its Packet Data, while the Mobile Adapter responds with 0xD2 "wait" bytes until the Game Boy finishes its TCP transfer. When the Game Boy's TCP transfer is done, the adapter sends any information from the server in its Packet Data while the Game Boy responds with 0x4B "wait" bytes. The chart below illustrates this concept and details what bytes are transferred by each side depending on their current role:

| Device         | Role     | Magic Bytes | Packet Header | Packet Checksum       | Packet Data   | Acknowledgement Signal                  |
| -------------- | -------- | ----------- | ------------- | --------------------- | ------------- | --------------------------------------- |
| Game Boy       | Sender   | 0x96 0x66   | Arbitrary     | Arbitrary             | Arbitrary     | Device ID OR 0x80 + 0x00                |
| Mobile Adapter | Receiver | 0xD2 0xD2   | 0xD2 0xD2 ... | 0xD2 0xD2 ... ... ... | 0xD2 0xD2 ... | Device ID OR 0x80 + Command ID XOR 0x80 |
| ---            | ---      | ---         | ---           | ---                   | ---           | ---                                     |
| Game Boy       | Receiver | 0x4B 0x4B   | 0x4B 0x4B ... | 0x4B 0x4B ... ... ... | 0x4B 0x4B ... | Device ID OR 0x80 + Command ID XOR 0x80 |
| Mobile Adapter | Sender   | 0x96 0x66   | Arbitrary     | Arbitrary             | Arbitrary     | Device ID OR 0x80 + 0x00                |

---

When beginning communications with the Mobile Adapter, the Game Boy typically assumes the role of sender first.

Many games appear to follow a certain order of commands initially. This may have been part of some kind of standard library available to developers in order to connect to an ISP. The commands most commonly look like this:

```
------------
Command 0x10  Begin Session. First is perhaps to test the presence of the Mobile Adapter
Command 0x11  Close Session.
Command 0x10  Begin Session. Open session for configuration data
------------
Command 0x19  Read Configuration Data. Grab first 96 bytes
Command 0x19  Read Configuration Data. Grab second 96 bytes
Command 0x11  Close Session.
Command 0x10  Begin Session. Open session to read configuration data again
Command 0x19  Read Configuration Data. Grab first 96 bytes
Command 0x19  Read Configuration Data. Grab second 96 bytes
------------
Command 0x17  Check Telephone Status if not busy
Command 0x12  Dial Telephone. Should be the ISP's number stored in configuration data
Command 0x21  ISP Login
Command 0x28  DNS Query
------------
```

From there, the software decides what next (if anything) needs to be done after successfully connecting to the internet.

When the GBC or GBA first start communicating with the Mobile Adapter, the first byte sent in response will be garbage data. Sending this first byte causes the Mobile Adapter to exit sleep mode, and the GBC or GBA will then have to wait a short interval (around 100ms) before starting communications proper. If it doesn't, the Mobile Adapter might send more garbage. Afterwards, however, it will reply with 0xD2 as its "idle" byte until a command is finished being sent. The Mobile Adapter enters sleep mode after 3 seconds since the last serial byte was transmitted. This implicitly cancels the command currently being processed, closes all connections currently open and ends the session.

## \[Mobile Adapter GB\] : Protocol - Commands

**Command 0x0F - Empty**

Data Sent: N/A. Empty Packet Data

Doesn't incite a reply from the adapter at all, aside from the Acknowledgement Signal. Presumably used to ping the adapter, not seen in any games in the wild.

**Command 0x10 - Begin Session**

Data Sent: "NINTENDO" ASCII string. 8 bytes only, not null-terminated

Data Received: "NINTENDO" ASCII string. 8 bytes only, not null-terminated

Sent to the adapter at the beginning of a session. The Game Boy sends an ASCII string containing "NINTENDO" and the adapter replies with a packet containing the same data. It must be noted that the adapter will not respond to other commands until it receives this command. If this command is sent twice, it returns an error.

**Command 0x11 - End Session**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

Sent to the adapter at the end of a session. The Packet Data is empty, and the length is zero bytes. This command causes all connections to be closed, and the phone to be hung up.

**Command 0x12 - Dial Telephone**

Data Sent: 1 unknown byte + telephone number

Data Received: N/A. Empty Packet Data

Instructs the adapter to dial a telephone number. The first byte's purpose is unknown, but seems to vary depending on the adapter type. 0 is sent for the blue/PDC adapter, 1 is sent for the green/PHS or red/DDI adapters, and 2 for the yellow/cdmaOne adapter. For unknown reasons, the blue adapter also accepts 16, the red adapter also accepts 9, and the yellow adapter doesn't actually verify this value. The following data is the telephone number represented in ASCII values, consisting of decimal numbers "0" through "9", as well as "#" and "\*". Any ASCII values not within this range are ignored. The maximum length of the phone number is 32 bytes.

**Command 0x13 - Hang Up Telephone**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

Instructs the adapter to close a telephone connection. This implicitly disconnects any open TCP/UDP connections. The Packet Data is empty, and the length is zero bytes.

**Command 0x14 - Wait For Telephone Call**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

Instructs the adapter to wait for and pick up an incoming call. This returns inmediately if there is no call to pick up, with an error packet with code 0. The Packet Data is empty, and the length is zero bytes.

**Command 0x15 - Transfer Data**

Data Sent: Connection ID + Arbitrary Data (optional)

Data Received: Connection ID + Arbitrary Data (optional)

Used to transfer data over TCP after command 0x23 (Open TCP Connection), transfer data over UDP after command 0x25 (Open UDP connection), or transfer data over the phone line after either 0x12 (Dial Telephone) or 0x14 (Wait For Telephone Call) have successfully been called. Only TCP/UDP communication is possible after a 0x21 (ISP Login) command, and the first byte indicates the connection that's being transferred over, as multiple can be opened simultaneously. If it's a mobile connection, the first byte is ignored, usually being set to 0xFF.

Generally, additional data is appended, although it is not required, such as when waiting for the server/other phone to send a reply. Large chunks of data greater than 254 bytes must be broken down into separate packets. While a connection is active, the Command ID in the Reply is 0x15 for the sender and 0x95 for the receiver. When a TCP connection is closed by the remote server (e.g. when an HTTP response has finished), and there's no leftover data to be received by the Game Boy, the Command ID in the Reply becomes 0x1F for the sender and 0x9F for the receiver, with a packet length of 0. Additionally, for TCP connections, if no data is sent, this command will wait for data to be received up to 1 second, before sending a reply.

During a phone-to-phone communication, no disconnection is detected, instead being indicated by the 0x17 (Telephone Status) command. However, most games implement this instead through a timeout during which no data has been received.

**Command 0x16 - Reset**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

This command does the same as sending commands 0x11 (End Session), followed by 0x10 (Begin Session). Additionally, it resets SIO32 Mode to the default state. Presumably used to reset the adapter, though not seen in any games in the wild.

**Command 0x17 - Telephone Status**

Data Sent: N/A. Empty Packet Data

Data Received: 3 bytes

Typically sent to the adapter before dialing. Also used to test the telephone status before opening a TCP connection, or to constantly ping the phone to make sure it's still active.

The reply is 3 bytes. The first byte indicates the phone's status, where 0xFF is returned if the phone is disconnected. If the phone isn't disconnected, bit 2 indicates whether the phone line is "busy" (i.e. in a call/picked up), and bit 0 indicates the presence of an incoming call (this remains if the incoming call is picked up). As such, the valid values are 0, 1, 4 and 5. Software may check bit 2 to know if the phone line is still connected. Most software doesn't seem to care about bit 0, but Net de Get: Mini Game @ 100 refuses to work with bit 0 set (value 5).

The second byte is related to the adapter type, where the blue/PDC adapter returns 0x4D, and the red/DDI and yellow/cdmaOne adapters return 0x48, though the actual meaning is unknown. The third byte is unknown, and usually hardcoded to 0. However, Pokemon Crystal reacts to the third byte being 0xF0 by allowing the player to bypass the 10 min/day battle time limit.

**Command 0x18 - SIO32 Mode**

Data Sent: 1 byte

Data Received: N/A. Empty Packet Data

This command is generally sent after Command 0x10. It enables/disables SIO32 Mode, which is useful for GBA games to be able to send more data, faster. The sent byte must be 1 to enable SIO32 mode, and 0 to disable it. SIO32 mode implies that any transmission will happen in chunks of 4 bytes instead of 1, which has implications with respect to the alignment of the communication.

When SIO32 mode is on, the packet data will be aligned to a multiple of 4 bytes, padding the remaining bytes with 0, and this won't be reflected in the packet length field. Similarly, the Acknowledgement Signal gains 2 padding bytes (hardcoded 0, not verified) at the end. Since the entire transmission (including Magic Bytes) is sent in chunks of 4, this means that the checksum is sent along with either the packet length (if length is 0) or the last 2 bytes of the packet (if length is not 0), and the acknowledgement signal is sent in the next chunk.

SIO32 Mode will only be toggled after the reply to this packet has been sent. The adapter should be allowed at least 100ms to toggle, as it might otherwise start sending garbage.

**Command 0x19 - Read Configuration Data**

Data Sent: 1 byte offset + 1 byte read length

Data Received: 1 byte offset + Requested Configuration Data

Requests data from the adapter's 256-byte configuration memory. The first byte sent to the adapter is the offset. The second byte sent is the length of data to read. The adapter responds with the same offset byte followed by configuration data from the adapter's internal memory. The maximum amount of data that can be requested at once is 128 bytes, and the adapter may return an error if the game requests more data. Most software send 2 of these commands to read 96-byte chunks, for a total of 192 bytes, which is the area of this memory that is actually used.

**Command 0x1A - Write Configuration Data**

Data Sent: 1 byte offset + Configuration Data to Write

Data Received: 1 byte offset

Writes data to the adapter's 256-byte configuration memory. The first byte sent to the adapter is the offset. The following bytes are the data to be written in the adapters internal memory. A maximum of 128 bytes may be written at once.

**Command 0x21 - ISP Login**

Data Sent: 1 byte Login ID Length + Login ID + 1 byte Password Length + Password + 4 bytes DNS Address #1 + 4 bytes DNS Address #2

Data Received: 4 bytes assigned IP + 4 bytes assigned DNS Address #1 + 4 bytes assigned DNS Address #2 /p>

Logs into the DION dial-up service, after calling it with command 0x12 (Dial Telephone), allowing the adapter to connect to the internet. Both the Login ID and Password are prefixed with bytes declaring their lengths, with a maximum length of 0x20. The IPv4 DNS addresses are 4 bytes each, with a single byte representing one octet. The reply contains the assigned IP address and DNS addresses. If the game sets either of the DNS addresses to 0, the adapter may assign the DNS address on its own, and return that in the reply, otherwise, the reply's DNS addresses are 0.0.0.0.

**Command 0x22 - ISP Logout**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

Logs out of the DION service. This command causes all connections to be closed.

**Command 0x23 - Open TCP Connection**

Data Sent: 4 bytes for IP Address + 2 Bytes for Port Number

Data Received: 1 byte Connection ID

Opens a TCP connection at the given IP address on the given port, after logging into the DION dial-up service. The IPv4 IP address is 4 bytes, with a single byte representing one octet. The port number is big-endian. Depending on which port the TCP connection opens (25, 80, 110), different protocols can be accessed on a server (SMTP, HTTP, and POP respectively). Handling the details of the protocol itself depends on the software and the server. The Mobile Adapter is merely responsible for opening the connection and handling TCP transfers such as when using Command 0x15. The reply contains the Connection ID, which must be used when using Command 0x15 (Transfer Data). The maximum amount of connections on a real adapter is 2.

**Command 0x24 - Close TCP Connection**

Data Sent: 1 byte Connection ID

Data Received: 1 byte Connection ID

Closes an active TCP connection.

**Command 0x25 - Open UDP Connection**

Data Sent: 4 bytes for IP Address + 2 Bytes for Port Number

Data Received: 1 byte Connection ID

Opens a UDP connection at the given IP address on the given port, after logging into the DION dial-up service. It's an analog of Command 0x23 (Open TCP Connection), but opens a UDP connection instead. This UDP connection is bound to the specified IP address and Port until it's closed. When using Command 0x15 (Transfer Data) with a UDP connection, it's impossible to know the sender of any received data, as it isn't verified.

**Command 0x26 - Close UDP Connection**

Data Sent: 1 byte Connection ID

Data Received: 1 byte Connection ID

Closes an active UDP connection.

**Command 0x28 - DNS Query**

Data Sent: Domain Name

Data Received: 4 bytes for IP Address

Looks up the IP address for a domain name, using the DNS server addresses sent in Command 0x21. This command also accepts an ASCII IPv4 address (as parsed by the inet_addr(3) function of POSIX), converting it into a 4-byte IPv4 address instead of querying the DNS server. The domain name is in ASCII and may contain zeroes, which truncate the name.

**Command 0x3F - Firmware Version**

Data Sent: N/A. Empty Packet Data

Data Received: N/A. Empty Packet Data

On a real Mobile Adapter, this causes it to send firmware version information through the serial pins on the phone connector, and enter a state in which no other commands can be used. Presumably, this enters a test mode of some kind. Likely not used by any games.

This command may not be used if the phone line is in use. The only way to resume sending commands after this one is sent, is sending Command 0x16 (Reset), or, exclusively on the blue adapter, Command 0x11 (End Session) may also be used.

**Command 0x6E - Error Status**

Data Sent: N/A. Adapter sends this in response to a failed command

Data Received: 1 byte for command that failed + 1 byte for error status

If a previously sent command fails, the adapter will respond with this instead, indicating the command that failed as well as a brief status code. The error statuses for one command do not indicate the same error for another command, so context matters when parsing the codes. The following commands and their known error status codes are listed below:

```
0x10: Error Code 0x01 - Sent twice
0x10: Error Code 0x02 - Invalid contents

0x11: Error Code 0x02 - Still connected/failed to disconnect(?)

0x12: Error Code 0x00 - Telephone line is busy
0x12: Error Code 0x01 - Invalid use (already connected)
0x12: Error Code 0x02 - Invalid contents (first byte isn't correct)
0x12: Error Code 0x03 - Communication failed/phone not connected
0x12: Error Code 0x04 - Call not established, redial

0x13: Error Code 0x01 - Invalid use (already hung up/phone not connected)

0x14: Error Code 0x00 - No call received/phone not connected
0x14: Error Code 0x01 - Invalid use (already calling)
0x14: Error Code 0x03 - Internal error (ringing but picking up fails)

0x15: Error Code 0x00 - Invalid connection/communication failed
0x15: Error Code 0x01 - Invalid use (Call was ended/never made)

0x16: Error Code 0x00 - Still connected/failed to disconnect(?)

0x18: Error Code 0x02 - Invalid contents (first byte not either 1 or 0)

0x19: Error Code 0x00 - Internal error (Failed to read config)
0x19: Error Code 0x02 - Read outside of config area/too big a chunk

0x1A: Error Code 0x00 - Internal error (Failed to write config)
0x1A: Error Code 0x02 - Write outside of config area/too big a chunk

0x21: Error Code 0x01 - Invalid use (Not in a call)
0x21: Error Code 0x02 - Unknown error (some kind of timeout?)
0x21: Error Code 0x03 - Unknown error (internal error?)

0x22: Error Code 0x00 - Invalid use (Not logged in)
0x22: Error Code 0x01 - Invalid use (Not in a call)
0x22: Error Code 0x02 - Unknown error (some kind of timeout?)

0x23: Error Code 0x00 - Too many connections
0x23: Error Code 0x01 - Invalid use (Not logged in)
0x23: Error Code 0x03 - Connection failed

0x24: Error Code 0x00 - Invalid connection (Not connected)
0x24: Error Code 0x01 - Invalid use (Not logged in)
0x24: Error Code 0x02 - Unknown error (???)

0x25: Error Code 0x00 - Too many connections
0x25: Error Code 0x01 - Invalid use (Not logged in)
0x25: Error Code 0x03 - Connection failed (though this can't happen)

0x26: Error Code 0x00 - Invalid connection (Not connected)
0x26: Error Code 0x01 - Invalid use (Not logged in)
0x26: Error Code 0x02 - Unknown error (???)

0x28: Error Code 0x01 - Invalid use (not logged in)
0x28: Error Code 0x02 - Invalid contents/lookup failed
```

## \[Mobile Adapter GB\] : Protocol - Configuration Data

The Mobile Adapter has small area of built-in memory designed to store various settings for its configuration. It only uses 192 bytes but data is readable and writable via the Commands 0x19 and 0x1A respectively. These fields are filled out when running the initial setup on Mobile Trainer. The memory is laid out as describe below:

```
--------------------------
0x00 - 0x01  :: "MA" in ASCII. The "Mobile Adapter" header.
0x02   :: Set to 0x1 during Mobile Trainer registration and 0x81 when registration is complete
0x04 - 0x07  :: Primary DNS server (210.196.3.183)
0x08 - 0x0B  :: Secondary DNS server (210.141.112.163)
0x0C - 0x15  :: Login ID in the format gXXXXXXXXX. Mobile Trainer only allows 9 editable characters
0x2C - 0x43  :: User email address in the format XXXXXXXX@YYYY.dion.ne.jp
0x4A - 0x5D  :: SMTP server in the format mail.XXXX.dion.ne.jp
0x5E - 0x70  :: POP server in the format pop.XXXX.dion.ne.jp
0x76 - 0x8D  :: Configuration Slot #1
0x8E - 0xA5  :: Configuration Slot #2
0xA6 - 0xBD  :: Configuration Slot #3
0xBE - 0xBF  :: 16-bit big-endian checksum
--------------------------
```

Each configuration slot may contain an 8-byte telephone number to be used to connect to the ISP and a 16-byte ID string. The telephone number is stored in a variant of binary-coded decimal, where 0x0A represents the "#" key, 0x0B represents the "\*" key, and 0x0F marks the end of the telephone number. These slots may have been intended to allow users to connect online using ISPs besides DION at some point, however, Nintendo never implemented any such plans.

If the Mobile Adapter is connected to a PDC or CDMA device, the telephone number defaults to #9677 with an ID string of "DION PDC/CDMAONE". If the Mobile Adapter is connected to a PHS or DDI device, the telephone number defaults to 0077487751 with an ID string of "DION DDI-POCKET". Only the first slot is configured by Mobile Trainer; it fills the rest with 0xFF and 0x00 bytes. An unidentified device (as reported by the Device ID in the Acknowledgement Signal of a packet) causes the Mobile Adapter to overwrite all configuration data with garbage values.

The checksum is simply the 16-bit sum of bytes 0x00 - 0xBD.

All software compatible with the Mobile Adapter appears to read the configuration data first and foremost. If the data cannot be read or if there is a problem with the data, they will refuse to even attempt logging in to the DION dial-up service. Generally, they return the error code 25-000 in that situation.

If any compatible software attempts to read or write configuration data outside the allotted 256 bytes via commands 0x19 and 0x1A, the entire I/O operation is cancelled. No data is written even if the initial offset is within the 256 bytes. No data is returned either, as both commands respond with Error Status packets.
