#include "core/Detect.h"

#include <shlobj.h>

#include <algorithm>
#include <cwctype>

namespace sk::detect {

namespace {

struct Installed {
    std::wstring name, location, icon;
};

std::vector<Installed> UninstallEntries() {
    std::vector<Installed> out;
    const std::wstring sub = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    const std::wstring sub32 = L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for (auto [root, path] : {std::pair{HKEY_LOCAL_MACHINE, sub}, std::pair{HKEY_LOCAL_MACHINE, sub32},
                              std::pair{HKEY_CURRENT_USER, sub}}) {
        for (auto& k : RegSubKeys(root, path)) {
            std::wstring full = path + L"\\" + k;
            auto name = RegReadString(root, full, L"DisplayName");
            if (!name) continue;
            out.push_back({*name, RegReadString(root, full, L"InstallLocation").value_or(L""),
                           RegReadString(root, full, L"DisplayIcon").value_or(L"")});
        }
    }
    return out;
}

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    return s;
}

bool Contains(const std::wstring& hay, const std::wstring& needle) {
    return Lower(hay).find(Lower(needle)) != std::wstring::npos;
}

const Installed* FindEntry(const std::vector<Installed>& list, std::initializer_list<const wchar_t*> needles) {
    for (auto& e : list)
        for (auto n : needles)
            if (Contains(e.name, n)) return &e;
    return nullptr;
}

fs::path IconExe(const Installed* e) {
    if (!e || e->icon.empty()) return {};
    std::wstring s = e->icon;
    if (auto c = s.find(L','); c != std::wstring::npos) s.resize(c);
    s.erase(std::remove(s.begin(), s.end(), L'"'), s.end());
    return s;
}

fs::path FirstExisting(std::initializer_list<fs::path> paths) {
    std::error_code ec;
    for (auto& p : paths)
        if (!p.empty() && fs::exists(p, ec)) return p;
    return {};
}

bool AnyRunning(const std::vector<std::wstring>& procs, std::initializer_list<const wchar_t*> names) {
    for (auto& p : procs)
        for (auto n : names)
            if (_wcsicmp(p.c_str(), n) == 0) return true;
    return false;
}

bool StoreAppInstalled(const wchar_t* familyPrefix) {
    std::error_code ec;
    fs::path pkgs = KnownFolder(FOLDERID_LocalAppData) / L"Packages";
    for (auto& d : fs::directory_iterator(pkgs, ec))
        if (Contains(d.path().filename().wstring(), familyPrefix)) return true;
    return false;
}

struct WindowTitles {
    std::vector<std::pair<std::wstring, std::wstring>> list;
};

BOOL CALLBACK EnumTitles(HWND h, LPARAM lp) {
    if (!IsWindowVisible(h)) return TRUE;
    wchar_t title[512];
    if (!GetWindowTextW(h, title, (int)std::size(title))) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    std::wstring exe;
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t buf[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, buf, &n)) exe = fs::path(buf).filename().wstring();
        CloseHandle(p);
    }
    reinterpret_cast<WindowTitles*>(lp)->list.emplace_back(title, exe);
    return TRUE;
}

}

const App* Find(const std::vector<App>& list, const std::string& id) {
    for (auto& a : list)
        if (a.id == id) return &a;
    return nullptr;
}

Snapshot Scan() {
    Snapshot s;
    auto entries = UninstallEntries();
    auto procs = RunningProcessNames();
    fs::path pf = KnownFolder(FOLDERID_ProgramFiles);
    fs::path local = KnownFolder(FOLDERID_LocalAppData);
    fs::path roaming = KnownFolder(FOLDERID_RoamingAppData);

    
    {
        App a{"obs", L"OBS Studio"};
        auto e = FindEntry(entries, {L"OBS Studio"});
        auto reg = RegReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio", L"");
        a.exe = FirstExisting({reg ? fs::path(*reg) / L"bin\\64bit\\obs64.exe" : fs::path{},
                               pf / L"obs-studio\\bin\\64bit\\obs64.exe"});
        a.installed = e || !a.exe.empty();
        a.running = AnyRunning(procs, {L"obs64.exe"});
        a.wingetId = "OBSProject.OBSStudio";
        s.streaming.push_back(a);
    }
    {
        App a{"tiktok-studio", L"TikTok LIVE Studio"};
        auto e = FindEntry(entries, {L"TikTok LIVE Studio", L"TikTok Live Studio"});
        a.exe = IconExe(e);
        a.installed = e != nullptr;
        a.running = AnyRunning(procs, {L"TikTok LIVE Studio.exe", L"TikTokLIVEStudio.exe"});
        s.streaming.push_back(a);
    }
    {
        App a{"streamlabs", L"Streamlabs Desktop"};
        auto e = FindEntry(entries, {L"Streamlabs Desktop", L"Streamlabs OBS"});
        a.exe = IconExe(e);
        a.installed = e != nullptr;
        a.running = AnyRunning(procs, {L"Streamlabs Desktop.exe", L"Streamlabs OBS.exe"});
        a.wingetId = "Streamlabs.Streamlabs";
        s.streaming.push_back(a);
    }

    {
        App a{"spotify", L"Spotify"};
        a.exe = FirstExisting({roaming / L"Spotify\\Spotify.exe"});
        a.installed = !a.exe.empty() || FindEntry(entries, {L"Spotify"}) || StoreAppInstalled(L"SpotifyAB.SpotifyMusic");
        a.running = AnyRunning(procs, {L"Spotify.exe"});
        a.wingetId = "Spotify.Spotify";
        s.music.push_back(a);
    }
    {
        App a{"deezer", L"Deezer"};
        auto e = FindEntry(entries, {L"Deezer"});
        a.exe = FirstExisting({local / L"Programs\\deezer-desktop\\Deezer.exe", IconExe(e)});
        a.installed = e || !a.exe.empty() || StoreAppInstalled(L"Deezer");
        a.running = AnyRunning(procs, {L"Deezer.exe"});
        a.wingetId = "Deezer.Deezer";
        s.music.push_back(a);
    }
    {
        App a{"apple-music", L"Apple Music"};
        a.installed = StoreAppInstalled(L"AppleInc.AppleMusicWin");
        a.running = AnyRunning(procs, {L"AppleMusic.exe"});
        s.music.push_back(a);
    }
    {
        App a{"itunes", L"iTunes"};
        auto e = FindEntry(entries, {L"iTunes"});
        a.installed = e || StoreAppInstalled(L"AppleInc.iTunes");
        a.running = AnyRunning(procs, {L"iTunes.exe"});
        s.music.push_back(a);
    }

    struct B { const char* id; const wchar_t* name; const wchar_t* exe; };
    for (auto b : {B{"chrome", L"Google Chrome", L"chrome.exe"}, B{"edge", L"Microsoft Edge", L"msedge.exe"},
                   B{"brave", L"Brave", L"brave.exe"}, B{"opera", L"Opera / Opera GX", L"opera.exe"},
                   B{"firefox", L"Firefox", L"firefox.exe"}}) {
        App a{b.id, b.name};
        a.running = AnyRunning(procs, {b.exe});
        a.installed = a.running || FindEntry(entries, {b.name});
        a.exe = b.exe;
        if (a.installed) s.browsers.push_back(a);
    }

    WindowTitles wt;
    EnumWindows(EnumTitles, (LPARAM)&wt);
    const std::pair<const wchar_t*, const wchar_t*> sites[] = {
        {L"YouTube Music", L"YouTube Music"}, {L"SoundCloud", L"SoundCloud"}, {L"Deezer", L"Deezer (web)"},
        {L"Spotify", L"Spotify (web)"},       {L"Apple Music", L"Apple Music (web)"},
        {L"- YouTube", L"YouTube"},           {L"Twitch", L"Twitch"},
    };
    for (auto& [title, exe] : wt.list) {
        bool isBrowser = false;
        for (auto& b : s.browsers)
            if (_wcsicmp(exe.c_str(), b.exe.c_str()) == 0) isBrowser = true;
        if (isBrowser) {
            for (auto [needle, label] : sites)
                if (Contains(title, needle)) {
                    s.musicSites.push_back({label, exe});
                    break;
                }
        }
        if (Contains(exe, L"GTAProcess") && Contains(title, L"FiveM")) s.fivemServerTitle = title;
    }

    s.fivem = App{"fivem", L"FiveM"};
    s.fivem.exe = FirstExisting({local / L"FiveM\\FiveM.exe"});
    s.fivem.installed = !s.fivem.exe.empty();
    s.fivem.running = AnyRunning(procs, {L"FiveM.exe"});

    s.vbCable = FindEntry(entries, {L"VB:Virtual Cable", L"VB-Audio Virtual Cable"}) != nullptr;
    s.voicemeeter = FindEntry(entries, {L"Voicemeeter"}) != nullptr;
    return s;
}

bool WingetAvailable() {
    return RunCommand(L"winget --version", 15000).exitCode == 0;
}

bool WingetInstall(const std::string& wingetId, std::string* log) {
    std::wstring cmd = L"winget install --id " + Widen(wingetId) +
                       L" --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity";
    auto r = RunCommand(cmd, 30 * 60 * 1000);
    if (log) *log = r.output;
    Log("winget " + wingetId + " -> " + std::to_string(r.exitCode));
    return r.exitCode == 0;
}

}
