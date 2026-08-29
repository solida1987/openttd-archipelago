# -*- coding: utf-8 -*-
"""Every setting the seed writes must also be locked against the player.

⚠⚠ WHY

A seed decides map size, economy, vehicle limits, station spread and forty
other things. All of them were editable in the settings window while a session
ran — and the change is saved, so it survives for the rest of the multiworld
while the player's game silently disagrees with everyone else's.

AP_ApplyGameSettings writes them; _ap_owned_settings lists them; and the two
live in the same file precisely so they can be compared. Adding a line to one
without the other leaves a setting the seed decides and the player can undo —
which looks like nothing at all until somebody's seed stops matching.

    py -3.13 tools/lint_settings_locked.py

Exit code 1 when the two lists disagree.
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "archipelago_manager.cpp"
SETTINGS = ROOT / "src" / "settings.cpp"


def main() -> int:
    if not SRC.is_file():
        print("SPRINGER OVER: kilden ligger ikke hvor den plejer.")
        return 0

    text = SRC.read_text(encoding="utf-8", errors="replace")

    # What the code actually writes.
    written = set(re.findall(r"_settings_newgame\.([A-Za-z_]+\.[A-Za-z_0-9]+)\s*=", text))

    # What the lock list claims.
    block = re.search(r"_ap_owned_settings\s*=\s*\{(.*?)\n\};", text, re.S)
    if block is None:
        print("  FAIL  _ap_owned_settings findes ikke længere — er låsen fjernet?")
        return 1
    listed = set(re.findall(r'"([A-Za-z_]+\.[A-Za-z_0-9]+)"', block.group(1)))

    missing = sorted(written - listed)
    extra = sorted(listed - written)

    print(f"  {len(written)} indstillinger skrives, {len(listed)} er låst")

    bad = False
    if missing:
        bad = True
        print(f"\n  FAIL  {len(missing)} indstilling(er) skrives af seedet, men er IKKE låst:")
        for n in missing:
            print(f"          {n}  — spilleren kan lave om på den bagefter")
    if extra:
        bad = True
        print(f"\n  FAIL  {len(extra)} indstilling(er) er låst, men skrives ikke længere:")
        for n in extra:
            print(f"          {n}  — låser noget seedet ikke ejer")

    # And the lock has to be reachable at all: one call, in the choke point.
    if SETTINGS.is_file():
        st = SETTINGS.read_text(encoding="utf-8", errors="replace")
        if "AP_IsSettingLocked" not in st:
            bad = True
            print("\n  FAIL  settings.cpp kalder ikke AP_IsSettingLocked — "
                  "listen findes, men ingen spørger den")
        else:
            print("  ok    SettingDesc::IsEditable spørger låsen")

    print()
    if bad:
        print("fejl")
        return 1
    print("0 fejl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
