#include "core/Config.h"

#include "core/Crypto.h"

namespace sk {

std::string Settings::Open(const std::string& sealed) { return crypto::Unprotect(sealed); }
std::string Settings::Seal(const std::string& plain) { return crypto::Protect(plain); }
std::string Settings::githubToken() const { return crypto::Unprotect(githubTokenSealed); }
void Settings::setGithubToken(const std::string& token) { githubTokenSealed = crypto::Protect(token); }

#define SK_SETTINGS_FIELDS(X) X(firstRunDone) X(platforms) X(software) X(packs) X(activePack) X(autoDetectServer) X(channel) X(alwaysKeepModifications) X(pendingCriticalVersion) X(skippedVersion) X(githubBackup) X(githubUser) X(githubTokenSealed) X(localBackupsToKeep) X(startWithWindows) X(minimizeToTray) X(micId) X(micName) X(micTested) X(micGainDb) X(gateOpenDb) X(gateCloseDb) X(rnnoise) X(compThresholdDb) X(compRatio) X(micQuality) X(musicApps) X(musicInVod) X(discordInLive) X(gameVolume) X(musicVolume) X(musicStart) X(musicPause) X(musicEnd) X(sceneOrder) X(scenesDisabled) X(transition) X(countdownStartMin) X(countdownEndMin) X(kitApplied) X(appliedPack) X(fivemPending) X(mainPlatform) X(twitchKeySealed) X(youtubeKeySealed) X(kickKeySealed) X(tiktokKeySealed) X(tiktokServer) X(twitchChannel) X(kickChannel) X(chatOverlay) X(chatPosition) X(nowPlaying) X(rpCard) X(rpName) X(rpJob) X(rpStatus) X(webhookEnabled) X(webhookSealed) X(webhookMessage) X(webhookTitle) X(webhookDescription) X(webhookEnd) X(perfMode) X(replayBuffer) X(replaySeconds) X(liveStats) X(obsWsPasswordSealed) X(obsWsPort) X(serverToken) X(remoteToken) X(serverPort) X(remoteEnabled) X(alertsEnabled) X(twitchTokenSealed) X(twitchRefreshSealed) X(twitchUserId) X(twitchLogin) X(layout)

void to_json(json& j, const Settings& s) {
    j = json::object();
#define SK_TO(f) j[#f] = s.f;
    SK_SETTINGS_FIELDS(SK_TO)
#undef SK_TO
}

template <class T>
static void Opt(const json& j, const char* key, T& out) {
    if (auto it = j.find(key); it != j.end() && !it->is_null()) {
        try {
            out = it->get<T>();
        } catch (...) {
        }
    }
}

void from_json(const json& j, Settings& s) {
#define SK_FROM(f) Opt(j, #f, s.f);
    SK_SETTINGS_FIELDS(SK_FROM)
#undef SK_FROM
}

static fs::path SettingsPath() { return AppDataDir() / L"settings.json"; }

Settings& Cfg() {
    static Settings s;
    return s;
}

Remote& RemoteCfg() {
    static Remote r;
    return r;
}

void LoadSettings() {
    std::string text = ReadFileText(SettingsPath());
    if (text.empty()) return;
    try {
        Cfg() = json::parse(text).get<Settings>();
    } catch (const std::exception& e) {
        Log(std::string("settings.json illisible, valeurs par defaut : ") + e.what());
    }
}

void SaveSettings() { WriteFileText(SettingsPath(), json(Cfg()).dump(2)); }

} 
