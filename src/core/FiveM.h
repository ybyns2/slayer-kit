#pragma once
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::fivem {

struct Fix {
    std::string key;
    std::string current;
    std::string wanted;
    std::wstring label;
    std::wstring why;
};

fs::path ConfigPath();
std::vector<Fix> Check();
bool Running();
bool Apply(const std::vector<Fix>& fixes, std::string* error);

}
