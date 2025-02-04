#ifndef LINK_UART_H
#define LINK_UART_H

// --------------------------------------------------------------------------
// A UART handler for the Link Port (8N1, 7N1, 8E1, 7E1, 8O1, 7E1).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkUART* linkUART = new LinkUART();
// - 2) Add the required interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_SERIAL, LINK_UART_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkUART->activate();
// - 4) Send/read data by using:
//       linkUART->send(0xFA);
//       linkUART->sendLine("hello");
//       u8 newByte = linkUART->read();
//       char newString[256];
//       linkUART->readLine(newString);
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#ifndef LINK_UART_QUEUE_SIZE
/**
 * @brief Buffer size in bytes.
 */
#define LINK_UART_QUEUE_SIZE 256
#endif

LINK_VERSION_TAG LINK_UART_VERSION = "vLinkUART/v8.0.0";

/**
 * @brief A UART handler for the Link Port (8N1, 7N1, 8E1, 7E1, 8O1, 7E1).
 */
class LinkUART {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using U8Queue = Link::Queue<u8, LINK_UART_QUEUE_SIZE>;

  static constexpr int BIT_CTS = 2;
  static constexpr int BIT_PARITY_CONTROL = 3;
  static constexpr int BIT_SEND_DATA_FLAG = 4;
  static constexpr int BIT_RECEIVE_DATA_FLAG = 5;
  static constexpr int BIT_ERROR_FLAG = 6;
  static constexpr int BIT_DATA_LENGTH = 7;
  static constexpr int BIT_FIFO_ENABLE = 8;
  static constexpr int BIT_PARITY_ENABLE = 9;
  static constexpr int BIT_SEND_ENABLE = 10;
  static constexpr int BIT_RECEIVE_ENABLE = 11;
  static constexpr int BIT_UART_1 = 12;
  static constexpr int BIT_UART_2 = 13;
  static constexpr int BIT_IRQ = 14;
  static constexpr int BIT_GENERAL_PURPOSE_LOW = 14;
  static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

 public:
  enum class BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  enum class DataSize { SIZE_7_BITS, SIZE_8_BITS };
  enum class Parity { NO, EVEN, ODD };

  /**
   * @brief Constructs a new LinkUART object.
   */
  explicit LinkUART() {
    config.baudRate = BaudRate::BAUD_RATE_0;
    config.dataSize = DataSize::SIZE_8_BITS;
    config.parity = Parity::NO;
    config.useCTS = false;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library using a specific UART mode.
   * Defaults: 9600bps, 8-bit data, no parity bit, no CTS_
   * @param baudRate One of the enum values from `LinkUART::BaudRate`.
   * @param dataSize One of the enum values from `LinkUART::DataSize`.
   * @param parity One of the enum values from `LinkUART::Parity`.
   * @param useCTS Enable RTS/CTS flow.
   */
  void activate(BaudRate baudRate = BaudRate::BAUD_RATE_0,
                DataSize dataSize = DataSize::SIZE_8_BITS,
                Parity parity = Parity::NO,
                bool useCTS = false) {
    LINK_READ_TAG(LINK_UART_VERSION);
    static_assert(LINK_UART_QUEUE_SIZE >= 1);

    config.baudRate = baudRate;
    config.dataSize = dataSize;
    config.parity = parity;
    config.useCTS = false;

    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    reset();

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    resetState();
    stop();
  }

  /**
   * @brief Takes a null-terminated `string`, and sends it followed by a `'\n'`
   * character. The null character is not sent.
   * @param string The null-terminated string.
   * \warning Blocks the system until completion.
   */
  void sendLine(const char* string) {
    sendLine(string, []() { return false; });
  }

  /**
   * @brief Takes a null-terminated `string`, and sends it followed by a `'\n'`
   * character. The null character is not sent.
   * @param string The null-terminated string.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  void sendLine(const char* string, F cancel) {
    for (u32 i = 0; string[i] != '\0'; i++) {
      while (!canSend())
        if (cancel())
          return;
      send(string[i]);
    }
    send('\n');
  }

  /**
   * @brief Reads characters into `string` until finding a `'\n'` character or a
   * character `limit` is reached. A null terminator is added at the end.
   * Returns `false` if the limit has been reached without finding a newline
   * character.
   * @param string The output string buffer.
   * @param limit The character limit.
   * \warning Blocks the system until completion.
   */
  bool readLine(char* string, u32 limit = LINK_UART_QUEUE_SIZE) {
    return readLine(string, []() { return false; }, limit);
  }

  /**
   * @brief Reads characters into `string` until finding a `'\n'` character or a
   * character `limit` is reached. A null terminator is added at the end.
   * Returns `false` if the limit has been reached without finding a newline
   * character.
   * @param string The output string buffer.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * @param limit The character limit.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  bool readLine(char* string, F cancel, u32 limit = LINK_UART_QUEUE_SIZE) {
    u32 readBytes = 0;
    char lastChar = '\0';
    bool aborted = false;

    while (lastChar != '\n') {
      while (!canRead())
        if (cancel())
          return false;
      string[readBytes++] = lastChar = read();
      if (readBytes >= limit - 1) {
        aborted = true;
        break;
      }
    }

    string[readBytes] = '\0';
    return !aborted && readBytes > 1;
  }

  /**
   * @brief Sends `size` bytes from `buffer`, starting at byte `offset`.
   * @param buffer The source buffer.
   * @param size The size in bytes.
   * @param offset The starting offset.
   */
  void send(const u8* buffer, u32 size, u32 offset = 0) {
    for (u32 i = 0; i < size; i++)
      send(buffer[offset + i]);
  }

  /**
   * @brief Tries to read `size` bytes into `(u8*)(buffer + offset)`. Returns
   * the number of read bytes.
   * @param buffer The target buffer.
   * @param size The size in bytes.
   * @param offset The offset from target buffer.
   */
  u32 read(u8* buffer, u32 size, u32 offset = 0) {
    for (u32 i = 0; i < size; i++) {
      if (!canRead())
        return i;
      buffer[offset + i] = read();
    }

    return size;
  }

  /**
   * @brief Returns whether there are bytes to read or not.
   */
  [[nodiscard]] bool canRead() { return !incomingQueue.isEmpty(); }

  /**
   * @brief Returns whether there is room to send new messages or not.
   */
  [[nodiscard]] bool canSend() { return !outgoingQueue.isFull(); }

  /**
   * @brief Returns the number of bytes available for read.
   */
  [[nodiscard]] u32 availableForRead() { return incomingQueue.size(); }

  /**
   * @brief Returns the number of bytes available for send (buffer size - queued
   * bytes).
   */
  [[nodiscard]] u32 availableForSend() {
    return LINK_UART_QUEUE_SIZE - outgoingQueue.size();
  }

  /**
   * @brief Reads a byte. Returns 0 if nothing is found.
   */
  u8 read() { return incomingQueue.syncPop(); }

  /**
   * @brief Sends a `data` byte.
   * @param data The value to be sent.
   */
  void send(u8 data) { outgoingQueue.syncPush(data); }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled || hasError())
      return;

    if (!incomingQueue.isReading() && canReceive())
      incomingQueue.push((u8)Link::_REG_SIODATA8);

    if (!outgoingQueue.isWriting() && canTransfer() && needsTransfer())
      Link::_REG_SIODATA8 = outgoingQueue.pop();
  }

 private:
  struct Config {
    BaudRate baudRate;
    DataSize dataSize;
    Parity parity;
    bool useCTS;
  };

  Config config;
  U8Queue incomingQueue;
  U8Queue outgoingQueue;
  volatile bool isEnabled = false;

  bool canReceive() { return !isBitHigh(BIT_RECEIVE_DATA_FLAG); }
  bool canTransfer() { return !isBitHigh(BIT_SEND_DATA_FLAG); }
  bool hasError() { return isBitHigh(BIT_ERROR_FLAG); }
  bool needsTransfer() { return !outgoingQueue.isEmpty(); }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    LINK_BARRIER;
    incomingQueue.clear();
    outgoingQueue.clear();
    LINK_BARRIER;
  }

  void stop() { setGeneralPurposeMode(); }

  void start() {
    setUARTMode();
    if (config.dataSize == DataSize::SIZE_8_BITS)
      set8BitData();
    if (config.parity > Parity::NO) {
      if (config.parity == Parity::ODD)
        setOddParity();
      setParityOn();
    }
    if (config.useCTS)
      setCTSOn();
    setFIFOOn();
    setInterruptsOn();
    setSendOn();
    setReceiveOn();
  }

  void set8BitData() { setBitHigh(BIT_DATA_LENGTH); }
  void setParityOn() { setBitHigh(BIT_PARITY_ENABLE); }
  void setOddParity() { setBitHigh(BIT_PARITY_CONTROL); }
  void setCTSOn() { setBitHigh(BIT_CTS); }
  void setFIFOOn() { setBitHigh(BIT_FIFO_ENABLE); }
  void setInterruptsOn() { setBitHigh(BIT_IRQ); }
  void setSendOn() { setBitHigh(BIT_SEND_ENABLE); }
  void setReceiveOn() { setBitHigh(BIT_RECEIVE_ENABLE); }

  void setUARTMode() {
    Link::_REG_RCNT = Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_HIGH);
    Link::_REG_SIOCNT = (1 << BIT_UART_1) | (1 << BIT_UART_2);
    Link::_REG_SIOCNT |= (int)config.baudRate;
    Link::_REG_SIOMLT_SEND = 0;
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  bool isBitHigh(u8 bit) { return (Link::_REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { Link::_REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { Link::_REG_SIOCNT &= ~(1 << bit); }
};

extern LinkUART* linkUART;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_UART_ISR_SERIAL() {
  linkUART->_onSerial();
}

#endif  // LINK_UART_H
