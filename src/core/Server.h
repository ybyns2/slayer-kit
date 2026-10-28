#pragma once
#include <functional>
#include <map>
#include <string>

#include "core/Util.h"

namespace sk::server {

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::string body;
    bool local = false;
};

struct Response {
    int status = 200;
    std::string type = "text/html; charset=utf-8";
    std::string body;
};

using Handler = std::function<Response(const Request&)>;

bool Start(int port, bool lan, Handler handler);
void Stop();
bool Running();
int Port();
int Broadcast(const std::string& json);
int Clients();
std::wstring LanAddress();

}
