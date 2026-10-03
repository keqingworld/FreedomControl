"""Verify an INPUT13 -> FREEDOM14 source overlay, without touching user files.

This is archive/configuration preservation, not Skyrim save or ABI validation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline", type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.baseline) as archive:
        names = [name for name in archive.namelist() if not name.endswith("/")]
        assert len(names) == len(set(names))
        assert all(name.startswith("FreedomControl/") and ".." not in Path(name).parts for name in names)
        old = {name.split("/", 1)[1]: archive.read(name) for name in names}
    assert old["LATEST_BUILD_ID.txt"].decode().strip() == "FC-0.5.2-INPUT13-20260925-I"
    assert all((ROOT / name).is_file() for name in old), "A baseline file disappeared"
    unchanged = ["package/FreedomControlRuntime.esp", "package/SKSE/Plugins/FreedomControl.ini",
                 "src/Core.cpp", "src/PlayerCustomization.cpp", "src/Input13.cpp",
                 "include/fc/PlayerCustomization.hpp",
                 "cmake/ResetPluginCache.cmake", "build-settings.example.json"]
    for name in unchanged:
        assert (ROOT / name).read_bytes() == old[name], name
    original_input=old["include/fc/InputPolicy13.hpp"].decode()
    current_input=(ROOT / "include/fc/InputPolicy13.hpp").read_text()
    assert original_input.split("// A menu session",1)[1] == current_input.split("// A menu session",1)[1], "Prior text/wheel policies changed"
    before = json.loads(old["vcpkg.json"])
    after = json.loads((ROOT / "vcpkg.json").read_text())
    before.pop("version-semver"); after.pop("version-semver")
    assert before == after, "Dependency versions/features changed unexpectedly"
    old_catalog = json.loads(old["package/SKSE/Plugins/FreedomControl.creatures.json"])
    new_catalog = json.loads((ROOT / "package/SKSE/Plugins/FreedomControl.creatures.json").read_text())
    old_catalog.pop("build"); new_catalog.pop("build")
    assert old_catalog == new_catalog, "Existing creature catalog was reduced or changed"
    old_pages = set(re.findall(r"void\s+(\w*Page\w*)\s*\(", old["src/Overlay.cpp"].decode()))
    new_pages = set(re.findall(r"void\s+(\w*Page\w*)\s*\(", (ROOT / "src/Overlay.cpp").read_text()))
    assert old_pages <= new_pages, "An existing feature page was removed"
    old_save_keys = set(re.findall(r'j\["(\w+)"\]', old["src/Engine.cpp"].decode()))
    new_save_keys = set(re.findall(r'j\["(\w+)"\]', (ROOT / "src/Engine.cpp").read_text()))
    assert old_save_keys <= new_save_keys, "An existing co-save section was removed"

    for source in (old["src/Engine.hpp"].decode(), (ROOT / "src/Engine.hpp").read_text()):
        ops = re.search(r"enum class Op\s*\{(.*?)\};", source, re.S).group(1)
        if "old_ops" not in locals():
            old_ops = [word.strip() for word in ops.split(",") if word.strip()]
        else:
            new_ops = [word.strip() for word in ops.split(",") if word.strip()]
    assert new_ops[:len(old_ops)] == old_ops, "Existing action IDs were reordered or removed"
    sentinels = ["build-settings.json", "commonlib.lock.txt", "build/vs2026/_commonlib/keep.lib",
                 "build/vs2026/vcpkg_installed/keep.txt", "dist/previous-MO2.zip"]
    with tempfile.TemporaryDirectory(prefix="fc-upgrade14-") as temporary:
        root = Path(temporary) / "FreedomControl"
        root.mkdir()
        for name, data in old.items():
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        for name in sentinels:
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"USER-OWNED KEEP")
        for source in ROOT.rglob("*"):
            if source.is_file() and "__pycache__" not in source.parts:
                destination = root / source.relative_to(ROOT)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
        assert all((root / name).read_bytes() == b"USER-OWNED KEEP" for name in sentinels)
    print(f"PASS: all {len(old)} baseline files retained; {len(unchanged)} protected files byte-identical; "
          f"{len(old_ops)} action IDs and {len(old_pages)} feature pages retained; full creature catalog, co-save keys, dependency configuration and {len(sentinels)} user/cache sentinels preserved")
    print("Baseline SHA256:", hashlib.sha256(args.baseline.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
