#pragma once
#include <string>

#include "core/Http.h"

namespace sk::updater {

struct Info {
    bool reachable = false;
    bool available = false;
    bool critical = false;
    std::string version;
    std::string notes;
    std::string installerUrl;
    std::string sha256;
    std::string channel;
    std::string error;
};

int CompareVersions(const std::string& a, const std::string& b);
Info Check();
bool StreamingSoftwareRunning();

bool Download(const Info& info, fs::path* installer, const http::Progress& progress, std::string* error);
bool LaunchInstallerAndExit(const fs::path& installer);

}
