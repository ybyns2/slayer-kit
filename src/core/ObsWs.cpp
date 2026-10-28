#include "core/ObsWs.h"

#include <shlobj.h>

#include <mutex>
#include <random>

#include "core/Crypto.h"
#include "core/WebSocket.h"

namespace sk::obsws {

namespace {

std::mutex g_mutex;

fs::path ConfigPath() {
    return KnownFolder(FOLDERID_RoamingAppData) / L"obs-studio" / L"plugin_config" / L"obs-websocket" / L"config.json";
}

std::string RandomPassword() {
    static const char* chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    std::random_device rd;
    std::string s;
    for (int i = 0; i < 24; ++i) s += chars[rd() % 57];
    return s;
}

std::string Sha256B64(const std::string& s) {
    std::vector<unsigned char> v(s.begin(), s.end());
    std::string hex = crypto::Sha256Hex(v);
    std::vector<unsigned char> raw;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) raw.push_back((unsigned char)std::stoi(hex.substr(i, 2), nullptr, 16));
    return Base64Encode(raw);
}

}

void EnableServer() {
    json cfg = json::object();
    try {
        cfg = json::parse(ReadFileText(ConfigPath()));
    } catch (...) {
    }
    std::string pass = cfg.value("server_password", "");
    if (pass.size() < 8) pass = RandomPassword();
    cfg["server_enabled"] = true;
    cfg["auth_required"] = true;
    cfg["server_password"] = pass;
    if (!cfg.contains("server_port")) cfg["server_port"] = 4455;
    cfg["first_load"] = false;
    cfg["alerts_enabled"] = false;
    WriteFileText(ConfigPath(), cfg.dump(2));
    Cfg().obsWsPasswordSealed = Settings::Seal(pass);
    Cfg().obsWsPort = cfg.value("server_port", 4455);
    SaveSettings();
}

json Request(const std::string& type, const json& data, std::string* error) {
    std::lock_guard lock(g_mutex);
    auto fail = [&](const std::string& e) {
        if (error) *error = e;
        return json();
    };
    if (!IsProcessRunning(L"obs64.exe")) return fail("OBS n'est pas ouvert");
    WebSocket ws;
    if (!ws.Connect(L"ws://127.0.0.1:" + std::to_wstring(Cfg().obsWsPort))) return fail("Connexion à OBS impossible");
    std::string msg;
    if (!ws.Receive(msg)) return fail("OBS ne répond pas");
    json hello = json::parse(msg, nullptr, false);
    json identify = {{"op", 1}, {"d", {{"rpcVersion", 1}, {"eventSubscriptions", 0}}}};
    if (hello.contains("d") && hello["d"].contains("authentication")) {
        auto& a = hello["d"]["authentication"];
        std::string secret = Sha256B64(Settings::Open(Cfg().obsWsPasswordSealed) + a.value("salt", ""));
        identify["d"]["authentication"] = Sha256B64(secret + a.value("challenge", ""));
    }
    ws.Send(identify.dump());
    if (!ws.Receive(msg) || json::parse(msg, nullptr, false).value("op", -1) != 2) return fail("Mot de passe OBS refusé");
    ws.Send(json{{"op", 6}, {"d", {{"requestType", type}, {"requestId", "sk"}, {"requestData", data}}}}.dump());
    for (int i = 0; i < 20 && ws.Receive(msg); ++i) {
        json r = json::parse(msg, nullptr, false);
        if (r.value("op", -1) == 7 && r["d"].value("requestId", "") == "sk") {
            if (!r["d"]["requestStatus"].value("result", false))
                return fail(r["d"]["requestStatus"].value("comment", "Requête refusée par OBS"));
            return r["d"].value("responseData", json::object());
        }
    }
    return fail("Pas de réponse d'OBS");
}

}
