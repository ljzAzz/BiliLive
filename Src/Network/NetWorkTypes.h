#pragma once
#include <string_view>
#include <unordered_map>
#include <string>

struct Url {
	std::string host;
	std::string path;
	std::string port;
	std::unordered_map<std::string, std::string> cookies;
	std::unordered_map<std::string, std::string> parameters;
};

struct BliveUrl {
	std::string url;
};
