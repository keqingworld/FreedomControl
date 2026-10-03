"""Check exact source ZIP bytes and release manifest, never a gameplay DLL."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile
from packaging_tests import source_sha

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as archive:
        assert archive.testzip() is None, "ZIP CRC failure"
        names = archive.namelist()
        assert len(names) == len(set(names)), "Duplicate ZIP entry"
        assert len(names) == len({name.casefold() for name in names}), "Windows path collision"
        forbidden = {".dll", ".exe", ".obj", ".o", ".lib", ".pdb", ".pyc", ".pyo", ".ttf", ".otf", ".ttc"}
        files = {}
        for name in names:
            path = PurePosixPath(name)
            assert path.parts[0] == "FreedomControl" and len(path.parts) > 1
            assert ".." not in path.parts and not path.is_absolute()
            assert "__pycache__" not in path.parts and path.suffix.lower() not in forbidden
            relative = path.relative_to("FreedomControl").as_posix()
            assert relative not in {"build-settings.json", "commonlib.lock.txt"}
            assert path.parts[1] not in {"build", "dist", "vcpkg_installed", ".git"}
            files[relative] = archive.read(name)
        manifest = json.loads(files["RELEASE_SHA256.json"])
        assert manifest["build"] == files["LATEST_BUILD_ID.txt"].decode().strip()
        assert manifest["source_identity_sha256"] == source_sha(ROOT)
        assert set(manifest["files"]) == set(files) - {"RELEASE_SHA256.json"}
        for name, data in files.items():
            assert data == (ROOT / name).read_bytes(), "Source/ZIP byte mismatch: " + name
            if name != "RELEASE_SHA256.json":
                assert hashlib.sha256(data).hexdigest() == manifest["files"][name], name
        expected = {path.relative_to(ROOT).as_posix() for path in ROOT.rglob("*")
                    if path.is_file() and "__pycache__" not in path.parts}
        assert expected == set(files), "Source file missing from archive"
    print(f"PASS: {len(files)} unique source files; CRC, Windows-safe paths, SHA256 manifest, "
          "source identity and byte-for-byte archive/source comparison; no DLL/EXE/object/font/cache files")
    print("ZIP SHA256:", hashlib.sha256(args.archive.read_bytes()).hexdigest())
    print("ZIP bytes:", args.archive.stat().st_size)


if __name__ == "__main__":
    main()
