#include "stdafx.h"
#include "ProtocolParser.h"
#include <boost/endian/conversion.hpp>

std::vector<uint8_t> MessageParser::BrotliUncompress(std::vector<uint8_t>&& in)
{
	std::vector<uint8_t> out;
	if (in.empty()) return out;

	auto* bds = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
	if (!bds) {
		LOG_ERROR("[MessageParser] Create BrotliDecoder failed");
		return out;
	}
	size_t avail_in = in.size();
	const uint8_t* next_in = in.data();
	size_t avail_out = 51200;
	std::vector<uint8_t> buf(avail_out);

	while (true) {
		uint8_t* next_out = buf.data();
		auto r = BrotliDecoderDecompressStream(bds, &avail_in, &next_in, &avail_out, &next_out, nullptr);

		size_t produced = buf.size() - avail_out;
		avail_out = buf.size();  // reset output 
		if (produced) out.insert(out.end(), buf.data(), buf.data() + produced);

		if (r == BROTLI_DECODER_RESULT_SUCCESS) {
			BrotliDecoderDestroyInstance(bds);
			return out;
		}
		if (r == BROTLI_DECODER_RESULT_ERROR) {
			auto err = BrotliDecoderGetErrorCode(bds);
			LOG_ERROR("Brotli ERROR: code={}, msg={}", (int)err, BrotliDecoderErrorString(err));
			BrotliDecoderDestroyInstance(bds);
			return out;
		}
		if (r == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
			// never should be happened
			LOG_WARN("Needs more input, but input exhausted");
			break;
		}
	}
	BrotliDecoderDestroyInstance(bds);
	return out;
}

std::vector<uint8_t> MessageParser::BuildBinaryMessage(const Message& mes)
{
	std::vector<uint8_t> data;
	uint32_t plen_be = boost::endian::native_to_big(mes.header.packetLength);
	uint16_t hlen_be = boost::endian::native_to_big(mes.header.headerLength);
	uint16_t ver_be = boost::endian::native_to_big(mes.header.version);
	uint32_t op_be = boost::endian::native_to_big(mes.header.operation);
	uint32_t  seq_be = boost::endian::native_to_big(mes.header.sequenceID);
	data.insert(data.end(), reinterpret_cast<uint8_t*>(&plen_be), reinterpret_cast<uint8_t*>(&plen_be) + sizeof(plen_be));
	data.insert(data.end(), reinterpret_cast<uint8_t*>(&hlen_be), reinterpret_cast<uint8_t*>(&hlen_be) + sizeof(hlen_be));
	data.insert(data.end(), reinterpret_cast<uint8_t*>(&ver_be), reinterpret_cast<uint8_t*>(&ver_be) + sizeof(ver_be));
	data.insert(data.end(), reinterpret_cast<uint8_t*>(&op_be), reinterpret_cast<uint8_t*>(&op_be) + sizeof(op_be));
	data.insert(data.end(), reinterpret_cast<uint8_t*>(&seq_be), reinterpret_cast<uint8_t*>(&seq_be) + sizeof(seq_be));
	data.append_range(mes.body);
	return data;
}

std::vector<MessageRe> MessageParser::ParseBinaryMessage(const std::vector<uint8_t>& data)
{
	std::vector<MessageRe> mess;
	if (data.size() == 0) {
		return mess;
	}
	Packet header;
	header.packetLength = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(data.data()));
	header.headerLength = boost::endian::big_to_native(*reinterpret_cast<const uint16_t*>(data.data() + 4));
	header.version = boost::endian::big_to_native(*reinterpret_cast<const uint16_t*>(data.data() + 6));
	header.operation = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(data.data() + 8));
	header.sequenceID = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(data.data() + 12));
	//LOG_DEBUG("[MessageBuilder]: Message header info {{header length is {}, packet length is {}, version is {}, opcode is {}, squence id is {} }}", header.header_length, header.packet_length, header.version, header.operation, header.sequence_id);
	if (header.packetLength == header.headerLength || header.packetLength == 0) {
		return mess;
	}
	if (header.version == 0) {
		MessageRe mes;
		mes.type = static_cast<MessageType>(header.operation);
		mes.body = std::string(data.data() + header.headerLength, data.data() + header.packetLength);
		mess.push_back(std::move(mes));
	}
	else if (header.version == 1) {
		MessageRe mes;
		mes.type = static_cast<MessageType>(header.operation);
		mes.body = std::string(data.data() + header.headerLength, data.data() + header.packetLength);
		mess.push_back(std::move(mes));
	}
	else if (header.version == 2) {
		LOG_WARN("[MessageBuilder]: Message version 2 is not supported yet");
	}
	else if (header.version == 3) {
		auto uncompressed = BrotliUncompress(std::vector<uint8_t>(data.data() + header.headerLength, data.data() + header.packetLength));
		//Packet header_;
		//header_.packet_length = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(uncompressed.data()));
		//header_.header_length = boost::endian::big_to_native(*reinterpret_cast<const uint16_t*>(uncompressed.data() + 4));
		//header_.version = boost::endian::big_to_native(*reinterpret_cast<const uint16_t*>(uncompressed.data() + 6));
		//header_.operation = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(uncompressed.data() + 8));
		//header_.sequence_id = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(uncompressed.data() + 12));
		if (uncompressed.size() == 0) {
			return mess;
		}
		mess.append_range(ParseBinaryMessage(uncompressed));
		return mess;
	}
	else {
		LOG_ERROR("[MessageBuilder]: Unknown message version: {}", header.version);
		return mess;
	}
	if (data.size() > header.packetLength) {
		mess.append_range(ParseBinaryMessage(std::vector<uint8_t>(data.data() + header.packetLength, data.data() + data.size())));
		return mess;
	}
	else {
		return mess;
	}
}

RoomMessager::RoomMessager(size_t size)
	:m_danmuMes(size)
{
}


RoomMessager::~RoomMessager()
{
}

void RoomMessager::SetDanmuQueueSize(size_t size)
{
	m_danmuMes.SetSize(size);
}

std::vector<MessageRe> RoomMessager::ParseBinaryMessage(const std::vector<uint8_t>& data)
{
	auto msgs = m_msgParser.ParseBinaryMessage(data);
	return msgs;

}
