#ifndef LINK_UART_H
#define LINK_UART_H

// --------------------------------------------------------------------------
// An UART handler for the Link Port (8N1, 7N1, 8E1, 7E1, 8O1, 7E1).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkUART* linkUART = new LinkUART();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_SERIAL, LINK_UART_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkUART->activate();
// - 4) Send/read bytes by using:
//       if (linkUART->canSend())
//         linkUART->send(0xFA);
//       if (linkUART->canRead())
//         u8 newByte = linkUART->read();
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#include <tonc_bios.h>
#include <tonc_core.h>

// Buffer size
#define LINK_UART_QUEUE_SIZE 256

#define LINK_UART_BIT_CTS 2
#define LINK_UART_BIT_PARITY_CONTROL 3
#define LINK_UART_BIT_SEND_DATA_FLAG 4
#define LINK_UART_BIT_RECEIVE_DATA_FLAG 5
#define LINK_UART_BIT_ERROR_FLAG 6
#define LINK_UART_BIT_DATA_LENGTH 7
#define LINK_UART_BIT_FIFO_ENABLE 8
#define LINK_UART_BIT_PARITY_ENABLE 9
#define LINK_UART_BIT_SEND_ENABLE 10
#define LINK_UART_BIT_RECEIVE_ENABLE 11
#define LINK_UART_BIT_UART_1 12
#define LINK_UART_BIT_UART_2 13
#define LINK_UART_BIT_IRQ 14
#define LINK_UART_BIT_GENERAL_PURPOSE_LOW 14
#define LINK_UART_BIT_GENERAL_PURPOSE_HIGH 15
#define LINK_UART_BARRIER asm volatile("" ::: "memory")

static volatile char LINK_UART_VERSION[] = "LinkUART/v6.2.3";

void LINK_UART_ISR_SERIAL();

class LinkUART {
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

  bool isActive() { return isEnabled; }

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

  bool canRead() { return !queue.isEmpty(); }
  u8 read() { return queue.pop(); }

  bool canSend() { return !isBitHigh(LINK_UART_BIT_SEND_DATA_FLAG); }
  void send(u8 data) { REG_SIODATA8 = data; }

  void _onSerial() {
    if (!isEnabled)
      return;

    if (hasError()) {
      reset();
      return;
    }

    if (canReceive())
      queue.push((u8)REG_SIODATA8);
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
  U8Queue queue;
  volatile bool isEnabled = false;

  bool hasError() { return isBitHigh(LINK_UART_BIT_ERROR_FLAG); }
  bool canReceive() { return !isBitHigh(LINK_UART_BIT_RECEIVE_DATA_FLAG); }

  void reset() {
    resetState();
    stop();
    start();
  }

  void resetState() { queue.clear(); }

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

  void set8BitData() { setBitHigh(LINK_UART_BIT_DATA_LENGTH); }
  void setParityOn() { setBitHigh(LINK_UART_BIT_PARITY_ENABLE); }
  void setOddParity() { setBitHigh(LINK_UART_BIT_PARITY_CONTROL); }
  void setCTSOn() { setBitHigh(LINK_UART_BIT_CTS); }
  void setFIFOOn() { setBitHigh(LINK_UART_BIT_FIFO_ENABLE); }
  void setInterruptsOn() { setBitHigh(LINK_UART_BIT_IRQ); }
  void setSendOn() { setBitHigh(LINK_UART_BIT_SEND_ENABLE); }
  void setReceiveOn() { setBitHigh(LINK_UART_BIT_RECEIVE_ENABLE); }

  void setUARTMode() {
    REG_RCNT = REG_RCNT & ~(1 << LINK_UART_BIT_GENERAL_PURPOSE_HIGH);
    REG_SIOCNT = (1 << LINK_UART_BIT_UART_1) | (1 << LINK_UART_BIT_UART_2);
    REG_SIOCNT |= config.baudRate;
    REG_SIOMLT_SEND = 0;
  }

  void setGeneralPurposeMode() {
    REG_RCNT = (REG_RCNT & ~(1 << LINK_UART_BIT_GENERAL_PURPOSE_LOW)) |
               (1 << LINK_UART_BIT_GENERAL_PURPOSE_HIGH);
  }

  bool isBitHigh(u8 bit) { return (REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { REG_SIOCNT &= ~(1 << bit); }
};

extern LinkUART* linkUART;

inline void LINK_UART_ISR_SERIAL() {
  linkUART->_onSerial();
}

#endif  // LINK_UART_H
