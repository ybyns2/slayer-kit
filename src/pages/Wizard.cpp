#include <cmath>

#include "app/App.h"
#include "core/Config.h"
#include "core/Constants.h"

namespace sk {

using namespace ui;

namespace {

const wchar_t* kStepNames[] = {L"Bienvenue", L"Plateformes", L"Logiciel", L"Serveur", L"Sauvegarde", L"Installation"};
constexpr int kSteps = 6;

struct PlatformDef {
    const char* id;
    const wchar_t* name;
    const wchar_t* letter;
    unsigned color;
    const wchar_t* note;
};

const PlatformDef kPlatforms[] = {
    {"twitch", L"Twitch", L"T", 0x9146FF, L"Redif sans musique grâce à une piste audio séparée."},
    {"tiktok", L"TikTok", L"♪", 0xFE2C55, L"Écran vertical 9:16, via OBS ou TikTok LIVE Studio."},
    {"kick", L"Kick", L"K", 0x53FC18, L"Même mix que TikTok, musique en fond réglée au micro."},
    {"youtube", L"YouTube", L"▶", 0xFF0033, L"Live horizontal, redif identique au live."},
};

std::wstring Size(unsigned long long b) {
    wchar_t buf[32];
    if (b >= 1ull << 30) swprintf_s(buf, L"%.1f Go", b / double(1ull << 30));
    else if (b >= 1ull << 20) swprintf_s(buf, L"%.0f Mo", b / double(1ull << 20));
    else swprintf_s(buf, L"%.0f Ko", b / 1024.0);
    return buf;
}

}

void App::DrawWizard() {
    auto& t = ui_.theme;
    float W = gfx_.Width(), H = gfx_.Height();
    float top = 46;

    float railW = 250;
    Rect rail{0, top, railW, H - top};
    gfx_.Fill(rail, t.bg.lighten(0.02f).alpha(0.7f));
    gfx_.Line(railW, top, railW, H, t.line);
    float y = top + 40;
    gfx_.Text(L"CONFIGURATION", {32, y, 200, 20}, Font::Caption, t.faint);
    y += 34;
    for (int i = 0; i < kSteps; ++i) {
        bool done = i < wizStep_, cur = i == wizStep_;
        float a = ui_.Anim("wz-dot" + std::to_string(i), cur ? 1.f : 0.f, 10);
        float cx = 46, cy = y + 16;
        if (i < kSteps - 1) gfx_.Line(cx, cy + 14, cx, cy + 44, done ? t.accent.alpha(0.7f) : t.line.alpha(2), 2);
        if (done) {
            gfx_.Circle(cx, cy, 12, t.accent);
            ui_.Check(cx, cy, 12, Color{1, 1, 1, 1});
        } else {
            gfx_.Circle(cx, cy, 12, t.panel2.mix(t.accent.alpha(0.25f), a));
            gfx_.Ring(cx, cy, 12, cur ? t.accent : t.line.alpha(2.5f), 2);
            gfx_.Text(std::to_wstring(i + 1), {cx - 12, cy - 12, 24, 24}, Font::SmallBold, cur ? t.text : t.faint,
                      Align::Center);
        }
        gfx_.Text(kStepNames[i], {72, y, railW - 80, 32}, cur ? Font::BodyBold : Font::Body,
                  cur ? t.text : (done ? t.muted : t.faint));
        y += 58;
    }
    gfx_.Text(L"Tout reste modifiable ensuite\ndans le tableau de bord.", {32, H - 84, railW - 48, 50}, Font::Small,
              t.faint, Align::Left, false, true);

    Rect area{railW + 56, top + 40, W - railW - 112, H - top - 40 - 100};
    float a = pageAnim_;
    gfx_.PushOpacity(a);
    gfx_.SetTransform(28 * (1 - a), 0);
    switch (wizStep_) {
    case 0: WizWelcome(area); break;
    case 1: WizPlatforms(area); break;
    case 2: WizSoftware(area); break;
    case 3: WizServers(area); break;
    case 4: WizBackup(area); break;
    case 5: WizInstall(area); break;
    }
    gfx_.ResetTransform();
    gfx_.PopOpacity();

    if (wizStep_ == 5) return;
    Rect footer{railW + 56, H - 84, W - railW - 112, 48};
    if (wizStep_ > 0 && ui_.Button("wz-back", {footer.x, footer.y, 130, 46}, L"Retour", Style::Ghost, true, L""))
        --wizStep_;

    bool canNext = true;
    std::wstring hint;
    if (wizStep_ == 1 && selPlatforms_.empty()) {
        canNext = false;
        hint = L"Choisis au moins une plateforme.";
    }
    if (wizStep_ == 3 && (selPacks_.empty() || selActive_.empty())) {
        canNext = false;
        hint = L"Choisis au moins un serveur.";
    }
    if (wizStep_ == 4 && wantGithub_ && gh_ != GhState::Connected) hint = L"Tu peux connecter GitHub plus tard.";
    std::wstring label = wizStep_ == 0 ? L"Commencer" : wizStep_ == 4 ? L"Installer le kit" : L"Continuer";
    if (!hint.empty())
        gfx_.Text(hint, {footer.x + 150, footer.y, footer.w - 400, 46}, Font::Small, t.muted, Align::Right);
    if (ui_.Button("wz-next", {footer.r() - 220, footer.y, 220, 46}, label, Style::Primary, canNext,
                   wizStep_ == 4 ? L"" : L"")) {
        if (wizStep_ == 4) StartInstall();
        ++wizStep_;
    }
}

void App::WizWelcome(const Rect& a) {
    auto& t = ui_.theme;
    float pulse = 0.5f + 0.5f * (float)std::sin(ui_.time * 2);
    Rect logo{a.x, a.y + 10, 120, 120};
    gfx_.Glow(logo.inset(14), t.accent, 30, 30 + 8 * pulse);
    gfx_.DrawImage(gfx_.ImageFromResource(10), logo);
    gfx_.Text(L"SLAYER KIT", {a.x, a.y + 150, a.w, 56}, Font::Display, t.text, Align::Left);
    gfx_.Text(L"Le kit stream RP complet, par yns_bnl.", {a.x, a.y + 204, a.w, 30}, Font::Heading, t.accent.lighten(0.3f));
    gfx_.Text(L"En quelques étapes, Slayer Kit prépare tout pour tes lives FiveM : overlays et transitions aux couleurs "
              L"de ton serveur, son parfait (micro, jeu, musique), redif Twitch sans musique, sauvegarde complète "
              L"de tes réglages et mises à jour automatiques.",
              {a.x, a.y + 250, std::min(a.w, 640.f), 90}, Font::Body, t.muted, Align::Left, false, true);

    struct F { const wchar_t* icon; const wchar_t* title; const wchar_t* text; };
    F feats[] = {{L"", L"Pensé pour FiveM", L"Détecte ton serveur et règle ton jeu."},
                 {L"", L"Son parfait", L"Test micro, mix auto, redif sans musique."},
                 {L"", L"Rien ne se perd", L"Sauvegarde locale + GitHub privé."}};
    float cw = (std::min(a.w, 760.f) - 32) / 3;
    for (int i = 0; i < 3; ++i) {
        Rect c{a.x + i * (cw + 16), a.y + 370, cw, 112};
        ui_.Panel(c, 14);
        gfx_.Text(feats[i].icon, {c.x + 18, c.y + 16, 30, 30}, Font::Icon, t.accent.lighten(0.2f));
        gfx_.Text(feats[i].title, {c.x + 18, c.y + 48, c.w - 36, 24}, Font::BodyBold, t.text);
        gfx_.Text(feats[i].text, {c.x + 18, c.y + 72, c.w - 36, 34}, Font::Small, t.muted, Align::Left, false, true);
    }
}

void App::WizPlatforms(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Où est-ce que tu lives ?", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Tu peux en choisir plusieurs : le kit prépare chaque plateforme avec le bon format et le bon son.",
              {a.x, a.y + 42, a.w, 24}, Font::Body, t.muted);
    float cw = (std::min(a.w, 820.f) - 16) / 2, ch = 128;
    for (int i = 0; i < 4; ++i) {
        auto& p = kPlatforms[i];
        Rect c{a.x + (i % 2) * (cw + 16), a.y + 96 + (i / 2) * (ch + 16), cw, ch};
        bool sel = selPlatforms_.count(p.id) > 0;
        Color brand = Color::Hex(p.color);
        if (ui_.SelectCard(std::string("pf-") + p.id, c, sel, brand)) {
            if (sel) selPlatforms_.erase(p.id);
            else selPlatforms_.insert(p.id);
        }
        float lift = -3 * ui_.Peek(std::string("pf-") + p.id + "#h");
        Rect cc = c.offset(0, lift);
        gfx_.Fill({cc.x + 20, cc.y + 22, 48, 48}, brand.alpha(0.18f), 12);
        gfx_.Text(p.letter, {cc.x + 20, cc.y + 22, 48, 48}, Font::Title, brand.lighten(0.2f), Align::Center);
        gfx_.Text(p.name, {cc.x + 84, cc.y + 22, cc.w - 140, 26}, Font::Heading, t.text);
        gfx_.Text(p.note, {cc.x + 84, cc.y + 50, cc.w - 110, 60}, Font::Small, t.muted, Align::Left, false, true);
    }
}

void App::WizSoftware(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Avec quel logiciel tu lives ?", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"J'ai regardé ce qui est installé sur ton PC.", {a.x, a.y + 42, a.w, 24}, Font::Body, t.muted);
    if (scanning_ && snap_.streaming.empty()) {
        ui_.Spinner(a.x + 20, a.y + 120, 14);
        gfx_.Text(L"Détection en cours…", {a.x + 46, a.y + 104, 300, 32}, Font::Body, t.muted);
        return;
    }
    struct S { const char* id; const wchar_t* title; const wchar_t* text; const wchar_t* icon; };
    S list[] = {
        {"obs", L"OBS Studio", L"Le kit configure tout directement : scènes, transitions, décomptes, son, raccourcis.", L""},
        {"tiktok-studio", L"TikTok LIVE Studio",
         L"OBS travaille en coulisse (caméra virtuelle + câble audio) : dans TikTok Studio, une seule source à ajouter.",
         L""},
        {"streamlabs", L"Streamlabs Desktop", L"Même méthode : le kit prépare tout dans OBS et l'envoie dans Streamlabs.",
         L""},
    };
    float y = a.y + 96;
    for (auto& s : list) {
        Rect c{a.x, y, std::min(a.w, 820.f), 108};
        bool sel = selSoftware_ == s.id;
        if (ui_.SelectCard(std::string("sw-") + s.id, c, sel, t.accent)) selSoftware_ = s.id;
        const detect::App* found = detect::Find(snap_.streaming, s.id);
        gfx_.Fill({c.x + 20, c.y + 26, 52, 52}, t.accent.alpha(0.14f), 14);
        gfx_.Text(s.icon, {c.x + 20, c.y + 26, 52, 52}, Font::Icon, t.accent.lighten(0.3f), Align::Center);
        gfx_.Text(s.title, {c.x + 92, c.y + 20, 300, 26}, Font::Heading, t.text);
        gfx_.Text(s.text, {c.x + 92, c.y + 48, c.w - 260, 48}, Font::Small, t.muted, Align::Left, false, true);
        std::wstring badge = !found ? L"?" : found->running ? L"OUVERT" : found->installed ? L"DÉTECTÉ" : L"NON INSTALLÉ";
        Color bc = found && found->running ? t.ok : found && found->installed ? t.accent2 : t.faint;
        float bw = gfx_.MeasureWidth(badge, Font::Caption) + 22;
        ui_.Badge({c.r() - bw - 56, c.y + 22, bw, 22}, badge, bc);
        y += 124;
    }
    if (selSoftware_ != "obs") {
        auto obs = detect::Find(snap_.streaming, "obs");
        if (obs && !obs->installed)
            gfx_.Text(L"OBS n'est pas installé : il sera installé automatiquement (gratuit, open source).",
                      {a.x, y + 4, a.w, 24}, Font::Small, t.warn);
    }
}

void App::WizServers(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"Sur quel serveur tu joues ?", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Choisis-en un ou plusieurs. Seuls les packs choisis sont téléchargés.", {a.x, a.y + 42, a.w, 24},
              Font::Body, t.muted);
    if (catalog_.packs.empty()) {
        ui_.Spinner(a.x + 20, a.y + 120, 14);
        gfx_.Text(L"Chargement du catalogue…", {a.x + 46, a.y + 104, 300, 32}, Font::Body, t.muted);
        return;
    }
    if (!catalog_.online)
        gfx_.Text(L"Hors ligne : catalogue local affiché.", {a.x, a.y + 68, a.w, 20}, Font::Small, t.warn);
    int n = (int)catalog_.packs.size();
    int cols = 3;
    float cw = (std::min(a.w, 900.f) - (cols - 1) * 16) / cols, ch = 214;
    for (int i = 0; i < n; ++i) {
        auto& p = catalog_.packs[i];
        Rect c{a.x + (i % cols) * (cw + 16), a.y + 100 + (i / cols) * (ch + 16), cw, ch};
        bool sel = selPacks_.count(p.id) > 0;
        Color acc{p.accent.r, p.accent.g, p.accent.b, 1}, acc2{p.accent2.r, p.accent2.g, p.accent2.b, 1};
        if (ui_.SelectCard("pk-" + p.id, c, sel, acc)) {
            if (sel) {
                selPacks_.erase(p.id);
                if (selActive_ == p.id) selActive_ = selPacks_.empty() ? "" : *selPacks_.begin();
            } else {
                selPacks_.insert(p.id);
                selActive_ = p.id;
            }
            ApplyTheme();
        }
        float lift = -3 * ui_.Peek("pk-" + p.id + "#h");
        Rect cc = c.offset(0, lift);
        Rect banner{cc.x + 1, cc.y + 1, cc.w - 2, 92};
        gfx_.PushClip(banner, 15);
        gfx_.FillGradient(banner, acc.darken(0.35f), acc2.darken(0.55f), false);
        gfx_.PopClip();
        ID2D1Bitmap* logo = p.installed ? gfx_.Image(p.localLogo()) : nullptr;
        Rect lr{cc.x + 20, cc.y + 56, 64, 64};
        gfx_.Fill(lr.inset(-3), t.panel, 16);
        if (logo) gfx_.DrawImage(logo, lr, 1, true, 13);
        else {
            gfx_.FillGradient(lr, acc, acc2, false, 13);
            gfx_.Text(Widen(p.name.substr(0, 1)), lr, Font::Title, Color{1, 1, 1, 1}, Align::Center);
        }
        gfx_.Text(Widen(p.name), {cc.x + 20, cc.y + 128, cc.w - 40, 28}, Font::Heading, t.text);
        gfx_.Text(Widen(p.discord.empty() ? p.description : p.discord), {cc.x + 20, cc.y + 154, cc.w - 40, 22},
                  Font::Small, t.muted);
        if (detectedServer_ == p.id) ui_.Badge({cc.x + 20, cc.b() - 34, 150, 22}, L"TU JOUES DESSUS", t.ok);
        else if (sel && selActive_ == p.id && selPacks_.size() > 1)
            ui_.Badge({cc.x + 20, cc.b() - 34, 120, 22}, L"PAR DÉFAUT", acc);
        else if (sel && selPacks_.size() > 1) {
            Rect mk{cc.x + 20, cc.b() - 36, 150, 26};
            if (ui_.Button("pk-def-" + p.id, mk, L"Mettre par défaut", Style::Ghost)) {
                selActive_ = p.id;
                ApplyTheme();
            }
        }
    }
    gfx_.Text(L"Ton serveur n'est pas là ? Tu pourras créer ton pack dans l'appli ou le proposer sur GitHub.",
              {a.x, a.b() - 10, a.w, 22}, Font::Small, t.faint);
}

void App::WizBackup(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(L"On sauvegarde tout avant de toucher à quoi que ce soit", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(L"Une copie complète de tes réglages actuels est créée. Tu pourras tout restaurer en un clic.",
              {a.x, a.y + 42, a.w, 24}, Font::Body, t.muted);

    float colW = std::min((a.w - 24) / 2, 430.f);
    Rect left{a.x, a.y + 90, colW, a.h - 100};
    ui_.Panel(left, 16);
    gfx_.Text(L"DÉTECTÉ SUR TON PC", {left.x + 22, left.y + 18, left.w, 20}, Font::Caption, t.faint);
    float y = left.y + 48;
    if (sources_.empty()) {
        ui_.Spinner(left.x + 36, y + 14, 10);
    }
    for (auto& s : sources_) {
        if (y > left.b() - 40) break;
        gfx_.Text(s.exists ? L"" : L"", {left.x + 22, y, 20, 28}, Font::Icon, s.exists ? t.ok : t.faint);
        gfx_.Text(s.label, {left.x + 50, y, left.w - 150, 28}, Font::Small, s.exists ? t.text : t.faint);
        if (s.exists) gfx_.Text(Size(s.bytes), {left.r() - 100, y, 80, 28}, Font::Small, t.muted, Align::Right);
        y += 32;
    }
    gfx_.Text(L"Ton ancien OBS est aussi zippé dans Documents\\Anciens kits OBS.", {left.x + 22, left.b() - 46, left.w - 44, 36},
              Font::Small, t.faint, Align::Left, false, true);

    Rect right{a.x + colW + 24, a.y + 90, colW, a.h - 100};
    ui_.Panel(right, 16);
    gfx_.Text(L"", {right.x + 22, right.y + 20, 30, 30}, Font::Icon, t.accent.lighten(0.3f));
    gfx_.Text(L"Sauvegarde GitHub privée", {right.x + 60, right.y + 18, right.w - 140, 32}, Font::Heading, t.text);
    ui_.Toggle("wz-gh", {right.r() - 64, right.y + 22, 44, 26}, wantGithub_);
    gfx_.Text(L"Un repo privé « slayer-kit-backup » est créé sur ton compte GitHub. Même après un formatage, tu "
              L"récupères tout depuis l'appli. Tes clés de stream et mots de passe ne partent jamais sur GitHub.",
              {right.x + 22, right.y + 60, right.w - 44, 80}, Font::Small, t.muted, Align::Left, false, true);
    if (wantGithub_) DrawGithubConnect({right.x + 22, right.y + 150, right.w - 44, right.h - 170});
}

void App::DrawGithubConnect(const Rect& r) {
    auto& t = ui_.theme;
    switch (gh_) {
    case GhState::Idle:
    case GhState::Error:
        if (gh_ == GhState::Error)
            gfx_.Text(Widen(ghError_), {r.x, r.y, r.w, 44}, Font::Small, t.bad, Align::Left, false, true);
        if (ui_.Button("gh-connect", {r.x, r.y + 50, std::min(r.w, 260.f), 44}, L"Connecter GitHub", Style::Secondary,
                       true, L""))
            StartGithubFlow();
        gfx_.Text(L"Pas de compte ? Crée-le gratuitement sur github.com.", {r.x, r.y + 104, r.w, 22}, Font::Small, t.faint);
        break;
    case GhState::Starting:
        ui_.Spinner(r.x + 14, r.y + 20, 10);
        gfx_.Text(L"Préparation de la connexion…", {r.x + 36, r.y + 6, r.w, 28}, Font::Small, t.muted);
        break;
    case GhState::Waiting: {
        gfx_.Text(L"1. Ouvre github.com/login/device   2. Entre ce code :", {r.x, r.y, r.w, 22}, Font::Small, t.muted);
        Rect code{r.x, r.y + 30, r.w, 64};
        gfx_.Fill(code, t.panel2, 12);
        gfx_.Stroke(code, t.accent.alpha(0.5f), 12);
        gfx_.Text(Widen(ghCode_), code, Font::Code, t.text, Align::Center);
        if (ui_.Button("gh-open", {r.x, r.y + 106, 230, 40}, L"Copier et ouvrir GitHub", Style::Primary, true, L"")) {
            if (OpenClipboard(hwnd_)) {
                EmptyClipboard();
                std::wstring w = Widen(ghCode_);
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
                memcpy(GlobalLock(mem), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(mem);
                SetClipboardData(CF_UNICODETEXT, mem);
                CloseClipboard();
            }
            OpenUrl(Widen(ghUri_));
        }
        ui_.Spinner(r.x + 254, r.y + 126, 9);
        gfx_.Text(L"En attente de ta validation…", {r.x + 272, r.y + 112, r.w - 272, 28}, Font::Small, t.muted);
        break;
    }
    case GhState::Connected:
        gfx_.Circle(r.x + 16, r.y + 16, 14, t.ok.alpha(0.2f));
        ui_.Check(r.x + 16, r.y + 16, 14, t.ok);
        gfx_.Text(L"Connecté : " + Widen(ghLogin_), {r.x + 40, r.y, r.w - 40, 32}, Font::BodyBold, t.text);
        gfx_.Text(L"Repo privé : " + Widen(ghLogin_) + L"/slayer-kit-backup", {r.x + 40, r.y + 28, r.w - 40, 22},
                  Font::Small, t.muted);
        break;
    }
}

void App::StartGithubFlow() {
    gh_ = GhState::Starting;
    ghError_.clear();
    ghCancel_ = false;
    Async([this] {
        auto clientId = RemoteCfg().githubClientId;
        if (clientId.empty()) {
            updater::Check();
            clientId = RemoteCfg().githubClientId;
        }
        auto dc = github::StartDeviceFlow(clientId);
        if (!dc.error.empty()) {
            Post([this, e = dc.error] {
                gh_ = GhState::Error;
                ghError_ = e;
            });
            return;
        }
        Post([this, dc] {
            gh_ = GhState::Waiting;
            ghCode_ = dc.userCode;
            ghUri_ = dc.verificationUri;
        });
        std::string err;
        std::string token = github::PollDeviceFlow(clientId, dc, ghCancel_, &err);
        if (token.empty()) {
            Post([this, err] {
                gh_ = GhState::Error;
                ghError_ = err;
            });
            return;
        }
        auto me = github::Me(token);
        std::string repoErr;
        bool repoOk = me.error.empty() && github::EnsurePrivateRepo(token, me.login, Narrow(k::kBackupRepo), &repoErr);
        Post([this, token, me, repoOk, repoErr] {
            if (!me.error.empty() || !repoOk) {
                gh_ = GhState::Error;
                ghError_ = !me.error.empty() ? me.error : repoErr;
                return;
            }
            Cfg().setGithubToken(token);
            Cfg().githubUser = me.login;
            Cfg().githubBackup = true;
            SaveSettings();
            ghLogin_ = me.login;
            gh_ = GhState::Connected;
            Toast(L"GitHub connecté, repo privé prêt.");
        });
    });
}

void App::PushGithubBackup(const backup::Entry& e) {
    std::string token = Cfg().githubToken();
    std::string user = Cfg().githubUser;
    if (token.empty() || user.empty()) return;
    ghBusy_ = true;
    Async([this, e, token, user] {
        fs::path dir;
        std::string err;
        bool ok = backup::StageForUpload(e, &dir, &err) &&
                  github::PushDirectory(token, user, Narrow(k::kBackupRepo), dir,
                                        "Sauvegarde " + e.stamp + " - " + e.reason,
                                        [this](const std::wstring& what, float p) {
                                            Post([this, what, p] {
                                                ghStatus_ = what;
                                                ghProgress_ = p;
                                            });
                                        },
                                        &err);
        std::error_code ec;
        if (!dir.empty()) fs::remove_all(dir, ec);
        Post([this, ok, err] {
            ghBusy_ = false;
            ghStatus_ = ok ? L"Dernière sauvegarde envoyée sur GitHub." : L"Échec GitHub : " + Widen(err);
            Toast(ok ? L"Sauvegarde envoyée sur ton GitHub privé." : L"Envoi GitHub impossible.", ok);
            if (ok) LoadGithubHistory();
        });
    });
}

void App::WizInstall(const Rect& a) {
    auto& t = ui_.theme;
    gfx_.Text(installDone_ ? L"C'est prêt !" : L"Installation du kit…", {a.x, a.y, a.w, 40}, Font::Title, t.text);
    gfx_.Text(installDone_ ? L"Ton kit est installé. Fais ton test micro dans « Audio & micro », puis « Lancer mon live »."
                           : L"Ne ferme pas l'application.",
              {a.x, a.y + 42, a.w, 24}, Font::Body, t.muted);
    Rect bar{a.x, a.y + 86, std::min(a.w, 760.f), 8};
    ui_.Progress(bar, installProgress_);
    float y = a.y + 120;
    for (auto& s : steps_) {
        float cx = a.x + 14, cy = y + 16;
        if (s.state == 1) ui_.Spinner(cx, cy, 10);
        else if (s.state == 2) {
            gfx_.Circle(cx, cy, 12, t.ok.alpha(0.2f));
            ui_.Check(cx, cy, 12, t.ok);
        } else if (s.state == 3) {
            gfx_.Circle(cx, cy, 12, t.bad.alpha(0.2f));
            gfx_.Text(L"", {cx - 12, cy - 12, 24, 24}, Font::Icon, t.bad, Align::Center);
        } else gfx_.Ring(cx, cy, 10, t.line.alpha(2.5f), 2);
        gfx_.Text(s.label, {a.x + 40, y, 420, 32}, s.state == 1 ? Font::BodyBold : Font::Body,
                  s.state == 0 ? t.faint : t.text);
        if (!s.detail.empty()) gfx_.Text(s.detail, {a.x + 470, y, a.w - 470, 32}, Font::Small, t.muted);
        y += 44;
    }
    if (installDone_ &&
        ui_.Button("wz-finish", {a.x, y + 16, 260, 48}, L"Ouvrir le tableau de bord", Style::Primary, true, L"")) {
        mode_ = Mode::Dashboard;
        section_ = Section::Home;
        Refresh();
    }
}

void App::StartInstall() {
    installing_ = true;
    installDone_ = false;
    steps_ = {{L"Archivage de ton ancien OBS"},
              {L"Sauvegarde complète de tes réglages"},
              {L"Téléchargement des packs serveur"},
              {L"Envoi de la sauvegarde sur GitHub (privé)"},
              {L"Musiques du kit"},
              {L"Configuration d'OBS (scènes, son, décomptes)"},
              {L"Enregistrement de ta configuration"},
              {L"Démarrage avec Windows"}};
    Cfg().platforms = std::set<std::string>(selPlatforms_.begin(), selPlatforms_.end());
    Cfg().software = selSoftware_;
    Cfg().packs.assign(selPacks_.begin(), selPacks_.end());
    Cfg().activePack = selActive_;
    for (auto& m : snap_.music)
        if (m.installed && std::find(Cfg().musicApps.begin(), Cfg().musicApps.end(), m.id) == Cfg().musicApps.end())
            Cfg().musicApps.push_back(m.id);
    SaveSettings();
    auto catalog = catalog_;
    auto snap = snap_;
    auto setStep = [this](size_t i, int state, std::wstring detail = {}) {
        Post([this, i, state, detail] {
            steps_[i].state = state;
            steps_[i].detail = detail;
            int done = 0;
            for (auto& s : steps_) done += s.state >= 2;
            installProgress_ = (float)done / steps_.size();
        });
    };
    std::vector<packs::Pack> toInstall;
    for (auto& id : selPacks_)
        if (auto p = catalog_.find(id)) toInstall.push_back(*p);
    bool github = wantGithub_ && gh_ == GhState::Connected;

    Async([this, setStep, toInstall, github, catalog, snap] {
        std::string err;
        setStep(0, 1);
        bool ok = backup::ArchiveExistingObs(&err);
        setStep(0, ok ? 2 : 3, ok ? L"Documents\\Anciens kits OBS" : Widen(err));

        setStep(1, 1);
        backup::Entry e;
        ok = backup::Create("Installation de Slayer Kit", nullptr, &e, &err);
        setStep(1, ok ? 2 : 3, ok ? e.zip.filename().wstring() : Widen(err));

        setStep(2, 1);
        int okPacks = 0;
        for (auto& p : toInstall) {
            if (p.files.empty() && !catalog_.online) continue;
            std::string perr;
            if (packs::InstallPack(p, nullptr, &perr)) ++okPacks;
            else Log("Pack " + p.id + " : " + perr);
        }
        setStep(2, okPacks == (int)toInstall.size() ? 2 : 3,
                std::to_wstring(okPacks) + L" / " + std::to_wstring(toInstall.size()) + L" installé(s)");

        if (github && ok) {
            setStep(3, 1);
            fs::path dir;
            std::string gerr;
            bool gok = backup::StageForUpload(e, &dir, &gerr) &&
                       github::PushDirectory(Cfg().githubToken(), Cfg().githubUser, Narrow(k::kBackupRepo), dir,
                                             "Installation de Slayer Kit - " + e.stamp, nullptr, &gerr);
            std::error_code ec;
            if (!dir.empty()) fs::remove_all(dir, ec);
            setStep(3, gok ? 2 : 3, gok ? L"Envoyée" : Widen(gerr));
        } else {
            setStep(3, 2, L"Ignoré (activable plus tard)");
        }

        setStep(4, 1);
        std::string merr;
        bool mok = packs::InstallCommon(catalog, &merr);
        setStep(4, mok ? 2 : 3, mok ? L"Démarrage, pause, fin" : Widen(merr));

        setStep(5, 1);
        auto cat2 = packs::LoadCatalog();
        const packs::Pack* active = cat2.find(Cfg().activePack);
        if (obskit::ObsRunning()) {
            setStep(5, 3, L"OBS est ouvert : applique plus tard (Scènes)");
        } else if (!active || !active->installed) {
            setStep(5, 3, L"Pack serveur absent");
        } else {
            auto snapNow = snap;
            auto obs = detect::Find(snap.streaming, "obs");
            if (!obs || !obs->installed) {
                detect::WingetInstall("OBSProject.OBSStudio");
                snapNow = detect::Scan();
            }
            auto r = obskit::Apply(*active, snapNow, kitfiles::Policy::Replace);
            setStep(5, r.ok ? 2 : 3, r.ok ? r.collection : Widen(r.error));
        }

        Post([this, setStep] {
            setStep(6, 1);
            Cfg().firstRunDone = true;
            SaveSettings();
            setStep(6, 2);
            setStep(7, 1);
            SetStartWithWindows(Cfg().startWithWindows);
            setStep(7, 2, Cfg().startWithWindows ? L"Activé" : L"Désactivé");
            Post([this] {
                installing_ = false;
                installDone_ = true;
                installProgress_ = 1;
                Refresh();
            });
        });
    });
}

}
