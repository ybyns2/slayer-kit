#include "core/Live.h"

#include <tlhelp32.h>

#include <ctime>

#include "core/Config.h"
#include "core/Constants.h"
#include "core/Http.h"

namespace sk::live {

fs::path StateFile() { return AppDataDir() / L"live-state.json"; }

State Read() {
    State s;
    try {
        auto j = json::parse(ReadFileText(StateFile()));
        s.live = j.value("live", false);
        s.since = j.value("time", 0ll);
    } catch (...) {
    }
    return s;
}

static std::string Duration(long long sec) {
    char b[32];
    snprintf(b, sizeof b, "%lldh%02lld", sec / 3600, (sec % 3600) / 60);
    return b;
}

bool SendWebhook(const packs::Pack& pack, bool start, long long durationSec, std::string* error) {
    auto& c = Cfg();
    std::string url = Settings::Open(c.webhookSealed);
    if (url.rfind("https://discord.com/api/webhooks/", 0) != 0 && url.rfind("https://discordapp.com/api/webhooks/", 0) != 0) {
        if (error) *error = "Lien de webhook Discord invalide";
        return false;
    }
    std::string channel = c.twitchChannel.empty() ? c.kickChannel : c.twitchChannel;
    std::string link = !c.twitchChannel.empty() ? "https://twitch.tv/" + c.twitchChannel
                       : !c.kickChannel.empty() ? "https://kick.com/" + c.kickChannel
                                                : "";
    int color = ((int)(pack.accent.r * 255) << 16) | ((int)(pack.accent.g * 255) << 8) | (int)(pack.accent.b * 255);
    std::string logo = Narrow(k::kPacksRawBase) + "packs/" + pack.id + "/" + (pack.logo.empty() ? "logo.png" : pack.logo);
    json embed;
    if (start) {
        embed = {{"title", c.webhookTitle.empty() ? (channel.empty() ? "Je suis en live !" : channel + " est en live sur " + pack.name)
                                                  : c.webhookTitle},
                 {"description", c.webhookDescription.empty() ? "Viens voir le RP en direct sur **" + pack.name + "** !"
                                                              : c.webhookDescription},
                 {"color", color},
                 {"thumbnail", {{"url", logo}}},
                 {"footer", {{"text", "Slayer Kit by yns_bnl"}}}};
        if (!link.empty()) embed["url"] = link;
        if (!link.empty())
            embed["fields"] = json::array({{{"name", "Regarder"}, {"value", "[" + link.substr(8) + "](" + link + ")"}, {"inline", true}},
                                           {{"name", "Serveur"}, {"value", pack.name}, {"inline", true}}});
    } else {
        embed = {{"title", "Live terminé"},
                 {"description", "Merci à tous d'être passés ! Durée : **" + Duration(durationSec) + "**"},
                 {"color", color},
                 {"thumbnail", {{"url", logo}}},
                 {"footer", {{"text", "Slayer Kit by yns_bnl"}}}};
    }
    char ts[32];
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_s(&tm, &now);
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tm);
    embed["timestamp"] = ts;
    json body = {{"username", "Slayer Kit"}, {"embeds", json::array({embed})}, {"allowed_mentions", {{"parse", json::array({"everyone", "roles"})}}}};
    if (start && !c.webhookMessage.empty()) body["content"] = c.webhookMessage;
    auto r = http::Request(L"POST", Widen(url), {{L"Content-Type", L"application/json"}}, body.dump());
    if (!r.ok()) {
        if (error) *error = "Discord a refusé l'envoi (" + std::to_string(r.status) + ")";
        return false;
    }
    return true;
}

void PerformanceMode(bool on) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{sizeof pe};
    auto boost = [](const std::wstring& n) {
        return n.find(L"GTAProcess") != std::wstring::npos || _wcsicmp(n.c_str(), L"obs64.exe") == 0;
    };
    auto calm = [](const std::wstring& n) {
        for (auto x : {L"Medal.exe", L"MedalEncoder.exe", L"OneDrive.exe", L"SearchIndexer.exe", L"Steam.exe",
                       L"EpicGamesLauncher.exe", L"steamwebhelper.exe"})
            if (_wcsicmp(n.c_str(), x) == 0) return true;
        return false;
    };
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring n = pe.szExeFile;
            DWORD cls = 0;
            if (boost(n)) cls = on ? ABOVE_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
            else if (calm(n)) cls = on ? IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
            if (!cls) continue;
            if (HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pe.th32ProcessID)) {
                SetPriorityClass(h, cls);
                CloseHandle(h);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    Log(on ? "Mode performance active" : "Mode performance desactive");
}

void RecordStats(long long start, long long end, const std::string& pack) {
    auto& s = Cfg().liveStats;
    s.push_back({{"start", start}, {"end", end}, {"pack", pack}});
    while (s.size() > 100) s.erase(s.begin());
    SaveSettings();
}

}
