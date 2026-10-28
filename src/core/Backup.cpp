#include "core/Backup.h"

#include <shlobj.h>

#include <algorithm>
#include <cwctype>

#include "core/Config.h"

namespace sk::backup {

namespace {

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    return s;
}

const wchar_t* kSkipDirs[] = {L"cache",       L"code cache", L"gpucache",      L"blob_storage", L"logs",
                              L"crashes",     L"crashpad",   L"profiler_data", L"updates",      L"obs-browser",
                              L"dawncache",   L"shadercache", L"service worker", L"indexeddb",   L"session storage",
                              L"local storage", L"network",  L"webstorage",    L"temp",         L"tmp",
                              L"server-cache", L"server-cache-priv", L"nui-storage", L"browser",
                              L"gecko_cache", L"effect",     L"transition",    L"dawnwebgpucache", L"dawngraphitecache",
                              L"ttnetcache",  L"aiassets",   L"scene_screenshot", L"instant_replay_covers",
                              L"camera_screenshot", L"watch_dog", L"driverupdater", L"shared dictionary",
                              L"sharedstorage", L"crashreports", L"plugin_config_cache"};

const wchar_t* kSecretNames[] = {L"service.json", L"cookies",       L"cookies-journal", L"login data",
                                 L"web data",     L"local state",   L"ros_id.dat",      L"auth.json",
                                 L"tokens.json",  L"secrets.json",  L"ttnet.json",      L"dips",
                                 L"tt_crash_reporter_config.dat",   L"ttwebview_server.json"};

const wchar_t* kSecretDirs[] = {L"ttstore", L"local storage", L"session storage", L"indexeddb", L"network"};

constexpr unsigned long long kMaxFile = 50ull * 1024 * 1024;
constexpr unsigned long long kMaxGithubFile = 25ull * 1024 * 1024;

bool SkipDir(const fs::path& name) {
    auto n = Lower(name.wstring());
    for (auto s : kSkipDirs)
        if (n == s) return true;
    return false;
}

fs::path Docs() { return KnownFolder(FOLDERID_Documents); }

unsigned long long DirSize(const fs::path& p) {
    unsigned long long total = 0;
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) return fs::file_size(p, ec);
    for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec) && SkipDir(it->path().filename())) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec)) {
            auto s = it->file_size(ec);
            if (s <= kMaxFile) total += s;
        }
    }
    return total;
}

bool CopyFiltered(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (fs::is_regular_file(from, ec)) {
        fs::create_directories(to.parent_path(), ec);
        return fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    }
    for (auto it = fs::recursive_directory_iterator(from, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) {
            if (SkipDir(it->path().filename())) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec) || it->file_size(ec) > kMaxFile) continue;
        fs::path rel = fs::relative(it->path(), from, ec);
        fs::create_directories((to / rel).parent_path(), ec);
        fs::copy_file(it->path(), to / rel, fs::copy_options::overwrite_existing, ec);
    }
    return true;
}

bool Zip(const fs::path& dir, const fs::path& zip) {
    std::error_code ec;
    fs::create_directories(zip.parent_path(), ec);
    auto r = RunCommand(L"tar.exe -a -c -f \"" + zip.wstring() + L"\" -C \"" + dir.wstring() + L"\" .");
    return r.exitCode == 0 && fs::exists(zip, ec);
}

bool Unzip(const fs::path& zip, const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto r = RunCommand(L"tar.exe -x -f \"" + zip.wstring() + L"\" -C \"" + dir.wstring() + L"\"");
    return r.exitCode == 0;
}

}

fs::path LocalBackupDir() {
    #ifdef SK_DEV
    fs::path p = Docs() / L"Slayer Kit (dev)" / L"Sauvegardes";
#else
    fs::path p = Docs() / L"Slayer Kit" / L"Sauvegardes";
#endif
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path OldObsKitsDir() {
    fs::path p = Docs() / L"Anciens kits OBS";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

std::vector<Source> Sources() {
    fs::path roaming = KnownFolder(FOLDERID_RoamingAppData);
    fs::path local = KnownFolder(FOLDERID_LocalAppData);
    std::vector<Source> list = {
        {"obs", L"OBS Studio (scènes, profils, scripts, plugins)", roaming / L"obs-studio"},
        {"tiktok-studio", L"TikTok LIVE Studio", roaming / L"TikTok LIVE Studio"},
        {"tiktok-studio-local", L"TikTok LIVE Studio (local)", local / L"TikTok LIVE Studio"},
        {"streamlabs", L"Streamlabs Desktop", roaming / L"slobs-client"},
        {"fivem", L"FiveM (réglages et touches)", roaming / L"CitizenFX"},
        {"fivem-ini", L"FiveM (CitizenFX.ini)", local / L"FiveM" / L"FiveM.app" / L"CitizenFX.ini"},
        {"gta", L"GTA V (réglages graphiques)", Docs() / L"Rockstar Games" / L"GTA V" / L"settings.xml"},
        {"slayerkit", L"Slayer Kit", AppDataDir()},
    };
    std::error_code ec;
    for (auto& s : list) {
        s.exists = fs::exists(s.path, ec);
        if (s.exists) s.bytes = DirSize(s.path);
    }
    return list;
}

bool IsSecretFile(const fs::path& relative) {
    auto n = Lower(relative.filename().wstring());
    for (auto s : kSecretNames)
        if (n == s) return true;
    auto full = Lower(relative.generic_wstring());
    if (full.find(L"obs-websocket/config.json") != std::wstring::npos) return true;
    if (full.rfind(L"tiktok-studio", 0) == 0 && relative.extension() == L".db") return true;
    for (auto& part : relative) {
        auto p = Lower(part.wstring());
        for (auto d : kSecretDirs)
            if (p == d) return true;
    }
    return false;
}

bool Create(const std::string& reason, const Step& step, Entry* out, std::string* error) {
    std::string stamp = NowStamp();
    fs::path staging = TempDir() / Widen("backup-" + stamp);
    std::error_code ec;
    fs::remove_all(staging, ec);

    auto sources = Sources();
    json manifest = {{"app", "Slayer Kit"}, {"version", SK_VERSION}, {"stamp", stamp}, {"reason", reason},
                     {"sources", json::array()}};
    int i = 0;
    for (auto& s : sources) {
        ++i;
        if (!s.exists) continue;
        if (step) step(L"Sauvegarde : " + s.label, (float)i / (sources.size() + 1));
        CopyFiltered(s.path, staging / Widen(s.id) / (fs::is_regular_file(s.path, ec) ? s.path.filename() : fs::path{}));
        manifest["sources"].push_back({{"id", s.id}, {"path", Narrow(s.path.wstring())},
                                       {"file", fs::is_regular_file(s.path, ec)}});
    }
    WriteFileText(staging / L"slayerkit-backup.json", manifest.dump(2));

    if (step) step(L"Compression de la sauvegarde", 0.95f);
    fs::path zip = LocalBackupDir() / Widen("SlayerKit_" + stamp + ".zip");
    bool ok = Zip(staging, zip);
    fs::remove_all(staging, ec);
    if (!ok) {
        if (error) *error = "Compression impossible";
        return false;
    }
    if (out) {
        out->zip = zip;
        out->stamp = stamp;
        out->reason = reason;
        out->bytes = fs::file_size(zip, ec);
    }
    WriteFileText(zip.wstring() + L".json", json{{"stamp", stamp}, {"reason", reason}}.dump());
    Log("Sauvegarde creee : " + Narrow(zip.wstring()));
    Rotate(Cfg().localBackupsToKeep);
    if (step) step(L"Sauvegarde terminée", 1.f);
    return true;
}

std::vector<Entry> List() {
    std::vector<Entry> out;
    std::error_code ec;
    for (auto& f : fs::directory_iterator(LocalBackupDir(), ec)) {
        if (f.path().extension() != L".zip") continue;
        Entry e;
        e.zip = f.path();
        e.bytes = f.file_size(ec);
        try {
            auto meta = json::parse(ReadFileText(f.path().wstring() + L".json"));
            e.stamp = meta.value("stamp", "");
            e.reason = meta.value("reason", "");
        } catch (...) {
            e.stamp = Narrow(f.path().stem().wstring());
        }
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.stamp > b.stamp; });
    return out;
}

void Rotate(int keep) {
    auto list = List();
    std::error_code ec;
    for (size_t i = (size_t)std::max(keep, 1); i < list.size(); ++i) {
        fs::remove(list[i].zip, ec);
        fs::remove(list[i].zip.wstring() + L".json", ec);
    }
}

bool StageForUpload(const Entry& e, fs::path* stagingDir, std::string* error) {
    fs::path dir = TempDir() / Widen("upload-" + e.stamp);
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (!Unzip(e.zip, dir)) {
        if (error) *error = "Lecture de la sauvegarde impossible";
        return false;
    }
    std::vector<fs::path> drop;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (IsSecretFile(fs::relative(it->path(), dir, ec)) || it->file_size(ec) > kMaxGithubFile) drop.push_back(it->path());
    }
    for (auto& p : drop) fs::remove(p, ec);
    if (!drop.empty()) Log("GitHub : " + std::to_string(drop.size()) + " fichier(s) secret(s) ou trop lourd(s) exclus");
    fs::path settings = dir / L"slayerkit" / L"settings.json";
    if (fs::exists(settings, ec)) {
        try {
            auto j = json::parse(ReadFileText(settings));
            j["githubTokenSealed"] = "";
            WriteFileText(settings, j.dump(2));
        } catch (...) {
            fs::remove(settings, ec);
        }
    }
    *stagingDir = dir;
    return true;
}

bool Restore(const fs::path& zip, const Step& step, std::string* error) {
    if (IsProcessRunning(L"obs64.exe")) {
        if (error) *error = "Ferme OBS avant de restaurer.";
        return false;
    }
    Entry safety;
    if (step) step(L"Sauvegarde de sécurité de l'état actuel", 0.1f);
    Create("Avant restauration", nullptr, &safety, nullptr);

    fs::path dir = TempDir() / L"restore";
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (step) step(L"Lecture de la sauvegarde", 0.3f);
    if (!Unzip(zip, dir)) {
        if (error) *error = "Archive illisible";
        return false;
    }
    json manifest;
    try {
        manifest = json::parse(ReadFileText(dir / L"slayerkit-backup.json"));
    } catch (...) {
        if (error) *error = "Ce fichier n'est pas une sauvegarde Slayer Kit";
        return false;
    }
    auto& srcs = manifest["sources"];
    int i = 0;
    for (auto& s : srcs) {
        ++i;
        std::string id = s.value("id", "");
        fs::path target = Widen(s.value("path", ""));
        if (id.empty() || target.empty()) continue;
        if (step) step(L"Restauration : " + Widen(id), 0.3f + 0.7f * i / srcs.size());
        fs::path from = dir / Widen(id);
        if (s.value("file", false)) {
            for (auto& f : fs::directory_iterator(from, ec)) {
                fs::create_directories(target.parent_path(), ec);
                fs::copy_file(f.path(), target, fs::copy_options::overwrite_existing, ec);
            }
        } else {
            fs::create_directories(target, ec);
            fs::copy(from, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
        if (ec) Log("Restauration " + id + " : " + ec.message());
    }
    fs::remove_all(dir, ec);
    Log("Restauration terminee depuis " + Narrow(zip.wstring()));
    return true;
}

bool ZipDir(const fs::path& dir, const fs::path& zip) { return Zip(dir, zip); }

bool ArchiveExistingObs(std::string* error) {
    fs::path obs = KnownFolder(FOLDERID_RoamingAppData) / L"obs-studio";
    std::error_code ec;
    if (!fs::exists(obs, ec)) return true;
    fs::path staging = TempDir() / L"old-obs";
    fs::remove_all(staging, ec);
    CopyFiltered(obs, staging / L"obs-studio");
    WriteFileText(staging / L"slayerkit-backup.json",
                  json{{"app", "Slayer Kit"}, {"stamp", NowStamp()}, {"reason", "OBS avant Slayer Kit"},
                       {"sources", {{{"id", "obs-studio"}, {"path", Narrow(obs.wstring())}, {"file", false}}}}}
                      .dump(2));
    fs::path zip = OldObsKitsDir() / Widen("OBS_avant_SlayerKit_" + NowStamp() + ".zip");
    bool ok = Zip(staging, zip);
    fs::remove_all(staging, ec);
    if (!ok && error) *error = "Archivage de l'ancien OBS impossible";
    return ok;
}

}
