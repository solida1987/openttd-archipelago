"""Every option combination must produce as many items as it has locations.

⚠⚠ WHAT THIS CAUGHT

pre_fill pulled every trap and utility item out of the pool and then placed
them only "while non_shop_locs" -- so a surplus was dropped on the floor, and
each dropped item left a location nothing could fill. AP's Fill then died with
"Unable to fill all locations", naming shop slots that had nothing to do with
it. The arithmetic at utility_count=300 with 400 ruins: 730 trap/utility items
(the padding is drawn from UTILITY_ITEMS too, so it lands in the same list)
for 600 non-shop places = 130 dropped, plus 5 precollected starting vehicles
= the 135 locations Fill reported.

None of that is visible from reading a diff. It is visible the moment you
generate at the edges of the ranges, which is what this does.

    py -3.13 tools/pool_balance_proof.py [<path to ArchipelagoGenerate.exe>]

Exit code 1 if any configuration fails to generate.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

AP_DEFAULT = pathlib.Path(r"C:\ProgramData\Archipelago\ArchipelagoGenerate.exe")
BASE = pathlib.Path(__file__).resolve().parent.parent.parent / "Reference"

# A YAML with every option at its default, so each case below differs only in
# the fields it names. Built from the world's own option defaults.
TEMPLATE = """\
name: {name}
description: pool balance proof
game: OpenTTD
OpenTTD:
  accessibility: full
  starting_vehicle_type: 0
  starting_vehicle_count: 5
  win_difficulty: 4
  enable_traps: true
  trap_breakdown_wave: true
  trap_maintenance_surge: true
  trap_signal_failure: true
  trap_fuel_shortage: true
  start_year: 1950
  map_size_x: 8
  map_size_y: 8
  landscape: 0
  enable_shark_ships: true
  enable_hover_vehicles: true
  enable_vactrain: true
  enable_wagon_unlocks: true
  enable_rail_direction_unlocks: true
  enable_road_direction_unlocks: true
  enable_signal_unlocks: true
  enable_bridge_unlocks: true
  enable_tunnel_unlocks: true
  enable_airport_unlocks: true
  enable_tree_unlocks: true
  enable_terraform_unlocks: true
  enable_town_action_unlocks: true
  speed_boost_count: 20
{extra}"""

# ⭐ The edges, not the middle. Each of these was chosen because it stresses a
# different side of the trap/utility-versus-non-shop-locations balance.
CASES = {
    # As shipped. Regression guard: this is what players already have.
    "defaults":        dict(trap_count=10,  utility_count=20,  ruin_pool_size=25),
    # No ruins at all -- missions are then the ONLY non-shop home for traps
    # and utility, which is the tightest the balance ever gets.
    "no-ruins-max-util": dict(trap_count=50, utility_count=300, ruin_pool_size=0),
    # Nothing to place: the empty-list path through pre_fill.
    "no-traps-min-util": dict(trap_count=0,  utility_count=5,   ruin_pool_size=0),
    # The shape a long game wants.
    "big":             dict(trap_count=30,  utility_count=300, ruin_pool_size=400),
    # Everything at its ceiling.
    "max":             dict(trap_count=50,  utility_count=300, ruin_pool_size=500,
                            max_active_ruins=10),
}


def generate(gen: pathlib.Path, name: str, opts: dict):
    work = pathlib.Path(tempfile.mkdtemp(prefix="poolproof_"))
    players, out = work / "Players", work / "out"
    players.mkdir()
    out.mkdir()
    extra = "".join(f"  {k}: {v}\n" for k, v in opts.items())
    (players / f"{name}.yaml").write_text(
        TEMPLATE.format(name=name, extra=extra), encoding="utf-8")
    r = subprocess.run(
        [str(gen), "--player_files_path", str(players), "--outputpath", str(out),
         "--seed", "1", "--spoiler", "3"],
        capture_output=True, text=True, errors="replace", timeout=3600)
    zips = list(out.glob("*.zip"))
    if not zips:
        blob = (r.stderr or "") + (r.stdout or "")
        # The one line that says what actually went wrong.
        why = next((l.strip() for l in blob.splitlines()
                    if "Error" in l or "error" in l or "Exception" in l), "")
        n_unfilled = re.search(r"Unfilled locations\((\d+)\)", blob)
        if n_unfilled:
            why = (f"{n_unfilled.group(1)} locations had no item — the pool and "
                   "the location table are out of step")
        shutil.rmtree(work, ignore_errors=True)
        return None, why
    with zipfile.ZipFile(zips[0]) as z:
        text = z.read(next(n for n in z.namelist()
                           if n.endswith(".txt"))).decode("utf-8", "replace")
    m = re.search(r"Location Count:\s*(\d+)", text)
    shutil.rmtree(work, ignore_errors=True)
    return (int(m.group(1)) if m else None), ""


def main() -> int:
    gen = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else AP_DEFAULT
    if not gen.is_file():
        print(f"  SKIPPED: no generator at {gen}")
        return 0

    ok = fail = 0
    for name, opts in CASES.items():
        # ⚠ flush: each case takes minutes, and Python block-buffers stdout the
        # moment it is redirected to a file. Without this the whole run looks
        # hung until it finishes, which is how one of these got killed.
        print(f"  ...  {name:20}", end="", flush=True)
        n, why = generate(gen, name, opts)
        if n is None:
            print(f"\r  FAIL {name:20} {why}", flush=True)
            fail += 1
        else:
            print(f"\r  ok   {name:20} {n} locations", flush=True)
            ok += 1
    print(f"\n{ok} ok, {fail} fejl")
    return 1 if fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
