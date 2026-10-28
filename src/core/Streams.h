#pragma once
#include <atomic>
#include <functional>
#include <string>

#include "core/Config.h"

namespace sk::streams {

using Emit = std::function<void(const json&)>;

struct DeviceCode {
    std::string deviceCode, userCode, uri, error;
    int interval = 5, expires = 1800;
};

DeviceCode TwitchStart();
bool TwitchPoll(const DeviceCode& dc, const std::atomic<bool>& cancel, std::string* error);
bool TwitchConnected();
void TwitchDisconnect();

void StartAlerts(Emit emit);
void StartKickChat(Emit emit);
void StopAll();

}
