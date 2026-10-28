#pragma once
#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk {

using json = nlohmann::json;

struct Settings {
    bool firstRunDone = false;

   
    std::set<std::string> platforms;  
    std::string software = "obs";      

   
    std::vector<std::string> packs;
    std::string activePack;
    bool autoDetectServer = true;

   
    std::string channel = "stable";   
    bool alwaysKeepModifications = false;
    std::string pendingCriticalVersion;
    std::string skippedVersion;

    bool githubBackup = false;
    std::string githubUser;
    std::string githubTokenSealed;     
    int localBackupsToKeep = 10;

   
    bool startWithWindows = true;
    bool minimizeToTray = true;

    std::string micId;
    std::string micName;
    bool micTested = false;
    float micGainDb = 0;
    float gateOpenDb = -45;
    float gateCloseDb = -53;
    bool rnnoise = true;
    float compThresholdDb = -18;
    float compRatio = 4;
    int micQuality = 2;

    std::vector<std::string> musicApps;
    bool musicInVod = false;
    bool discordInLive = true;
    float gameVolume = 0.35f;
    float musicVolume = 0.25f;

    std::string musicStart, musicPause, musicEnd;

    std::vector<std::string> sceneOrder;
    std::set<std::string> scenesDisabled;
    std::string transition = "balayage";
    int countdownStartMin = 5;
    int countdownEndMin = 5;
    bool kitApplied = false;
    std::string appliedPack;
    bool fivemPending = false;

    std::string mainPlatform = "twitch";
    std::string twitchKeySealed, youtubeKeySealed, kickKeySealed, tiktokKeySealed;
    std::string tiktokServer;
    std::string twitchChannel, kickChannel;

    bool chatOverlay = true;
    std::string chatPosition = "droite";
    bool nowPlaying = true;
    bool rpCard = false;
    std::string rpName, rpJob, rpStatus;

    bool webhookEnabled = false;
    std::string webhookSealed;
    std::string webhookMessage = "@everyone Je suis en live !";
    std::string webhookTitle;
    std::string webhookDescription;
    bool webhookEnd = true;

    bool perfMode = true;
    bool replayBuffer = true;
    int replaySeconds = 60;
    json liveStats = json::array();

    std::string obsWsPasswordSealed;
    int obsWsPort = 4455;
    std::string serverToken;
    std::string remoteToken;
    int serverPort = 8787;
    bool remoteEnabled = false;
    bool alertsEnabled = true;
    std::string twitchTokenSealed, twitchRefreshSealed, twitchUserId, twitchLogin;
    json layout = json::object();

    static std::string Open(const std::string& sealed);
    static std::string Seal(const std::string& plain);
    std::string githubToken() const;
    void setGithubToken(const std::string& token);
};

void to_json(json& j, const Settings& s);
void from_json(const json& j, Settings& s);

Settings& Cfg();
void LoadSettings();
void SaveSettings();

struct Remote {
    std::string helpDiscord;
    std::string githubClientId;
    std::string twitchClientId;
    std::string vbCableUrl = "https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip";
};
Remote& RemoteCfg();

} 
