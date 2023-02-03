# gba-link-connection

A set of Game Boy Advance (GBA) C++ libraries to interact with the Serial Port. Its main purpose is providing multiplayer support to homebrew games.

- [👾](#-LinkCable) [LinkCable.h](lib/LinkCable.h): The classic 16-bit **Multi-Play mode** (up to 4 players) using a GBA Link Cable!
- [💻](#-LinkCableMultiboot) [LinkCableMultiboot.h](lib/LinkCableMultiboot.h): ‍Send **Multiboot software** (small 256KiB ROMs) to other GBAs with no cartridge!
- [🔌](#-LinkGPIO) [LinkGPIO.h](lib/LinkGPIO.h): Use the Link Port however you want to control **any device** (like LEDs, rumble motors, and that kind of stuff)!
- [🔗](#-LinkSPI) [LinkSPI.h](lib/LinkSPI.h): Connect with a PC (like a **Raspberry Pi**) or another GBA (with a GBC Link Cable) using this mode. Transfer up to 2Mbit/s!
- [📻](#-LinkWireless) [LinkWireless.h](lib/LinkWireless.h): Connect up to 5 consoles with the **Wireless Adapter**!

*(click on the emojis for documentation)*

## Usage

- Include the library you want (e.g. [LinkCable.h](lib/LinkCable.h)) in your game code, and read its comment for instructions.
- Check out the [examples](examples) folder
	* Builds are available in *Releases*.
	* They can be tested on real GBAs or using emulators (*NO$GBA*, *mGBA*, or *VBA-M*).

### Makefile actions (for all examples)

```bash
make [ clean | build | start | rebuild | restart ]
```

# 👾 LinkCable

*(aka Multi-Play Mode)*

This is the Link Port mode that games use for multiplayer.

The library uses message queues to send/receive data and transmits when it's possible. As it uses CPU interrupts, the connection is alive even if a console drops a frame or gets stucked in a long iteration loop. After such event, all nodes end up receiving all the pending messages, so a lockstep communication protocol can be used.

![screenshot](https://user-images.githubusercontent.com/1631752/99154109-1d131980-268c-11eb-86b1-7a728f639e5e.png)

## Constructor

`new LinkCable(...)` accepts these **optional** parameters:

Name | Type | Default | Description
--- | --- | --- | ---
`baudRate` | **BaudRate** | `LinkCable::BaudRate::BAUD_RATE_1` | Sets a specific baud rate.
`timeout` | **u32** | `3` | Number of *frames* without an `II_SERIAL` IRQ to reset the connection.
`remoteTimeout` | **u32** | `5` | Number of *messages* with `0xFFFF` to mark a player as disconnected.
`bufferSize` | **u32** | `30` | Number of *messages* that the queues will be able to store.
`interval` | **u16** | `50` | Number of *1024cycles* (61.04μs) ticks between messages *(50 = 3,052ms)*. It's the interval of Timer #`sendTimerId`.
`sendTimerId` | **u8** *(0~3)* | `3` | GBA Timer to use for sending.

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate()` | - | Activates the library.
`deactivate()` | - | Deactivates the library.
`isConnected()` | **bool** | Returns `true` if there are at least 2 connected players.
`playerCount()` | **u8** *(0~4)* | Returns the number of connected players.
`currentPlayerId()` | **u8** *(0~3)* | Returns the current player id.
`canRead(playerId)` | **bool** | Returns `true` if there are pending messages from player #`playerId`.
`read(playerId)` | **u16** | Returns one message from player #`playerId`.
`consume()` | - | Marks the current data as processed, enabling the library to fetch more.
`send(data)` | - | Sends `data` to all connected players.

⚠️ `0xFFFF` and `0x0` are reserved values, so don't use them!

# 💻 LinkCableMultiboot

*(aka Multiboot through Multi-Play mode)*

This tool allows sending Multiboot ROMs (small 256KiB programs that fit in EWRAM) from one GBA to up to 3 slaves, using a single cartridge.

![photo](https://user-images.githubusercontent.com/1631752/213667130-fafcbdb1-767f-4f74-98cb-d7e36c4d7e4e.jpg)

## Methods

Name | Return type | Description
--- | --- | ---
`sendRom(rom, romSize, cancel)` | **LinkCableMultiboot::Result** | Sends the `rom`. During the handshake process, the library will continuously invoke `cancel`, and abort the transfer if it returns `true`. The `romSize` must be a number between `448` and `262144`, and a multiple of `16`. Once completed, the return value should be `LinkCableMultiboot::Result::SUCCESS`.

⚠️ for better results, turn on the GBAs **after** calling the `sendRom` method!

# 🔌 LinkGPIO

*(aka General Purpose Mode)*

This is the default Link Port mode, and it allows users to manipulate pins `SI`, `SO`, `SD` and `SC` directly.

![photo](https://user-images.githubusercontent.com/1631752/212867547-e0a795aa-da00-4b2c-8640-8db7ea857e19.jpg)

## Methods

Name | Return type | Description
--- | --- | ---
`reset()` | - | Resets communication mode to General Purpose. **Required to initialize the library!**
`setMode(pin, direction)` | - | Configures a `pin` to use a `direction` (input or output).
`getMode(pin)` | **LinkGPIO::Direction** | Returns the direction set at `pin`.
`readPin(pin)` | **bool** | Returns whether a `pin` is *HIGH* or not (when set as an input).
`writePin(pin, isHigh)` | - | Sets a `pin` to be high or not (when set as an output).
`setSIInterrupts(isEnabled)` | - | If it `isEnabled`, a IRQ will be generated when `SI` changes from *HIGH* to *LOW*.

⚠️ always set the `SI` terminal to an input!

# 🔗 LinkSPI

*(aka Normal Mode)*

This is GBA's implementation of SPI. In this library, packets are set to 32-bit, as there's no benefit to using the 8-bit version. You can use this to interact with other GBAs or computers that know SPI.

![screenshot](https://user-images.githubusercontent.com/1631752/213068614-875049f6-bb01-41b6-9e30-98c73cc69b25.png)

## Methods

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate(mode)` | - | Activates the library in a specific `mode` (one of `LinkSPI::Mode::SLAVE`, `LinkSPI::Mode::MASTER_256KBPS`, or `LinkSPI::Mode::MASTER_2MBPS`).
`deactivate()` | - | Deactivates the library.
`transfer(data)` | **u32** | Exchanges `data` with the other end. Returns the received data.
`transfer(data, cancel)` | **u32** | Like `transfer(data)` but accepts a `cancel` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`transferAsync(data, [cancel])` | - | Schedules a `data` transfer and returns. After this, call `getAsyncState` and `getAsyncData`. Note that until you retrieve the async data, normal `transfer(...)`s won't do anything!
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

# 📻 LinkWireless

*(aka GBA Wireless Adapter)*

This is a driver for an accessory that enables wireless games up to 5 players. The inner workings of the adapter are highly unknown, but [this article](docs/wireless_adapter.md) is very helpful. I've updated the blog post to add more details about the things I learnt by the means of ~~reverse engineering~~ brute force and trial&error.

The library, by default, implements a lightweight protocol on top of the adapter's message system. This allows detecting disconnections, forwarding messages to all nodes, and retransmitting to prevent packet loss.

![photo](https://user-images.githubusercontent.com/1631752/216233248-1f8ee26e-c8c1-418a-ad02-ad7c283dc49f.png)

## Constructor

`new LinkWireless(...)` accepts these **optional** parameters:

Name | Type | Default | Description
--- | --- | --- | ---
`forwarding` | **bool** | `true` | If `true`, the server forwards all messages to the clients. Otherwise, clients only see messages sent from the server (ignoring other peers).
`retransmission` | **bool** | `true` | If `true`, the library handles retransmission for you, so there should be no packet loss.
`maxPlayers` | **u8** | `5` | Maximum number of allowed players.
`msgTimeout` | **u32** | `5` | Timeout used by `receive(...)`. It's the maximum number of *receive calls* without a message from other connected player to disconnect.
`multiReceiveTimeout` | **u32** | `1140` | An extra timeout used by `receive(messages, times)`. It's the maximum number of *vertical lines* without a message from anybody to disconnect *(228 vertical lines = 1 frame)*.
`bufferSize` | **u32** | `30` | Number of *messages* that the queues will be able to store.

## Methods

✔️ Most of the methods return a boolean, indicating if the action was successful. If not, you can call `getLastError()` to know the reason. Usually, unless it's a trivial error (like buffers being full), the connection with the adapter is reset and the game needs to start again. You can check the connection state with `getState()`. All actions are synchronic.

Name | Return type | Description
--- | --- | ---
`isActive()` | **bool** | Returns whether the library is active or not.
`activate()` | **bool** | Activates the library.
`deactivate()` | - | Deactivates the library.
`serve()` | **bool** | Starts broadcasting a server and changes the state to `SERVING`.
`acceptConnections()` | **bool** | Accepts new clients and updates the player count.
`connect(serverId)` | **bool** | Starts a connection with `serverId` and changes the state to `CONNECTING`.
`keepConnecting()` | **bool** | When connecting, needs to be called until the state is `CONNECTED`. It assigns a player id.
`getServerIds(serverIds)` | **bool** | Fills the `serverIds` vector with all the currently broadcasting servers.
`send(data)` | **bool** | Enqueues `data` to be sent to other nodes. Note that this data will be sent in the next `receive(...)` call.
`receive(messages)` | **bool** | Sends the pending data and fills the `messages` vector with incoming messages, checking for timeouts and forwarding if needed. This call doesn't block the hardware waiting for messages, it returns if there are no incoming messages.
`receive(messages, times)` | **bool** | Perform multiple `receive(...)` calls until successfully exchanging data a number of `times`. This can only be called if `retransmission` is on.
`receive(messages, times, cancel)` | **bool** | Like `receive(messages, times)` but accepts a `cancel` function. The library will continuously invoke it, and abort the transfer if it returns `true`.
`disconnect()` | **bool** | Disconnects and resets the adapter.
`getState()` | **LinkWireless::State** | Returns the current state (one of `LinkWireless::State::NEEDS_RESET`, `LinkWireless::State::AUTHENTICATED`, `LinkWireless::State::SERVING`, `LinkWireless::State::CONNECTING`, or `LinkWireless::State::CONNECTED`).
`getPlayerId()` | **u8** *(0~4)* | Returns the current player id.
`getPlayerCount()` | **u8** *(1~5)* | Returns the connected players.
`canSend()` | **bool** | Returns `false` only if the next `send(...)` call would fail due to full buffers.
`getPendingCount()` | **u32** | Returns the number of outgoing messages ready to be sent. It will always be lower than `bufferSize`.
`didReceiveBytes()` | **bool** | Returns whether the last `receive(...)` call gathered any bytes or not.
`getLastError()` | **LinkWireless::Error** | If one of the other methods returns `false`, you can inspect this to know the cause. After this call, the last error is cleared.

⚠️ servers can send up to `19` words of 32 bits at a time!
⚠️ clients can send up to `3` words of 32 bits at a time!
⚠️ if `retransmission` is on, these limits drop to `14` and `1`!
⚠️ you can workaround these limits by doing multiple exchanges with `receive(messages, times)`!