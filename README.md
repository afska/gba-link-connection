# gba-link-connection

A set of Game Boy Advance (GBA) C++ libraries to interact with the Serial Port. Its main purpose is to provide multiplayer support to homebrew games. [C bindings](#c-bindings) are also included for compatibility.

- [👾](#-LinkCable) [LinkCable.hpp](lib/LinkCable.hpp): The classic 16-bit **Multi-Play mode** (up to 4 players) using a GBA Link Cable!
  - [💻](#-LinkCableMultiboot) [LinkCableMultiboot.hpp](lib/LinkCableMultiboot.hpp): ‍Send **Multiboot software** (small 256KiB ROMs) to other GBAs with no cartridge!
  - [🔧👾](#-LinkRawCable) [LinkRawCable.hpp](lib/LinkRawCable.hpp): A **minimal** low-level API for the 16-bit Multi-Play mode.
- [📻](#-LinkWireless) [LinkWireless.hpp](lib/LinkWireless.hpp): Connect up to 5 consoles with the **Wireless Adapter**!
  - [📡](#-LinkWirelessMultiboot) [LinkWirelessMultiboot.hpp](lib/LinkWirelessMultiboot.hpp): ‍Send Multiboot software (small 256KiB ROMs) to other GBAs **over the air**!
  - [🔧📻](#-LinkRawWireless) [LinkRawWireless.hpp](lib/LinkRawWireless.hpp): A **minimal** low-level API for the Wireless Adapter.
  - [🔧🏛️](#-LinkWirelessOpenSDK) [LinkWirelessOpenSDK.hpp](lib/LinkWirelessOpenSDK.hpp): An abstraction of the **official** software level protocol of the Wireless Adapter.
- [🌎](#-LinkUniversal) [LinkUniversal.hpp](lib/LinkUniversal.hpp): Add multiplayer support to your game, both with 👾 _Link Cables_ and 📻 _Wireless Adapters_, using the **same API**!
- [🔌](#-LinkGPIO) [LinkGPIO.hpp](lib/LinkGPIO.hpp): Use the Link Port however you want to control **any device** (like LEDs, rumble motors, and that kind of stuff)!
- [🔗](#-LinkSPI) [LinkSPI.hpp](lib/LinkSPI.hpp): Connect with a PC (like a **Raspberry Pi**) or another GBA (with a GBC Link Cable) using this mode. Transfer up to 2Mbit/s!
- [⏱️](#%EF%B8%8F-LinkUART) [LinkUART.hpp](lib/LinkUART.hpp): Easily connect to **any PC** using a USB to UART cable!
- [🟪](#-LinkCube) [LinkCube.hpp](lib/LinkCube.hpp): Exchange data with a _Wii_ or a _GameCube_ using the classic **Joybus** protocol!
- [📱](#-LinkMobile) [LinkMobile.hpp](lib/LinkMobile.hpp): Connect to **the internet** using the _Mobile Adapter GB_, brought back to life thanks to the [REON](https://github.com/REONTeam) project!
- [🖱️](#%EF%B8%8F-LinkPS2Mouse) [LinkPS2Mouse.hpp](lib/LinkPS2Mouse.hpp): Connect a **PS/2 mouse** to the GBA for extended controls!
- [⌨️](#%EF%B8%8F-LinkPS2Keyboard) [LinkPS2Keyboard.hpp](lib/LinkPS2Keyboard.hpp): Connect a **PS/2 keyboard** to the GBA for extended controls!

_(click on the emojis for documentation)_

> <img alt="rlabs" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116227197-400d2380-a72a-11eb-9e7b-389aae76f13e.png" /> Created by [[r]labs](https://r-labs.io).

> 💬 Check out my other GBA projects: [piuGBA](https://github.com/afska/piugba), [beat-beast](https://github.com/afska/beat-beast), [gba-remote-play](https://github.com/afska/gba-remote-play), [gba-flashcartio](https://github.com/afska/gba-flashcartio).

## Usage

- Copy the contents of the [lib/](lib/) folder into a directory that is part of your project's include path. Then, `#include` the library you need, such as [LinkCable.hpp](lib/LinkCable.hpp), in your project. No external dependencies are required.
- For initial instructions and setup details, refer to the large comment block at the beginning of each file, the documentation included here, and the provided examples.
- Check out the [examples/](examples/) folder.
  - **Compiled ROMs are available** in [Releases](https://github.com/afska/gba-link-connection/releases).
  - The example code uses [libtonc](https://github.com/gbadev-org/libtonc) (and [libugba](https://github.com/AntonioND/libugba) for interrupts), but any library can be used.
  - The examples can be tested on real GBAs or using emulators.
  - The [LinkUniversal_real](https://github.com/afska/gba-link-universal-test) ROM tests a more real scenario using an audio player, a background video, text and sprites.
  - The `LinkCableMultiboot_demo` and `LinkWirelessMultiboot_demo` examples can bootstrap all other examples, allowing you to test with multiple units even if you only have one flashcart.

> The files use some compiler extensions, so using **GCC** is required.

> The example ROMs were compiled with [devkitARM](https://devkitpro.org), using GCC `14.1.0` with `-std=c++17` as the standard and `-Ofast` as the optimization level.

> To learn implementation details, you might also want to check out the [docs/](docs/) folder, which contains important documentation.

### Compiling the examples

Running `./compile.sh` builds all the examples with the right configuration.

The project must be in a path without spaces; _devkitARM_ and some \*nix commands are required.

All the projects understand these Makefile actions:

```bash
make [ clean | build | start | rebuild | restart ]
```

### C bindings

- To use the libraries in a C project, include the files from the [lib/c_bindings/](lib/c_bindings/) directory.
- For documentation, use this `README.md` file or comments inside the main C++ files.
- Some libraries may not be available in C.
- Some methods/overloads may not be available in the C implementations.
- Unlike the main libraries, C bindings depend on _libtonc_.

```cpp
// Instantiating
LinkSomething* linkSomething = new LinkSomething(a, b); // C++
LinkSomethingHandle cLinkSomething = C_LinkSomething_create(a, b); // C

// Calling methods
linkSomething->method(a, b); // C++
C_LinkSomething_method(cLinkSomething, a, b); // C

// Destroying
delete linkSomething; // C++
C_LinkSomething_destroy(cLinkSomething); // C
```

# 👾 LinkCable

_(aka Multi-Play Mode)_

[⬆️](#gba-link-connection) This is the Link Port mode that games use for multiplayer.

The library uses message queues to send/receive data and transmits when it's possible. As it uses CPU interrupts, the connection is alive even if a console drops a frame or gets stuck in a long iteration loop. After such an event, all nodes end up receiving all the pending messages.

![screenshot](https://user-images.githubusercontent.com/1631752/99154109-1d131980-268c-11eb-86b1-7a728f639e5e.png)

## Constructor

`new LinkCable(...)` accepts these **optional** parameters:

| Name          | Type           | Default       | Description                                                                                                                                                                                                                                                                    |
| ------------- | -------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `baudRate`    | **BaudRate**   | `BAUD_RATE_1` | Sets a specific baud rate.                                                                                                                                                                                                                                                     |
| `timeout`     | **u32**        | `3`           | Maximum number of _frames_ without receiving data from other player before marking them as disconnected or resetting the connection.                                                                                                                                           |
| `interval`    | **u16**        | `50`          | Number of _1024-cycle ticks_ (61.04μs) between transfers _(50 = 3.052ms)_. It's the interval of Timer #`sendTimerId`. Lower values will transfer faster but also consume more CPU. You can use `Link::perFrame(...)` to convert from _packets per frame_ to _interval values_. |
| `sendTimerId` | **u8** _(0~3)_ | `3`           | GBA Timer to use for sending.                                                                                                                                                                                                                                                  |

You can update these values at any time without creating a new instance:

- Call `deactivate()`.
- Mutate the `config` property.
- Call `activate()`.

You can also change these compile-time constants:

- `LINK_CABLE_QUEUE_SIZE`: to set a custom buffer size (how many incoming and outgoing messages the queues can store at max **per player**). The default value is `15`, which seems fine for most games.
  - This affects how much memory is allocated. With the default value, it's around `390` bytes. There's a double-buffered pending queue (to avoid data races), `1` incoming queue and `1` outgoing queue.
  - You can approximate the memory usage with:
    - `(LINK_CABLE_QUEUE_SIZE * sizeof(u16) * LINK_CABLE_MAX_PLAYERS) * 3 + LINK_CABLE_QUEUE_SIZE * sizeof(u16)` <=> `LINK_CABLE_QUEUE_SIZE * 26`

## Methods

| Name                        | Return type    | Description                                                                                                                                                                      |
| --------------------------- | -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`                | **bool**       | Returns whether the library is active or not.                                                                                                                                    |
| `activate()`                | -              | Activates the library.                                                                                                                                                           |
| `deactivate()`              | -              | Deactivates the library.                                                                                                                                                         |
| `isConnected()`             | **bool**       | Returns `true` if there are at least 2 connected players.                                                                                                                        |
| `playerCount()`             | **u8** _(0~4)_ | Returns the number of connected players.                                                                                                                                         |
| `currentPlayerId()`         | **u8** _(0~3)_ | Returns the current player ID.                                                                                                                                                   |
| `sync()`                    | -              | Collects available messages from interrupts for later processing with `read(...)`. Call this method whenever you need to fetch new data.                                                                                                                          |
| `waitFor(playerId)`         | **bool**       | Waits for data from player #`playerId`. Returns `true` on success, or `false` on disconnection.                                                                                  |
| `waitFor(playerId, cancel)` | **bool**       | Like `waitFor(playerId)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the wait if it returns `true`.                                    |
| `canRead(playerId)`         | **bool**       | Returns `true` if there are pending messages from player #`playerId`. Keep in mind that if this returns `false`, it will keep doing so until you _fetch new data_ with `sync()`. |
| `read(playerId)`            | **u16**        | Dequeues and returns the next message from player #`playerId`. If there's no data from that player, a `0` will be returned.                                                      |
| `peek(playerId)`            | **u16**        | Returns the next message from player #`playerId` without dequeuing it. If there's no data from that player, a `0` will be returned.                                              |
| `send(data)`                | -              | Sends `data` to all connected players. If `data` is invalid or the send queue is full, a `false` will be returned.                                                                                                                                           |
| `didQueueOverflow()`              | **bool**              | Returns whether the internal receive queue lost messages at some point due to being full. This can happen if your queue size is too low, if you receive too much data without calling `sync(...)` enough times, or if you don't `read(...)` enough messages before the next `sync()` call. After this call, the overflow flag is cleared if `clear` is `true` (default behavior).                                                                                                                                   |
| `resetTimer()`              | -              | Restarts the send timer without disconnecting. Call this if you changed `config.interval`                                                                                                                                   |

⚠️ `0xFFFF` and `0x0` are reserved values, so don't send them!

# 💻 LinkCableMultiboot

_(aka Multiboot through Multi-Play Mode)_

[⬆️](#gba-link-connection) This tool allows sending Multiboot ROMs (small 256KiB programs that fit in EWRAM) from one GBA to up to 3 slaves, using a single cartridge.

Its demo (`LinkCableMultiboot_demo`) has all the other gba-link-connection ROMs bundled with it, so it can be used to quickly test the library.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/6ff55944-5437-436f-bcc7-a89b05dc5486)

You can change these compile-time constants:

- `LINK_CABLE_MULTIBOOT_PALETTE_DATA`: to control how the logo is displayed.
  - Format: `0b1CCCDSS1`, where `C`=color, `D`=direction, `S`=speed.
  - Default: `0b10010011`.

## Methods

| Name                                    | Return type                    | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| --------------------------------------- | ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `sendRom(rom, romSize, cancel, [mode])` | **LinkCableMultiboot::Result** | Sends the `rom`. During the handshake process, the library will continuously invoke `cancel`, and abort the transfer if it returns `true`. The `romSize` must be a number between `448` and `262144`, and a multiple of `16`. The `mode` can be either `LinkCableMultiboot::TransferMode::MULTI_PLAY` for GBA cable (default value) or `LinkCableMultiboot::TransferMode::SPI` for GBC cable. Once completed, the return value should be `LinkCableMultiboot::Result::SUCCESS`. |

⚠️ stop DMA before sending the ROM! _(you might need to stop your audio player)_

# 🔧👾 LinkRawCable

[⬆️](#gba-link-connection)

- This is a minimal hardware wrapper designed for the _Multi-Play mode_.
- It doesn't include any of the features of [👾 LinkCable](#-LinkCable), so it's not well suited for games.
- Its demo (`LinkRawCable_demo`) can help emulator developers in enhancing accuracy.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/29a25bf7-211e-49d6-a32a-566c72a44973)

## Methods

| Name                               | Return type                  | Description                                                                                                                                                                               |
| ---------------------------------- | ---------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`                       | **bool**                     | Returns whether the library is active or not.                                                                                                                                             |
| `activate(baudRate = BAUD_RATE_1)` | -                            | Activates the library in a specific `baudRate` (`LinkRawCable::BaudRate`).                                                                                                                |
| `deactivate()`                     | -                            | Deactivates the library.                                                                                                                                                                  |
| `transfer(data)`                   | **LinkRawCable::Response**   | Exchanges `data` with the connected consoles. Returns the received data, including the assigned player ID.                                                                                |
| `transfer(data, cancel)`           | **LinkRawCable::Response**   | Like `transfer(data)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.                                            |
| `transferAsync(data)`              | -                            | Schedules a `data` transfer and returns. After this, call `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the async data, normal `transfer(...)`s won't do anything! |
| `getAsyncState()`                  | **LinkRawCable::AsyncState** | Returns the state of the last async transfer (one of `LinkRawCable::AsyncState::IDLE`, `LinkRawCable::AsyncState::WAITING`, or `LinkRawCable::AsyncState::READY`).                        |
| `getAsyncData()`                   | **LinkRawCable::Response**   | If the async state is `READY`, returns the remote data and switches the state back to `IDLE`. If not, returns an empty response.                                                          |
| `getBaudRate()`                    | **LinkRawCable::BaudRate**   | Returns the current `baudRate`.                                                                                                                                                           |
| `isMaster()`                       | **bool**                     | Returns whether the console is connected as master or not. Returns garbage when the cable is not properly connected.                                                                      |
| `isReady()`                        | **bool**                     | Returns whether all connected consoles have entered the multiplayer mode. Returns garbage when the cable is not properly connected.                                                       |

⚠️ advanced usage only; if you're building a game, use `LinkCable`!

⚠️ don't send `0xFFFF`, it's a reserved value that means _disconnected client_!

⚠️ only `transfer(...)` if `isReady()`!

# 📻 LinkWireless

_(aka GBA Wireless Adapter)_

[⬆️](#gba-link-connection) This is a driver for an accessory that enables wireless games up to 5 players. The inner workings of the adapter are highly unknown, but [this blog post](docs/wireless_adapter.md) is very helpful. I've updated it to add more details about the things I learned by means of ~~reverse engineering~~ brute force and trial&error.

The library, by default, implements a lightweight protocol (on top of the adapter's message system) that sends packet IDs and checksums. This allows detecting disconnections, forwarding messages to all nodes, and retransmitting to prevent packet loss.

https://github.com/afska/gba-link-connection/assets/1631752/7eeafc49-2dfa-4902-aa78-57b391720564

## Constructor

`new LinkWireless(...)` accepts these **optional** parameters:

| Name             | Type           | Default | Description                                                                                                                                                                                                                                                                    |
| ---------------- | -------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `forwarding`     | **bool**       | `true`  | If `true`, the server forwards all messages to the clients. Otherwise, clients only see messages sent from the server (ignoring other peers).                                                                                                                                  |
| `retransmission` | **bool**       | `true`  | If `true`, the library handles retransmission for you, so there should be no packet loss.                                                                                                                                                                                      |
| `maxPlayers`     | **u8** _(2~5)_ | `5`     | Maximum number of allowed players.                                                                                                                                                                                                                                             |
| `timeout`        | **u32**        | `10`    | Maximum number of _frames_ without receiving data from other player before resetting the connection.                                                                                                                                                                           |
| `interval`       | **u16**        | `50`    | Number of _1024-cycle ticks_ (61.04μs) between transfers _(50 = 3.052ms)_. It's the interval of Timer #`sendTimerId`. Lower values will transfer faster but also consume more CPU. You can use `Link::perFrame(...)` to convert from _packets per frame_ to _interval values_. |
| `sendTimerId`    | **u8** _(0~3)_ | `3`     | GBA Timer to use for sending.                                                                                                                                                                                                                                                  |

You can update these values at any time without creating a new instance:

- Call `deactivate()`.
- Mutate the `config` property.
- Call `activate()`.

You can also change these compile-time constants:

- `LINK_WIRELESS_QUEUE_SIZE`: to set a custom buffer size (how many incoming and outgoing messages the queues can store at max). The default value is `30`, which seems fine for most games.
  - This affects how much memory is allocated. With the default value, it's around `960` bytes. There's a double-buffered incoming queue and a double-buffered outgoing queue (to avoid data races).
  - You can approximate the memory usage with:
    - `LINK_WIRELESS_QUEUE_SIZE * sizeof(Message) * 4` <=> `LINK_WIRELESS_QUEUE_SIZE * 32`
- `LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH` and `LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH`: to set the biggest allowed transfer per timer tick. Transfers contain retransmission headers and multiple user messages. These values must be in the range `[6;20]` for servers and `[2;4]` for clients. The default values are `20` and `4`, but you might want to set them a bit lower to reduce CPU usage.
- `LINK_WIRELESS_PUT_ISR_IN_IWRAM`: to put critical functions in IWRAM, which can significantly improve performance due to its faster access. This is disabled by default to conserve IWRAM space, which is limited, but it's enabled in demos to showcase its performance benefits.
  - If you enable this, make sure that `LinkWireless.cpp` gets compiled! For example, in a Makefile-based project, verify that the file is in your `SRCDIRS` list.
- `LINK_WIRELESS_ENABLE_NESTED_IRQ`: to allow `LINK_WIRELESS_ISR_*` functions to be interrupted. This can be useful, for example, if your audio engine requires calling a VBlank handler with precise timing.
  - This won't produce any effect if `LINK_WIRELESS_PUT_ISR_IN_IWRAM` is disabled.
- `LINK_WIRELESS_USE_SEND_RECEIVE_LATCH`: to alternate between sends and receives on each timer tick (instead of doing both things). This is disabled by default. Enabling it will introduce a bit of latency but also reduce _a lot_ the overall CPU usage. It's enabled in the `LinkWireless_demo` example, but disabled in the `LinkUniversal_*` examples.
- `LINK_WIRELESS_TWO_PLAYERS_ONLY`: to optimize the library for two players. This will make the code smaller and use less CPU. It will also let you "misuse" `5` bits from the packet header to send small packets really fast (e.g. pressed keys) without confirmation, using the `QUICK_SEND` and `QUICK_RECEIVE` properties. When this option is enabled, the `maxPlayers` constructor parameter will be ignored.

## Methods

- Most of these methods return a boolean, indicating if the action was successful. If not, you can call `getLastError()` to know the reason. Usually, unless it's a trivial error (like buffers being full), the connection with the adapter is reset and the game needs to start again.
- You can check the connection state at any time with `getState()`.
- Until a session starts, all actions are synchronous.
- During sessions (when the state is `SERVING` or `CONNECTED`), the message transfers are IRQ-driven, so `send(...)` and `receive(...)` won't waste extra cycles. Though there are some synchronous methods that can be called during a session:
  - `serve(...)`, to update the broadcast data.
  - `closeServer()`, to make it the room unavailable for new players.
  - `getSignalLevel(...)`, to retrieve signal levels.

| Name                                      | Return type             | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ----------------------------------------- | ----------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`                              | **bool**                | Returns whether the library is active or not.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| `activate()`                              | **bool**                | Activates the library. When an adapter is connected, it changes the state to `AUTHENTICATED`. It can also be used to disconnect or reset the adapter.                                                                                                                                                                                                                                                                                                                                                                                  |
| `restoreExistingConnection()`                              | **bool**                | Restores the state from an existing connection on the Wireless Adapter hardware. This is useful, for example, after a fresh launch of a Multiboot game, to synchronize the library with the current state and avoid a reconnection. Returns whether the restoration was successful. On success, the state should be either `SERVING` or `CONNECTED`. This should be used as a replacement for `activate()`.                                                                                                                                                                                                                                                                                                                                                                                  |
| `deactivate([turnOff])`                            | **bool**                | Puts the adapter into a low consumption mode and then deactivates the library. It returns a boolean indicating whether the transition to low consumption mode was successful. You can disable the transition and deactivate directly by setting `turnOff` to `true`.                                                                                                                                                                                                                                                                   |
| `serve([gameName], [userName], [gameId])` | **bool**                | Starts broadcasting a server and changes the state to `SERVING`. You can, optionally, provide a `gameName` (max `14` characters), a `userName` (max `8` characters), and a `gameId` _(0 ~ 0x7FFF)_ that games will be able to read. The strings must be null-terminated character arrays. If the adapter is already serving, this method only updates the broadcast data. Updating broadcast data while serving can fail if the adapter is busy. In that case, this will return `false` and `getLastError()` will be `BUSY_TRY_AGAIN`. |
| `closeServer()`                           | **bool**                | Closes the server while keeping the session active, to prevent new users from joining the room. This action can fail if the adapter is busy. In that case, this will return `false` and `getLastError()` will be `BUSY_TRY_AGAIN`.                                                                                                                                                                                                                                                                                              |
| `getSignalLevel(response)`                           | **bool**                | Retrieves the signal level of each player (0-255), filling the `response` struct. For hosts, the array will contain the signal level of each client in indexes 1-4. For clients, it will only include the index corresponding to the `currentPlayerId()`. This action can fail if the adapter is busy. In that case, this will return `false` and `getLastError()` will be `BUSY_TRY_AGAIN`.                                                                                                                                                                                                                                                                                              |
| `getServers(servers, [onWait])`           | **bool**                | Fills the `servers` array with all the currently broadcasting servers. This action takes 1 second to complete, but you can optionally provide an `onWait()` function which will be invoked each time VBlank starts.                                                                                                                                                                                                                                                                                                                    |
| `getServersAsyncStart()`                  | **bool**                | Starts looking for broadcasting servers and changes the state to `SEARCHING`. After this, call `getServersAsyncEnd(...)` 1 second later.                                                                                                                                                                                                                                                                                                                                                                                               |
| `getServersAsyncEnd(servers)`             | **bool**                | Fills the `servers` array with all the currently broadcasting servers. Changes the state to `AUTHENTICATED` again.                                                                                                                                                                                                                                                                                                                                                                                                                     |
| `connect(serverId)`                       | **bool**                | Starts a connection with `serverId` and changes the state to `CONNECTING`.                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| `keepConnecting()`                        | **bool**                | When connecting, this needs to be called until the state is `CONNECTED`. It assigns a player ID. Keep in mind that `isConnected()` and `playerCount()` won't be updated until the first message from the server arrives.                                                                                                                                                                                                                                                                                                               |
| `send(data)`                              | **bool**                | Enqueues `data` to be sent to other nodes.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| `receive(messages)`                       | **bool**                | Fills the `messages` array with incoming messages, forwarding if needed.                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `getState()`                              | **LinkWireless::State** | Returns the current state (one of `LinkWireless::State::NEEDS_RESET`, `LinkWireless::State::AUTHENTICATED`, `LinkWireless::State::SEARCHING`, `LinkWireless::State::SERVING`, `LinkWireless::State::CONNECTING`, or `LinkWireless::State::CONNECTED`).                                                                                                                                                                                                                                                                                 |
| `isConnected()`                           | **bool**                | Returns `true` if the player count is higher than `1`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `isSessionActive()`                       | **bool**                | Returns `true` if the state is `SERVING` or `CONNECTED`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `isServerClosed()`                       | **bool**                | Returns `true` if the server was closed with `closeServer()`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `playerCount()`                           | **u8** _(1~5)_          | Returns the number of connected players.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `currentPlayerId()`                       | **u8** _(0~4)_          | Returns the current player ID.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| `didQueueOverflow()`                       | **bool**          | Returns whether the internal receive queue lost messages at some point due to being full. This can happen if your queue size is too low, if you receive too much data without calling `receive(...)` enough times, or if excessive `receive(...)` calls prevent the ISR from copying data. After this call, the overflow flag is cleared if `clear` is `true` (default behavior).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| `getLastError([clear])`                   | **LinkWireless::Error** | If one of the other methods returns `false`, you can inspect this to know the cause. After this call, the last error is cleared if `clear` is `true` (default behavior).                                                                                                                                                                                                                                                                                                                                                               |
| `resetTimer()`                            | -                       | Restarts the send timer without disconnecting. Call this if you changed `config.interval`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |

⚠️ `0xFFFF` is a reserved value, so don't send it!

# 💻 LinkWirelessMultiboot

_(aka Multiboot through Wireless Adapter)_

[⬆️](#gba-link-connection) This tool allows sending Multiboot ROMs (small 256KiB programs that fit in EWRAM) from one GBA to up to 4 slaves, wirelessly, using a single cartridge.

Its demo (`LinkWirelessMultiboot_demo`) has all the other gba-link-connection ROMs bundled with it, so it can be used to quickly test the library.

https://github.com/afska/gba-link-connection/assets/1631752/9a648bff-b14f-4a85-92d4-ccf366adce2d

## Methods

| Name                                                                 | Return type                       | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| -------------------------------------------------------------------- | --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `sendRom(rom, romSize, gameName, userName, gameId, players, listener, [keepConnectionAlive])` | **LinkWirelessMultiboot::Result** | Sends the `rom`. The `players` must be the exact number of consoles that will download the ROM. Once this number of players is reached, the code will start transmitting the ROM bytes. During the process, the library will continuously invoke `listener` (passing a `LinkWirelessMultiboot::MultibootProgress` object as argument), and abort the transfer if it returns `true`. The `romSize` must be a number between `448` and `262144`. It's recommended to use a ROM size that is a multiple of `16`, since this also ensures compatibility with Multiboot via Link Cable. Once completed, the return value should be `LinkWirelessMultiboot::Result::SUCCESS`. You can start the transfer before the player count is reached by running `*progress.ready = true;` in the`cancel` callback. If a `keepConnectionAlive` is `true`, the adapter won't be reset after a successful transfer, so users can continue the session using `LinkWireless::restoreExistingConnection()`. |
| `reset()` | - | Turns off the adapter. It returns a boolean indicating whether the transition to low consumption mode was successful. |

# 🔧📻 LinkRawWireless

[⬆️](#gba-link-connection)

- This is a minimal hardware wrapper designed for the _Wireless Adapter_.
- It doesn't include any of the features of [📻 LinkWireless](#-LinkWireless), so it's not well suited for games.
- Its demo (`LinkRawWireless_demo`) can help emulator developers in enhancing accuracy.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/bc7e5a7d-a1bd-46a5-8318-98160c1229ae)

## Methods

- There's one method for every supported Wireless Adapter command:
  - `setup` = `0x17`
  - `getSystemStatus` = `0x13`
  - `broadcast` = `0x16`
  - `startHost` = `0x19`
  - `getSignalLevel` = `0x11`
  - `getSlotStatus` = `0x14`
  - `acceptConnections` = `0x1a`
  - `endHost` = `0x1b`
  - `broadcastReadStart` = `0x1c`
  - `broadcastReadPoll` = `0x1d`
  - `broadcastReadEnd` = `0x1e`
  - `connect` = `0x1f`
  - `keepConnecting` = `0x20`
  - `finishConnection` = `0x21`
  - `sendData` = `0x24`
  - `sendDataAndWait` = `0x25`
  - `receiveData` = `0x26`
  - `wait` = `0x27`
  - `bye` = `0x3d`
- Use `sendCommand(...)` to send arbitrary commands.
- Use `sendCommandAsync(...)` to send arbitrary commands asynchronously.
  - This requires setting `LINK_RAW_WIRELESS_ISR_SERIAL` as the `SERIAL` interrupt handler.
  - After calling this method, call `getAsyncState()` and `getAsyncCommandResult()`.
  - Do not call any other methods until the async state is `IDLE` again, or the adapter will desync!
- When sending arbitrary commands, the responses are not parsed. The exceptions are SendData and ReceiveData, which have these helpers:
  - `getSendDataHeaderFor(...)`
  - `getReceiveDataResponse(...)`

⚠️ advanced usage only; if you're building a game, use `LinkWireless`!

# 🔧🏛 LinkWirelessOpenSDK

[⬆️](#gba-link-connection) All first-party games, including the Multiboot 'bootloader' sent by the adapter, use an official software-level protocol. This class provides methods for creating and reading packets that adhere to this protocol. It's supposed to be used in conjunction with [🔧📻 LinkRawWireless](#-LinkRawWireless).

Additionally, there's a `LinkWirelessOpenSDK::MultiTransfer` class for file transfers, used by multiboot.

## Methods

| Name                                                                                  | Return type                                          | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| ------------------------------------------------------------------------------------- | ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `getChildrenData(response)`                                                           | **LinkWirelessOpenSDK::ChildrenData**                | Parses the `response` and returns a struct containing all the received packets from the connected clients.                                                                                                                                                                                                                                                                                                                                                                 |
| `getParentData(response)`                                                             | **LinkWirelessOpenSDK::ParentData**                  | Parses the `response` and returns a struct containing all the received packets from the host.                                                                                                                                                                                                                                                                                                                                                                              |
| `createServerBuffer(fullPayload, fullPayloadSize, sequence, [targetSlots], [offset])` | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the host to send a `fullPayload` with a valid header. If `fullPayloadSize` is higher than `84` (the maximum payload size), the buffer will only contain the **first** `84` bytes (unless an `offset` > 0 is used). A `sequence` number must be created by using `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`. Optionally, a `targetSlots` bit array can be used to exclude some clients from the transmissions (the default is `0b1111`). |
| `createServerACKBuffer(clientHeader, clientNumber)`                                   | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the host to acknowledge a header received from a certain `clientNumber`.                                                                                                                                                                                                                                                                                                                                                                              |
| `createClientBuffer(fullPayload, fullPayloadSize, sequence, [offset])`                | **LinkWirelessOpenSDK::SendBuffer<ClientSDKHeader>** | Creates a buffer for the client to send a `fullPayload` with a valid header. If `fullPayloadSize` is higher than `14` (the maximum payload size), the buffer will only contain the **first** `14` bytes (unless an `offset` > 0 is used). A `sequence` number must be created by using `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`.                                                                                                                           |
| `createClientACKBuffer(serverHeader)`                                                 | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the client to acknowledge a header received from the host.                                                                                                                                                                                                                                                                                                                                                                                            |

⚠️ advanced usage only; you only need this if you want to interact with N software!

# 🌎 LinkUniversal

[⬆️](#gba-link-connection) A multiuse library that doesn't care whether you plug a Link Cable or a Wireless Adapter. It continuously switches between both and tries to connect to other peers, supporting the hot swapping of cables and adapters and all the features from [👾 LinkCable](#-LinkCable) and [📻 LinkWireless](#-LinkWireless).

https://github.com/afska/gba-link-connection/assets/1631752/d1f49a48-6b17-4954-99d6-d0b7586f5730

## Constructor

`new LinkUniversal(...)` accepts these **optional** parameters:

| Name              | Type                               | Default                | Description                                                                                                                                                                                                                                                   |
| ----------------- | ---------------------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `protocol`        | **LinkUniversal::Protocol**        | `AUTODETECT`           | Specifies what protocol should be used (one of `LinkUniversal::Protocol::AUTODETECT`, `LinkUniversal::Protocol::CABLE`, `LinkUniversal::Protocol::WIRELESS_AUTO`, `LinkUniversal::Protocol::WIRELESS_SERVER`, `LinkUniversal::Protocol::WIRELESS_CLIENT`, or `LinkUniversal::Protocol::WIRELESS_RESTORE_EXISTING`). |
| `gameName`        | **const char\***                   | `""`                   | The game name that will be broadcasted in wireless sessions (max `14` characters). The string must be a null-terminated character array. The library uses this to only connect to servers from the same game.                                                 |
| `cableOptions`    | **LinkUniversal::CableOptions**    | _same as LinkCable_    | All the [👾 LinkCable](#-LinkCable) constructor parameters in one _struct_.                                                                                                                                                                                   |
| `wirelessOptions` | **LinkUniversal::WirelessOptions** | _same as LinkWireless_ | All the [📻 LinkWireless](#-LinkWireless) constructor parameters in one _struct_.                                                                                                                                                                             |
| `randomSeed`      | **int**                            | `123`                  | Random seed used for waits to prevent livelocks. If you use _libtonc_, pass `__qran_seed`.                                                                                                                                                                    |

You can also change these compile-time constants:

- `LINK_UNIVERSAL_MAX_PLAYERS`: to set a maximum number of players. The default value is `5`, but since LinkCable's limit is `4`, you might want to decrease it.
- `LINK_UNIVERSAL_GAME_ID_FILTER`: to restrict wireless connections to rooms with a specific game ID (`0x0000` ~ `0x7fff`). The default value (`0`) connects to any game ID and uses `0x7fff` when serving.

## Methods

The interface is the same as [👾 LinkCable](#-LinkCable). Additionally, it supports these methods:

| Name                    | Return type                 | Description                                                                                                                                                                                                                                        |
| ----------------------- | --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `getState()`            | **LinkUniversal::State**    | Returns the current state (one of `LinkUniversal::State::INITIALIZING`, `LinkUniversal::State::WAITING`, or `LinkUniversal::State::CONNECTED`).                                                                                                    |
| `getMode()`             | **LinkUniversal::Mode**     | Returns the active mode (one of `LinkUniversal::Mode::LINK_CABLE`, or `LinkUniversal::Mode::LINK_WIRELESS`).                                                                                                                                       |
| `getProtocol()`         | **LinkUniversal::Protocol** | Returns the active protocol (one of `LinkUniversal::Protocol::AUTODETECT`, `LinkUniversal::Protocol::CABLE`, `LinkUniversal::Protocol::WIRELESS_AUTO`, `LinkUniversal::Protocol::WIRELESS_SERVER`, `LinkUniversal::Protocol::WIRELESS_CLIENT`, or `LinkUniversal::Protocol::WIRELESS_RESTORE_EXISTING`). |
| `getWirelessState()`    | **LinkWireless::State**     | Returns the wireless state (same as [📻 LinkWireless](#-LinkWireless)'s `getState()`).                                                                                                                                                             |
| `setProtocol(protocol)` | -                           | Sets the active `protocol`.                                                                                                                                                                                                                        |
| `getLinkCable()`          | **LinkCable\***                           | Returns the internal `LinkCable` instance (for advanced usage).                                                                                                                                                                                              |
| `getLinkWireless()`          | **LinkWireless\***                           | Returns the internal `LinkWireless` instance (for advanced usage).                                                                                                                                                                                              |

# 🔌 LinkGPIO

_(aka General Purpose Mode)_

[⬆️](#gba-link-connection) This is the default Link Port mode, and it allows users to manipulate pins `SI`, `SO`, `SD` and `SC` directly.

![photo](https://github.com/afska/gba-link-connection/assets/1631752/b53ddc3d-46c5-441b-9036-489150d9de9f)

## Methods

| Name                         | Return type             | Description                                                                                                     |
| ---------------------------- | ----------------------- | --------------------------------------------------------------------------------------------------------------- |
| `reset()`                    | -                       | Resets communication mode to General Purpose (same as `Link::reset()`). **Required to initialize the library!** |
| `setMode(pin, direction)`    | -                       | Configures a `pin` to use a `direction` (input or output).                                                      |
| `getMode(pin)`               | **LinkGPIO::Direction** | Returns the direction set at `pin`.                                                                             |
| `readPin(pin)`               | **bool**                | Returns whether a `pin` is _HIGH_ or not (when set as an input).                                                |
| `writePin(pin, isHigh)`      | -                       | Sets a `pin` to be high or not (when set as an output).                                                         |
| `setSIInterrupts(isEnabled)` | -                       | If it `isEnabled`, an IRQ will be generated when `SI` changes from _HIGH_ to _LOW_.                             |

⚠️ always set the `SI` terminal to an input!

⚠️ call `reset()` when you finish doing GPIO stuff! (for compatibility with the other libraries)

# 🔗 LinkSPI

_(aka Normal Mode)_

[⬆️](#gba-link-connection) This is the GBA's implementation of SPI. You can use this to interact with other GBAs or computers that know SPI.

![screenshot](https://user-images.githubusercontent.com/1631752/213068614-875049f6-bb01-41b6-9e30-98c73cc69b25.png)

## Methods

| Name                            | Return type             | Description                                                                                                                                                                                                                                |
| ------------------------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `isActive()`                    | **bool**                | Returns whether the library is active or not.                                                                                                                                                                                              |
| `activate(mode, [dataSize])`    | -                       | Activates the library in a specific `mode` (one of `LinkSPI::Mode::SLAVE`, `LinkSPI::Mode::MASTER_256KBPS`, or `LinkSPI::Mode::MASTER_2MBPS`). By default, the `dataSize` is 32-bit, but can be changed to `LinkSPI::DataSize::SIZE_8BIT`. |
| `deactivate()`                  | -                       | Deactivates the library.                                                                                                                                                                                                                   |
| `transfer(data)`                | **u32**                 | Exchanges `data` with the other end. Returns the received data.                                                                                                                                                                            |
| `transfer(data, cancel)`        | **u32**                 | Like `transfer(data)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.                                                                                             |
| `transferAsync(data, [cancel])` | -                       | Schedules a `data` transfer and returns. After this, call `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the async data, normal `transfer(...)`s won't do anything!                                                  |
| `getAsyncState()`               | **LinkSPI::AsyncState** | Returns the state of the last async transfer (one of `LinkSPI::AsyncState::IDLE`, `LinkSPI::AsyncState::WAITING`, or `LinkSPI::AsyncState::READY`).                                                                                        |
| `getAsyncData()`                | **u32**                 | If the async state is `READY`, returns the remote data and switches the state back to `IDLE`. If not, returns an empty response.                                                                                                           |
| `getMode()`                     | **LinkSPI::Mode**       | Returns the current `mode`.                                                                                                                                                                                                                |
| `getDataSize()`                 | **LinkSPI::DataSize**   | Returns the current `dataSize`.                                                                                                                                                                                                            |
| `setWaitModeActive(isActive)`   | -                       | Enables or disables `waitMode` (\*).                                                                                                                                                                                                       |
| `isWaitModeActive()`            | **bool**                | Returns whether `waitMode` (\*) is active or not.                                                                                                                                                                                          |

> (\*) `waitMode`: The GBA adds an extra feature over SPI. When working as master, it can check whether the other terminal is ready to receive (ready: `MISO=LOW`), and wait if it's not (not ready: `MISO=HIGH`). That makes the connection more reliable, but it's not always supported on other hardware units (e.g. the Wireless Adapter), so it must be disabled in those cases.
>
> `waitMode` is disabled by default.
>
> `MISO` means `SO` on the slave side and `SI` on the master side.

⚠️ when using Normal Mode between two GBAs, use a GBC Link Cable!

⚠️ only use the 2Mbps mode with custom hardware (very short wires)!

⚠️ returns `0xFFFFFFFF` (or `0xFF`) on misuse or cancelled transfers!

## SPI Configuration

The GBA operates using **SPI mode 3** (`CPOL=1, CPHA=1`). Here's a connection diagram that illustrates how to connect a Link Cable to a Raspberry Pi 3's SPI pins:

<table>
  <tr>
    <td>
      <p>
        <img src="https://github.com/afska/gba-link-connection/assets/1631752/a5fffad6-3aef-4f81-8d6c-ae4e99c2d5b4" alt="pinout">
      </p>
    </td>
    <td>
      <p>
        <img src="https://github.com/afska/gba-link-connection/assets/1631752/203dc766-e316-4d92-a4b7-bc2264bffe71" alt="rpigba">
      </p>
    </td>
  </tr>
</table>

# ⏱️ LinkUART

_(aka UART Mode)_

[⬆️](#gba-link-connection) This is the GBA's implementation of UART. You can use this to interact with a PC using a _USB to UART cable_. You can change the buffer size by setting the compile-time constant `LINK_UART_QUEUE_SIZE`.

![photo](https://github.com/afska/gba-link-connection/assets/1631752/2ca8abb8-1a38-40bb-bf7d-bf29a0f880cd)

## Methods

| Name                                           | Return type | Description                                                                                                                                                                                                                  |
| ---------------------------------------------- | ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`                                   | **bool**    | Returns whether the library is active or not.                                                                                                                                                                                |
| `activate(baudRate, dataSize, parity, useCTS)` | -           | Activates the library using a specific UART mode. _Defaults: 9600bps, 8-bit data, no parity bit, no CTS_.                                                                                                                    |
| `deactivate()`                                 | -           | Deactivates the library.                                                                                                                                                                                                     |
| `sendLine(string)`                             | -           | Takes a null-terminated `string`, and sends it followed by a `'\n'` character. The null character is not sent.                                                                                                               |
| `sendLine(data, cancel)`                       | -           | Like `sendLine(string)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.                                                                             |
| `readLine(string, [limit])`                    | **bool**    | Reads characters into `string` until finding a `'\n'` character or a character `limit` is reached. A null terminator is added at the end. Returns `false` if the limit has been reached without finding a newline character. |
| `readLine(string, cancel, [limit])`            | **bool**    | Like `readLine(string, [limit])` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.                                                                    |
| `send(buffer, size, offset)`                   | -           | Sends `size` bytes from `buffer`, starting at byte `offset`.                                                                                                                                                                 |
| `read(buffer, size, offset)`                   | **u32**     | Tries to read `size` bytes into `(u8*)(buffer + offset)`. Returns the number of read bytes.                                                                                                                                  |
| `canRead()`                                    | **bool**    | Returns whether there are bytes to read or not.                                                                                                                                                                              |
| `canSend()`                                    | **bool**    | Returns whether there is room to send new messages or not.                                                                                                                                                                   |
| `availableForRead()`                           | **u32**     | Returns the number of bytes available for read.                                                                                                                                                                              |
| `availableForSend()`                           | **u32**     | Returns the number of bytes available for send (buffer size - queued bytes).                                                                                                                                                 |
| `read()`                                       | **u8**      | Reads a byte. Returns 0 if nothing is found.                                                                                                                                                                                 |
| `send(data)`                                   | -           | Sends a `data` byte.                                                                                                                                                                                                         |

## UART Configuration

The GBA operates using `1` stop bit, but everything else can be configured. By default, the library uses `8N1`, which means 8-bit data and no parity bit. RTS/CTS is disabled by default.

![diagram](https://github.com/afska/gba-link-connection/assets/1631752/a6a58f94-da24-4fd9-9603-9c7c9a493f93)

- Black wire (`GND`) -> GBA `GND`.
- Green wire (`TX`) -> GBA `SI`.
- White wire (`RX`) -> GBA `SO`.

# 🟪 LinkCube

_(aka JOYBUS Mode)_

[⬆️](#gba-link-connection) This is the GBA's implementation of JOYBUS, in which users connect the console to a _GameCube_ (or _Wii_ with GC ports) using an official adapter. The library can be tested using _Dolphin/mGBA_ and [gba-joybus-tester](https://github.com/afska/gba-joybus-tester).

![screenshot](https://github.com/user-attachments/assets/93c11c9a-bdbf-4726-a070-895465739789)

You can change these compile-time constants:

- `LINK_CUBE_QUEUE_SIZE`: to set a custom buffer size (how many incoming and outgoing values the queues can store at max). The default value is `10`, which seems fine for most games.
  - This affects how much memory is allocated. With the default value, it's around `120` bytes. There's a double-buffered pending queue (to avoid data races), and 1 outgoing queue.
  - You can approximate the memory usage with:
    - `LINK_CUBE_QUEUE_SIZE * sizeof(u32) * 3` <=> `LINK_CUBE_QUEUE_SIZE * 12`

## Methods

| Name                | Return type | Description                                                                                                                                        |
| ------------------- | ----------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`        | **bool**    | Returns whether the library is active or not.                                                                                                      |
| `activate()`        | -           | Activates the library.                                                                                                                             |
| `deactivate()`      | -           | Deactivates the library.                                                                                                                           |
| `wait()`            | **bool**    | Waits for data. Returns `true` on success, or `false` on JOYBUS reset.                                                                             |
| `wait(cancel)`      | **bool**    | Like `wait()` but accepts a `cancel()` function. The library will invoke it after every SERIAL interrupt, and abort the wait if it returns `true`. |
| `canRead()`         | **bool**    | Returns `true` if there are pending received values to read.                                                                                       |
| `read()`            | **u32**     | Dequeues and returns the next received value. If there's no received data, a `0` will be returned.                                                 |
| `peek()`            | **u32**     | Returns the next received value without dequeuing it. If there's no received data, a `0` will be returned.                                         |
| `send(data)`        | -           | Sends 32-bit `data`. If the other end asks for data at the same time you call this method, a `0x00000000` will be sent.                            |
| `pendingCount()`    | **u32**     | Returns the number of pending outgoing transfers.                                                                                                  |
| `didQueueOverflow([clear])`    | **bool**     | Returns whether the internal receive queue lost messages at some point due to being full. This can happen if your queue size is too low, if you receive too much data without calling `read(...)` enough times, or if excessive `read(...)` calls prevent the ISR from copying data. After this call, the overflow flag is cleared if `clear` is `true` (default behavior).                                                                                                 |
| `didReset([clear])` | **bool**    | Returns whether a JOYBUS reset was requested or not. After this call, the reset flag is cleared if `clear` is `true` (default behavior).           |

# 📱 LinkMobile

_(aka Mobile Adapter GB)_

[⬆️](#gba-link-connection) This is a driver for an accessory that enables online connectivity on the GB and GBA. The protocol was reverse-engineered by the _REON Team_.

The original accessory was sold in Japan only and using it nowadays is hard since it relies on old tech, but REON has created an open-source implementation called [libmobile](https://github.com/REONTeam/libmobile), as well as support for emulators and microcontrollers.

It has two modes of operation:

- Direct call (P2P): Calling someone directly for a 2-player session, using the other person's IP address or the phone number provided by the [relay server](https://github.com/REONTeam/mobile-relay).
- ISP call (PPP): Calling an ISP number for internet access. In this mode, the adapter can open up to 2 TCP/UDP sockets and transfer arbitrary data.

![screenshot](https://github.com/user-attachments/assets/fcc1488b-4955-4a1b-8ffa-dd660175a45c)

## Constructor

`new LinkMobile(...)` accepts these **optional** parameters:

| Name      | Type           | Default | Description                                                            |
| --------- | -------------- | ------- | ---------------------------------------------------------------------- |
| `timeout` | **u32**        | `600`   | Number of _frames_ without completing a request to reset a connection. |
| `timerId` | **u8** _(0~3)_ | `3`     | GBA Timer to use for sending.                                          |

You can update these values at any time without creating a new instance:

- Call `deactivate()`.
- Mutate the `config` property.
- Call `activate()`.

You can also change these compile-time constants:

- `LINK_MOBILE_QUEUE_SIZE`: to set a custom request queue size (how many commands can be queued at the same time). The default value is `10`, which seems fine for most games.
  - This affects how much memory is allocated. With the default value, it's around `3` KB.

## Methods

- All actions are asynchronous/nonblocking. That means, they will return `true` if nothing is awfully wrong, but the actual consequence will occur some frames later. You can call `getState()` at any time to know what it's doing.
- On fatal errors, the library will transition to a `NEEDS_RESET` state. In that case, you can call `getError()` to know more details on what happened, and then `activate()` to restart.
- When calling `deactivate()`, the adapter automatically turns itself off after `3` seconds of inactivity. However, to gracefully turn it off, it's recommended to call `shutdown()` first, wait until the state is `SHUTDOWN`, and then `deactivate()`.

| Name                                           | Return type           | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ---------------------------------------------- | --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `isActive()`                                   | **bool**              | Returns whether the library is active or not.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `activate()`                                   | -                     | Activates the library. After some time, if an adapter is connected, the state will be changed to `SESSION_ACTIVE`. If not, the state will be `NEEDS_RESET`, and you can retrieve the error with `getError()`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `deactivate()`                                 | -                     | Deactivates the library, resetting the serial mode to GPIO. Calling `shutdown()` first is recommended, but the adapter will put itself in sleep mode after 3 seconds anyway.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| `shutdown()`                                   | **bool**              | Gracefully shuts down the adapter, closing all connections. After some time, the state will be changed to `SHUTDOWN`, and only then it's safe to call `deactivate()`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `call(phoneNumber)`                            | **bool**              | Initiates a P2P connection with a `phoneNumber`. After some time, the state will be `CALL_ESTABLISHED` (or `ACTIVE_SESSION` if the connection fails or ends). In REON/libmobile the phone number can be a number assigned by the relay server, or a 12-digit IPv4 address (for example, `"127000000001"` would be `127.0.0.1`).                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| `callISP(password, loginId)`                   | **bool**              | Calls the ISP number registered in the adapter configuration, or a default number if the adapter hasn't been configured. Then, performs a login operation using the provided `password` and `loginId`. After some time, the state will be `PPP_ACTIVE`. If `loginId` is empty and the adapter has been configured, it will use the one stored in the configuration. Both parameters are null-terminated strings (max `32` characters).                                                                                                                                                                                                                                                                                                                              |
| `dnsQuery(domainName, result)`                 | **bool**              | Looks up the IPv4 address for a `domainName` (a null-terminated string, max `253` characters). It also accepts an ASCII IPv4 address, converting it into a 4-byte address instead of querying the DNS server. The `result` is a pointer to a `LinkMobile::DNSQuery` struct that will be filled with the result. When the request is completed, the `completed` field will be `true`. If an IP address was found, the `success` field will be `true` and the `ipv4` field can be read as a 4-byte address.                                                                                                                                                                                                                                                           |
| `openConnection(ip, port, type, result)`       | **bool**              | Opens a TCP/UDP (`type`) connection at the given `ip` (4-byte address) on the given `port`. The `result` is a pointer to a `LinkMobile::OpenConn` struct that will be filled with the result. When the request is completed, the `completed` field will be `true`. If the connection was successful, the `success` field will be `true` and the `connectionId` field can be used when calling the `transfer(...)` method. Only `2` connections can be opened at the same time.                                                                                                                                                                                                                                                                                      |
| `closeConnection(connectionId, type, result)`  | **bool**              | Closes an active TCP/UDP (`type`) connection. The `result` is a pointer to a `LinkMobile::CloseConn` struct that will be filled with the result. When the request is completed, the `completed` field will be `true`. If the connection was closed correctly, the `success` field will be `true`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| `transfer(dataToSend, result, [connectionId])` | **bool**              | Requests a data transfer (up to `254` bytes) and responds the received data. The transfer can be done with the other node in a P2P connection, or with any open TCP/UDP connection if a PPP session is active. In the case of a TCP/UDP connection, the `connectionId` must be provided. The `result` is a pointer to a `LinkMobile::DataTransfer` struct that will be filled with the received data. It can also point to `dataToSend` to reuse the struct. When the request is completed, the `completed` field will be `true`. If the transfer was successful, the `success` field will be `true`. If not, you can assume that the connection was closed.                                                                                                        |
| `waitFor(asyncRequest)`                        | **bool**              | Waits for `asyncRequest` to be completed. Returns `true` if the request was completed && successful, and the adapter session is still alive. Otherwise, it returns `false`. The `asyncRequest` is a pointer to a `LinkMobile::DNSQuery`, `LinkMobile::OpenConn`, `LinkMobile::CloseConn`, or `LinkMobile::DataTransfer`.                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `hangUp()`                                     | **bool**              | Hangs up the current P2P or PPP call. Closes all connections.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `readConfiguration(configurationData)`         | **bool**              | Retrieves the adapter configuration, and puts it in the `configurationData` struct. If the adapter has an active session, the data is already loaded, so it's instantaneous.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| `getState()`                                   | **LinkMobile::State** | Returns the current state (one of `LinkMobile::State::NEEDS_RESET`, `LinkMobile::State::PINGING`, `LinkMobile::State::WAITING_TO_START`, `LinkMobile::State::STARTING_SESSION`, `LinkMobile::State::ACTIVATING_SIO32`, `LinkMobile::State::WAITING_32BIT_SWITCH`, `LinkMobile::State::READING_CONFIGURATION`, `LinkMobile::State::SESSION_ACTIVE`, `LinkMobile::State::CALL_REQUESTED`, `LinkMobile::State::CALLING`, `LinkMobile::State::CALL_ESTABLISHED`, `LinkMobile::State::ISP_CALL_REQUESTED`, `LinkMobile::State::ISP_CALLING`, `LinkMobile::State::PPP_LOGIN`, `LinkMobile::State::PPP_ACTIVE`, `LinkMobile::State::SHUTDOWN_REQUESTED`, `LinkMobile::State::ENDING_SESSION`, `LinkMobile::State::WAITING_8BIT_SWITCH`, or `LinkMobile::State::SHUTDOWN`). |
| `getRole()`                                    | **LinkMobile::Role**  | Returns the current role in the P2P connection (one of `LinkMobile::Role::NO_P2P_CONNECTION`, `LinkMobile::Role::CALLER`, or `LinkMobile::Role::RECEIVER`).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| `isConfigurationValid()`                       | **int**               | Returns whether the adapter has been configured or not. Returns `1` = yes, `0` = no, `-1` = unknown (no session active).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `isConnectedP2P()`                             | **bool**              | Returns `true` if a P2P call is established (the state is `CALL_ESTABLISHED`).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| `isConnectedPPP()`                             | **bool**              | Returns `true` if a PPP session is active (the state is `PPP_ACTIVE`).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| `isSessionActive()`                            | **bool**              | Returns `true` if the session is active.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `canShutdown()`                                | **bool**              | Returns `true` if there's an active session and there's no previous shutdown requests.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| `getDataSize()`                                | **LinkSPI::DataSize** | Returns the current operation mode (`LinkSPI::DataSize`).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| `getError()`                                   | **LinkMobile::Error** | Returns details about the last error that caused the connection to be aborted.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |

# 🖱️ LinkPS2Mouse

[⬆️](#gba-link-connection) A PS/2 mouse driver for the GBA. Use it to add mouse support to your homebrew games. It's a straight port from [this library](https://github.com/kristopher/PS2-Mouse-Arduino).

![photo](https://github.com/afska/gba-link-connection/assets/1631752/6856ff0d-0f06-4a9d-8ded-280052e02b8d)

## Constructor

`new LinkPS2Mouse(timerId)`, where `timerId` is the GBA Timer used for delays.

## Methods

| Name              | Return type | Description                                                                                                                                                                                                                                                                                |
| ----------------- | ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `isActive()`      | **bool**    | Returns whether the library is active or not.                                                                                                                                                                                                                                              |
| `activate()`      | -           | Activates the library.                                                                                                                                                                                                                                                                     |
| `deactivate()`    | -           | Deactivates the library.                                                                                                                                                                                                                                                                   |
| `report(data[3])` | -           | Fills the `data` int array with a report. The first int contains _clicks_ that you can check against the bitmasks `LINK_PS2_MOUSE_LEFT_CLICK`, `LINK_PS2_MOUSE_MIDDLE_CLICK`, and `LINK_PS2_MOUSE_RIGHT_CLICK`. The second int is the _X movement_, and the third int is the _Y movement_. |

⚠️ calling `activate()` or `report(...)` could freeze the system if nothing is connected: detecting timeouts using interrupts is the user's responsibility!

## Pinout

```
 ____________
|PS/2 --- GBA|
|------------|
|CLOCK -> SI |
|DATA --> SO |
|VCC ---> VCC|
|GND ---> GND|
```

# ⌨️ LinkPS2Keyboard

[⬆️](#gba-link-connection) A PS/2 keyboard driver for the GBA. Use it to add keyboard support to your homebrew games.

![photo](https://github.com/afska/gba-link-connection/assets/1631752/4c5fa3ed-5d96-45fe-ad24-73bc3f71c63f)

## Constructor

`new LinkPS2Keyboard(onEvent)`, where `onEvent` is a function pointer that will receive the scan codes (`u8`). You should check a PS/2 scan code list online, but most common keys/events are included in enums like `LINK_PS2_KEYBOARD_KEY::ENTER` and `LINK_PS2_KEYBOARD_EVENT::RELEASE`.

## Methods

| Name           | Return type | Description                                   |
| -------------- | ----------- | --------------------------------------------- |
| `isActive()`   | **bool**    | Returns whether the library is active or not. |
| `activate()`   | -           | Activates the library.                        |
| `deactivate()` | -           | Deactivates the library.                      |

## Pinout

```
 ____________
|PS/2 --- GBA|
|------------|
|CLOCK -> SI |
|DATA --> SO |
|VCC ---> VCC|
|GND ---> GND|
```
