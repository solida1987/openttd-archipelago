# -*- coding: utf-8 -*-
"""Gate: the packed .apworld files must match apworld/openttd/.

Written after a stale openttd.apworld generated a seed from code that was
eight kilobytes behind the source -- the GRF requirements were in the source
and simply absent from the seed. Nothing was red; the artefact was just old.
Same shape as the Game/apworld/ mistake in Diablo II.

    python tools/lint_apworld_fresh.py

Compares content, not timestamps: a rebuild that changes nothing should not
fail, and a file touched without being rebuilt should.
"""
import os
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_apworld import BUILDS, ROOT, collect  # noqa: E402


def expected(files, folder):
    out = {}
    for rel, data in files:
        out[folder + "/" + rel] = data
    return out


def check(name, folder, files):
    path = os.path.join(ROOT, "apworld", name)
    if not os.path.isfile(path):
        return ["%s findes ikke -- koer tools/build_apworld.py" % name]

    want = expected(files, folder)
    try:
        with zipfile.ZipFile(path) as z:
            have = {n: z.read(n) for n in z.namelist()}
    except zipfile.BadZipFile:
        return ["%s er ikke en gyldig zip" % name]

    problems = []
    for missing in sorted(set(want) - set(have)):
        problems.append("%s: mangler %s" % (name, missing))
    for extra in sorted(set(have) - set(want)):
        problems.append("%s: har %s som ikke findes i kilden" % (name, extra))
    for shared in sorted(set(want) & set(have)):
        if want[shared] != have[shared]:
            problems.append("%s: %s afviger fra kilden (%d vs %d bytes)"
                            % (name, shared, len(have[shared]), len(want[shared])))
    return problems


def main():
    files = collect()
    problems = []
    for name, folder in BUILDS:
        p = check(name, folder, files)
        print("  %-22s %s" % (name, "OK" if not p else "%d afvigelser" % len(p)))
        problems += p

    if problems:
        print()
        for p in problems[:20]:
            print("  " + p)
        if len(problems) > 20:
            print("  ... og %d mere" % (len(problems) - 20))
        print()
        print("FEJL: pakket .apworld matcher ikke kilden. Koer tools/build_apworld.py")
        return 1

    print("OK: .apworld svarer til apworld/openttd/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
