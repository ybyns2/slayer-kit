#include "core/KitFiles.h"

#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#include "core/Config.h"
#include "core/Crypto.h"

namespace sk::kitfiles {

namespace {

std::mutex g_mutex;

fs::path BaseDir() {
    fs::path p = LocalDataDir() / L"kit-baseline";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path IndexPath() { return BaseDir() / L"index.json"; }

json LoadIndex() {
    try {
        auto t = ReadFileText(IndexPath());
        if (!t.empty()) return json::parse(t);
    } catch (...) {
    }
    return json::object();
}

void SaveIndex(const json& j) { WriteFileText(IndexPath(), j.dump(2)); }

std::string Key(const fs::path& p) { return Narrow(p.wstring()); }

fs::path BaselineFile(const fs::path& p) {
    std::string k = Key(p);
    return BaseDir() / Widen(crypto::Sha256Hex({k.begin(), k.end()}).substr(0, 24) + ".base");
}

enum class Kind { Json, Ini, Raw };

Kind KindOf(const fs::path& p) {
    auto e = p.extension().wstring();
    if (_wcsicmp(e.c_str(), L".json") == 0) return Kind::Json;
    if (_wcsicmp(e.c_str(), L".ini") == 0) return Kind::Ini;
    return Kind::Raw;
}

bool UiStateKey(const std::string& k) {
    static const std::set<std::string> keys = {"uuid", "source_uuid", "id_counter", "prev_ver", "hotkeys", "current_scene",
                                               "current_program_scene", "saved_projectors", "scaling_enabled", "scaling_level",
                                               "scaling_off_x", "scaling_off_y", "preview_locked", "resolution",
                                               "migration_resolution", "canvases", "version", "private_settings"};
    return keys.count(k) > 0;
}

bool EmptyLike(const json& v) { return v.is_null() || ((v.is_array() || v.is_object()) && v.empty()); }

bool Same(const json& a, const json& b);

bool SameObject(const json& a, const json& b) {
    for (auto& [k, v] : a.items()) {
        if (UiStateKey(k)) continue;
        auto it = b.find(k);
        if (it == b.end()) {
            if (!EmptyLike(v)) return false;
        } else if (!Same(v, *it)) return false;
    }
    for (auto& [k, v] : b.items()) {
        if (UiStateKey(k)) continue;
        if (!a.contains(k) && !EmptyLike(v)) return false;
    }
    return true;
}

bool Same(const json& a, const json& b) {
    if (a.is_number() && b.is_number()) return std::fabs(a.get<double>() - b.get<double>()) < 1e-4;
    if (a.is_object() && b.is_object()) return SameObject(a, b);
    if (a.is_array() && b.is_array()) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (!Same(a[i], b[i])) return false;
        return true;
    }
    return a == b;
}

bool NamedArray(const json& a) {
    if (!a.is_array() || a.empty()) return false;
    for (auto& e : a)
        if (!e.is_object() || !e.contains("name") || !e["name"].is_string()) return false;
    return true;
}

const json* FindNamed(const json& arr, const std::string& name) {
    if (!arr.is_array()) return nullptr;
    for (auto& e : arr)
        if (e.is_object() && e.value("name", "") == name) return &e;
    return nullptr;
}

json KeepObsExtras(json n, const json& b, const json& u) {
    if (!n.is_object() || !u.is_object()) return n;
    for (auto& [k, v] : u.items()) {
        bool extra = !n.contains(k) && !(b.is_object() && b.contains(k));
        if (UiStateKey(k) || extra) n[k] = v;
    }
    return n;
}

json Merge(const json& b, const json& n, const json& u) {
    if (Same(u, b)) return KeepObsExtras(n, b, u);
    if (Same(n, b)) return u;
    if (n.is_object() && u.is_object()) {
        json out = u;
        for (auto& [k, nv] : n.items()) {
            if (UiStateKey(k) && u.contains(k)) continue;
            bool inB = b.is_object() && b.contains(k);
            bool inU = u.contains(k);
            if (inB && inU) out[k] = Merge(b[k], nv, u[k]);
            else if (!inB || EmptyLike(b[k])) out[k] = nv;
        }
        if (b.is_object())
            for (auto& [k, bv] : b.items())
                if (!n.contains(k) && u.contains(k) && Same(u[k], bv)) out.erase(k);
        return out;
    }
    if (NamedArray(n) || NamedArray(u)) {
        json out = json::array();
        std::set<std::string> used;
        for (auto& ue : u) {
            std::string name = ue.is_object() ? ue.value("name", "") : "";
            const json* ne = FindNamed(n, name);
            const json* be = FindNamed(b, name);
            if (ne && be) out.push_back(Merge(*be, *ne, ue));
            else if (ne) out.push_back(*ne);
            else if (be && Same(*be, ue)) continue;
            else out.push_back(ue);
            used.insert(name);
        }
        for (auto& ne : n) {
            std::string name = ne.value("name", "");
            if (used.count(name)) continue;
            if (FindNamed(b, name) && !FindNamed(u, name)) continue;
            out.push_back(ne);
        }
        return out;
    }
    return u;
}

void Diff(const json& b, const json& u, const std::string& where, std::vector<std::wstring>& out) {
    if (out.size() > 12 || Same(b, u)) return;
    if (b.is_object() && u.is_object()) {
        for (auto& [k, bv] : b.items()) {
            if (UiStateKey(k)) continue;
            if (!u.contains(k)) {
                if (!EmptyLike(bv)) out.push_back(Widen(where + " : « " + k + " » supprimé"));
            }
            else Diff(bv, u[k], where.empty() ? k : where + " › " + k, out);
        }
        return;
    }
    if (NamedArray(b)) {
        for (auto& be : b) {
            std::string name = be.value("name", "");
            const json* ue = FindNamed(u, name);
            if (!ue) out.push_back(Widen(where + " : « " + name + " » supprimé"));
            else Diff(be, *ue, name, out);
        }
        return;
    }
    out.push_back(Widen(where.empty() ? "valeur modifiée" : where));
}

using Ini = std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>;

Ini ParseIni(const std::string& text) {
    Ini ini;
    std::istringstream in(text);
    std::string line;
    ini.push_back({"", {}});
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line.erase(0, 3);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            ini.push_back({line.substr(1, line.size() - 2), {}});
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        ini.back().second.push_back({line.substr(0, eq), line.substr(eq + 1)});
    }
    return ini;
}

std::map<std::string, std::string> Flat(const Ini& ini) {
    std::map<std::string, std::string> m;
    for (auto& [sec, kv] : ini)
        for (auto& [k, v] : kv) m[sec + "\x1f" + k] = v;
    return m;
}

std::string WriteIni(const Ini& ini) {
    std::string out;
    for (auto& [sec, kv] : ini) {
        if (sec.empty() && kv.empty()) continue;
        if (!sec.empty()) out += "[" + sec + "]\n";
        for (auto& [k, v] : kv) out += k + "=" + v + "\n";
        out += "\n";
    }
    return out;
}

}

std::string MergeJson(const std::string& base, const std::string& next, const std::string& user) {
    json b = json::parse(base, nullptr, false), n = json::parse(next, nullptr, false), u = json::parse(user, nullptr, false);
    if (n.is_discarded()) return user;
    if (b.is_discarded() || u.is_discarded()) return next;
    return Merge(b, n, u).dump(4);
}

std::string MergeIni(const std::string& base, const std::string& next, const std::string& user) {
    Ini u = ParseIni(user), n = ParseIni(next);
    auto fb = Flat(ParseIni(base)), fu = Flat(u);
    for (auto& [sec, kv] : n) {
        auto sit = std::find_if(u.begin(), u.end(), [&](auto& s) { return s.first == sec; });
        if (sit == u.end()) {
            u.push_back({sec, kv});
            continue;
        }
        for (auto& [k, v] : kv) {
            std::string key = sec + "\x1f" + k;
            auto kit = std::find_if(sit->second.begin(), sit->second.end(), [&](auto& p) { return p.first == k; });
            bool userChanged = fb.count(key) && fu.count(key) && fu[key] != fb[key];
            if (kit == sit->second.end()) sit->second.push_back({k, v});
            else if (!userChanged) kit->second = v;
        }
    }
    return WriteIni(u);
}

bool Install(const fs::path& path, const std::string& content, const std::wstring& label, Policy policy, bool* merged) {
    std::lock_guard lock(g_mutex);
    if (merged) *merged = false;
    std::error_code ec;
    std::string finalContent = content;
    fs::path basePath = BaselineFile(path);
    if (policy == Policy::KeepUserChanges && fs::exists(path, ec) && fs::exists(basePath, ec)) {
        std::string base = ReadFileText(basePath), user = ReadFileText(path);
        switch (KindOf(path)) {
        case Kind::Json: finalContent = MergeJson(base, content, user); break;
        case Kind::Ini: finalContent = MergeIni(base, content, user); break;
        case Kind::Raw: finalContent = user == base ? content : user; break;
        }
        if (merged) *merged = finalContent != content;
    }
    if (!WriteFileText(path, finalContent)) return false;
    WriteFileText(basePath, content);
    auto idx = LoadIndex();
    idx[Key(path)] = {{"label", Narrow(label)}};
    SaveIndex(idx);
    return true;
}

void Forget(const fs::path& path) {
    std::lock_guard lock(g_mutex);
    auto idx = LoadIndex();
    idx.erase(Key(path));
    SaveIndex(idx);
    std::error_code ec;
    fs::remove(BaselineFile(path), ec);
}

std::vector<Modified> DetectModified() {
    std::lock_guard lock(g_mutex);
    std::vector<Modified> out;
    std::error_code ec;
    const json idx = LoadIndex();
    for (auto& [key, v] : idx.items()) {
        fs::path p = Widen(key);
        std::wstring label = Widen(v.value("label", key));
        if (!fs::exists(p, ec)) {
            out.push_back({p, label, true, {}});
            continue;
        }
        std::string base = ReadFileText(BaselineFile(p)), cur = ReadFileText(p);
        if (base.empty()) continue;
        Modified m{p, label, false, {}};
        if (KindOf(p) == Kind::Json) {
            json b = json::parse(base, nullptr, false), u = json::parse(cur, nullptr, false);
            if (!b.is_discarded() && !u.is_discarded()) {
                json bs = b, us = u;
                for (auto* j : {&bs, &us}) {
                    j->erase("current_scene");
                    j->erase("current_program_scene");
                    j->erase("saved_projectors");
                }
                Diff(bs, us, "", m.details);
            }
        } else if (KindOf(p) == Kind::Ini) {
            auto fb = Flat(ParseIni(base)), fu = Flat(ParseIni(cur));
            for (auto& [k, val] : fb) {
                auto it = fu.find(k);
                std::string name = k.substr(k.find('\x1f') + 1);
                if (it == fu.end() || it->second != val) m.details.push_back(Widen(name));
            }
        } else if (base != cur) {
            m.details.push_back(L"contenu modifié");
        }
        if (!m.details.empty()) out.push_back(m);
    }
    return out;
}

}
