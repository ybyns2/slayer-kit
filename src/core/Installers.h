#pragma once
#include <string>

#include "core/Util.h"

namespace sk::installers {

bool AitumInstalled();
bool InstallAitumVertical(std::string* error);
bool InstallVbCable(std::string* error);

}
