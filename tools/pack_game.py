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
import time
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


def newest_engine_build():
    """The freshest openttd.exe, with a guard against shipping a stale one.

    This used to be hardcoded to build/RelWithDebInfo. That folder is a dead
    CMake cache -- it still points at the old "...with Archipelago-exp" source
    path and cannot build at all -- so the exe sitting in it was from whenever
    it last worked. Packaging would have picked it up and shipped a binary
    older than the source, silently: the zip looks right, the version says the
    new number, and none of the fixes are in it.

    So: look through every build folder, take the newest exe, and refuse if it
    is older than the newest source file it should contain.
    """
    cands = []
    for d in os.listdir(ROOT):
        if not d.startswith("build"):
            continue
        for cfg in ("Release", "RelWithDebInfo", "Debug"):
            exe = os.path.join(ROOT, d, cfg, "openttd.exe")
            if os.path.isfile(exe):
                cands.append(exe)
    if not cands:
        raise SystemExit("no openttd.exe in any build*/ folder -- build the engine first")

    exe = max(cands, key=os.path.getmtime)
    exe_time = os.path.getmtime(exe)

    newest_src, newest_src_time = None, 0.0
    src_dir = os.path.join(ROOT, "src")
    for dirpath, _dirs, files in os.walk(src_dir):
        if "3rdparty" in dirpath:
            continue
        for name in files:
            if not name.endswith((".cpp", ".h", ".hpp")):
                continue
            full = os.path.join(dirpath, name)
            t = os.path.getmtime(full)
            if t > newest_src_time:
                newest_src, newest_src_time = full, t

    if newest_src and newest_src_time > exe_time:
        raise SystemExit(
            "STOP: the engine is older than the source.\n"
            f"  exe    {exe}\n"
            f"         {time.strftime('%Y-%m-%d %H:%M', time.localtime(exe_time))}\n"
            f"  source {newest_src}\n"
            f"         {time.strftime('%Y-%m-%d %H:%M', time.localtime(newest_src_time))}\n"
            "Rebuild before packaging -- otherwise the release carries a binary "
            "without the changes its version number promises.")

    print(f"  engine: {os.path.relpath(exe, ROOT)}  "
          f"({time.strftime('%Y-%m-%d %H:%M', time.localtime(exe_time))})")
    return os.path.dirname(exe)


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
    build_bin = newest_engine_build()
    seeds = os.path.join(ROOT, "standalone_seeds")
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

    # ⚠ AND THE FRESH APWORLD. The base folder is an old release, so it carries
    # that release's .apworld -- and only the engine used to be copied over it.
    # The zip then held a new binary and a stale apworld: options the game reads
    # but the world never sends, an "(Experimental)" banner long after it was
    # removed, and a world_version that disagreed with the release it shipped in.
    # Caught packaging v2.1.0, where the base was still v1.4.1.
    apw_src = os.path.join(ROOT, "apworld", "openttd.apworld")
    if not os.path.isfile(apw_src):
        raise SystemExit("no apworld/openttd.apworld -- run tools/build_apworld.py first")
    apw_dst_dir = os.path.join(staging, "apworld")
    os.makedirs(apw_dst_dir, exist_ok=True)
    for old in os.listdir(apw_dst_dir):
        if old.endswith(".apworld"):
            os.remove(os.path.join(apw_dst_dir, old))
    shutil.copy2(apw_src, os.path.join(apw_dst_dir, "openttd.apworld"))

    # ⚠⚠ AND THE LOOSE SOURCE BESIDE IT. Exactly the same trap as above, one
    # level down: the base folder also carries apworld/openttd/*.py from its
    # own release, and replacing only the .apworld left the package holding
    # two copies of the world that disagreed with each other. Caught packaging
    # v2.1.2 -- the zip was current while the .py files beside it still had
    # the old option ranges and none of the pre_fill fix.
    #
    # Both are shipped on purpose (the source goes out with every release), so
    # both have to be the same world.
    src_tree = os.path.join(ROOT, "apworld", "openttd")
    dst_tree = os.path.join(apw_dst_dir, "openttd")
    if os.path.isdir(dst_tree):
        shutil.rmtree(dst_tree)
    shutil.copytree(src_tree, dst_tree,
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))

    # And prove it took: reading the version back is the only way to know the
    # copies above actually replaced what the base folder brought. Read from
    # BOTH, because the whole point is that they must agree.
    with zipfile.ZipFile(apw_src) as _apw:
        _v = json.loads(_apw.read("openttd/archipelago.json"))["world_version"]
    with io.open(os.path.join(dst_tree, "archipelago.json"), encoding="utf-8") as _f:
        _vs = json.load(_f)["world_version"]
    if _v != _vs:
        raise SystemExit(f"apworld zip says {_v} but its source says {_vs}")
    print(f"  apworld: openttd.apworld + source (world_version {_v})")

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
