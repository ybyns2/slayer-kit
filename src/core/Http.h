#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/Util.h"

namespace sk::http {

struct Response {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;
    bool ok() const { return status >= 200 && status < 300; }
};

using Headers = std::vector<std::pair<std::wstring, std::wstring>>;
using Progress = std::function<void(unsigned long long done, unsigned long long total)>;

Response Request(const std::wstring& method, const std::wstring& url, const Headers& headers = {},
                 const std::string& body = {});
Response Get(const std::wstring& url, const Headers& headers = {});
bool Download(const std::wstring& url, const fs::path& dest, const Progress& progress = {});

}
