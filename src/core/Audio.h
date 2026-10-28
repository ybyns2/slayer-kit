#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::audio {

struct Device {
    std::wstring id;
    std::wstring name;
    bool isDefault = false;
    bool capture = true;
};

std::vector<Device> Inputs();
std::vector<Device> Outputs();
float PeakLevel(const std::wstring& deviceId);

struct Phase {
    float rmsDb = -100;
    float peakDb = -100;
};

struct TestResult {
    bool ok = false;
    std::string error;
    Phase silence, voice, shout;
    float gainDb = 0;
    float gateOpenDb = -40, gateCloseDb = -48;
    bool rnnoise = true;
    float compThresholdDb = -18;
    float compRatio = 4;
    int quality = 0;
    std::wstring verdict;
};

using TestStep = std::function<void(int phase, float progress, float liveLevel)>;

bool Record(const std::wstring& deviceId, float seconds, Phase* out, const TestStep& step, int phaseIndex,
            const std::atomic<bool>& cancel);
TestResult Analyse(const Phase& silence, const Phase& voice, const Phase& shout);

}
