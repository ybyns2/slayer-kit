#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "core/Server.h"

#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace sk::server {

namespace {

std::atomic<bool> g_run{false};
SOCKET g_listen = INVALID_SOCKET;
int g_port = 0;
Handler g_handler;
std::mutex g_sseMutex;
std::vector<SOCKET> g_sse;

std::string Decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else out += s[i];
    }
    return out;
}

void SendAll(SOCKET s, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        int n = send(s, data.data() + off, (int)(data.size() - off), 0);
        if (n <= 0) return;
        off += n;
    }
}

const char* Reason(int code) {
    switch (code) {
    case 200: return "OK";
    case 204: return "No Content";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    default: return "Error";
    }
}

void Client(SOCKET s, bool local) {
    DWORD timeout = 10000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
    std::string raw;
    char buf[8192];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos && raw.size() < 65536) {
        int n = recv(s, buf, sizeof buf, 0);
        if (n <= 0) break;
        raw.append(buf, n);
        headerEnd = raw.find("\r\n\r\n");
    }
    if (headerEnd == std::string::npos) {
        closesocket(s);
        return;
    }
    Request req;
    req.local = local;
    std::istringstream head(raw.substr(0, headerEnd));
    std::string target, line;
    head >> req.method >> target;
    size_t contentLength = 0;
    std::getline(head, line);
    while (std::getline(head, line)) {
        if (_strnicmp(line.c_str(), "Content-Length:", 15) == 0) contentLength = (size_t)std::atoi(line.c_str() + 15);
    }
    req.body = raw.substr(headerEnd + 4);
    while (req.body.size() < contentLength && req.body.size() < 65536) {
        int n = recv(s, buf, sizeof buf, 0);
        if (n <= 0) break;
        req.body.append(buf, n);
    }
    auto q = target.find('?');
    req.path = target.substr(0, q);
    if (q != std::string::npos) {
        std::istringstream qs(target.substr(q + 1));
        std::string kv;
        while (std::getline(qs, kv, '&')) {
            auto e = kv.find('=');
            req.query[Decode(kv.substr(0, e))] = e == std::string::npos ? "" : Decode(kv.substr(e + 1));
        }
    }

    Response res = g_handler ? g_handler(req) : Response{404, "text/plain", "introuvable"};
    if (res.type == "text/event-stream") {
        SendAll(s, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
                   "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n: ok\n\n");
        std::lock_guard lock(g_sseMutex);
        g_sse.push_back(s);
        return;
    }
    std::ostringstream os;
    os << "HTTP/1.1 " << res.status << " " << Reason(res.status) << "\r\nContent-Type: " << res.type
       << "\r\nContent-Length: " << res.body.size()
       << "\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
    SendAll(s, os.str() + res.body);
    shutdown(s, SD_BOTH);
    closesocket(s);
}

}

bool Start(int port, bool lan, Handler handler) {
    if (g_run) return true;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) return false;
    BOOL excl = TRUE;
    setsockopt(g_listen, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof excl);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    if (bind(g_listen, (sockaddr*)&addr, sizeof addr) == SOCKET_ERROR || listen(g_listen, 16) == SOCKET_ERROR) {
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
        Log("Serveur local : port " + std::to_string(port) + " indisponible");
        return false;
    }
    g_port = port;
    g_handler = std::move(handler);
    g_run = true;
    std::thread([] {
        while (g_run) {
            sockaddr_in from{};
            int len = sizeof from;
            SOCKET c = accept(g_listen, (sockaddr*)&from, &len);
            if (c == INVALID_SOCKET) break;
            bool local = from.sin_addr.s_addr == htonl(INADDR_LOOPBACK);
            std::thread(Client, c, local).detach();
        }
    }).detach();
    std::thread([] {
        while (g_run) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            Broadcast("");
        }
    }).detach();
    Log("Serveur local demarre sur le port " + std::to_string(port));
    return true;
}

void Stop() {
    g_run = false;
    g_port = 0;
    if (g_listen != INVALID_SOCKET) closesocket(g_listen);
    g_listen = INVALID_SOCKET;
    std::lock_guard lock(g_sseMutex);
    for (auto s : g_sse) closesocket(s);
    g_sse.clear();
}

bool Running() { return g_run; }
int Port() { return g_port; }

int Broadcast(const std::string& json) {
    std::string frame = json.empty() ? ": ping\n\n" : "data: " + json + "\n\n";
    std::lock_guard lock(g_sseMutex);
    for (auto it = g_sse.begin(); it != g_sse.end();) {
        int n = send(*it, frame.data(), (int)frame.size(), 0);
        if (n <= 0) {
            closesocket(*it);
            it = g_sse.erase(it);
        } else ++it;
    }
    return (int)g_sse.size();
}

int Clients() {
    std::lock_guard lock(g_sseMutex);
    return (int)g_sse.size();
}

std::wstring LanAddress() {
    ULONG size = 32 * 1024;
    std::vector<unsigned char> buf(size);
    auto* list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, list, &size) != NO_ERROR) return L"127.0.0.1";
    for (auto* a = list; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || !a->FirstGatewayAddress || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        std::wstring desc = a->Description ? a->Description : L"";
        if (desc.find(L"Virtual") != std::wstring::npos || desc.find(L"VPN") != std::wstring::npos) continue;
        if (auto* u = a->FirstUnicastAddress) {
            auto* sin = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            wchar_t ip[32];
            InetNtopW(AF_INET, &sin->sin_addr, ip, 32);
            return ip;
        }
    }
    return L"127.0.0.1";
}

}
