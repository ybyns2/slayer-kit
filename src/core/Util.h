#pragma once
#include <windows.h>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#ifndef SK_VERSION
#define SK_VERSION "0.0.0"
#endif
#define SK_WIDEN2(x) L##x
#define SK_WIDEN(x) SK_WIDEN2(x)
#define SK_WIDEN_VERSION SK_WIDEN(SK_VERSION)

namespace sk {

std::wstring Widen(const std::string& s);
std::string Narrow(const std::wstring& s);

std::string ReadFileText(const fs::path& p);
std::vector<unsigned char> ReadFileBytes(const fs::path& p);
bool WriteFileText(const fs::path& p, const std::string& text);
bool WriteFileBytes(const fs::path& p, const std::vector<unsigned char>& data);

fs::path KnownFolder(const GUID& id);
fs::path AppDataDir();
fs::path LocalDataDir();
fs::path DocumentsDir();
fs::path ExeDir();
fs::path TempDir();

std::string NowStamp();
std::string NowIso();

bool IsProcessRunning(const std::wstring& exeName);
std::vector<std::wstring> RunningProcessNames();

struct RunResult { DWORD exitCode = 1; std::string output; };
RunResult RunCommand(const std::wstring& cmdLine, DWORD timeoutMs = 600000);

std::optional<std::wstring> RegReadString(HKEY root, const std::wstring& sub, const std::wstring& value);
std::vector<std::wstring> RegSubKeys(HKEY root, const std::wstring& sub);

std::string Base64Encode(const std::vector<unsigned char>& data);
std::vector<unsigned char> Base64Decode(const std::string& text);

void Log(const std::string& line);
fs::path LogFile();

void OpenUrl(const std::wstring& url);
void OpenFolder(const fs::path& p);

}
