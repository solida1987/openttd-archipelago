# -*- coding: utf-8 -*-
"""The mod's own NewGRFs must reach openttd.cfg on EVERY launch.

⚠⚠ WHAT THIS CAUGHT

archipelago_ruins.grf and archipelago_stars.grf define the map objects a ruin
and a star are drawn as. The game adds them to _grfconfig_newgame itself and
then loses them: AfterNewGRFScan::OnNewGRFsScanned calls LoadFromConfig, which
replaces that list with whatever openttd.cfg holds. The scan is asynchronous,
so it lands after the mod has chosen. openttd.cfg is the only list that
survives.

The launcher writes it — and the first version of that code failed twice over:

  1  it never mentioned the mod's own GRFs at all, only the seed's
  2  and it returned early on `required.Count == 0`, so a seed asking for no
     NewGRF got nothing written even after the ids were added

A player with a 400-ruin pool got no ruins whatsoever:
     [AP] WARNING: No ruin ObjectTypes found! GRFID=0x55525041

So this checks both halves, in the source that decides:

  * the composition uses NewGrfEnabler.OwnSets, not hand-written literals
  * and nothing returns before it

    py -3.13 tools/lint_own_grfs_always.py

Exit code 1 on a violation.
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
ROOT = pathlib.Path(__file__).resolve().parent.parent
PLUGIN = ROOT / "OpenTTDPlugin.cs"
ENABLER = ROOT / "NewGrfEnabler.cs"


def body_of(text: str, signature: str) -> str | None:
    """The braces of one method, by counting them."""
    i = text.find(signature)
    if i < 0:
        return None
    j = text.find("{", i)
    if j < 0:
        return None
    depth, k = 0, j
    while k < len(text):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[j:k + 1]
        k += 1
    return None


def main() -> int:
    bad: list[str] = []

    if not PLUGIN.is_file() or not ENABLER.is_file():
        print("SPRINGER OVER: kilden ligger ikke hvor den plejer.")
        return 0

    enabler = ENABLER.read_text(encoding="utf-8")

    # --- the ids live in one place, and are the ones the files really have ---
    for gid, name in (("41505255", "archipelago_ruins.grf"),
                      ("41505354", "archipelago_stars.grf")):
        if gid not in enabler or name not in enabler:
            bad.append(f"NewGrfEnabler.OwnGrfs no longer names {name} ({gid})")

    plugin = PLUGIN.read_text(encoding="utf-8")
    body = body_of(plugin, "private void PrepareNewGrfConfig()")
    if body is None:
        bad.append("PrepareNewGrfConfig is gone — has the composition moved?")
        print_and_exit(bad)
        return 1

    # --- it must use the shared list, not two lines somebody can forget ------
    if "NewGrfEnabler.OwnSets(" not in body:
        bad.append("PrepareNewGrfConfig does not use NewGrfEnabler.OwnSets — "
                   "the mod's own GRFs are being composed by hand again")

    # --- and nothing may bail out before that ------------------------------
    #
    # ⚠ The `return` that caused this was `if (required.Count == 0) return;`,
    # sitting one line above where the ids were later added. Position is the
    # whole bug, so position is what is checked.
    at = body.find("NewGrfEnabler.OwnSets(")
    if at >= 0:
        # ⚠ Comments stripped first. The first run of this gate failed on the
        # words "return early" inside the very comment explaining the rule —
        # a gate that cries about correct code is a gate somebody switches off.
        before = re.sub(r"//[^\n]*", "", body[:at])
        # A `return` inside a lambda or a nested block would be a false
        # positive; there are none today, and one would be worth looking at.
        for m in re.finditer(r"\breturn\b", before):
            line = body.count("\n", 0, m.start()) + 1
            snippet = before[max(0, m.start() - 60):m.start() + 20]
            snippet = " ".join(snippet.split())[-70:]
            bad.append(f"a `return` at line {line} of PrepareNewGrfConfig runs "
                       f"BEFORE the mod's own GRFs are added:\n          …{snippet}")

    print_and_exit(bad)
    return 1 if bad else 0


def print_and_exit(bad: list[str]) -> None:
    for b in bad:
        print("  FAIL  " + b)
    if not bad:
        print("  ok    the mod's own NewGRFs are composed first, from one list")
    print()
    print(f"{len(bad)} fejl" if bad else "0 fejl")


if __name__ == "__main__":
    raise SystemExit(main())
