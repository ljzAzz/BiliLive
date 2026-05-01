#pragma once
#include <string_view>

constexpr const std::string_view UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

constexpr const std::string_view RoomInfoHost = "api.live.bilibili.com";
constexpr const std::string_view RoomInfoPath = "/room/v1/Room/get_info";

constexpr const std::string_view LiveHost = "api.live.bilibili.com";
constexpr const std::string_view LivePath = "/room/v1/Room/playUrl";

constexpr const std::string_view DanmuInfoHost = "api.live.bilibili.com";
constexpr const std::string_view DanmuInfoPath = "/xlive/web-room/v1/index/getDanmuInfo";

constexpr const std::string_view WbiHost = "api.bilibili.com";
constexpr const std::string_view WbiPath = "/x/web-interface/nav";

constexpr const std::string_view MainHost = "www.bilibili.com";
constexpr const std::string_view MainPath = "/";

constexpr const std::string_view SedDanmuHost = "api.live.bilibili.com";
constexpr const std::string_view SedDanmuPath = "/msg/send";
