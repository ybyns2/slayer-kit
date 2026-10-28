#pragma once
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Http.h"

namespace sk::packs {

struct Color { float r = 1, g = 1, b = 1; };
Color ParseHex(const std::string& hex, Color fallback = {});

struct PackFile {
    std::string path;
    std::string sha256;
    unsigned long long size = 0;
};

struct Pack {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string discord;
    std::vector<std::string> fivemTitles;
    Color accent{0.93f, 0.18f, 0.30f};
    Color accent2{1.0f, 0.54f, 0.24f};
    Color background{0.06f, 0.05f, 0.09f};
    std::string logo;
    std::vector<PackFile> files;
    bool installed = false;
    std::string installedVersion;
    fs::path localDir;
    fs::path localLogo() const;
};

struct Catalog {
    std::vector<Pack> packs;
    std::vector<PackFile> commonFiles;
    std::string commonVersion;
    json commonMusic = json::object();
    bool online = false;
    std::string error;
    const Pack* find(const std::string& id) const;
};

Catalog LoadCatalog();
bool InstallPack(const Pack& p, const http::Progress& progress, std::string* error);
bool InstallCommon(const Catalog& c, std::string* error);
fs::path CommonDir();
bool CommonInstalled();
fs::path PacksDir();
const Pack* DetectFromFiveMTitle(const Catalog& c, const std::wstring& title);

}
