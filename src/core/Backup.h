#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::backup {

struct Source {
    std::string id;
    std::wstring label;
    fs::path path;
    bool exists = false;
    unsigned long long bytes = 0;
};

struct Entry {
    fs::path zip;
    std::string stamp;
    std::string reason;
    unsigned long long bytes = 0;
};

using Step = std::function<void(const std::wstring& what, float progress)>;

std::vector<Source> Sources();
fs::path LocalBackupDir();
fs::path OldObsKitsDir();

bool IsSecretFile(const fs::path& relative);

bool Create(const std::string& reason, const Step& step, Entry* out, std::string* error);
std::vector<Entry> List();
void Rotate(int keep);

bool StageForUpload(const Entry& e, fs::path* stagingDir, std::string* error);
bool Restore(const fs::path& zip, const Step& step, std::string* error);

bool ArchiveExistingObs(std::string* error);
bool ZipDir(const fs::path& dir, const fs::path& zip);

}
