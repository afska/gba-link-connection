#ifndef LINK_WIRELESS_OPEN_SDK_H
#define LINK_WIRELESS_OPEN_SDK_H

// --------------------------------------------------------------------------
// An open-source implementation of the "official" Wireless Adapter protocol.
// --------------------------------------------------------------------------
// TODO: Document
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <tonc_math.h>
#include "LinkRawWireless.hpp"

#define LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_WORDS 23
#define LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_SERVER 87
#define LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_CLIENT 16
#define LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER 3
#define LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT 2
#define LINK_WIRELESS_OPEN_SDK_HEADER_MASK_SERVER \
  ((1 << (LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER * 8)) - 1)
#define LINK_WIRELESS_OPEN_SDK_HEADER_MASK_CLIENT \
  ((1 << (LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT * 8)) - 1)
#define LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER     \
  (LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_SERVER - \
   LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER)
#define LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_CLIENT     \
  (LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_CLIENT - \
   LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT)
#define LINK_WIRELESS_OPEN_SDK_MAX_PACKETS_SERVER     \
  (LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_SERVER / \
   LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER)
#define LINK_WIRELESS_OPEN_SDK_MAX_PACKETS_CLIENT     \
  (LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_BYTES_CLIENT / \
   LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT)

static volatile char LINK_WIRELESS_OPEN_SDK_VERSION[] =
    "LinkWirelessOpenSDK/v6.2.0";

class LinkWirelessOpenSDK {
 public:  // TODO: Build some private methods
  template <class T>
  struct SendBuffer {
    T header;
    std::array<u32, LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_WORDS> data;
    u32 dataSize = 0;
    u32 totalByteCount = 0;
  };
  // TODO: SlotState enum

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
  struct ServerPacket {
    ServerSDKHeader header;
    u8 payload[LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER];
  };
  struct ServerResponse {
    ServerPacket packets[LINK_WIRELESS_OPEN_SDK_MAX_PACKETS_SERVER];
    u32 packetsSize = 0;
  };
  struct ParentData {
    ServerResponse response;
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
  struct ClientPacket {
    ClientSDKHeader header;
    u8 payload[LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_CLIENT];
  };
  struct ClientResponse {
    ClientPacket packets[LINK_WIRELESS_OPEN_SDK_MAX_PACKETS_CLIENT];
    u32 packetsSize = 0;
  };
  struct ChildrenData {
    ClientResponse responses[4];
  };

  ChildrenData getChildrenData(LinkRawWireless::ReceiveDataResponse response) {
    u8* buffer = (u8*)response.data;
    u32 bufferSize = response.dataSize * 4;

    u32 cursor = 0;
    ChildrenData childrenData;

    for (u32 i = 1; i < LINK_RAW_WIRELESS_MAX_PLAYERS) {
      ClientResponse* clientResponse = childrenData.responses[i - 1];
      u32 remainingBytes = response.sentBytes[i];

      while (remainingBytes >= LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT) {
        ClientPacket* packet =
            &clientResponse->packets[clientResponse->packetsSize];

        packet->header = parseClientHeader(*((u16*)(buffer + cursor)));
        cursor += LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT;
        remainingBytes -= LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT;

        if (header.payloadSize > 0 &&
            header.payloadSize <= LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_CLIENT &&
            remainingBytes >= packet->header.payloadSize) {
          for (u32 j = 0; j < header.payloadSize; j++)
            packet->payload[j] = buffer[cursor++];
          remainingBytes -= header.payloadSize;
        }

        clientResponse->packetsSize++;
      }
    }

    return childrenData;
  }

  SendBuffer<ServerSDKHeader> createServerBuffer(const u8* fullPayload,
                                                 u32 fullPayloadSize,
                                                 u8 n,
                                                 u8 phase,
                                                 u8 slotState,
                                                 u32 offset = 0,
                                                 u8 targetSlots = 0b1111) {
    SendBuffer<ServerSDKHeader> buffer;
    u32 payloadSize =
        min(fullPayloadSize, LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER);

    buffer.header.isACK = 0;
    buffer.header.targetSlots = targetSlots;
    buffer.header.payloadSize = payloadSize;
    buffer.header.n = n;
    buffer.header.phase = phase;
    buffer.header.slotState = slotState;
    u32 headerInt = serializeServerHeader(buffer.header);

    buffer.data[buffer.dataSize++] =
        offset < fullPayloadSize ? (fullPayload[offset] << 24) | headerInt
                                 : headerInt;

    for (u32 i = 1; i < payloadSize; i += 4) {
      u32 word = 0;
      for (u32 j = 0; j < 4; j++) {
        if (offset + i + j < fullPayloadSize &&
            i + j < LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER) {
          u8 byte = fullPayload[offset + i + j];
          word |= byte << (j * 8);
        }
      }
      buffer.data[buffer.dataSize++] = word;
    }

    buffer.totalByteCount =
        LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER + payloadSize;

    return buffer;
  }

  SendBuffer<ServerSDKHeader> createServerACKBuffer(
      ClientSDKHeader clientHeader) {
    SendBuffer<ServerSDKHeader> buffer;

    buffer.header = createACKHeaderFor(clientHeader, 0);
    u32 headerInt = serializeServerHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    buffer.totalByteCount = LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER;

    return buffer;
  }

  // SendBuffer<ClientSDKHeader> createClientBuffer(const u8* fullPayload,
  //                                                u32 fullPayloadSize,
  //                                                u8 n,
  //                                                u8 phase,
  //                                                u8 slotState,
  //                                                u32 offset = 0) {}
  // TODO: IMPLEMENT

  SendBuffer<ClientSDKHeader> createClientACKBuffer(
      ServerSDKHeader serverHeader) {
    SendBuffer<ClientSDKHeader> buffer;

    buffer.header = createACKHeaderFor(serverHeader);
    u16 headerInt = serializeClientHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    buffer.totalByteCount = LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_CLIENT;

    return buffer;
  }

  ServerSDKHeader createACKHeaderFor(ClientSDKHeader clientHeader,
                                     u8 clientNumber) {
    ServerSDKHeader serverHeader;
    serverHeader.isACK = 1;
    serverHeader.targetSlots = (1 << clientNumber);
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.slotState = clientHeader.slotState;

    return serverHeader;
  }

  ClientSDKHeader createACKHeaderFor(ServerSDKHeader serverHeader) {
    ClientSDKHeader clientHeader;
    clientHeader.isACK = 1;
    clientHeader.payloadSize = 0;
    clientHeader.n = serverHeader.n;
    clientHeader.phase = serverHeader.phase;
    clientHeader.slotState = serverHeader.slotState;

    return clientHeader;
  }

  ClientSDKHeader parseClientHeader(u32 clientHeaderInt) {
    ClientSDKHeaderSerializer clientSerializer;
    clientSerializer.asInt =
        clientHeaderInt & LINK_WIRELESS_OPEN_SDK_HEADER_MASK_CLIENT;
    return clientSerializer.asStruct;
  }

  u16 serializeClientHeader(ClientSDKHeader clientHeader) {
    ClientSDKHeaderSerializer clientSerializer;
    clientSerializer.asStruct = clientHeader;
    return clientSerializer.asInt & LINK_WIRELESS_OPEN_SDK_HEADER_MASK_CLIENT;
  }

  ServerSDKHeader parseServerHeader(u32 serverHeaderInt) {
    ServerSDKHeaderSerializer serverSerializer;
    serverSerializer.asInt =
        serverHeaderInt & LINK_WIRELESS_OPEN_SDK_HEADER_MASK_SERVER;
    return serverSerializer.asStruct;
  }

  u32 serializeServerHeader(ServerSDKHeader serverHeader) {
    ServerSDKHeaderSerializer serverSerializer;
    serverSerializer.asStruct = serverHeader;
    return serverSerializer.asInt & LINK_WIRELESS_OPEN_SDK_HEADER_MASK_SERVER;
  }
};

#endif  // LINK_WIRELESS_OPEN_SDK_H
