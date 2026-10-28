#include "core/Util.h"

#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>

namespace sk {

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string ReadFileText(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s.erase(0, 3);
    return s;
}

std::vector<unsigned char> ReadFileBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), {}};
}

bool WriteFileBytes(const fs::path& p, const std::vector<unsigned char>& data) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool WriteFileText(const fs::path& p, const std::string& text) {
    return WriteFileBytes(p, std::vector<unsigned char>(text.begin(), text.end()));
}

fs::path KnownFolder(const GUID& id) {
    PWSTR raw = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

static fs::path Ensure(fs::path p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

#ifdef SK_DEV
constexpr const wchar_t* kDataName = L"SlayerKit-Dev";
#else
constexpr const wchar_t* kDataName = L"SlayerKit";
#endif

fs::path AppDataDir() { return Ensure(KnownFolder(FOLDERID_RoamingAppData) / kDataName); }
fs::path LocalDataDir() { return Ensure(KnownFolder(FOLDERID_LocalAppData) / kDataName); }
fs::path DocumentsDir() { return KnownFolder(FOLDERID_Documents); }
fs::path TempDir() { return Ensure(LocalDataDir() / L"temp"); }

fs::path ExeDir() {
    wchar_t buf[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
    return fs::path(buf).parent_path();
}

static std::string Stamp(const char* fmt) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[64];
    std::strftime(buf, sizeof buf, fmt, &tm);
    return buf;
}

std::string NowStamp() { return Stamp("%Y-%m-%d_%H-%M-%S"); }
std::string NowIso() { return Stamp("%Y-%m-%dT%H:%M:%S"); }

std::vector<std::wstring> RunningProcessNames() {
    std::vector<std::wstring> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{sizeof pe};
    if (Process32FirstW(snap, &pe)) {
        do out.emplace_back(pe.szExeFile);
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

bool IsProcessRunning(const std::wstring& exeName) {
    for (auto& n : RunningProcessNames())
        if (_wcsicmp(n.c_str(), exeName.c_str()) == 0) return true;
    return false;
}

RunResult RunCommand(const std::wstring& cmdLine, DWORD timeoutMs) {
    RunResult r;
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return r;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdLine;
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        return r;
    }
    CloseHandle(wr);

    auto start = GetTickCount64();
    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail) {
            DWORD got = 0;
            if (ReadFile(rd, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) && got)
                r.output.append(buf, got);
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) {
            DWORD got = 0;
            while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail &&
                   ReadFile(rd, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) && got)
                r.output.append(buf, got);
            break;
        }
        if (GetTickCount64() - start > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    GetExitCodeProcess(pi.hProcess, &r.exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    return r;
}

std::optional<std::wstring> RegReadString(HKEY root, const std::wstring& sub, const std::wstring& value) {
    DWORD size = 0;
    if (RegGetValueW(root, sub.c_str(), value.empty() ? nullptr : value.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                     nullptr, nullptr, &size) != ERROR_SUCCESS)
        return std::nullopt;
    std::wstring s(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, sub.c_str(), value.empty() ? nullptr : value.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                     nullptr, s.data(), &size) != ERROR_SUCCESS)
        return std::nullopt;
    s.resize(wcslen(s.c_str()));
    return s;
}

std::vector<std::wstring> RegSubKeys(HKEY root, const std::wstring& sub) {
    std::vector<std::wstring> out;
    HKEY k;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return out;
    wchar_t name[512];
    for (DWORD i = 0;; ++i) {
        DWORD len = (DWORD)std::size(name);
        if (RegEnumKeyExW(k, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        out.emplace_back(name, len);
    }
    RegCloseKey(k);
    return out;
}

std::string Base64Encode(const std::vector<unsigned char>& data) {
    if (data.empty()) return {};
    DWORD len = 0;
    CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
    std::string s(len, '\0');
    CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, s.data(), &len);
    s.resize(len);
    return s;
}

std::vector<unsigned char> Base64Decode(const std::string& text) {
    DWORD len = 0;
    if (!CryptStringToBinaryA(text.c_str(), (DWORD)text.size(), CRYPT_STRING_BASE64, nullptr, &len, nullptr, nullptr))
        return {};
    std::vector<unsigned char> out(len);
    CryptStringToBinaryA(text.c_str(), (DWORD)text.size(), CRYPT_STRING_BASE64, out.data(), &len, nullptr, nullptr);
    out.resize(len);
    return out;
}

fs::path LogFile() { return LocalDataDir() / L"slayerkit.log"; }

void Log(const std::string& line) {
    static std::mutex m;
    std::lock_guard lock(m);
    std::ofstream f(LogFile(), std::ios::app | std::ios::binary);
    f << NowIso() << "  " << line << "\n";
}

void OpenUrl(const std::wstring& url) { ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
void OpenFolder(const fs::path& p) { ShellExecuteW(nullptr, L"explore", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

}
