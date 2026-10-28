#pragma once
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::kitfiles {

struct Modified {
    fs::path path;
    std::wstring label;
    bool deleted = false;
    std::vector<std::wstring> details;
};

std::vector<Modified> DetectModified();

enum class Policy { KeepUserChanges, Replace };
bool Install(const fs::path& path, const std::string& content, const std::wstring& label, Policy policy,
             bool* merged = nullptr);
void Forget(const fs::path& path);

std::string MergeJson(const std::string& base, const std::string& next, const std::string& user);
std::string MergeIni(const std::string& base, const std::string& next, const std::string& user);

}
