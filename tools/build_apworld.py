# -*- coding: utf-8 -*-
"""Build both .apworld files from apworld/openttd_exp/.

The release script builds the stable one at package time, so the shipped
package is always fresh. The two loose files in apworld/ are untracked local
artefacts that nothing rebuilt -- and a stale one silently generates seeds
from old code. This makes them reproducible; lint_apworld_fresh.py makes
staleness an error.

    python tools/build_apworld.py

Same rules as the release script: Python zipfile (forward slashes -- what
Compress-Archive writes breaks on Linux), __pycache__ left out, and the
stable copy gets OpenTTD-Exp renamed to OpenTTD in the two files that name
the game.
"""
import io
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "apworld", "openttd_exp")

# (output name, folder inside the zip, rename the game?)
BUILDS = [
    ("openttd_exp.apworld", "openttd_exp", False),
    ("openttd.apworld",     "openttd",     True),
]

# Only these carry the game name; renaming everything would hit descriptions.
RENAME_IN = ("archipelago.json", "__init__.py")


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


def build(files, name, folder, rename):
    dst = os.path.join(ROOT, "apworld", name)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        for rel, data in files:
            if rename and os.path.basename(rel) in RENAME_IN:
                data = data.replace(b"OpenTTD-Exp", b"OpenTTD")
            z.writestr(folder + "/" + rel, data)
    return dst, os.path.getsize(dst)


def main():
    if not os.path.isdir(SRC):
        print("kilden findes ikke: " + SRC)
        return 1
    files = collect()
    print("%d filer fra apworld/openttd_exp/" % len(files))
    for name, folder, rename in BUILDS:
        dst, size = build(files, name, folder, rename)
        print("  %-22s %7d bytes%s" % (name, size, "  (navn: OpenTTD)" if rename else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
