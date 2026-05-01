#include "stdafx.h"
#include "UI/RoomViewRenderer.h"
#include "WebSocketClient.h"


WebSocket::WebSocket()
	: m_ctx(ssl::context::tlsv12_client)
	, m_resolver(net::make_strand(m_ioc))
{
}

WebSocket::~WebSocket()
{
}

bool WebSocket::WebSocketHandShake(const Url& url)
{
	if (!SSLResolve(url)) {
		return false;
	}
	auto cookies = "SESSDATA=" + User::GetSessData() + ";bili_jct=" + User::GetBilijct()
		+";buvid3="+User::GetBuvid3();
	m_pWebSocketStream->set_option(
		beast::websocket::stream_base::decorator(
			[=](beast::websocket::request_type& req) {
				req.set(beast::http::field::user_agent, UserAgent);
				req.set(beast::http::field::cookie, cookies);
			}
		)
	);
	beast::error_code ec;

	beast::websocket::response_type res;

	m_pWebSocketStream->handshake(res, url.host, url.path, ec);
	if (ec) {
		LOG_ERROR("[WebSocket]: WebSocket handshake failed: {}", AcpToUtf8(ec.message()));
		return false;
	}

	if (res.result() != beast::http::status::switching_protocols) {
		LOG_ERROR("[WebSocket]: Server did not switch protocols, status: {}", res.result_int());
		return false;
	}
	m_pWebSocketStream->binary(true);
	return true;
}

bool WebSocket::SSLResolve(const Url& url)
{
	auto stream = beast::ssl_stream<beast::tcp_stream>(net::make_strand(m_ioc), m_ctx);
	auto const results = m_resolver.resolve(url.host, url.port);
	if (results.empty()) {
		LOG_ERROR("[WebSocket]: Http Resolve error");
		return false;
	}
	beast::get_lowest_layer(stream).connect(results);

	SSL_set_tlsext_host_name(stream.native_handle(), url.host.data());

	stream.set_verify_mode(boost::asio::ssl::verify_none);

	beast::error_code ec;
	stream.handshake(ssl::stream_base::client, ec);
	if (ec)
	{
		LOG_ERROR("[WebSocket]: Handshake failed: {}", AcpToUtf8(ec.message()));
		return false;
	}
	m_pWebSocketStream.reset();
	m_pWebSocketStream.emplace(std::move(stream));
	LOG_DEBUG("[WebSocket] wss://{}{}", url.host, url.path);
	return true;
}

void WebSocket::Write(const std::vector<uint8_t>& msg, std::function<void(boost::beast::error_code err, size_t size)> wcb)
{
	std::unique_lock<std::mutex>(m_mutex);
	boost::beast::error_code ec;
	try {
		m_pWebSocketStream->write(net::buffer(msg), ec);
		if (ec) {
			LOG_ERROR("[WebSocket] Write err: {}", AcpToUtf8(ec.message()));
		}
	}
	catch (const std::exception& e) {
		LOG_ERROR("[WebSocket] Write exception: {}", AcpToUtf8(e.what()));
		throw std::runtime_error("[WebSocket] Write exception");

	}
}

void WebSocket::Read(std::function<void(boost::beast::error_code err, size_t size)> rcb)
{
	boost::beast::error_code ec;
	try {
		m_pWebSocketStream->read(m_buffer,ec);
		if (ec == boost::beast::websocket::error::closed) {
			LOG_ERROR("[WebSocket] Closed gracefully, code: {}", AcpToUtf8(m_pWebSocketStream->reason().reason.c_str()));
			throw std::runtime_error("[WebSocket] Read exception");
		}
		else if (ec == boost::asio::error::eof) {
			LOG_ERROR("[WebSocket] Tcp connection closed by peer (EOF)");
			throw std::runtime_error("[WebSocket] Read exception");
		}
		else if (ec == boost::asio::error::connection_reset) {
			LOG_ERROR("[WebSocket] Connection reset by peer");
			throw std::runtime_error("[WebSocket] Read exception");
		}
		if (ec) {
			LOG_ERROR("[WebSocket] Read err: {}", AcpToUtf8(ec.message()));
			throw std::runtime_error("[WebSocket] Read exception");
		}
	}
	catch (const std::exception& e) {
		LOG_ERROR("[WebSocket] Read exception: {}", AcpToUtf8(e.what()));
		throw std::runtime_error("[WebSocket] Read exception");
	}
}

void DanmakuClient::SendAuth(uint64_t roomID, const std::string& token)
{
	nlohmann::ordered_json auth;
	auth["uid"] = User::GetUid();
	auth["roomid"] = roomID;
	auth["protover"] = 3;
	auth["platform"] = "web";
	auth["type"] = 2;
	auth["buvid"] = User::GetBuvid3();	
	auth["key"] = token;
	Message msg;
	auto mesbody = nlohmann::to_string(auth);

	msg.body = mesbody;
	msg.header.headerLength = 16;
	msg.header.packetLength = static_cast<uint32_t>(mesbody.length() + 16);
	msg.header.operation = 7;
	msg.header.version = 1;
	msg.header.sequenceID = 1;
	auto data = m_msger.BuildBinaryMessage(msg);
	 
	LOG_DEBUG("[DanmakuClient] Auth packet:\n {}", auth.dump(4));
	LOG_DEBUG("[DanmakuClient] Auth  bin packet : {:spn}", spdlog::to_hex(data));

	try {
		m_webSocket->Write(data, [](boost::beast::error_code err, size_t size) {});
	}
	catch (const std::exception& e) {
		if (m_retryFlag.load(std::memory_order_relaxed) == true) {
			return;
		}
		else if (m_retryFlag.load(std::memory_order_relaxed) == false) {
			m_retryFlag.store(true, std::memory_order_relaxed);
			m_retryFlag.notify_all();
		}
		return;
	}
	try {
		m_webSocket->Read([](boost::beast::error_code err, size_t size) {});
	}
	catch (const std::exception& e) {
		if (m_retryFlag.load(std::memory_order_relaxed) == true) {
			return;
		}
		else if (m_retryFlag.load(std::memory_order_relaxed) == false) {
			m_retryFlag.store(true, std::memory_order_relaxed);
			m_retryFlag.notify_all();
		}
		return;
	}
	m_retryFlag.store(false, std::memory_order_relaxed);
}

void DanmakuClient::Retry()
{
	m_writeFinish.store(false, std::memory_order_relaxed);
	m_readFinish.store(false, std::memory_order_relaxed);
	m_retry();
}

void DanmakuClient::SendHeartbeat()
{
	Message heartbeat;
	heartbeat.body = "";
	heartbeat.header.packetLength = 16;
	heartbeat.header.headerLength = 16;
	heartbeat.header.operation = 2;
	heartbeat.header.version = 1;
	heartbeat.header.sequenceID = 1;
	auto bin = m_msger.BuildBinaryMessage(heartbeat);

	m_webSocket->Write(bin, [&](boost::beast::error_code err, size_t size) {});

}

DanmakuClient::DanmakuClient(uint64_t roomID, const Url& url, const std::string& token, ImGuiDanmaku& d, struct RoomState& s)
	:m_url(url)
	,m_webSocket(std::make_unique<WebSocket>())
	,m_danmaku(d)
	,m_running(true)
	,m_readFinish(false)
	,m_writeFinish(false)
	,m_roomState(s)
	,m_retryFlag(false)
	,m_roomID(roomID)
	,m_token(token)
{
}

DanmakuClient::~DanmakuClient()
{
	LOG_INFO("[RoomExit] DanmakuClient deconstruction");

}


void DanmakuClient::Run() {
	m_retryThread = std::jthread([self = shared_from_this()]() {
		while (self->m_running.load(std::memory_order_relaxed) == true) {
			while (self->m_retryFlag.load(std::memory_order_relaxed) == false) {
				self->m_retryFlag.wait(false, std::memory_order_relaxed);
			}
			self->Retry();
			self->m_retryFlag.store(false, std::memory_order_relaxed);
		}
	});
	m_retryThread.detach();

	if (!m_webSocket->WebSocketHandShake(m_url)) {
		LOG_ERROR("[DanmakuClient] Failed to connect danmaku servers,try next");
		if (m_retryFlag.load(std::memory_order_relaxed) == false) {
			m_retryFlag.store(true, std::memory_order_relaxed);
			m_retryFlag.notify_all();
			Retry();
			return;
		}
	}
	SendAuth(m_roomID, m_token);
	if (m_retryFlag.load(std::memory_order_relaxed) == true) {
		return;
	}

	m_readThread = std::jthread([self = shared_from_this()]() {
		auto uid = User::GetUid();
		while (self->m_running.load(std::memory_order_relaxed) == true) {
			try {
				self->m_webSocket->Read([](boost::beast::error_code err, size_t size){});
			}
			catch (const std::exception& e) {
				if (self->m_retryFlag.load(std::memory_order_relaxed) == false) {
					self->m_retryFlag.store(true, std::memory_order_relaxed);
					self->m_retryFlag.notify_all();
					self->Retry();
				}
				break;
			}
			auto& buffer = self->m_webSocket->GetBuffer();
			std::string str = beast::buffers_to_string(buffer.data());
			std::vector<uint8_t> data(static_cast<uint8_t*>(buffer.data().data()), static_cast<uint8_t*>(buffer.data().data()) + buffer.size());
			buffer.consume(buffer.size());
			auto msgs = self->m_msger.ParseBinaryMessage(data);
			auto danmu = msgs
				| std::views::filter([](const MessageRe& r) { 
					if (r.type == MessageType::AuthReply) { 
						LOG_INFO("[RoomMessager]: Auth reply: {}", r.body); 
					}
					return r.type == MessageType::Message; 
				})
				| std::views::filter([](const MessageRe& r) {
				try {
					auto json = nlohmann::json::parse(r.body);
					if (json["cmd"] == "DANMU_MSG") {
						return true;
					}
					else {
						//if (json["cmd"] == "ONLINE_RANK_COUNT") {
						//	LOG_INFO("[RoomMessager] Room online info :\n{}", json.dump(4));
						//}
						return false;
					}

				}
				catch (const std::exception& e) {
					LOG_WARN("[RoomMessager]: Parse json failed: {}", AcpToUtf8(e.what()));
					return false;
				}
					});

			std::vector<std::string> dans;
			for (const auto& d : danmu) {
				try {
					auto json = nlohmann::json::parse(d.body);
					auto dj = json["info"][0][15]["extra"].get<std::string>();
					auto data = nlohmann::json::parse(dj);
					auto colorHex = data["color"].get<uint64_t>();
					auto R = floor(colorHex / 65536);
					auto G = floor((colorHex % 65536) / 256);
					auto B = colorHex % 256;
					auto color = IM_COL32(R, G, B, 255);

					if (uid != 0) {
						auto user = json["info"][0][15]["user"]["uid"].get<uint64_t>();

						bool sbm = user == uid;
						auto ct = data["content"].get<std::string>();
						self->m_danmaku.Push(ct, sbm, color);
					}
					else {

						auto ct = data["content"].get<std::string>();
						self->m_danmaku.Push(ct, false, color);
					}

				}
				catch (const std::exception& e) {
					LOG_WARN("[RoomMessager]: Parse json failed: {}", AcpToUtf8(e.what()));
				}
			}
		}
		self->m_readFinish.store(true, std::memory_order_relaxed);
		self->m_readFinish.notify_all();
		LOG_INFO("[RoomExit] DanmakuClient read thread exit");

	});
	
	m_writeThread = std::jthread([self = shared_from_this()]() {
		while (self->m_running.load(std::memory_order_relaxed) == true) {
			try{
				self->SendHeartbeat();
			}
			catch (const std::exception& e) {
				if (self->m_retryFlag.load(std::memory_order_relaxed) == false) {
					self->m_retryFlag.store(true, std::memory_order_relaxed);
					self->m_retryFlag.notify_all();
					self->Retry();
				}
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(500000));
		}
		self->m_writeFinish.store(true, std::memory_order_relaxed);
		self->m_writeFinish.notify_all();

		LOG_INFO("[RoomExit] DanmakuClient write thread exit");
	});
	m_writeThread.detach();
	m_readThread.detach();
}

void DanmakuClient::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	while (m_writeFinish.load(std::memory_order_relaxed) == false) {
		m_writeFinish.wait(false, std::memory_order_relaxed);
	}
	while (m_readFinish.load(std::memory_order_relaxed) == false) {
		m_readFinish.wait(false, std::memory_order_relaxed);
	}
}
