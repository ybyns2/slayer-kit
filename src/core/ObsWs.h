#pragma once
#include <string>

#include "core/Config.h"

namespace sk::obsws {

void EnableServer();
json Request(const std::string& type, const json& data = json::object(), std::string* error = nullptr);

}
