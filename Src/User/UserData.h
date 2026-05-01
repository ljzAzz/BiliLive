#pragma once


class User {
	friend struct AppSetting;
private:
	struct UserSetting {
		bool save = true;
	};

	static inline UserSetting s_setting;
private:
	struct UserData {
		std::atomic_uint64_t uid = 0;
		std::atomic_bool isLogin = false;
		std::string userName;
		std::string sessData;
		std::string buvid3;
		std::string wbiKey;
		std::string bilijct;
		std::string refreshToken;
	};
	static inline UserData s_userData;
	static inline nlohmann::json s_userjson;
public:
	static bool IsLogin() { return s_userData.isLogin.load(std::memory_order_relaxed); }
	static bool NeedToGetBuvid3() {  return s_userData.buvid3.empty(); }
	static bool NeedToGetSessData() { return s_userData.sessData.empty(); }
	static bool NeedToGetWbiKey() { return s_userData.wbiKey.empty(); }
	static bool NeedToGetUid() {  return s_userData.uid.load(std::memory_order_acquire) == 0; }
	static bool NeedToGetUserName() {  return s_userData.userName.empty(); }
	static bool NeedToRefresh();
	static void SetLogin() { 
		s_userData.isLogin.store(true, std::memory_order_relaxed);
		s_userjson["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	static bool BilijctEmpty() { return s_userData.bilijct.empty(); }
	static void UnSetLogin() { s_userData.isLogin.store(false, std::memory_order_relaxed); }
	static void SetUid(uint64_t uid) { s_userData.uid.store(uid,std::memory_order_relaxed); }
	static void SetUserName(const std::string& userName) { s_userData.userName = userName; }
	static void SetBuvid3(const std::string& buvid3) { s_userData.buvid3 = buvid3; }
	static void SetWbiKey(const std::string& wbiKey) { s_userData.wbiKey = wbiKey; }
	static void SetSessData(const std::string& sessData) { s_userData.sessData = sessData; }
	static void SetBilijct(const std::string& bilijct) { s_userData.bilijct = bilijct; }
	static void SetRefreshToken(const std::string& refreshToken) { s_userData.refreshToken = refreshToken; }
	static uint64_t GetUid() {  return s_userData.uid.load(std::memory_order_relaxed); }
	static const std::string& GetUserName() {  return s_userData.userName; }
	static const std::string& GetBuvid3() {  return s_userData.buvid3; }
	static const std::string& GetWbiKey() {  return s_userData.wbiKey; }
	static const std::string& GetSessData() {  return s_userData.sessData; }
	static const std::string& GetBilijct() { return s_userData.bilijct; }
	static const std::string& GetRefreshToken() {  return s_userData.refreshToken; }
	static void clear() {
		s_userData.uid.store(0, std::memory_order_relaxed);
		s_userData.isLogin.store(false, std::memory_order_relaxed);
		s_userData.userName.clear();
		s_userData.sessData.clear();
		s_userData.buvid3.clear();
		s_userData.wbiKey.clear();
		s_userData.refreshToken.clear();
	}
	static const auto& GetSetting() { return s_setting; }
	static void Save();
	static void Read();
};

class WBIKEY {
private:
	static constexpr const auto WBI_KEY_INDEX_TABLE = {
		46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
		27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13
	};

private:
	static std::string GetMd5Hex(const std::string& Input_str);
	static std::string JsonToUrlEncodeStr(const nlohmann::json& Json);
public:
	static nlohmann::json CalcSign(nlohmann::json& Params, const std::string& wbi);
	static std::string CalcWbiKey(const std::string& imgKey, const std::string& subKey);
};
