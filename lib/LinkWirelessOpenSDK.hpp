#ifndef LINK_WIRELESS_OPEN_SDK_H
#define LINK_WIRELESS_OPEN_SDK_H

// --------------------------------------------------------------------------
// An open-source implementation of the "official" Wireless Adapter protocol.
// --------------------------------------------------------------------------
// - Advanced usage only!
// - You only need this if you want to interact with N software.
// --------------------------------------------------------------------------

#include "_link_common.hpp"

#include "LinkRawWireless.hpp"

static volatile char VERSION[] = "LinkWirelessOpenSDK/v7.0.0";

/**
 * @brief An open-source implementation of the "official" Wireless Adapter
 * protocol.
 * \warning Advanced usage only!
 * \warning You only need this if you want to interact with N software.
 */
class LinkWirelessOpenSDK {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

 public:
  static constexpr int MAX_TRANSFER_WORDS = 23;
  static constexpr int MAX_TRANSFER_BYTES_SERVER = 87;
  static constexpr int MAX_TRANSFER_BYTES_CLIENT = 16;
  static constexpr int HEADER_SIZE_SERVER = 3;
  static constexpr int HEADER_SIZE_CLIENT = 2;
  static constexpr int HEADER_MASK_SERVER = (1 << (HEADER_SIZE_SERVER * 8)) - 1;
  static constexpr int HEADER_MASK_CLIENT = (1 << (HEADER_SIZE_CLIENT * 8)) - 1;
  static constexpr int MAX_PAYLOAD_SERVER =
      MAX_TRANSFER_BYTES_SERVER - HEADER_SIZE_SERVER;
  static constexpr int MAX_PAYLOAD_CLIENT =
      MAX_TRANSFER_BYTES_CLIENT - HEADER_SIZE_CLIENT;
  static constexpr int MAX_PACKETS_SERVER =
      MAX_TRANSFER_BYTES_SERVER / HEADER_SIZE_SERVER;
  static constexpr int MAX_PACKETS_CLIENT =
      MAX_TRANSFER_BYTES_CLIENT / HEADER_SIZE_CLIENT;

 public:
  template <class T>
  struct SendBuffer {
    T header;
    std::array<u32, MAX_TRANSFER_WORDS> data;
    u32 dataSize = 0;
    u32 totalByteCount = 0;
  };

  enum CommState : unsigned int {
    OFF = 0,
    STARTING = 1,
    COMMUNICATING = 2,
    ENDING = 3,
    DIRECT = 4
  };

  struct SequenceNumber {
    u32 n = 0;
    u32 phase = 0;
    CommState commState = OFF;

    static SequenceNumber fromPacketId(u32 packetId) {
      return SequenceNumber{.n = ((packetId + 4) / 4) % 4,
                            packetId % 4,
                            .commState = COMMUNICATING};
    }

    bool operator==(const SequenceNumber& other) {
      return n == other.n && phase == other.phase &&
             commState == other.commState;
    }
  };

  struct ServerSDKHeader {
    unsigned int payloadSize : 7;
    unsigned int _unused_ : 2;
    unsigned int phase : 2;
    unsigned int n : 2;
    unsigned int isACK : 1;
    CommState commState : 4;
    unsigned int targetSlots : 4;

    SequenceNumber sequence() {
      return SequenceNumber{.n = n, .phase = phase, .commState = commState};
    }
  };
  union ServerSDKHeaderSerializer {
    ServerSDKHeader asStruct;
    u32 asInt;
  };
  struct ServerPacket {
    ServerSDKHeader header;
    u8 payload[MAX_PAYLOAD_SERVER];
  };
  struct ServerResponse {
    ServerPacket packets[MAX_PACKETS_SERVER];
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
    CommState commState : 4;

    SequenceNumber sequence() {
      return SequenceNumber{.n = n, .phase = phase, .commState = commState};
    }
  };
  union ClientSDKHeaderSerializer {
    ClientSDKHeader asStruct;
    u16 asInt;
  };
  struct ClientPacket {
    ClientSDKHeader header;
    u8 payload[MAX_PAYLOAD_CLIENT];
  };
  struct ClientResponse {
    ClientPacket packets[MAX_PACKETS_CLIENT];
    u32 packetsSize = 0;
  };
  struct ChildrenData {
    ClientResponse responses[4];
  };

  /**
   * @brief Parses the `response` and returns a struct containing all the
   * received packets from the connected clients.
   * @param response The response to be parsed.
   */
  [[nodiscard]]
  ChildrenData getChildrenData(LinkRawWireless::ReceiveDataResponse response) {
    u8* buffer = (u8*)response.data;
    u32 cursor = 0;
    ChildrenData childrenData;

    if (response.sentBytes[1] + response.sentBytes[2] + response.sentBytes[3] +
            response.sentBytes[4] >
        response.dataSize * 4)
      return childrenData;

    for (u32 i = 1; i < LINK_RAW_WIRELESS_MAX_PLAYERS; i++) {
      ClientResponse* clientResponse = &childrenData.responses[i - 1];
      u32 remainingBytes = response.sentBytes[i];

      while (remainingBytes >= HEADER_SIZE_CLIENT) {
        ClientPacket* packet =
            &clientResponse->packets[clientResponse->packetsSize];

        u32 headerInt = *((u16*)(buffer + cursor));
        packet->header = parseClientHeader(headerInt);
        cursor += HEADER_SIZE_CLIENT;
        remainingBytes -= HEADER_SIZE_CLIENT;

        if (packet->header.payloadSize > 0 &&
            packet->header.payloadSize <= MAX_PAYLOAD_CLIENT &&
            remainingBytes >= packet->header.payloadSize) {
          for (u32 j = 0; j < packet->header.payloadSize; j++)
            packet->payload[j] = buffer[cursor++];
          remainingBytes -= packet->header.payloadSize;
        }

        clientResponse->packetsSize++;
      }
    }

    return childrenData;
  }

  /**
   * @brief Parses the `response` and returns a struct containing all the
   * received packets from the host.
   * @param response The response to be parsed.
   */
  [[nodiscard]]
  ParentData getParentData(LinkRawWireless::ReceiveDataResponse response) {
    u8* buffer = (u8*)response.data;
    u32 cursor = 0;
    ParentData parentData;

    if (response.sentBytes[0] > response.dataSize * 4)
      return parentData;

    ServerResponse* serverResponse = &parentData.response;
    u32 remainingBytes = response.sentBytes[0];

    while (remainingBytes >= HEADER_SIZE_SERVER) {
      ServerPacket* packet =
          &serverResponse->packets[serverResponse->packetsSize];

      u32 headerInt = (*((u16*)(buffer + cursor))) |
                      (((*((u8*)(buffer + cursor + 2)))) << 16);
      packet->header = parseServerHeader(headerInt);
      cursor += HEADER_SIZE_SERVER;
      remainingBytes -= HEADER_SIZE_SERVER;

      if (packet->header.payloadSize > 0 &&
          packet->header.payloadSize <= MAX_PAYLOAD_SERVER &&
          remainingBytes >= packet->header.payloadSize) {
        for (u32 j = 0; j < packet->header.payloadSize; j++)
          packet->payload[j] = buffer[cursor++];
        remainingBytes -= packet->header.payloadSize;
      }

      serverResponse->packetsSize++;
    }

    return parentData;
  }

  /**
   * @brief Creates a buffer for the host to send a `fullPayload` with a valid
   * header. If `fullPayloadSize` is higher than `84` (the maximum payload
   * size), the buffer will only contain the **first** `84` bytes (unless an
   * `offset` > 0 is used). A `sequence` number must be created by using
   * `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`. Optionally, a
   * `targetSlots` bit array can be used to exclude some clients from the
   * transmissions (the default is `0b1111`).
   * @param fullPayload A pointer to the payload buffer.
   * @param fullPayloadSize Total size of the payload.
   * @param sequence A sequence number created using
   * `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`.
   * @param targetSlots A bit array that can be used to exclude some clients
   * (the default is `0b1111`).
   * @param offset The offset within the `fullPayload` pointer. Defaults to `0`.
   */
  [[nodiscard]]
  SendBuffer<ServerSDKHeader> createServerBuffer(const u8* fullPayload,
                                                 u32 fullPayloadSize,
                                                 SequenceNumber sequence,
                                                 u8 targetSlots = 0b1111,
                                                 u32 offset = 0) {
    SendBuffer<ServerSDKHeader> buffer;
    u32 payloadSize = Link::_min(fullPayloadSize, MAX_PAYLOAD_SERVER);

    buffer.header.isACK = 0;
    buffer.header.targetSlots = targetSlots;
    buffer.header.payloadSize = payloadSize;
    buffer.header.n = sequence.n;
    buffer.header.phase = sequence.phase;
    buffer.header.commState = sequence.commState;
    u32 headerInt = serializeServerHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    if (offset < fullPayloadSize)
      buffer.data[0] |= fullPayload[offset] << 24;

    for (u32 i = 1; i < payloadSize; i += 4) {
      u32 word = 0;
      for (u32 j = 0; j < 4; j++) {
        if (offset + i + j < fullPayloadSize && i + j < MAX_PAYLOAD_SERVER) {
          u8 byte = fullPayload[offset + i + j];
          word |= byte << (j * 8);
        }
      }
      buffer.data[buffer.dataSize++] = word;
    }

    buffer.totalByteCount = HEADER_SIZE_SERVER + payloadSize;

    return buffer;
  }

  /**
   * @brief Creates a buffer for the host to acknowledge a header received from
   * a certain `clientNumber`.
   * @param clientHeader The header of the received packet.
   * @param clientNumber `(0~3)` The client number that sent the packet.
   */
  [[nodiscard]]
  SendBuffer<ServerSDKHeader> createServerACKBuffer(
      ClientSDKHeader clientHeader,
      u8 clientNumber) {
    SendBuffer<ServerSDKHeader> buffer;

    buffer.header = createACKHeaderFor(clientHeader, clientNumber);
    u32 headerInt = serializeServerHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    buffer.totalByteCount = HEADER_SIZE_SERVER;

    return buffer;
  }

  /**
   * @brief Creates a buffer for the client to send a `fullPayload` with a valid
   * header. If `fullPayloadSize` is higher than `14` (the maximum payload
   * size), the buffer will only contain the **first** `14` bytes (unless an
   * `offset` > 0 is used). A `sequence` number must be created by using
   * `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`.
   * @param fullPayload A pointer to the payload buffer.
   * @param fullPayloadSize Total size of the payload.
   * @param sequence A sequence number created using
   * `LinkWirelessOpenSDK::SequenceNumber::fromPacketId(...)`.
   * @param offset The offset within the `fullPayload` pointer. Defaults to `0`.
   */
  [[nodiscard]]
  SendBuffer<ClientSDKHeader> createClientBuffer(const u8* fullPayload,
                                                 u32 fullPayloadSize,
                                                 SequenceNumber sequence,
                                                 u32 offset = 0) {
    SendBuffer<ClientSDKHeader> buffer;
    u32 payloadSize = Link::_min(fullPayloadSize, MAX_PAYLOAD_CLIENT);

    buffer.header.isACK = 0;
    buffer.header.payloadSize = payloadSize;
    buffer.header.n = sequence.n;
    buffer.header.phase = sequence.phase;
    buffer.header.commState = sequence.commState;
    u16 headerInt = serializeClientHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    if (offset < fullPayloadSize)
      buffer.data[0] |= fullPayload[offset] << 16;
    if (offset + 1 < fullPayloadSize)
      buffer.data[0] |= fullPayload[offset + 1] << 24;

    for (u32 i = 2; i < payloadSize; i += 4) {
      u32 word = 0;
      for (u32 j = 0; j < 4; j++) {
        if (offset + i + j < fullPayloadSize && i + j < MAX_PAYLOAD_CLIENT) {
          u8 byte = fullPayload[offset + i + j];
          word |= byte << (j * 8);
        }
      }
      buffer.data[buffer.dataSize++] = word;
    }

    buffer.totalByteCount = HEADER_SIZE_CLIENT + payloadSize;

    return buffer;
  }

  /**
   * @brief Creates a buffer for the client to acknowledge a header received
   * from the host.
   * @param serverHeader The header of the received packet.
   */
  [[nodiscard]]
  SendBuffer<ClientSDKHeader> createClientACKBuffer(
      ServerSDKHeader serverHeader) {
    SendBuffer<ClientSDKHeader> buffer;

    buffer.header = createACKHeaderFor(serverHeader);
    u16 headerInt = serializeClientHeader(buffer.header);

    buffer.data[buffer.dataSize++] = headerInt;
    buffer.totalByteCount = HEADER_SIZE_CLIENT;

    return buffer;
  }

 private:
  [[nodiscard]]
  ServerSDKHeader createACKHeaderFor(ClientSDKHeader clientHeader,
                                     u8 clientNumber) {
    ServerSDKHeader serverHeader;
    serverHeader.isACK = 1;
    serverHeader.targetSlots = (1 << clientNumber);
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.commState = clientHeader.commState;

    return serverHeader;
  }

  [[nodiscard]]
  ClientSDKHeader createACKHeaderFor(ServerSDKHeader serverHeader) {
    ClientSDKHeader clientHeader;
    clientHeader.isACK = 1;
    clientHeader.payloadSize = 0;
    clientHeader.n = serverHeader.n;
    clientHeader.phase = serverHeader.phase;
    clientHeader.commState = serverHeader.commState;

    return clientHeader;
  }

  [[nodiscard]]
  ClientSDKHeader parseClientHeader(u32 clientHeaderInt) {
    ClientSDKHeaderSerializer clientSerializer;
    clientSerializer.asInt = clientHeaderInt & HEADER_MASK_CLIENT;
    return clientSerializer.asStruct;
  }

  [[nodiscard]]
  u16 serializeClientHeader(ClientSDKHeader clientHeader) {
    ClientSDKHeaderSerializer clientSerializer;
    clientSerializer.asStruct = clientHeader;
    return clientSerializer.asInt & HEADER_MASK_CLIENT;
  }

  [[nodiscard]]
  ServerSDKHeader parseServerHeader(u32 serverHeaderInt) {
    ServerSDKHeaderSerializer serverSerializer;
    serverSerializer.asInt = serverHeaderInt & HEADER_MASK_SERVER;
    return serverSerializer.asStruct;
  }

  [[nodiscard]]
  u32 serializeServerHeader(ServerSDKHeader serverHeader) {
    ServerSDKHeaderSerializer serverSerializer;
    serverSerializer.asStruct = serverHeader;
    return serverSerializer.asInt & HEADER_MASK_SERVER;
  }
};

#endif  // LINK_WIRELESS_OPEN_SDK_H
