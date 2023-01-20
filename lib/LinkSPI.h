#ifndef LINK_SPI_H
#define LINK_SPI_H

// --------------------------------------------------------------------------
// An SPI handler for the Link Port (Normal Mode, 32bits).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkSPI* linkSPI = new LinkSPI(LinkSPI::Mode::MASTER_256KBPS);
//       // (use LinkSPI::Mode::SLAVE on the other end)
// - 2) Initialize the library with:
//       linkSPI->activate();
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
// --------------------------------------------------------------------------

#include <tonc_core.h>

#define LINK_SPI_CANCELED 0xffffffff
#define LINK_SPI_RCNT_NORMAL 0
#define LINK_SPI_SIOCNT_NORMAL 0
#define LINK_SPI_RCNT_GENERAL_PURPOSE (1 << 15)
#define LINK_SPI_SIOCNT_GENERAL_PURPOSE 0
#define LINK_SPI_BIT_CLOCK 0
#define LINK_SPI_BIT_CLOCK_SPEED 1
#define LINK_SPI_BIT_SI 2
#define LINK_SPI_BIT_SO 3
#define LINK_SPI_BIT_START 7
#define LINK_SPI_BIT_LENGTH 12
#define LINK_SPI_BIT_IRQ 14
#define LINK_SPI_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_SPI_SET_LOW(REG, BIT) REG &= ~(1 << BIT)

class LinkSPI {
 public:
  enum Mode { SLAVE, MASTER_256KBPS, MASTER_2MBPS };

  explicit LinkSPI(Mode mode) { this->mode = mode; }

  bool isActive() { return isEnabled; }

  void activate() {
    setNormalMode();
    set32BitPackets();

    if (mode == SLAVE)
      setSlaveMode();
    else {
      setMasterMode();

      if (mode == MASTER_256KBPS)
        set256KbpsSpeed();
      else if (mode == MASTER_2MBPS)
        set2MbpsSpeed();
    }

    disableTransfer();
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
    if (isEnabled && isMaster())
      activate();

    setData(data);
    enableTransfer();

    while (isMaster() && !isSlaveReady())
      if (cancel())
        return LINK_SPI_CANCELED;

    startTransfer();

    while (!isReady())
      if (cancel())
        return LINK_SPI_CANCELED;

    disableTransfer();
    return getData();
  }

  Mode getMode() { return mode; }

 private:
  Mode mode;
  bool isEnabled = false;

  void setNormalMode() {
    REG_RCNT = LINK_SPI_RCNT_NORMAL;
    REG_SIOCNT = LINK_SPI_SIOCNT_NORMAL;
  }

  void setGeneralPurposeMode() {
    REG_RCNT = LINK_SPI_RCNT_GENERAL_PURPOSE;
    REG_SIOCNT = LINK_SPI_SIOCNT_GENERAL_PURPOSE;
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
