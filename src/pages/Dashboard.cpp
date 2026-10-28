#include <cmath>

#include "app/App.h"
#include "core/Config.h"
#include "core/Constants.h"
#include "core/FiveM.h"

namespace sk {

using namespace ui;

namespace {

std::wstring Size(unsigned long long b) {
    wchar_t buf[32];
    if (b >= 1ull << 30) swprintf_s(buf, L"%.1f Go", b / double(1ull << 30));
    else if (b >= 1ull << 20) swprintf_s(buf, L"%.0f Mo", b / double(1ull << 20));
    else swprintf_s(buf, L"%.0f Ko", b / 1024.0);
    return buf;
}

std::wstring PrettyStamp(const std::string& s) {
    if (s.size() < 16) return Widen(s);
    return Widen(s.substr(8, 2) + "/" + s.substr(5, 2) + "/" + s.substr(0, 4) + " à " + s.substr(11, 2) + "h" +
                 s.substr(14, 2));
}

struct NavDef {
    Section s;
    const wchar_t* icon;
    const wchar_t* label;
};

const NavDef kNav[] = {
    {Section::Home, L"", L"Accueil"},
    {Section::Servers, L"", L"Serveurs"},
    {Section::Platforms, L"", L"Diffusion & clés"},
    {Section::Audio, L"", L"Audio & micro"},
    {Section::Music, L"", L"Musique"},
    {Section::Scenes, L"", L"Scènes"},
    {Section::Overlays, L"", L"Overlays & chat"},
    {Section::Extras, L"", L"Extras"},
    {Section::Backups, L"", L"Sauvegardes"},
    {Section::Updates, L"", L"Mises à jour"},
    {Section::Settings, L"", L"Paramètres"},
    {Section::About, L"", L"Aide & à propos"},
};

}

void App::DrawDashboard() {
    auto& t = ui_.theme;
    float W = gfx_.Width(), H = gfx_.Height(), top = 46;
    float navW = 236;
    gfx_.Fill({0, top, navW, H - top}, t.bg.lighten(0.02f).alpha(0.75f));
    gfx_.Line(navW, top, navW, H, t.line);
    float y = top + 18;
    for (auto& n : kNav) {
        if (n.s == Section::Backups) {
            y += 10;
            gfx_.Line(20, y, navW - 20, y, t.line);
            y += 12;
        }
        std::wstring badge;
        if (n.s == Section::Updates && update_.available) badge = L"NOUVEAU";
        if (ui_.NavItem("nav" + std::to_string((int)n.s), {12, y, navW - 24, 40}, n.icon, n.label, section_ == n.s, badge))
            section_ = n.s;
        y += 44;
    }
    gfx_.Text(L"v" SK_WIDEN_VERSION L"  ·  " + Widen(Cfg().channel == "beta" ? "bêta" : "stable"),
              {24, H - 40, navW - 40, 24}, Font::Small, t.faint);

    if (section_ != Section::Audio) meterRun_ = false;
    Rect area{navW + 44, top + 34, W - navW - 88, H - top - 60};
    float a = pageAnim_;
    gfx_.PushOpacity(a);
    gfx_.SetTransform(0, 18 * (1 - a));
    switch (section_) {
    case Section::Home: DashHome(area); break;
    case Section::Servers: DashServers(area); break;
    case Section::Audio: DashAudio(area); break;
    case Section::Music: DashMusic(area); break;
    case Section::Scenes: DashScenes(area); break;
    case Section::Platforms: DashPlatforms(area); break;
    case Section::Overlays: DashOverlays(area); break;
    case Section::Extras: DashExtras(area); break;
    case Section::Backups: DashBackups(area); break;
    case Section::Updates: DashUpdates(area); break;
    case Section::Settings: DashSettings(area); break;
    case Section::About: DashAbout(area); break;
    }
    gfx_.ResetTransform();
    gfx_.PopOpacity();
}

void App::DashHome(const Rect& a) {
    auto& t = ui_.theme;
    const packs::Pack* p = ActivePack();
    Rect hero{a.x, a.y, a.w, 190};
    gfx_.PushClip(hero, 20);
    gfx_.FillGradient(hero, t.accent.darken(0.45f), t.accent2.darken(0.7f), false);
    for (int i = 0; i < 6; ++i) {
        float x = hero.r() - 260 + i * 46;
        gfx_.Line(x, hero.b(), x + 120, hero.y, Color{1, 1, 1, 0.05f}, 14);
    }
    gfx_.PopClip();
    gfx_.Stroke(hero, t.accent.alpha(0.35f), 20);
    Rect logo{hero.x + 30, hero.y + 35, 120, 120};
    ID2D1Bitmap* bmp = p && p->installed ? gfx_.Image(p->localLogo()) : nullptr;
    gfx_.Glow(logo, t.accent, 24, 16);
    if (bmp) gfx_.DrawImage(bmp, logo, 1, true, 24);
    else gfx_.DrawImage(gfx_.ImageFromResource(10), logo);
    gfx_.Text(L"PRÊT POUR LE LIVE", {hero.x + 176, hero.y + 42, 400, 20}, Font::Caption, t.accent.lighten(0.5f));
    gfx_.Text(p ? Widen(p->name) : L"Slayer Kit", {hero.x + 176, hero.y + 62, hero.w - 420, 50}, Font::Display, t.text);
    gfx_.Text(p && !p->discord.empty() ? Widen(p->discord) : L"Kit stream RP by yns_bnl",
              {hero.x + 176, hero.y + 112, hero.w - 420, 24}, Font::Body, t.text.alpha(0.75f));
    Rect go{hero.r() - 250, hero.y + 70, 220, 52};
    if (ui_.Button("home-live", go, kitBusy_ ? L"Préparation…" : L"Lancer mon live", Style::Primary, !kitBusy_, L""))
        LaunchLive();
    ui_.Tooltip(go, L"Applique le kit à OBS (si besoin) et ouvre OBS sur l'écran de démarrage.");

    float cw = (a.w - 3 * 16) / 4, ch = 120, y = hero.b() + 24;
    struct Tile { const wchar_t* icon; std::wstring title; std::wstring value; Color c; };
    std::wstring soft = Cfg().software == "tiktok-studio" ? L"TikTok LIVE Studio"
                        : Cfg().software == "streamlabs" ? L"Streamlabs"
                                                         : L"OBS Studio";
    std::wstring plats;
    for (auto& pl : Cfg().platforms) plats += (plats.empty() ? L"" : L", ") + Widen(pl == "tiktok" ? "TikTok" : pl == "twitch" ? "Twitch" : pl == "kick" ? "Kick" : "YouTube");
    std::wstring fivem = !snap_.fivemServerTitle.empty() ? L"En jeu" : snap_.fivem.running ? L"Lancé" : snap_.fivem.installed ? L"Installé" : L"Non trouvé";
    std::wstring last = backups_.empty() ? L"Aucune" : PrettyStamp(backups_.front().stamp);
    Tile tiles[] = {{L"", L"Logiciel", soft, t.accent},
                    {L"", L"Plateformes", plats.empty() ? L"—" : plats, t.accent2},
                    {L"", L"FiveM", fivem, snap_.fivemServerTitle.empty() ? t.muted : t.ok},
                    {L"", L"Dernière sauvegarde", last, t.ok}};
    for (int i = 0; i < 4; ++i) {
        Rect c{a.x + i * (cw + 16), y, cw, ch};
        ui_.Panel(c, 16);
        gfx_.Fill({c.x + 18, c.y + 18, 36, 36}, tiles[i].c.alpha(0.15f), 10);
        gfx_.Text(tiles[i].icon, {c.x + 18, c.y + 18, 36, 36}, Font::Icon, tiles[i].c.lighten(0.2f), Align::Center);
        gfx_.Text(tiles[i].title, {c.x + 18, c.y + 62, c.w - 36, 20}, Font::Small, t.muted);
        gfx_.Text(tiles[i].value, {c.x + 18, c.y + 82, c.w - 36, 26}, Font::BodyBold, t.text);
    }

    y += ch + 24;
    Rect check{a.x, y, a.w, a.b() - y};
    ui_.Panel(check, 16);
    gfx_.Text(L"CHECK-UP", {check.x + 22, check.y + 18, 300, 20}, Font::Caption, t.faint);
    struct Row { std::wstring label; int state; std::wstring note; };
    bool obsOk = detect::Find(snap_.streaming, "obs") && detect::Find(snap_.streaming, "obs")->installed;
    bool music = false;
    for (auto& m : snap_.music) music |= m.installed;
    std::vector<Row> rows = {
        {L"Logiciel de live", obsOk ? 2 : 1, obsOk ? L"OBS détecté" : L"OBS sera installé au lancement du live"},
        {L"Kit OBS", Cfg().kitApplied ? 2 : 1, Cfg().kitApplied ? L"Appliqué" : L"Pas encore appliqué (Scènes)"},
        {L"Micro", Cfg().micTested ? 2 : 1, Cfg().micTested ? Widen(Cfg().micName) : L"Fais le test micro (Audio & micro)"},
        {L"Musique", music || !snap_.musicSites.empty() ? 2 : 1,
         music ? L"Application de musique détectée" : L"Aucune appli détectée : on t'aidera à en installer une"},
        {L"Sauvegarde GitHub", Cfg().githubBackup ? 2 : 1,
         Cfg().githubBackup ? L"Connecté : " + Widen(Cfg().githubUser) : L"Non connecté (Sauvegardes)"},
        {L"Mises à jour", update_.reachable && !update_.available ? 2 : 1,
         !update_.reachable ? L"Serveur de mises à jour injoignable pour l'instant"
         : update_.available ? L"Version " + Widen(update_.version) + L" disponible"
                             : L"À jour"},
    };
    float ry = check.y + 50;
    for (auto& r : rows) {
        if (ry > check.b() - 30) break;
        Color c = r.state == 2 ? t.ok : t.warn;
        gfx_.Circle(check.x + 30, ry + 14, 5, c);
        gfx_.Glow({check.x + 25, ry + 9, 10, 10}, c, 5, 6);
        gfx_.Text(r.label, {check.x + 48, ry, 240, 28}, Font::BodyBold, t.text);
        gfx_.Text(r.note, {check.x + 290, ry, check.w - 310, 28}, Font::Small, t.muted);
        ry += 36;
    }
}

void App::DashServers(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Serveurs", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Installe d'autres packs, choisis celui par défaut. Le thème bascule tout seul selon le serveur ouvert dans FiveM.",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);
    Rect auto_{a.x, a.y + 78, a.w, 56};
    ui_.Panel(auto_, 14);
    gfx_.Text(L"", {auto_.x + 20, auto_.y, 24, auto_.h}, Font::Icon, t.accent.lighten(0.2f));
    gfx_.Text(L"Détection automatique du serveur FiveM", {auto_.x + 56, auto_.y, 400, auto_.h}, Font::BodyBold, t.text);
    if (ui_.Toggle("srv-auto", {auto_.r() - 70, auto_.y + 15, 50, 26}, Cfg().autoDetectServer)) SaveSettings();

    float y = a.y + 156;
    float cw = (a.w - 2 * 16) / 3, ch = 200;
    int i = 0;
    for (auto& p : catalog_.packs) {
        Rect c{a.x + (i % 3) * (cw + 16), y + (i / 3) * (ch + 16), cw, ch};
        ++i;
        Color acc{p.accent.r, p.accent.g, p.accent.b, 1}, acc2{p.accent2.r, p.accent2.g, p.accent2.b, 1};
        bool active = Cfg().activePack == p.id;
        bool owned = std::find(Cfg().packs.begin(), Cfg().packs.end(), p.id) != Cfg().packs.end();
        if (active) gfx_.Glow(c, acc, 16, 14);
        ui_.Panel(c, 16);
        if (active) gfx_.Stroke(c, acc, 16, 2);
        Rect banner{c.x + 1, c.y + 1, c.w - 2, 70};
        gfx_.PushClip(banner, 15);
        gfx_.FillGradient(banner, acc.darken(0.35f), acc2.darken(0.55f), false);
        gfx_.PopClip();
        Rect lr{c.x + 18, c.y + 36, 56, 56};
        gfx_.Fill(lr.inset(-3), t.panel, 14);
        if (auto bmp = p.installed ? gfx_.Image(p.localLogo()) : nullptr) gfx_.DrawImage(bmp, lr, 1, true, 12);
        else {
            gfx_.FillGradient(lr, acc, acc2, false, 12);
            gfx_.Text(Widen(p.name.substr(0, 1)), lr, Font::Title, Color{1, 1, 1, 1}, Align::Center);
        }
        gfx_.Text(Widen(p.name), {c.x + 18, c.y + 100, c.w - 36, 26}, Font::Heading, t.text);
        gfx_.Text(L"v" + Widen(p.version) + (p.installed ? L" · installé" : L""), {c.x + 18, c.y + 124, c.w - 36, 20},
                  Font::Small, t.muted);
        Rect btn{c.x + 18, c.b() - 52, c.w - 36, 38};
        bool outdated = p.installed && updater::CompareVersions(p.version, p.installedVersion) > 0;
        bool install = false;
        if (outdated) {
            install = ui_.Button("srv-up-" + p.id, btn, L"Mettre à jour (v" + Widen(p.version) + L")", Style::Primary,
                                 !p.files.empty(), L"");
        } else if (active) {
            ui_.Badge({c.x + 18, c.b() - 44, 110, 22}, L"PAR DÉFAUT", acc);
        } else if (p.installed) {
            if (ui_.Button("srv-use-" + p.id, btn, L"Utiliser par défaut", Style::Secondary)) {
                if (!owned) Cfg().packs.push_back(p.id);
                Cfg().activePack = p.id;
                SaveSettings();
                ApplyTheme();
            }
        } else {
            install = ui_.Button("srv-get-" + p.id, btn, L"Installer", Style::Secondary, !p.files.empty(), L"");
        }
        if (install) {
            auto pack = p;
            Toast(L"Installation de " + Widen(p.name) + L"…");
            Async([this, pack] {
                std::string err;
                bool ok = packs::InstallPack(pack, nullptr, &err);
                Post([this, pack, ok, err] {
                    if (ok) {
                        if (std::find(Cfg().packs.begin(), Cfg().packs.end(), pack.id) == Cfg().packs.end())
                            Cfg().packs.push_back(pack.id);
                        if (Cfg().activePack.empty()) Cfg().activePack = pack.id;
                        SaveSettings();
                        catalogLoading_ = false;
                        Refresh();
                    }
                    Toast(ok ? Widen(pack.name) + L" installé." : L"Échec : " + Widen(err), ok);
                });
            });
        }
    }
}

void App::LoadGithubHistory() {
    std::string token = Cfg().githubToken();
    if (token.empty()) return;
    Async([this, token, user = Cfg().githubUser] {
        std::string err;
        auto h = github::History(token, user, Narrow(k::kBackupRepo), &err);
        Post([this, h] { ghHistory_ = h; });
    });
}

void App::DashBackups(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Sauvegardes", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Tout est sauvegardé avant chaque changement. Restaure n'importe quelle version en un clic.",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);

    float colW = (a.w - 20) / 2;
    Rect local{a.x, a.y + 82, colW, a.h - 82};
    ui_.Panel(local, 16);
    gfx_.Text(L"SUR CE PC", {local.x + 22, local.y + 18, 200, 20}, Font::Caption, t.faint);
    if (ui_.Button("bk-now", {local.x + 22, local.y + 48, 200, 40}, L"Sauvegarder maintenant", Style::Primary,
                   !backupBusy_, L"")) {
        backupBusy_ = true;
        Async([this] {
            backup::Entry e;
            std::string err;
            bool ok = backup::Create("Sauvegarde manuelle", [this](const std::wstring& w, float p) {
                Post([this, w, p] {
                    backupStatus_ = w;
                    backupProgress_ = p;
                });
            }, &e, &err);
            Post([this, ok, e, err] {
                backupBusy_ = false;
                backupStatus_.clear();
                Toast(ok ? L"Sauvegarde créée." : L"Échec : " + Widen(err), ok);
                backups_ = backup::List();
                if (ok && Cfg().githubBackup) PushGithubBackup(e);
            });
        });
    }
    if (ui_.Button("bk-folder", {local.x + 232, local.y + 48, 150, 40}, L"Ouvrir le dossier", Style::Ghost, true, L""))
        OpenFolder(backup::LocalBackupDir());
    if (backupBusy_) {
        ui_.Progress({local.x + 22, local.y + 100, local.w - 44, 6}, backupProgress_);
        gfx_.Text(backupStatus_, {local.x + 22, local.y + 110, local.w - 44, 24}, Font::Small, t.muted);
    }
    Rect list{local.x + 12, local.y + 140, local.w - 24, local.h - 200};
    float content = 64.f * backups_.size();
    float sc = ui_.Scroll("bk-scroll", list, content);
    gfx_.PushClip(list);
    float y = list.y - sc;
    for (size_t i = 0; i < backups_.size(); ++i) {
        auto& e = backups_[i];
        Rect row{list.x, y, list.w - 10, 56};
        if (row.b() > list.y && row.y < list.b()) {
            bool hover = ui_.Hover(row) && ui_.Hover(list);
            if (hover) gfx_.Fill(row, Color{1, 1, 1, 0.04f}, 10);
            gfx_.Text(PrettyStamp(e.stamp), {row.x + 12, row.y + 6, 220, 24}, Font::BodyBold, t.text);
            gfx_.Text(Widen(e.reason) + L"  ·  " + Size(e.bytes), {row.x + 12, row.y + 28, row.w - 140, 22}, Font::Small,
                      t.muted);
            if (ui_.Button("bk-rs" + std::to_string(i), {row.r() - 110, row.y + 10, 100, 36}, L"Restaurer", Style::Secondary)) {
                fs::path zip = e.zip;
                Dialog d;
                d.icon = L"";
                d.title = L"Restaurer cette sauvegarde ?";
                d.body = L"OBS, TikTok LIVE Studio, FiveM et Slayer Kit reviennent exactement à l'état du " +
                         PrettyStamp(e.stamp) + L". Ton état actuel est sauvegardé juste avant. Ferme OBS d'abord.";
                d.buttons = {{L"Annuler", Style::Ghost, nullptr},
                             {L"Restaurer", Style::Primary, [this, zip] {
                                  Async([this, zip] {
                                      std::string err;
                                      bool ok = backup::Restore(zip, nullptr, &err);
                                      Post([this, ok, err] {
                                          Toast(ok ? L"Restauration terminée." : Widen(err), ok);
                                          Refresh();
                                      });
                                  });
                              }}};
                ShowDialog(std::move(d));
            }
        }
        y += 64;
    }
    gfx_.PopClip();
    if (backups_.empty()) gfx_.Text(L"Aucune sauvegarde pour l'instant.", list, Font::Small, t.faint, Align::Center);
    if (ui_.Button("bk-old", {local.x + 22, local.b() - 52, local.w - 44, 38}, L"Charger un ancien kit OBS",
                   Style::Ghost, true, L"")) {
        std::vector<fs::path> zips;
        std::error_code ec;
        for (auto& f : fs::directory_iterator(backup::OldObsKitsDir(), ec))
            if (f.path().extension() == L".zip") zips.push_back(f.path());
        std::sort(zips.begin(), zips.end(), std::greater<>());
        Dialog dl;
        dl.icon = L"";
        if (zips.empty()) {
            dl.title = L"Aucun ancien kit";
            dl.body = L"Le dossier « Anciens kits OBS » de tes Documents est vide.";
            dl.buttons = {{L"OK", Style::Primary, nullptr}};
        } else {
            fs::path latest = zips.front();
            dl.title = L"Recharger ton ancien OBS ?";
            dl.body = L"Ton OBS revient exactement comme avant Slayer Kit (scènes, profils, scripts). Ton état actuel est sauvegardé juste avant. Ferme OBS d'abord.";
            for (size_t z = 0; z < zips.size() && z < 6; ++z) dl.list.push_back(zips[z].filename().wstring());
            dl.buttons = {{L"Ouvrir le dossier", Style::Ghost, [] { OpenFolder(backup::OldObsKitsDir()); }},
                          {L"Recharger le plus récent", Style::Primary, [this, latest] {
                               Async([this, latest] {
                                   std::string err;
                                   bool ok = backup::Restore(latest, nullptr, &err);
                                   Post([this, ok, err] {
                                       Toast(ok ? L"Ancien OBS rechargé." : Widen(err), ok);
                                       Refresh();
                                   });
                               });
                           }}};
        }
        ShowDialog(std::move(dl));
    }

    Rect gh{a.x + colW + 20, a.y + 82, colW, a.h - 82};
    ui_.Panel(gh, 16);
    gfx_.Text(L"GITHUB PRIVÉ", {gh.x + 22, gh.y + 18, 200, 20}, Font::Caption, t.faint);
    if (!Cfg().githubBackup) {
        gfx_.Text(L"Connecte ton compte : un repo privé « slayer-kit-backup » garde l'historique de toutes tes sauvegardes.",
                  {gh.x + 22, gh.y + 46, gh.w - 44, 48}, Font::Small, t.muted, Align::Left, false, true);
        DrawGithubConnect({gh.x + 22, gh.y + 100, gh.w - 44, gh.h - 120});
        return;
    }
    gfx_.Text(L"Connecté : " + Widen(Cfg().githubUser), {gh.x + 22, gh.y + 44, gh.w - 44, 26}, Font::BodyBold, t.text);
    if (ui_.Button("gh-repo", {gh.r() - 160, gh.y + 40, 138, 34}, L"Voir le repo", Style::Ghost, true, L""))
        OpenUrl(L"https://github.com/" + Widen(Cfg().githubUser) + L"/" + k::kBackupRepo);
    if (ui_.Button("gh-push", {gh.x + 22, gh.y + 84, 230, 40}, L"Envoyer la dernière", Style::Secondary,
                   !ghBusy_ && !backups_.empty(), L""))
        PushGithubBackup(backups_.front());
    if (ui_.Button("gh-hist", {gh.x + 262, gh.y + 84, 150, 40}, L"Actualiser", Style::Ghost, true, L""))
        LoadGithubHistory();
    if (ghBusy_) ui_.Progress({gh.x + 22, gh.y + 136, gh.w - 44, 6}, ghProgress_);
    if (!ghStatus_.empty()) gfx_.Text(ghStatus_, {gh.x + 22, gh.y + 146, gh.w - 44, 22}, Font::Small, t.muted);

    float y2 = gh.y + 180;
    if (ghHistory_.empty()) {
        static bool asked = false;
        if (!asked) {
            asked = true;
            LoadGithubHistory();
        }
        gfx_.Text(L"Historique vide ou en chargement.", {gh.x + 22, y2, gh.w - 44, 24}, Font::Small, t.faint);
    }
    for (size_t i = 0; i < ghHistory_.size() && y2 < gh.b() - 60; ++i) {
        auto& s = ghHistory_[i];
        Rect row{gh.x + 12, y2, gh.w - 24, 52};
        gfx_.Text(Widen(s.message), {row.x + 12, row.y + 4, row.w - 140, 24}, Font::SmallBold, t.text);
        gfx_.Text(Widen(s.date.substr(0, 10)), {row.x + 12, row.y + 26, 200, 20}, Font::Small, t.muted);
        if (ui_.Button("gh-rs" + std::to_string(i), {row.r() - 110, row.y + 8, 100, 34}, L"Restaurer", Style::Secondary,
                       !ghBusy_)) {
            std::string sha = s.sha;
            ghBusy_ = true;
            Async([this, sha] {
                fs::path dir = TempDir() / L"gh-restore";
                std::error_code ec;
                fs::remove_all(dir, ec);
                std::string err;
                bool ok = github::PullSnapshot(Cfg().githubToken(), Cfg().githubUser, Narrow(k::kBackupRepo), sha, dir,
                                               nullptr, &err);
                fs::remove(dir / L"README.md", ec);
                fs::path zip = TempDir() / L"gh-restore.zip";
                ok = ok && backup::ZipDir(dir, zip) && backup::Restore(zip, nullptr, &err);
                fs::remove_all(dir, ec);
                Post([this, ok, err] {
                    ghBusy_ = false;
                    Toast(ok ? L"Restauré depuis GitHub. Les clés de stream sont à ressaisir." : Widen(err), ok);
                    Refresh();
                });
            });
        }
        y2 += 58;
    }
}

void App::DashUpdates(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Mises à jour", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Chaque mise à jour publiée sur GitHub arrive chez tout le monde, sans écraser tes modifications.",
              {a.x, a.y + 40, a.w, 24}, Font::Body, t.muted);
    Rect card{a.x, a.y + 84, std::min(a.w, 820.f), 150};
    ui_.Panel(card, 16);
    gfx_.Text(L"VERSION INSTALLÉE", {card.x + 22, card.y + 20, 300, 20}, Font::Caption, t.faint);
    gfx_.Text(L"v" SK_WIDEN_VERSION, {card.x + 22, card.y + 42, 300, 44}, Font::Title, t.text);
    std::wstring st = updateChecking_ ? L"Vérification…"
                      : update_.available ? L"Version " + Widen(update_.version) + (update_.critical ? L" (critique)" : L"") + L" disponible"
                      : update_.reachable ? L"Tu es à jour."
                                          : L"Pas encore vérifié";
    gfx_.Text(st, {card.x + 22, card.y + 90, card.w - 300, 24}, Font::Body, update_.available ? t.accent2 : t.muted);
    if (updateBusy_) {
        ui_.Progress({card.x + 22, card.y + 124, card.w - 44, 6}, updateProgress_, updateProgress_ <= 0);
        gfx_.Text(updateStatus_, {card.x + 22, card.y + 100, card.w - 44, 22}, Font::Small, t.muted);
    } else if (update_.available) {
        if (ui_.Button("up-install", {card.r() - 222, card.y + 50, 200, 46}, L"Mettre à jour", Style::Primary, true, L""))
            OfferUpdate();
    } else if (ui_.Button("up-check", {card.r() - 222, card.y + 50, 200, 46}, L"Vérifier", Style::Secondary,
                          !updateChecking_, L""))
        CheckUpdates(true);
    if (criticalWaiting_)
        gfx_.Text(L"Mise à jour critique en attente : elle sera appliquée dès que tu couperas ton live.",
                  {card.x + 22, card.b() + 8, card.w, 22}, Font::Small, t.warn);

    float y = card.b() + 40;
    Rect opts{a.x, y, card.w, 150};
    ui_.Panel(opts, 16);
    gfx_.Text(L"Canal bêta", {opts.x + 22, opts.y + 18, 400, 26}, Font::BodyBold, t.text);
    gfx_.Text(L"Reçois les nouveautés avant tout le monde (peut contenir des bugs).", {opts.x + 22, opts.y + 42, 520, 22},
              Font::Small, t.muted);
    bool beta = Cfg().channel == "beta";
    if (ui_.Toggle("up-beta", {opts.r() - 70, opts.y + 22, 50, 26}, beta)) {
        Cfg().channel = beta ? "beta" : "stable";
        SaveSettings();
        CheckUpdates(false);
    }
    gfx_.Line(opts.x + 22, opts.y + 76, opts.r() - 22, opts.y + 76, t.line);
    gfx_.Text(L"Toujours garder mes modifications", {opts.x + 22, opts.y + 88, 400, 26}, Font::BodyBold, t.text);
    gfx_.Text(L"Les mises à jour n'écrasent jamais ce que tu as personnalisé, sans te le demander.",
              {opts.x + 22, opts.y + 112, 560, 22}, Font::Small, t.muted);
    if (ui_.Toggle("up-keep", {opts.r() - 70, opts.y + 92, 50, 26}, Cfg().alwaysKeepModifications)) SaveSettings();

    y = opts.b() + 24;
    gfx_.Text(modified_.empty() ? L"Aucune modification personnelle détectée sur les fichiers du kit."
                                : L"Tes modifications personnelles (gardées lors des mises à jour) :",
              {a.x, y, a.w, 24}, Font::Small, t.faint);
    y += 28;
    for (size_t i = 0; i < modified_.size() && y < a.b() - 20; ++i) {
        auto& m = modified_[i];
        std::wstring d;
        for (size_t k = 0; k < m.details.size() && k < 3; ++k) d += (k ? L", " : L"") + m.details[k];
        if (m.details.size() > 3) d += L"…";
        gfx_.Text(L"• " + m.label + (d.empty() ? L"" : L" — " + d), {a.x + 8, y, std::min(a.w, 820.f), 22}, Font::Small, t.muted);
        y += 24;
    }
}

void App::DashSettings(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Paramètres", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    struct Opt { const wchar_t* title; const wchar_t* text; bool* value; int kind; };
    Opt opts[] = {
        {L"Démarrer avec Windows", L"Slayer Kit se lance discrètement près de l'horloge.", &Cfg().startWithWindows, 1},
        {L"Fermer = réduire près de l'horloge", L"La croix cache la fenêtre au lieu de quitter.", &Cfg().minimizeToTray, 0},
    };
    float y = a.y + 60;
    for (auto& o : opts) {
        Rect r{a.x, y, std::min(a.w, 820.f), 76};
        ui_.Panel(r, 14);
        gfx_.Text(o.title, {r.x + 22, r.y + 14, r.w - 120, 26}, Font::BodyBold, t.text);
        gfx_.Text(o.text, {r.x + 22, r.y + 40, r.w - 120, 22}, Font::Small, t.muted);
        if (ui_.Toggle(Narrow(o.title), {r.r() - 70, r.y + 25, 50, 26}, *o.value)) {
            SaveSettings();
            if (o.kind == 1) SetStartWithWindows(*o.value);
        }
        y += 88;
    }
    {
        static std::vector<fivem::Fix> fixes;
        static double lastCheck = -100;
        if (ui_.time - lastCheck > 5) {
            fixes = fivem::Check();
            lastCheck = ui_.time;
        }
        Rect fr{a.x, y, std::min(a.w, 820.f), 70.f + 30.f * std::max<size_t>(fixes.size(), 1)};
        ui_.Panel(fr, 14);
        gfx_.Text(L"Réglages FiveM / GTA", {fr.x + 22, fr.y + 14, fr.w - 280, 26}, Font::BodyBold, t.text);
        float fy = fr.y + 46;
        if (fixes.empty())
            gfx_.Text(Cfg().fivemPending ? L"En attente de la fermeture de FiveM…" : L"Tout est déjà bien réglé pour le live.",
                      {fr.x + 22, fy, fr.w - 44, 24}, Font::Small, Cfg().fivemPending ? t.warn : t.ok);
        for (auto& f : fixes) {
            gfx_.Text(f.label, {fr.x + 22, fy, 260, 26}, Font::Small, t.text);
            gfx_.Text(f.why, {fr.x + 290, fy, fr.w - 320, 26}, Font::Small, t.muted);
            fy += 30;
        }
        if (!fixes.empty() &&
            ui_.Button("fivem-apply", {fr.r() - 240, fr.y + 10, 220, 36},
                       fivem::Running() ? L"Appliquer à la fermeture" : L"Appliquer", Style::Primary, !Cfg().fivemPending,
                       L"")) {
            std::string err;
            if (fivem::Running()) {
                Cfg().fivemPending = true;
                SaveSettings();
                Toast(L"Les réglages seront posés dès que tu fermeras FiveM.");
            } else if (fivem::Apply(fixes, &err)) {
                Toast(L"Réglages FiveM appliqués.");
                lastCheck = -100;
            } else {
                Toast(Widen(err), false);
            }
        }
        y = fr.b() + 12;
    }
    Rect redo{a.x, y + 10, 300, 44};
    if (ui_.Button("set-wizard", redo, L"Relancer l'assistant de configuration", Style::Secondary, true, L"")) {
        mode_ = Mode::Wizard;
        wizStep_ = 1;
        installDone_ = false;
    }
    if (ui_.Button("set-logs", {a.x + 316, y + 10, 220, 44}, L"Ouvrir les journaux", Style::Ghost, true, L""))
        OpenFolder(LocalDataDir());
}

void App::DashAbout(const Rect& a) {
    auto& t = ui_.theme;
    Rect logo{a.x, a.y, 96, 96};
    gfx_.DrawImage(gfx_.ImageFromResource(10), logo);
    gfx_.Text(L"Slayer Kit", {a.x + 120, a.y + 10, 400, 44}, Font::Title, t.text);
    gfx_.Text(L"by yns_bnl  ·  v" SK_WIDEN_VERSION L"  ·  Licence MIT", {a.x + 120, a.y + 54, 500, 24}, Font::Body, t.muted);
    gfx_.Text(L"Kit stream RP open source, créé par yns_bnl. Le code est public sur GitHub : tout le monde peut "
              L"vérifier ce que fait l'application et proposer des améliorations ou de nouveaux serveurs.",
              {a.x, a.y + 124, std::min(a.w, 720.f), 60}, Font::Body, t.muted, Align::Left, false, true);
    float y = a.y + 204;
    if (ui_.Button("ab-bug", {a.x, y, 240, 46}, L"Signaler un problème", Style::Primary, true, L"")) {
        OpenUrl(std::wstring(k::kIssuesUrl) + L"?template=bug.yml&version=" SK_WIDEN_VERSION L"&logiciel=" +
                Widen(Cfg().software) + L"&serveur=" + Widen(Cfg().activePack));
        OpenFolder(LocalDataDir());
    }
    bool hasDiscord = !RemoteCfg().helpDiscord.empty();
    if (ui_.Button("ab-discord", {a.x + 256, y, 220, 46}, L"Aide sur Discord", Style::Secondary, hasDiscord, L""))
        OpenUrl(Widen(RemoteCfg().helpDiscord));
    if (!hasDiscord) ui_.Tooltip({a.x + 256, y, 220, 46}, L"Le lien du Discord d'aide arrive bientôt.");
    if (ui_.Button("ab-gh", {a.x + 492, y, 200, 46}, L"Code source", Style::Ghost, true, L"")) OpenUrl(k::kRepoUrl);

    y += 86;
    gfx_.Text(L"Participer", {a.x, y, 400, 28}, Font::Heading, t.text);
    gfx_.Text(L"Chaque proposition est relue par yns_bnl avant d'arriver dans l'application.",
              {a.x, y + 30, std::min(a.w, 720.f), 24}, Font::Small, t.muted);
    y += 66;
    if (ui_.Button("ab-idea", {a.x, y, 240, 46}, L"Proposer une idée", Style::Secondary, true, L""))
        OpenUrl(std::wstring(k::kIssuesUrl) + L"?template=idee.yml");
    if (ui_.Button("ab-pack", {a.x + 256, y, 220, 46}, L"Proposer un serveur", Style::Secondary, true, L""))
        OpenUrl(k::kPacksIssuesUrl);
    if (ui_.Button("ab-contrib", {a.x + 492, y, 200, 46}, L"Contribuer au code", Style::Ghost, true, L""))
        OpenUrl(std::wstring(k::kRepoUrl) + L"/blob/main/CONTRIBUTING.md");
}

}
