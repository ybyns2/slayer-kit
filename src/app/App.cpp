#include "app/App.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <thread>

#include "core/Config.h"
#include "core/Constants.h"
#include "core/FiveM.h"
#include "core/Server.h"
#include "core/Streams.h"

namespace sk {

using namespace ui;

namespace {
constexpr float kTitleH = 46;
constexpr UINT_PTR kAnimTimer = 1;
constexpr UINT_PTR kWatchTimer = 2;
constexpr UINT_PTR kDetectTimer = 3;
constexpr UINT_PTR kLiveTimer = 4;
const wchar_t* kClass = L"SlayerKitWindow";
}

App& TheApp() {
    static App app;
    return app;
}

void App::Post(std::function<void()> fn) {
    {
        std::lock_guard lock(qMutex_);
        queue_.push_back(std::move(fn));
    }
    if (hwnd_) PostMessageW(hwnd_, WM_APP_RUN, 0, 0);
}

void App::Async(std::function<void()> work) {
    std::thread([w = std::move(work)] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        try {
            w();
        } catch (const std::exception& e) {
            Log(std::string("Erreur tache : ") + e.what());
        }
        CoUninitialize();
    }).detach();
}

const packs::Pack* App::ActivePack() const {
    std::string id = mode_ == Mode::Wizard && !selActive_.empty() ? selActive_ : Cfg().activePack;
    return catalog_.find(id);
}

void App::ApplyTheme() {
    Theme t;
    if (auto p = ActivePack()) {
        t.accent = {p->accent.r, p->accent.g, p->accent.b, 1};
        t.accent2 = {p->accent2.r, p->accent2.g, p->accent2.b, 1};
        Color bg{p->background.r, p->background.g, p->background.b, 1};
        t.bg = bg;
        t.panel = bg.lighten(0.045f);
        t.panel2 = bg.lighten(0.08f);
    }
    ui_.SetTheme(t);
}

void App::Toast(const std::wstring& text, bool good) {
    toast_ = text;
    toastGood_ = good;
    toastUntil_ = ui_.time + 4.0;
    StartAnimTimer();
}

void App::Refresh() {
    if (!scanning_) {
        scanning_ = true;
        Async([this] {
            auto s = detect::Scan();
            auto src = backup::Sources();
            auto list = backup::List();
            auto mods = kitfiles::DetectModified();
            Post([this, s = std::move(s), src = std::move(src), list = std::move(list), mods = std::move(mods)] {
                snap_ = s;
                sources_ = src;
                backups_ = list;
                modified_ = mods;
                scanning_ = false;
                AutoDetectServer();
                if (selSoftware_.empty()) {
                    for (auto& a : snap_.streaming)
                        if (a.running || a.installed) {
                            selSoftware_ = a.id;
                            break;
                        }
                    if (selSoftware_.empty()) selSoftware_ = "obs";
                }
            });
        });
    }
    if (!catalogLoading_) {
        catalogLoading_ = true;
        Async([this] {
            auto c = packs::LoadCatalog();
            Post([this, c = std::move(c)] {
                catalog_ = c;
                catalogLoading_ = false;
                AutoDetectServer();
                ApplyTheme();
            });
        });
    }
}

void App::AutoDetectServer() {
    if (snap_.fivemServerTitle.empty() || catalog_.packs.empty()) return;
    if (auto p = packs::DetectFromFiveMTitle(catalog_, snap_.fivemServerTitle)) {
        detectedServer_ = p->id;
        if (mode_ == Mode::Wizard && selPacks_.empty()) {
            selPacks_.insert(p->id);
            selActive_ = p->id;
        }
        if (mode_ == Mode::Dashboard && Cfg().autoDetectServer && Cfg().activePack != p->id &&
            std::find(Cfg().packs.begin(), Cfg().packs.end(), p->id) != Cfg().packs.end()) {
            Cfg().activePack = p->id;
            SaveSettings();
            Toast(L"Serveur détecté dans FiveM : " + Widen(p->name) + L". Thème appliqué.");
        }
        ApplyTheme();
    }
}

void App::StartAnimTimer() {
    if (!timerOn_ && hwnd_) {
        SetTimer(hwnd_, kAnimTimer, 15, nullptr);
        timerOn_ = true;
    }
}

int App::Run(HINSTANCE inst, bool startHidden) {
    inst_ = inst;
    LoadSettings();
    mode_ = Cfg().firstRunDone ? Mode::Dashboard : Mode::Wizard;
#ifdef SK_DEV
    if (auto c = wcsstr(GetCommandLineW(), L"--step=")) {
        mode_ = Mode::Wizard;
        wizStep_ = _wtoi(c + 7);
    }
    if (auto c = wcsstr(GetCommandLineW(), L"--dash=")) {
        mode_ = Mode::Dashboard;
        section_ = (Section)_wtoi(c + 7);
    }
#endif
    for (auto& p : Cfg().platforms) selPlatforms_.insert(p);
    selSoftware_ = Cfg().firstRunDone ? Cfg().software : "";
    for (auto& p : Cfg().packs) selPacks_.insert(p);
    selActive_ = Cfg().activePack;

    WNDCLASSEXW wc{sizeof wc};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    UINT dpi = GetDpiForSystem();
    int w = MulDiv(1180, dpi, 96), h = MulDiv(760, dpi, 96);
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + (work.right - work.left - w) / 2, y = work.top + (work.bottom - work.top - h) / 2;
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kClass, L"Slayer Kit", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr,
                            inst, this);
    if (!hwnd_) return 1;

    MARGINS m{0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(hwnd_, &m);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof dark);
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof corner);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    gfx_.Init(hwnd_);
    QueryPerformanceFrequency(&freq_);
    QueryPerformanceCounter(&last_);
    ApplyTheme();
    TrayAdd();

    if (startHidden && Cfg().firstRunDone) {
        visible_ = false;
    } else {
#ifdef SK_DEV
        if (wcsstr(GetCommandLineW(), L"--offscreen")) {
            SetWindowPos(hwnd_, HWND_BOTTOM, -6000, 100, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        } else
#endif
            ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        StartAnimTimer();
    }

    Refresh();
    StartServices();
    CheckUpdates(false);
    SetTimer(hwnd_, kDetectTimer, 20000, nullptr);
    SetTimer(hwnd_, kLiveTimer, 3000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    TrayRemove();
    return (int)msg.wParam;
}

LRESULT CALLBACK App::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        self = reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        self->hwnd_ = h;
    }
    return self ? self->Handle(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT App::Handle(UINT m, WPARAM w, LPARAM l) {
    float scale = 96.f / gfx_.Dpi();
    switch (m) {
    case WM_NCCALCSIZE:
        if (w) {
            if (IsZoomed(hwnd_)) {
                auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);
                UINT dpi = GetDpiForWindow(hwnd_);
                int fx = GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                int fy = GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                p->rgrc[0].left += fx;
                p->rgrc[0].right -= fx;
                p->rgrc[0].top += fy;
                p->rgrc[0].bottom -= fy;
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(hwnd_, &pt);
        RECT rc;
        GetClientRect(hwnd_, &rc);
        int b = MulDiv(6, (int)gfx_.Dpi(), 96);
        if (!IsZoomed(hwnd_)) {
            bool L = pt.x < b, R = pt.x >= rc.right - b, T = pt.y < b, B = pt.y >= rc.bottom - b;
            if (T && L) return HTTOPLEFT;
            if (T && R) return HTTOPRIGHT;
            if (B && L) return HTBOTTOMLEFT;
            if (B && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT;
            if (R) return HTRIGHT;
            if (T) return HTTOP;
            if (B) return HTBOTTOM;
        }
        float x = pt.x * scale, y = pt.y * scale;
        if (y < kTitleH && x < gfx_.Width() - 100 && !dialogOpen_) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(l);
        UINT dpi = hwnd_ ? GetDpiForWindow(hwnd_) : 96;
        mm->ptMinTrackSize = {MulDiv(1020, dpi, 96), MulDiv(680, dpi, 96)};
        return 0;
    }
    case WM_SIZE:
        gfx_.Resize(LOWORD(l), HIWORD(l));
        if (w == SIZE_MINIMIZED) {
            KillTimer(hwnd_, kAnimTimer);
            timerOn_ = false;
        } else {
            InvalidateRect(hwnd_, nullptr, FALSE);
            StartAnimTimer();
        }
        return 0;
    case WM_DPICHANGED: {
        gfx_.SetDpi((float)HIWORD(w));
        auto* r = reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_TIMER:
        if (w == kAnimTimer) InvalidateRect(hwnd_, nullptr, FALSE);
        else if (w == kWatchTimer) CriticalWatch();
        else if (w == kLiveTimer) LiveTick();
        else if (w == kDetectTimer) {
            if (mode_ == Mode::Dashboard) Refresh();
            if (Cfg().fivemPending && !fivem::Running()) {
                std::string err;
                auto fixes = fivem::Check();
                if (fixes.empty() || fivem::Apply(fixes, &err)) {
                    Cfg().fivemPending = false;
                    SaveSettings();
                    if (!fixes.empty()) TrayBalloon(L"Réglages FiveM appliqués", L"Ton jeu est prêt pour le live.");
                }
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        ui_.in.mx = GET_X_LPARAM(l) * scale;
        ui_.in.my = GET_Y_LPARAM(l) * scale;
        if (!tracking_) {
            TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&t);
            tracking_ = true;
        }
        StartAnimTimer();
        return 0;
    case WM_MOUSELEAVE:
        tracking_ = false;
        if (!ui_.in.down) ui_.in.mx = ui_.in.my = -1;
        StartAnimTimer();
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);
        ui_.in.down = ui_.in.pressed = true;
        ui_.in.mx = ui_.in.px = GET_X_LPARAM(l) * scale;
        ui_.in.my = ui_.in.py = GET_Y_LPARAM(l) * scale;
        StartAnimTimer();
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        ui_.in.mx = GET_X_LPARAM(l) * scale;
        ui_.in.my = GET_Y_LPARAM(l) * scale;
        ui_.in.down = false;
        ui_.in.released = true;
        StartAnimTimer();
        return 0;
    case WM_CHAR: {
        wchar_t ch = (wchar_t)w;
        if (ch == 8) ++ui_.in.backspaces;
        else if (ch == 13) ui_.in.enter = true;
        else if (ch == 27) ui_.in.escape = true;
        else if (ch == 1) ui_.in.selectAll = true;
        else if (ch == 22) {
            if (OpenClipboard(hwnd_)) {
                if (HANDLE hd = GetClipboardData(CF_UNICODETEXT)) {
                    if (auto* p = static_cast<const wchar_t*>(GlobalLock(hd))) {
                        std::wstring clip = p;
                        clip.erase(std::remove_if(clip.begin(), clip.end(), [](wchar_t c) { return c == 13 || c == 10; }),
                                   clip.end());
                        ui_.in.chars += clip;
                        GlobalUnlock(hd);
                    }
                }
                CloseClipboard();
            }
        } else if (ch >= 32) ui_.in.chars.push_back(ch);
        StartAnimTimer();
        return 0;
    }
    case WM_MOUSEWHEEL:
        ui_.in.wheel += GET_WHEEL_DELTA_WPARAM(w) / 120.f;
        StartAnimTimer();
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, ui_.wantsCursorHand ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_ACTIVATE:
        StartAnimTimer();
        break;
    case WM_APP_RUN: {
        std::deque<std::function<void()>> q;
        {
            std::lock_guard lock(qMutex_);
            q.swap(queue_);
        }
        for (auto& fn : q) fn();
        StartAnimTimer();
        return 0;
    }
    case WM_APP_SHOW:
        ShowWindowFront();
        return 0;
    case WM_APP_TRAY:
        if (LOWORD(l) == WM_LBUTTONUP || LOWORD(l) == NIN_BALLOONUSERCLICK) ShowWindowFront();
        else if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_CONTEXTMENU) TrayMenu();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case 1: ShowWindowFront(); break;
        case 2: CheckUpdates(true); ShowWindowFront(); break;
        case 3: DestroyWindow(hwnd_); break;
        }
        return 0;
    case WM_CLOSE:
        if (Cfg().minimizeToTray && Cfg().firstRunDone) {
            HideToTray();
            return 0;
        }
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        ghCancel_ = true;
        twCancel_ = true;
        streams::StopAll();
        server::Stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

void App::Paint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);
    EndPaint(hwnd_, &ps);
    if (!visible_ || IsIconic(hwnd_)) return;
    Frame();
}

void App::Frame() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - last_.QuadPart) / (float)freq_.QuadPart;
    last_ = now;

    if (!gfx_.Begin()) return;
    ui_.BeginFrame(dt);
    gfx_.Clear(ui_.theme.bg);
    DrawBackground();

    ui_.Block(dialogOpen_);
    int key = mode_ == Mode::Wizard ? wizStep_ : 100 + (int)section_;
    if (key != lastPageKey_) {
        lastPageKey_ = key;
        ui_.Snap("page", 0);
    }
    pageAnim_ = ui_.Anim("page", 1.f, 9);

    if (mode_ == Mode::Wizard) DrawWizard();
    else DrawDashboard();

    ui_.Block(false);
    DrawTitleBar();
    if (dialogOpen_ || ui_.Anim("dialog", 0) > 0.01f) DrawDialog();

    if (!toast_.empty()) {
        float a = ui_.Anim("toast", ui_.time < toastUntil_ ? 1.f : 0.f, 10);
        if (a > 0.01f) {
            float w = std::min(gfx_.MeasureWidth(toast_, Font::BodyBold) + 70, gfx_.Width() - 40);
            Rect r{(gfx_.Width() - w) / 2, gfx_.Height() - 70 + 20 * (1 - a), w, 46};
            gfx_.PushOpacity(a);
            gfx_.Glow(r, Color{0, 0, 0, 1}, 23, 18);
            gfx_.Fill(r, ui_.theme.panel2.lighten(0.05f), 23);
            gfx_.Stroke(r, (toastGood_ ? ui_.theme.ok : ui_.theme.bad).alpha(0.5f), 23);
            gfx_.Text(toastGood_ ? L"" : L"", {r.x + 18, r.y, 20, r.h}, Font::Icon,
                      toastGood_ ? ui_.theme.ok : ui_.theme.bad);
            gfx_.Text(toast_, {r.x + 46, r.y, r.w - 60, r.h}, Font::BodyBold, ui_.theme.text);
            gfx_.PopOpacity();
        } else if (ui_.time >= toastUntil_) toast_.clear();
    }

    ui_.EndFrame();
    gfx_.End();

    bool foreground = GetForegroundWindow() == hwnd_;
    if (!ui_.animating && !foreground && timerOn_) {
        KillTimer(hwnd_, kAnimTimer);
        timerOn_ = false;
    }
}

void App::DrawBackground() {
    auto& t = ui_.theme;
    float W = gfx_.Width(), H = gfx_.Height();
    bool fg = GetForegroundWindow() == hwnd_;
    double tt = ui_.time * 0.12;
    float ax = W * (0.78f + 0.08f * (float)std::sin(tt)), ay = H * (0.18f + 0.06f * (float)std::cos(tt * 1.3));
    float bx = W * (0.15f + 0.07f * (float)std::cos(tt * 0.9)), by = H * (0.92f + 0.05f * (float)std::sin(tt * 1.1));
    gfx_.FillRadial(ax, ay, W * 0.55f, H * 0.75f, t.accent.alpha(0.17f), t.accent.alpha(0));
    gfx_.FillRadial(bx, by, W * 0.5f, H * 0.6f, t.accent2.alpha(0.10f), t.accent2.alpha(0));
    for (int i = 0; i < 7; ++i) {
        float x = W * 0.55f + i * 70.f;
        gfx_.Line(x, H, x + H * 0.45f, 0, Color{1, 1, 1, 0.012f}, 1);
    }
    if (fg) ui_.animating = true;
}

void App::DrawTitleBar() {
    auto& t = ui_.theme;
    float W = gfx_.Width();
    gfx_.Fill({0, 0, W, kTitleH}, t.bg.alpha(0.35f));
    gfx_.Line(0, kTitleH, W, kTitleH, t.line);
    gfx_.DrawImage(gfx_.ImageFromResource(10), {14, 10, 26, 26});
    gfx_.Text(L"SLAYER KIT", {48, 0, 120, kTitleH}, Font::Heading, t.text);
    gfx_.Text(L"by yns_bnl", {146, 1, 100, kTitleH}, Font::Small, t.faint);
    if (auto p = ActivePack()) {
        std::wstring name = Widen(p->name);
        float w = gfx_.MeasureWidth(name, Font::Caption) + 26;
        Rect pill{240, 13, w, 20};
        gfx_.Fill(pill, t.accent.alpha(0.16f), 10);
        gfx_.Circle(pill.x + 10, pill.y + 10, 3.5f, t.accent);
        gfx_.Text(name, {pill.x + 18, pill.y, w - 18, pill.h}, Font::Caption, t.accent.lighten(0.35f));
    }
    Rect close{W - 50, 0, 50, kTitleH}, min{W - 100, 0, 50, kTitleH};
    if (ui_.IconButton("tb-min", min, L"", Color{1, 1, 1, 0.08f}, t.muted)) ShowWindow(hwnd_, SW_MINIMIZE);
    if (ui_.IconButton("tb-close", close, L"", Color::Hex(0xE81123, 0.9f), t.muted)) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void App::DrawDialog() {
    auto& t = ui_.theme;
    float a = ui_.Anim("dialog", dialogOpen_ ? 1.f : 0.f, 14);
    float W = gfx_.Width(), H = gfx_.Height();
    gfx_.Fill({0, 0, W, H}, Color{0, 0, 0, 0.55f * a});
    if (a < 0.02f) return;

    float w = 520;
    float bodyH = gfx_.MeasureHeight(dialog_.body, Font::Body, w - 64);
    float listH = dialog_.list.empty() ? 0 : std::min<float>(160, 26.f * dialog_.list.size()) + 16;
    float h = 92 + bodyH + listH + (dialog_.hasCheck ? 40 : 0) + 84;
    Rect r{(W - w) / 2, (H - h) / 2 + 16 * (1 - a), w, h};
    gfx_.PushOpacity(a);
    gfx_.Glow(r, t.accent, 20, 26);
    gfx_.Fill(r, t.panel, 20);
    gfx_.Stroke(r, t.accent.alpha(0.35f), 20);
    gfx_.FillGradient({r.x, r.y, r.w, 4}, t.accent, t.accent2, false, 2);

    float y = r.y + 30;
    if (!dialog_.icon.empty()) {
        gfx_.Circle(r.x + 50, y + 18, 20, t.accent.alpha(0.18f));
        gfx_.Text(dialog_.icon, {r.x + 30, y - 2, 40, 40}, Font::Icon, t.accent.lighten(0.3f), Align::Center);
    }
    gfx_.Text(dialog_.title, {r.x + 84, y, r.w - 110, 36}, Font::Heading, t.text);
    y += 56;
    gfx_.Text(dialog_.body, {r.x + 32, y, r.w - 64, bodyH + 4}, Font::Body, t.muted, Align::Left, false, true);
    y += bodyH + 12;
    if (!dialog_.list.empty()) {
        Rect lr{r.x + 32, y, r.w - 64, listH - 12};
        gfx_.Fill(lr, t.panel2, 10);
        gfx_.PushClip(lr, 10);
        float ly = lr.y + 6;
        for (auto& item : dialog_.list) {
            gfx_.Circle(lr.x + 16, ly + 13, 3, t.accent2);
            gfx_.Text(item, {lr.x + 28, ly, lr.w - 36, 26}, Font::Small, t.text);
            ly += 26;
        }
        gfx_.PopClip();
        y += listH;
    }
    if (dialog_.hasCheck) {
        ui_.Block(false);
        if (ui_.Toggle("dlg-check", {r.x + 32, y, r.w - 64, 30}, dialog_.checkValue) && dialog_.onCheck)
            dialog_.onCheck(dialog_.checkValue);
        gfx_.Text(dialog_.checkLabel, {r.x + 84, y, r.w - 120, 30}, Font::Small, t.text);
        y += 40;
    }

    ui_.Block(false);
    float bx = r.r() - 32;
    for (int i = (int)dialog_.buttons.size() - 1; i >= 0; --i) {
        auto& b = dialog_.buttons[i];
        float bw = std::max(110.f, gfx_.MeasureWidth(b.label, Font::BodyBold) + 40);
        bx -= bw;
        if (ui_.Button("dlg-b" + std::to_string(i), {bx, r.b() - 70, bw, 42}, b.label, b.style) && dialogOpen_) {
            auto action = b.action;
            CloseDialog();
            if (action) action();
        }
        bx -= 10;
    }
    gfx_.PopOpacity();
}

void App::TrayAdd() {
    tray_.cbSize = sizeof tray_;
    tray_.hWnd = hwnd_;
    tray_.uID = 1;
    tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = WM_APP_TRAY;
    tray_.hIcon = (HICON)LoadImageW(inst_, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON), 0);
    wcscpy_s(tray_.szTip, L"Slayer Kit by yns_bnl");
    Shell_NotifyIconW(NIM_ADD, &tray_);
    tray_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &tray_);
}

void App::TrayRemove() { Shell_NotifyIconW(NIM_DELETE, &tray_); }

void App::TrayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"Ouvrir Slayer Kit");
    AppendMenuW(m, MF_STRING, 2, L"Vérifier les mises à jour");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 3, L"Quitter");
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, p.x, p.y, 0, hwnd_, nullptr);
    DestroyMenu(m);
}

void App::TrayBalloon(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW n = tray_;
    n.uFlags = NIF_INFO;
    wcsncpy_s(n.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
    n.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    n.hBalloonIcon = tray_.hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

void App::ShowWindowFront() {
    visible_ = true;
    ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd_);
    StartAnimTimer();
    Refresh();
}

void App::HideToTray() {
    visible_ = false;
    ShowWindow(hwnd_, SW_HIDE);
    KillTimer(hwnd_, kAnimTimer);
    timerOn_ = false;
    static bool told = false;
    if (!told) {
        told = true;
        TrayBalloon(L"Slayer Kit reste actif", L"Il tourne près de l'horloge : détection du serveur et mises à jour.");
    }
}

void App::SetStartWithWindows(bool on) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (on) {
        std::wstring cmd = L"schtasks /Create /F /TN \"SlayerKit\" /SC ONLOGON /RL HIGHEST /TR \"\\\"" +
                           std::wstring(exe) + L"\\\" --tray\"";
        Async([cmd] { RunCommand(cmd, 30000); });
    } else {
        Async([] { RunCommand(L"schtasks /Delete /F /TN \"SlayerKit\"", 30000); });
    }
}

void App::CheckUpdates(bool manual) {
    if (updateChecking_) return;
    updateChecking_ = true;
    Async([this, manual] {
        auto info = updater::Check();
        Post([this, info, manual] {
            update_ = info;
            updateChecking_ = false;
            if (!info.reachable) {
                if (manual) Toast(L"Vérification impossible : " + Widen(info.error), false);
                return;
            }
            if (!info.available) {
                if (manual) Toast(L"Tu as la dernière version (" SK_WIDEN_VERSION L").");
                return;
            }
            if (info.critical) {
                Cfg().pendingCriticalVersion = info.version;
                SaveSettings();
                if (updater::StreamingSoftwareRunning()) {
                    criticalWaiting_ = true;
                    SetTimer(hwnd_, kWatchTimer, 30000, nullptr);
                    TrayBalloon(L"Mise à jour critique à faire",
                                L"Elle sera appliquée dès que vous couperez votre live.");
                    Toast(L"Mise à jour critique : appliquée dès la fin de ton live.", false);
                } else {
                    InstallUpdate();
                }
                return;
            }
            if (!manual && Cfg().skippedVersion == info.version) return;
            if (!visible_) TrayBalloon(L"Mise à jour disponible", L"Slayer Kit " + Widen(info.version) + L" est prêt.");
            OfferUpdate();
        });
    });
}

void App::OfferUpdate() {
    Dialog d;
    d.icon = L"";
    d.title = L"Une mise à jour est disponible";
    d.body = L"Slayer Kit " + Widen(update_.version) + L" est prêt (tu as " SK_WIDEN_VERSION L").\n\n" +
             (update_.notes.empty() ? L"" : Widen(update_.notes) + L"\n\n") + L"Voulez-vous la faire ?";
    d.buttons = {{L"Non", Style::Ghost,
                  [this] {
                      Cfg().skippedVersion = update_.version;
                      SaveSettings();
                  }},
                 {L"Oui, mettre à jour", Style::Primary, [this] { AskAboutModifications(); }}};
    ShowDialog(std::move(d));
}

void App::AskAboutModifications() {
    modified_ = kitfiles::DetectModified();
    if (modified_.empty() || Cfg().alwaysKeepModifications) {
        InstallUpdate();
        return;
    }
    Dialog d;
    d.icon = L"";
    d.title = L"Tu as modifié ton kit";
    d.body = L"Je détecte des modifications sur votre interface et vos paramètres de live. "
             L"Voulez-vous les garder (recommandé) ou les retirer pour repartir sur la version du kit ?";
    for (auto& m : modified_)
        d.list.push_back(m.label + (m.deleted ? L"  (supprimé)" : m.details.empty() ? L"" : L" : " + m.details.front()));
    d.hasCheck = true;
    d.checkLabel = L"Toujours garder mes modifications (ne plus demander)";
    d.checkValue = false;
    d.onCheck = [](bool v) {
        Cfg().alwaysKeepModifications = v;
        SaveSettings();
    };
    d.buttons = {{L"Retirer mes modifs", Style::Danger,
                  [this] {
                      WriteFileText(LocalDataDir() / L"pending-policy.txt", "replace");
                      InstallUpdate();
                  }},
                 {L"Garder mes modifs + update", Style::Primary, [this] {
                      WriteFileText(LocalDataDir() / L"pending-policy.txt", "keep");
                      InstallUpdate();
                  }}};
    ShowDialog(std::move(d));
}

void App::InstallUpdate() {
    if (updateBusy_) return;
    updateBusy_ = true;
    updateStatus_ = L"Sauvegarde avant mise à jour…";
    section_ = Section::Updates;
    Async([this] {
        backup::Entry e;
        backup::Create("Avant mise a jour " + update_.version, nullptr, &e, nullptr);
        Post([this] { updateStatus_ = L"Téléchargement de la mise à jour…"; });
        fs::path installer;
        std::string err;
        bool ok = updater::Download(update_, &installer,
                                    [this](auto d, auto t) {
                                        float p = t ? (float)d / t : 0;
                                        Post([this, p] { updateProgress_ = p; });
                                    },
                                    &err);
        Post([this, ok, installer, err] {
            if (!ok) {
                updateBusy_ = false;
                updateStatus_.clear();
                Toast(Widen(err), false);
                return;
            }
            updateStatus_ = L"Installation… Slayer Kit va redémarrer.";
            Cfg().pendingCriticalVersion.clear();
            SaveSettings();
            if (updater::LaunchInstallerAndExit(installer)) DestroyWindow(hwnd_);
            else {
                updateBusy_ = false;
                Toast(L"Lancement de l'installateur refusé.", false);
            }
        });
    });
}

void App::CriticalWatch() {
    if (!criticalWaiting_) return;
    if (updater::StreamingSoftwareRunning()) return;
    criticalWaiting_ = false;
    KillTimer(hwnd_, kWatchTimer);
    TrayBalloon(L"Live terminé", L"Application de la mise à jour critique…");
    InstallUpdate();
}

}
