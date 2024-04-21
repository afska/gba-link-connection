# gba-link-connection

A set of Game Boy Advance (GBA) C++ libraries to interact with the Serial Port. Its main purpose is to provide multiplayer support to homebrew games.

- [👾](#-LinkCable) [LinkCable.hpp](lib/LinkCable.hpp): The classic 16-bit **Multi-Play mode** (up to 4 players) using a GBA Link Cable!
  - [💻](#-LinkCableMultiboot) [LinkCableMultiboot.hpp](lib/LinkCableMultiboot.hpp): ‍Send **Multiboot software** (small 256KiB ROMs) to other GBAs with no cartridge!
  - [🔧👾](#-LinkRawCable) [LinkRawCable.hpp](lib/LinkRawCable.hpp): A **minimal** low-level API for the 16-bit Multi-Play mode.
- [📻](#-LinkWireless) [LinkWireless.hpp](lib/LinkWireless.hpp): Connect up to 5 consoles with the **Wireless Adapter**!
  - [📡](#-LinkWirelessMultiboot) [LinkWirelessMultiboot.hpp](lib/LinkWirelessMultiboot.hpp): ‍Send Multiboot software (small 256KiB ROMs) to other GBAs **over the air**!
  - [🔧📻](#-LinkRawWireless) [LinkRawWireless.hpp](lib/LinkRawWireless.hpp): A **minimal** low-level API for the Wireless Adapter.
  - [🔧🏛️](#-LinkWirelessOpenSDK) [LinkWirelessOpenSDK.hpp](lib/LinkWirelessOpenSDK.hpp): An abstraction of the **official** software level protocol of the Wireless Adapter.
- [🌎](#-LinkUniversal) [LinkUniversal.hpp](lib/LinkUniversal.hpp): Add multiplayer support to your game, both with 👾 *Link Cables* and 📻 *Wireless Adapters*, using the **same API**!
- [🔌](#-LinkGPIO) [LinkGPIO.hpp](lib/LinkGPIO.hpp): Use the Link Port however you want to control **any device** (like LEDs, rumble motors, and that kind of stuff)!
- [🔗](#-LinkSPI) [LinkSPI.hpp](lib/LinkSPI.hpp): Connect with a PC (like a **Raspberry Pi**) or another GBA (with a GBC Link Cable) using this mode. Transfer up to 2Mbit/s!
- [⏱️](#-LinkUART) [LinkUART.hpp](lib/LinkUART.hpp): Easily connect to **any PC** using a USB to UART cable!

*(click on the emojis for documentation)*

> <img alt="rlabs" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116227197-400d2380-a72a-11eb-9e7b-389aae76f13e.png" /> Created by [[r]labs](https://r-labs.io).

> 💬 Check out my other GBA projects: [piuGBA](https://github.com/afska/piugba), [gba-remote-play](https://github.com/afska/gba-remote-play)

## Usage

- Include the library you want (e.g. [LinkCable.hpp](lib/LinkCable.hpp)) in your game code, and refer to its comments for instructions. Most of these libraries are provided as single header files for simplicity. The only external dependency is **libtonc**, which comes preinstalled with *devkitPro*.
- Check out the [examples](examples) folder.
	* Builds are available in [Releases](https://github.com/afska/gba-link-connection/releases).
	* They can be tested on real GBAs or using emulators.
	* For `LinkCable`/`LinkWireless`/`LinkUniversal` there are stress tests that you can use to tweak your configuration.

To learn implementation details, you might also want to check out the [docs](docs) folder, which contains important documentation.

### Makefile actions (for all examples)

```bash
make [ clean | build | start | rebuild | restart ]
```

# 👾 LinkCable

*(aka Multi-Play Mode)*

This is the Link Port mode that games use for multiplayer.

The library uses message queues to send/receive data and transmits when it's possible. As it uses CPU interrupts, the connection is alive even if a console drops a frame or gets stuck in a long iteration loop. After such an event, all nodes end up receiving all the pending messages.

![screenshot](https://user-images.githubusercontent.com/1631752/99154109-1d131980-268c-11eb-86b1-7a728f639e5e.png)

## Constructor

`new LinkCable(...)` accepts these **optional** parameters:

Name | Type | Default | Description
--- | --- | --- | ---
`baudRate` | **BaudRate** | `BAUD_RATE_1` | Sets a specific baud rate.
`timeout` | **u32** | `3` | Number of *frames* without an `II_SERIAL` IRQ to reset the connection.
`remoteTimeout` | **u32** | `5` | Number of *messages* with `0xFFFF` to mark a player as disconnected.
`interval` | **u16** | `50` | Number of *1024-cycle ticks* (61.04μs) between transfers *(50 = 3.052ms)*. It's the interval of Timer #`sendTimerId`. Lower values will transfer faster but also consume more CPU.
`sendTimerId` | **u8** *(0~3)* | `3` | GBA Timer to use for sending.

You can update these values at any time without creating a new instance:
- Call `deactivate()`.
- Mutate the `config` property.
- Call `activate()`.

You can also change these compile-time constants:
- `LINK_CABLE_QUEUE_SIZE`: to set a custom buffer size (how many incoming and outgoing messages the queues can store at max **per player**). The default value is `15`, which seems fine for most games.

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate()` | - | Activates the library.
`deactivate()` | - | Deactivates the library.
`isConnected()` | **bool** | Returns `true` if there are at least 2 connected players.
`playerCount()` | **u8** *(0~4)* | Returns the number of connected players.
`currentPlayerId()` | **u8** *(0~3)* | Returns the current player id.
`sync()` | - | Call this method every time you need to fetch new data.
`waitFor(playerId)` | **bool** | Waits for data from player #`playerId`. Returns `true` on success, or `false` on disconnection.
`waitFor(playerId, cancel)` | **bool** | Like `waitFor(playerId)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the wait if it returns `true`.
`canRead(playerId)` | **bool** | Returns `true` if there are pending messages from player #`playerId`. Keep in mind that if this returns `false`, it will keep doing so until you *fetch new data* with `sync()`.
`read(playerId)` | **u16** | Dequeues and returns the next message from player #`playerId`.
`peek(playerId)` | **u16** | Returns the next message from player #`playerId` without dequeuing it.
`send(data)` | - | Sends `data` to all connected players.

⚠️ `0xFFFF` and `0x0` are reserved values, so don't send them!

# 💻 LinkCableMultiboot

*(aka Multiboot through Multi-Play Mode)*

This tool allows sending Multiboot ROMs (small 256KiB programs that fit in EWRAM) from one GBA to up to 3 slaves, using a single cartridge.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/6ff55944-5437-436f-bcc7-a89b05dc5486)

## Methods

Name | Return type | Description
--- | --- | ---
`sendRom(rom, romSize, cancel)` | **LinkCableMultiboot::Result** | Sends the `rom`. During the handshake process, the library will continuously invoke `cancel`, and abort the transfer if it returns `true`. The `romSize` must be a number between `448` and `262144`, and a multiple of `16`. Once completed, the return value should be `LinkCableMultiboot::Result::SUCCESS`.

⚠️ for better results, turn on the GBAs **after** calling the `sendRom` method!

# 🔧👾 LinkRawCable

- This is a minimal hardware wrapper designed for the *Multi-Play mode*.
- It doesn't include any of the features of [👾 LinkCable](#-LinkCable), so it's not well suited for games.
- Its demo (`LinkRawCable_demo`) can help emulator developers in enhancing accuracy.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/29a25bf7-211e-49d6-a32a-566c72a44973)

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate(baudRate = BAUD_RATE_1)` | - | Activates the library in a specific `baudRate` (`LinkRawCable::BaudRate`).
`deactivate()` | - | Deactivates the library.
`transfer(data)` | **LinkRawCable::Response** | Exchanges `data` with the connected consoles. Returns the received data, including the assigned player id.
`transfer(data, cancel)` | **LinkRawCable::Response** | Like `transfer(data)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`transferAsync(data)` | - | Schedules a `data` transfer and returns. After this, call `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the async data, normal `transfer(...)`s won't do anything!
`getAsyncState()` | **LinkRawCable::AsyncState** | Returns the state of the last async transfer (one of `LinkRawCable::AsyncState::IDLE`, `LinkRawCable::AsyncState::WAITING`, or `LinkRawCable::AsyncState::READY`).
`getAsyncData()` | **LinkRawCable::Response** | If the async state is `READY`, returns the remote data and switches the state back to `IDLE`.
`isMaster()` | **bool** | Returns whether the console is connected as master or not. Returns garbage when the cable is not properly connected.
`isReady()` | **bool** | Returns whether all connected consoles have entered the multiplayer mode. Returns garbage when the cable is not properly connected.
`getBaudRate()` | **LinkRawCable::BaudRate** | Returns the current `baudRate`.

- don't send `0xFFFF`, it's a reserved value that means *disconnected client*
- only `transfer(...)` if `isReady()`

# 📻 LinkWireless

*(aka GBA Wireless Adapter)*

This is a driver for an accessory that enables wireless games up to 5 players. The inner workings of the adapter are highly unknown, but [this blog post](docs/wireless_adapter.md) is very helpful. I've updated it to add more details about the things I learnt by the means of ~~reverse engineering~~ brute force and trial&error.

The library, by default, implements a lightweight protocol (on top of the adapter's message system) that sends packet IDs and checksums. This allows detecting disconnections, forwarding messages to all nodes, and retransmitting to prevent packet loss.

https://github.com/afska/gba-link-connection/assets/1631752/7eeafc49-2dfa-4902-aa78-57b391720564

## Constructor

`new LinkWireless(...)` accepts these **optional** parameters:

Name | Type | Default | Description
--- | --- | --- | ---
`forwarding` | **bool** | `true` | If `true`, the server forwards all messages to the clients. Otherwise, clients only see messages sent from the server (ignoring other peers).
`retransmission` | **bool** | `true` | If `true`, the library handles retransmission for you, so there should be no packet loss.
`maxPlayers` | **u8** *(2~5)* | `5` | Maximum number of allowed players. The adapter will accept connections after reaching the limit, but the library will ignore them. If your game only supports -for example- two players, set this to `2` as it will make transfers faster.
`timeout` | **u32** | `10` | Number of *frames* without receiving *any* data to reset the connection.
`remoteTimeout` | **u32** | `10` | Number of *successful transfers* without a message from a client to mark the player as disconnected.
`interval` | **u16** | `50` | Number of *1024-cycle ticks* (61.04μs) between transfers *(50 = 3.052ms)*. It's the interval of Timer #`sendTimerId`. Lower values will transfer faster but also consume more CPU.
`sendTimerId` | **u8** *(0~3)* | `3` | GBA Timer to use for sending.
`asyncACKTimerId` | **s8** *(0~3 or -1)* | `-1` | GBA Timer to use for ACKs. If you have free timers, use one here to reduce CPU usage.

You can update these values at any time without creating a new instance:
- Call `deactivate()`.
- Mutate the `config` property.
- Call `activate()`.

You can also change these compile-time constants:
- `LINK_WIRELESS_QUEUE_SIZE`: to set a custom buffer size (how many incoming and outgoing messages the queues can store at max). The default value is `30`, which seems fine for most games.
- `LINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH` and `LINK_WIRELESS_MAX_CLIENT_TRANSFER_LENGTH`: to set the biggest allowed transfer per timer tick. Transfers contain retransmission headers and multiple user messages. These values must be in the range `[6;20]` for servers and `[2;4]` for clients. The default values are `20` and `4`, but you might want to set them a bit lower to reduce CPU usage.
- `LINK_WIRELESS_PUT_ISR_IN_IWRAM`: to put critical functions (~3.5KB) in IWRAM, which can significantly improve performance due to its faster access. This is disabled by default to conserve IWRAM space, which is limited, but it's enabled in demos to showcase its performance benefits.
- `LINK_WIRELESS_USE_SEND_RECEIVE_LATCH`: to alternate between sends and receives on each timer tick (instead of doing both things). This is disabled by default. Enabling it will introduce some latency but reduce overall CPU usage.

## Methods

- Most of these methods return a boolean, indicating if the action was successful. If not, you can call `getLastError()` to know the reason. Usually, unless it's a trivial error (like buffers being full), the connection with the adapter is reset and the game needs to start again.
- You can check the connection state at any time with `getState()`.
- Until a session starts, all actions are synchronic.
- During sessions (when the state is `SERVING` or `CONNECTED`), the message transfers are IRQ-driven, so `send(...)` and `receive(...)` won't waste extra cycles.

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate()` | **bool** | Activates the library. When an adapter is connected, it changes the state to `AUTHENTICATED`. It can also be used to disconnect or reset the adapter.
`deactivate()` | **bool** | Puts the adapter into a low consumption mode and then deactivates the library. It returns a boolean indicating whether the transition to low consumption mode was successful.
`serve([gameName], [userName], [gameId])` | **bool** | Starts broadcasting a server and changes the state to `SERVING`. You can, optionally, provide a `gameName` (max `14` characters), a `userName` (max `8` characters), and a `gameId` *(0 ~ 0x7FFF)* that games will be able to read. The strings must be null-terminated character arrays.  If the adapter is already serving, this method only updates the broadcast data.
`getServers(servers, [onWait])` | **bool** | Fills the `servers` array with all the currently broadcasting servers. This action takes 1 second to complete, but you can optionally provide an `onWait()` function which will be invoked each time VBlank starts.
`getServersAsyncStart()` | **bool** | Starts looking for broadcasting servers and changes the state to `SEARCHING`. After this, call `getServersAsyncEnd(...)` 1 second later.
`getServersAsyncEnd(servers)` | **bool** | Fills the `servers` array with all the currently broadcasting servers. Changes the state to `AUTHENTICATED` again.
`connect(serverId)` | **bool** | Starts a connection with `serverId` and changes the state to `CONNECTING`.
`keepConnecting()` | **bool** | When connecting, this needs to be called until the state is `CONNECTED`. It assigns a player id. Keep in mind that `isConnected()` and `playerCount()` won't be updated until the first message from server arrives.
`send(data)` | **bool** | Enqueues `data` to be sent to other nodes.
`receive(messages)` | **bool** | Fills the `messages` array with incoming messages, forwarding if needed.
`getState()` | **LinkWireless::State** | Returns the current state (one of `LinkWireless::State::NEEDS_RESET`, `LinkWireless::State::AUTHENTICATED`, `LinkWireless::State::SEARCHING`, `LinkWireless::State::SERVING`, `LinkWireless::State::CONNECTING`, or `LinkWireless::State::CONNECTED`).
`isConnected()` | **bool** | Returns true if the player count is higher than 1.
`isSessionActive()` | **bool** | Returns true if the state is `SERVING` or `CONNECTED`.
`playerCount()` | **u8** *(1~5)* | Returns the number of connected players.
`currentPlayerId()` | **u8** *(0~4)* | Returns the current player id.
`getLastError([clear])` | **LinkWireless::Error** | If one of the other methods returns `false`, you can inspect this to know the cause. After this call, the last error is cleared if `clear` is `true` (default behavior).

⚠️ `0xFFFF` is a reserved value, so don't send it!

# 💻 LinkWirelessMultiboot

*(aka Multiboot through Wireless Adapter)*

This tool allows sending Multiboot ROMs (small 256KiB programs that fit in EWRAM) from one GBA to up to 4 slaves, wirelessly, using a single cartridge.

https://github.com/afska/gba-link-connection/assets/1631752/9a648bff-b14f-4a85-92d4-ccf366adce2d

## Methods

Name | Return type | Description
--- | --- | ---
`sendRom(rom, romSize, gameName, userName, gameId, players, cancel)` | **LinkWirelessMultiboot::Result** | Sends the `rom`. The `players` must be the exact number of consoles that will download the ROM. Once this number of players is reached, the code will start transmitting the ROM bytes. During the process, the library will continuously invoke `cancel` (passing a `LinkWirelessMultiboot::MultibootProgress` object as argument), and abort the transfer if it returns `true`. The `romSize` must be a number between `448` and `262144`. It's recommended to use a ROM size that is a multiple of `16`, as this also ensures compatibility with Multiboot via Link Cable. Once completed, the return value should be `LinkWirelessMultiboot::Result::SUCCESS`.

# 🔧📻 LinkRawWireless

- This is a minimal hardware wrapper designed for the *Wireless Adapter*.
- It doesn't include any of the features of [📻 LinkWireless](#-LinkWireless), so it's not well suited for games.
- Its demo (`LinkRawWireless_demo`) can help emulator developers in enhancing accuracy.

![screenshot](https://github.com/afska/gba-link-connection/assets/1631752/bc7e5a7d-a1bd-46a5-8318-98160c1229ae)

## Methods

- There's one method for every supported wireless adapter command.
- Use `sendCommand(...)` to send arbitrary commands.

# 🔧🏛 LinkWirelessOpenSDK

All first-party games, including the Multiboot 'bootloader' sent by the adapter, use an official software-level protocol. This class provides methods for creating and reading packets that adhere to this protocol. It's supposed to be used in conjunction with [🔧📻 LinkRawWireless](#-LinkRawWireless).

## Methods

Name | Return type | Description
--- | --- | ---
`getChildrenData(response)` | **LinkWirelessOpenSDK::ChildrenData** | Parses the `response` and returns a struct containing all the received packets from the connected clients.
`getParentData(response)` | **LinkWirelessOpenSDK::ParentData** | Parses the `response` and returns a struct containing all the received packets from the host.
`createServerBuffer(fullPayload, fullPayloadSize, sequence, [targetSlots], [offset])` | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the host to send a `fullPayload` with a valid header. If `fullPayloadSize` is higher than `84` (the maximum payload size), the buffer will only contain the **first** `84` bytes (unless an `offset` > 0 is used). A `sequence` number must be created by using `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`. Optionally, a `targetSlots` bit array can be used to exclude some clients from the transmissions (the default is `0b1111`).
`createServerACKBuffer(clientHeader, clientNumber)` | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the host to acknowledge a header received from a certain `clientNumber`.
`createClientBuffer(fullPayload, fullPayloadSize, sequence, [offset])` | **LinkWirelessOpenSDK::SendBuffer<ClientSDKHeader>** | Creates a buffer for the client to send a `fullPayload` with a valid header. If `fullPayloadSize` is higher than `14` (the maximum payload size), the buffer will only contain the **first** `14` bytes (unless an `offset` > 0 is used). A `sequence` number must be created by using `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`.
`createClientACKBuffer(serverHeader)` | **LinkWirelessOpenSDK::SendBuffer<ServerSDKHeader>** | Creates a buffer for the client to acknowledge a header received from the host.

# 🌎 LinkUniversal

A multiuse library that doesn't care whether you plug a Link Cable or a Wireless Adapter. It continuously switches between both and tries to connect to other peers, supporting the hot swapping of cables and adapters and all the features from [👾 LinkCable](#-LinkCable) and [📻 LinkWireless](#-LinkWireless).

https://github.com/afska/gba-link-connection/assets/1631752/d1f49a48-6b17-4954-99d6-d0b7586f5730

## Constructor

`new LinkUniversal(...)` accepts these **optional** parameters:

Name | Type | Default | Description
--- | --- | --- | ---
`protocol` | **LinkUniversal::Protocol** | `AUTODETECT` | Specifies what protocol should be used (one of `LinkUniversal::Protocol::AUTODETECT`, `LinkUniversal::Protocol::CABLE`, `LinkUniversal::Protocol::WIRELESS_AUTO`, `LinkUniversal::Protocol::WIRELESS_SERVER`, or `LinkUniversal::Protocol::WIRELESS_CLIENT`).
`gameName` | **const char\*** | `""` | The game name that will be broadcasted in wireless sessions (max `14` characters). The string must be a null-terminated character array. The library uses this to only connect to servers from the same game.
`cableOptions` | **LinkUniversal::CableOptions** | *same as LinkCable* | All the [👾 LinkCable](#-LinkCable) constructor parameters in one *struct*.
`wirelessOptions` | **LinkUniversal::WirelessOptions** | *same as LinkWireless* | All the [📻 LinkWireless](#-LinkWireless) constructor parameters in one *struct*.

You can also change these compile-time constants:
- `LINK_UNIVERSAL_MAX_PLAYERS`: to set a maximum number of players. The default value is `5`, but since LinkCable's limit is `4`, you might want to decrease it.
- `LINK_UNIVERSAL_GAME_ID_FILTER`: to restrict wireless connections to rooms with a specific game ID (`0x0000` - `0x7fff`). The default value (`0`) connects to any game ID and uses `0x7fff` when serving.

## Methods

The interface is the same as [👾 LinkCable](#-LinkCable). Additionally, it supports these methods:

Name | Return type | Description
--- | --- | ---
`getState()` | **LinkUniversal::State** | Returns the current state (one of `LinkUniversal::State::INITIALIZING`, `LinkUniversal::State::WAITING`, or `LinkUniversal::State::CONNECTED`).
`getMode()` | **LinkUniversal::Mode** | Returns the active mode (one of `LinkUniversal::Mode::LINK_CABLE`, or `LinkUniversal::Mode::LINK_WIRELESS`).
`getProtocol()` | **LinkUniversal::Protocol** | Returns the active protocol (one of `LinkUniversal::Protocol::AUTODETECT`, `LinkUniversal::Protocol::CABLE`, `LinkUniversal::Protocol::WIRELESS_AUTO`, `LinkUniversal::Protocol::WIRELESS_SERVER`, or `LinkUniversal::Protocol::WIRELESS_CLIENT`).
`setProtocol(protocol)` | - | Sets the active `protocol`.
`getWirelessState()` | **LinkWireless::State** | Returns the wireless state (same as [📻 LinkWireless](#-LinkWireless)'s `getState()`).

# 🔌 LinkGPIO

*(aka General Purpose Mode)*

This is the default Link Port mode, and it allows users to manipulate pins `SI`, `SO`, `SD` and `SC` directly.

![photo](https://github.com/afska/gba-link-connection/assets/1631752/b53ddc3d-46c5-441b-9036-489150d9de9f)

## Methods

Name | Return type | Description
--- | --- | ---
`reset()` | - | Resets communication mode to General Purpose. **Required to initialize the library!**
`setMode(pin, direction)` | - | Configures a `pin` to use a `direction` (input or output).
`getMode(pin)` | **LinkGPIO::Direction** | Returns the direction set at `pin`.
`readPin(pin)` | **bool** | Returns whether a `pin` is *HIGH* or not (when set as an input).
`writePin(pin, isHigh)` | - | Sets a `pin` to be high or not (when set as an output).
`setSIInterrupts(isEnabled)` | - | If it `isEnabled`, an IRQ will be generated when `SI` changes from *HIGH* to *LOW*.

⚠️ always set the `SI` terminal to an input!

⚠️ call `reset()` when you finish doing GPIO stuff! (for compatibility with the other libraries)

# 🔗 LinkSPI

*(aka Normal Mode)*

This is the GBA's implementation of SPI. You can use this to interact with other GBAs or computers that know SPI. By default, it uses 32-bit packets, but you can switch to 8-bit by enabling the compile-time constant `LINK_SPI_8BIT_MODE`.

![screenshot](https://user-images.githubusercontent.com/1631752/213068614-875049f6-bb01-41b6-9e30-98c73cc69b25.png)

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate(mode)` | - | Activates the library in a specific `mode` (one of `LinkSPI::Mode::SLAVE`, `LinkSPI::Mode::MASTER_256KBPS`, or `LinkSPI::Mode::MASTER_2MBPS`).
`deactivate()` | - | Deactivates the library.
`transfer(data)` | **u32** | Exchanges `data` with the other end. Returns the received data.
`transfer(data, cancel)` | **u32** | Like `transfer(data)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`transferAsync(data, [cancel])` | - | Schedules a `data` transfer and returns. After this, call `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the async data, normal `transfer(...)`s won't do anything!
`getAsyncState()` | **LinkSPI::AsyncState** | Returns the state of the last async transfer (one of `LinkSPI::AsyncState::IDLE`, `LinkSPI::AsyncState::WAITING`, or `LinkSPI::AsyncState::READY`).
`getAsyncData()` | **u32** | If the async state is `READY`, returns the remote data and switches the state back to `IDLE`.
`getMode()` | **LinkSPI::Mode** | Returns the current `mode`.
`setWaitModeActive(isActive)` | - | Enables or disables `waitMode` (*).
`isWaitModeActive()` | **bool** | Returns whether `waitMode` (*) is active or not.

> (*) `waitMode`: The GBA adds an extra feature over SPI. When working as master, it can check whether the other terminal is ready to receive, and wait if it's not. That makes the connection more reliable, but it's not always supported on other hardware units (e.g. the Wireless Adapter), so it must be disabled in those cases.
> 
> `waitMode` is disabled by default.

⚠️ when using Normal Mode between two GBAs, use a GBC Link Cable!

⚠️ only use the 2Mbps mode with custom hardware (very short wires)!

⚠️ don't send `0xFFFFFFFF`, it's reserved for errors!

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

*(aka UART Mode)*

This is the GBA's implementation of UART. You can use this to interact with a PC using a _USB to UART cable_. You can change the buffer size by changing the compile-time constant `LINK_UART_QUEUE_SIZE`.

![photo](https://github.com/afska/gba-link-connection/assets/1631752/2ca8abb8-1a38-40bb-bf7d-bf29a0f880cd)

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate(baudRate, dataSize, parity, useCTS)` | - | Activates the library using a specific UART mode. _Defaults: 9600bps, 8-bit data, no parity bit, no CTS_.
`deactivate()` | - | Deactivates the library.
`sendLine(string)` | - | Receives a null-terminated `string`, and sends the string followed by a `'\n'` character. The null character is not sent.
`sendLine(data, cancel)` | - | Like `sendLine(string)` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`readLine(string, [limit])` | **bool** | Reads characters into `string` until finding a `'\n'` character or a character `limit` is reached. A null terminator is added at the end. Returns `false` if the limit has been reached without finding a newline character.
`readLine(string, cancel, [limit])` | - | Like `readLine(string, [limit])` but accepts a `cancel()` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`send(buffer, size, offset)` | - | Sends `size` bytes from `buffer`, starting at byte `offset`.
`read(buffer, size, offset)` | **u32** | Tries to read `size` bytes into `(u8*)(buffer + offset)`. Returns the number of read bytes. 
`canRead()` | **bool** | Returns whether there are bytes to read or not.
`canSend()` | **bool** | Returns whether there is room to send new messages or not.
`availableForRead()` | **u32** | Returns the number of bytes available for read.
`availableForSend()` | **u32** | Returns the number of bytes available for send (buffer size - queued bytes).
`read()` | **u8** | Reads a byte. Returns 0 if nothing is found.
`send(data)` | - | Sends a `data` byte.

## UART Configuration

The GBA operates using 1 stop bit, but everything else can be configured. By default, the library uses `8N1`, which means 8-bit data and no parity bit. RTS/CTS is disabled by default.

![diagram](https://github.com/afska/gba-link-connection/assets/1631752/a6a58f94-da24-4fd9-9603-9c7c9a493f93)

- Black wire (GND) -> GBA GND.
- Green wire (TX) -> GBA SI.
- White wire (RX) -> GBA SO.