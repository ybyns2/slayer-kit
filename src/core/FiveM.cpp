#include "core/FiveM.h"

#include <shlobj.h>

#include <regex>
#include <sstream>

namespace sk::fivem {

namespace {

struct Rule {
    const char* key;
    const char* wanted;
    const wchar_t* label;
    const wchar_t* why;
};

const Rule kRules[] = {
    {"profile_audioMuteOnFocusLoss", "0", L"Son coupé hors focus",
     L"Sinon le son du jeu disparaît du live dès que tu cliques sur OBS ou Discord."},
    {"profile_musicVolumeInMp", "0", L"Radio GTA en multijoueur",
     L"Les musiques de la radio GTA sont protégées : coupées pour éviter les mutes Twitch/TikTok."},
    {"profile_voiceEnable", "1", L"Chat vocal activé", L"Indispensable pour entendre et être entendu en RP."},
    {"profile_voiceOutputDevice", "0", L"Sortie du vocal = défaut Windows",
     L"Le vocal de proximité sort sur le même périphérique que le jeu, capté proprement par le kit."},
    {"profile_voiceInputDevice", "0", L"Micro en jeu = micro par défaut", L"Le même micro que celui réglé par le kit."},
};

std::string ValueOf(const std::string& cfg, const std::string& key) {
    std::regex re("seta \"" + key + "\" \"([^\"]*)\"");
    std::smatch m;
    if (std::regex_search(cfg, m, re)) return m[1];
    return {};
}

}

fs::path ConfigPath() { return KnownFolder(FOLDERID_RoamingAppData) / L"CitizenFX" / L"fivem.cfg"; }

bool Running() {
    for (auto& p : RunningProcessNames())
        if (_wcsnicmp(p.c_str(), L"FiveM", 5) == 0) return true;
    return false;
}

std::vector<Fix> Check() {
    std::vector<Fix> out;
    std::string cfg = ReadFileText(ConfigPath());
    if (cfg.empty()) return out;
    for (auto& r : kRules) {
        std::string cur = ValueOf(cfg, r.key);
        if (cur.empty() || cur == r.wanted) continue;
        out.push_back({r.key, cur, r.wanted, r.label, r.why});
    }
    return out;
}

bool Apply(const std::vector<Fix>& fixes, std::string* error) {
    if (Running()) {
        if (error) *error = "FiveM est ouvert : les réglages seront appliqués à sa fermeture.";
        return false;
    }
    fs::path p = ConfigPath();
    std::string cfg = ReadFileText(p);
    if (cfg.empty()) {
        if (error) *error = "fivem.cfg introuvable";
        return false;
    }
    std::error_code ec;
    fs::copy_file(p, p.wstring() + L".avant-slayerkit", fs::copy_options::skip_existing, ec);
    for (auto& f : fixes) {
        std::regex re("seta \"" + f.key + "\" \"[^\"]*\"");
        cfg = std::regex_replace(cfg, re, "seta \"" + f.key + "\" \"" + f.wanted + "\"");
    }
    if (!WriteFileText(p, cfg)) {
        if (error) *error = "Écriture de fivem.cfg impossible";
        return false;
    }
    Log("FiveM : " + std::to_string(fixes.size()) + " reglage(s) applique(s)");
    return true;
}

}
