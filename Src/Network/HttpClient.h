#pragma once
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/asio/io_context.hpp>
#include "User/UserData.h"
#include "NetWorkTypes.h"
#include "Room/RoomTypes.h"
#include "BiliLiveApi.h"
#include "Common/Common.h"

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;


class HttpClient {
private:
	net::io_context m_ioc;
	ssl::context m_ctx;
	tcp::resolver m_resolver;
	std::optional<beast::ssl_stream<beast::tcp_stream>> m_stream;
	beast::flat_buffer m_buffer;
	std::mutex m_mutex;
private:
	bool SSLResolve(const Url& url);
public:
	HttpClient();
	virtual ~HttpClient() = default;
	http::response<http::string_body> Get(const Url& url);
	void Run();
	void Stop() { m_ioc.stop(); }
	http::response<http::string_body> Post(const Url& url);
};

class UserInfo {
private:
	static inline HttpClient s_networkClient;
public:
	static void UpdateAll();
	static void UpdateBuvid();
};

class BliveClient:public std::enable_shared_from_this<BliveClient> {
private:
	HttpClient m_networkClient;
	Url m_roomInfoUrl;
	Url m_roomLiveUrl;
	Url m_wbiKeyUrl;
	Url m_danmuInfoUrl;
	Url m_userInfoUrl;
	uint64_t m_roomID;
	std::vector<BliveUrl> m_liveUrls;
	std::vector<Url> m_danmakuUrls;
	std::string m_token;
	std::thread m_danmaThread;
	std::atomic_bool m_running;
	std::atomic_bool m_exit;
	LockFreeQueue<std::string> m_danmus;
private:
	void SetWbiKey();
	void SetDanmakuUrls();
	void SetLiveUrls(uint64_t realID);
public:
	BliveClient(uint64_t roomID);
	void SetRoomInfo(RoomInfo& roomInfo);

	~BliveClient();
	std::vector<BliveUrl> GetLiveUrls() const { return m_liveUrls; }
	std::vector<Url> GetDanmakuUrls() const {  return m_danmakuUrls; }
	std::string GetToken() const { return m_token; }
	void PreDanmuThread();
	void SendDanmu(std::string danmu);
	void Stop();
};