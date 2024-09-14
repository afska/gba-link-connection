#ifndef LINK_MOBILE_H
#define LINK_MOBILE_H

// --------------------------------------------------------------------------
// A high level driver for the Mobile Adapter GB.
// Check out the REON project -> https://github.com/REONTeam
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkMobile* linkMobile = new LinkMobile();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_MOBILE_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_MOBILE_ISR_SERIAL);
//       irq_add(II_TIMER3, LINK_MOBILE_ISR_TIMER);
// - 3) Initialize the library with:
//       linkMobile->activate();
//       // (do something until `linkMobile->isSessionActive()` returns `true`)
// - 4) Call someone:
//       linkMobile->call("127000000001");
//       // (do something until `linkMobile->isConnectedP2P()` returns `true`)
// - 5) Send/receive data:
//       LinkMobile::DataTransfer dataTransfer = { .size = 5 };
//       for (u32 i = 0; i < 5; i++)
//         dataTransfer.data[i] = ((u8*)"hello")[i];
//       linkMobile->transfer(dataTransfer, &dataTransfer);
//       // (do something until `dataTransfer.completed` is `true`)
//       // (use `dataTransfer` as the received data)
// - 6) Hang up:
//       linkMobile->hangUp();
// - 7) Connect to the internet:
//       linkMobile->callISP("REON password");
//       // (do something until `linkMobile->isConnectedPPP()` returns `true`)
// - 8) Run DNS queries:
//       LinkMobile::DNSQuery dnsQuery;
//       linkMobile->dnsQuery("something.com", &dnsQuery);
//       // (do something until `dnsQuery.completed` is `true`)
//       // (use `dnsQuery.success` and `dnsQuery.ipv4`)
// - 9) Open connections:
//       auto type = LinkMobile::ConnectionType::TCP;
//       LinkMobile::OpenConn openConn;
//       linkMobile->openConnection(dnsQuery.ipv4, connType, &openConn);
//       // (do something until `openConn.completed` is `true`)
//       // (use `openConn.connectionId` as last argument of `transfer(...)`)
// - 10) Close connections:
//       LinkMobile::CloseConn closeConn;
//       linkMobile->closeConnection(openConn.connectionId, type, &closeConn);
//       // (do something until `closeConn.completed` is `true`)
// - 11) Synchronously wait for an action to be completed:
//       linkMobile->waitFor(&dnsQuery);
// - 12) Turn off the adapter:
//       linkMobile->shutdown();
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include <cstring>
#include "LinkGPIO.hpp"
#include "LinkSPI.hpp"

#ifndef LINK_MOBILE_QUEUE_SIZE
/**
 * @brief Request queue size (how many commands can be queued at the same time).
 * The default value is `10`, which seems fine for most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around 3 KB.
 */
#define LINK_MOBILE_QUEUE_SIZE 10
#endif

static volatile char LINK_MOBILE_VERSION[] = "LinkMobile/v7.0.1";

#define LINK_MOBILE_MAX_USER_TRANSFER_LENGTH 254
#define LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH 255
#define LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH 32
#define LINK_MOBILE_MAX_LOGIN_ID_LENGTH 32
#define LINK_MOBILE_MAX_PASSWORD_LENGTH 32
#define LINK_MOBILE_MAX_DOMAIN_NAME_LENGTH 253
#define LINK_MOBILE_COMMAND_TRANSFER_BUFFER \
  (LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH + 4)
#define LINK_MOBILE_DEFAULT_TIMEOUT (60 * 10)
#define LINK_MOBILE_DEFAULT_TIMER_ID 3
#define LINK_MOBILE_BARRIER asm volatile("" ::: "memory")

#if LINK_ENABLE_DEBUG_LOGS != 0
#define _LMLOG_(...) Link::log(__VA_ARGS__)
#else
#define _LMLOG_(...)
#endif

/**
 * @brief A high level driver for the Mobile Adapter GB.
 */
class LinkMobile {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr auto BASE_FREQUENCY = Link::_TM_FREQ_1024;
  static constexpr int INIT_WAIT_FRAMES = 7;
  static constexpr int INIT_TIMEOUT_FRAMES = 60 * 3;
  static constexpr int PING_FREQUENCY_FRAMES = 60;
  static constexpr int ADAPTER_WAITING = 0xd2;
  static constexpr u32 ADAPTER_WAITING_32BIT = 0xd2d2d2d2;
  static constexpr int GBA_WAITING = 0x4b;
  static constexpr u32 GBA_WAITING_32BIT = 0x4b4b4b4b;
  static constexpr int OR_VALUE = 0x80;
  static constexpr int COMMAND_MAGIC_VALUE1 = 0x99;
  static constexpr int COMMAND_MAGIC_VALUE2 = 0x66;
  static constexpr int DEVICE_GBA = 0x1;
  static constexpr int DEVICE_ADAPTER_BLUE = 0x8;
  static constexpr int DEVICE_ADAPTER_YELLOW = 0x9;
  static constexpr int DEVICE_ADAPTER_GREEN = 0xa;
  static constexpr int DEVICE_ADAPTER_RED = 0xb;
  static constexpr int ACK_SENDER = 0;
  static constexpr int CONFIGURATION_DATA_SIZE = 192;
  static constexpr int CONFIGURATION_DATA_CHUNK = CONFIGURATION_DATA_SIZE / 2;
  static constexpr const char* FALLBACK_ISP_NUMBER = "#9677";
  static constexpr int COMMAND_BEGIN_SESSION = 0x10;
  static constexpr int COMMAND_END_SESSION = 0x11;
  static constexpr int COMMAND_DIAL_TELEPHONE = 0x12;
  static constexpr int COMMAND_HANG_UP_TELEPHONE = 0x13;
  static constexpr int COMMAND_WAIT_FOR_TELEPHONE_CALL = 0x14;
  static constexpr int COMMAND_TRANSFER_DATA = 0x15;
  static constexpr int COMMAND_RESET = 0x16;
  static constexpr int COMMAND_TELEPHONE_STATUS = 0x17;
  static constexpr int COMMAND_SIO32 = 0x18;
  static constexpr int COMMAND_READ_CONFIGURATION_DATA = 0x19;
  static constexpr int COMMAND_ISP_LOGIN = 0x21;
  static constexpr int COMMAND_ISP_LOGOUT = 0x22;
  static constexpr int COMMAND_OPEN_TCP_CONNECTION = 0x23;
  static constexpr int COMMAND_CLOSE_TCP_CONNECTION = 0x24;
  static constexpr int COMMAND_OPEN_UDP_CONNECTION = 0x25;
  static constexpr int COMMAND_CLOSE_UDP_CONNECTION = 0x26;
  static constexpr int COMMAND_DNS_QUERY = 0x28;
  static constexpr int COMMAND_CONNECTION_CLOSED = 0x1f;
  static constexpr int COMMAND_ERROR_STATUS = 0x6e | OR_VALUE;
  static constexpr u8 WAIT_TICKS[] = {4, 8};
  static constexpr int LOGIN_PARTS_SIZE = 8;
  static constexpr u8 LOGIN_PARTS[] = {0x4e, 0x49, 0x4e, 0x54,
                                       0x45, 0x4e, 0x44, 0x4f};
  static constexpr int SUPPORTED_DEVICES_SIZE = 4;
  static constexpr u8 SUPPORTED_DEVICES[] = {
      DEVICE_ADAPTER_BLUE, DEVICE_ADAPTER_YELLOW, DEVICE_ADAPTER_GREEN,
      DEVICE_ADAPTER_RED};
  static constexpr u8 DIAL_PHONE_FIRST_BYTE[] = {0, 2, 1, 1};

 public:
  enum State {
    NEEDS_RESET = 0,
    PINGING = 1,
    WAITING_TO_START = 2,
    STARTING_SESSION = 3,
    ACTIVATING_SIO32 = 4,
    WAITING_32BIT_SWITCH = 5,
    READING_CONFIGURATION = 6,
    SESSION_ACTIVE = 7,
    CALL_REQUESTED = 8,
    CALLING = 9,
    CALL_ESTABLISHED = 10,
    ISP_CALL_REQUESTED = 11,
    ISP_CALLING = 12,
    PPP_LOGIN = 13,
    PPP_ACTIVE = 14,
    SHUTDOWN_REQUESTED = 15,
    ENDING_SESSION = 16,
    WAITING_8BIT_SWITCH = 17,
    SHUTDOWN = 18
  };

  enum Role { NO_P2P_CONNECTION, CALLER, RECEIVER };

  enum ConnectionType { TCP, UDP };

  struct ConfigurationData {
    char magic[2];
    u8 registrationState;
    u8 _unused1_;
    u8 primaryDNS[4];
    u8 secondaryDNS[4];
    char loginId[10];
    u8 _unused2_[22];
    char email[24];
    u8 _unused3_[6];
    char smtpServer[20];
    char popServer[19];
    u8 _unused4_[5];
    u8 configurationSlot1[24];
    u8 configurationSlot2[24];
    u8 configurationSlot3[24];
    u8 checksumHigh;
    u8 checksumLow;

    char _ispNumber1[16 + 1];  // (parsed from `configurationSlot1`)
  } __attribute__((packed));

  struct AsyncRequest {
    volatile bool completed = false;
    bool success = false;

    bool fail() {
      success = false;
      completed = true;
      return false;
    }
  };

  struct DNSQuery : public AsyncRequest {
    u8 ipv4[4] = {};
  };

  struct OpenConn : public AsyncRequest {
    u8 connectionId = 0;
  };

  struct CloseConn : public AsyncRequest {};

  struct DataTransfer : public AsyncRequest {
    u8 data[LINK_MOBILE_MAX_USER_TRANSFER_LENGTH] = {};
    u8 size = 0;
  };

  enum CommandResult {
    PENDING,
    SUCCESS,
    INVALID_DEVICE_ID,
    INVALID_COMMAND_ACK,
    INVALID_MAGIC_BYTES,
    WEIRD_DATA_SIZE,
    WRONG_CHECKSUM,
    ERROR_CODE,
    WEIRD_ERROR_CODE
  };

  struct Error {
    enum Type {
      NONE,
      ADAPTER_NOT_CONNECTED,
      PPP_LOGIN_FAILED,
      COMMAND_FAILED,
      WEIRD_RESPONSE,
      TIMEOUT,
      WTF
    };

    Error::Type type = Error::Type::NONE;
    State state = State::NEEDS_RESET;
    u8 cmdId = 0;
    CommandResult cmdResult = CommandResult::PENDING;
    u8 cmdErrorCode = 0;
    bool cmdIsSending = false;
    int reqType = -1;
  };

  /**
   * @brief Constructs a new LinkMobile object.
   * @param timeout Number of *frames* without completing a request to reset a
   * connection. Defaults to 600 (10 seconds).
   * @param timerId GBA Timer to use for waiting.
   */
  explicit LinkMobile(u32 timeout = LINK_MOBILE_DEFAULT_TIMEOUT,
                      u8 timerId = LINK_MOBILE_DEFAULT_TIMER_ID) {
    this->config.timeout = timeout;
    this->config.timerId = timerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library. After some time, if an adapter is connected,
   * the state will be changed to `SESSION_ACTIVE`. If not, the state will be
   * `NEEDS_RESET`, and you can retrieve the error with `getError()`.
   */
  void activate() {
    error = {};

    LINK_MOBILE_BARRIER;
    isEnabled = false;
    LINK_MOBILE_BARRIER;

    resetState();
    stop();

    LINK_MOBILE_BARRIER;
    isEnabled = true;
    LINK_MOBILE_BARRIER;

    start();
  }

  /**
   * @brief Deactivates the library, resetting the serial mode to GPIO.
   * \warning Calling `shutdown()` first is recommended, but the adapter will
   * put itself in sleep mode after 3 seconds anyway.
   */
  void deactivate() {
    error = {};
    isEnabled = false;
    resetState();
    stop();
  }

  /**
   * @brief Gracefully shuts down the adapter, closing all connections.
   * After some time, the state will be changed to `SHUTDOWN`, and only then
   * it's safe to call `deactivate()`.
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active session or available request slots.
   */
  bool shutdown() {
    if (!canShutdown() || userRequests.isFull())
      return false;

    pushRequest(UserRequest{.type = UserRequest::Type::SHUTDOWN});
    return true;
  }

  /**
   * @brief Initiates a P2P connection with a `phoneNumber`. After some time,
   * the state will be `CALL_ESTABLISHED` (or `ACTIVE_SESSION` if the
   * connection fails or ends).
   * @param phoneNumber The phone number to call. In REON/libmobile this can be
   * a number assigned by the relay server, or a 12-digit IPv4 address (for
   * example, "127000000001" would be 127.0.0.1).
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active session or available request slots.
   */
  bool call(const char* phoneNumber) {
    if (state != SESSION_ACTIVE || userRequests.isFull())
      return false;

    auto request = UserRequest{.type = UserRequest::Type::CALL};
    copyString(request.phoneNumber, phoneNumber,
               LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH);
    pushRequest(request);
    return true;
  }

  /**
   * @brief Calls the ISP number registered in the adapter configuration, or a
   * default number if the adapter hasn't been configured. Then, performs a
   * login operation using the provided REON `password` and `loginId`. After
   * some time, the state will be `PPP_ACTIVE`. If `loginId` is empty and the
   * adapter has been configured, it will use the one stored in the
   * configuration.
   * @param password The password, as a null-terminated string (max `32`
   * characters).
   * @param loginId The login ID, as a null-terminated string (max `32`
   * characters). It can be empty if it's already stored in the configuration.
   * \warning Non-blocking. Returns `true` immediately, or `false`
   * if there's no active session, no available request slots, or no login ID.
   */
  bool callISP(const char* password, const char* loginId = "") {
    if (state != SESSION_ACTIVE || userRequests.isFull())
      return false;

    auto request = UserRequest{.type = UserRequest::Type::PPP_LOGIN};
    copyString(request.password, password, LINK_MOBILE_MAX_PASSWORD_LENGTH);

    if (std::strlen(loginId) > 0)
      copyString(request.loginId, loginId, LINK_MOBILE_MAX_LOGIN_ID_LENGTH);
    else if (adapterConfiguration.isValid())
      copyString(request.loginId, adapterConfiguration.fields._ispNumber1,
                 LINK_MOBILE_MAX_LOGIN_ID_LENGTH);
    else
      return false;

    pushRequest(request);
    return true;
  }

  /**
   * @brief Looks up the IPv4 address for a domain name.
   * @param domain A null-terminated string for the domain name (max `253`
   * characters). It also accepts a ASCII IPv4 address, converting it into a
   * 4-byte address instead of querying the DNS server.
   * @param result A pointer to a `LinkMobile::DNSQuery` struct that
   * will be filled with the result. When the request is completed, the
   * `completed` field will be `true`. If an IP address was found, the `success`
   * field will be `true` and the `ipv4` field can be read as a 4-byte address.
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active PPP session or available request slots.
   */
  bool dnsQuery(const char* domainName, DNSQuery* result) {
    if (state != PPP_ACTIVE || userRequests.isFull())
      return result->fail();

    result->completed = false;
    result->success = false;
    u32 size = std::strlen(domainName);
    if (size > LINK_MOBILE_MAX_DOMAIN_NAME_LENGTH)
      size = LINK_MOBILE_MAX_DOMAIN_NAME_LENGTH;

    auto request = UserRequest{.type = UserRequest::Type::DNS_QUERY,
                               .dns = result,
                               .send = {.data = {}},
                               .commandSent = false};
    for (u32 i = 0; i < size; i++)
      request.send.data[i] = domainName[i];
    request.send.size = size;

    pushRequest(request);
    return true;
  }

  /**
   * @brief Opens a TCP/UDP (`type`) connection at the given `ip` (4-byte
   * address) on the given `port`.
   * @param ip The 4-byte address.
   * @param port The port.
   * @param type One of the enum values from `LinkMobile::ConnectionType`.
   * @param result A pointer to a `LinkMobile::OpenConn` struct that
   * will be filled with the result. When the request is completed, the
   * `completed` field will be `true`. If the connection was successful, the
   * `success` field will be `true` and the `connectionId` field can be used
   * when calling the `transfer(...)` method. If not, you can assume that the
   * connection was closed.
   * \warning Only `2` connections can be opened at the same time.
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active PPP session, no available request slots.
   */
  bool openConnection(const u8* ip,
                      u16 port,
                      ConnectionType type,
                      OpenConn* result) {
    if (state != PPP_ACTIVE || userRequests.isFull())
      return result->fail();

    result->completed = false;
    result->success = false;

    auto request = UserRequest{.type = UserRequest::Type::OPEN_CONNECTION,
                               .open = result,
                               .connectionType = type,
                               .commandSent = false};
    for (u32 i = 0; i < 4; i++)
      request.ip[i] = ip[i];
    request.port = port;

    pushRequest(request);
    return true;
  }

  /**
   * @brief Closes an active TCP/UDP (`type`) connection.
   * @param connectionId The ID of the connection.
   * @param type One of the enum values from `LinkMobile::ConnectionType`.
   * @param result A pointer to a `LinkMobile::CloseConn` struct that
   * will be filled with the result. When the request is completed, the
   * `completed` field will be `true`. If the connection was closed correctly,
   * the `success` field will be `true`.
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active PPP session, no available request slots.
   */
  bool closeConnection(u8 connectionId,
                       ConnectionType type,
                       CloseConn* result) {
    if (state != PPP_ACTIVE || userRequests.isFull())
      return result->fail();

    result->completed = false;
    result->success = false;

    auto request = UserRequest{.type = UserRequest::Type::CLOSE_CONNECTION,
                               .close = result,
                               .connectionType = type,
                               .commandSent = false};
    request.connectionId = connectionId;

    pushRequest(request);
    return true;
  }

  /**
   * @brief Requests a data transfer and responds the received data. The
   * transfer can be done with the other node in a P2P connection, or with any
   * open TCP/UDP connection if a PPP session is active. In the case of a
   * TCP/UDP connection, the `connectionId` must be provided.
   * @param dataToSend The data to send, up to 254 bytes.
   * @param result A pointer to a `LinkMobile::DataTransfer` struct that
   * will be filled with the received data. It can also point to `dataToSend` to
   * reuse the struct. When the transfer is completed, the `completed` field
   * will be `true`. If the transfer was successful, the `success` field will be
   * `true`.
   * \warning Non-blocking. Returns `true` immediately, or `false` if
   * there's no active call or available request slots.
   */
  bool transfer(DataTransfer dataToSend,
                DataTransfer* result,
                u8 connectionId = 0xff) {
    if ((state != CALL_ESTABLISHED && state != PPP_ACTIVE) ||
        userRequests.isFull())
      return result->fail();

    result->completed = false;
    result->success = false;

    auto request = UserRequest{.type = UserRequest::Type::TRANSFER,
                               .connectionId = connectionId,
                               .send = {.data = {}, .size = dataToSend.size},
                               .receive = result,
                               .commandSent = false};
    for (u32 i = 0; i < dataToSend.size; i++)
      request.send.data[i] = dataToSend.data[i];
    pushRequest(request);
    return true;
  }

  /**
   * @brief Waits for `asyncRequest` to be completed. Returns `true` if the
   * request was completed && successful, and the adapter session is still
   * alive. Otherwise, it returns `false`.
   * @param asyncRequest A pointer to a `LinkMobile::DNSQuery`,
   * `LinkMobile::OpenConn`, `LinkMobile::CloseConn`, or
   * `LinkMobile::DataTransfer`.
   */
  bool waitFor(AsyncRequest* asyncRequest) {
    while (isSessionActive() && !asyncRequest->completed)
      Link::_IntrWait(1, Link::_IRQ_SERIAL | Link::_IRQ_VBLANK);

    return isSessionActive() && asyncRequest->completed &&
           asyncRequest->success;
  }

  /**
   * @brief Hangs up the current P2P or PPP call. Closes all connections.
   * \warning Non-blocking. Returns `true` immediately, or `false` if there's no
   * active call or available request slots.
   */
  bool hangUp() {
    if ((state != CALL_ESTABLISHED && state != PPP_ACTIVE) ||
        userRequests.isFull())
      return false;

    pushRequest(UserRequest{.type = UserRequest::Type::HANG_UP});
    return true;
  }

  /**
   * @brief Retrieves the adapter configuration.
   * @param configurationData A structure that will be filled with the
   * configuration data. If the adapter has an active session, the data is
   * already loaded, so it's instantaneous.
   * \warning Returns `true` if `configurationData` has been filled, or `false`
   * if there's no active session.
   */
  [[nodiscard]] bool readConfiguration(ConfigurationData& configurationData) {
    if (!isSessionActive())
      return false;

    configurationData = adapterConfiguration.fields;
    return true;
  }

  /**
   * @brief Returns the current state.
   * @return One of the enum values from `LinkMobile::State`.
   */
  [[nodiscard]] State getState() { return state; }

  /**
   * @brief Returns the current role in the P2P connection.
   * @return One of the enum values from `LinkMobile::Role`.
   */
  [[nodiscard]] Role getRole() { return role; }

  /**
   * @brief Returns whether the adapter has been configured or not.
   * @return 1 = yes, 0 = no, -1 = unknown (no session active).
   */
  [[nodiscard]] int isConfigurationValid() {
    if (!isSessionActive())
      return -1;

    return (int)adapterConfiguration.isValid();
  }

  /**
   * @brief Returns `true` if a P2P call is established (the state is
   * `CALL_ESTABLISHED`).
   */
  [[nodiscard]] bool isConnectedP2P() { return state == CALL_ESTABLISHED; }

  /**
   * @brief Returns `true` if a PPP session is active (the state is
   * `PPP_ACTIVE`).
   */
  [[nodiscard]] bool isConnectedPPP() { return state == PPP_ACTIVE; }

  /**
   * @brief Returns `true` if the session is active.
   */
  [[nodiscard]] bool isSessionActive() {
    return state >= SESSION_ACTIVE && state <= SHUTDOWN_REQUESTED;
  }

  /**
   * @brief Returns `true` if there's an active session and there's no previous
   * shutdown requests.
   */
  [[nodiscard]] bool canShutdown() {
    return isSessionActive() && state != SHUTDOWN_REQUESTED;
  }

  /**
   * @brief Returns the current operation mode (`LinkSPI::DataSize`).
   */
  [[nodiscard]] LinkSPI::DataSize getDataSize() {
    return linkSPI->getDataSize();
  }

  /**
   * @brief Returns details about the last error that caused the connection to
   * be aborted.
   */
  [[nodiscard]] Error getError() { return error; }

  ~LinkMobile() { delete linkSPI; }

  /**
   * @brief This method is called by the VBLANK interrupt handler.
   * \warning This is internal API!
   */
  void _onVBlank() {
    if (!isEnabled)
      return;

    if (shouldAbortOnStateTimeout()) {
      timeoutStateFrames++;
      if (timeoutStateFrames >= INIT_TIMEOUT_FRAMES)
        return abort(Error::Type::ADAPTER_NOT_CONNECTED);
    }

    pingFrameCount++;
    if (pingFrameCount >= PING_FREQUENCY_FRAMES && isSessionActive() &&
        !asyncCommand.isActive) {
      pingFrameCount = 0;
      cmdTelephoneStatus();
    }

    processUserRequests();
    processNewFrame();
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled)
      return;

    linkSPI->_onSerial();
    u32 newData = linkSPI->getAsyncData();

    if (state == NEEDS_RESET)
      return;

    if (asyncCommand.isActive) {
      if (asyncCommand.state == AsyncCommand::State::PENDING) {
        if (isSIO32Mode()) {
          if (asyncCommand.direction == AsyncCommand::Direction::SENDING)
            sendAsyncCommandSIO32(newData);
          else
            receiveAsyncCommandSIO32(newData);
        } else {
          if (asyncCommand.direction == AsyncCommand::Direction::SENDING)
            sendAsyncCommandSIO8(newData);
          else
            receiveAsyncCommandSIO8(newData);
        }

        if (asyncCommand.state == AsyncCommand::State::COMPLETED) {
          asyncCommand.isActive = false;
          processAsyncCommand();
        }
      }
    } else {
      processLoosePacket(newData);
    }
  }

  /**
   * @brief This method is called by the TIMER interrupt handler.
   * \warning This is internal API!
   */
  void _onTimer() {
    if (!isEnabled || !hasPendingTransfer)
      return;

    linkSPI->transferAsync(pendingTransfer);
    stopTimer();
    hasPendingTransfer = false;
  }

  struct Config {
    u32 timeout;
    u32 timerId;
  };

  /**
   * @brief LinkMobile configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  enum AdapterType { BLUE, YELLOW, GREEN, RED, UNKNOWN };

  struct UserRequest {
    enum Type {
      CALL,
      PPP_LOGIN,
      DNS_QUERY,
      OPEN_CONNECTION,
      CLOSE_CONNECTION,
      TRANSFER,
      HANG_UP,
      SHUTDOWN
    };

    Type type;
    char phoneNumber[LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH + 1];
    char loginId[LINK_MOBILE_MAX_LOGIN_ID_LENGTH + 1];
    char password[LINK_MOBILE_MAX_PASSWORD_LENGTH + 1];
    DNSQuery* dns;
    OpenConn* open;
    CloseConn* close;
    u8 ip[4];
    u16 port;
    ConnectionType connectionType;
    u8 connectionId;
    DataTransfer send;
    DataTransfer* receive;
    bool commandSent;
    u32 timeout;
    bool finished;

    void cleanup() {
      if (finished)
        return;
      AsyncRequest* metadata = type == DNS_QUERY          ? (AsyncRequest*)dns
                               : type == OPEN_CONNECTION  ? (AsyncRequest*)open
                               : type == CLOSE_CONNECTION ? (AsyncRequest*)close
                               : type == TRANSFER ? (AsyncRequest*)receive
                                                  : nullptr;
      if (metadata != nullptr) {
        metadata->success = false;
        metadata->completed = true;
      }
    }
  };

  union AdapterConfiguration {
    ConfigurationData fields;
    char bytes[CONFIGURATION_DATA_SIZE];

    bool isValid() {
      return fields.magic[0] == 'M' && fields.magic[1] == 'A' &&
             (fields.registrationState & 1) == 1 &&
             calculatedChecksum() == reportedChecksum();
    }

    u16 calculatedChecksum() {
      u16 result = 0;
      for (u32 i = 0; i < CONFIGURATION_DATA_SIZE - 2; i++)
        result += bytes[i];
      return result;
    }

    u16 reportedChecksum() {
      return buildU16(fields.checksumHigh, fields.checksumLow);
    }
  };

  struct MagicBytes {
    u8 magic1 = COMMAND_MAGIC_VALUE1;
    u8 magic2 = COMMAND_MAGIC_VALUE2;
  } __attribute__((packed));

  struct PacketData {
    u8 bytes[LINK_MOBILE_COMMAND_TRANSFER_BUFFER] = {};
  } __attribute__((packed));

  struct PacketHeader {
    u8 commandId = 0;
    u8 _unused_ = 0;
    u8 _unusedSizeHigh_ = 0;
    u8 size = 0;

    u16 sum() { return commandId + _unused_ + _unusedSizeHigh_ + size; }
    u8 pureCommandId() { return commandId & (~OR_VALUE); }
  } __attribute__((packed));

  struct PacketChecksum {
    u8 high = 0;
    u8 low = 0;
  } __attribute__((packed));

  struct Command {
    MagicBytes magicBytes;
    PacketHeader header;
    PacketData data;
    PacketChecksum checksum;
  };

  struct CommandResponse {
    CommandResult result = CommandResult::PENDING;
    Command command;
  };

  struct AsyncCommand {
    enum State { PENDING, COMPLETED };
    enum Direction { SENDING, RECEIVING };

    State state;
    CommandResult result;
    u32 transferred;
    Command cmd;
    Direction direction;
    u16 expectedChecksum;
    u8 errorCommandId;
    u8 errorCode;
    bool isActive = false;

    void reset() {
      state = AsyncCommand::State::PENDING;
      result = CommandResult::PENDING;
      transferred = 0;
      cmd = Command{};
      direction = AsyncCommand::Direction::SENDING;
      expectedChecksum = 0;
      errorCommandId = 0;
      errorCode = 0;
      isActive = false;
    }

    u8 relatedCommandId() {
      return result == CommandResult::ERROR_CODE ? errorCommandId
                                                 : cmd.header.pureCommandId();
    }

    bool respondsTo(u8 commandId) {
      return direction == AsyncCommand::Direction::RECEIVING &&
             (result == CommandResult::ERROR_CODE
                  ? errorCommandId == commandId
                  : cmd.header.commandId == (commandId | OR_VALUE));
    }

    void finish() {
      if (cmd.header.commandId == COMMAND_ERROR_STATUS) {
        if (cmd.header.size != 2) {
          result = CommandResult::WEIRD_ERROR_CODE;
        } else {
          result = CommandResult::ERROR_CODE;
          errorCommandId = cmd.data.bytes[0];
          errorCode = cmd.data.bytes[1];
        }
      } else {
        result = CommandResult::SUCCESS;
      }

      state = AsyncCommand::State::COMPLETED;
    }

    void fail(CommandResult _result) {
      result = _result;
      state = AsyncCommand::State::COMPLETED;
    }
  };

  static constexpr u32 PREAMBLE_SIZE =
      sizeof(MagicBytes) + sizeof(PacketHeader);
  static constexpr u32 CHECKSUM_SIZE = sizeof(PacketChecksum);

  using RequestQueue = Link::Queue<UserRequest, LINK_MOBILE_QUEUE_SIZE, false>;

  RequestQueue userRequests;
  AdapterConfiguration adapterConfiguration;
  AsyncCommand asyncCommand;
  u32 waitFrames = 0;
  u32 timeoutStateFrames = 0;
  u32 pingFrameCount = 0;
  Role role = Role::NO_P2P_CONNECTION;
  LinkSPI* linkSPI = new LinkSPI();
  State state = NEEDS_RESET;
  PacketData nextCommandData;
  u32 nextCommandDataSize = 0;
  bool hasPendingTransfer = false;
  u32 pendingTransfer = 0;
  AdapterType adapterType = AdapterType::UNKNOWN;
  Error error = {};
  volatile bool isEnabled = false;

  void processUserRequests() {
    if (!userRequests.canMutate() || userRequests.isEmpty())
      return;

    if (!isSessionActive()) {
      userRequests.clear();
      return;
    }

    if (userRequests.peek().finished)
      userRequests.pop();
    if (userRequests.isEmpty())
      return;

    auto request = userRequests.peek();
    request.timeout++;
    if (shouldAbortOnRequestTimeout() && request.timeout >= config.timeout)
      return abort(Error::Type::TIMEOUT);

    switch (request.type) {
      case UserRequest::Type::CALL: {
        if (state != SESSION_ACTIVE && state != CALL_REQUESTED) {
          popRequest();
          return;
        }
        if (state != CALL_REQUESTED)
          setState(CALL_REQUESTED);

        if (!asyncCommand.isActive) {
          setState(CALLING);
          cmdDialTelephone(request.phoneNumber);
          popRequest();
        }
        break;
      }
      case UserRequest::Type::PPP_LOGIN: {
        if (state != SESSION_ACTIVE && state != ISP_CALL_REQUESTED &&
            state != ISP_CALLING) {
          popRequest();
          return;
        }
        if (state == SESSION_ACTIVE)
          setState(ISP_CALL_REQUESTED);

        if (!asyncCommand.isActive && state == ISP_CALL_REQUESTED) {
          setState(ISP_CALLING);
          cmdDialTelephone(adapterConfiguration.isValid()
                               ? adapterConfiguration.fields._ispNumber1
                               : FALLBACK_ISP_NUMBER);
        }
        break;
      }
      case UserRequest::Type::DNS_QUERY: {
        if (state != PPP_ACTIVE) {
          popRequest();
          return;
        }

        if (!asyncCommand.isActive && !request.commandSent) {
          cmdDNSQuery(request.send.data, request.send.size);
          request.commandSent = true;
        }
        break;
      }
      case UserRequest::Type::OPEN_CONNECTION: {
        if (state != PPP_ACTIVE) {
          popRequest();
          return;
        }

        if (!asyncCommand.isActive && !request.commandSent) {
          if (request.connectionType == ConnectionType::TCP)
            cmdOpenTCPConnection(request.ip, request.port);
          else
            cmdOpenUDPConnection(request.ip, request.port);
          request.commandSent = true;
        }
        break;
      }
      case UserRequest::Type::CLOSE_CONNECTION: {
        if (state != PPP_ACTIVE) {
          popRequest();
          return;
        }

        if (!asyncCommand.isActive && !request.commandSent) {
          if (request.connectionType == ConnectionType::TCP)
            cmdCloseTCPConnection(request.connectionId);
          else
            cmdCloseUDPConnection(request.connectionId);
          request.commandSent = true;
        }
        break;
      }
      case UserRequest::Type::TRANSFER: {
        if (state != CALL_ESTABLISHED && state != PPP_ACTIVE) {
          popRequest();
          return;
        }

        if (!asyncCommand.isActive && !request.commandSent) {
          cmdTransferData(request.connectionId, request.send.data,
                          request.send.size);
          request.commandSent = true;
        }
        break;
      }
      case UserRequest::Type::HANG_UP: {
        if (state != CALL_ESTABLISHED && state != PPP_ACTIVE) {
          popRequest();
          return;
        }
        if (!asyncCommand.isActive)
          cmdHangUpTelephone();
        break;
      }
      case UserRequest::Type::SHUTDOWN: {
        if (state != SHUTDOWN_REQUESTED)
          setState(SHUTDOWN_REQUESTED);

        if (!asyncCommand.isActive) {
          setState(ENDING_SESSION);
          cmdEndSession();
          popRequest();
        }
        break;
      }
      default: {
      }
    }
  }

  void processNewFrame() {
    switch (state) {
      case WAITING_TO_START: {
        waitFrames--;

        if (waitFrames == 0) {
          setState(STARTING_SESSION);
          cmdBeginSession();
        }
        break;
      }
      case WAITING_32BIT_SWITCH: {
        waitFrames--;

        if (waitFrames == 0) {
          setState(READING_CONFIGURATION);
          cmdReadConfigurationData(0, CONFIGURATION_DATA_CHUNK);
        }
        break;
      }
      case SESSION_ACTIVE: {
        if (!asyncCommand.isActive)
          cmdWaitForTelephoneCall();

        break;
      }
      case WAITING_8BIT_SWITCH: {
        waitFrames--;

        if (waitFrames == 0) {
          error = {};
          setState(SHUTDOWN);
        }
        break;
      }
      default: {
      }
    }
  }

  void processAsyncCommand() {
    if (asyncCommand.result != CommandResult::SUCCESS) {
      if (shouldAbortOnCommandFailure())
        return abort(Error::Type::COMMAND_FAILED);
      else
        abort(Error::Type::COMMAND_FAILED, false);  // (log the error)
    }

    _LMLOG_("%s $%X [%d]",
            asyncCommand.direction == AsyncCommand::Direction::SENDING ? ">!"
                                                                       : "<!",
            asyncCommand.cmd.header.pureCommandId(),
            asyncCommand.cmd.header.size);

    if (asyncCommand.direction == AsyncCommand::Direction::SENDING) {
      receiveCommandAsync();
      return;
    }

    if (asyncCommand.respondsTo(COMMAND_TELEPHONE_STATUS)) {
      if (asyncCommand.cmd.header.size != 3)
        return abort(Error::Type::WEIRD_RESPONSE);
      if (state == CALL_ESTABLISHED) {
        if (!isBitHigh(asyncCommand.cmd.data.bytes[0], 2)) {
          // (call terminated)
          setState(SESSION_ACTIVE);
        }
      }
      return;
    }

    switch (state) {
      case STARTING_SESSION: {
        if (!asyncCommand.respondsTo(COMMAND_BEGIN_SESSION))
          return;
        if (asyncCommand.cmd.header.size != LOGIN_PARTS_SIZE)
          return abort(Error::Type::WEIRD_RESPONSE);

        for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++) {
          if (asyncCommand.cmd.data.bytes[i] != LOGIN_PARTS[i])
            return abort(Error::Type::WEIRD_RESPONSE);
        }

        setState(ACTIVATING_SIO32);
        cmdSIO32(true);
        break;
      }
      case ACTIVATING_SIO32: {
        if (asyncCommand.respondsTo(COMMAND_RESET)) {
          // If the adapter responds to a 0x16 instead of 0x18,
          // it's libmobile telling us that SIO32 is not supported.
          // In that case, we continue using SIO8.
          setState(READING_CONFIGURATION);
          cmdReadConfigurationData(0, CONFIGURATION_DATA_CHUNK);
          return;
        }
        if (!asyncCommand.respondsTo(COMMAND_SIO32))
          return;

        setState(WAITING_32BIT_SWITCH);
        waitFrames = INIT_WAIT_FRAMES;
        linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                          LinkSPI::DataSize::SIZE_32BIT);
        break;
      }
      case READING_CONFIGURATION: {
        if (!asyncCommand.respondsTo(COMMAND_READ_CONFIGURATION_DATA))
          return;

        u32 offset = asyncCommand.cmd.data.bytes[0];
        u32 sizeWithOffsetByte = asyncCommand.cmd.header.size;
        if (asyncCommand.result != CommandResult::SUCCESS ||
            sizeWithOffsetByte != CONFIGURATION_DATA_CHUNK + 1 ||
            (offset != 0 && offset != CONFIGURATION_DATA_CHUNK))
          return abort(Error::Type::WEIRD_RESPONSE);

        for (u32 i = 0; i < CONFIGURATION_DATA_CHUNK; i++)
          adapterConfiguration.bytes[offset + i] =
              asyncCommand.cmd.data.bytes[1 + i];

        if (offset == 0) {
          cmdReadConfigurationData(CONFIGURATION_DATA_CHUNK,
                                   CONFIGURATION_DATA_CHUNK);
        } else {
          setISPNumber();
          setState(SESSION_ACTIVE);
        }
        break;
      }
      case SESSION_ACTIVE: {
        if (!asyncCommand.respondsTo(COMMAND_WAIT_FOR_TELEPHONE_CALL))
          return;

        if (asyncCommand.result == CommandResult::SUCCESS) {
          setState(CALL_ESTABLISHED);
          role = Role::RECEIVER;
        } else {
          // (no call received)
        }
        break;
      }
      case CALLING: {
        if (!asyncCommand.respondsTo(COMMAND_DIAL_TELEPHONE))
          return;

        if (asyncCommand.result == CommandResult::SUCCESS) {
          setState(CALL_ESTABLISHED);
          role = Role::CALLER;
        } else {
          // (call failed)
          setState(SESSION_ACTIVE);
        }
        break;
      }
      case CALL_ESTABLISHED: {
        if (asyncCommand.respondsTo(COMMAND_HANG_UP_TELEPHONE)) {
          setState(SESSION_ACTIVE);
          return;
        }
        if (!asyncCommand.respondsTo(COMMAND_TRANSFER_DATA))
          return;
        if (userRequests.isEmpty())
          return abort(Error::Type::WTF);
        auto request = userRequests.peekRef();

        handleTransferDataResponse(request);

        break;
      }
      case ISP_CALLING: {
        if (!asyncCommand.respondsTo(COMMAND_DIAL_TELEPHONE))
          return;
        if (userRequests.isEmpty())
          return abort(Error::Type::WTF);
        auto request = userRequests.peekRef();
        if (request->type != UserRequest::PPP_LOGIN)
          return abort(Error::Type::WTF);

        if (asyncCommand.result == CommandResult::SUCCESS) {
          setState(PPP_LOGIN);
          cmdISPLogin(request->loginId, request->password);
        } else {
          // (ISP call failed)
          setState(SESSION_ACTIVE);
        }
        request->finished = true;

        break;
      }
      case PPP_LOGIN: {
        if (!asyncCommand.respondsTo(COMMAND_ISP_LOGIN))
          return;
        if (asyncCommand.result != CommandResult::SUCCESS)
          return abort(Error::Type::PPP_LOGIN_FAILED);

        setState(PPP_ACTIVE);
        break;
      }
      case PPP_ACTIVE: {
        if (asyncCommand.respondsTo(COMMAND_HANG_UP_TELEPHONE)) {
          setState(SESSION_ACTIVE);
          return;
        }

        if (userRequests.isEmpty())
          return;
        auto request = userRequests.peekRef();

        if (asyncCommand.respondsTo(COMMAND_DNS_QUERY)) {
          if (request->type != UserRequest::DNS_QUERY)
            return abort(Error::Type::WTF);

          if (asyncCommand.result == CommandResult::SUCCESS) {
            if (asyncCommand.cmd.header.size != 4)
              return abort(Error::Type::WEIRD_RESPONSE);
            for (u32 i = 0; i < 4; i++)
              request->dns->ipv4[i] = asyncCommand.cmd.data.bytes[i];
            request->dns->success = true;
          } else {
            request->dns->success = false;
          }

          request->dns->completed = true;
          request->finished = true;
        } else if (asyncCommand.respondsTo(COMMAND_OPEN_TCP_CONNECTION) ||
                   asyncCommand.respondsTo(COMMAND_OPEN_UDP_CONNECTION)) {
          if (request->type != UserRequest::OPEN_CONNECTION)
            return abort(Error::Type::WTF);

          if (asyncCommand.result == CommandResult::SUCCESS) {
            if (asyncCommand.cmd.header.size != 1)
              return abort(Error::Type::WEIRD_RESPONSE);
            request->open->connectionId = asyncCommand.cmd.data.bytes[0];
            request->open->success = true;
          } else {
            request->open->success = false;
          }

          request->open->completed = true;
          request->finished = true;
        } else if (asyncCommand.respondsTo(COMMAND_CLOSE_TCP_CONNECTION) ||
                   asyncCommand.respondsTo(COMMAND_CLOSE_UDP_CONNECTION)) {
          if (request->type != UserRequest::CLOSE_CONNECTION)
            return abort(Error::Type::WTF);

          request->close->success =
              asyncCommand.result == CommandResult::SUCCESS;
          request->close->completed = true;
          request->finished = true;
        } else if (asyncCommand.respondsTo(COMMAND_TRANSFER_DATA)) {
          if (request->type != UserRequest::TRANSFER)
            return abort(Error::Type::WTF);

          handleTransferDataResponse(request);
        } else if (asyncCommand.respondsTo(COMMAND_CONNECTION_CLOSED)) {
          if (request->type != UserRequest::TRANSFER)
            return abort(Error::Type::WTF);

          // (connection closed)
          request->receive->success = false;
          request->receive->completed = true;
          request->finished = true;
        }
        break;
      }
      case ENDING_SESSION: {
        if (!asyncCommand.respondsTo(COMMAND_END_SESSION))
          return;

        setState(WAITING_8BIT_SWITCH);
        waitFrames = INIT_WAIT_FRAMES;
        linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                          LinkSPI::DataSize::SIZE_8BIT);
        break;
      }
      default: {
      }
    }
  }

  void handleTransferDataResponse(UserRequest* request) {
    if (request->type != UserRequest::TRANSFER)
      return abort(Error::Type::WTF);

    if (asyncCommand.result == CommandResult::SUCCESS) {
      if (asyncCommand.cmd.header.size == 0)
        return abort(Error::Type::WEIRD_RESPONSE);

      u32 size = asyncCommand.cmd.header.size - 1;
      for (u32 i = 0; i < size; i++)
        request->receive->data[i] = asyncCommand.cmd.data.bytes[1 + i];

      request->receive->data[size] = '\0';
      // (just for convenience when using strings, the buffer is big enough)

      request->receive->size = size;
      request->receive->success = true;
    } else {
      request->receive->success = false;
    }

    request->receive->completed = true;
    request->finished = true;
  }

  void processLoosePacket(u32 newData) {
    switch (state) {
      case PINGING: {
        setState(WAITING_TO_START);
        waitFrames = INIT_WAIT_FRAMES;
        break;
      }
      default: {
      }
    }
  }

  void cmdBeginSession() {
    for (u32 i = 0; i < LOGIN_PARTS_SIZE; i++)
      addData(LOGIN_PARTS[i], i == 0);
    sendCommandAsync(buildCommand(COMMAND_BEGIN_SESSION, true));
  }

  void cmdEndSession() { sendCommandAsync(buildCommand(COMMAND_END_SESSION)); }

  void cmdDialTelephone(const char* phoneNumber) {
    addData(DIAL_PHONE_FIRST_BYTE[adapterType], true);
    for (u32 i = 0; i < std::strlen(phoneNumber); i++)
      addData(phoneNumber[i]);
    sendCommandAsync(buildCommand(COMMAND_DIAL_TELEPHONE, true));
  }

  void cmdHangUpTelephone() {
    sendCommandAsync(buildCommand(COMMAND_HANG_UP_TELEPHONE, true));
  }

  void cmdWaitForTelephoneCall() {
    sendCommandAsync(buildCommand(COMMAND_WAIT_FOR_TELEPHONE_CALL));
  }

  void cmdTransferData(u8 connectionId, const u8* data, u8 size) {
    addData(connectionId, true);
    for (u32 i = 0; i < size; i++)
      addData(data[i]);
    sendCommandAsync(buildCommand(COMMAND_TRANSFER_DATA, true));
  }

  void cmdTelephoneStatus() {
    sendCommandAsync(buildCommand(COMMAND_TELEPHONE_STATUS));
  }

  void cmdSIO32(bool enabled) {
    addData(enabled, true);
    sendCommandAsync(buildCommand(COMMAND_SIO32, true));
  }

  void cmdReadConfigurationData(u8 offset, u8 size) {
    addData(offset, true);
    addData(CONFIGURATION_DATA_CHUNK);
    sendCommandAsync(buildCommand(COMMAND_READ_CONFIGURATION_DATA, true));
  }

  void cmdISPLogin(const char* loginId, const char* password) {
    u32 loginIdLength = std::strlen(loginId);
    addData(loginIdLength, true);
    for (u32 i = 0; i < loginIdLength; i++)
      addData(loginId[i]);

    u32 passwordLength = std::strlen(password);
    addData(passwordLength);
    for (u32 i = 0; i < passwordLength; i++)
      addData(password[i]);

    bool isConfigured = isConfigurationValid();
    for (u32 i = 0; i < 4; i++)
      addData(isConfigured ? adapterConfiguration.fields.primaryDNS[i] : 0);
    for (u32 i = 0; i < 4; i++)
      addData(isConfigured ? adapterConfiguration.fields.secondaryDNS[i] : 0);

    sendCommandAsync(buildCommand(COMMAND_ISP_LOGIN, true));
  }

  void cmdOpenTCPConnection(const u8* ip, u16 port) {
    for (u32 i = 0; i < 4; i++)
      addData(ip[i], i == 0);
    addData(msB16(port));
    addData(lsB16(port));
    sendCommandAsync(buildCommand(COMMAND_OPEN_TCP_CONNECTION, true));
  }

  void cmdCloseTCPConnection(u8 connectionId) {
    addData(connectionId);
    sendCommandAsync(buildCommand(COMMAND_CLOSE_TCP_CONNECTION, true));
  }

  void cmdOpenUDPConnection(const u8* ip, u16 port) {
    for (u32 i = 0; i < 4; i++)
      addData(ip[i], i == 0);
    addData(msB16(port));
    addData(lsB16(port));
    sendCommandAsync(buildCommand(COMMAND_OPEN_UDP_CONNECTION, true));
  }

  void cmdCloseUDPConnection(u8 connectionId) {
    addData(connectionId);
    sendCommandAsync(buildCommand(COMMAND_CLOSE_UDP_CONNECTION, true));
  }

  void cmdDNSQuery(const u8* data, u8 size) {
    for (int i = 0; i < size; i++)
      addData(data[i], i == 0);
    sendCommandAsync(buildCommand(COMMAND_DNS_QUERY, true));
  }

  void setISPNumber() {
    static const char BCD[16] = "0123456789#*cde";

    for (u32 i = 0; i < 8; i++) {
      u8 b = adapterConfiguration.fields.configurationSlot1[i];
      adapterConfiguration.fields._ispNumber1[i * 2] = BCD[b >> 4];
      adapterConfiguration.fields._ispNumber1[i * 2 + 1] = BCD[b & 0xF];
    }
  }

  void pushRequest(UserRequest request) {
    request.timeout = 0;
    request.finished = false;
    userRequests.syncPush(request);
  }

  void popRequest() {
    auto request = userRequests.peekRef();
    request->cleanup();
    request->finished = true;
    userRequests.pop();
  }

  bool shouldAbortOnStateTimeout() {
    return state > NEEDS_RESET && state < SESSION_ACTIVE;
  }

  bool shouldAbortOnRequestTimeout() { return true; }

  bool shouldAbortOnCommandFailure() {
    u8 commandId = asyncCommand.relatedCommandId();
    return asyncCommand.direction == AsyncCommand::Direction::SENDING ||
           (commandId != COMMAND_WAIT_FOR_TELEPHONE_CALL &&
            commandId != COMMAND_DIAL_TELEPHONE && !isAsyncRequest(commandId));
  }

  bool isAsyncRequest(u8 commandId) {
    return commandId == COMMAND_DNS_QUERY ||
           commandId == COMMAND_OPEN_TCP_CONNECTION ||
           commandId == COMMAND_CLOSE_TCP_CONNECTION ||
           commandId == COMMAND_OPEN_UDP_CONNECTION ||
           commandId == COMMAND_CLOSE_UDP_CONNECTION ||
           commandId == COMMAND_TRANSFER_DATA;
  }

  void addData(u8 value, bool start = false) {
    if (start) {
      nextCommandDataSize = 0;
      nextCommandData = PacketData{};
    }
    nextCommandData.bytes[nextCommandDataSize] = value;
    nextCommandDataSize++;
  }

  void copyString(char* target, const char* source, u32 length) {
    u32 len = std::strlen(source);

    for (u32 i = 0; i < length + 1; i++)
      if (i < len)
        target[i] = source[i];
      else
        target[i] = '\0';
  }

  void setState(State newState) {
    role = Role::NO_P2P_CONNECTION;
    State oldState = state;
    state = newState;
    timeoutStateFrames = 0;
    pingFrameCount = 0;
    _LMLOG_("!! new state: %d -> %d", oldState, newState);
    (void)oldState;
  }

  void abort(Error::Type errorType, bool fatal = true) {
    auto newError = Error{
        .type = errorType,
        .state = state,
        .cmdId = asyncCommand.relatedCommandId(),
        .cmdResult = asyncCommand.result,
        .cmdErrorCode = asyncCommand.errorCode,
        .cmdIsSending =
            asyncCommand.direction == AsyncCommand::Direction::SENDING,

        .reqType = userRequests.isEmpty() ? -1 : userRequests.peek().type};

    _LMLOG_(
        "!! %s:\n  error: %d\n  cmdId: %s$%X\n  cmdResult: %d\n  "
        "cmdErrorCode: %d",
        fatal ? "aborted" : "failed", newError.type,
        newError.cmdIsSending ? ">" : "<", newError.cmdId, newError.cmdResult,
        newError.cmdErrorCode);
    (void)newError;

    if (fatal) {
      error = newError;
      resetState();
      stop();
    }
  }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    setState(NEEDS_RESET);

    this->adapterConfiguration = AdapterConfiguration{};
    this->userRequests.clear();
    this->asyncCommand.reset();
    this->waitFrames = 0;
    this->timeoutStateFrames = 0;
    this->role = Role::NO_P2P_CONNECTION;
    this->nextCommandDataSize = 0;
    this->hasPendingTransfer = false;
    this->pendingTransfer = 0;
    this->adapterType = AdapterType::UNKNOWN;

    userRequests.syncClear();
  }

  void stop() {
    stopTimer();
    linkSPI->deactivate();
  }

  void start() {
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS,
                      LinkSPI::DataSize::SIZE_8BIT);

    setState(PINGING);
    transferAsync(0);
  }

  void stopTimer() {
    Link::_REG_TM[config.timerId].cnt =
        Link::_REG_TM[config.timerId].cnt & (~Link::_TM_ENABLE);
  }

  void startTimer(u16 interval) {
    Link::_REG_TM[config.timerId].start = -interval;
    Link::_REG_TM[config.timerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | BASE_FREQUENCY;
  }

  void sendCommandAsync(Command command) {
    _LMLOG_(">> $%X [%d] (...)", command.header.commandId, command.header.size);
    asyncCommand.reset();
    asyncCommand.cmd = command;
    asyncCommand.isActive = true;

    if (isSIO32Mode())  // Magic+Header
      advance32(buildU32(command.magicBytes.magic1, command.magicBytes.magic2,
                         command.header.commandId, command.header._unused_));
    else  // Magic Bytes (1)
      advance8(command.magicBytes.magic1);
  }

  void receiveCommandAsync() {
    _LMLOG_("<< ...");
    asyncCommand.reset();
    asyncCommand.direction = AsyncCommand::Direction::RECEIVING;
    asyncCommand.isActive = true;

    if (isSIO32Mode())
      transferAsync(GBA_WAITING_32BIT);
    else
      transferAsync(GBA_WAITING);
  }

  void sendAsyncCommandSIO8(u32 newData) {
    const u8* commandBytes = (const u8*)&asyncCommand.cmd;
    u32 mainSize = PREAMBLE_SIZE + asyncCommand.cmd.header.size;

    if (asyncCommand.transferred < mainSize) {
      // Magic Bytes (2) + Packet Header + Packet Data
      advance8(commandBytes[asyncCommand.transferred]);
    } else if (asyncCommand.transferred < mainSize + CHECKSUM_SIZE) {
      // Packet Checksum
      commandBytes += PREAMBLE_SIZE + LINK_MOBILE_COMMAND_TRANSFER_BUFFER;
      advance8(commandBytes[asyncCommand.transferred - mainSize]);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      advance8(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance8(ACK_SENDER);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 2) {
      // Acknowledgement Signal (3)
      if (newData != (asyncCommand.cmd.header.commandId ^ OR_VALUE))
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
    }
  }

  void sendAsyncCommandSIO32(u32 newData) {
    u32 dataSize = asyncCommand.cmd.header.size;
    u32 alignment = dataSize % 4;
    u32 padding = alignment != 0 ? 4 - alignment : 0;
    u32 mainSize = PREAMBLE_SIZE + dataSize + padding;

    if (asyncCommand.transferred == 4) {
      // Header+Data || Header+Checksum
      advance32(dataSize > 0
                    ? buildU32(asyncCommand.cmd.header._unusedSizeHigh_,
                               asyncCommand.cmd.header.size,
                               asyncCommand.cmd.data.bytes[0],
                               asyncCommand.cmd.data.bytes[1])
                    : buildU32(asyncCommand.cmd.header._unusedSizeHigh_,
                               asyncCommand.cmd.header.size,
                               asyncCommand.cmd.checksum.high,
                               asyncCommand.cmd.checksum.low));
    } else if (asyncCommand.transferred < mainSize) {
      // Data || Data+Checksum
      u32 transferredDataCount = asyncCommand.transferred - PREAMBLE_SIZE;
      u32 pendingDataCount = (dataSize + padding) - transferredDataCount;
      advance32(
          pendingDataCount > 2
              ? buildU32(asyncCommand.cmd.data.bytes[transferredDataCount],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 1],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 2],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 3])
              : buildU32(asyncCommand.cmd.data.bytes[transferredDataCount],
                         asyncCommand.cmd.data.bytes[transferredDataCount + 1],
                         asyncCommand.cmd.checksum.high,
                         asyncCommand.cmd.checksum.low));
    } else if (asyncCommand.transferred < mainSize + 4) {
      // Acknowledgement Signal (1)
      advance32(buildU32(DEVICE_GBA | OR_VALUE, ACK_SENDER, 0, 0));
    } else {
      // Acknowledgement Signal (2)
      u16 ackData = msB32(newData);
      if (!isSupportedAdapter(msB16(ackData)))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      if (lsB16(ackData) != (asyncCommand.cmd.header.commandId ^ OR_VALUE))
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
    }
  }

  void receiveAsyncCommandSIO8(u32 newData) {
    u8* commandBytes = (u8*)&asyncCommand.cmd;
    u32 mainSize = PREAMBLE_SIZE + asyncCommand.cmd.header.size;

    if (asyncCommand.transferred == 0) {
      // Magic Bytes (1)
      if (newData == ADAPTER_WAITING)
        return transferAsync(GBA_WAITING);
      if (newData != COMMAND_MAGIC_VALUE1)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == 1) {
      // Magic Bytes (1)
      if (newData != COMMAND_MAGIC_VALUE2)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred < PREAMBLE_SIZE) {
      // Packet Header
      commandBytes[asyncCommand.transferred] = newData;
      if (asyncCommand.cmd.header._unusedSizeHigh_ != 0 ||
          asyncCommand.cmd.header.size >
              LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH)
        return asyncCommand.fail(CommandResult::WEIRD_DATA_SIZE);
      advance8(GBA_WAITING);
      if (asyncCommand.transferred == PREAMBLE_SIZE)
        asyncCommand.expectedChecksum = asyncCommand.cmd.header.sum();
    } else if (asyncCommand.transferred < mainSize) {
      // Packet Data
      commandBytes[asyncCommand.transferred] = newData;
      asyncCommand.expectedChecksum += newData;
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize) {
      // Packet Checksum (1)
      if (newData != msB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance8(GBA_WAITING);
    } else if (asyncCommand.transferred == mainSize + 1) {
      // Packet Checksum (2)
      if (newData != lsB16(asyncCommand.expectedChecksum))
        return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
      advance8(DEVICE_GBA | OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE) {
      // Acknowledgement Signal (1)
      if (!isSupportedAdapter(newData))
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      advance8(asyncCommand.cmd.header.commandId ^ OR_VALUE);
    } else if (asyncCommand.transferred == mainSize + CHECKSUM_SIZE + 1) {
      // Acknowledgement Signal (2)
      if (newData != ACK_SENDER)
        return asyncCommand.fail(CommandResult::INVALID_COMMAND_ACK);
      asyncCommand.finish();
    }
  }

  void receiveAsyncCommandSIO32(u32 newData) {
    u32 dataSize = asyncCommand.cmd.header.size;
    u32 alignment = dataSize % 4;
    u32 padding = alignment != 0 ? 4 - alignment : 0;
    u32 mainSize = PREAMBLE_SIZE + dataSize + padding;

    if (asyncCommand.transferred == 0) {
      // Magic+Header
      if (newData == ADAPTER_WAITING || newData == ADAPTER_WAITING_32BIT)
        return transferAsync(GBA_WAITING_32BIT);
      u16 magic = msB32(newData);
      u16 firstHalfHeader = lsB32(newData);
      if (msB16(magic) != COMMAND_MAGIC_VALUE1 ||
          lsB16(magic) != COMMAND_MAGIC_VALUE2)
        return asyncCommand.fail(CommandResult::INVALID_MAGIC_BYTES);
      asyncCommand.cmd.header.commandId = msB16(firstHalfHeader);
      asyncCommand.cmd.header._unused_ = lsB16(firstHalfHeader);
      advance32(GBA_WAITING_32BIT);
    } else if (asyncCommand.transferred == 4) {
      // Header+Data || Header+Checksum
      u16 secondHalfHeader = msB32(newData);
      asyncCommand.cmd.header._unusedSizeHigh_ = msB16(secondHalfHeader);
      asyncCommand.cmd.header.size = lsB16(secondHalfHeader);
      if (asyncCommand.cmd.header._unusedSizeHigh_ != 0 ||
          asyncCommand.cmd.header.size >
              LINK_MOBILE_MAX_COMMAND_TRANSFER_LENGTH)
        return asyncCommand.fail(CommandResult::WEIRD_DATA_SIZE);
      asyncCommand.expectedChecksum = asyncCommand.cmd.header.sum();
      if (asyncCommand.cmd.header.size > 0) {
        u16 firstData = lsB32(newData);
        u8 b0 = msB16(firstData), b1 = lsB16(firstData);
        asyncCommand.cmd.data.bytes[0] = b0;
        asyncCommand.cmd.data.bytes[1] = b1;
        asyncCommand.expectedChecksum += b0 + b1;
      } else {
        u16 checksum = lsB32(newData);
        if (checksum != asyncCommand.expectedChecksum)
          return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
        asyncCommand.cmd.checksum.high = msB16(checksum);
        asyncCommand.cmd.checksum.low = lsB16(checksum);
      }
      advance32(GBA_WAITING_32BIT);
    } else if (asyncCommand.transferred < mainSize) {
      // Data || Data+Checksum
      u32 transferredDataCount = asyncCommand.transferred - PREAMBLE_SIZE;
      u32 pendingDataCount = (dataSize + padding) - transferredDataCount;
      if (pendingDataCount > 2) {
        u16 dataHigh = msB32(newData);
        u16 dataLow = lsB32(newData);
        u8 b0 = msB16(dataHigh), b1 = lsB16(dataHigh), b2 = msB16(dataLow),
           b3 = lsB16(dataLow);
        asyncCommand.cmd.data.bytes[transferredDataCount] = b0;
        asyncCommand.cmd.data.bytes[transferredDataCount + 1] = b1;
        asyncCommand.cmd.data.bytes[transferredDataCount + 2] = b2;
        asyncCommand.cmd.data.bytes[transferredDataCount + 3] = b3;
        asyncCommand.expectedChecksum += b0 + b1 + b2 + b3;
        advance32(GBA_WAITING_32BIT);
      } else {
        u16 lastData = msB32(newData);
        u8 b0 = msB16(lastData), b1 = lsB16(lastData);
        asyncCommand.cmd.data.bytes[transferredDataCount] = b0;
        asyncCommand.cmd.data.bytes[transferredDataCount + 1] = b1;
        asyncCommand.expectedChecksum += b0 + b1;
        u16 checksum = lsB32(newData);
        if (checksum != asyncCommand.expectedChecksum)
          return asyncCommand.fail(CommandResult::WRONG_CHECKSUM);
        asyncCommand.cmd.checksum.high = msB16(checksum);
        asyncCommand.cmd.checksum.low = lsB16(checksum);
        advance32(buildU32(DEVICE_GBA | OR_VALUE,
                           asyncCommand.cmd.header.commandId ^ OR_VALUE, 0, 0));
      }
    } else {
      // Acknowledgement Signal
      u32 ackData = msB32(newData);
      if (!isSupportedAdapter(msB16(ackData)) || lsB16(ackData) != ACK_SENDER)
        return asyncCommand.fail(CommandResult::INVALID_DEVICE_ID);
      asyncCommand.finish();
    }
  }

  bool isSupportedAdapter(u8 ack) {
    for (u32 i = 0; i < SUPPORTED_DEVICES_SIZE; i++) {
      if ((SUPPORTED_DEVICES[i] | OR_VALUE) == ack) {
        if (adapterType == AdapterType::UNKNOWN)
          adapterType = static_cast<AdapterType>(i);

        return true;
      }
    }

    return false;
  }

  Command buildCommand(u8 type, bool withData = false) {
    Command command;
    command.header.commandId = type;
    command.header._unused_ = 0;
    command.header._unusedSizeHigh_ = 0;
    command.header.size = withData ? (u8)nextCommandDataSize : 0;
    if (withData)
      command.data = nextCommandData;
    u16 checksum = command.header.sum();
    for (u32 i = 0; i < command.header.size; i++)
      checksum += command.data.bytes[i];
    command.checksum.high = msB16(checksum);
    command.checksum.low = lsB16(checksum);

    return command;
  }

  void advance8(u32 data) {
    transferAsync(data);
    asyncCommand.transferred++;
  }

  void advance32(u32 data) {
    transferAsync(data);
    asyncCommand.transferred += 4;
  }

  void transferAsync(u32 data) {
    hasPendingTransfer = true;
    pendingTransfer = data;
    startTimer(WAIT_TICKS[isSIO32Mode()]);
  }

  bool isSIO32Mode() {
    return linkSPI->getDataSize() == LinkSPI::DataSize::SIZE_32BIT;
  }

  static u32 buildU32(u8 msB, u8 byte2, u8 byte3, u8 lsB) {
    return ((msB & 0xFF) << 24) | ((byte2 & 0xFF) << 16) |
           ((byte3 & 0xFF) << 8) | (lsB & 0xFF);
  }
  static u16 buildU16(u8 msB, u8 lsB) { return (msB << 8) | lsB; }
  static u16 msB32(u32 value) { return value >> 16; }
  static u16 lsB32(u32 value) { return value & 0xffff; }
  static u8 msB16(u16 value) { return value >> 8; }
  static u8 lsB16(u16 value) { return value & 0xff; }
  bool isBitHigh(u8 byte, u8 bit) { return (byte >> bit) & 1; }
};

extern LinkMobile* linkMobile;

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_MOBILE_ISR_VBLANK() {
  linkMobile->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_MOBILE_ISR_SERIAL() {
  linkMobile->_onSerial();
}

/**
 * @brief TIMER interrupt handler.
 */
inline void LINK_MOBILE_ISR_TIMER() {
  linkMobile->_onTimer();
}

#undef _LMLOG_

#endif  // LINK_MOBILE_H
