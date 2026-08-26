# -*- coding: utf-8 -*-
"""Fail the release if it ships a NewGRF that is not ours.

Eight third-party vehicle sets went out in every release up to v1.4.1 -- about
79 MB of other people's work, one of them credited, three with no licence
statement in the file at all. The repository looked clean the whole time,
because the release script un-tracked them while the packaging step copied the
whole folder regardless.

So this does not look at git, and it does not look at filenames. It reads the
GRFID out of each .grf and compares it against the short list of sets we wrote
ourselves. A renamed file, a re-downloaded file or a file somebody dropped in
by hand all answer the same question the same way.

Only newgrf/ is examined. baseset/ holds OpenGFX and OpenTTD's own graphics --
free replacements for the original Transport Tycoon data, required for the game
to run at all, and credited in THIRD-PARTY-NOTICES.md. They are a different
question from the optional vehicle sets and are not this gate's business.

    python tools/lint_no_foreign_grf.py <folder-or-zip>

Exit code 0 = clean, 1 = something foreign is in there.
"""
import os
import re
import struct
import sys
import zipfile

# Sets we wrote. Everything else, however harmless, is somebody else's to
# distribute -- and several of the ones we shipped are GPL, which would oblige
# us to offer their .nml source too.
OURS = {
    "41505255": "Archipelago Ruins",
    "41505354": "Archipelago Stars",
}

V2_MAGIC = b"\x00\x00GRF\x82\x0d\x0a\x1a\x0a"


def grf_identity(blob):
    """(grfid, name) from Action 8, or (None, reason)."""
    v2 = blob.startswith(V2_MAGIC)
    pos = 15 if v2 else 0            # v2: 10 magic + 4 data offset + 1 compression

    for _ in range(256):
        if v2:
            if pos + 5 > len(blob):
                break
            size = struct.unpack_from("<I", blob, pos)[0]
            head = 5
        else:
            if pos + 3 > len(blob):
                break
            size = struct.unpack_from("<H", blob, pos)[0]
            head = 3
        if size <= 0:
            break

        info = blob[pos + head - 1]
        data = blob[pos + head:pos + head + size]
        if info == 0xFF and data and data[0] == 0x08 and len(data) >= 7:
            grfid = data[2:6].hex()
            name = data[6:].split(b"\0")[0].decode("utf-8", "replace")
            return grfid, name
        pos += head + size

    return None, "no Action 8 found"


def in_scope(path):
    """Only the optional-sets folder. See the note at the top of this file."""
    p = path.replace("\\", "/").lower()
    return "/newgrf/" in p or p.startswith("newgrf/") or os.path.dirname(p) == ""


def collect(target):
    """[(display path, bytes)] for every .grf under newgrf/."""
    found = []
    if os.path.isdir(target):
        for root, _dirs, names in os.walk(target):
            for n in names:
                if not n.lower().endswith(".grf"):
                    continue
                p = os.path.join(root, n)
                rel = os.path.relpath(p, target)
                if not in_scope(rel):
                    continue
                with open(p, "rb") as f:
                    found.append((rel, f.read(400000)))
    elif zipfile.is_zipfile(target):
        z = zipfile.ZipFile(target)
        for e in z.namelist():
            if e.lower().endswith(".grf") and in_scope(e):
                found.append((e, z.read(e)[:400000]))
    else:
        print("not a folder or a zip: %s" % target)
        sys.exit(2)
    return found


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)

    target = sys.argv[1]
    grfs = collect(target)
    if not grfs:
        print("no .grf files in %s" % target)
        return 0

    bad = []
    print("%-34s %-10s %s" % ("file", "grfid", "name"))
    print("-" * 78)
    for path, blob in sorted(grfs):
        grfid, name = grf_identity(blob)
        ours = grfid in OURS
        if not ours:
            bad.append((path, grfid, name))
        print("%-34s %-10s %s%s" % (path, grfid or "-", name,
                                    "" if ours else "   <-- NOT OURS"))

    print()
    if bad:
        print("FAIL: %d third-party NewGRF(s) in the package." % len(bad))
        print()
        print("Players install these themselves through OpenTTD's Check Online")
        print("Content. Shipping them means distributing someone else's work,")
        print("and for the GPL ones it means owing their source as well.")
        return 1

    print("OK: only our own NewGRFs (%d)." % len(grfs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
