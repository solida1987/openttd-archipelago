"""Every option combination must produce a seed that can actually be finished.

⚠⚠ WHAT THIS CAUGHT

pre_fill pulled every trap and utility item out of the pool and then placed
them only "while non_shop_locs" -- so a surplus was dropped on the floor, and
each dropped item left a location nothing could fill. AP's Fill then died with
"Unable to fill all locations", naming shop slots that had nothing to do with
it. The arithmetic at utility_count=300 with 400 ruins: 730 trap/utility items
(the padding is drawn from UTILITY_ITEMS too, so it lands in the same list)
for 600 non-shop places = 130 dropped, plus 5 precollected starting vehicles
= the 135 locations Fill reported.

⚠⚠ AND WHAT IT MISSED, BECAUSE IT ONLY EVER MOVED THREE DIALS

The first version varied trap_count, utility_count and ruin_pool_size and left
everything else at its default -- and set map_size_x/y to 8, which is not even
a legal value for the Choice (3..6), so it cannot have run green. Four whole
classes of failure sat outside what it looked at:

  - the shop grew past the class-level registry (858 slots against 600 names)
    as soon as enable_stars was off, because the stars are what normally halve
    the remainder;
  - the vehicle gates in rules.py multiply two option values together and ran
    to 200 where a Toyland seed can hand out 52 vehicles;
  - win_target_missions asked for 70 where the seed had 46 missions;
  - multiplayer_mode promised Ruins/Colby/Demigods were off and the pool
    computation never heard about it.

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

# Every option each case starts from. Cases override by key, so no option is
# ever written twice into the same YAML.
DEFAULTS = {
    "accessibility": "full",
    "starting_vehicle_type": 0,
    "starting_vehicle_count": 5,
    "win_difficulty": 4,
    "enable_traps": "true",
    "trap_breakdown_wave": "true",
    "trap_maintenance_surge": "true",
    "trap_signal_failure": "true",
    "trap_fuel_shortage": "true",
    "start_year": 1950,
    # ⚠ 3..6 = 512..4096. The old file said 8, which the Choice rejects.
    "map_size_x": 3,
    "map_size_y": 3,
    "landscape": 0,
    "enable_shark_ships": "true",
    "enable_hover_vehicles": "true",
    "enable_vactrain": "true",
    "enable_wagon_unlocks": "true",
    "enable_rail_direction_unlocks": "true",
    "enable_road_direction_unlocks": "true",
    "enable_signal_unlocks": "true",
    "enable_bridge_unlocks": "true",
    "enable_tunnel_unlocks": "true",
    "enable_airport_unlocks": "true",
    "enable_tree_unlocks": "true",
    "enable_terraform_unlocks": "true",
    "enable_town_action_unlocks": "true",
    "speed_boost_count": 20,
    "trap_count": 10,
    "utility_count": 20,
    "ruin_pool_size": 25,
}

# Every GRF that adds vehicles. Turning them all on is what pushes the item
# pool -- and with it the shop -- to its ceiling.
ALL_GRFS = {
    "enable_iron_horse": "true",
    "enable_military_items": "true",
    "enable_shark_ships": "true",
    "enable_hover_vehicles": "true",
    "enable_heqs": "true",
    "enable_vactrain": "true",
    "enable_aircraftpack": "true",
}

# ⭐ The edges, not the middle. Each of these was chosen because it stresses a
# different side of the balance, and the second block exists because the first
# block would have passed every one of the four findings above.
CASES = {
    # ── pool balance: traps and utility against the non-shop locations ──
    # As shipped. Regression guard: this is what players already have.
    "defaults":          dict(trap_count=10, utility_count=20, ruin_pool_size=25),
    # No ruins at all -- missions are then the ONLY non-shop home for traps
    # and utility, which is the tightest the balance ever gets.
    "no-ruins-max-util": dict(trap_count=50, utility_count=300, ruin_pool_size=0),
    # Nothing to place: the empty-list path through pre_fill.
    "no-traps-min-util": dict(trap_count=0, utility_count=5, ruin_pool_size=0),
    # The shape a long game wants.
    "big":               dict(trap_count=30, utility_count=300, ruin_pool_size=400),
    # Everything at its ceiling.
    "max":               dict(trap_count=50, utility_count=300, ruin_pool_size=500,
                              max_active_ruins=10),

    # ── shop against the class-level location registry ──────────────────
    # enable_stars off hands the WHOLE remainder to the shop instead of
    # halving it, and every GRF plus every unlock toggle makes that remainder
    # as large as it gets. This is the case that produced Shop_Purchase_0601.
    "shop-ceiling":      dict(landscape=2, enable_stars="false", trap_count=50,
                              utility_count=300, speed_boost_count=100,
                              ruin_pool_size=0, **ALL_GRFS),

    # ── vehicle gates against the vehicle supply ────────────────────────
    # Toyland has the smallest vehicle pool there is, and these three options
    # multiply out to a Hard gate of 100, an Extreme gate of 200 and a Victory
    # gate of 200 -- against 52 vehicles that can ever exist.
    "toyland-gates":     dict(landscape=3, enable_wagon_unlocks="false",
                              mission_tier_unlock_count=20,
                              hard_tier_vehicle_multiplier=5,
                              extreme_tier_vehicle_multiplier=10,
                              victory_vehicle_requirement=50),
    # The same dials on the largest vehicle pool, so the cap is proved as a
    # ratio and not just at one point.
    "temperate-gates":   dict(landscape=0, enable_wagon_unlocks="false",
                              mission_tier_unlock_count=20,
                              hard_tier_vehicle_multiplier=5,
                              extreme_tier_vehicle_multiplier=10,
                              victory_vehicle_requirement=50, **ALL_GRFS),

    # ── win target against the mission count ───────────────────────────
    # The smallest seed the options allow, asked for the largest win target.
    "small-pool-big-goal": dict(landscape=3, enable_wagon_unlocks="false",
                                trap_count=0, utility_count=5,
                                ruin_pool_size=0, enable_stars="false",
                                win_difficulty=10,
                                win_custom_missions_completed=500),

    # ── multiplayer mode against the location table ────────────────────
    # Asks for the content multiplayer mode disables. Every Ruin_ and
    # Demigod_ location it still built was one nobody could ever check.
    "multiplayer":       dict(multiplayer_mode="true", ruin_pool_size=500,
                              enable_demigods="true", demigod_count=10,
                              colby_event="true", enable_stars="true",
                              star_pool_size=1000),

    # ── wagons off: the DEFAULT, and where the pool count drifted ──────
    "wagons-off-temperate": dict(landscape=0, enable_wagon_unlocks="false"),
    "wagons-off-arctic":    dict(landscape=1, enable_wagon_unlocks="false"),
    "wagons-off-tropic":    dict(landscape=2, enable_wagon_unlocks="false"),

    # ── economies with no Goods cargo ──────────────────────────────────
    # Steeltown and Arctic Basic have no Goods, so the "deliver goods in one
    # year" missions and the Colby event had nothing to measure.
    "firs-steeltown":    dict(enable_firs="true", firs_economy=3,
                              colby_event="true"),
    "firs-arctic-basic": dict(landscape=1, enable_firs="true", firs_economy=1,
                              colby_event="true"),
}


# What the seed must look like afterwards, beyond "it generated at all".
# Generation surviving is not the whole proof: multiplayer_mode ignoring its
# own promise builds Ruin_ locations that generate perfectly well and then
# hang the multiworld, because nothing in the session can ever check them.
EXPECT = {
    # Stars are in this list because the game disables FIVE things in
    # multiplayer, not four -- archipelago_manager.cpp clears enable_stars
    # alongside the rest, and a thousand Star_ locations nobody can check hang
    # the multiworld just as surely as the ruins do.
    "multiplayer": {"ruins": 0, "demigods": 0, "stars": 0},
}

# No case may ever ask for more shop slots than the registry in __init__.py
# carries names for. Read from the world itself, not copied here — a copy
# would go stale exactly when it mattered.
_WORLD_INIT = (pathlib.Path(__file__).resolve().parent.parent
               / "apworld" / "openttd" / "__init__.py")
_m = re.search(r"^SHOP_REGISTRY_SIZE\s*=\s*(\d+)",
               _WORLD_INIT.read_text(encoding="utf-8"), re.M)
SHOP_REGISTRY_SIZE = int(_m.group(1)) if _m else 600

# The world prints its own pool shape during generation; this is that line.
POOL_LINE = re.compile(
    r"\[OpenTTD\].*?(\d+) missions \+ (\d+) shop \+ (\d+) ruins \+ "
    r"(\d+) demigods \+ (\d+) stars")


def check_pool(name: str, blob: str) -> str:
    """Read the world's own pool report back and hold it to the case."""
    m = POOL_LINE.search(blob)
    if not m:
        return ""      # older build, or the line moved -- not a failure here
    missions, shop, ruins, demigods, stars = (int(g) for g in m.groups())
    if shop > SHOP_REGISTRY_SIZE:
        return (f"{shop} shop slots against {SHOP_REGISTRY_SIZE} registered "
                "names — Shop_Purchase_ beyond the registry has no id")
    for key, want in EXPECT.get(name, {}).items():
        got = dict(missions=missions, shop=shop, ruins=ruins,
                   demigods=demigods, stars=stars)[key]
        if got != want:
            return f"{got} {key}, expected {want}"
    return ""


def write_yaml(path: pathlib.Path, name: str, opts: dict) -> None:
    merged = dict(DEFAULTS)
    merged.update(opts)
    lines = [f"name: {name}", "description: pool balance proof",
             "game: OpenTTD", "OpenTTD:"]
    lines += [f"  {k}: {v}" for k, v in merged.items()]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate(gen: pathlib.Path, name: str, opts: dict):
    work = pathlib.Path(tempfile.mkdtemp(prefix="poolproof_"))
    players, out = work / "Players", work / "out"
    players.mkdir()
    out.mkdir()
    write_yaml(players / f"{name}.yaml", name, opts)
    r = subprocess.run(
        [str(gen), "--player_files_path", str(players), "--outputpath", str(out),
         "--seed", "1", "--spoiler", "3"],
        capture_output=True, text=True, errors="replace", timeout=3600)
    blob = (r.stderr or "") + (r.stdout or "")
    zips = list(out.glob("*.zip"))
    if not zips:
        # ⚠ The one line that says what actually went wrong -- and it has to be
        # OUR line. Other apworlds in custom_worlds throw their own tracebacks
        # into the same stderr while the generator loads them, and a plain
        # "first line containing Error" picked one of those up and reported a
        # missing directory for a seed that had died in Fill.
        why = ""
        for pattern in (r"^\s*(Fill\.FillError: .*)$",
                        r"^\s*(\w*Error: .*)$",
                        r"^\s*(Exception: .*)$"):
            m = re.search(pattern, blob, re.M)
            if m:
                why = m.group(1).strip()
                break
        n_unfilled = re.search(r"Unfilled locations\((\d+)\)", blob)
        if n_unfilled:
            why = (f"{n_unfilled.group(1)} locations had no item — the pool and "
                   "the location table are out of step")
        shutil.rmtree(work, ignore_errors=True)
        return None, (why or "generation produced no seed")
    with zipfile.ZipFile(zips[0]) as z:
        text = z.read(next(n for n in z.namelist()
                           if n.endswith(".txt"))).decode("utf-8", "replace")
    m = re.search(r"Location Count:\s*(\d+)", text)
    shutil.rmtree(work, ignore_errors=True)

    why = check_pool(name, blob)
    if why:
        return None, why
    return (int(m.group(1)) if m else None), ""


def main() -> int:
    gen = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else AP_DEFAULT
    if not gen.is_file():
        print(f"  SKIPPED: no generator at {gen}")
        return 0

    only = sys.argv[2] if len(sys.argv) > 2 else ""
    ok = fail = 0
    for name, opts in CASES.items():
        if only and only not in name:
            continue
        # ⚠ flush: each case takes minutes, and Python block-buffers stdout the
        # moment it is redirected to a file. Without this the whole run looks
        # hung until it finishes, which is how one of these got killed.
        print(f"  ...  {name:22}", end="", flush=True)
        n, why = generate(gen, name, opts)
        if n is None:
            print(f"\r  FAIL {name:22} {why}", flush=True)
            fail += 1
        else:
            print(f"\r  ok   {name:22} {n} locations", flush=True)
            ok += 1
    print(f"\n{ok} ok, {fail} failed")
    return 1 if fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
