#include "core/Updater.h"

#include <shellapi.h>

#include <sstream>

#include "core/Config.h"
#include "core/Constants.h"
#include "core/Crypto.h"

namespace sk::updater {

static std::vector<int> Parts(const std::string& v) {
    std::vector<int> out;
    std::stringstream ss(v[0] == 'v' ? v.substr(1) : v);
    std::string item;
    while (std::getline(ss, item, '.')) {
        try {
            out.push_back(std::stoi(item));
        } catch (...) {
            out.push_back(0);
        }
    }
    while (out.size() < 3) out.push_back(0);
    return out;
}

int CompareVersions(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return a.empty() ? (b.empty() ? 0 : -1) : 1;
    auto x = Parts(a), y = Parts(b);
    for (size_t i = 0; i < std::max(x.size(), y.size()); ++i) {
        int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

Info Check() {
    Info info;
    info.channel = Cfg().channel;
    auto r = http::Get(k::kUpdateJsonUrl, {{L"Cache-Control", L"no-cache"}});
    if (!r.ok()) {
        info.error = r.error.empty() ? "update.json : HTTP " + std::to_string(r.status) : r.error;
        return info;
    }
    try {
        auto j = json::parse(r.body);
        info.reachable = true;
        auto& remote = RemoteCfg();
        remote.helpDiscord = j.value("helpDiscord", remote.helpDiscord);
        remote.githubClientId = j.value("githubClientId", remote.githubClientId);
        remote.twitchClientId = j.value("twitchClientId", remote.twitchClientId);
        remote.vbCableUrl = j.value("vbCableUrl", remote.vbCableUrl);

        auto channels = j.value("channels", json::object());
        json ch = channels.value(info.channel, json::object());
        if (info.channel == "beta") {
            json st = channels.value("stable", json::object());
            if (CompareVersions(st.value("version", ""), ch.value("version", "")) > 0) ch = st;
        }
        info.version = ch.value("version", "");
        info.notes = ch.value("notes", "");
        info.installerUrl = ch.value("installer", "");
        info.sha256 = ch.value("sha256", "");
        info.critical = ch.value("critical", false) ||
                        CompareVersions(SK_VERSION, j.value("minimumVersion", "0.0.0")) < 0;
        info.available = !info.installerUrl.empty() && CompareVersions(info.version, SK_VERSION) > 0;
        if (!info.available) info.critical = false;
    } catch (const std::exception& e) {
        info.error = std::string("update.json invalide : ") + e.what();
    }
    return info;
}

bool StreamingSoftwareRunning() {
    for (auto exe : {L"obs64.exe", L"TikTok LIVE Studio.exe", L"TikTokLIVEStudio.exe", L"Streamlabs Desktop.exe"})
        if (IsProcessRunning(exe)) return true;
    return false;
}

bool Download(const Info& info, fs::path* installer, const http::Progress& progress, std::string* error) {
    if (info.installerUrl.rfind("https://github.com/" + Narrow(k::kGithubOwner) + "/", 0) != 0) {
        if (error) *error = "Adresse de mise à jour refusée (doit venir du GitHub officiel du kit).";
        return false;
    }
    fs::path dest = TempDir() / Widen("SlayerKit-Setup-" + info.version + ".exe");
    if (!http::Download(Widen(info.installerUrl), dest, progress)) {
        if (error) *error = "Téléchargement de la mise à jour impossible.";
        return false;
    }
    if (info.sha256.empty() || crypto::Sha256File(dest) != info.sha256) {
        std::error_code ec;
        fs::remove(dest, ec);
        if (error) *error = "Empreinte SHA-256 incorrecte : mise à jour refusée par sécurité.";
        return false;
    }
    *installer = dest;
    return true;
}

bool LaunchInstallerAndExit(const fs::path& installer) {
    SHELLEXECUTEINFOW sei{sizeof sei};
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"open";
    sei.lpFile = installer.c_str();
    sei.lpParameters = L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /RELAUNCH";
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return false;
    Log("Installation de la mise a jour lancee");
    return true;
}

}
