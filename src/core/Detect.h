#pragma once
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::detect {

struct App {
    std::string id;
    std::wstring name;
    bool installed = false;
    bool running = false;
    fs::path exe;
    std::string wingetId;
};

struct MusicSite {
    std::wstring site;
    std::wstring browser;
};

struct Snapshot {
    std::vector<App> streaming;
    std::vector<App> music;
    std::vector<App> browsers;
    std::vector<MusicSite> musicSites;
    App fivem;
    std::wstring fivemServerTitle;
    bool vbCable = false;
    bool voicemeeter = false;
};

Snapshot Scan();
const App* Find(const std::vector<App>& list, const std::string& id);

bool WingetInstall(const std::string& wingetId, std::string* log = nullptr);
bool WingetAvailable();

}
