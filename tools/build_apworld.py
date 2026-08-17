# -*- coding: utf-8 -*-
"""Build openttd.apworld from apworld/openttd/.

The release script builds the stable one at package time, so the shipped
package is always fresh. The two loose files in apworld/ are untracked local
artefacts that nothing rebuilt -- and a stale one silently generates seeds
from old code. This makes them reproducible; lint_apworld_fresh.py makes
staleness an error.

    python tools/build_apworld.py

Same rules as the release script: Python zipfile (forward slashes -- what
Compress-Archive writes breaks on Linux), __pycache__ left out, and the
the game.
"""
import io
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "apworld", "openttd")

# (output name, folder inside the zip)
BUILDS = [
    ("openttd.apworld", "openttd"),
]


def collect():
    """Every file that belongs in an apworld, as (arc-relative path, bytes)."""
    out = []
    for base, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for f in sorted(files):
            if f.endswith(".pyc"):
                continue
            full = os.path.join(base, f)
            rel = os.path.relpath(full, SRC).replace(os.sep, "/")
            out.append((rel, io.open(full, "rb").read()))
    return sorted(out)


def build(files, name, folder):
    dst = os.path.join(ROOT, "apworld", name)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        for rel, data in files:
            z.writestr(folder + "/" + rel, data)
    return dst, os.path.getsize(dst)


def main():
    if not os.path.isdir(SRC):
        print("kilden findes ikke: " + SRC)
        return 1
    files = collect()
    print("%d filer fra apworld/openttd/" % len(files))
    for name, folder in BUILDS:
        dst, size = build(files, name, folder)
        print("  %-22s %7d bytes" % (name, size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
