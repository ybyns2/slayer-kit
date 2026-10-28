#pragma once
#include <windows.h>
#include <winhttp.h>

#include <string>

#include "core/Util.h"

namespace sk {

class WebSocket {
public:
    WebSocket() = default;
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    ~WebSocket() { Close(); }

    bool Connect(const std::wstring& url);
    bool Send(const std::string& text);
    bool Receive(std::string& out);
    void Close();
    bool Open() const { return ws_ != nullptr; }

private:
    HINTERNET session_ = nullptr, conn_ = nullptr, req_ = nullptr, ws_ = nullptr;
};

}
