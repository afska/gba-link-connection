#ifndef LINK_WIRELESS_OPEN_SDK_H
#define LINK_WIRELESS_OPEN_SDK_H

// --------------------------------------------------------------------------
// An open-source implementation of the "official" Wireless Adapter protocol.
// --------------------------------------------------------------------------
// TODO: Document
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <tonc_math.h>

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
 public:
  template <class T>
  struct SendBuffer {
    T header;
    std::array<u32, LINK_WIRELESS_OPEN_SDK_MAX_TRANSFER_WORDS> data;
    u32 dataSize = 0;
    u32 totalByteCount = 0;
  };

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

  SendBuffer<ServerSDKHeader> createServerBuffer(const u8* fullPayload,
                                                 u32 fullPayloadSize,
                                                 u8 n,
                                                 u8 phase,
                                                 u8 slotState,
                                                 u32 offset,
                                                 u8 targetSlots) {
    SendBuffer<ServerSDKHeader> buffer;
    u32 payloadSize =
        min(fullPayloadSize, LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER);

    buffer.header.isACK = 0;
    buffer.header.targetSlots = targetSlots;
    buffer.header.payloadSize = payloadSize;
    buffer.header.n = n;
    buffer.header.phase = phase;
    buffer.header.slotState = slotState;
    u32 sndHeader = serializeServerHeader(buffer.header);

    if (offset < fullPayloadSize)
      buffer.data[buffer.dataSize++] = (fullPayload[offset] << 24) | sndHeader;

    for (u32 i = 1; i < payloadSize; i += 4) {
      u32 d = 0;
      for (u32 j = 0; j < 4; j++) {
        if (offset + i + j < fullPayloadSize &&
            i + j < LINK_WIRELESS_OPEN_SDK_MAX_PAYLOAD_SERVER) {
          u8 byte = fullPayload[offset + i + j];
          d |= byte << (j * 8);
        }
      }
      buffer.data[buffer.dataSize++] = d;
    }

    buffer.totalByteCount =
        LINK_WIRELESS_OPEN_SDK_HEADER_SIZE_SERVER + payloadSize;

    return buffer;
  }

  ServerSDKHeader createACKFor(ClientSDKHeader clientHeader, u8 clientNumber) {
    ServerSDKHeader serverHeader;
    serverHeader.isACK = 1;
    serverHeader.targetSlots = (1 << clientNumber);
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.slotState = clientHeader.slotState;

    return serverHeader;
  }

  ClientSDKHeader createACKFor(ServerSDKHeader serverHeader) {
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
