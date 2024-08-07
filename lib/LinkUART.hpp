#ifndef LINK_UART_H
#define LINK_UART_H

// --------------------------------------------------------------------------
// A UART handler for the Link Port (8N1, 7N1, 8E1, 7E1, 8O1, 7E1).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkUART* linkUART = new LinkUART();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_SERIAL, LINK_UART_ISR_SERIAL);
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

#include "_link_common.hpp"

// Buffer size
#define LINK_UART_QUEUE_SIZE 256

static volatile char LINK_UART_VERSION[] = "LinkUART/v7.0.0";

#define LINK_UART_BARRIER asm volatile("" ::: "memory")

class LinkUART {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using vs32 = volatile signed int;
  using vu32 = volatile unsigned int;

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
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  enum DataSize { SIZE_7_BITS, SIZE_8_BITS };
  enum Parity { NO, EVEN, ODD };

  explicit LinkUART() {
    this->config.baudRate = BAUD_RATE_0;
    this->config.dataSize = SIZE_8_BITS;
    this->config.parity = NO;
    this->config.useCTS = false;
  }

  [[nodiscard]] bool isActive() { return isEnabled; }

  void activate(BaudRate baudRate = BAUD_RATE_0,
                DataSize dataSize = SIZE_8_BITS,
                Parity parity = NO,
                bool useCTS = false) {
    this->config.baudRate = baudRate;
    this->config.dataSize = dataSize;
    this->config.parity = parity;
    this->config.useCTS = false;

    LINK_UART_BARRIER;
    isEnabled = false;
    LINK_UART_BARRIER;

    reset();

    LINK_UART_BARRIER;
    isEnabled = true;
    LINK_UART_BARRIER;
  }

  void deactivate() {
    LINK_UART_BARRIER;
    isEnabled = false;
    LINK_UART_BARRIER;

    resetState();
    stop();
  }

  void sendLine(const char* string) {
    sendLine(string, []() { return false; });
  }

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

  bool readLine(char* string, u32 limit = LINK_UART_QUEUE_SIZE) {
    return readLine(string, []() { return false; }, limit);
  }

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

  void send(const u8* buffer, u32 size, u32 offset = 0) {
    for (u32 i = 0; i < size; i++)
      send(buffer[offset + i]);
  }

  u32 read(u8* buffer, u32 size, u32 offset = 0) {
    for (u32 i = 0; i < size; i++) {
      if (!canRead())
        return i;
      buffer[offset + i] = read();
    }

    return size;
  }

  [[nodiscard]] bool canRead() { return !incomingQueue.isEmpty(); }
  [[nodiscard]] bool canSend() { return !outgoingQueue.isFull(); }
  [[nodiscard]] u32 availableForRead() { return incomingQueue.size(); }
  [[nodiscard]] u32 availableForSend() {
    return LINK_UART_QUEUE_SIZE - outgoingQueue.size();
  }

  u8 read() {
    LINK_UART_BARRIER;
    isReading = true;
    LINK_UART_BARRIER;

    u8 data = incomingQueue.pop();

    LINK_UART_BARRIER;
    isReading = false;
    LINK_UART_BARRIER;

    return data;
  }

  void send(u8 data) {
    LINK_UART_BARRIER;
    isAdding = true;
    LINK_UART_BARRIER;

    outgoingQueue.push(data);

    LINK_UART_BARRIER;
    isAdding = false;
    LINK_UART_BARRIER;
  }

  void _onSerial() {
    if (!isEnabled || hasError())
      return;

    if (!isReading && canReceive())
      incomingQueue.push((u8)Link::_REG_SIODATA8);

    if (!isAdding && canTransfer() && needsTransfer())
      Link::_REG_SIODATA8 = outgoingQueue.pop();
  }

 private:
  class U8Queue {
   public:
    void push(u8 item) {
      if (isFull())
        pop();

      rear = (rear + 1) % LINK_UART_QUEUE_SIZE;
      arr[rear] = item;
      count++;
    }

    u16 pop() {
      if (isEmpty())
        return 0;

      auto x = arr[front];
      front = (front + 1) % LINK_UART_QUEUE_SIZE;
      count--;

      return x;
    }

    u16 peek() {
      if (isEmpty())
        return 0;

      return arr[front];
    }

    void clear() {
      front = count = 0;
      rear = -1;
    }

    u32 size() { return count; }
    bool isEmpty() { return size() == 0; }
    bool isFull() { return size() == LINK_UART_QUEUE_SIZE; }

   private:
    u8 arr[LINK_UART_QUEUE_SIZE];
    vs32 front = 0;
    vs32 rear = -1;
    vu32 count = 0;
  };

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
  volatile bool isReading = false;
  volatile bool isAdding = false;

  bool canReceive() { return !isBitHigh(BIT_RECEIVE_DATA_FLAG); }
  bool canTransfer() { return !isBitHigh(BIT_SEND_DATA_FLAG); }
  bool hasError() { return isBitHigh(BIT_ERROR_FLAG); }
  bool needsTransfer() { return outgoingQueue.size() > 0; }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() {
    incomingQueue.clear();
    outgoingQueue.clear();
  }

  void stop() { setGeneralPurposeMode(); }

  void start() {
    setUARTMode();
    if (config.dataSize == SIZE_8_BITS)
      set8BitData();
    if (config.parity > NO) {
      if (config.parity == ODD)
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
    Link::_REG_SIOCNT |= config.baudRate;
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

inline void LINK_UART_ISR_SERIAL() {
  linkUART->_onSerial();
}

#endif  // LINK_UART_H
