#ifndef LINK_WIRELESS_MULTIBOOT_H
#define LINK_WIRELESS_MULTIBOOT_H

// --------------------------------------------------------------------------
// A Wireless Multiboot tool to send small ROMs from a GBA to up to 4 slaves.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWirelessMultiboot* linkWirelessMultiboot =
//         new LinkWirelessMultiboot();
// - 2) Send the ROM:
//       LinkWirelessMultiboot::Result result = linkWirelessMultiboot->sendRom(
//         romBytes, // for current ROM, use: ((const void*)MEM_EWRAM)
//         romLength, // should be multiple of 0x10
//         []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, transfer will be canceled)
//         }
//       );
//       // `result` should be LinkWirelessMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include "LinkRawWireless.hpp"

#define LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xc0)
#define LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define LINK_WIRELESS_MULTIBOOT_HEADER_SIZE 0xC0

static volatile char LINK_WIRELESS_MULTIBOOT_VERSION[] =
    "LinkWirelessMultiboot/v6.2.0";

class LinkWirelessMultiboot {
  typedef void (*Logger)(std::string);

 public:
  Logger logger = [](std::string str) {};
  enum Result { SUCCESS, INVALID_SIZE, CANCELED, FAILURE };

  struct ServerSDKHeader {
    unsigned int payloadSize : 7;
    unsigned int _unused_ : 2;
    unsigned int phase : 2;
    unsigned int n : 2;
    unsigned int isACK : 1;
    unsigned int slotState : 4;
    unsigned int targetSlots : 4;
  };
  union ServerSDKHeaderSerializer {
    ServerSDKHeader asStruct;
    u32 asInt;
  };

  struct ClientSDKHeader {
    unsigned int payloadSize : 5;
    unsigned int phase : 2;
    unsigned int n : 2;
    unsigned int isACK : 1;
    unsigned int slotState : 4;
  };
  union ClientSDKHeaderSerializer {
    ClientSDKHeader asStruct;
    u16 asInt;
  };

  template <typename F>
  Result sendRom(const u8* rom, u32 romSize, F cancel) {
    // if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE)
    //   return INVALID_SIZE;
    // if (romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
    //   return INVALID_SIZE;
    // if ((romSize % 0x10) != 0)
    //   return INVALID_SIZE;

    // std::vector<u8> bytes = {};

    bool success = true;
    success = link->activate();
    if (!success) {
      logger("cannot activate");
      return FAILURE;
    }
    logger("activated");

    success = link->sendCommand(LINK_RAW_WIRELESS_COMMAND_SETUP,
                                std::vector<u32>{0x003F0120})
                  .success;  // TODO: IMPLEMENT SETUP
    if (!success) {
      logger("setup failed");
      return FAILURE;
    }
    logger("setup ok");

    success = link->broadcast("Multi", "Test", 0b1111111111111111);
    if (!success) {
      logger("broadcast failed");
      return FAILURE;
    }
    logger("broadcast set");

    success = link->startHost();
    if (!success) {
      logger("start host failed");
      return FAILURE;
    }
    logger("host started");

    LinkRawWireless::AcceptConnectionsResponse acceptResponse;
    while (link->playerCount() == 1) {
      link->acceptConnections(acceptResponse);
    }

    logger("connected!");
    linkRawWireless->wait(228 * 20);  // ~300ms

    // HANDSHAKE

    bool hasData = false;
    LinkRawWireless::ReceiveDataResponse response;
    while (!hasData) {
      if (!sendAndExpectData(std::vector<u32>{}, 1, response))
        return FAILURE;
      hasData = response.data.size() > 0;
    }

    logger("data received");
    ClientSDKHeader clientHeader = parseClientHeader(response.data[0]);
    logger("client size: " + std::to_string(clientHeader.payloadSize));
    logger("n: " + std::to_string(clientHeader.n));
    logger("phase: " + std::to_string(clientHeader.phase));
    logger("ack: " + std::to_string(clientHeader.isACK));
    logger("slotState:" + std::to_string(clientHeader.slotState));

    logger("sending ACK");
  firstack:
    ServerSDKHeader serverHeader;
    serverHeader = createACKFor(clientHeader);
    u32 sndHeader = serializeServerHeader(serverHeader);

    if (!sendAndExpectData(std::vector<u32>{sndHeader}, 3, response))
      return FAILURE;

    if (response.data.size() == 0) {
      goto firstack;
    }
    clientHeader = parseClientHeader(response.data[0]);
    if (clientHeader.n == 1)
      goto firstack;

    if (clientHeader.n == 2 && clientHeader.slotState == 1) {
      logger("N IS NOW 2, slotstate = 1");
    } else {
      logger("Error: weird packet");
      return FAILURE;
    }

  secondack:
    serverHeader = createACKFor(clientHeader);
    sndHeader = serializeServerHeader(serverHeader);
    if (!sendAndExpectData(std::vector<u32>{sndHeader}, 3, response))
      return FAILURE;

    if (response.data.size() == 0) {
      goto secondack;
    }
    clientHeader = parseClientHeader(response.data[0]);
    if (clientHeader.n == 2 && clientHeader.slotState == 1)
      goto secondack;

    if (clientHeader.n == 1 && clientHeader.slotState == 2) {
      logger("NI STARTED");
    } else {
      logger("NI DIDN'T START");
      return FAILURE;
    }

    while (clientHeader.slotState > 0) {
      link->wait(228);
      serverHeader = createACKFor(clientHeader);
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(std::vector<u32>{sndHeader}, 3, response))
        return FAILURE;

      clientHeader = parseClientHeader(response.data[0]);
    }

    logger("slotState IS NOW 0");

    // ROM START COMMAND
    bool didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 7;
      serverHeader.n = 1;
      serverHeader.phase = 0;
      serverHeader.slotState = 1;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(std::vector<u32>{sndHeader, 0x54, 0x02}, 10,
                             response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK == 1 && clientHeader.n == 1 &&
          clientHeader.phase == 0 && clientHeader.slotState == 1)
        didClientRespond = true;
    }

    logger("READY TO SEND ROM!");

    // ROM START
    u32 transferredBytes = 0;
    u32 n = 1;
    u32 phase = 0;
    bool isRetry = false;
    u32 progress = 0;
    while (transferredBytes < romSize) {
      isRetry = false;
    retry:
      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 84;      // 87 - 3 (serversdkheader)
      serverHeader.n = n;
      serverHeader.phase = phase;
      serverHeader.slotState = 2;
      sndHeader = serializeServerHeader(serverHeader);
      std::vector<u32> data;
      data.push_back(sndHeader | (rom[transferredBytes] << 24));
      // if (!isRetry)
      //   bytes.push_back(rom[transferredBytes]);
      for (u32 i = 1; i < 84; i += 4) {
        u32 d = 0;
        for (u32 j = 0; j < 4; j++) {
          if (transferredBytes + i + j < romSize && i + j < 84) {
            u8 byte = rom[transferredBytes + i + j];
            d |= byte << (j * 8);
            // if (!isRetry)
            //   bytes.push_back(byte);
          }
        }
        data.push_back(d);
      }
      LinkRawWireless::ReceiveDataResponse response;
      if (!sendAndExpectData(data, 87, response)) {
        logger("SendData failed!");
        return FAILURE;
      }
      if (response.data.size() == 0) {
        isRetry = true;
        goto retry;
      }

      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK && clientHeader.n == n &&
          clientHeader.phase == phase) {
        phase++;
        if (phase == 4) {
          phase = 0;
          n++;
          if (n == 4)
            n = 0;
        }
        transferredBytes += 84;
        u32 newProgress = transferredBytes * 100 / romSize;
        if (newProgress != progress) {
          progress = newProgress;
          logger("-> " + std::to_string(transferredBytes * 100 / romSize));
        }
      } else {
        isRetry = true;
        goto retry;
      }
    }

    logger("SEND FINISHED! Confirming...");

    // ROM END COMMAND
    didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 0;
      serverHeader.n = 0;
      serverHeader.phase = 0;
      serverHeader.slotState = 3;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(std::vector<u32>{sndHeader}, 3, response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      if (clientHeader.isACK == 1 && clientHeader.n == 0 &&
          clientHeader.phase == 0 && clientHeader.slotState == 3)
        didClientRespond = true;
    }

    logger("Reconfirming...");

    // ROM END 2 COMMAND
    didClientRespond = false;
    while (!didClientRespond) {
      link->wait(228);

      serverHeader.isACK = 0;
      serverHeader.targetSlots = 0b0001;  //  TODO: Implement
      serverHeader.payloadSize = 0;
      serverHeader.n = 1;
      serverHeader.phase = 0;
      serverHeader.slotState = 0;
      sndHeader = serializeServerHeader(serverHeader);
      if (!sendAndExpectData(std::vector<u32>{sndHeader}, 3, response))
        return FAILURE;
      clientHeader = parseClientHeader(response.data[0]);
      // if (clientHeader.slotState == 0)
      didClientRespond = true;
    }

    logger("SUCCESS!");

    // u32 diffs = 0;
    // for (u32 i = 0; i < romSize; i++) {
    //   if (rom[i] != bytes[i]) {
    //     logger("DIFF AT " + std::to_string(i) + ": " + link->toHex(bytes[i])
    //     +
    //            " vs " + link->toHex(rom[i]));
    //     diffs++;
    //   }
    //   if (diffs > 100)
    //     break;
    // }

    // logger("??");

    return SUCCESS;
  }

  LinkRawWireless* link = new LinkRawWireless();

  ClientSDKHeader lastACK;

 private:
  bool sendAndExpectData(std::vector<u32> data,
                         u32 _bytes,
                         LinkRawWireless::ReceiveDataResponse& response) {
    LinkRawWireless::RemoteCommand remoteCommand;
    bool success = false;
    success = link->sendDataAndWait(data, remoteCommand, _bytes);
    if (!success) {
      logger("senddatawait no");
      return false;
    }
    if (remoteCommand.commandId != 0x28) {
      logger("expected response 0x28");
      logger("but got " + link->toHex(remoteCommand.commandId));
      return false;
    }
    success = link->receiveData(response);
    if (!success) {
      logger("receive data failed");
      return false;
    }
    return true;
  }

  ServerSDKHeader createACKFor(ClientSDKHeader clientHeader) {
    ServerSDKHeader serverHeader;
    serverHeader.isACK = 1;
    serverHeader.targetSlots = 0b0001;  //  TODO: Implement
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.slotState = clientHeader.slotState;

    return serverHeader;
  }

  ClientSDKHeader parseClientHeader(u32 clientHeaderInt) {
    ClientSDKHeaderSerializer clientSerializer;
    clientSerializer.asInt = clientHeaderInt;
    return clientSerializer.asStruct;
  }

  u32 serializeServerHeader(ServerSDKHeader serverHeader) {
    ServerSDKHeaderSerializer serverSerializer;
    serverSerializer.asStruct = serverHeader;
    return serverSerializer.asInt & 0xffffff;
  }
};

extern LinkWirelessMultiboot* linkWirelessMultiboot;

#endif  // LINK_WIRELESS_MULTIBOOT_H
