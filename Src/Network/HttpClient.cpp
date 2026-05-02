#include "stdafx.h"
#include "BiliLiveApi.h"
#include "HttpClient.h"

bool HttpClient::SSLResolve(const Url& url)
{
	if (!m_stream) {
		m_stream.emplace(m_ioc, m_ctx);
	}
	else {
		m_stream.reset();
		m_stream.emplace(m_ioc, m_ctx);
	}
	if (m_buffer.size() > 0) {
		m_buffer.consume(m_buffer.size());
	}

	auto const results = m_resolver.resolve(url.host, url.port);
	if (results.empty()) {
		LOG_ERROR("[HttpClient]: Http Resolve error");
		return false;
	}
	auto& stream = m_stream.value();
	beast::get_lowest_layer(stream).connect(results);

	SSL_set_tlsext_host_name(stream.native_handle(), url.host.data());

	stream.set_verify_mode(boost::asio::ssl::verify_none);

	beast::error_code ec;
	stream.handshake(ssl::stream_base::client, ec);
	if (ec)
	{
		LOG_ERROR("[NetWork]: Handshake failed: {}", ec.message());
		return false;
	}

	return true;
}

HttpClient::HttpClient()
	: m_ctx(ssl::context::tlsv12_client)
	, m_resolver(net::make_strand(m_ioc))
{
}

http::response<http::string_body> HttpClient::Get(const Url& url)
{
	std::unique_lock lock(m_mutex);
	if (!SSLResolve(url)) {
		return {};
	}
	std::string fullPath = std::string(url.path) + "?";
	std::for_each(url.parameters.cbegin(), url.parameters.cend(), [&fullPath](const std::pair<std::string, std::string>& p) {
		if (p.second.empty()) {
			fullPath += p.first;
			fullPath += "&";
			return;
		}
		fullPath += p.first;
		fullPath += "=";
		fullPath += p.second;
		fullPath += "&";
		});
	if (fullPath.back() == '&') {
		fullPath.pop_back();
	}
	LOG_DEBUG("[HttpClient]: Request URL: {}{}", url.host, fullPath);
	// Build the HTTP request message
	http::request<http::string_body> req{ http::verb::get, fullPath, 11 };     // HTTP/1.1
	req.set(http::field::host, url.host);                               // Required: Host header
	req.set(http::field::user_agent, UserAgent);   // Optional: Identifies the client
	std::string cookie_header;

	for (const auto& [k, v] : url.cookies) {
		if (!cookie_header.empty()) cookie_header += "; ";
		cookie_header += k;
		cookie_header += "=";
		cookie_header += v;
	}

	if (!cookie_header.empty()) {
		req.set(http::field::cookie, cookie_header);
	}
	boost::beast::error_code ec;
	LOG_DEBUG("[HttpClient] Write start.");
	http::write(m_stream.value(), req, ec);
	if (ec) {
		LOG_ERROR("[HttpClient] Write err : {}", ec.message());
		
	}
	ec.clear();
	http::response<http::string_body> res;
	http::read(m_stream.value(), m_buffer, res, ec);
	if (ec) {
		LOG_ERROR("[HttpClient] Read err : {}", ec.message());
	}

	m_stream.value().shutdown(ec);
	if (ec == boost::asio::error::eof)
		ec.clear();
	beast::get_lowest_layer(m_stream.value()).close();

	ec.clear();
	if (m_buffer.size() > 0) {
		m_buffer.consume(m_buffer.size());
	}
	return res;

}

void HttpClient::Run()
{
	auto guard = net::make_work_guard(m_ioc);
	m_ioc.run();
}

http::response<http::string_body> HttpClient::Post(const Url& url)
{
	std::unique_lock lock(m_mutex);
	if (!SSLResolve(url)) {
		return{};
	}
	std::string fullPath = std::string(url.path);
	std::string params;
	std::for_each(url.parameters.cbegin(), url.parameters.cend(), [&params](const std::pair<std::string, std::string>& p) {
		if (p.second.empty()) {
			params += p.first;
			params += "&";
			return;
		}
		params += p.first;
		params += "=";
		params += p.second;
		params += "&";
		});
	if (!params.empty()&&params.back() == '&') {
		params.pop_back();
	}

	LOG_DEBUG("[HttpClient]: Request URL: {}{}", url.host, fullPath);
	// Build the HTTP request message
	http::request<http::string_body> req{ http::verb::post, fullPath, 11 };     // HTTP/1.1
	req.set(http::field::host, url.host);                               // Required: Host header
	req.set(http::field::user_agent, UserAgent);   // Optional: Identifies the client
	req.set(http::field::content_type, "application/x-www-form-urlencoded");
	req.body() = params;
	req.prepare_payload();

	std::string cookie_header;

	for (const auto& [k, v] : url.cookies) {
		if (!cookie_header.empty()) cookie_header += ";";
		cookie_header += k;
		cookie_header += "=";
		cookie_header += v;
	}

	if (!cookie_header.empty()) {
		req.set(http::field::cookie, cookie_header);
	}


	beast::error_code ec;
	http::write(m_stream.value(), req, ec);
	http::response<http::string_body> res;

	http::read(m_stream.value(), m_buffer, res, ec);
	if (ec) {
		LOG_ERROR("[HttpClient] Read err : {}", ec.message());
		return {};
	}

	return res;
}



BliveClient::BliveClient(uint64_t roomID)
	:m_roomInfoUrl{ .host = std::string(RoomInfoHost),.path = std::string(RoomInfoPath),.port = "443",.parameters = {{"room_id", std::to_string(roomID)}}}
	,m_wbiKeyUrl{ .host = std::string(WbiHost),.path = std::string(WbiPath),.port = "443",.cookies = {{"SESSDATA",User::GetSessData()}}}
	,m_danmuInfoUrl{ .host = std::string(DanmuInfoHost),.path = std::string(DanmuInfoPath),.port = "443" }
	,m_roomLiveUrl{ .host = std::string(LiveHost), .path = std::string(LivePath), .port = "443"}
	,m_running(false)
	,m_exit(false)
	,m_danmus(10)
	,m_roomID(0)
{

}

BliveClient::~BliveClient()
{
}

void BliveClient::PreDanmuThread()
{
	m_running.store(true, std::memory_order_relaxed);
	m_danmaThread = std::thread([this]() {
		while (m_running.load(std::memory_order_relaxed)==true) {
			if (m_danmus.Empty()) {
				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
			std::string str;
			if (!m_danmus.Pop(str)) {
				std::this_thread::sleep_for(std::chrono::seconds(2));
				continue;
			}
			auto csrf = User::GetBilijct();
			Url url{ .host = std::string(SedDanmuHost),.path = std::string(SedDanmuPath),.port = "443",
				.cookies = {{"SESSDATA",User::GetSessData()},{"bili_jct",csrf}} };
			std::unordered_map<std::string, std::string> params;
			params.insert({ "roomid",std::to_string(m_roomID)});
			params.insert({ "msg",str });
			params.insert({ "rnd",std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) });
			params.insert({ "fontsize","25" });
			params.insert({ "color","16777215" });
			params.insert({ "csrf",csrf });
			url.parameters = params;
			for (const auto& [k, v] : params) {
				LOG_DEBUG("[SendDanmu] key={},value={}", k, v);
			}
			auto res = m_networkClient.Post(url);
			nlohmann::json json;
			try {
				json = nlohmann::json::parse(res.body());
			}
			catch (const std::exception& e) {
				LOG_ERROR("[HttpClient] Failed to parse json {}", e.what());
				LOG_ERROR("[HttpClient] Res is :\n{}", res.body());
				return;
			}
			LOG_DEBUG("[HttpClient] json:\n", json.dump(4));
		}
		m_exit.store(true, std::memory_order_relaxed);
		m_exit.notify_all();
	});
	m_danmaThread.detach();
}

void BliveClient::SendDanmu(std::string danmu)
{
	m_danmus.Push(danmu);
}

void BliveClient::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	while (m_exit.load(std::memory_order_relaxed)==false) {
		m_exit.wait(false, std::memory_order_relaxed);
	}
}

void BliveClient::SetRoomInfo(RoomInfo& roomInfo)
{
	auto res = m_networkClient.Get(m_roomInfoUrl);
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(res.body());
	}
	catch (const std::exception& e) {
		LOG_ERROR("[BliveClient] Failed to parse json when set room info: {}", e.what());
		return;
	}
	auto title = json["data"]["title"].get<std::string>();
	auto roomid = json["data"]["room_id"].get<uint64_t>();
	auto uid = json["data"]["uid"].get<uint64_t>();
	auto live_status = json["data"]["live_status"].get<int>()!=0;
	LOG_DEBUG("[BliveClient] Room live status : {}", json["data"]["live_status"].get<int>());
	roomInfo.ownerUid = uid;
	roomInfo.realRoomID = roomid;
	roomInfo.roomTitle = title;
	roomInfo.liveStatus = live_status;
	m_roomID = roomid;
	SetLiveUrls(roomid);
	if (User::NeedToGetWbiKey()) {
		SetWbiKey();
	}
	SetDanmakuUrls();
}

void BliveClient::SetWbiKey()
{
	UserInfo::UpdateAll();
}

void BliveClient::SetDanmakuUrls()
{
	UserInfo::UpdateAll();
	auto wbi = User::GetWbiKey();
	auto rid = m_roomInfoUrl.parameters["room_id"];

	nlohmann::json params;
	params["id"] = m_roomID;
	params["type"] = 0;
	params["web_location"] = "444.8";
	auto wbiSign = WBIKEY::CalcSign(params, wbi);
	std::unordered_map<std::string, std::string> wbiSignMap;
	wbiSignMap.insert({ "id" ,std::to_string(m_roomID)});
	wbiSignMap.insert({ "type" ,"0"});
	wbiSignMap.insert({ "web_location" ,"444.8"});
	wbiSignMap.insert({ "w_rid" ,wbiSign });
	auto wts = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	wbiSignMap.insert({ "wts" ,std::to_string(wts) });
	m_danmuInfoUrl.cookies = { {"SESSDATA",User::GetSessData()},{"buvid3",User::GetBuvid3()} };
	m_danmuInfoUrl.parameters = wbiSignMap;


	auto res = m_networkClient.Get(m_danmuInfoUrl);

	nlohmann::json data;
	try {
		data = nlohmann::json::parse(res.body());
	}
	catch (const std::exception& e) {
		LOG_ERROR("[BliveClient] Failed to parse json when get danmaku info: {}", e.what());
		return;
	}
	LOG_DEBUG("[BliveClient] Danmu Info: \n{}", data.dump(4));

	int retryTime = 0;
	if (data["code"] != 0) {
		while (data["code"] != 0 && retryTime < 3) {
			data.clear();
			wbiSign.clear();
			wbiSignMap.clear();
			UserInfo::UpdateAll();
			auto wbi = User::GetWbiKey();
			LOG_ERROR("[BliveClient] Failed to get danmuInfo，retry :{}", retryTime++);
			wbiSign = WBIKEY::CalcSign(params, wbi);
			wbiSignMap.insert({ "id" ,std::to_string(m_roomID) });
			wbiSignMap.insert({ "type" ,"0" });
			wbiSignMap.insert({ "web_location" ,"444.8" });
			wbiSignMap.insert({ "w_rid" ,wbiSign });
			auto wts = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			wbiSignMap.insert({ "wts" ,std::to_string(wts)});

			m_danmuInfoUrl.cookies = { {"SESSDATA",User::GetSessData()},{"buvid3",User::GetBuvid3()} };
			m_danmuInfoUrl.parameters = wbiSignMap;

			auto res = m_networkClient.Get(m_danmuInfoUrl);
			try {
				data = nlohmann::json::parse(res.body());
			}
			catch (const std::exception& e) {
				LOG_ERROR("[BliveClient] Failed to parse json when get danmaku info: {}", e.what());
				return;
			}
		}
		if (data["code"] != -352) {
			LOG_ERROR("[BliveClient] Get DanmuInfo error");
		}
	}
	LOG_DEBUG("[BliveClient] Danmu Info: \n{}", data.dump(4));

	for (const auto& h : data["data"]["host_list"]) {
		Url url;
		url.host = h["host"].get<std::string>();
		url.port = to_string(h["wss_port"]);
		url.path = "/sub";
		m_danmakuUrls.push_back(url);
	}
	if (data["data"].find("token") != data["data"].end()) {
		m_token = data["data"]["token"].get<std::string>();
	}
	else {
		LOG_ERROR("[BliveClient] Failed to get danmaku token");
	}

}

void BliveClient::SetLiveUrls(uint64_t realID)
{
	m_roomLiveUrl.parameters = { { "cid", std::to_string(realID)}, {"quality","4"}, {"platform","web"} };
	auto res = m_networkClient.Get(m_roomLiveUrl);

	nlohmann::json json;
	try {
		json = nlohmann::json::parse(res.body());
		LOG_DEBUG("[BliveClient] Live urls:\n{}", json.dump(4));
	}
	catch (const std::exception& e) {
		LOG_ERROR("[BliveClient] Failed to parse json when handle live urls: {}", e.what());
		return;
	}
	LOG_DEBUG("[BliveClient] Player URL Info: \n{}", json.dump(4));
	if (json["code"] != 0) {
		if (json["code"] == -400) {
			LOG_WARN("[BliveClient] Live params error");
		}
		else if (json["code"] == 19002003) {
			LOG_WARN("[BliveClient] RoomInfo does not exist");
		}
		else {
			LOG_WARN("[BliveClient] Failed to init get live urls.");
		}
		return;
	}
	for (auto& u : json["data"]["durl"]) {
		BliveUrl url;
		url.url = u["url"].get<std::string>();
		m_liveUrls.push_back(url);
	}
}

void UserInfo::UpdateAll()
{
	UpdateBuvid();
	Url wbiKeyUrl = Url{ .host = std::string(WbiHost),.path = std::string(WbiPath),.port = "443",.cookies = {{"SESSDATA",User::GetSessData()}}};
	auto res = s_networkClient.Get(wbiKeyUrl);
	nlohmann::json data;
	try {
		data = nlohmann::json::parse(res.body());
	}
	catch (const std::exception& e) {
		LOG_ERROR("[BliveClient] Failed to parse json when get wbi: {}", e.what());
	}
	LOG_DEBUG("[BliveClient] Wbi Key Info: \n{}", data.dump(4));
	if (data["code"] == 0) {
		if (!User::IsLogin()) {
			LOG_INFO("[BliveClient] User login.");
			auto uid = data["data"]["mid"].get<uint64_t>();
			auto uname = data["data"]["uname"].get<std::string>();
			User::SetUid(uid);
			User::SetUserName(uname);
			User::SetLogin();
		}
	}
	else if (data["code"] == -101) {
		User::clear();
		User::Save();
	}
	const std::string imgUrl = data["data"]["wbi_img"]["img_url"];
	const std::string subUrl = data["data"]["wbi_img"]["sub_url"];

	std::string imgKey = imgUrl.substr(imgUrl.find("wbi/") + 4, imgUrl.find(".png") - imgUrl.find("wbi/") - 4);
	std::string subKey = subUrl.substr(subUrl.find("wbi/") + 4, subUrl.find(".png") - subUrl.find("wbi/") - 4);

	auto wbi = WBIKEY::CalcWbiKey(imgKey, subKey);
	User::SetWbiKey(wbi);
	LOG_DEBUG("[User] Set WbiKey: {}", wbi);
}

void UserInfo::UpdateBuvid()
{
	Url mainUrl = Url{ .host = std::string(MainHost), .path = std::string(MainPath),.port = "443",.cookies = {{"SESSDATA",User::GetSessData()}} };
	auto res = s_networkClient.Get(mainUrl);
	auto range = res.equal_range(http::field::set_cookie);
	for (auto it = range.first; it != range.second; ++it) {
		auto line = it->value();
		auto pos = line.find(';');
		std::string kv = (pos == std::string::npos) ? line : line.substr(0, pos);

		auto eq = kv.find('=');
		if (eq == std::string::npos) return;

		std::string name = kv.substr(0, eq);
		std::string value = kv.substr(eq + 1);
		LOG_DEBUG("[User] cookie :{}", line);
		if (name == "buvid3") {
			User::SetBuvid3(value);
			LOG_DEBUG("[User] Set buvid3 :{}", value);
			break;
		}
	}
}
