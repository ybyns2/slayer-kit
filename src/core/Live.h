#pragma once
#include <string>

#include "core/Packs.h"

namespace sk::live {

fs::path StateFile();

struct State {
    bool live = false;
    long long since = 0;
};
State Read();

bool SendWebhook(const packs::Pack& pack, bool start, long long durationSec, std::string* error);
void PerformanceMode(bool on);
void RecordStats(long long start, long long end, const std::string& pack);

}
