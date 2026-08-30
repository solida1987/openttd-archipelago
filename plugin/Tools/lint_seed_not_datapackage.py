"""The "x of y" counter must carry the SEED's size, never the datapackage's.

⚠⚠ WHAT THIS CAUGHT

The game showed a player "Checks 2 / 1911" while his seed held 355 locations.
1911 is not a seed at all: it is every location name the OpenTTD world can
ever hand out (200 missions + 600 shop + 100 ruins + 10 demigods + 1000 stars
+ the goal), and it is the same number for every OpenTTD seed ever generated.
The plugin had both numbers to hand and sent the wrong one.

Worse, it was inconsistent: one call site sent seed.Placements.Count, which is
right, and two later ones sent _idToName.Count, which is the datapackage. When
two places write the same value, the last write wins -- not the correct one.

So this walks every argument handed to SendLocationCountAsync and insists it
comes from a seed-sized source.

    py -3.13 tools/lint_seed_not_datapackage.py [<file> ...]

Exit code 1 on a violation, so it can gate a build.
"""
import pathlib
import re
import sys

# Sources that are the seed. Anything else is guilty until named here.
ALLOWED = (
    "SeedLocationCount()",       # checked + unchecked from the AP slot
    "seed.Placements.Count",     # the offline standalone seed's own table
)

# The datapackage tables, by name. Naming them explicitly makes the failure
# message say WHY rather than just "unrecognised".
DATAPACKAGE = ("_idToName", "_nameToId", "_idToLabel")

# ⚠ The leading "." is what separates a CALL from the DECLARATION. Without it
# this matched `public Task SendLocationCountAsync(int n) => ...` in the pipe
# server and failed the clean tree — and a gate that cries on correct code is
# a gate somebody switches off.
CALL = re.compile(r"\.SendLocationCountAsync\s*\(([^;]*?)\)\s*;")


def check(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    bad: list[str] = []
    for m in CALL.finditer(text):
        arg = m.group(1).strip()
        line = text.count("\n", 0, m.start()) + 1
        if any(a in arg for a in ALLOWED):
            continue
        why = next((d for d in DATAPACKAGE if d in arg), None)
        bad.append(
            f"{path.name}:{line}: SendLocationCountAsync({arg})"
            + (f"\n    {why} is the DATAPACKAGE — the same size for every seed."
               "\n    Use SeedLocationCount() (AP session) or seed.Placements.Count"
               " (standalone)." if why else
               "\n    Not a recognised seed-sized source; add it to ALLOWED if it"
               " really is one.")
        )
    return bad


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    files = ([pathlib.Path(a) for a in sys.argv[1:]]
             or [p for p in root.glob("*.cs")])
    bad: list[str] = []
    scanned = 0
    for f in files:
        if not f.is_file():
            continue
        scanned += 1
        bad += check(f)

    # ⚠ A lint that silently scans nothing is worse than no lint: it reports
    # success forever. Say what was covered.
    calls = sum(len(CALL.findall(f.read_text(encoding="utf-8")))
                for f in files if f.is_file())
    print(f"  {scanned} filer, {calls} kald til SendLocationCountAsync")
    for b in bad:
        print("  FAIL " + b)
    if calls == 0:
        print("  FAIL ingen kald fundet — leder linten det rigtige sted?")
        return 1
    print("  ok  hvert kald henter sit tal fra seedet" if not bad
          else f"\n{len(bad)} fejl")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
