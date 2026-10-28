#include "core/GitHub.h"

#include <thread>

#include "core/Http.h"

namespace sk::github {

namespace {

const std::wstring kApi = L"https://api.github.com";

http::Headers ApiHeaders(const std::string& token) {
    http::Headers h = {{L"Accept", L"application/vnd.github+json"}, {L"X-GitHub-Api-Version", L"2022-11-28"}};
    if (!token.empty()) h.push_back({L"Authorization", L"Bearer " + Widen(token)});
    return h;
}

http::Response Api(const std::string& token, const std::wstring& method, const std::wstring& path,
                   const json& body = nullptr) {
    auto h = ApiHeaders(token);
    std::string payload;
    if (!body.is_null()) {
        h.push_back({L"Content-Type", L"application/json"});
        payload = body.dump();
    }
    return http::Request(method, kApi + path, h, payload);
}

std::string Explain(const http::Response& r) {
    if (!r.error.empty()) return r.error;
    try {
        auto j = json::parse(r.body);
        return "GitHub " + std::to_string(r.status) + " : " + j.value("message", "");
    } catch (...) {
        return "GitHub " + std::to_string(r.status);
    }
}

std::wstring Repo(const std::string& owner, const std::string& repo) {
    return L"/repos/" + Widen(owner) + L"/" + Widen(repo);
}

std::string FormEncode(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string out;
    for (auto& [k, v] : kv) {
        if (!out.empty()) out += '&';
        out += k + '=';
        for (unsigned char c : v) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
            else {
                char buf[4];
                snprintf(buf, sizeof buf, "%%%02X", c);
                out += buf;
            }
        }
    }
    return out;
}

} 

DeviceCode StartDeviceFlow(const std::string& clientId) {
    DeviceCode dc;
    if (clientId.empty()) {
        dc.error = "La connexion GitHub n'est pas encore configurée par l'auteur du kit.";
        return dc;
    }
    auto r = http::Request(L"POST", L"https://github.com/login/device/code",
                           {{L"Accept", L"application/json"}, {L"Content-Type", L"application/x-www-form-urlencoded"}},
                           FormEncode({{"client_id", clientId}, {"scope", "repo"}}));
    if (!r.ok()) {
        dc.error = Explain(r);
        return dc;
    }
    try {
        auto j = json::parse(r.body);
        dc.deviceCode = j.value("device_code", "");
        dc.userCode = j.value("user_code", "");
        dc.verificationUri = j.value("verification_uri", "https://github.com/login/device");
        dc.interval = j.value("interval", 5);
        dc.expiresIn = j.value("expires_in", 900);
        if (dc.deviceCode.empty()) dc.error = j.value("error_description", "Réponse GitHub inattendue");
    } catch (...) {
        dc.error = "Réponse GitHub illisible";
    }
    return dc;
}

std::string PollDeviceFlow(const std::string& clientId, const DeviceCode& dc, const std::atomic<bool>& cancel,
                           std::string* error) {
    int interval = std::max(dc.interval, 5);
    auto deadline = GetTickCount64() + (ULONGLONG)dc.expiresIn * 1000;
    while (!cancel && GetTickCount64() < deadline) {
        for (int i = 0; i < interval * 10 && !cancel; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (cancel) break;
        auto r = http::Request(
            L"POST", L"https://github.com/login/oauth/access_token",
            {{L"Accept", L"application/json"}, {L"Content-Type", L"application/x-www-form-urlencoded"}},
            FormEncode({{"client_id", clientId},
                        {"device_code", dc.deviceCode},
                        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}}));
        if (!r.ok()) continue;
        try {
            auto j = json::parse(r.body);
            if (auto t = j.value("access_token", ""); !t.empty()) return t;
            auto e = j.value("error", "");
            if (e == "authorization_pending") continue;
            if (e == "slow_down") {
                interval += 5;
                continue;
            }
            if (error) *error = j.value("error_description", e);
            return {};
        } catch (...) {
        }
    }
    if (error) *error = cancel ? "Annulé" : "Le code a expiré, recommence.";
    return {};
}

User Me(const std::string& token) {
    User u;
    auto r = Api(token, L"GET", L"/user");
    if (!r.ok()) {
        u.error = Explain(r);
        return u;
    }
    try {
        u.login = json::parse(r.body).value("login", "");
    } catch (...) {
        u.error = "Réponse GitHub illisible";
    }
    return u;
}

bool EnsurePrivateRepo(const std::string& token, const std::string& owner, const std::string& repo, std::string* error) {
    auto r = Api(token, L"GET", Repo(owner, repo));
    if (r.ok()) {
        try {
            if (!json::parse(r.body).value("private", false)) {
                if (error) *error = "Le repo " + repo + " existe déjà mais il est PUBLIC. Rends-le privé ou supprime-le.";
                return false;
            }
        } catch (...) {
        }
        return true;
    }
    if (r.status != 404) {
        if (error) *error = Explain(r);
        return false;
    }
    auto c = Api(token, L"POST", L"/user/repos",
                 {{"name", repo},
                  {"private", true},
                  {"auto_init", true},
                  {"description", "Sauvegarde privée Slayer Kit (OBS, TikTok LIVE Studio, FiveM). Ne pas rendre public."}});
    if (!c.ok()) {
        if (error) *error = Explain(c);
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}

bool PushDirectory(const std::string& token, const std::string& owner, const std::string& repo, const fs::path& dir,
                   const std::string& message, const Progress& progress, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        Log("GitHub push : " + why);
        return false;
    };
    auto ref = Api(token, L"GET", Repo(owner, repo) + L"/git/ref/heads/main");
    if (!ref.ok()) return fail(Explain(ref));
    std::string parent = json::parse(ref.body)["object"].value("sha", "");

    std::vector<fs::path> files;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file(ec)) files.push_back(it->path());

    json tree = json::array();
    for (size_t i = 0; i < files.size(); ++i) {
        std::string rel = Narrow(fs::relative(files[i], dir, ec).generic_wstring());
        if (progress) progress(L"Envoi : " + Widen(rel), (float)i / (files.size() + 2));
        auto blob = Api(token, L"POST", Repo(owner, repo) + L"/git/blobs",
                        {{"content", Base64Encode(ReadFileBytes(files[i]))}, {"encoding", "base64"}});
        if (!blob.ok()) return fail(rel + " : " + Explain(blob));
        tree.push_back({{"path", rel}, {"mode", "100644"}, {"type", "blob"}, {"sha", json::parse(blob.body).value("sha", "")}});
    }
    tree.push_back({{"path", "README.md"}, {"mode", "100644"}, {"type", "blob"},
                    {"content", "# Sauvegarde Slayer Kit\n\nRepo **privé** créé par Slayer Kit by yns_bnl.\n"
                                "Chaque commit est une sauvegarde complète ; restaure-la depuis l'application.\n"
                                "Les clés de stream et mots de passe ne sont jamais envoyés ici.\n"}});

    if (progress) progress(L"Création de la sauvegarde sur GitHub", 0.95f);
    auto t = Api(token, L"POST", Repo(owner, repo) + L"/git/trees", {{"tree", tree}});
    if (!t.ok()) return fail(Explain(t));
    auto c = Api(token, L"POST", Repo(owner, repo) + L"/git/commits",
                 {{"message", message}, {"tree", json::parse(t.body).value("sha", "")}, {"parents", {parent}}});
    if (!c.ok()) return fail(Explain(c));
    auto u = Api(token, L"PATCH", Repo(owner, repo) + L"/git/refs/heads/main",
                 {{"sha", json::parse(c.body).value("sha", "")}, {"force", false}});
    if (!u.ok()) return fail(Explain(u));
    if (progress) progress(L"Sauvegarde envoyée sur GitHub", 1.f);
    return true;
}

std::vector<Snapshot> History(const std::string& token, const std::string& owner, const std::string& repo,
                              std::string* error) {
    std::vector<Snapshot> out;
    auto r = Api(token, L"GET", Repo(owner, repo) + L"/commits?per_page=50");
    if (!r.ok()) {
        if (error) *error = Explain(r);
        return out;
    }
    try {
        for (auto& c : json::parse(r.body)) {
            std::string msg = c["commit"].value("message", "");
            if (msg.rfind("Initial commit", 0) == 0) continue;
            out.push_back({c.value("sha", ""), msg, c["commit"]["author"].value("date", "")});
        }
    } catch (...) {
        if (error) *error = "Historique illisible";
    }
    return out;
}

bool PullSnapshot(const std::string& token, const std::string& owner, const std::string& repo, const std::string& sha,
                  const fs::path& dest, const Progress& progress, std::string* error) {
    auto t = Api(token, L"GET", Repo(owner, repo) + L"/git/trees/" + Widen(sha) + L"?recursive=1");
    if (!t.ok()) {
        if (error) *error = Explain(t);
        return false;
    }
    auto items = json::parse(t.body).value("tree", json::array());
    size_t i = 0;
    for (auto& it : items) {
        ++i;
        if (it.value("type", "") != "blob") continue;
        std::string path = it.value("path", "");
        if (path.find("..") != std::string::npos) continue;
        if (progress) progress(L"Téléchargement : " + Widen(path), (float)i / items.size());
        auto b = Api(token, L"GET", Repo(owner, repo) + L"/git/blobs/" + Widen(it.value("sha", "")));
        if (!b.ok()) {
            if (error) *error = path + " : " + Explain(b);
            return false;
        }
        std::string content = json::parse(b.body).value("content", "");
        content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
        WriteFileBytes(dest / Widen(path), Base64Decode(content));
    }
    return true;
}

}
