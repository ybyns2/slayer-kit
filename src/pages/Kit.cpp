#include <shobjidl.h>

#include <algorithm>
#include <cmath>
#include <thread>

#include "app/App.h"
#include "core/Config.h"

namespace sk {

using namespace ui;

namespace {

std::wstring Db(float v) {
    wchar_t b[32];
    swprintf_s(b, L"%+.1f dB", v);
    return b;
}

const wchar_t* kPhaseText[] = {L"Ne dis rien pendant 5 secondes : je mesure le bruit de fond.",
                               L"Parle normalement, comme en RP, pendant 6 secondes.",
                               L"Maintenant crie comme dans une scène tendue, 4 secondes."};
const float kPhaseSeconds[] = {5, 6, 4};

}

std::wstring App::PickAudioFile() {
    std::wstring out;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
    COMDLG_FILTERSPEC spec[] = {{L"Musiques", L"*.mp3;*.ogg;*.wav;*.flac;*.m4a"}};
    dlg->SetFileTypes(1, spec);
    dlg->SetTitle(L"Choisis ta musique");
    if (SUCCEEDED(dlg->Show(hwnd_))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) out = path;
            CoTaskMemFree(path);
            item->Release();
        }
    }
    dlg->Release();
    return out;
}

void App::StartMeter() {
    std::string dev = Cfg().micId;
    if (meterRun_ && meterDevice_ == dev) return;
    meterRun_ = false;
    meterDevice_ = dev;
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    meterRun_ = true;
    Async([this, dev] {
        while (meterRun_) {
            meterLevel_ = audio::PeakLevel(Widen(dev));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
}

void App::StartMicTest() {
    micCancel_ = false;
    micPhase_ = 0;
    micHasResult_ = false;
    std::wstring dev = Widen(Cfg().micId);
    Async([this, dev] {
        audio::Phase ph[3];
        for (int i = 0; i < 3; ++i) {
            Post([this, i] {
                micPhase_ = i;
                micPhaseProgress_ = 0;
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            bool ok = audio::Record(dev, kPhaseSeconds[i], &ph[i],
                                    [this](int, float p, float lvl) {
                                        Post([this, p, lvl] {
                                            micPhaseProgress_ = p;
                                            micLive_ = lvl;
                                        });
                                    },
                                    i, micCancel_);
            if (!ok) {
                Post([this] {
                    micPhase_ = -1;
                    if (!micCancel_) Toast(L"Impossible d'écouter ce micro.", false);
                });
                return;
            }
        }
        auto r = audio::Analyse(ph[0], ph[1], ph[2]);
        Post([this, r] {
            micPhase_ = -1;
            micResult_ = r;
            micHasResult_ = true;
            if (!r.ok) {
                Toast(Widen(r.error), false);
                return;
            }
            auto& c = Cfg();
            c.micTested = true;
            c.micGainDb = r.gainDb;
            c.gateOpenDb = r.gateOpenDb;
            c.gateCloseDb = r.gateCloseDb;
            c.rnnoise = r.rnnoise;
            c.compThresholdDb = r.compThresholdDb;
            c.compRatio = r.compRatio;
            c.micQuality = r.quality;
            c.musicVolume = r.quality == 0 ? 0.15f : r.quality == 1 ? 0.2f : 0.25f;
            SaveSettings();
            Toast(L"Micro réglé. Pense à appliquer le kit à OBS.");
        });
    });
}

void App::DashAudio(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Audio & micro", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Le kit capte ton micro, le jeu, la musique et Discord séparément pour un mix parfait.",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);
    if (mics_.empty()) {
        mics_ = audio::Inputs();
        if (c.micId.empty() && !mics_.empty()) {
            c.micId = Narrow(mics_.front().id);
            c.micName = Narrow(mics_.front().name);
            SaveSettings();
        }
    }
    StartMeter();

    float colW = (a.w - 20) / 2;
    Rect left{a.x, a.y + 82, colW, a.h - 82};
    ui_.Panel(left, 16);
    gfx_.Text(L"VOTRE MICRO PRINCIPAL, C'EST LEQUEL ?", {left.x + 22, left.y + 18, left.w - 80, 20}, Font::Caption, t.faint);
    if (ui_.IconButton("mic-refresh", {left.r() - 50, left.y + 10, 32, 32}, L"", Color{1, 1, 1, 0.08f}, t.muted))
        mics_.clear();
    Rect list{left.x + 14, left.y + 46, left.w - 28, 4 * 52.f};
    float sc = ui_.Scroll("mic-scroll", list, 52.f * mics_.size());
    gfx_.PushClip(list);
    float y = list.y - sc;
    for (size_t i = 0; i < mics_.size(); ++i) {
        auto& m = mics_[i];
        bool sel = Narrow(m.id) == c.micId;
        Rect row{list.x, y + 3, list.w - 10, 46};
        if (row.b() > list.y && row.y < list.b()) {
            if (ui_.SelectCard("mic" + std::to_string(i), row, sel, t.accent) && ui_.Hover(list)) {
                c.micId = Narrow(m.id);
                c.micName = Narrow(m.name);
                c.micTested = false;
                micHasResult_ = false;
                SaveSettings();
            }
            gfx_.Text(m.name + (m.isDefault ? L"  ·  par défaut" : L""), {row.x + 16, row.y, row.w - 60, row.h}, Font::Small,
                      t.text);
        }
        y += 52;
    }
    gfx_.PopClip();

    float ty = left.y + 272;
    gfx_.Line(left.x + 22, ty - 12, left.r() - 22, ty - 12, t.line);
    gfx_.Text(L"NIVEAU EN DIRECT", {left.x + 22, ty, 200, 20}, Font::Caption, t.faint);
    ui_.Meter({left.x + 22, ty + 28, left.w - 44, 10}, micPhase_ >= 0 ? std::pow(10.f, micLive_ / 20.f) : meterLevel_.load());
    ty += 56;
    if (micPhase_ >= 0) {
        gfx_.Text(L"Étape " + std::to_wstring(micPhase_ + 1) + L" / 3", {left.x + 22, ty, 200, 24}, Font::BodyBold,
                  t.accent.lighten(0.3f));
        gfx_.Text(kPhaseText[micPhase_], {left.x + 22, ty + 26, left.w - 44, 44}, Font::Body, t.text, Align::Left, false, true);
        ui_.Progress({left.x + 22, ty + 80, left.w - 44, 6}, micPhaseProgress_);
        if (ui_.Button("mic-cancel", {left.x + 22, ty + 100, 120, 34}, L"Annuler", Style::Ghost)) micCancel_ = true;
    } else {
        if (ui_.Button("mic-test", {left.x + 22, ty, 240, 44}, c.micTested ? L"Refaire le test micro" : L"Lancer le test micro",
                       Style::Primary, !c.micId.empty(), L""))
            StartMicTest();
        if (micHasResult_ && micResult_.ok) {
            gfx_.Text(micResult_.verdict, {left.x + 22, ty + 56, left.w - 44, 60}, Font::Small, t.muted, Align::Left, false, true);
        } else if (micHasResult_) {
            gfx_.Text(Widen(micResult_.error) + L" Relance le test et suis les 3 étapes.", {left.x + 22, ty + 56, left.w - 44, 60},
                      Font::Small, t.warn, Align::Left, false, true);
        } else if (c.micTested) {
            gfx_.Text(L"Micro déjà testé et réglé.", {left.x + 22, ty + 56, left.w - 44, 24}, Font::Small, t.ok);
        }
    }

    Rect right{a.x + colW + 20, a.y + 82, colW, a.h - 82};
    ui_.Panel(right, 16);
    gfx_.Text(L"RÉGLAGES APPLIQUÉS", {right.x + 22, right.y + 18, 300, 20}, Font::Caption, t.faint);
    struct V { const wchar_t* label; std::wstring value; };
    V vals[] = {{L"Gain du micro", Db(c.micGainDb)},
                {L"Suppression de bruit", c.rnnoise ? L"Activée (RNNoise)" : L"Désactivée"},
                {L"Portillon", Db(c.gateOpenDb) + L" / " + Db(c.gateCloseDb)},
                {L"Compresseur", Db(c.compThresholdDb) + L", ratio " + std::to_wstring((int)c.compRatio) + L":1"}};
    float ry = right.y + 46;
    for (auto& v : vals) {
        gfx_.Text(v.label, {right.x + 22, ry, 200, 26}, Font::Small, t.muted);
        gfx_.Text(v.value, {right.x + 220, ry, right.w - 240, 26}, Font::SmallBold, t.text, Align::Right);
        ry += 28;
    }
    ry += 10;
    gfx_.Line(right.x + 22, ry, right.r() - 22, ry, t.line);
    ry += 14;
    auto toggleRow = [&](const char* id, const wchar_t* title, const wchar_t* text, bool& value) {
        gfx_.Text(title, {right.x + 22, ry, right.w - 110, 24}, Font::BodyBold, t.text);
        gfx_.Text(text, {right.x + 22, ry + 24, right.w - 110, 36}, Font::Small, t.muted, Align::Left, false, true);
        if (ui_.Toggle(id, {right.r() - 70, ry + 6, 50, 26}, value)) SaveSettings();
        ry += 66;
    };
    toggleRow("au-discord", L"Discord dans le live", L"Tes potes en vocal s'entendent en live et en redif.", c.discordInLive);
    bool vodClean = !c.musicInVod;
    gfx_.Text(L"Redif Twitch sans musique", {right.x + 22, ry, right.w - 110, 24}, Font::BodyBold, t.text);
    gfx_.Text(L"Les viewers entendent la musique en direct, la redif ne l'a pas (évite les mutes).",
              {right.x + 22, ry + 24, right.w - 110, 36}, Font::Small, t.muted, Align::Left, false, true);
    if (ui_.Toggle("au-vod", {right.r() - 70, ry + 6, 50, 26}, vodClean)) {
        c.musicInVod = !vodClean;
        SaveSettings();
    }
    ry += 70;
    gfx_.Text(L"Volume du jeu", {right.x + 22, ry, 160, 24}, Font::Small, t.muted);
    if (ui_.Slider("au-game", {right.x + 180, ry, right.w - 270, 24}, c.gameVolume, 0.05f, 1.f)) SaveSettings();
    gfx_.Text(std::to_wstring((int)std::lround(c.gameVolume * 100)) + L" %", {right.r() - 80, ry, 58, 24}, Font::SmallBold,
              t.text, Align::Right);
    ry += 34;
    gfx_.Text(L"Volume de la musique", {right.x + 22, ry, 160, 24}, Font::Small, t.muted);
    if (ui_.Slider("au-music", {right.x + 180, ry, right.w - 270, 24}, c.musicVolume, 0.05f, 1.f)) SaveSettings();
    gfx_.Text(std::to_wstring((int)std::lround(c.musicVolume * 100)) + L" %", {right.r() - 80, ry, 58, 24}, Font::SmallBold,
              t.text, Align::Right);
    ry += 40;
    if (ui_.Button("au-reset", {right.x + 22, std::min(ry, right.b() - 50), 260, 38}, L"Remettre l'audio du kit par défaut",
                   Style::Ghost, true, L"")) {
        c.micGainDb = 0;
        c.gateOpenDb = -45;
        c.gateCloseDb = -53;
        c.rnnoise = true;
        c.compThresholdDb = -18;
        c.compRatio = 4;
        c.gameVolume = 0.35f;
        c.musicVolume = 0.25f;
        c.musicInVod = false;
        c.micTested = false;
        SaveSettings();
        Toast(L"Audio remis par défaut.");
    }
}

void App::DashMusic(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Musique", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Où écoutez-vous de la musique ? Coche les applis à capter en live.", {a.x, a.y + 40, a.w, 24}, Font::Body,
              t.muted);
    auto has = [&](const std::string& id) { return std::find(c.musicApps.begin(), c.musicApps.end(), id) != c.musicApps.end(); };
    auto toggle = [&](const std::string& id) {
        if (has(id)) c.musicApps.erase(std::remove(c.musicApps.begin(), c.musicApps.end(), id), c.musicApps.end());
        else c.musicApps.push_back(id);
        SaveSettings();
    };

    float y = a.y + 82;
    bool any = false;
    float cw = (a.w - 3 * 14) / 4;
    int col = 0;
    for (auto& m : snap_.music) {
        if (!m.installed) continue;
        any = true;
        Rect r{a.x + col * (cw + 14), y, cw, 84};
        if (ui_.SelectCard("mu-" + m.id, r, has(m.id), t.accent)) toggle(m.id);
        gfx_.Text(m.name, {r.x + 18, r.y + 16, r.w - 56, 26}, Font::BodyBold, t.text);
        gfx_.Text(m.running ? L"Ouvert" : L"Installé", {r.x + 18, r.y + 44, r.w - 36, 22}, Font::Small, m.running ? t.ok : t.muted);
        if (++col == 4) {
            col = 0;
            y += 98;
        }
    }
    for (auto& s : snap_.musicSites) {
        std::string id = "browser:" + Narrow(s.browser);
        if (col == 4) {
            col = 0;
            y += 98;
        }
        any = true;
        Rect r{a.x + col * (cw + 14), y, cw, 84};
        if (ui_.SelectCard("mu-" + id + Narrow(s.site), r, has(id), t.accent2)) toggle(id);
        gfx_.Text(s.site, {r.x + 18, r.y + 16, r.w - 56, 26}, Font::BodyBold, t.text);
        gfx_.Text(L"Dans " + s.browser, {r.x + 18, r.y + 44, r.w - 36, 22}, Font::Small, t.muted);
        ++col;
    }
    if (!any) {
        Rect r{a.x, y, std::min(a.w, 820.f), 120};
        ui_.Panel(r, 16);
        gfx_.Text(L"Je n'ai détecté aucune application Deezer ou Spotify. Vous voulez laquelle ?", {r.x + 22, r.y + 16, r.w - 44, 26},
                  Font::BodyBold, t.text);
        struct I { const char* id; const char* winget; const wchar_t* name; };
        I apps[] = {{"spotify", "Spotify.Spotify", L"Installer Spotify"}, {"deezer", "Deezer.Deezer", L"Installer Deezer"}};
        float bx = r.x + 22;
        for (auto& app : apps) {
            if (ui_.Button(std::string("mu-inst-") + app.id, {bx, r.y + 60, 200, 42}, app.name, Style::Primary, !installingApp_,
                           L"")) {
                installingApp_ = true;
                std::string id = app.id, wid = app.winget;
                Toast(L"Installation en cours…");
                Async([this, id, wid] {
                    bool ok = detect::WingetInstall(wid);
                    Post([this, id, ok] {
                        installingApp_ = false;
                        if (ok) {
                            Cfg().musicApps.push_back(id);
                            SaveSettings();
                        }
                        Toast(ok ? L"Installé et ajouté au kit." : L"Installation impossible (winget).", ok);
                        Refresh();
                    });
                });
            }
            bx += 214;
        }
        if (installingApp_) ui_.Spinner(bx + 20, r.y + 81, 10);
        y += 134;
    } else {
        y += col ? 98 : 0;
    }

    y += 14;
    gfx_.Text(L"LES 3 MUSIQUES DES ÉCRANS D'ATTENTE", {a.x, y, a.w, 20}, Font::Caption, t.faint);
    y += 28;
    struct Slot { const char* id; const wchar_t* title; std::string* value; const wchar_t* def; };
    Slot slots[] = {{"start", L"Démarrage & décompte", &c.musicStart, L"Volatile Reaction · Kevin MacLeod (CC BY 4.0)"},
                    {"pause", L"Pause & problème technique", &c.musicPause, L"Lobby Time · Kevin MacLeod (CC BY 4.0)"},
                    {"end", L"Fin du live", &c.musicEnd, L"Cool Vibes · Kevin MacLeod (CC BY 4.0)"}};
    for (auto& s : slots) {
        Rect r{a.x, y, std::min(a.w, 900.f), 64};
        ui_.Panel(r, 14);
        gfx_.Text(L"", {r.x + 18, r.y, 24, r.h}, Font::Icon, t.accent.lighten(0.2f));
        gfx_.Text(s.title, {r.x + 52, r.y + 10, 300, 24}, Font::BodyBold, t.text);
        gfx_.Text(s.value->empty() ? s.def : fs::path(Widen(*s.value)).filename().wstring(), {r.x + 52, r.y + 34, r.w - 360, 22},
                  Font::Small, t.muted);
        if (ui_.Button(std::string("ms-pick-") + s.id, {r.r() - 300, r.y + 14, 140, 36}, L"Changer", Style::Secondary, true,
                       L"")) {
            std::wstring f = PickAudioFile();
            if (!f.empty()) {
                *s.value = Narrow(f);
                SaveSettings();
            }
        }
        if (ui_.Button(std::string("ms-def-") + s.id, {r.r() - 150, r.y + 14, 130, 36}, L"Par défaut", Style::Ghost,
                       !s.value->empty())) {
            s.value->clear();
            SaveSettings();
        }
        y += 74;
    }
}

void App::DashScenes(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Scènes & transitions", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Active, retire et range tes scènes. Raccourcis : Ctrl + Alt + numéro de la scène.", {a.x, a.y + 40, a.w, 24},
              Font::Body, t.muted);
    auto order = obskit::OrderedScenes();
    float colW = std::min(a.w * 0.58f, 620.f);
    float y = a.y + 82;
    for (size_t i = 0; i < order.size(); ++i) {
        auto& id = order[i];
        auto def = std::find_if(obskit::AllScenes().begin(), obskit::AllScenes().end(), [&](auto& s) { return s.id == id; });
        bool enabled = !c.scenesDisabled.count(id);
        Rect r{a.x, y, colW, 58};
        ui_.Panel(r, 12);
        std::wstring name = enabled ? obskit::SceneName(id) : L"— · " + def->title;
        gfx_.Text(name, {r.x + 18, r.y + 6, r.w - 220, 26}, Font::BodyBold, enabled ? t.text : t.faint);
        gfx_.Text(def->hint, {r.x + 18, r.y + 30, r.w - 220, 22}, Font::Small, t.muted);
        if (ui_.IconButton("sc-up" + id, {r.r() - 170, r.y + 13, 32, 32}, L"", Color{1, 1, 1, 0.08f}, t.muted) && i > 0) {
            std::swap(order[i], order[i - 1]);
            c.sceneOrder = order;
            SaveSettings();
        }
        if (ui_.IconButton("sc-dn" + id, {r.r() - 132, r.y + 13, 32, 32}, L"", Color{1, 1, 1, 0.08f}, t.muted) &&
            i + 1 < order.size()) {
            std::swap(order[i], order[i + 1]);
            c.sceneOrder = order;
            SaveSettings();
        }
        if (def->canDisable) {
            if (ui_.Toggle("sc-on" + id, {r.r() - 76, r.y + 16, 50, 26}, enabled)) {
                if (enabled) c.scenesDisabled.erase(id);
                else c.scenesDisabled.insert(id);
                SaveSettings();
            }
        } else {
            ui_.Badge({r.r() - 96, r.y + 18, 76, 22}, L"TOUJOURS", t.accent2);
        }
        y += 66;
    }

    Rect side{a.x + colW + 20, a.y + 82, a.w - colW - 20, a.h - 82};
    ui_.Panel(side, 16);
    float sy = side.y + 18;
    gfx_.Text(L"TRANSITION", {side.x + 22, sy, 200, 20}, Font::Caption, t.faint);
    sy += 28;
    struct Tr { const char* id; const wchar_t* label; };
    Tr trs[] = {{"balayage", L"Balayage"}, {"fondu", L"Fondu"}, {"glissement", L"Glissement"}};
    float bw = (side.w - 44 - 16) / 3;
    for (int i = 0; i < 3; ++i) {
        if (ui_.Button(std::string("tr-") + trs[i].id, {side.x + 22 + i * (bw + 8), sy, bw, 36}, trs[i].label,
                       c.transition == trs[i].id ? Style::Primary : Style::Secondary)) {
            c.transition = trs[i].id;
            SaveSettings();
        }
    }
    sy += 56;
    auto stepper = [&](const char* id, const wchar_t* label, int& value) {
        gfx_.Text(label, {side.x + 22, sy, side.w - 160, 32}, Font::Small, t.text);
        if (ui_.Button(std::string(id) + "-", {side.r() - 140, sy, 32, 32}, L"−", Style::Secondary) && value > 1) {
            --value;
            SaveSettings();
        }
        gfx_.Text(std::to_wstring(value) + L" min", {side.r() - 104, sy, 64, 32}, Font::BodyBold, t.text, Align::Center);
        if (ui_.Button(std::string(id) + "+", {side.r() - 54, sy, 32, 32}, L"+", Style::Secondary) && value < 30) {
            ++value;
            SaveSettings();
        }
        sy += 44;
    };
    stepper("cd-start", L"Décompte de démarrage", c.countdownStartMin);
    stepper("cd-end", L"Décompte de fin", c.countdownEndMin);
    sy += 10;
    gfx_.Line(side.x + 22, sy, side.r() - 22, sy, t.line);
    sy += 18;
    gfx_.Text(c.kitApplied ? L"Kit appliqué à OBS. Réapplique après tes changements." : L"Le kit n'est pas encore appliqué à OBS.",
              {side.x + 22, sy, side.w - 44, 40}, Font::Small, c.kitApplied ? t.ok : t.warn, Align::Left, false, true);
    sy += 48;
    if (ui_.Button("kit-apply", {side.x + 22, sy, side.w - 44, 46}, L"Appliquer à OBS", Style::Primary, !kitBusy_, L""))
        ApplyKit(false);
    sy += 56;
    if (ui_.Button("kit-open", {side.x + 22, sy, side.w - 44, 40}, L"Ouvrir OBS avec le kit", Style::Secondary, !kitBusy_,
                   L""))
        ApplyKit(true);
    if (!kitStatus_.empty())
        gfx_.Text(kitStatus_, {side.x + 22, sy + 50, side.w - 44, 60}, Font::Small, t.muted, Align::Left, false, true);
}

void App::ApplyKit(bool launchAfter, bool startStreaming) {
    const packs::Pack* p = ActivePack();
    if (!p || !p->installed) {
        Toast(L"Installe d'abord un pack serveur (Serveurs).", false);
        section_ = Section::Servers;
        return;
    }
    auto run = [this, launchAfter, startStreaming](kitfiles::Policy policy) {
        kitBusy_ = true;
        kitStatus_ = L"Préparation…";
        packs::Pack pack = *ActivePack();
        auto snap = snap_;
        auto cat = catalog_;
        Async([this, pack, snap, cat, launchAfter, startStreaming, policy] {
            std::string err;
            if (!packs::CommonInstalled() || cat.online) packs::InstallCommon(cat, &err);
            auto obs = detect::Find(snap.streaming, "obs");
            auto snapNow = snap;
            if (!obs || !obs->installed) {
                Post([this] { kitStatus_ = L"Installation d'OBS Studio…"; });
                detect::WingetInstall("OBSProject.OBSStudio");
                snapNow = detect::Scan();
            }
            bool recent = false;
            {
                auto list = backup::List();
                std::error_code ec;
                if (!list.empty()) {
                    auto age = fs::file_time_type::clock::now() - fs::last_write_time(list.front().zip, ec);
                    recent = !ec && age < std::chrono::hours(6);
                }
            }
            if (!recent) {
                Post([this] { kitStatus_ = L"Sauvegarde avant modification…"; });
                backup::Entry e;
                backup::Create("Avant application du kit", nullptr, &e, nullptr);
            }
            Post([this] { kitStatus_ = L"Écriture des scènes OBS…"; });
            auto r = obskit::Apply(pack, snapNow, policy);
            bool launched = r.ok && launchAfter && obskit::LaunchObs(pack, snapNow, startStreaming);
            Post([this, r, launched, launchAfter, snapNow] {
                kitBusy_ = false;
                snap_ = snapNow;
                if (!r.ok) {
                    kitStatus_ = Widen(r.error);
                    Toast(Widen(r.error), false);
                    return;
                }
                kitStatus_ = L"Collection « " + r.collection + L" » prête dans OBS.";
                if (!r.keptUserFiles.empty())
                    kitStatus_ += L" " + std::to_wstring(r.keptUserFiles.size()) + L" fichier(s) modifié(s) par toi conservé(s).";
                Toast(launchAfter ? (launched ? L"OBS s'ouvre avec ton kit." : L"Kit appliqué, mais OBS n'a pas pu s'ouvrir.")
                                  : L"Kit appliqué à OBS.",
                      !launchAfter || launched);
            });
        });
    };
    if (obskit::ObsRunning()) {
        if (launchAfter) {
            Toast(L"OBS est déjà ouvert.");
            return;
        }
        Dialog d;
        d.icon = L"";
        d.title = L"Ferme OBS d'abord";
        d.body = L"OBS réécrit ses fichiers quand il se ferme : ferme-le puis clique à nouveau sur « Appliquer à OBS ».";
        d.buttons = {{L"OK", Style::Primary, nullptr}};
        ShowDialog(std::move(d));
        return;
    }
    if (launchAfter) {
        run(kitfiles::Policy::KeepUserChanges);
        return;
    }
    auto mods = kitfiles::DetectModified();
    if (!mods.empty() && !Cfg().alwaysKeepModifications) {
        Dialog d;
        d.icon = L"";
        d.title = L"Tu as modifié ton kit dans OBS";
        d.body = L"Je détecte des modifications sur votre interface et vos paramètres de live. Voulez-vous les garder ou les remplacer par la version du kit ?";
        for (auto& m : mods)
            d.list.push_back(m.label + (m.details.empty() ? L"" : L" : " + m.details.front() +
                                                                  (m.details.size() > 1 ? L" (+" + std::to_wstring(m.details.size() - 1) + L")" : L"")));
        d.buttons = {{L"Remplacer", Style::Danger, [run] { run(kitfiles::Policy::Replace); }},
                     {L"Garder mes modifs", Style::Primary, [run] { run(kitfiles::Policy::KeepUserChanges); }}};
        ShowDialog(std::move(d));
        return;
    }
    run(Cfg().alwaysKeepModifications ? kitfiles::Policy::KeepUserChanges : kitfiles::Policy::Replace);
}

void App::LaunchLive() {
    if (obskit::ObsRunning()) {
        Toast(L"OBS est déjà ouvert : passe sur la scène Démarrage.");
        return;
    }
    ApplyKit(true, false);
}

}
