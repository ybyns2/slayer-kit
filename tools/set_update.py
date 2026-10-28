import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OWNER, REPO = "ybyns2", "slayer-kit"

ap = argparse.ArgumentParser()
ap.add_argument("--version", required=True)
ap.add_argument("--channel", choices=["stable", "beta"], required=True)
ap.add_argument("--critical", default="false")
ap.add_argument("--sha256", required=True)
ap.add_argument("--notes", default="")
a = ap.parse_args()

def ver(v: str) -> tuple:
    return tuple(int(x) if x.isdigit() else 0 for x in v.split("."))

path = ROOT / "update.json"
data = json.loads(path.read_text(encoding="utf-8"))
entry = {
    "version": a.version,
    "critical": a.critical.lower() == "true",
    "notes": a.notes,
    "installer": f"https://github.com/{OWNER}/{REPO}/releases/download/v{a.version}/SlayerKit-Setup-{a.version}.exe",
    "sha256": a.sha256,
}
data.setdefault("channels", {})[a.channel] = entry
if a.channel == "stable":
    beta = data["channels"].get("beta", {})
    if not beta.get("installer") or ver(beta.get("version", "0")) <= ver(a.version):
        data["channels"]["beta"] = dict(entry)
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps(data["channels"], ensure_ascii=False, indent=2))
