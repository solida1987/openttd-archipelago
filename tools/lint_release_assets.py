# -*- coding: utf-8 -*-
"""Every address we publish must answer on the NEWEST release.

⚠⚠ WHAT THIS CAUGHT

plugin.json points London at

    releases/latest/download/openttd_archipelago.londonplugin

and the catalogue points the AP-worlds button at

    releases/latest/download/openttd.apworld

"latest" moves the moment a new release is tagged. v2.1.3 was created with the
game package and the apworld but WITHOUT the plugin, and that URL went 404 for
everyone the instant the tag landed — the release looked complete, and
installing the plugin was broken.

Nothing in the release step notices: `gh release create` succeeds, the assets
it was given are all there, and the one that is missing is missing precisely
because nobody passed it.

So this asks the internet, not the release script.

    py -3.13 tools/lint_release_assets.py

⚠ A fresh upload can 404 for a minute or two while GitHub's CDN catches up, so
each address is retried before it is called broken. A real miss stays missing.
"""
from __future__ import annotations

import sys
import time
import urllib.error
import urllib.request

sys.stdout.reconfigure(encoding="utf-8")

REPO = "solida1987/openttd-archipelago"

# Every bare name something out there resolves through releases/latest.
# The bare name IS the download: a versioned asset alone gives 404 for anyone
# whose URL does not carry the version, which is all of them.
REQUIRED = {
    "game_package.zip": "the game itself — London's install and update",
    "openttd.apworld": "catalog/apworlds.json, and the Update AP Worlds button",
    "openttd_archipelago.londonplugin": "plugin.json's packageUrl",
}

RETRIES = 6
GAP = 20


def head(url: str) -> int:
    req = urllib.request.Request(url, method="GET",
                                 headers={"User-Agent": "release-gate",
                                          "Range": "bytes=0-0"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return 0


def main() -> int:
    bad = []
    for name, who in REQUIRED.items():
        url = f"https://github.com/{REPO}/releases/latest/download/{name}"
        code = 0
        for attempt in range(RETRIES):
            code = head(url)
            if code in (200, 206):
                break
            if attempt < RETRIES - 1:
                print(f"  ...  {name}: {code}, venter på CDN'en "
                      f"({attempt + 1}/{RETRIES - 1})")
                time.sleep(GAP)
        if code in (200, 206):
            print(f"  ok    {name}")
        else:
            bad.append(f"{name} answers {code} on releases/latest — {who}")

    print()
    for b in bad:
        print("  FAIL  " + b)
    if bad:
        print(f"\n{len(bad)} fejl")
        return 1
    print("0 fejl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
