#include "core/SelfTest.h"

#include "core/Config.h"
#include "core/KitFiles.h"

namespace sk::selftest {

int Run() {
    int fails = 0;
    auto check = [&](bool ok, const std::string& what) {
        Log(std::string(ok ? "SELFTEST OK   " : "SELFTEST ECHEC ") + what);
        if (!ok) ++fails;
    };

    json base = {{"name", "Kit"},
                 {"sources",
                  {{{"name", "Chat"}, {"settings", {{"width", 440}}}, {"volume", 1.0}},
                   {{"name", "Micro"}, {"settings", {{"device_id", "default"}}}, {"volume", 1.0}},
                   {{"name", "Ancien"}, {"settings", json::object()}}}}};
    json user = base;
    user["sources"][0]["settings"]["width"] = 600;
    user["sources"][1]["prev_ver"] = 537001986;
    user["sources"][1]["volume"] = 0.99999994;
    user["sources"].push_back({{"name", "Ma source"}, {"settings", json::object()}});
    json next = base;
    next["sources"][1]["settings"]["device_id"] = "{micro-jbl}";
    next["sources"].erase(2);
    next["sources"].push_back({{"name", "Alertes"}, {"settings", json::object()}});

    json m = json::parse(kitfiles::MergeJson(base.dump(), next.dump(), user.dump()));
    auto find = [&](const std::string& n) -> json {
        for (auto& s : m["sources"])
            if (s["name"] == n) return s;
        return nullptr;
    };
    check(find("Chat")["settings"]["width"] == 600, "modif utilisateur conservee (largeur du chat)");
    check(find("Micro")["settings"]["device_id"] == "{micro-jbl}", "nouveaute du kit appliquee (micro)");
    check(find("Micro").contains("prev_ver"), "cle ajoutee par OBS gardee");
    check(find("Ancien").is_null(), "source retiree par le kit supprimee");
    check(!find("Ma source").is_null(), "source ajoutee par l'utilisateur gardee");
    check(!find("Alertes").is_null(), "nouvelle source du kit ajoutee");

    json kitFile = {{"name", "Kit"}, {"scaling_enabled", false}, {"sources", {{{"name", "Texte"}, {"filters", json::array()}, {"volume", 1.0}}}}};
    json obsFile = {{"name", "Kit"}, {"scaling_enabled", true}, {"sources", {{{"name", "Texte"}, {"volume", 1.0}, {"prev_ver", 1}}}}};
    json merged2 = json::parse(kitfiles::MergeJson(kitFile.dump(), kitFile.dump(), obsFile.dump()));
    check(merged2["sources"].size() == 1, "reecriture OBS (filtres vides, apercu) sans effet");

    std::string ib = "[Output]\nMode=Advanced\n[AdvOut]\nEncoder=x264\nRecRB=true\n";
    std::string iu = "[Output]\nMode=Simple\n[AdvOut]\nEncoder=x264\nRecRB=true\nExtra=1\n";
    std::string in = "[Output]\nMode=Advanced\n[AdvOut]\nEncoder=nvenc\nRecRB=true\nTrack2Name=Redif\n";
    std::string mi = kitfiles::MergeIni(ib, in, iu);
    check(mi.find("Mode=Simple") != std::string::npos, "ini : choix utilisateur conserve");
    check(mi.find("Encoder=nvenc") != std::string::npos, "ini : nouveaute du kit appliquee");
    check(mi.find("Extra=1") != std::string::npos, "ini : cle OBS gardee");
    check(mi.find("Track2Name=Redif") != std::string::npos, "ini : nouvelle cle ajoutee");

    Log("SELFTEST termine : " + std::to_string(fails) + " echec(s)");
    return fails;
}

}
