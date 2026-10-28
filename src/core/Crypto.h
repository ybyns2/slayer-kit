#pragma once
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::crypto {

std::string Sha256Hex(const std::vector<unsigned char>& data);
std::string Sha256File(const fs::path& p);

std::string Protect(const std::string& plain);
std::string Unprotect(const std::string& sealed);

} 
