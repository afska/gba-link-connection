#ifndef LINK_SPI_H
#define LINK_SPI_H

// --------------------------------------------------------------------------
// An SPI handler for the Link Port (Normal Mode, 32bits).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkSPI* linkSPI = new LinkSPI();
// - 2) Initialize the library with:
//       linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);
//       // (use LinkSPI::Mode::SLAVE on the other end)
// - 3) Exchange 32-bit data with the other end:
//       u32 data = linkGPIO->transfer(0x1234);
//       // (this blocks the console indefinitely)
// - 4) Exchange data with a cancellation callback:
//       u32 data = linkGPIO->transfer(0x1234, []() {
//         u16 keys = ~REG_KEYS & KEY_ANY;
//         return keys & KEY_START;
//       });
// --------------------------------------------------------------------------
// considerations:
// - when using Normal Mode between two GBAs, use a GBC Link Cable!
// - only use the 2Mbps mode with custom hardware (very short wires)!
// - don't send 0xFFFFFFFF, it's reserved for errors!
// --------------------------------------------------------------------------

#include <tonc_core.h>

#define LINK_SPI_CANCELED 0xffffffff
#define LINK_SPI_SIOCNT_NORMAL 0
#define LINK_SPI_BIT_CLOCK 0
#define LINK_SPI_BIT_CLOCK_SPEED 1
#define LINK_SPI_BIT_SI 2
#define LINK_SPI_BIT_SO 3
#define LINK_SPI_BIT_START 7
#define LINK_SPI_BIT_LENGTH 12
#define LINK_SPI_BIT_IRQ 14
#define LINK_SPI_BIT_GENERAL_PURPOSE_LOW 14
#define LINK_SPI_BIT_GENERAL_PURPOSE_HIGH 15
#define LINK_SPI_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_SPI_SET_LOW(REG, BIT) REG &= ~(1 << BIT)

class LinkSPI {
 public:
  enum Mode { SLAVE, MASTER_256KBPS, MASTER_2MBPS };

  bool isActive() { return isEnabled; }

  void activate(Mode mode, bool waitMode = false) {
    this->mode = mode;
    this->waitMode = waitMode;

    setNormalMode();
    set32BitPackets();
    disableTransfer();

    if (mode == SLAVE)
      setSlaveMode();
    else {
      setMasterMode();

      if (mode == MASTER_256KBPS)
        set256KbpsSpeed();
      else if (mode == MASTER_2MBPS)
        set2MbpsSpeed();
    }

    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
    stopTransfer();
    disableTransfer();
    setGeneralPurposeMode();
  }

  u32 transfer(u32 data) {
    return transfer(data, []() { return false; });
  }

  template <typename F>
  u32 transfer(u32 data, F cancel) {
    setData(data);
    enableTransfer();

    while (isMaster() && waitMode && !isSlaveReady())
      if (cancel()) {
        disableTransfer();
        return LINK_SPI_CANCELED;
      }

    startTransfer();

    while (!isReady())
      if (cancel()) {
        stopTransfer();
        disableTransfer();
        return LINK_SPI_CANCELED;
      }

    disableTransfer();
    return getData();
  }

  Mode getMode() { return mode; }
  bool isWaitModeActive() { return waitMode; }

 private:
  Mode mode = Mode::SLAVE;
  bool waitMode = false;
  bool isEnabled = false;

  void setNormalMode() {
    LINK_SPI_SET_LOW(REG_RCNT, LINK_SPI_BIT_GENERAL_PURPOSE_HIGH);
    REG_SIOCNT = LINK_SPI_SIOCNT_NORMAL;
  }

  void setGeneralPurposeMode() {
    LINK_SPI_SET_LOW(REG_RCNT, LINK_SPI_BIT_GENERAL_PURPOSE_LOW);
    LINK_SPI_SET_HIGH(REG_RCNT, LINK_SPI_BIT_GENERAL_PURPOSE_HIGH);
  }

  void setData(u32 data) { REG_SIODATA32 = data; }
  u32 getData() { return REG_SIODATA32; }

  void enableTransfer() { setBitLow(LINK_SPI_BIT_SO); }
  void disableTransfer() { setBitHigh(LINK_SPI_BIT_SO); }
  void startTransfer() { setBitHigh(LINK_SPI_BIT_START); }
  void stopTransfer() { setBitLow(LINK_SPI_BIT_START); }
  bool isReady() { return !isBitHigh(LINK_SPI_BIT_START); }
  bool isSlaveReady() { return !isBitHigh(LINK_SPI_BIT_SI); }

  void set32BitPackets() { setBitHigh(LINK_SPI_BIT_LENGTH); }
  void setMasterMode() { setBitHigh(LINK_SPI_BIT_CLOCK); }
  void setSlaveMode() { setBitLow(LINK_SPI_BIT_CLOCK); }
  void set256KbpsSpeed() { setBitLow(LINK_SPI_BIT_CLOCK_SPEED); }
  void set2MbpsSpeed() { setBitHigh(LINK_SPI_BIT_CLOCK_SPEED); }

  bool isMaster() { return mode != SLAVE; }
  bool isBitHigh(u8 bit) { return (REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { LINK_SPI_SET_HIGH(REG_SIOCNT, bit); }
  void setBitLow(u8 bit) { LINK_SPI_SET_LOW(REG_SIOCNT, bit); }
};

extern LinkSPI* linkSPI;

#endif  // LINK_SPI_H
