#pragma once
#include <string>

#include "core/Packs.h"

namespace sk::overlays {

fs::path Dir();
fs::path NowPlayingFile();

std::string KickChatroomId(const std::string& slug, std::string* error);
std::wstring NowPlaying();
void UpdateNowPlayingFile();
bool OpenChatWindow(const std::wstring& url);
std::wstring LocalUrl(const std::string& path, bool window = false);

}
