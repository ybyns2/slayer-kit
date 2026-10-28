#include "core/Overlays.h"

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>

#include "core/Config.h"
#include "core/Http.h"

namespace sk::overlays {

namespace {

struct Titles {
    std::vector<std::pair<std::wstring, std::wstring>> list;
};

BOOL CALLBACK Enum(HWND h, LPARAM lp) {
    wchar_t title[512];
    if (!GetWindowTextW(h, title, 512) || !title[0]) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    std::wstring exe;
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t buf[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, buf, &n)) exe = fs::path(buf).filename().wstring();
        CloseHandle(p);
    }
    reinterpret_cast<Titles*>(lp)->list.emplace_back(exe, title);
    return TRUE;
}

}

fs::path Dir() {
    fs::path p = LocalDataDir() / L"overlays";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}
fs::path ChatFile() { return Dir() / L"chat.html"; }
fs::path ChatWindowFile() { return Dir() / L"chat-fenetre.html"; }
fs::path NowPlayingFile() { return Dir() / L"musique-en-cours.txt"; }

std::string KickChatroomId(const std::string& slug, std::string* error) {
    if (slug.empty()) return {};
    auto r = http::Get(L"https://kick.com/api/v2/channels/" + Widen(slug),
                       {{L"User-Agent", L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36"},
                        {L"Accept", L"application/json"}});
    if (!r.ok()) {
        if (error) *error = "Chaîne Kick introuvable (" + std::to_string(r.status) + ")";
        return {};
    }
    try {
        auto j = json::parse(r.body);
        return std::to_string(j["chatroom"].value("id", 0));
    } catch (...) {
        if (error) *error = "Réponse Kick illisible";
        return {};
    }
}

std::wstring NowPlaying() {
    Titles t;
    EnumWindows(Enum, (LPARAM)&t);
    for (auto& [exe, title] : t.list) {
        if (_wcsicmp(exe.c_str(), L"Spotify.exe") == 0) {
            if (title.find(L" - ") != std::wstring::npos && title.rfind(L"Spotify", 0) != 0) return title;
        } else if (_wcsicmp(exe.c_str(), L"Deezer.exe") == 0) {
            auto p = title.rfind(L" - Deezer");
            if (p != std::wstring::npos && p > 0) return title.substr(0, p);
        } else if (_wcsicmp(exe.c_str(), L"AppleMusic.exe") == 0 && title != L"Apple Music") {
            return title;
        }
    }
    for (auto& [exe, title] : t.list) {
        auto p = title.find(L" - YouTube Music");
        if (p != std::wstring::npos) return title.substr(0, p);
        p = title.find(L" | Écoute gratuite sur SoundCloud");
        if (p != std::wstring::npos) return title.substr(0, p);
    }
    return {};
}

void UpdateNowPlayingFile() {
    static std::wstring last = L"#";
    std::wstring now = Cfg().nowPlaying ? NowPlaying() : L"";
    if (now == last) return;
    last = now;
    WriteFileText(NowPlayingFile(), now.empty() ? std::string() : Narrow(L"♪  " + now));
}

std::wstring LocalUrl(const std::string& path, bool window) {
    auto& c = Cfg();
    std::wstring u = L"http://127.0.0.1:" + std::to_wstring(c.serverPort) + Widen(path) + L"?t=" + Widen(c.serverToken);
    if (!c.twitchChannel.empty()) u += L"&twitch=" + Widen(c.twitchChannel);
    if (window) u += L"&fenetre=1";
    return u;
}

bool OpenChatWindow(const std::wstring& url) {
    for (auto exe : {L"msedge.exe", L"chrome.exe"}) {
        HINSTANCE h = ShellExecuteW(nullptr, L"open", exe, (L"--app=\"" + url + L"\" --window-size=460,820").c_str(), nullptr,
                                    SW_SHOWNORMAL);
        if ((INT_PTR)h > 32) return true;
    }
    return (INT_PTR)ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

}
