#include "core/Streams.h"

#include <set>
#include <thread>

#include "core/Http.h"
#include "core/Overlays.h"
#include "core/WebSocket.h"

namespace sk::streams {

namespace {

std::atomic<bool> g_stop{false};
std::atomic<bool> g_alertsRunning{false}, g_kickRunning{false};
const char* kScopes = "moderator:read:followers channel:read:subscriptions bits:read";

std::string Form(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string out;
    for (auto& [k, v] : kv) {
        if (!out.empty()) out += '&';
        out += k + '=';
        for (unsigned char c : v) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
            else {
                char b[4];
                snprintf(b, sizeof b, "%%%02X", c);
                out += b;
            }
        }
    }
    return out;
}

http::Response PostForm(const std::wstring& url, const std::string& body) {
    return http::Request(L"POST", url, {{L"Content-Type", L"application/x-www-form-urlencoded"}}, body);
}

bool SaveTokens(const json& j) {
    std::string at = j.value("access_token", "");
    if (at.empty()) return false;
    auto& c = Cfg();
    c.twitchTokenSealed = Settings::Seal(at);
    c.twitchRefreshSealed = Settings::Seal(j.value("refresh_token", ""));
    SaveSettings();
    return true;
}

bool Refresh() {
    auto& c = Cfg();
    std::string rt = Settings::Open(c.twitchRefreshSealed);
    if (rt.empty()) return false;
    auto r = PostForm(L"https://id.twitch.tv/oauth2/token",
                      Form({{"client_id", RemoteCfg().twitchClientId}, {"grant_type", "refresh_token"}, {"refresh_token", rt}}));
    if (!r.ok()) return false;
    return SaveTokens(json::parse(r.body, nullptr, false));
}

http::Headers Helix() {
    return {{L"Client-Id", Widen(RemoteCfg().twitchClientId)},
            {L"Authorization", L"Bearer " + Widen(Settings::Open(Cfg().twitchTokenSealed))},
            {L"Content-Type", L"application/json"}};
}

bool EnsureUser() {
    auto& c = Cfg();
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto r = http::Get(L"https://api.twitch.tv/helix/users", Helix());
        if (r.status == 401 && Refresh()) continue;
        if (!r.ok()) return false;
        auto j = json::parse(r.body, nullptr, false);
        if (!j.contains("data") || j["data"].empty()) return false;
        c.twitchUserId = j["data"][0].value("id", "");
        c.twitchLogin = j["data"][0].value("login", "");
        if (c.twitchChannel.empty()) c.twitchChannel = c.twitchLogin;
        SaveSettings();
        return true;
    }
    return false;
}

void Subscribe(const std::string& session, const std::string& type, const std::string& version, const json& condition) {
    json body = {{"type", type}, {"version", version}, {"condition", condition},
                 {"transport", {{"method", "websocket"}, {"session_id", session}}}};
    auto r = http::Request(L"POST", L"https://api.twitch.tv/helix/eventsub/subscriptions", Helix(), body.dump());
    if (!r.ok()) Log("EventSub " + type + " : " + std::to_string(r.status) + " " + r.body.substr(0, 200));
}

json ToAlert(const std::string& type, const json& e) {
    if (type == "channel.follow") return {{"type", "alert"}, {"kind", "follow"}, {"user", e.value("user_name", "")}};
    if (type == "channel.subscribe")
        return {{"type", "alert"}, {"kind", e.value("is_gift", false) ? "giftsub" : "sub"}, {"user", e.value("user_name", "")},
                {"tier", e.value("tier", "1000")}};
    if (type == "channel.subscription.message")
        return {{"type", "alert"}, {"kind", "resub"}, {"user", e.value("user_name", "")},
                {"months", e.value("cumulative_months", 1)}, {"message", e.contains("message") ? e["message"].value("text", "") : ""}};
    if (type == "channel.subscription.gift")
        return {{"type", "alert"}, {"kind", "gift"}, {"user", e.value("is_anonymous", false) ? "Anonyme" : e.value("user_name", "")},
                {"amount", e.value("total", 1)}};
    if (type == "channel.cheer")
        return {{"type", "alert"}, {"kind", "bits"}, {"user", e.value("is_anonymous", false) ? "Anonyme" : e.value("user_name", "")},
                {"amount", e.value("bits", 0)}, {"message", e.value("message", "")}};
    if (type == "channel.raid")
        return {{"type", "alert"}, {"kind", "raid"}, {"user", e.value("from_broadcaster_user_name", "")},
                {"amount", e.value("viewers", 0)}};
    return {};
}

void AlertsLoop(Emit emit) {
    std::wstring url = L"wss://eventsub.wss.twitch.tv/ws";
    while (!g_stop) {
        if (!TwitchConnected() || RemoteCfg().twitchClientId.empty() || !Cfg().alertsEnabled) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        if (Cfg().twitchUserId.empty() && !EnsureUser()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue;
        }
        Refresh();
        WebSocket ws;
        if (!ws.Connect(url)) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }
        std::string msg;
        bool reconnect = false;
        while (!g_stop && ws.Receive(msg)) {
            json m = json::parse(msg, nullptr, false);
            if (m.is_discarded()) continue;
            std::string kind = m["metadata"].value("message_type", "");
            if (kind == "session_welcome") {
                std::string session = m["payload"]["session"].value("id", "");
                std::string uid = Cfg().twitchUserId;
                Subscribe(session, "channel.follow", "2", {{"broadcaster_user_id", uid}, {"moderator_user_id", uid}});
                Subscribe(session, "channel.subscribe", "1", {{"broadcaster_user_id", uid}});
                Subscribe(session, "channel.subscription.message", "1", {{"broadcaster_user_id", uid}});
                Subscribe(session, "channel.subscription.gift", "1", {{"broadcaster_user_id", uid}});
                Subscribe(session, "channel.cheer", "1", {{"broadcaster_user_id", uid}});
                Subscribe(session, "channel.raid", "1", {{"to_broadcaster_user_id", uid}});
                url = L"wss://eventsub.wss.twitch.tv/ws";
            } else if (kind == "notification") {
                json a = ToAlert(m["metadata"].value("subscription_type", ""), m["payload"]["event"]);
                if (!a.empty()) emit(a);
            } else if (kind == "session_reconnect") {
                url = Widen(m["payload"]["session"].value("reconnect_url", "wss://eventsub.wss.twitch.tv/ws"));
                reconnect = true;
                break;
            }
        }
        if (!reconnect) std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void KickLoop(Emit emit) {
    std::set<std::string> seen;
    std::string room, slug;
    bool primed = false;
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        auto& c = Cfg();
        if (c.kickChannel.empty() || !c.chatOverlay) continue;
        if (slug != c.kickChannel) {
            slug = c.kickChannel;
            room = overlays::KickChatroomId(slug, nullptr);
            seen.clear();
            primed = false;
        }
        if (room.empty()) continue;
        auto r = http::Get(L"https://web.kick.com/api/v1/chat/" + Widen(room) + L"/history",
                           {{L"User-Agent", L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36"},
                            {L"Accept", L"application/json"}});
        if (!r.ok()) continue;
        json j = json::parse(r.body, nullptr, false);
        if (j.is_discarded() || !j.contains("data")) continue;
        auto msgs = j["data"].value("messages", json::array());
        std::vector<json> fresh;
        for (auto& m : msgs) {
            std::string id = m.value("id", "");
            if (id.empty() || seen.count(id)) continue;
            seen.insert(id);
            if (primed) fresh.push_back(m);
        }
        primed = true;
        std::reverse(fresh.begin(), fresh.end());
        for (auto& m : fresh) {
            auto& s = m["sender"];
            emit({{"type", "chat"}, {"p", "k"}, {"user", s.value("username", "")},
                  {"color", s.contains("identity") ? s["identity"].value("color", "") : ""}, {"text", m.value("content", "")}});
        }
        if (seen.size() > 5000) seen.clear(), primed = false;
    }
}

}

DeviceCode TwitchStart() {
    DeviceCode dc;
    std::string id = RemoteCfg().twitchClientId;
    if (id.empty()) {
        dc.error = "La connexion Twitch n'est pas encore activée par l'auteur du kit.";
        return dc;
    }
    auto r = PostForm(L"https://id.twitch.tv/oauth2/device", Form({{"client_id", id}, {"scopes", kScopes}}));
    json j = json::parse(r.body, nullptr, false);
    if (!r.ok() || j.is_discarded()) {
        dc.error = "Twitch a refusé la demande (" + std::to_string(r.status) + ")";
        return dc;
    }
    dc.deviceCode = j.value("device_code", "");
    dc.userCode = j.value("user_code", "");
    dc.uri = j.value("verification_uri", "https://www.twitch.tv/activate");
    dc.interval = j.value("interval", 5);
    dc.expires = j.value("expires_in", 1800);
    return dc;
}

bool TwitchPoll(const DeviceCode& dc, const std::atomic<bool>& cancel, std::string* error) {
    auto deadline = GetTickCount64() + (ULONGLONG)dc.expires * 1000;
    while (!cancel && GetTickCount64() < deadline) {
        for (int i = 0; i < dc.interval * 10 && !cancel; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto r = PostForm(L"https://id.twitch.tv/oauth2/token",
                          Form({{"client_id", RemoteCfg().twitchClientId},
                                {"scopes", kScopes},
                                {"device_code", dc.deviceCode},
                                {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}}));
        json j = json::parse(r.body, nullptr, false);
        if (r.ok() && SaveTokens(j)) {
            Cfg().twitchUserId.clear();
            EnsureUser();
            return true;
        }
        std::string msg = j.is_discarded() ? "" : j.value("message", "");
        if (msg != "authorization_pending" && !msg.empty() && msg != "slow_down") {
            if (error) *error = msg;
            return false;
        }
    }
    if (error) *error = cancel ? "Annulé" : "Le code a expiré";
    return false;
}

bool TwitchConnected() { return !Cfg().twitchTokenSealed.empty(); }

void TwitchDisconnect() {
    auto& c = Cfg();
    c.twitchTokenSealed.clear();
    c.twitchRefreshSealed.clear();
    c.twitchUserId.clear();
    SaveSettings();
}

void StartAlerts(Emit emit) {
    if (g_alertsRunning.exchange(true)) return;
    std::thread([emit] { AlertsLoop(emit); }).detach();
}

void StartKickChat(Emit emit) {
    if (g_kickRunning.exchange(true)) return;
    std::thread([emit] { KickLoop(emit); }).detach();
}

void StopAll() { g_stop = true; }

}
