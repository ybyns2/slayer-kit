#include <algorithm>
#include <cmath>
#include <ctime>

#include "app/App.h"
#include "core/Config.h"
#include "core/Installers.h"
#include "core/Live.h"
#include "core/ObsKit.h"
#include "core/Overlays.h"

namespace sk {

using namespace ui;

namespace {

struct Plat {
    const char* id;
    const wchar_t* name;
    unsigned color;
    const wchar_t* guide;
    const wchar_t* url;
    std::string Settings::*sealed;
};

const Plat kPlats[] = {
    {"twitch", L"Twitch", 0x9146FF, L"Tableau de bord Twitch → Paramètres → Stream → « Clé de stream principale » → Copier.",
     L"https://dashboard.twitch.tv/settings/stream", &Settings::twitchKeySealed},
    {"youtube", L"YouTube", 0xFF0033, L"YouTube Studio → Créer → Passer au direct → Diffusion → « Clé de flux » → Copier.",
     L"https://studio.youtube.com/", &Settings::youtubeKeySealed},
    {"kick", L"Kick", 0x53FC18, L"Kick → Tableau de bord → Paramètres → Stream → « Stream Key » → Copier.",
     L"https://kick.com/dashboard/settings/stream", &Settings::kickKeySealed},
    {"tiktok", L"TikTok", 0xFE2C55,
     L"TikTok LIVE Center → Diffuser avec un logiciel → copie l'URL du serveur ET la clé (si ton compte y a accès).",
     L"https://livecenter.tiktok.com/producer", &Settings::tiktokKeySealed},
};

std::wstring Ago(long long t) {
    std::time_t tt = (std::time_t)t;
    std::tm tm{};
    localtime_s(&tm, &tt);
    wchar_t b[64];
    wcsftime(b, 64, L"%d/%m/%Y à %Hh%M", &tm);
    return b;
}

}

void App::DashPlatforms(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Diffusion & clés", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Tes clés restent chiffrées par Windows sur ce PC, jamais envoyées (même pas sur ton GitHub privé).",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);

    static std::wstring keyEdit[4], tiktokServer, twitchCh, kickCh;
    static bool loaded = false;
    if (!loaded) {
        for (int i = 0; i < 4; ++i) keyEdit[i] = Widen(Settings::Open(c.*(kPlats[i].sealed)));
        tiktokServer = Widen(c.tiktokServer);
        twitchCh = Widen(c.twitchChannel);
        kickCh = Widen(c.kickChannel);
        loaded = true;
    }

    float kw = std::min(a.w - 280, 820.f);
    float y = a.y + 82;
    gfx_.Text(L"PLATEFORME PRINCIPALE (SORTIE D'OBS)", {a.x, y, a.w, 20}, Font::Caption, t.faint);
    y += 26;
    float bw = (kw - 3 * 10) / 4;
    for (int i = 0; i < 4; ++i) {
        Color brand = Color::Hex(kPlats[i].color);
        bool sel = c.mainPlatform == kPlats[i].id;
        Rect r{a.x + i * (bw + 10), y, bw, 46};
        if (ui_.SelectCard(std::string("mp-") + kPlats[i].id, r, sel, brand)) {
            c.mainPlatform = kPlats[i].id;
            SaveSettings();
        }
        gfx_.Text(kPlats[i].name, r, Font::BodyBold, sel ? t.text : t.muted, Align::Center);
    }
    y += 64;

    for (int i = 0; i < 4; ++i) {
        auto& p = kPlats[i];
        float h = i == 3 ? 132.f : 94.f;
        Rect r{a.x, y, kw, h};
        ui_.Panel(r, 14);
        Color brand = Color::Hex(p.color);
        gfx_.Fill({r.x + 18, r.y + 18, 8, 8}, brand, 4);
        gfx_.Text(p.name, {r.x + 34, r.y + 8, 200, 28}, Font::BodyBold, t.text);
        gfx_.Text(p.guide, {r.x + 18, r.y + 34, r.w - 200, 20}, Font::Small, t.muted);
        ui_.Tooltip({r.x + 18, r.y + 34, r.w - 200, 20}, p.guide);
        if (ui_.Button(std::string("guide-") + p.id, {r.r() - 170, r.y + 10, 152, 30}, L"Ouvrir la page", Style::Ghost, true,
                       L""))
            OpenUrl(p.url);
        float fy = r.y + 56;
        if (i == 3) {
            if (ui_.TextField("tk-srv", {r.x + 18, fy, r.w - 36, 34}, tiktokServer, L"URL du serveur TikTok (rtmp://…)")) {
                c.tiktokServer = Narrow(tiktokServer);
                SaveSettings();
            }
            fy += 40;
        }
        if (ui_.TextField(std::string("key-") + p.id, {r.x + 18, fy, r.w - 36, 34}, keyEdit[i], L"Colle ta clé de stream ici", true)) {
            c.*(p.sealed) = keyEdit[i].empty() ? std::string() : Settings::Seal(Narrow(keyEdit[i]));
            SaveSettings();
        }
        y += h + 10;
    }

    float sx = a.x + kw + 20;
    {
        Rect side{sx, a.y + 82, a.r() - sx, 240};
        ui_.Panel(side, 14);
        gfx_.Text(L"CHAÎNES (POUR LE CHAT ET L'ANNONCE)", {side.x + 18, side.y + 16, side.w - 36, 20}, Font::Caption, t.faint);
        gfx_.Text(L"Twitch", {side.x + 18, side.y + 46, 100, 22}, Font::Small, t.muted);
        if (ui_.TextField("ch-twitch", {side.x + 18, side.y + 70, side.w - 36, 34}, twitchCh, L"ton pseudo Twitch")) {
            c.twitchChannel = Narrow(twitchCh);
            SaveSettings();
        }
        gfx_.Text(L"Kick", {side.x + 18, side.y + 114, 100, 22}, Font::Small, t.muted);
        if (ui_.TextField("ch-kick", {side.x + 18, side.y + 138, side.w - 36, 34}, kickCh, L"ton pseudo Kick")) {
            c.kickChannel = Narrow(kickCh);
            SaveSettings();
        }
        gfx_.Text(L"Réapplique le kit (Scènes) après un changement.", {side.x + 18, side.y + 186, side.w - 36, 40}, Font::Small,
                  t.faint, Align::Left, false, true);

        Rect tools{sx, side.b() + 14, side.w, 250};
        ui_.Panel(tools, 14);
        float ty = tools.y + 16;
        gfx_.Text(L"OUTILS TIKTOK", {tools.x + 18, ty, tools.w - 36, 20}, Font::Caption, t.faint);
        ty += 26;
        bool aitum = installers::AitumInstalled();
        gfx_.Text(L"Aitum Vertical : écran 9:16 qui diffuse sur TikTok en même temps que Twitch.", {tools.x + 18, ty, tools.w - 36, 40},
                  Font::Small, t.muted, Align::Left, false, true);
        ty += 42;
        if (ui_.Button("inst-aitum", {tools.x + 18, ty, tools.w - 36, 36}, aitum ? L"Aitum Vertical installé" : L"Installer Aitum Vertical",
                       Style::Secondary, !aitum && !installingApp_, L"")) {
            installingApp_ = true;
            Async([this] {
                std::string err;
                bool ok = installers::InstallAitumVertical(&err);
                Post([this, ok, err] {
                    installingApp_ = false;
                    Toast(ok ? L"Aitum Vertical installé : un 2e écran vertical apparaît dans OBS." : Widen(err), ok);
                });
            });
        }
        ty += 50;
        gfx_.Text(snap_.vbCable ? L"VB-CABLE installé : le son d'OBS part dans TikTok LIVE Studio."
                                : L"VB-CABLE : envoie le son d'OBS dans TikTok LIVE Studio.",
                  {tools.x + 18, ty, tools.w - 36, 40}, Font::Small, t.muted, Align::Left, false, true);
        ty += 42;
        if (ui_.Button("inst-vb", {tools.x + 18, ty, tools.w - 36, 36}, snap_.vbCable ? L"VB-CABLE installé" : L"Installer VB-CABLE",
                       Style::Secondary, !snap_.vbCable && !installingApp_, L"")) {
            installingApp_ = true;
            Async([this] {
                std::string err;
                bool ok = installers::InstallVbCable(&err);
                Post([this, ok, err] {
                    installingApp_ = false;
                    Toast(ok ? L"VB-CABLE installé. Redémarre le PC si Windows le demande, puis réapplique le kit." : Widen(err), ok);
                    Refresh();
                });
            });
        }
    }
}

void App::DrawLayoutEditor(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Disposition du live", {a.x, a.y, a.w - 260, 40}, Font::Title, t.text);
    gfx_.Text(L"Glisse chaque élément à la souris. Réapplique le kit (Scènes) pour l'envoyer dans OBS.", {a.x, a.y + 40, a.w, 24},
              Font::Body, t.muted);
    if (ui_.Button("lay-done", {a.r() - 150, a.y + 2, 150, 40}, L"Terminé", Style::Primary, true, L"")) layoutEdit_ = false;
    if (ui_.Button("lay-reset", {a.r() - 320, a.y + 2, 160, 40}, L"Par défaut", Style::Ghost, true, L"")) {
        c.layout = json::object();
        SaveSettings();
    }
    float maxW = a.w, maxH = a.h - 84;
    float cw = std::min(maxW, maxH * 16 / 9), ch = cw * 9 / 16;
    Rect canvas{a.x + (a.w - cw) / 2, a.y + 80, cw, ch};
    float k = cw / 1920.f;
    if (auto p = ActivePack()) {
        try {
            auto pj = json::parse(ReadFileText(p->localDir / L"pack.json"));
            fs::path still = p->localDir / Widen(pj["assets"].value("backgroundStill", "fond-fixe.png"));
            if (auto bmp = gfx_.Image(still)) gfx_.DrawImage(bmp, canvas, 0.55f, true, 10);
        } catch (...) {
        }
    }
    gfx_.Fill(canvas, Color{0, 0, 0, 0.35f}, 10);
    gfx_.Stroke(canvas, t.line.alpha(3), 10);
    gfx_.Text(L"JEU (CAPTURE FIVEM)", canvas, Font::Heading, Color{1, 1, 1, 0.12f}, Align::Center);
    if (!ui_.in.down && !dragItem_.empty()) {
        SaveSettings();
        dragItem_.clear();
    }
    for (auto& it : obskit::LayoutItems()) {
        bool on = it.id == "chat" ? c.chatOverlay : it.id == "nowplaying" ? c.nowPlaying : it.id == "rpcard" ? c.rpCard : true;
        float x, y;
        obskit::LayoutPos(it.id, &x, &y);
        Rect box{canvas.x + x * k, canvas.y + y * k, it.w * k, it.h * k};
        bool hover = ui_.Hover(box);
        if (hover) ui_.wantsCursorHand = true;
        if (ui_.in.pressed && dragItem_.empty() && box.contains(ui_.PressX(), ui_.PressY())) {
            dragItem_ = it.id;
            dragDx_ = ui_.PressX() - box.x;
            dragDy_ = ui_.PressY() - box.y;
        }
        if (dragItem_ == it.id && ui_.in.down && ui_.in.mx >= 0) {
            float nx = std::clamp((ui_.in.mx - dragDx_ - canvas.x) / k, 0.f, 1920.f - it.w);
            float ny = std::clamp((ui_.in.my - dragDy_ - canvas.y) / k, 0.f, 1080.f - it.h);
            c.layout[it.id] = json::array({std::round(nx), std::round(ny)});
            box.x = canvas.x + nx * k;
            box.y = canvas.y + ny * k;
        }
        bool active = dragItem_ == it.id;
        gfx_.Fill(box, t.accent.alpha(on ? (active ? 0.45f : 0.28f) : 0.1f), 6);
        gfx_.Stroke(box, on ? t.accent.lighten(0.3f) : t.faint, 6, active ? 2.f : 1.f);
        gfx_.Text(it.label + (on ? L"" : L" (désactivé)"), box.inset(6, 0), Font::SmallBold, on ? t.text : t.faint, Align::Center, true,
                  true);
    }
}

void App::DashOverlays(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    if (layoutEdit_) {
        DrawLayoutEditor(a);
        return;
    }
    if (ui_.Button("lay-open", {a.r() - 240, a.y + 2, 240, 40}, L"Disposer à la souris", Style::Secondary, true, L""))
        layoutEdit_ = true;
    gfx_.Text(L"Overlays & chat", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Tout s'ajoute à la scène En live. Tu peux ensuite tout déplacer à la souris dans OBS : le kit garde tes réglages.",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);
    float colW = (a.w - 20) / 2;
    Rect left{a.x, a.y + 82, colW, a.h - 82};
    ui_.Panel(left, 16);
    float y = left.y + 18;
    auto row = [&](const char* id, const wchar_t* title, const wchar_t* text, bool& v) {
        gfx_.Text(title, {left.x + 22, y, left.w - 110, 24}, Font::BodyBold, t.text);
        gfx_.Text(text, {left.x + 22, y + 24, left.w - 110, 36}, Font::Small, t.muted, Align::Left, false, true);
        if (ui_.Toggle(id, {left.r() - 70, y + 6, 50, 26}, v)) SaveSettings();
        y += 70;
    };
    row("ov-chat", L"Chat unifié", L"Messages Twitch et Kick dans le live, avec les emotes, aux couleurs du serveur.", c.chatOverlay);
    gfx_.Text(L"Position du chat", {left.x + 22, y, 160, 30}, Font::Small, t.muted);
    for (int i = 0; i < 2; ++i) {
        const char* id = i ? "gauche" : "droite";
        if (ui_.Button(std::string("ov-pos-") + id, {left.x + 180 + i * 110, y, 100, 30}, i ? L"Gauche" : L"Droite",
                       c.chatPosition == id ? Style::Primary : Style::Secondary)) {
            c.chatPosition = id;
            SaveSettings();
        }
    }
    y += 44;
    if (ui_.Button("ov-window", {left.x + 22, y, 300, 38}, L"Ouvrir le chat dans une fenêtre", Style::Secondary, true, L""))
        overlays::OpenChatWindow(overlays::LocalUrl("/overlay/chat", true));
    gfx_.Text(L"Fenêtre à poser sur ton 2e écran, hors du live.", {left.x + 22, y + 44, left.w - 44, 22}, Font::Small, t.faint);
    y += 84;
    row("ov-np", L"Musique en cours", L"« ♪ Artiste - Titre » affiché quand Spotify, Deezer ou YouTube Music joue.", c.nowPlaying);
    std::wstring np = overlays::NowPlaying();
    gfx_.Text(np.empty() ? L"Rien en lecture pour l'instant." : L"En ce moment : " + np, {left.x + 22, y - 8, left.w - 44, 22},
              Font::Small, np.empty() ? t.faint : t.ok);
    DrawAlertsCard({left.x + 12, left.b() - 212, left.w - 24, 200});

    Rect right{a.x + colW + 20, a.y + 82, colW, a.h - 82};
    ui_.Panel(right, 16);
    static std::wstring name = Widen(c.rpName), job = Widen(c.rpJob), status = Widen(c.rpStatus);
    float ry = right.y + 18;
    gfx_.Text(L"Carte personnage RP", {right.x + 22, ry, right.w - 110, 24}, Font::BodyBold, t.text);
    if (ui_.Toggle("ov-rp", {right.r() - 70, ry, 50, 26}, c.rpCard)) SaveSettings();
    ry += 40;
    auto field = [&](const char* id, const wchar_t* label, std::wstring& v, std::string& dst, const wchar_t* ph) {
        gfx_.Text(label, {right.x + 22, ry, 200, 20}, Font::Small, t.muted);
        if (ui_.TextField(id, {right.x + 22, ry + 22, right.w - 44, 36}, v, ph, false, 60)) {
            dst = Narrow(v);
            SaveSettings();
        }
        ry += 70;
    };
    field("rp-name", L"Nom du personnage", name, c.rpName, L"ex : Abdou Djamal");
    field("rp-job", L"Métier / groupe", job, c.rpJob, L"ex : Mécano · LS Customs");
    field("rp-status", L"État / objectif", status, c.rpStatus, L"ex : En recherche d'un business");
    Rect prev{right.x + 22, ry + 6, std::min(right.w - 44, 400.f), 116};
    gfx_.Fill(prev, t.bg.alpha(0.9f), 8);
    gfx_.Fill({prev.x, prev.y, 6, prev.h}, t.accent);
    gfx_.Text(name.empty() ? L"Nom du perso" : name, {prev.x + 22, prev.y + 6, prev.w - 30, 50}, Font::Title, t.text);
    gfx_.Text(job, {prev.x + 22, prev.y + 54, prev.w - 30, 30}, Font::BodyBold, t.accent2);
    gfx_.Text(status, {prev.x + 22, prev.y + 82, prev.w - 30, 26}, Font::Small, t.muted);
}

void App::DashExtras(const Rect& a) {
    auto& t = ui_.theme;
    auto& c = Cfg();
    gfx_.Text(L"Extras", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    float colW = (a.w - 20) / 2;
    Rect left{a.x, a.y + 56, colW, a.h - 56};
    ui_.Panel(left, 16);
    static std::wstring hook = Widen(Settings::Open(c.webhookSealed)), msg = Widen(c.webhookMessage),
                        title = Widen(c.webhookTitle), desc = Widen(c.webhookDescription);
    float y = left.y + 18;
    gfx_.Text(L"Annonce Discord « je suis en live »", {left.x + 22, y, left.w - 110, 24}, Font::BodyBold, t.text);
    if (ui_.Toggle("wh-on", {left.r() - 70, y, 50, 26}, c.webhookEnabled)) SaveSettings();
    y += 36;
    auto field = [&](const char* id, const wchar_t* label, std::wstring& v, const wchar_t* ph, bool secret, auto onChange) {
        gfx_.Text(label, {left.x + 22, y, left.w - 44, 20}, Font::Small, t.muted);
        if (ui_.TextField(id, {left.x + 22, y + 22, left.w - 44, 36}, v, ph, secret, 400)) onChange();
        y += 66;
    };
    field("wh-url", L"Lien du webhook (Salon → Modifier → Intégrations → Webhooks)", hook, L"https://discord.com/api/webhooks/…",
          true, [&] {
              c.webhookSealed = hook.empty() ? std::string() : Settings::Seal(Narrow(hook));
              SaveSettings();
          });
    field("wh-msg", L"Message (mention)", msg, L"@everyone Je suis en live !", false, [&] {
        c.webhookMessage = Narrow(msg);
        SaveSettings();
    });
    field("wh-title", L"Titre de l'embed (vide = automatique)", title, L"<pseudo> est en live sur <serveur>", false, [&] {
        c.webhookTitle = Narrow(title);
        SaveSettings();
    });
    field("wh-desc", L"Description (vide = automatique)", desc, L"Viens voir le RP en direct !", false, [&] {
        c.webhookDescription = Narrow(desc);
        SaveSettings();
    });
    bool endMsg = c.webhookEnd;
    gfx_.Text(L"Message de fin avec la durée du live", {left.x + 22, y, left.w - 110, 26}, Font::Small, t.text);
    if (ui_.Toggle("wh-end", {left.r() - 70, y, 50, 26}, endMsg)) {
        c.webhookEnd = endMsg;
        SaveSettings();
    }
    y += 40;
    if (ui_.Button("wh-test", {left.x + 22, y, 220, 40}, L"Envoyer un test", Style::Primary, !hook.empty(), L"")) {
        if (auto p = ActivePack()) {
            auto pack = *p;
            Async([this, pack] {
                std::string err;
                bool ok = live::SendWebhook(pack, true, 0, &err);
                Post([this, ok, err] { Toast(ok ? L"Annonce envoyée sur Discord." : Widen(err), ok); });
            });
        }
    }

    DrawRemoteCard({a.x + colW + 20, a.y + 56, colW, 236});
    Rect right{a.x + colW + 20, a.y + 56 + 250, colW, a.h - 56 - 250};
    ui_.Panel(right, 16);
    float ry = right.y + 18;
    auto row = [&](const char* id, const wchar_t* t1, const wchar_t* t2, bool& v) {
        gfx_.Text(t1, {right.x + 22, ry, right.w - 110, 24}, Font::BodyBold, t.text);
        gfx_.Text(t2, {right.x + 22, ry + 24, right.w - 110, 40}, Font::Small, t.muted, Align::Left, false, true);
        if (ui_.Toggle(id, {right.r() - 70, ry + 6, 50, 26}, v)) SaveSettings();
        ry += 76;
    };
    row("ex-perf", L"Mode performance", L"Pendant le live : FiveM et OBS passent en priorité, Medal/Steam/OneDrive en arrière-plan.",
        c.perfMode);
    row("ex-clip", L"Clips RP instantanés", L"Ctrl + Alt + C enregistre les dernières secondes du live dans Vidéos.", c.replayBuffer);
    if (c.replayBuffer) {
        gfx_.Text(L"Durée du clip", {right.x + 22, ry - 10, 160, 30}, Font::Small, t.muted);
        for (int i = 0; i < 3; ++i) {
            int s = i == 0 ? 30 : i == 1 ? 60 : 120;
            if (ui_.Button("ex-cl" + std::to_string(s), {right.r() - 22 - (3 - i) * 70.f, ry - 10, 64, 30}, std::to_wstring(s) + L" s",
                           c.replaySeconds == s ? Style::Primary : Style::Secondary)) {
                c.replaySeconds = s;
                SaveSettings();
            }
        }
        ry += 34;
    }
    gfx_.Line(right.x + 22, ry, right.r() - 22, ry, t.line);
    ry += 14;
    gfx_.Text(L"STATISTIQUES DES DERNIERS LIVES", {right.x + 22, ry, right.w - 44, 20}, Font::Caption, t.faint);
    ry += 28;
    auto& st = c.liveStats;
    if (st.empty()) gfx_.Text(L"Aucun live enregistré pour l'instant.", {right.x + 22, ry, right.w - 44, 24}, Font::Small, t.faint);
    long long total = 0;
    for (auto& s : st) total += s.value("end", 0ll) - s.value("start", 0ll);
    if (!st.empty()) {
        wchar_t b[128];
        swprintf_s(b, L"%zu live(s) · %lldh%02lld au total", st.size(), total / 3600, (total % 3600) / 60);
        gfx_.Text(b, {right.x + 22, ry, right.w - 44, 24}, Font::BodyBold, t.text);
        ry += 30;
    }
    for (size_t i = st.size(); i-- > 0 && ry < right.b() - 30;) {
        auto& s = st[i];
        long long d = s.value("end", 0ll) - s.value("start", 0ll);
        wchar_t b[64];
        swprintf_s(b, L"%lldh%02lld", d / 3600, (d % 3600) / 60);
        gfx_.Text(Ago(s.value("start", 0ll)), {right.x + 22, ry, 220, 22}, Font::Small, t.muted);
        gfx_.Text(Widen(s.value("pack", "")), {right.x + 240, ry, 160, 22}, Font::Small, t.muted);
        gfx_.Text(b, {right.r() - 100, ry, 78, 22}, Font::SmallBold, t.text, Align::Right);
        ry += 24;
    }
}

void App::LiveTick() {
    if (Cfg().nowPlaying) overlays::UpdateNowPlayingFile();
    auto s = live::Read();
    bool obsUp = IsProcessRunning(L"obs64.exe");
    if (!obsUp && s.live) s.live = false;
    if (s.live == wasLive_) return;
    wasLive_ = s.live;
    const packs::Pack* p = ActivePack();
    auto& c = Cfg();
    if (s.live) {
        liveStart_ = s.since ? s.since : (long long)std::time(nullptr);
        if (c.perfMode) live::PerformanceMode(true);
        if (c.webhookEnabled && p) {
            auto pack = *p;
            Async([pack] { live::SendWebhook(pack, true, 0, nullptr); });
        }
    } else {
        long long end = (long long)std::time(nullptr);
        if (c.perfMode) live::PerformanceMode(false);
        if (liveStart_ && end - liveStart_ > 60) {
            live::RecordStats(liveStart_, end, p ? p->name : "");
            if (c.webhookEnabled && c.webhookEnd && p) {
                auto pack = *p;
                long long dur = end - liveStart_;
                Async([pack, dur] { live::SendWebhook(pack, false, dur, nullptr); });
            }
        }
        liveStart_ = 0;
    }
}

}
