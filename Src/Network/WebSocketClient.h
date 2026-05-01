#pragma once
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/asio/io_context.hpp>
#include "User/UserData.h"
#include "BiliLiveApi.h"
#include "NetWorkTypes.h"
#include "ProtocolParser.h"

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;


class WebSocket {
private:
	net::io_context m_ioc;
	ssl::context m_ctx;
	beast::flat_buffer m_buffer;
	tcp::resolver m_resolver;
	std::optional<boost::beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_pWebSocketStream;
	std::mutex m_mutex;
private:
	bool SSLResolve(const Url& url);
public:
	WebSocket();
	virtual ~WebSocket();
	bool WebSocketHandShake(const Url& url);
	void Write(const std::vector<uint8_t>& msg, std::function<void(boost::beast::error_code err, size_t size)> wcb);
	void Read(std::function<void(boost::beast::error_code err, size_t size)> rcb);
	auto& GetBuffer() { return m_buffer; }
	void Run() { net::executor_work_guard<net::any_io_executor> guard(m_ioc.get_executor()); m_ioc.run(); }
};

class DanmakuClient :public std::enable_shared_from_this<DanmakuClient>{
private:
	std::unique_ptr<WebSocket> m_webSocket;
	Url m_url;
	RoomMessager m_msger;
	class ImGuiDanmaku& m_danmaku;
	struct RoomState& m_roomState;
	std::atomic_bool m_running;
	std::atomic_bool m_retryFlag;
	uint64_t m_roomID;
	std::string m_token;

	std::jthread m_readThread;
	std::jthread m_writeThread;
	std::jthread m_retryThread;
	std::atomic_bool m_readFinish;
	std::atomic_bool m_writeFinish;
	std::function<void()> m_retry;
private:
	void SendHeartbeat();
	void SendAuth(uint64_t roomID, const std::string& token);
	void Retry();
public:
	DanmakuClient(uint64_t roomID, const Url& url, const std::string& token, class ImGuiDanmaku& d, struct RoomState& s);
	~DanmakuClient();
	void SetRetryCallBack(std::function<void()> cb) { m_retry = cb; }
	void Run();
	void Stop();
	void SetUrls(const Url& url) { m_url = url; }
	void SetToken(const std::string& token) { m_token = token; }
};