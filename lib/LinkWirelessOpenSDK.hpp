#ifndef LINK_WIRELESS_OPEN_SDK_H
#define LINK_WIRELESS_OPEN_SDK_H

// --------------------------------------------------------------------------
// An open-source implementation of the "official" Wireless Adapter protocol.
// --------------------------------------------------------------------------
// - advanced usage only; you only need this if you want to interact with N
// software!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#include "LinkRawWireless.hpp"

LINK_VERSION_TAG LINK_WIRELESS_OPEN_SDK_VERSION = "LinkWirelessOpenSDK/v8.0.0";

/**
 * @brief An open-source implementation of the "official" Wireless Adapter
 * protocol.
 */
class LinkWirelessOpenSDK {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;

 public:
  static constexpr int MAX_TRANSFER_WORDS = 23;
  static constexpr int MAX_TRANSFER_BYTES_SERVER = 87;
  static constexpr int MAX_TRANSFER_BYTES_CLIENT = 16;
  static constexpr int HEADER_SIZE_SERVER = 3;
  static constexpr int HEADER_SIZE_CLIENT = 2;
  static constexpr int HEADER_MASK_SERVER = 0b1111111111111111111111;
  static constexpr int HEADER_MASK_CLIENT = 0b11111111111111;
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
    u32 data[MAX_TRANSFER_WORDS];
    u32 dataSize = 0;
    u32 totalByteCount = 0;
  };

  enum class CommState : unsigned int {
    OFF = 0,
    STARTING = 1,
    COMMUNICATING = 2,
    ENDING = 3,
    DIRECT = 4
  };

  struct SequenceNumber {
    u32 n = 0;
    u32 phase = 0;
    CommState commState = CommState::OFF;

    static SequenceNumber fromPacketId(u32 packetId) {
      return SequenceNumber{.n = ((packetId + 4) / 4) % 4,
                            .phase = packetId % 4,
                            .commState = CommState::COMMUNICATING};
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

        u32 headerInt = (buffer[cursor + 1] << 8) | buffer[cursor];
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

      u32 headerInt = (buffer[cursor + 2] << 16) | (buffer[cursor + 1] << 8) |
                      buffer[cursor];
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
    buffer.header._unused_ = 0;
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
    serverHeader.targetSlots = 1 << clientNumber;
    serverHeader.payloadSize = 0;
    serverHeader.n = clientHeader.n;
    serverHeader.phase = clientHeader.phase;
    serverHeader.commState = clientHeader.commState;
    serverHeader._unused_ = 0;

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

  template <u32 MaxInflightPackets>
  struct Transfer {
   private:
    struct PendingTransfer {
      u32 cursor;
      bool ack;
      bool isActive = false;

      void reset() { isActive = false; }
    };

    struct PendingTransferList {
      PendingTransfer transfers[MaxInflightPackets] = {};

      void reset() {
        for (u32 i = 0; i < MaxInflightPackets; i++)
          transfers[i].reset();
      }

      [[nodiscard]]
      PendingTransfer* max(bool ack = false) {
        int maxCursor = -1;
        int maxI = -1;
        for (u32 i = 0; i < MaxInflightPackets; i++) {
          if (transfers[i].isActive && (int)transfers[i].cursor > maxCursor &&
              (!ack || transfers[i].ack)) {
            maxCursor = transfers[i].cursor;
            maxI = i;
          }
        }
        return maxI > -1 ? &transfers[maxI] : nullptr;
      }

      [[nodiscard]]
      PendingTransfer* minWithoutAck() {
        u32 minCursor = 0xFFFFFFFF;
        int minI = -1;
        for (u32 i = 0; i < MaxInflightPackets; i++) {
          if (transfers[i].isActive && transfers[i].cursor < minCursor &&
              !transfers[i].ack) {
            minCursor = transfers[i].cursor;
            minI = i;
          }
        }
        return minI > -1 ? &transfers[minI] : nullptr;
      }

      void addIfNeeded(u32 newCursor) {
        auto maxTransfer = max();
        if (maxTransfer != nullptr && newCursor <= maxTransfer->cursor)
          return;

        for (u32 i = 0; i < MaxInflightPackets; i++) {
          if (!transfers[i].isActive) {
            transfers[i].cursor = newCursor;
            transfers[i].ack = false;
            transfers[i].isActive = true;
            break;
          }
        }
      }

      int ack(SequenceNumber sequence) {
        int index = findIndex(sequence);
        if (index == -1)
          return -1;

        transfers[index].ack = true;

        auto maxAckTransfer = max(true);
        bool canUpdateCursor = maxAckTransfer != nullptr &&
                               isAckCompleteUpTo(maxAckTransfer->cursor);

        if (canUpdateCursor)
          cleanup();

        return canUpdateCursor ? maxAckTransfer->cursor + 1 : -1;
      }

      void cleanup() {
        for (u32 i = 0; i < MaxInflightPackets; i++) {
          if (transfers[i].isActive && transfers[i].ack)
            transfers[i].isActive = false;
        }
      }

      [[nodiscard]]
      bool isFull() {
        return size() == MaxInflightPackets;
      }

      [[nodiscard]]
      u32 size() {
        u32 size = 0;
        for (u32 i = 0; i < MaxInflightPackets; i++)
          if (transfers[i].isActive)
            size++;
        return size;
      }

     private:
      [[nodiscard]]
      bool isAckCompleteUpTo(u32 cursor) {
        for (u32 i = 0; i < MaxInflightPackets; i++)
          if (transfers[i].isActive && !transfers[i].ack &&
              transfers[i].cursor < cursor)
            return false;
        return true;
      }

      [[nodiscard]]
      int findIndex(SequenceNumber sequence) {
        for (u32 i = 0; i < MaxInflightPackets; i++) {
          if (transfers[i].isActive &&
              SequenceNumber::fromPacketId(transfers[i].cursor) == sequence) {
            return i;
          }
        }

        return -1;
      }
    };

   public:
    u32 cursor = 0;
    PendingTransferList pendingTransferList = {};

    void reset() {
      cursor = 0;
      pendingTransferList.reset();
    }

    [[nodiscard]]
    u32 nextCursor(bool canSendInflightPackets) {
      u32 pendingCount = pendingTransferList.size();

      if (canSendInflightPackets && pendingCount > 0 &&
          pendingCount < MaxInflightPackets) {
        auto max = pendingTransferList.max();
        // (`max` is never null here! but the compiler complains...)
        return max != nullptr ? max->cursor + 1 : 0;
      } else {
        auto minWithoutAck = pendingTransferList.minWithoutAck();
        return minWithoutAck != nullptr ? minWithoutAck->cursor : cursor;
      }
    }

    void addIfNeeded(u32 newCursor) {
      if (newCursor >= cursor)
        pendingTransferList.addIfNeeded(newCursor);
    }

    [[nodiscard]]
    u32 transferred() {
      return cursor * MAX_PAYLOAD_SERVER;
    }

    [[nodiscard]]
    SequenceNumber sequence() {
      return SequenceNumber::fromPacketId(cursor);
    }
  };

 public:
  /**
   * @brief A file transfer from a host to N clients.
   * @tparam MaxInflightPackets Maximum number of packets that can be sent
   * without a confirmation.
   */
  template <u32 MaxInflightPackets>
  class MultiTransfer {
   public:
    /**
     * @brief Constructs a new MultiTransfer object.
     * @param linkWirelessOpenSDK An pointer to a `LinkWirelessOpenSDK`.
     */
    explicit MultiTransfer(LinkWirelessOpenSDK* linkWirelessOpenSDK) {
      this->linkWirelessOpenSDK = linkWirelessOpenSDK;
    }

    /**
     * @brief Configures the file transfer and resets the state.
     * @param fileSize Size of the file.
     * @param connectedClients Number of clients.
     */
    void configure(u32 fileSize, u32 connectedClients) {
      this->fileSize = fileSize;
      this->connectedClients = connectedClients;
      for (u32 i = 0; i < LINK_RAW_WIRELESS_MAX_PLAYERS - 1; i++)
        transfers[i].reset();
      this->finished = false;
      this->cursor = 0;
    }

    /**
     * @brief Returns whether the transfer has completed or not.
     */
    [[nodiscard]]
    bool hasFinished() {
      return finished;
    }

    /**
     * @brief Returns the current cursor (packet number).
     */
    [[nodiscard]]
    u32 getCursor() {
      return cursor;
    }

    /**
     * @brief Returns a `SendBuffer`, ready for use with
     * `LinkRawWireless::sendData(...)` to send the next packet. The internal
     * state is updated to keep track of the transfer.
     * @param fileBytes The pointer to the file bytes. It should always be the
     * same across all calls unless you're changing it on the fly.
     */
    [[nodiscard]]
    SendBuffer<ServerSDKHeader> createNextSendBuffer(const u8* fileBytes) {
      if (finished)
        return SendBuffer<ServerSDKHeader>{};

      u32 offset = cursor * LinkWirelessOpenSDK::MAX_PAYLOAD_SERVER;
      auto sequence = SequenceNumber::fromPacketId(cursor);

      auto sendBuffer = linkWirelessOpenSDK->createServerBuffer(
          fileBytes, fileSize, sequence, 0b1111, offset);

      for (u32 i = 0; i < connectedClients; i++)
        transfers[i].addIfNeeded(cursor);

      return sendBuffer;
    }

    /**
     * @brief Processes a response from `LinkRawWireless::receiveData(...)`,
     * updating the cursor and the internal state.
     * @param response The received response from the adapter.
     * @return The completion percentage (0~100).
     */
    u8 processResponse(LinkRawWireless::ReceiveDataResponse response) {
      if (finished)
        return 100;

      auto childrenData = linkWirelessOpenSDK->getChildrenData(response);
      updateACKs(childrenData);

      auto transferredBytes = minClientTransferredBytes();
      finished = transferredBytes >= fileSize;
      cursor = findMinCursor();
      return Link::_min(transferredBytes * 100 / fileSize, 100);
    }

   private:
    Transfer<MaxInflightPackets> transfers[LINK_RAW_WIRELESS_MAX_PLAYERS - 1] =
        {};

    LinkWirelessOpenSDK* linkWirelessOpenSDK;
    u32 fileSize = 0;
    u32 connectedClients = 0;
    bool finished = false;
    u32 cursor = 0;

    void updateACKs(ChildrenData childrenData) {
      for (u32 i = 0; i < connectedClients; i++) {
        for (u32 j = 0; j < childrenData.responses[i].packetsSize; j++) {
          auto header = childrenData.responses[i].packets[j].header;

          if (header.isACK) {
            int newAckCursor =
                transfers[i].pendingTransferList.ack(header.sequence());
            if (newAckCursor > -1)
              transfers[i].cursor = newAckCursor;
          }
        }
      }
    }

    [[nodiscard]]
    u32 minClientTransferredBytes() {
      return transfers[findMinClient()].transferred();
    }

    [[nodiscard]]
    u32 findMinClient() {
      u32 minTransferredBytes = 0xFFFFFFFF;
      u32 minClient = 0;

      for (u32 i = 0; i < connectedClients; i++) {
        u32 transferred = transfers[i].transferred();
        if (transferred < minTransferredBytes) {
          minTransferredBytes = transferred;
          minClient = i;
        }
      }

      return minClient;
    }

    [[nodiscard]]
    u32 findMinCursor() {
      u32 minNextCursor = 0xFFFFFFFF;

      bool canSendInflightPackets = true;
      for (u32 i = 0; i < connectedClients; i++) {
        if (transfers[i].pendingTransferList.isFull())
          canSendInflightPackets = false;
      }

      for (u32 i = 0; i < connectedClients; i++) {
        u32 nextCursor = transfers[i].nextCursor(canSendInflightPackets);
        if (nextCursor < minNextCursor)
          minNextCursor = nextCursor;
      }

      return minNextCursor;
    }
  };
};

#endif  // LINK_WIRELESS_OPEN_SDK_H
