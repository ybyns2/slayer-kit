#include "core/Http.h"

#include <windows.h>
#include <winhttp.h>

#include <fstream>

namespace sk::http {

namespace {

struct Handle {
    HINTERNET h = nullptr;
    Handle() = default;
    explicit Handle(HINTERNET v) : h(v) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
};

const wchar_t* kAgent = L"SlayerKit/" SK_WIDEN_VERSION;

struct Parsed {
    std::wstring host, path;
    INTERNET_PORT port = 443;
    bool https = true;
};

bool Parse(const std::wstring& url, Parsed& out) {
    URL_COMPONENTS uc{sizeof uc};
    wchar_t host[256], path[4096];
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)std::size(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)std::size(path);
    wchar_t extra[4096];
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = (DWORD)std::size(extra);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    out.host.assign(host, uc.dwHostNameLength);
    out.path.assign(path, uc.dwUrlPathLength);
    out.path.append(extra, uc.dwExtraInfoLength);
    if (out.path.empty()) out.path = L"/";
    out.port = uc.nPort;
    out.https = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

template <class Sink>
Response Perform(const std::wstring& method, const std::wstring& url, const Headers& headers, const std::string& body,
                 Sink&& sink) {
    Response r;
    Parsed p;
    if (!Parse(url, p)) {
        r.error = "URL invalide";
        return r;
    }
    Handle session(WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        r.error = "WinHttpOpen";
        return r;
    }
    WinHttpSetTimeouts(session, 10000, 15000, 30000, 60000);
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);

    Handle conn(WinHttpConnect(session, p.host.c_str(), p.port, 0));
    if (!conn) {
        r.error = "Connexion impossible";
        return r;
    }
    Handle req(WinHttpOpenRequest(conn, method.c_str(), p.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, p.https ? WINHTTP_FLAG_SECURE : 0));
    if (!req) {
        r.error = "Requete impossible";
        return r;
    }
    std::wstring hdr;
    for (auto& [k, v] : headers) hdr += k + L": " + v + L"\r\n";
    BOOL sent = WinHttpSendRequest(req, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(), (DWORD)-1L,
                                   body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(),
                                   (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
        r.error = "Pas de reponse (connexion internet ?)";
        return r;
    }
    DWORD status = 0, len = sizeof status;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &len, WINHTTP_NO_HEADER_INDEX);
    r.status = (int)status;

    unsigned long long total = 0;
    wchar_t cl[32];
    DWORD clLen = sizeof cl;
    if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &clLen,
                            WINHTTP_NO_HEADER_INDEX))
        total = _wcstoui64(cl, nullptr, 10);

    unsigned long long done = 0;
    std::vector<char> buf(64 * 1024);
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) {
            r.error = "Telechargement interrompu";
            break;
        }
        if (!avail) break;
        DWORD got = 0;
        if (!WinHttpReadData(req, buf.data(), std::min<DWORD>(avail, (DWORD)buf.size()), &got) || !got) break;
        done += got;
        if (!sink(buf.data(), got, done, total)) {
            r.error = "Annule";
            break;
        }
    }
    return r;
}

}

Response Request(const std::wstring& method, const std::wstring& url, const Headers& headers, const std::string& body) {
    std::string data;
    Response r = Perform(method, url, headers, body, [&](const char* p, DWORD n, auto, auto) {
        data.append(p, n);
        return true;
    });
    r.body = std::move(data);
    return r;
}

Response Get(const std::wstring& url, const Headers& headers) { return Request(L"GET", url, headers); }

bool Download(const std::wstring& url, const fs::path& dest, const Progress& progress) {
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    fs::path tmp = dest;
    tmp += L".part";
    bool ok = false;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        Response r = Perform(L"GET", url, {}, {}, [&](const char* p, DWORD n, unsigned long long done, unsigned long long total) {
            f.write(p, n);
            if (progress) progress(done, total);
            return (bool)f;
        });
        ok = r.ok() && r.error.empty() && (bool)f;
        if (!ok) Log("Telechargement echoue " + Narrow(url) + " : " + std::to_string(r.status) + " " + r.error);
    }
    if (!ok) {
        fs::remove(tmp, ec);
        return false;
    }
    return MoveFileExW(tmp.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}
