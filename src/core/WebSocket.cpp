#include "core/WebSocket.h"

namespace sk {

bool WebSocket::Connect(const std::wstring& url) {
    Close();
    URL_COMPONENTS uc{sizeof uc};
    wchar_t host[256], path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    wchar_t extra[2048];
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 2048;
    std::wstring u = url;
    bool secure = u.rfind(L"wss://", 0) == 0;
    if (secure) u.replace(0, 3, L"https");
    else if (u.rfind(L"ws://", 0) == 0) u.replace(0, 2, L"http");
    if (!WinHttpCrackUrl(u.c_str(), 0, 0, &uc)) return false;
    std::wstring h(host, uc.dwHostNameLength), p(path, uc.dwUrlPathLength);
    p.append(extra, uc.dwExtraInfoLength);
    if (p.empty()) p = L"/";

    session_ = WinHttpOpen(L"SlayerKit", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!session_) return false;
    WinHttpSetTimeouts(session_, 5000, 5000, 10000, 0);
    conn_ = WinHttpConnect(session_, h.c_str(), uc.nPort, 0);
    if (!conn_) return Close(), false;
    req_ = WinHttpOpenRequest(conn_, L"GET", p.c_str(), nullptr, nullptr, nullptr, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!req_) return Close(), false;
    if (!WinHttpSetOption(req_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) return Close(), false;
    if (!WinHttpSendRequest(req_, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
        !WinHttpReceiveResponse(req_, nullptr))
        return Close(), false;
    ws_ = WinHttpWebSocketCompleteUpgrade(req_, 0);
    WinHttpCloseHandle(req_);
    req_ = nullptr;
    if (!ws_) return Close(), false;
    return true;
}

bool WebSocket::Send(const std::string& text) {
    if (!ws_) return false;
    return WinHttpWebSocketSend(ws_, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)text.data(), (DWORD)text.size()) ==
           ERROR_SUCCESS;
}

bool WebSocket::Receive(std::string& out) {
    out.clear();
    if (!ws_) return false;
    char buf[16384];
    for (;;) {
        DWORD got = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
        if (WinHttpWebSocketReceive(ws_, buf, sizeof buf, &got, &type) != ERROR_SUCCESS) return false;
        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) return false;
        out.append(buf, got);
        if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
            return true;
    }
}

void WebSocket::Close() {
    if (ws_) {
        WinHttpWebSocketClose(ws_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(ws_);
        ws_ = nullptr;
    }
    if (req_) WinHttpCloseHandle(req_), req_ = nullptr;
    if (conn_) WinHttpCloseHandle(conn_), conn_ = nullptr;
    if (session_) WinHttpCloseHandle(session_), session_ = nullptr;
}

}
