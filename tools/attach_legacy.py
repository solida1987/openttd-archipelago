# -*- coding: utf-8 -*-
"""Carry the legacy build onto a release.

The pre-launcher build (its own in-game Archipelago client) is frozen, but it
stays available: it rides along as a plain zip on EVERY release, so nobody has
to dig through old tags to find it. There is no separate legacy release.

The file is not kept in git -- 43 MB of binary would land in every clone. It
is copied forward from the newest release that already carries it, so the
chain sustains itself as long as one release has it.

    python tools/attach_legacy.py --tag v2.1.0

Fails loudly rather than shipping a release without it.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "solida1987/openttd-archipelago"
PREFIX = "openttd-archipelago-legacy-"


def gh_json(args):
    out = subprocess.run(["gh"] + args, capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    if out.returncode != 0:
        raise SystemExit("gh failed: " + (out.stderr or "").strip())
    return json.loads(out.stdout)


def find_legacy_asset():
    """(release tag, asset name) of the newest release carrying the legacy zip."""
    releases = gh_json(["api", f"repos/{REPO}/releases", "--paginate"])
    for rel in releases:
        for asset in rel.get("assets", []):
            if asset["name"].startswith(PREFIX) and asset["name"].endswith(".zip"):
                return rel["tag_name"], asset["name"]
    return None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True, help="release tag to attach it to")
    ap.add_argument("--local", help="path to the legacy zip (first time only)")
    args = ap.parse_args()

    target = gh_json(["api", f"repos/{REPO}/releases/tags/{args.tag}"])
    if any(a["name"].startswith(PREFIX) for a in target.get("assets", [])):
        print("legacy-zippen ligger allerede paa " + args.tag)
        return 0

    path = args.local
    tmp = None
    if path is None:
        tag, name = find_legacy_asset()
        if name is None:
            raise SystemExit(
                "no release carries the legacy zip yet -- pass --local <path> once")
        tmp = tempfile.mkdtemp(prefix="ottd_legacy_")
        print("henter %s fra %s" % (name, tag))
        subprocess.run(["gh", "release", "download", tag, "--pattern", name,
                        "--dir", tmp, "--repo", REPO], check=True)
        path = os.path.join(tmp, name)

    if not os.path.isfile(path):
        raise SystemExit("legacy zip not found: " + path)

    gate = subprocess.run(
        [sys.executable, os.path.join(ROOT, "tools", "lint_no_foreign_grf.py"), path],
        capture_output=True, text=True)
    print(gate.stdout.strip())
    if gate.returncode != 0:
        raise SystemExit("foreign-GRF gate FAILED on the legacy zip")

    subprocess.run(["gh", "release", "upload", args.tag, path, "--repo", REPO],
                   check=True)
    print("legacy-zippen er paa " + args.tag)
    return 0


if __name__ == "__main__":
    sys.exit(main())
