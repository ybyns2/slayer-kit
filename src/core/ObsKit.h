#pragma once
#include <string>
#include <vector>

#include "core/Detect.h"
#include "core/KitFiles.h"
#include "core/Packs.h"

namespace sk::obskit {

struct SceneDef {
    std::string id;
    std::wstring title;
    std::wstring hint;
    bool canDisable = true;
};

const std::vector<SceneDef>& AllScenes();
std::vector<std::string> OrderedScenes();
std::wstring SceneName(const std::string& id);

fs::path ObsDir();
std::wstring CollectionName(const packs::Pack& p);
std::wstring ProfileName();

struct Result {
    bool ok = false;
    std::string error;
    std::vector<std::wstring> keptUserFiles;
    std::wstring collection;
};

Result Apply(const packs::Pack& pack, const detect::Snapshot& snap, kitfiles::Policy policy);
bool LaunchObs(const packs::Pack& pack, const detect::Snapshot& snap, bool startStreaming = false);
bool ObsRunning();
void SelectInObs(const packs::Pack& pack);

struct LayoutItem {
    std::string id;
    std::wstring label;
    float w, h;
};
const std::vector<LayoutItem>& LayoutItems();
void LayoutPos(const std::string& id, float* x, float* y);

}
