# -*- coding: utf-8 -*-
"""Gate: every option the launcher offers must be one the apworld accepts.

The launcher's YAML form is generated from options.py, so in principle they
cannot disagree. In practice the generated file is checked in, and a checked-in
generated file goes stale the moment somebody edits options.py without
re-running the tool -- which is exactly how the packed .apworld got eight
kilobytes behind its own source.

So this does not compare the two by reading them. It writes a YAML containing
EVERY key the launcher would offer, at its default, and hands it to
Archipelago's own generator. A key the apworld does not know fails there, which
is the same place it would fail for a player.

    python tools/lint_yaml_options.py [--archipelago C:\\ProgramData\\Archipelago]

Exit 0 = the form and the apworld agree.
"""
import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GEN = os.path.join(os.path.dirname(ROOT), "OpenTTD-London-Plugin",
                   "OpenTTDYamlOptions.g.cs")

# new("key", "kind", "display", "help", "group", default, min, max, choices)
ROW = re.compile(r'^\s*new\("((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)",'
                 r'.*?,\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),', re.S)


def read_schema():
    """(key, kind, default) for every option in the generated file."""
    text = io.open(GEN, encoding="utf-8").read()
    game = re.search(r'public const string Game = "([^"]+)"', text)
    rows = []
    for line in text.split("\n"):
        m = ROW.match(line)
        if m:
            rows.append((m.group(1), m.group(2), int(m.group(3))))
    return (game.group(1) if game else "OpenTTD"), rows


def write_yaml(path, game, rows, player="GateCheck"):
    out = [
        "name: " + player,
        "description: written by tools/lint_yaml_options.py",
        "game: " + game,
        game + ":",
        "  progression_balancing: normal",
        "  accessibility: full",
    ]
    for key, kind, default in rows:
        value = ("true" if default else "false") if kind == "toggle" else str(default)
        out.append("  %s: %s" % (key, value))
    io.open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    return len(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archipelago", default=r"C:\ProgramData\Archipelago")
    args = ap.parse_args()

    if not os.path.isfile(GEN):
        print("no generated schema -- run tools/gen_yaml_options.py first")
        return 1

    game, rows = read_schema()
    if not rows:
        print("the generated schema has no options in it")
        return 1

    gen_exe = os.path.join(args.archipelago, "ArchipelagoGenerate.exe")
    if not os.path.isfile(gen_exe):
        print("Archipelago not found at " + args.archipelago)
        print("  (pass --archipelago <folder>; this gate needs the real generator)")
        return 2

    # The apworld the launcher's form was generated from has to be the one the
    # generator uses, or this proves nothing.
    src = os.path.join(ROOT, "apworld", "openttd.apworld")
    if not os.path.isfile(src):
        print("no apworld/openttd.apworld -- run tools/build_apworld.py")
        return 1
    shutil.copyfile(src, os.path.join(args.archipelago, "custom_worlds",
                                      "openttd.apworld"))

    tmp = tempfile.mkdtemp(prefix="openttd_yaml_gate_")
    try:
        players = os.path.join(tmp, "players")
        output = os.path.join(tmp, "out")
        os.makedirs(players)
        os.makedirs(output)
        n = write_yaml(os.path.join(players, "gate.yaml"), game, rows)
        print("wrote a YAML with all %d options at their defaults" % n)

        proc = subprocess.run(
            [gen_exe, "--player_files_path", players, "--outputpath", output,
             "--seed", "20260814"],
            capture_output=True, text=True, timeout=900)

        made = [f for f in os.listdir(output) if f.endswith(".zip")]
        if made:
            print("OK: Archipelago accepted every key and generated a seed.")
            for line in proc.stdout.split("\n"):
                if "OpenTTD]" in line:
                    print("   " + line.strip())
            return 0

        print("FEJL: generering fejlede.")
        tail = (proc.stdout + proc.stderr).strip().split("\n")
        for line in tail[-25:]:
            print("   " + line)
        return 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
