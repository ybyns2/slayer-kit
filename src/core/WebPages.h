#pragma once
#include <string>

#include "core/Packs.h"

namespace sk::web {

struct Theme {
    std::string accent, accent2, background, name, logoUrl;
};

Theme FromPack(const packs::Pack* p);
std::string ChatPage(const Theme& t, const std::string& token);
std::string AlertsPage(const Theme& t, const std::string& token);
std::string RemotePage(const Theme& t, const std::string& token);

}
