#include "stdafx.h"
#include "UserData.h"
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cpr/cpr.h>
#include <cryptopp/md5.h>
#include <cryptopp/hex.h>
#include <Common/Common.h>

bool User::NeedToRefresh()
{
	//todo
	return false;
}

void User::Save()
{
    auto curPath = GetExePath();
    auto dir = curPath.concat("/UserData");
    if (!std::filesystem::exists(dir)) {
        LOG_INFO("[User] File path not exists. now create......");
        if (std::filesystem::create_directory(dir)) {
            LOG_INFO("[User] Success to create directory");
        }
        else {
            LOG_ERROR("[User] Failed to create directory, user data will be not saved");
            return;
        }
    }
    std::ofstream file;
    file.open(dir.concat("/User.json"));
    if (!file.is_open()) {
        LOG_ERROR("[User] Failed to create file, user data will be not saved");
        return;
    }
    else {
        nlohmann::json json;
        s_userjson["uid"] = s_userData.uid.load(std::memory_order_relaxed);
        s_userjson["isLogin"] = s_userData.isLogin.load(std::memory_order_relaxed);
        s_userjson["userName"] = s_userData.userName;
        s_userjson["sessData"] = s_userData.sessData;
        s_userjson["buvid3"] = s_userData.buvid3;
        s_userjson["wbiKey"] = s_userData.wbiKey;
        s_userjson["bilijct"] = s_userData.bilijct;
        s_userjson["refreshToken"] = s_userData.refreshToken;
        file << s_userjson;
        file.close();
        LOG_INFO("[User] Success save user data");
    }
}

void User::Read()
{
    auto curPath = GetExePath();
    auto dir = curPath.concat("/UserData");
    if (!std::filesystem::exists(dir)) {
        LOG_INFO("[User] Directory not exists. abandon to read user data from file");
        return;
    }
    std::ifstream file;
    nlohmann::json json;
    file.open(dir.concat("/User.json"),std::ios::_Nocreate);
    if (!file.is_open()) {
        LOG_INFO("[User] Json file not exists. abandon to read user data from file");
        return;
    }
    else {
        file >> json;
        if(json.find("uid")!=json.end())
            s_userData.uid.store(json["uid"].get<uint64_t>(),std::memory_order_relaxed);
        if (json.find("isLogin") != json.end())
            s_userData.isLogin.store(json["isLogin"].get<bool>(),std::memory_order_relaxed);
        if (json.find("userName") != json.end())
            s_userData.userName = json["userName"].get<std::string>();
        if (json.find("sessData") != json.end())
            s_userData.sessData = json["sessData"].get<std::string>();
        if (json.find("buvid3") != json.end())
            s_userData.buvid3 = json["buvid3"].get<std::string>();
        if (json.find("wbiKey") != json.end())
            s_userData.wbiKey = json["wbiKey"].get<std::string>();
        if (json.find("bilijct") != json.end())
            s_userData.bilijct = json["bilijct"].get<std::string>();
        if (json.find("refreshToken") != json.end())
            s_userData.refreshToken = json["refreshToken"].get<std::string>();
        LOG_INFO("[User] Success to read user data from json file");
    }
}

std::string WBIKEY::GetMd5Hex(const std::string& inputStr)
{
    CryptoPP::Weak1::MD5 hash;
    std::string md5Hex;

    CryptoPP::StringSource ss(inputStr, true,
        new CryptoPP::HashFilter(hash,
            new CryptoPP::HexEncoder(
                new CryptoPP::StringSink(md5Hex)
            )
        )
    );

    std::ranges::for_each(md5Hex, [](char& x) { x = std::tolower(x); });
    return md5Hex;
}

std::string WBIKEY::JsonToUrlEncodeStr(const nlohmann::json& Json)
{
    std::string encodeStr;
    for (const auto& [key, value] : Json.items()) {
        encodeStr.append(key).append("=").append(cpr::util::urlEncode(value.is_string() ? value.get<std::string>() : to_string(value))).append("&");
    }

    encodeStr.resize(encodeStr.size() - 1, '\0');
    return encodeStr;
}

std::string WBIKEY::CalcSign(nlohmann::json Params, const std::string& wbi)
{
    Params["wts"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    const std::string encodeStr = JsonToUrlEncodeStr(Params).append(wbi);
    return GetMd5Hex(encodeStr);
}

std::string WBIKEY::CalcWbiKey(const std::string& imgKey, const std::string& subKey)
{
    std::string raw_wbi_key_str = imgKey + subKey;
    std::string result;

    std::ranges::for_each(MIXIN_KEY_ENC_TAB_, [&result, &raw_wbi_key_str](const uint8_t x) {
        result.push_back(raw_wbi_key_str.at(x));
        });

    return result.substr(0, 32);
}
