#pragma once
#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "core/Audio.h"
#include "core/Backup.h"
#include "core/Detect.h"
#include "core/GitHub.h"
#include "core/KitFiles.h"
#include "core/ObsKit.h"
#include "core/Packs.h"
#include "core/Updater.h"
#include "ui/Ui.h"

namespace sk {

inline constexpr UINT WM_APP_RUN = WM_APP + 1;
inline constexpr UINT WM_APP_TRAY = WM_APP + 2;
inline constexpr UINT WM_APP_SHOW = WM_APP + 3;

enum class Mode { Wizard, Dashboard };
enum class Section { Home, Servers, Platforms, Audio, Music, Scenes, Overlays, Extras, Backups, Updates, Settings, About };

struct DialogButton {
    std::wstring label;
    ui::Style style = ui::Style::Secondary;
    std::function<void()> action;
};

struct Dialog {
    std::wstring icon;
    std::wstring title;
    std::wstring body;
    std::vector<std::wstring> list;
    std::vector<DialogButton> buttons;
    bool hasCheck = false;
    std::wstring checkLabel;
    bool checkValue = false;
    std::function<void(bool)> onCheck;
};

struct InstallStep {
    std::wstring label;
    int state = 0;
    std::wstring detail;
};

enum class GhState { Idle, Starting, Waiting, Connected, Error };

class App {
public:
    int Run(HINSTANCE inst, bool startHidden);

    void Post(std::function<void()> fn);
    void Async(std::function<void()> work);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);
    void Paint();
    void Frame();
    void StartAnimTimer();

    void DrawBackground();
    void DrawTitleBar();
    void DrawDialog();

    void DrawWizard();
    void WizWelcome(const ui::Rect& area);
    void WizPlatforms(const ui::Rect& area);
    void WizSoftware(const ui::Rect& area);
    void WizServers(const ui::Rect& area);
    void WizBackup(const ui::Rect& area);
    void WizInstall(const ui::Rect& area);
    void StartInstall();

    void DrawDashboard();
    void DashHome(const ui::Rect& area);
    void DashServers(const ui::Rect& area);
    void DashBackups(const ui::Rect& area);
    void DashAudio(const ui::Rect& area);
    void DashPlatforms(const ui::Rect& area);
    void DashOverlays(const ui::Rect& area);
    void DashExtras(const ui::Rect& area);
    void LiveTick();
    void StartServices();
    std::wstring RemoteUrl() const;
    void DrawQr(const ui::Rect& r, const std::wstring& text);
    void DrawRemoteCard(const ui::Rect& r);
    void DrawAlertsCard(const ui::Rect& r);
    void DrawLayoutEditor(const ui::Rect& r);
    void DashMusic(const ui::Rect& area);
    void DashScenes(const ui::Rect& area);
    void StartMicTest();
    void StartMeter();
    void ApplyKit(bool launchAfter, bool startStreaming = false);
    void LaunchLive();
    std::wstring PickAudioFile();
    void DashUpdates(const ui::Rect& area);
    void DashSettings(const ui::Rect& area);
    void DashAbout(const ui::Rect& area);

    void DrawGithubConnect(const ui::Rect& r);
    void StartGithubFlow();
    void PushGithubBackup(const backup::Entry& e);
    void LoadGithubHistory();

    void Refresh();
    void ApplyTheme();
    void CheckUpdates(bool manual);
    void OfferUpdate();
    void AskAboutModifications();
    void InstallUpdate();
    void CriticalWatch();
    void AutoDetectServer();

    void ShowDialog(Dialog d) { dialog_ = std::move(d); dialogOpen_ = true; }
    void CloseDialog() { dialogOpen_ = false; }
    void Toast(const std::wstring& text, bool good = true);

    void TrayAdd();
    void TrayRemove();
    void TrayMenu();
    void TrayBalloon(const std::wstring& title, const std::wstring& text);
    void ShowWindowFront();
    void HideToTray();
    void SetStartWithWindows(bool on);

    const packs::Pack* ActivePack() const;

    HINSTANCE inst_ = nullptr;
    HWND hwnd_ = nullptr;
    ui::Gfx gfx_;
    ui::Ui ui_{gfx_};
    LARGE_INTEGER freq_{}, last_{};
    bool timerOn_ = false;
    bool visible_ = true;
    bool tracking_ = false;
    NOTIFYICONDATAW tray_{};

    std::mutex qMutex_;
    std::deque<std::function<void()>> queue_;

    Mode mode_ = Mode::Wizard;
    Section section_ = Section::Home;
    int wizStep_ = 0;
    float pageAnim_ = 1;
    int lastPageKey_ = -1;

    detect::Snapshot snap_;
    bool scanning_ = false;
    packs::Catalog catalog_;
    bool catalogLoading_ = false;
    std::vector<backup::Source> sources_;
    std::vector<backup::Entry> backups_;
    std::vector<kitfiles::Modified> modified_;

    std::set<std::string> selPlatforms_;
    std::string selSoftware_;
    std::set<std::string> selPacks_;
    std::string selActive_;
    bool wantGithub_ = true;

    std::vector<InstallStep> steps_;
    float installProgress_ = 0;
    bool installing_ = false;
    bool installDone_ = false;

    GhState gh_ = GhState::Idle;
    std::string ghCode_, ghUri_, ghLogin_, ghError_;
    std::atomic<bool> ghCancel_{false};
    std::vector<github::Snapshot> ghHistory_;
    bool ghBusy_ = false;
    std::wstring ghStatus_;
    float ghProgress_ = 0;

    updater::Info update_;
    bool updateChecking_ = false;
    bool updateBusy_ = false;
    float updateProgress_ = 0;
    std::wstring updateStatus_;
    bool criticalWaiting_ = false;

    bool backupBusy_ = false;
    std::wstring backupStatus_;
    float backupProgress_ = 0;

    Dialog dialog_;
    bool dialogOpen_ = false;

    std::wstring toast_;
    bool toastGood_ = true;
    double toastUntil_ = 0;

    std::string detectedServer_;

    std::vector<audio::Device> mics_;
    std::atomic<float> meterLevel_{0};
    std::atomic<bool> meterRun_{false};
    std::string meterDevice_;
    int micPhase_ = -1;
    float micPhaseProgress_ = 0;
    float micLive_ = -100;
    std::atomic<bool> micCancel_{false};
    audio::TestResult micResult_;
    bool micHasResult_ = false;
    bool kitBusy_ = false;
    std::wstring kitStatus_;
    bool installingApp_ = false;
    bool wasLive_ = false;
    long long liveStart_ = 0;
    int twState_ = 0;
    bool layoutEdit_ = false;
    std::string dragItem_;
    float dragDx_ = 0, dragDy_ = 0;
    std::string twCode_;
    std::atomic<bool> twCancel_{false};
};

App& TheApp();

}
