#include "core/Packs.h"

#include <algorithm>
#include <cwctype>

#include "core/Constants.h"
#include "core/Crypto.h"

namespace sk::packs {

Color ParseHex(const std::string& hex, Color fallback) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    if (h.size() != 6) return fallback;
    try {
        unsigned v = std::stoul(h, nullptr, 16);
        return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f};
    } catch (...) {
        return fallback;
    }
}

fs::path PacksDir() {
    fs::path p = LocalDataDir() / L"packs";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path Pack::localLogo() const { return logo.empty() ? fs::path{} : localDir / Widen(logo); }

const Pack* Catalog::find(const std::string& id) const {
    for (auto& p : packs)
        if (p.id == id) return &p;
    return nullptr;
}

static Pack FromJson(const json& j) {
    Pack p;
    p.id = j.value("id", "");
    p.name = j.value("name", p.id);
    p.version = j.value("version", "1.0.0");
    p.description = j.value("description", "");
    p.discord = j.value("discord", "");
    p.logo = j.value("logo", "");
    if (j.contains("fivem")) p.fivemTitles = j["fivem"].value("windowTitleContains", std::vector<std::string>{});
    if (j.contains("colors")) {
        auto& c = j["colors"];
        p.accent = ParseHex(c.value("accent", ""), p.accent);
        p.accent2 = ParseHex(c.value("accent2", ""), p.accent2);
        p.background = ParseHex(c.value("background", ""), p.background);
    }
    for (auto& f : j.value("files", json::array()))
        p.files.push_back({f.value("path", ""), f.value("sha256", ""), f.value("size", 0ull)});
    return p;
}

static std::vector<Pack> BuiltIn() {
    auto mk = [](const char* id, const char* name, const char* a, const char* a2, const char* bg, const char* d,
                 std::vector<std::string> titles) {
        Pack p;
        p.id = id;
        p.name = name;
        p.accent = ParseHex(a);
        p.accent2 = ParseHex(a2);
        p.background = ParseHex(bg);
        p.discord = d;
        p.fivemTitles = std::move(titles);
        p.logo = "logo.png";
        return p;
    };
    return {
        mk("unity-legacy", "Unity Legacy", "#9B5CFF", "#D46BFF", "#0D0916", "discord.gg/unitylegacy", {"UNITY Legacy"}),
        mk("fratworld", "FratWorld", "#FF7A1A", "#FFB347", "#100B07", "", {"FratWorld"}),
        mk("night-fa", "Night FA", "#D63AF9", "#FFAA50", "#0C0712", "discord.gg/vzaD3s6PBS", {"NightFA", "Night FA"}),
    };
}

static void MarkInstalled(std::vector<Pack>& list) {
    for (auto& p : list) {
        p.localDir = PacksDir() / Widen(p.id);
        std::error_code ec;
        p.installed = fs::exists(p.localDir / L"pack.json", ec);
        if (p.installed) {
            try {
                p.installedVersion = json::parse(ReadFileText(p.localDir / L"pack.json")).value("version", "");
            } catch (...) {
            }
        }
    }
}

Catalog LoadCatalog() {
    Catalog c;
    fs::path cache = PacksDir() / L"index.json";
    auto r = http::Get(k::kPacksIndexUrl);
    std::string text;
    if (r.ok()) {
        text = r.body;
        c.online = true;
        WriteFileText(cache, text);
    } else {
        c.error = r.error.empty() ? "Catalogue indisponible (" + std::to_string(r.status) + ")" : r.error;
        text = ReadFileText(cache);
    }
    try {
        if (!text.empty()) {
            json idx = json::parse(text);
            for (auto& j : idx.value("packs", json::array())) c.packs.push_back(FromJson(j));
            json common = idx.value("common", json::object());
            c.commonVersion = common.value("version", "");
            c.commonMusic = common.value("music", json::object());
            for (auto& f : common.value("files", json::array()))
                c.commonFiles.push_back({f.value("path", ""), f.value("sha256", ""), f.value("size", 0ull)});
        }
    } catch (const std::exception& e) {
        Log(std::string("index.json invalide : ") + e.what());
    }
    if (c.packs.empty()) c.packs = BuiltIn();
    MarkInstalled(c.packs);
    return c;
}

bool InstallPack(const Pack& p, const http::Progress& progress, std::string* error) {
    fs::path dir = PacksDir() / Widen(p.id);
    fs::path staging = PacksDir() / Widen(p.id + ".new");
    std::error_code ec;
    fs::remove_all(staging, ec);

    unsigned long long total = 0, done = 0;
    for (auto& f : p.files) total += f.size;

    std::wstring base = std::wstring(k::kPacksRawBase) + L"packs/" + Widen(p.id) + L"/";
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        Log("Pack " + p.id + " : " + why);
        fs::remove_all(staging, ec);
        return false;
    };

    if (!http::Download(base + L"pack.json", staging / L"pack.json")) return fail("pack.json introuvable");
    for (auto& f : p.files) {
        if (f.path.find("..") != std::string::npos) return fail("chemin refuse : " + f.path);
        fs::path dest = staging / Widen(f.path);
        unsigned long long before = done;
        bool ok = http::Download(base + Widen(f.path), dest, [&](auto d, auto) {
            if (progress) progress(before + d, total);
        });
        if (!ok) return fail("echec du telechargement de " + f.path);
        if (!f.sha256.empty() && crypto::Sha256File(dest) != f.sha256) return fail("fichier corrompu : " + f.path);
        done = before + f.size;
    }
    fs::remove_all(dir, ec);
    fs::rename(staging, dir, ec);
    if (ec) return fail("installation impossible : " + ec.message());
    return true;
}

fs::path CommonDir() { return PacksDir() / L"communs"; }

bool CommonInstalled() {
    std::error_code ec;
    return fs::exists(CommonDir() / L"communs.json", ec);
}

bool InstallCommon(const Catalog& c, std::string* error) {
    if (c.commonFiles.empty()) {
        if (CommonInstalled()) return true;
        if (error) *error = "Musiques du kit indisponibles hors ligne";
        return false;
    }
    fs::path dir = CommonDir(), staging = PacksDir() / L"communs.new";
    std::error_code ec;
    fs::remove_all(staging, ec);
    std::wstring base = std::wstring(k::kPacksRawBase) + L"communs/";
    for (auto& f : c.commonFiles) {
        if (f.path.find("..") != std::string::npos) continue;
        fs::path dest = staging / Widen(f.path);
        fs::path cached = dir / Widen(f.path);
        if (fs::exists(cached, ec) && crypto::Sha256File(cached) == f.sha256) {
            fs::create_directories(dest.parent_path(), ec);
            fs::copy_file(cached, dest, fs::copy_options::overwrite_existing, ec);
            continue;
        }
        if (!http::Download(base + Widen(f.path), dest) || (!f.sha256.empty() && crypto::Sha256File(dest) != f.sha256)) {
            if (error) *error = "Téléchargement impossible : " + f.path;
            fs::remove_all(staging, ec);
            return false;
        }
    }
    WriteFileText(staging / L"communs.json", json{{"version", c.commonVersion}, {"music", c.commonMusic}}.dump(2));
    fs::remove_all(dir, ec);
    fs::rename(staging, dir, ec);
    return !ec;
}

const Pack* DetectFromFiveMTitle(const Catalog& c, const std::wstring& title) {
    std::wstring t = title;
    std::transform(t.begin(), t.end(), t.begin(), [](wchar_t ch) { return (wchar_t)std::towlower(ch); });
    for (auto& p : c.packs)
        for (auto& needle : p.fivemTitles) {
            std::wstring n = Widen(needle);
            std::transform(n.begin(), n.end(), n.begin(), [](wchar_t ch) { return (wchar_t)std::towlower(ch); });
            if (!n.empty() && t.find(n) != std::wstring::npos) return &p;
        }
    return nullptr;
}

}
