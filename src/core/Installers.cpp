#include "core/Installers.h"

#include <shellapi.h>
#include <shlobj.h>

#include "core/Config.h"
#include "core/Http.h"

namespace sk::installers {

namespace {

bool RunAndWait(const fs::path& exe, const std::wstring& args) {
    SHELLEXECUTEINFOW sei{sizeof sei};
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = exe.c_str();
    sei.lpParameters = args.empty() ? nullptr : args.c_str();
    std::wstring dir = exe.parent_path().wstring();
    sei.lpDirectory = dir.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    WaitForSingleObject(sei.hProcess, 30 * 60 * 1000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return true;
}

}

bool AitumInstalled() {
    std::error_code ec;
    fs::path pd = KnownFolder(FOLDERID_ProgramData) / L"obs-studio" / L"plugins" / L"vertical-canvas";
    fs::path pf = KnownFolder(FOLDERID_ProgramFiles) / L"obs-studio" / L"obs-plugins" / L"64bit" / L"vertical-canvas.dll";
    return fs::exists(pd, ec) || fs::exists(pf, ec);
}

bool InstallAitumVertical(std::string* error) {
    auto r = http::Get(L"https://api.github.com/repos/Aitum/obs-vertical-canvas/releases/latest",
                       {{L"Accept", L"application/vnd.github+json"}});
    std::string url;
    try {
        for (auto& a : json::parse(r.body).value("assets", json::array()))
            if (a.value("name", "") == "vertical-canvas-windows-installer.exe") url = a.value("browser_download_url", "");
    } catch (...) {
    }
    if (url.rfind("https://github.com/Aitum/", 0) != 0) {
        if (error) *error = "Installateur Aitum Vertical introuvable sur GitHub";
        return false;
    }
    fs::path exe = TempDir() / L"vertical-canvas-windows-installer.exe";
    if (!http::Download(Widen(url), exe)) {
        if (error) *error = "Téléchargement d'Aitum Vertical impossible";
        return false;
    }
    if (IsProcessRunning(L"obs64.exe")) {
        if (error) *error = "Ferme OBS avant d'installer le plugin";
        return false;
    }
    return RunAndWait(exe, L"");
}

bool InstallVbCable(std::string* error) {
    std::string url = RemoteCfg().vbCableUrl;
    if (url.rfind("https://download.vb-audio.com/", 0) != 0) {
        if (error) *error = "Lien VB-CABLE invalide";
        return false;
    }
    fs::path zip = TempDir() / L"vbcable.zip", dir = TempDir() / L"vbcable";
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (!http::Download(Widen(url), zip)) {
        if (error) *error = "Téléchargement de VB-CABLE impossible";
        return false;
    }
    fs::create_directories(dir, ec);
    if (RunCommand(L"tar.exe -x -f \"" + zip.wstring() + L"\" -C \"" + dir.wstring() + L"\"").exitCode != 0) {
        if (error) *error = "Archive VB-CABLE illisible";
        return false;
    }
    fs::path setup = dir / L"VBCABLE_Setup_x64.exe";
    if (!fs::exists(setup, ec)) {
        if (error) *error = "Installateur VB-CABLE absent de l'archive";
        return false;
    }
    return RunAndWait(setup, L"");
}

}
