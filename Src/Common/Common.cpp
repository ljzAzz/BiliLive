#include "stdafx.h"
#include "Common.h"

std::string AcpToUtf8(const std::string& s)
{
    if (s.empty()) return {};

    int wlen = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring ws(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), ws.data(), wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), out.data(), ulen, nullptr, nullptr);

    return out;
}

std::string Utf16ToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), out.data(), len, nullptr, nullptr);
    return out;
}

std::string winErrToUtf8(DWORD err)
{
    LPWSTR buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, (LPWSTR)&buf, 0, nullptr);
    std::wstring ws = buf ? buf : L"";
    if (buf) LocalFree(buf);
    return Utf16ToUtf8(ws);
}

std::filesystem::path GetExePath()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::canonical(buffer).parent_path();
}
