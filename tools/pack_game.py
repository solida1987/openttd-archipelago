# -*- coding: utf-8 -*-
"""Assemble game_package.zip -- what the London plugin installs.

Layout: the proven v-win64 folder as base, the freshly built engine on top,
and the standalone seed pool. Runs the foreign-GRF gate on the result and
writes game_manifest.json (version + per-file sha256) beside it.

    python tools/pack_game.py --version v2.0.0
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")


def newest_win64_base():
    cands = [d for d in os.listdir(DIST)
             if d.endswith("-win64") and os.path.isdir(os.path.join(DIST, d))]
    if not cands:
        raise SystemExit("no dist/*-win64 base folder -- run the stable package build once")
    return os.path.join(DIST, sorted(cands)[-1])


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    args = ap.parse_args()

    base = newest_win64_base()
    build_bin = os.path.join(ROOT, "build", "RelWithDebInfo")
    seeds = os.path.join(ROOT, "standalone_seeds")
    if not os.path.isfile(os.path.join(build_bin, "openttd.exe")):
        raise SystemExit("no build/RelWithDebInfo/openttd.exe -- build the engine first")
    if not os.path.isfile(os.path.join(seeds, "index.json")):
        raise SystemExit("no standalone_seeds/index.json -- run tools/gen_standalone_seeds.py")

    staging = os.path.join(DIST, "game_package")
    if os.path.isdir(staging):
        shutil.rmtree(staging)
    shutil.copytree(base, staging)

    # Fresh engine on top of the proven layout.
    for f in os.listdir(build_bin):
        if f == "openttd.exe" or f.endswith(".dll"):
            shutil.copy2(os.path.join(build_bin, f), os.path.join(staging, f))

    shutil.copytree(seeds, os.path.join(staging, "standalone_seeds"))

    # Never ship anyone's play state.
    for junk in ("save", "standalone", "ap_launcher.log", "ap_version.txt"):
        p = os.path.join(staging, junk)
        if os.path.isdir(p):
            shutil.rmtree(p)
        elif os.path.isfile(p):
            os.remove(p)

    zip_path = os.path.join(DIST, "game_package.zip")
    if os.path.isfile(zip_path):
        os.remove(zip_path)
    manifest = {"version": args.version, "files": {}}
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for dirpath, _dirnames, filenames in os.walk(staging):
            for name in filenames:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, staging).replace("\\", "/")
                z.write(full, rel)
                manifest["files"][rel] = sha256(full)

    io.open(os.path.join(DIST, "game_manifest.json"), "w", encoding="utf-8").write(
        json.dumps(manifest, indent=1))

    gate = subprocess.run([sys.executable,
                           os.path.join(ROOT, "tools", "lint_no_foreign_grf.py"),
                           zip_path], capture_output=True, text=True)
    print(gate.stdout.strip())
    if gate.returncode != 0:
        print(gate.stderr.strip())
        raise SystemExit("foreign-GRF gate FAILED -- package not usable")

    print("game_package.zip: %.1f MB, %d filer, version %s" % (
        os.path.getsize(zip_path) / 1048576, len(manifest["files"]), args.version))
    return 0


if __name__ == "__main__":
    sys.exit(main())
