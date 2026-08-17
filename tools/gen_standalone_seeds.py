# -*- coding: utf-8 -*-
"""Pre-generate standalone seeds with Archipelago's own generator.

Standalone must not re-implement the apworld's randomisation -- two
implementations drift. Instead every standalone seed IS a real solo
Archipelago seed, generated here at build time. The launcher then answers the
game locally from the seed file: same pipe protocol, no server.

Output: standalone_seeds/standard_<n>.json, each carrying slot_data, the
location name<->id tables, and the item placements, all lifted from the
generated multidata.

    python tools/gen_standalone_seeds.py [--archipelago C:/ProgramData/Archipelago] [--count 10]
"""
import argparse
import collections
import io
import json
import os
import pickle
import shutil
import subprocess
import sys
import tempfile
import zipfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "standalone_seeds")

sys.path.insert(0, os.path.join(ROOT, "tools"))
import lint_yaml_options as gate  # noqa: E402  (schema reader + YAML writer)


class _Opaque:
    def __init__(self, *a, **k):
        pass

    def __setstate__(self, s):
        pass


class _Unpickler(pickle.Unpickler):
    """Multidata is pickled with a few NetUtils types; stub what we read past."""

    def find_class(self, module, name):
        if module in ("NetUtils", "Utils"):
            if name == "NetworkItem":
                return collections.namedtuple("NetworkItem", "item location player flags")
            if name == "NetworkSlot":
                return collections.namedtuple("NetworkSlot", "name game type group_members")
            return _Opaque
        return super().find_class(module, name)


def read_multidata(zip_path):
    z = zipfile.ZipFile(zip_path)
    md_name = next(n for n in z.namelist() if n.endswith(".archipelago"))
    return _Unpickler(io.BytesIO(zlib.decompress(z.read(md_name)[1:]))).load()


def generate_one(ap_dir, game_name, rows, seed):
    tmp = tempfile.mkdtemp(prefix="ottd_seedgen_")
    try:
        players = os.path.join(tmp, "players")
        out = os.path.join(tmp, "out")
        os.makedirs(players)
        os.makedirs(out)
        gate.write_yaml(os.path.join(players, "solo.yaml"), game_name, rows)
        proc = subprocess.run(
            [os.path.join(ap_dir, "ArchipelagoGenerate.exe"),
             "--player_files_path", players, "--outputpath", out,
             "--seed", str(seed)],
            capture_output=True, text=True, timeout=900)
        zips = [f for f in os.listdir(out) if f.endswith(".zip")]
        if not zips:
            raise RuntimeError("generation failed:\n" +
                               (proc.stdout + proc.stderr)[-600:])
        return read_multidata(os.path.join(out, zips[0]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def to_seed_file(md, game_name, label):
    dp = md["datapackage"][game_name]
    loc_name_to_id = dp["location_name_to_id"]
    item_id_to_name = {v: k for k, v in dp["item_name_to_id"].items()}

    placements = {}
    for loc_id, (item_id, player, _flags) in md["locations"][1].items():
        if player != 1:
            raise RuntimeError("solo seed placed an item for another player")
        placements[str(loc_id)] = item_id

    return {
        "label": label,
        "seed_name": md["seed_name"],
        "game": game_name,
        "slot_data": md["slot_data"][1],
        "location_name_to_id": loc_name_to_id,
        "item_id_to_name": item_id_to_name,
        "placements": placements,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archipelago", default=r"C:\ProgramData\Archipelago")
    ap.add_argument("--count", type=int, default=10)
    args = ap.parse_args()

    game, rows = gate.read_schema()
    game_name = game

    src = os.path.join(ROOT, "apworld", "openttd.apworld")
    if not os.path.isfile(src):
        print("no apworld/openttd.apworld -- run tools/build_apworld.py first")
        return 1
    shutil.copyfile(src, os.path.join(args.archipelago, "custom_worlds",
                                      "openttd.apworld"))

    os.makedirs(OUT_DIR, exist_ok=True)
    index = []
    for n in range(1, args.count + 1):
        label = "standard_%d" % n
        md = generate_one(args.archipelago, game_name, rows, seed=900000 + n)
        seed_file = to_seed_file(md, game_name, label)
        path = os.path.join(OUT_DIR, label + ".json")
        io.open(path, "w", encoding="utf-8").write(
            json.dumps(seed_file, ensure_ascii=False))
        index.append({"label": label,
                      "seed_name": seed_file["seed_name"],
                      "locations": len(seed_file["placements"]),
                      "missions": len(seed_file["slot_data"].get("missions", []))})
        print("  %-12s %3d lokationer, %2d missioner  (%s)" % (
            label, index[-1]["locations"], index[-1]["missions"],
            seed_file["seed_name"]))

    io.open(os.path.join(OUT_DIR, "index.json"), "w", encoding="utf-8").write(
        json.dumps(index, ensure_ascii=False, indent=1))
    print("skrev %d seeds + index.json til standalone_seeds/" % len(index))
    return 0


if __name__ == "__main__":
    sys.exit(main())
