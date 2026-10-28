#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "core/Config.h"

namespace sk::github {

struct DeviceCode {
    std::string deviceCode;
    std::string userCode;
    std::string verificationUri;
    int interval = 5;
    int expiresIn = 900;
    std::string error;
};

DeviceCode StartDeviceFlow(const std::string& clientId);
std::string PollDeviceFlow(const std::string& clientId, const DeviceCode& dc, const std::atomic<bool>& cancel,
                           std::string* error);

struct User {
    std::string login;
    std::string error;
};
User Me(const std::string& token);

bool EnsurePrivateRepo(const std::string& token, const std::string& owner, const std::string& repo, std::string* error);

struct Snapshot {
    std::string sha;
    std::string message;
    std::string date;
};

using Progress = std::function<void(const std::wstring& what, float progress)>;

bool PushDirectory(const std::string& token, const std::string& owner, const std::string& repo, const fs::path& dir,
                   const std::string& message, const Progress& progress, std::string* error);
std::vector<Snapshot> History(const std::string& token, const std::string& owner, const std::string& repo,
                              std::string* error);
bool PullSnapshot(const std::string& token, const std::string& owner, const std::string& repo, const std::string& sha,
                  const fs::path& dest, const Progress& progress, std::string* error);

}
