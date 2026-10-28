#include <qrcodegen.hpp>

#include <algorithm>
#include <random>

#include "app/App.h"
#include "core/Config.h"
#include "core/ObsWs.h"
#include "core/Server.h"
#include "core/Streams.h"
#include "core/WebPages.h"

namespace sk {

using namespace ui;

namespace {

std::string NewToken() {
    static const char* chars = "abcdefghijkmnopqrstuvwxyz23456789";
    std::random_device rd;
    std::string s;
    for (int i = 0; i < 20; ++i) s += chars[rd() % 33];
    return s;
}

server::Response Json(const json& j, int status = 200) { return {status, "application/json", j.dump()}; }

}

std::wstring App::RemoteUrl() const {
    return L"http://" + server::LanAddress() + L":" + std::to_wstring(Cfg().serverPort) + L"/remote?t=" + Widen(Cfg().remoteToken);
}

void App::StartServices() {
    auto& c = Cfg();
    if (c.serverToken.empty() || c.remoteToken.empty()) {
        if (c.serverToken.empty()) c.serverToken = NewToken();
        if (c.remoteToken.empty()) c.remoteToken = NewToken();
        SaveSettings();
    }
    server::Stop();
    auto handler = [this](const server::Request& r) -> server::Response {
        auto t = r.query.find("t");
        std::string given = t == r.query.end() ? "" : t->second;
        bool overlayPath = r.path.rfind("/overlay/", 0) == 0 || r.path == "/events";
        if (overlayPath) {
            if (!r.local || given != Cfg().serverToken) return {403, "text/plain", "Lien invalide"};
        } else {
            if (!r.local && !Cfg().remoteEnabled) return {403, "text/plain", "Télécommande désactivée"};
            if (given != Cfg().remoteToken) return {403, "text/plain", "Lien invalide"};
        }
        web::Theme theme = web::FromPack(ActivePack());
        if (r.path == "/overlay/chat") return {200, "text/html; charset=utf-8", web::ChatPage(theme, Cfg().serverToken)};
        if (r.path == "/overlay/alerts") return {200, "text/html; charset=utf-8", web::AlertsPage(theme, Cfg().serverToken)};
        if (r.path == "/events") return {200, "text/event-stream", ""};
        if (r.path == "/remote") return {200, "text/html; charset=utf-8", web::RemotePage(theme, Cfg().remoteToken)};
        std::string err;
        if (r.path == "/api/launch") {
            Post([this] { LaunchLive(); });
            return Json({{"ok", true}, {"message", "OBS s'ouvre avec ton kit…"}});
        }
        if (r.path == "/api/state") {
            json out = {{"obs", true}, {"scenes", json::array()}, {"current", ""}, {"streaming", false}, {"micMuted", false}};
            if (!IsProcessRunning(L"obs64.exe")) return Json({{"obs", false}});
            json list = obsws::Request("GetSceneList", json::object(), &err);
            if (list.is_null()) return Json({{"obs", true}, {"error", err}});
            out["current"] = list.value("currentProgramSceneName", "");
            auto scenes = list.value("scenes", json::array());
            std::sort(scenes.begin(), scenes.end(), [](auto& a, auto& b) { return a.value("sceneIndex", 0) > b.value("sceneIndex", 0); });
            for (auto& s : scenes) out["scenes"].push_back(s.value("sceneName", ""));
            json st = obsws::Request("GetStreamStatus", json::object(), nullptr);
            if (!st.is_null()) out["streaming"] = st.value("outputActive", false);
            json mute = obsws::Request("GetInputMute", {{"inputName", "Micro"}}, nullptr);
            if (!mute.is_null()) out["micMuted"] = mute.value("inputMuted", false);
            return Json(out);
        }
        json result;
        if (r.path == "/api/scene") {
            auto n = r.query.find("name");
            if (n == r.query.end()) return Json({{"error", "Scène manquante"}}, 400);
            result = obsws::Request("SetCurrentProgramScene", {{"sceneName", n->second}}, &err);
        } else if (r.path == "/api/mute") {
            result = obsws::Request("ToggleInputMute", {{"inputName", "Micro"}}, &err);
        } else if (r.path == "/api/stream") {
            result = obsws::Request("ToggleStream", json::object(), &err);
        } else if (r.path == "/api/clip") {
            result = obsws::Request("SaveReplayBuffer", json::object(), &err);
            if (result.is_null() && IsProcessRunning(L"obs64.exe")) {
                if (!obsws::Request("StartReplayBuffer", json::object(), &err).is_null())
                    return Json({{"ok", true}, {"message", "Tampon de clips démarré : appuie de nouveau sur Clip dans quelques secondes."}});
                err = "Tampon de clips indisponible : réapplique le kit (Scènes) avec les clips activés.";
            }
            if (!result.is_null()) return Json({{"ok", true}, {"message", "Clip enregistré dans Vidéos !"}});
        } else if (r.path == "/api/test-alert") {
            int n = server::Broadcast(json{{"type", "alert"}, {"kind", "follow"}, {"user", "Slayer Kit"}}.dump());
            if (n == 0)
                return Json({{"error", "Aucun overlay d'alertes ouvert : ouvre OBS avec le kit (scène En live)."}}, 400);
            return Json({{"ok", true}, {"message", "Alerte envoyée dans OBS ✓"}});
        } else {
            return {404, "text/plain", "introuvable"};
        }
        if (result.is_null()) return Json({{"error", err}}, 400);
        return Json({{"ok", true}});
    };
#ifdef SK_DEV
    if (c.serverPort == 8787) c.serverPort = 8788;
#endif
    server::Start(c.serverPort, c.remoteEnabled, handler);
    auto emit = [](const json& j) { server::Broadcast(j.dump()); };
    streams::StartKickChat(emit);
    streams::StartAlerts(emit);
}

void App::DrawQr(const Rect& r, const std::wstring& text) {
    using qrcodegen::QrCode;
    QrCode qr = QrCode::encodeText(Narrow(text).c_str(), QrCode::Ecc::MEDIUM);
    int n = qr.getSize();
    gfx_.Fill(r, Color{1, 1, 1, 1}, 10);
    float cell = (r.w - 20) / n;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qr.getModule(x, y)) gfx_.Fill({r.x + 10 + x * cell, r.y + 10 + y * cell, cell + 0.4f, cell + 0.4f}, Color{0, 0, 0, 1});
}

void App::DrawRemoteCard(const Rect& r) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    ui_.Panel(r, 16);
    gfx_.Text(L"Télécommande téléphone", {r.x + 22, r.y + 16, r.w - 110, 26}, Font::BodyBold, t.text);
    if (ui_.Toggle("rm-on", {r.r() - 70, r.y + 16, 50, 26}, c.remoteEnabled)) {
        SaveSettings();
        StartServices();
    }
    if (!c.remoteEnabled) {
        gfx_.Text(L"Scanne un QR code avec ton téléphone (même Wi-Fi) : scènes, micro, live, clips en un doigt. "
                  L"Windows peut demander d'autoriser Slayer Kit sur le réseau privé.",
                  {r.x + 22, r.y + 50, r.w - 44, 60}, Font::Small, t.muted, Align::Left, false, true);
        return;
    }
    float q = std::min(r.h - 70, 170.f);
    DrawQr({r.x + 22, r.y + 52, q, q}, RemoteUrl());
    float tx = r.x + 22 + q + 18;
    gfx_.Text(L"Scanne avec l'appareil photo du téléphone (même Wi-Fi).", {tx, r.y + 52, r.r() - tx - 18, 44}, Font::Small, t.muted,
              Align::Left, false, true);
    gfx_.Text(RemoteUrl(), {tx, r.y + 100, r.r() - tx - 18, 40}, Font::Small, t.text, Align::Left, false, true);
    if (ui_.Button("rm-new", {tx, r.y + 150, 170, 34}, L"Nouveau lien", Style::Ghost, true, L"")) {
        c.remoteToken = NewToken();
        SaveSettings();
        Toast(L"Ancien lien désactivé.");
    }
}

void App::DrawAlertsCard(const Rect& r) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    ui_.Panel(r, 16);
    gfx_.Text(L"Alertes Twitch", {r.x + 22, r.y + 16, r.w - 110, 26}, Font::BodyBold, t.text);
    if (ui_.Toggle("al-on", {r.r() - 70, r.y + 16, 50, 26}, c.alertsEnabled)) SaveSettings();
    float y = r.y + 50;
    gfx_.Text(L"Follow, sub, resub, subs offerts, bits et raids : animation aux couleurs du serveur + son.",
              {r.x + 22, y, r.w - 44, 40}, Font::Small, t.muted, Align::Left, false, true);
    y += 44;
    if (streams::TwitchConnected()) {
        gfx_.Text(L"Connecté : " + Widen(c.twitchLogin.empty() ? "Twitch" : c.twitchLogin), {r.x + 22, y, r.w - 260, 34}, Font::SmallBold,
                  t.ok);
        if (ui_.Button("al-off", {r.r() - 150, y, 128, 32}, L"Déconnecter", Style::Ghost)) streams::TwitchDisconnect();
    } else if (twState_ == 1) {
        gfx_.Text(L"Va sur twitch.tv/activate et entre :", {r.x + 22, y, r.w - 44, 22}, Font::Small, t.muted);
        gfx_.Text(Widen(twCode_), {r.x + 22, y + 20, 220, 40}, Font::Title, t.text);
        if (ui_.Button("al-open", {r.r() - 180, y + 22, 158, 34}, L"Ouvrir Twitch", Style::Primary, true, L""))
            OpenUrl(L"https://www.twitch.tv/activate");
    } else {
        bool ready = !RemoteCfg().twitchClientId.empty();
        if (ui_.Button("al-connect", {r.x + 22, y, 220, 36}, L"Connecter Twitch", Style::Secondary, ready, L"")) {
            twState_ = 0;
            twCancel_ = false;
            Async([this] {
                auto dc = streams::TwitchStart();
                if (!dc.error.empty()) {
                    Post([this, e = dc.error] { Toast(Widen(e), false); });
                    return;
                }
                Post([this, dc] {
                    twCode_ = dc.userCode;
                    twState_ = 1;
                });
                std::string err;
                bool ok = streams::TwitchPoll(dc, twCancel_, &err);
                Post([this, ok, err] {
                    twState_ = 0;
                    Toast(ok ? L"Twitch connecté : les alertes sont actives." : L"Connexion Twitch : " + Widen(err), ok);
                });
            });
        }
        if (!ready)
            gfx_.Text(L"Bientôt : en attente de l'appli Twitch de l'auteur du kit.", {r.x + 22, y + 40, r.w - 44, 22}, Font::Small,
                      t.faint);
    }
    if (ui_.Button("al-test", {r.x + 22, r.b() - 50, 200, 34}, L"Tester une alerte", Style::Ghost, true, L"")) {
        int n = server::Broadcast(json{{"type", "alert"}, {"kind", "sub"}, {"user", c.twitchLogin.empty() ? "Viewer" : c.twitchLogin}}.dump());
        Toast(n ? L"Alerte envoyée dans OBS (scène En live)." : L"OBS n'affiche pas l'overlay d'alertes : ouvre OBS avec le kit, scène En live.", n > 0);
    }
}

}
