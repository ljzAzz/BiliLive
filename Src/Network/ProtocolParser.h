#pragma once
#include <zlib.h>
#include <brotli/decode.h>
#include "Common/Common.h"

enum class MessageType : uint32_t {
	Auth = 7,
	Heartbeat = 2,
	HeartbeatReply = 3,
	Message = 5,
	AuthReply = 8,
};
struct Packet {
	uint32_t packetLength;
	uint16_t headerLength;
	uint16_t version;
	uint32_t operation;
	uint32_t sequenceID;
};
struct Message {
	Packet header;
	std::string body;
};

struct MessageRe {
	MessageType type;
	std::string body;
};

class MessageParser {
private:
	std::vector<uint8_t> BrotliUncompress(std::vector<uint8_t>&& in);
public:
	std::vector<uint8_t> BuildBinaryMessage(const Message& mes);
	std::vector<MessageRe> ParseBinaryMessage(const std::vector<uint8_t>& data);
};

class RoomMessager {
private:
	LockFreeQueue<std::string> m_danmuMes;
	MessageParser m_msgParser;
public:
	RoomMessager(size_t size = 1024);
	~RoomMessager();
	void SetDanmuQueueSize(size_t size);
	std::vector<uint8_t> BuildBinaryMessage(const Message& mes) { return m_msgParser.BuildBinaryMessage(mes); }
	std::vector<MessageRe> ParseBinaryMessage(const std::vector<uint8_t>& data);
};