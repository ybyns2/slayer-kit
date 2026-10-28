#include "core/ObsKit.h"

#include <windows.h>
#include <dxgi.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <algorithm>

#include "core/Audio.h"
#include "core/Config.h"
#include "core/Crypto.h"
#include "core/Live.h"
#include "core/ObsWs.h"
#include "core/Overlays.h"

namespace sk::obskit {

namespace {

constexpr float W = 1920, H = 1080;

std::string g_uuidSeed;

std::string Uuid(const std::string& name) {
    std::string src = "slayerkit:" + g_uuidSeed + ":" + name;
    std::string h = crypto::Sha256Hex({src.begin(), src.end()});
    return h.substr(0, 8) + "-" + h.substr(8, 4) + "-4" + h.substr(13, 3) + "-a" + h.substr(17, 3) + "-" + h.substr(20, 12);
}

unsigned Abgr(const packs::Color& c, float a = 1) {
    auto ch = [](float v) { return (unsigned)std::clamp((int)std::lround(v * 255), 0, 255); };
    return (ch(a) << 24) | (ch(c.b) << 16) | (ch(c.g) << 8) | ch(c.r);
}

std::string Fwd(const fs::path& p) {
    std::string s = Narrow(p.wstring());
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string Encoder() {
    Microsoft::WRL::ComPtr<IDXGIFactory1> f;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f)))) return "obs_x264";
    Microsoft::WRL::ComPtr<IDXGIAdapter1> a;
    for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d;
        a->GetDesc1(&d);
        if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (d.VendorId == 0x10DE) return "obs_nvenc_h264_tex";
        if (d.VendorId == 0x1002) return "h264_texture_amf";
        if (d.VendorId == 0x8086) return "obs_qsv11_v2";
    }
    return "obs_x264";
}

struct Builder {
    json sources = json::array();
    std::map<std::string, std::string> uuidOf;

    json& Add(const std::string& name, const std::string& id, const std::string& vid, json settings, int mixers = 0,
              float volume = 1, bool muted = false) {
        std::string u = Uuid(name);
        uuidOf[name] = u;
        sources.push_back({{"name", name},
                           {"uuid", u},
                           {"id", id},
                           {"versioned_id", vid},
                           {"settings", std::move(settings)},
                           {"mixers", mixers},
                           {"sync", 0},
                           {"flags", 0},
                           {"volume", volume},
                           {"balance", 0.5},
                           {"enabled", true},
                           {"muted", muted},
                           {"push-to-mute", false},
                           {"push-to-talk", false},
                           {"monitoring_type", 0},
                           {"deinterlace_mode", 0},
                           {"deinterlace_field_order", 0},
                           {"hotkeys", json::object()},
                           {"private_settings", json::object()}});
        return sources.back();
    }

    json& Find(const std::string& name) {
        for (auto& s : sources)
            if (s["name"] == name) return s;
        throw std::runtime_error("source " + name);
    }

    json& Text(const std::string& name, const std::string& text, int size, unsigned color, int cx, int cy,
               const char* weight = "Bold", const char* align = "center") {
        return Add(name, "text_gdiplus", "text_gdiplus_v3",
                   {{"text", text},
                    {"font", {{"face", "Bahnschrift"}, {"style", weight}, {"size", size}, {"flags", 1}}},
                    {"color", color},
                    {"align", align},
                    {"valign", "center"},
                    {"extents", true},
                    {"extents_cx", cx},
                    {"extents_cy", cy},
                    {"extents_wrap", false},
                    {"antialiasing", true},
                    {"outline", false}});
    }

    json& Color(const std::string& name, unsigned abgr, int cx, int cy) {
        return Add(name, "color_source", "color_source_v3", {{"color", abgr}, {"width", cx}, {"height", cy}});
    }

    json& Media(const std::string& name, const fs::path& file, bool loop, int mixers, float volume, bool restart) {
        return Add(name, "ffmpeg_source", "ffmpeg_source",
                   {{"local_file", Fwd(file)},
                    {"is_local_file", true},
                    {"looping", loop},
                    {"restart_on_activate", restart},
                    {"close_when_inactive", true},
                    {"hw_decode", true},
                    {"clear_on_media_end", false}},
                   mixers, volume, mixers == 0);
    }

    json& Image(const std::string& name, const fs::path& file) {
        return Add(name, "image_source", "image_source", {{"file", Fwd(file)}, {"unload", false}});
    }
};

json Item(const Builder& b, const std::string& name, int id, float x, float y, float bw = 0, float bh = 0,
          int boundsType = 0, bool visible = true) {
    return {{"name", name},
            {"source_uuid", b.uuidOf.at(name)},
            {"visible", visible},
            {"locked", false},
            {"rot", 0.0},
            {"scale_ref", {{"x", W}, {"y", H}}},
            {"align", 5},
            {"bounds_type", boundsType},
            {"bounds_align", 0},
            {"bounds_crop", boundsType == 3},
            {"crop_left", 0},
            {"crop_top", 0},
            {"crop_right", 0},
            {"crop_bottom", 0},
            {"id", id},
            {"group_item_backup", false},
            {"pos", {{"x", x}, {"y", y}}},
            {"pos_rel", {{"x", (x - W / 2) / (H / 2)}, {"y", (y - H / 2) / (H / 2)}}},
            {"scale", {{"x", 1.0}, {"y", 1.0}}},
            {"scale_rel", {{"x", 1.0}, {"y", 1.0}}},
            {"bounds", {{"x", bw}, {"y", bh}}},
            {"bounds_rel", {{"x", bw / (H / 2)}, {"y", bh / (H / 2)}}},
            {"scale_filter", "disable"},
            {"blend_method", "default"},
            {"blend_type", "normal"},
            {"show_transition", {{"duration", 0}}},
            {"hide_transition", {{"duration", 0}}},
            {"private_settings", json::object()}};
}

json Filter(const std::string& name, const std::string& id, const std::string& vid, json settings) {
    return {{"name", name}, {"id", id}, {"versioned_id", vid}, {"settings", std::move(settings)}, {"enabled", true}};
}

std::vector<unsigned char> Resource(int id) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!r) return {};
    HGLOBAL h = LoadResource(nullptr, r);
    auto* p = (unsigned char*)LockResource(h);
    return {p, p + SizeofResource(nullptr, r)};
}

std::string ProcessWindow(const std::string& exe, const std::string& title, const std::string& cls) {
    return title + ":" + cls + ":" + exe;
}

std::string MusicExe(const std::string& app) {
    if (app == "spotify") return "Spotify.exe";
    if (app == "deezer") return "Deezer.exe";
    if (app == "apple-music") return "AppleMusic.exe";
    if (app == "itunes") return "iTunes.exe";
    if (app.rfind("browser:", 0) == 0) return app.substr(8);
    return {};
}

std::string MusicCredit(const fs::path& common, const char* key, const std::string& custom) {
    if (!custom.empty()) return {};
    try {
        auto j = json::parse(ReadFileText(common / L"communs.json"));
        auto m = j["music"].value(key, json::object());
        if (m.empty()) return {};
        return "Musique : " + m.value("title", "") + " - " + m.value("artist", "") + " (incompetech.com) - CC BY 4.0";
    } catch (...) {
        return {};
    }
}

}

const std::vector<SceneDef>& AllScenes() {
    static const std::vector<SceneDef> s = {
        {"demarrage", L"DÉMARRAGE", L"Écran d'attente avec la musique de démarrage", true},
        {"decompte", L"DÉCOMPTE", L"Décompte puis lancement automatique du live", true},
        {"live", L"EN LIVE", L"Le jeu en plein écran avec le bandeau Discord", false},
        {"chat", L"CHAT", L"Le jeu en plus petit pour discuter avec les viewers", true},
        {"pause", L"PAUSE", L"Écran de pause avec la musique de pause", true},
        {"technique", L"PROBLÈME TECHNIQUE", L"« On revient vite » si le jeu crash", true},
        {"fin", L"FIN DU LIVE", L"Décompte de fin puis coupure automatique du live", true},
    };
    return s;
}

std::vector<std::string> OrderedScenes() {
    std::vector<std::string> order;
    for (auto& id : Cfg().sceneOrder)
        if (std::any_of(AllScenes().begin(), AllScenes().end(), [&](auto& s) { return s.id == id; })) order.push_back(id);
    for (auto& s : AllScenes())
        if (std::find(order.begin(), order.end(), s.id) == order.end()) order.push_back(s.id);
    return order;
}

std::wstring SceneName(const std::string& id) {
    int n = 0;
    for (auto& s : OrderedScenes()) {
        if (Cfg().scenesDisabled.count(s) && s != "live") continue;
        ++n;
        if (s == id)
            for (auto& d : AllScenes())
                if (d.id == id) return std::to_wstring(n) + L" · " + d.title;
    }
    return {};
}

const std::vector<LayoutItem>& LayoutItems() {
    static const std::vector<LayoutItem> items = {
        {"chat", L"Chat", 440, 760},
        {"nowplaying", L"Musique en cours", 760, 40},
        {"rpcard", L"Carte RP", 400, 116},
        {"banner", L"Bannière Discord", 468, 60},
    };
    return items;
}

void LayoutPos(const std::string& id, float* x, float* y) {
    bool left = Cfg().chatPosition == "gauche";
    float dx = 0, dy = 0;
    if (id == "chat") dx = left ? 24 : W - 464, dy = 170;
    else if (id == "nowplaying") dx = 28, dy = H - 136;
    else if (id == "rpcard") dx = left ? W - 424 : 24, dy = 24;
    else if (id == "banner") dx = 28, dy = H - 88;
    auto& l = Cfg().layout;
    if (l.contains(id) && l[id].is_array() && l[id].size() == 2) {
        dx = l[id][0].get<float>();
        dy = l[id][1].get<float>();
    }
    *x = dx;
    *y = dy;
}

fs::path ObsDir() { return KnownFolder(FOLDERID_RoamingAppData) / L"obs-studio"; }
std::wstring CollectionName(const packs::Pack& p) {
    std::wstring n;
    for (wchar_t ch : Widen(p.name))
        if (ch < 128) n += ch;
    return L"Slayer Kit - " + n;
}
std::wstring ProfileName() { return L"Slayer Kit"; }

bool ObsRunning() { return IsProcessRunning(L"obs64.exe"); }

static void SetIni(std::string& text, const std::string& section, const std::string& key, const std::string& value) {
    const std::string nl(1, static_cast<char>(10));
    std::string head = "[" + section + "]";
    size_t sec = text.find(head);
    if (sec == std::string::npos) {
        text += nl + head + nl + key + "=" + value + nl;
        return;
    }
    size_t end = text.find(nl + "[", sec + head.size());
    if (end == std::string::npos) end = text.size();
    size_t k = text.find(nl + key + "=", sec);
    if (k != std::string::npos && k < end) {
        size_t eol = text.find(nl, k + 1);
        text.replace(k + 1, (eol == std::string::npos ? text.size() : eol) - (k + 1), key + "=" + value);
    } else {
        text.insert(end, nl + key + "=" + value);
    }
}

void SelectInObs(const packs::Pack& pack) {
    fs::path ui = ObsDir() / L"user.ini";
    std::string t = ReadFileText(ui);
    if (t.empty()) t = "[Basic]";
    SetIni(t, "Basic", "SceneCollection", Narrow(CollectionName(pack)));
    SetIni(t, "Basic", "SceneCollectionFile", "SlayerKit_" + pack.id + ".json");
    SetIni(t, "Basic", "Profile", Narrow(ProfileName()));
    SetIni(t, "Basic", "ProfileDir", "SlayerKit");
    WriteFileText(ui, t);
}

Result Apply(const packs::Pack& pack, const detect::Snapshot& , kitfiles::Policy policy) {
    Result res;
    if (ObsRunning()) {
        res.error = "Ferme OBS : la configuration s'applique quand OBS est fermé (il écrase ses fichiers en quittant).";
        return res;
    }
    auto& c = Cfg();
    json pj;
    try {
        pj = json::parse(ReadFileText(pack.localDir / L"pack.json"));
    } catch (...) {
        res.error = "Pack " + pack.name + " non installé.";
        return res;
    }
    auto assets = pj.value("assets", json::object());
    fs::path dir = pack.localDir;
    fs::path common = packs::CommonDir() / L"musiques";
    auto asset = [&](const char* key) { return dir / Widen(assets.value(key, "")); };
    fs::path bg = asset("background"), still = asset("backgroundStill"), banner = asset("bannerDiscord"),
             logo = asset("logo");
    fs::path mStart = c.musicStart.empty() ? common / L"demarrage.mp3" : fs::path(Widen(c.musicStart));
    fs::path mPause = c.musicPause.empty() ? common / L"pause.mp3" : fs::path(Widen(c.musicPause));
    fs::path mEnd = c.musicEnd.empty() ? common / L"fin.mp3" : fs::path(Widen(c.musicEnd));

    bool tiktokStudio = c.software == "tiktok-studio" || c.software == "streamlabs";
    std::wstring cableId, cableName;
    if (tiktokStudio)
        for (auto& o : audio::Outputs())
            if (o.name.find(L"CABLE Input") != std::wstring::npos) {
                cableId = o.id;
                cableName = o.name;
            }
    int monitor = !cableId.empty() ? 2 : 0;

    unsigned white = 0xFFFFFFFF;
    unsigned accent = Abgr(pack.accent), accent2 = Abgr(pack.accent2);
    int musicMixers = c.musicInVod ? 3 : 1;

    g_uuidSeed = pack.id;
    Builder b;
    b.Media("Fond animé", bg, true, 0, 1, false);
    b.Image("Fond fixe", still);
    b.Color("Voile sombre", 0x99000000, 1920, 1080);
    if (logo.extension() == L".gif" || logo.extension() == L".mp4") b.Media("Logo serveur", logo, true, 0, 1, false);
    else b.Image("Logo serveur", logo);
    b.Media("Bannière Discord", banner, true, 0, 1, false);
    b.Color("Barre basse", Abgr(pack.background, 0.85f), 1920, 92);
    b.Color("Filet", accent, 1920, 4);
    b.Text("Discord serveur", pack.discord.empty() ? pack.name : pack.discord, 34, white, 900, 92, "Bold", "left");
    b.Text("Signature", "Slayer Kit by yns_bnl", 20, 0x99FFFFFF, 400, 92, "Regular", "right");

    b.Text("Titre · Démarrage", "LE LIVE VA COMMENCER", 104, white, 1920, 140);
    b.Text("Sous-titre · Démarrage", "INSTALLE-TOI, " + pack.name + " ARRIVE", 42, accent2, 1920, 70);
    b.Text("Titre · Décompte", "LE LIVE COMMENCE DANS", 72, white, 1920, 110);
    b.Text("Décompte · chiffres", "05:00", 220, accent, 1920, 260);
    b.Text("Titre · Pause", "PETITE PAUSE", 120, white, 1920, 150);
    b.Text("Sous-titre · Pause", "ON REVIENT TRÈS VITE, RESTE LÀ", 42, accent2, 1920, 70);
    b.Text("Titre · Technique", "PROBLÈME TECHNIQUE", 110, white, 1920, 150);
    b.Text("Sous-titre · Technique", "ON RÈGLE ÇA ET ON REVIENT DANS UN INSTANT", 40, accent2, 1920, 70);
    b.Text("Titre · Fin", "MERCI D'AVOIR REGARDÉ", 100, white, 1920, 140);
    b.Text("Décompte fin · chiffres", "05:00", 150, accent, 1920, 190);
    b.Text("Sous-titre · Fin", "REJOINS LE DISCORD : " + (pack.discord.empty() ? pack.name : pack.discord), 40, accent2,
           1920, 70);
    b.Text("Titre · Chat", "DISCUSSION", 64, white, 400, 90, "Bold", "center");
    b.Text("Sous-titre · Chat", "AVEC LE CHAT", 34, accent2, 400, 60, "Bold", "center");

    std::string credStart = MusicCredit(packs::CommonDir(), "demarrage", c.musicStart);
    std::string credPause = MusicCredit(packs::CommonDir(), "pause", c.musicPause);
    std::string credEnd = MusicCredit(packs::CommonDir(), "fin", c.musicEnd);
    b.Text("Crédit · Démarrage", credStart, 20, 0xCCFFFFFF, 900, 40, "Regular", "right");
    b.Text("Crédit · Pause", credPause, 20, 0xCCFFFFFF, 900, 40, "Regular", "right");
    b.Text("Crédit · Fin", credEnd, 20, 0xCCFFFFFF, 900, 40, "Regular", "right");

    b.Media("Musique d'attente", mStart, true, musicMixers, 0.7f, true);
    b.Media("Musique de pause", mPause, true, musicMixers, 0.7f, true);
    b.Media("Musique de fin", mEnd, true, musicMixers, 0.7f, true);
    for (auto n : {"Musique d'attente", "Musique de pause", "Musique de fin"}) b.Find(n)["monitoring_type"] = monitor;

    std::string micId = c.micId.empty() ? "default" : c.micId;
    auto& mic = b.Add("Micro", "wasapi_input_capture", "wasapi_input_capture",
                      {{"device_id", micId}, {"use_device_timing", false}}, 3, 1, false);
    mic["monitoring_type"] = monitor;
    mic["filters"] = json::array({
        Filter("1 · Gain", "gain_filter", "gain_filter", {{"db", c.micGainDb}}),
        Filter("2 · Suppression de bruit", "noise_suppress_filter", "noise_suppress_filter_v2",
               {{"method", "rnnoise"}, {"suppress_level", -30}}),
        Filter("3 · Portillon", "noise_gate_filter", "noise_gate_filter",
               {{"open_threshold", c.gateOpenDb}, {"close_threshold", c.gateCloseDb}, {"attack_time", 5},
                {"hold_time", 250}, {"release_time", 150}}),
        Filter("4 · Égaliseur voix", "basic_eq_filter", "basic_eq_filter", {{"low", -4.0}, {"mid", 0.0}, {"high", 2.0}}),
        Filter("5 · Compresseur", "compressor_filter", "compressor_filter",
               {{"ratio", c.compRatio}, {"threshold", c.compThresholdDb}, {"attack_time", 6}, {"release_time", 80},
                {"output_gain", 2.0}}),
        Filter("6 · Limiteur", "limiter_filter", "limiter_filter", {{"threshold", -2.0}, {"release_time", 60}}),
    });
    if (!c.rnnoise) mic["filters"][1]["enabled"] = false;

    auto ducking = [](float threshold) {
        return Filter("Baisse quand je parle", "compressor_filter", "compressor_filter",
                      {{"ratio", 8.0}, {"threshold", threshold}, {"attack_time", 10}, {"release_time", 400},
                       {"output_gain", 0.0}, {"sidechain_source", "Micro"}});
    };

    auto& game = b.Add("Son du jeu (FiveM)", "wasapi_process_output_capture", "wasapi_process_output_capture",
                       {{"window", ProcessWindow("FiveM_GTAProcess.exe", "FiveM", "grcWindow")}, {"priority", 0}}, 3,
                       c.gameVolume, false);
    game["monitoring_type"] = monitor;
    game["filters"] = json::array({ducking(-32), Filter("Limiteur", "limiter_filter", "limiter_filter",
                                                        {{"threshold", -3.0}, {"release_time", 60}})});

    std::vector<std::string> musicSources;
    int mi = 0;
    for (auto& app : c.musicApps) {
        std::string exe = MusicExe(app);
        if (exe.empty()) continue;
        std::string name = "Musique (" + exe.substr(0, exe.size() - 4) + ")";
        auto& m = b.Add(name, "wasapi_process_output_capture", "wasapi_process_output_capture",
                        {{"window", ProcessWindow(exe, exe.substr(0, exe.size() - 4), "Chrome_WidgetWin_0")}, {"priority", 2}},
                        musicMixers, c.musicVolume, false);
        m["monitoring_type"] = monitor;
        m["filters"] = json::array({ducking(-36)});
        musicSources.push_back(name);
        ++mi;
    }

    bool discord = c.discordInLive;
    if (discord) {
        auto& d = b.Add("Discord", "wasapi_process_output_capture", "wasapi_process_output_capture",
                        {{"window", ProcessWindow("Discord.exe", "Discord", "Chrome_WidgetWin_1")}, {"priority", 2}}, 3, 0.8f,
                        false);
        d["monitoring_type"] = monitor;
    }

    b.Add("Capture FiveM", "game_capture", "game_capture",
          {{"capture_mode", "window"},
           {"window", "FiveM:grcWindow:FiveM_GTAProcess.exe"},
           {"priority", 0},
           {"capture_cursor", false},
           {"allow_transparency", false},
           {"capture_overlays", false},
           {"anti_cheat_hook", true}});

    obsws::EnableServer();
    bool chat = c.chatOverlay && (!c.twitchChannel.empty() || !c.kickChannel.empty());
    if (chat) {
        b.Add("Chat", "browser_source", "browser_source",
              {{"is_local_file", false},
               {"url", Narrow(overlays::LocalUrl("/overlay/chat"))},
               {"width", 440},
               {"height", 760},
               {"fps", 30},
               {"shutdown", false},
               {"restart_when_active", false},
               {"reroute_audio", false},
               {"css", ""}});
    }
    if (c.nowPlaying) {
        if (!fs::exists(overlays::NowPlayingFile())) WriteFileText(overlays::NowPlayingFile(), "");
        b.Add("Musique en cours", "text_gdiplus", "text_gdiplus_v3",
              {{"read_from_file", true},
               {"file", Fwd(overlays::NowPlayingFile())},
               {"font", {{"face", "Bahnschrift"}, {"style", "SemiBold"}, {"size", 26}, {"flags", 0}}},
               {"color", 0xFFFFFFFF},
               {"outline", true},
               {"outline_size", 3},
               {"outline_color", 0xC0000000},
               {"extents", true},
               {"extents_cx", 760},
               {"extents_cy", 40},
               {"align", "left"},
               {"valign", "center"}});
    }
    if (c.rpCard && !c.rpName.empty()) {
        b.Color("Carte RP · fond", Abgr(pack.background, 0.82f), 400, 116);
        b.Color("Carte RP · filet", accent, 6, 116);
        b.Text("Carte RP · nom", c.rpName, 38, white, 370, 50, "Bold", "left");
        b.Text("Carte RP · métier", c.rpJob, 24, accent2, 370, 34, "SemiBold", "left");
        b.Text("Carte RP · état", c.rpStatus, 20, 0xCCFFFFFF, 370, 30, "Regular", "left");
    }
    bool alerts = c.alertsEnabled;
    if (alerts)
        b.Add("Alertes", "browser_source", "browser_source",
              {{"is_local_file", false},
               {"url", Narrow(overlays::LocalUrl("/overlay/alerts"))},
               {"width", 1920},
               {"height", 1080},
               {"fps", 60},
               {"shutdown", false},
               {"restart_when_active", false},
               {"reroute_audio", false},
               {"css", ""}});
    auto liveOverlays = [&](json& items, int& id, bool smallScene) {
        float px, py;
        if (c.nowPlaying) {
            LayoutPos("nowplaying", &px, &py);
            items.push_back(Item(b, "Musique en cours", id++, px, py));
        }
        if (c.rpCard && !c.rpName.empty()) {
            LayoutPos("rpcard", &px, &py);
            items.push_back(Item(b, "Carte RP · fond", id++, px, py));
            items.push_back(Item(b, "Carte RP · filet", id++, px, py));
            items.push_back(Item(b, "Carte RP · nom", id++, px + 22, py + 6));
            items.push_back(Item(b, "Carte RP · métier", id++, px + 22, py + 54));
            items.push_back(Item(b, "Carte RP · état", id++, px + 22, py + 82));
        }
        if (alerts) items.push_back(Item(b, "Alertes", id++, 0, 0, W, H, 2));
        if (chat) {
            if (smallScene) items.push_back(Item(b, "Chat", id++, 1490, 600, 400, 380, 2));
            else {
                LayoutPos("chat", &px, &py);
                items.push_back(Item(b, "Chat", id++, px, py, 440, 760, 2));
            }
        }
    };

    auto waitingBase = [&](json& items, int& id, bool animated) {
        items.push_back(Item(b, animated ? "Fond animé" : "Fond fixe", id++, 0, 0, W, H, 3));
        items.push_back(Item(b, "Voile sombre", id++, 0, 0));
    };
    auto bottomBar = [&](json& items, int& id, const std::string& credit) {
        items.push_back(Item(b, "Barre basse", id++, 0, H - 92));
        items.push_back(Item(b, "Filet", id++, 0, H - 96));
        items.push_back(Item(b, "Discord serveur", id++, 40, H - 92));
        if (!credit.empty()) items.push_back(Item(b, credit, id++, W - 940, H - 136));
        items.push_back(Item(b, "Signature", id++, W - 440, H - 92));
    };
    auto audioCommon = [&](json& items, int& id, bool full) {
        items.push_back(Item(b, "Micro", id++, 0, 0));
        if (discord) items.push_back(Item(b, "Discord", id++, 0, 0));
        if (full) {
            items.push_back(Item(b, "Son du jeu (FiveM)", id++, 0, 0));
            for (auto& m : musicSources) items.push_back(Item(b, m, id++, 0, 0));
        }
    };

    json scenes = json::array();
    json order = json::array();
    int key = 1;
    std::string firstScene;
    for (auto& sid : OrderedScenes()) {
        if (c.scenesDisabled.count(sid) && sid != "live") continue;
        std::string name = Narrow(SceneName(sid));
        if (firstScene.empty()) firstScene = name;
        json items = json::array();
        int id = 1;
        if (sid == "demarrage") {
            waitingBase(items, id, true);
            items.push_back(Item(b, "Musique d'attente", id++, 0, 0));
            items.push_back(Item(b, "Logo serveur", id++, W / 2 - 140, 150, 280, 280, 2));
            items.push_back(Item(b, "Titre · Démarrage", id++, 0, 470));
            items.push_back(Item(b, "Sous-titre · Démarrage", id++, 0, 610));
            bottomBar(items, id, "Crédit · Démarrage");
            audioCommon(items, id, false);
        } else if (sid == "decompte") {
            waitingBase(items, id, true);
            items.push_back(Item(b, "Musique d'attente", id++, 0, 0));
            items.push_back(Item(b, "Logo serveur", id++, W / 2 - 110, 90, 220, 220, 2));
            items.push_back(Item(b, "Titre · Décompte", id++, 0, 340));
            items.push_back(Item(b, "Décompte · chiffres", id++, 0, 450));
            bottomBar(items, id, "Crédit · Démarrage");
            audioCommon(items, id, false);
        } else if (sid == "live") {
            items.push_back(Item(b, "Capture FiveM", id++, 0, 0, W, H, 2));
            {
                float bx, by;
                LayoutPos("banner", &bx, &by);
                items.push_back(Item(b, "Bannière Discord", id++, bx, by, 468, 60, 2));
            }
            liveOverlays(items, id, false);
            audioCommon(items, id, true);
        } else if (sid == "chat") {
            waitingBase(items, id, false);
            items.push_back(Item(b, "Capture FiveM", id++, 40, 135, 1440, 810, 2));
            items.push_back(Item(b, "Logo serveur", id++, 1560, 170, 240, 240, 2));
            items.push_back(Item(b, "Titre · Chat", id++, 1480, 450));
            items.push_back(Item(b, "Sous-titre · Chat", id++, 1480, 530));
            liveOverlays(items, id, true);
            bottomBar(items, id, "");
            audioCommon(items, id, true);
        } else if (sid == "pause") {
            waitingBase(items, id, true);
            items.push_back(Item(b, "Musique de pause", id++, 0, 0));
            items.push_back(Item(b, "Logo serveur", id++, W / 2 - 140, 150, 280, 280, 2));
            items.push_back(Item(b, "Titre · Pause", id++, 0, 470));
            items.push_back(Item(b, "Sous-titre · Pause", id++, 0, 620));
            bottomBar(items, id, "Crédit · Pause");
            audioCommon(items, id, false);
        } else if (sid == "technique") {
            waitingBase(items, id, false);
            items.push_back(Item(b, "Musique de pause", id++, 0, 0));
            items.push_back(Item(b, "Logo serveur", id++, W / 2 - 120, 170, 240, 240, 2));
            items.push_back(Item(b, "Titre · Technique", id++, 0, 460));
            items.push_back(Item(b, "Sous-titre · Technique", id++, 0, 610));
            bottomBar(items, id, "Crédit · Pause");
            audioCommon(items, id, false);
        } else if (sid == "fin") {
            waitingBase(items, id, false);
            items.push_back(Item(b, "Musique de fin", id++, 0, 0));
            items.push_back(Item(b, "Logo serveur", id++, W / 2 - 120, 90, 240, 240, 2));
            items.push_back(Item(b, "Titre · Fin", id++, 0, 360));
            items.push_back(Item(b, "Décompte fin · chiffres", id++, 0, 500));
            items.push_back(Item(b, "Sous-titre · Fin", id++, 0, 720));
            bottomBar(items, id, "Crédit · Fin");
            audioCommon(items, id, false);
        }
        auto& sc = b.Add(name, "scene", "scene", {{"id_counter", id}, {"custom_size", false}, {"items", items}});
        if (key <= 9)
            sc["hotkeys"] = {{"OBSBasic.SelectScene",
                              json::array({{{"control", true}, {"alt", true}, {"key", "OBS_KEY_" + std::to_string(key)}}})}};
        ++key;
        order.push_back({{"name", name}});
    }

    fs::path obs = ObsDir();
    fs::path scriptsDir = obs / L"scripts" / L"slayerkit";
    std::string decompteScene = Narrow(SceneName("decompte")), liveScene = Narrow(SceneName("live")),
                finScene = Narrow(SceneName("fin"));

    std::map<std::string, std::string> transitionNames = {{"fondu", "Fondu"}, {"balayage", "Balayage"}, {"glissement", "Glissement"}};
    std::string curTransition = transitionNames.count(c.transition) ? transitionNames[c.transition] : "Balayage";

    json collection = {
        {"name", Narrow(CollectionName(pack))},
        {"current_scene", firstScene},
        {"current_program_scene", firstScene},
        {"scene_order", order},
        {"sources", b.sources},
        {"groups", json::array()},
        {"transitions",
         json::array({{{"name", "Balayage"},
                       {"id", "wipe_transition"},
                       {"settings", {{"luma_image", "C:/Program Files/obs-studio/data/obs-plugins/obs-transitions/luma_wipes/burst.png"},
                                     {"luma_softness", 0.05},
                                     {"luma_invert", false}}}},
                      {{"name", "Glissement"}, {"id", "slide_transition"}, {"settings", {{"direction", 0}}}}})},
        {"current_transition", curTransition},
        {"transition_duration", 600},
        {"quick_transitions", json::array()},
        {"saved_projectors", json::array()},
        {"preview_locked", false},
        {"scaling_enabled", false},
        {"virtual-camera", {{"type2", 3}}},
        {"modules",
         {{"scripts-tool",
           json::array({{{"path", Fwd(scriptsDir / L"decompte-live.lua")},
                         {"settings",
                          {{"scene_decompte", decompteScene},
                           {"scene_live", liveScene},
                           {"source_texte", "Décompte · chiffres"},
                           {"source_musique", "Musique d'attente"},
                           {"duree_min", c.countdownStartMin},
                           {"fondu_sec", 6},
                           {"lancer_stream", true}}}},
                        {{"path", Fwd(scriptsDir / L"fin-de-live.lua")},
                         {"settings",
                          {{"scene_cible", finScene},
                           {"source_texte", "Décompte fin · chiffres"},
                           {"source_musique", "Musique de fin"},
                           {"duree_min", c.countdownEndMin},
                           {"fondu_sec", 6},
                           {"texte_fin", "MERCI ET À BIENTÔT"},
                           {"couper_stream", true},
                           {"couper_rec", true}}}},
                        {{"path", Fwd(scriptsDir / L"slayerkit-events.lua")},
                         {"settings", {{"fichier", Fwd(live::StateFile())}, {"tampon", c.replayBuffer}}}}})}}},
        {"version", 2},
    };

    auto install = [&](const fs::path& p, const std::string& data, const std::wstring& label) {
        bool merged = false;
        if (!kitfiles::Install(p, data, label, policy, &merged)) return false;
        if (merged) res.keptUserFiles.push_back(label);
        return true;
    };
    auto res2s = [](const std::vector<unsigned char>& v) { return std::string(v.begin(), v.end()); };
    std::string colText = collection.dump(4);
    std::string fileStem = "SlayerKit_" + pack.id;
    if (!install(obs / L"basic" / L"scenes" / Widen(fileStem + ".json"), colText,
                 L"Scènes OBS · " + Widen(pack.name)) ||
        !install(scriptsDir / L"decompte-live.lua", res2s(Resource(20)), L"Script décompte de démarrage") ||
        !install(scriptsDir / L"fin-de-live.lua", res2s(Resource(21)), L"Script décompte de fin") ||
        !install(scriptsDir / L"slayerkit-events.lua", res2s(Resource(22)), L"Script suivi du live")) {
        res.error = "Écriture des fichiers OBS impossible.";
        return res;
    }

    fs::path profile = obs / L"basic" / L"profiles" / L"SlayerKit";
    std::error_code ec;
    fs::create_directories(profile, ec);
    std::string enc = Encoder();
    std::string ini =
        "[General]\nName=Slayer Kit\n\n"
        "[Output]\nMode=Advanced\nRecType=Standard\n\n"
        "[AdvOut]\nTrackIndex=1\nVodTrackIndex=2\nVodTrackEnabled=true\nEncoder=" + enc +
        "\nAudioEncoder=ffmpeg_aac\nRecType=Standard\nRecFilePath=" + Fwd(KnownFolder(FOLDERID_Videos)) +
        "\nRecFormat2=hybrid_mp4\nRecTracks=3\nRecEncoder=none\nRecAudioEncoder=ffmpeg_aac\nTrack1Bitrate=192\nTrack2Bitrate=192\n"
        "Track1Name=Live (tout)\nTrack2Name=Redif (sans musique)\n" +
        std::string(c.replayBuffer ? "RecRB=true\nRecRBTime=" + std::to_string(c.replaySeconds) + "\nRecRBSize=1024\n" : "RecRB=false\n") +
        "\n[SimpleOutput]\nFilePath=" + Fwd(KnownFolder(FOLDERID_Videos)) + "\nRecFormat2=hybrid_mp4\nVBitrate=6000\n"
        "StreamEncoder=" + std::string(enc == "obs_x264" ? "x264" : enc == "obs_nvenc_h264_tex" ? "nvenc" : enc == "h264_texture_amf" ? "amd" : "qsv") +
        "\nABitrate=192\nRecQuality=Stream\n\n"
        "[Video]\nBaseCX=1920\nBaseCY=1080\nOutputCX=1920\nOutputCY=1080\nFPSType=0\nFPSCommon=60\nScaleType=lanczos\n"
        "ColorFormat=NV12\nColorSpace=709\nColorRange=Partial\n\n"
        "[Audio]\nSampleRate=48000\nChannelSetup=Stereo\nMonitoringDeviceId=" +
        (cableId.empty() ? std::string("default") : Narrow(cableId)) + "\nMonitoringDeviceName=" +
        (cableName.empty() ? std::string("Par défaut") : Narrow(cableName)) +
        "\nDesktopAudioDevice1=disabled\nDesktopAudioDevice2=disabled\nAuxAudioDevice1=disabled\n"
        "AuxAudioDevice2=disabled\nAuxAudioDevice3=disabled\nAuxAudioDevice4=disabled\n\n"
        "[Hotkeys]\nReplayBuffer={\"ReplayBuffer.Save\":[{\"control\":true,\"alt\":true,\"key\":\"OBS_KEY_C\"}]}\n";
    if (!install(profile / L"basic.ini", ini, L"Profil OBS Slayer Kit")) {
        res.error = "Écriture du profil OBS impossible.";
        return res;
    }

    auto keyFor = [&](const std::string& pf) {
        if (pf == "twitch") return Settings::Open(c.twitchKeySealed);
        if (pf == "youtube") return Settings::Open(c.youtubeKeySealed);
        if (pf == "kick") return Settings::Open(c.kickKeySealed);
        if (pf == "tiktok") return Settings::Open(c.tiktokKeySealed);
        return std::string();
    };
    std::string mainKey = keyFor(c.mainPlatform);
    if (!mainKey.empty()) {
        json svc;
        if (c.mainPlatform == "twitch")
            svc = {{"type", "rtmp_common"}, {"settings", {{"service", "Twitch"}, {"server", "auto"}, {"key", mainKey}, {"bwtest", false}}}};
        else if (c.mainPlatform == "youtube")
            svc = {{"type", "rtmp_common"},
                   {"settings", {{"service", "YouTube - RTMPS"}, {"server", "rtmps://a.rtmps.youtube.com:443/live2"}, {"key", mainKey}}}};
        else if (c.mainPlatform == "kick")
            svc = {{"type", "rtmp_custom"},
                   {"settings", {{"server", "rtmps://fa723fc1b171.global-contribute.live-video.net:443/app/"}, {"key", mainKey}, {"use_auth", false}}}};
        else
            svc = {{"type", "rtmp_custom"}, {"settings", {{"server", c.tiktokServer}, {"key", mainKey}, {"use_auth", false}}}};
        std::string svcText = svc.dump(4);
        WriteFileBytes(profile / L"service.json", {svcText.begin(), svcText.end()});
    }
    if (!fs::exists(profile / L"service.json", ec)) {
        fs::path best;
        fs::file_time_type newest{};
        for (auto& d : fs::directory_iterator(obs / L"basic" / L"profiles", ec)) {
            fs::path s = d.path() / L"service.json";
            if (d.path() == profile || !fs::exists(s, ec)) continue;
            auto t = fs::last_write_time(s, ec);
            if (best.empty() || t > newest) {
                best = s;
                newest = t;
            }
        }
        if (!best.empty()) fs::copy_file(best, profile / L"service.json", ec);
    }

    SelectInObs(pack);
    c.kitApplied = true;
    c.appliedPack = pack.id;
    SaveSettings();
    res.ok = true;
    res.collection = CollectionName(pack);
    Log("Kit OBS applique : " + pack.id + " (encodeur " + enc + ")");
    return res;
}

bool LaunchObs(const packs::Pack& pack, const detect::Snapshot& snap, bool startStreaming) {
    const detect::App* obs = detect::Find(snap.streaming, "obs");
    if (!obs || obs->exe.empty()) return false;
    if (!ObsRunning()) {
        std::error_code ec;
        for (auto& f : fs::directory_iterator(ObsDir() / L".sentinel", ec)) fs::remove(f.path(), ec);
    }
    if (!ObsRunning()) SelectInObs(pack);
    std::wstring args = L"--disable-shutdown-check";
    if (Cfg().software == "tiktok-studio" || Cfg().software == "streamlabs") args += L" --startvirtualcam";
    if (startStreaming) args += L" --startstreaming";
    std::wstring dir = obs->exe.parent_path().wstring();
    HINSTANCE h = ShellExecuteW(nullptr, L"open", obs->exe.c_str(), args.c_str(), dir.c_str(), SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
}

}
