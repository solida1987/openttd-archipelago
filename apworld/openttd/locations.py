from typing import Dict, List, NamedTuple, Optional
from BaseClasses import LocationProgressType

OPENTTD_LOC_BASE_ID = 6_100_000

# Cargo types split by climate so mission generator can pick
# only cargos that actually exist on the chosen map landscape.
CARGO_BY_LANDSCAPE = {
    0: [  # Temperate
        "Passengers", "Mail", "Coal", "Oil",
        "Livestock", "Goods", "Grain", "Wood",
        "Iron Ore", "Steel", "Valuables",
    ],
    1: [  # Arctic
        "Passengers", "Mail", "Coal", "Oil",
        "Livestock", "Goods", "Wheat", "Wood",
        "Paper", "Food", "Gold",
    ],
    2: [  # Tropical
        "Passengers", "Mail", "Oil",
        "Goods", "Maize", "Wood",
        "Rubber", "Fruit", "Copper Ore", "Water", "Food", "Diamonds",
    ],
    3: [  # Toyland
        "Passengers", "Mail",
        "Sweets", "Cola", "Candyfloss", "Bubbles",
        "Plastic", "Fizzy Drinks", "Toffee",
    ],
}

# Backwards compat — used for class-level access before landscape is known
CARGO_TYPES = CARGO_BY_LANDSCAPE[0]

# ─────────────────────────────────────────────────────────────────────────────
#  MISSION TEMPLATES
#  Each entry: (description_template, amount_min, amount_max, unit)
#
#  SPACING RULES (enforced in _generate_missions):
#  - No two missions of the same type+unit may have amounts closer than 5×
#    e.g. "Have X vehicles": 3 → next must be ≥15, then ≥75
#  - No exact duplicates (same type + same amount)
#  - "Buy a vehicle from the shop" is capped at 1 per difficulty level
# ─────────────────────────────────────────────────────────────────────────────

MISSION_TEMPLATES = {
    # ── EASY ─────────────────────────────────────────────────────────────
    # Simple objectives suitable for new players building their first routes.
    # Unique to easy: "Buy a vehicle from the shop", "Build X stations".
    # Named missions (passengers_to_town / mail_to_town) force map exploration.
    "easy": [
        ("Transport {amount} units of {cargo}",              2_000,   15_000,  "units"),
        ("Earn £{amount} total profit",                      80_000,  300_000, "£"),
        ("Have {amount} vehicles running simultaneously",    3,       6,       "vehicles"),
        ("Service {amount} different towns",                 2,       4,       "towns"),
        ("Transport {amount} passengers",                    3_000,   20_000,  "passengers"),
        ("Buy a vehicle from the shop",                      1,       1,       "purchase"),
        ("Build {amount} stations",                          3,       6,       "stations"),
        ("Earn £{amount} in one month",                      8_000,   40_000,  "£/month"),
        ("Have {amount} trains running simultaneously",      2,       5,       "trains"),
        ("Have {amount} road vehicles running simultaneously", 2,     6,       "road vehicles"),
        # Named-destination missions — C++ fills in real town/industry name at session start
        ("Deliver {amount} passengers to [Town]",            500,     4_000,   "passengers_to_town"),
        ("Deliver {amount} mail to [Town]",                  300,     3_000,   "mail_to_town"),
    ],
    # ── MEDIUM ───────────────────────────────────────────────────────────
    # Introduces ships, aircraft, station quality and multi-city rail networks.
    # Unique to medium: "Deliver X tons to one station", "Maintain 75%".
    # Named missions force players to build routes to specific map locations.
    "medium": [
        ("Transport {amount} units of {cargo}",                   30_000,  150_000,  "units"),
        ("Earn £{amount} total profit",                           800_000, 4_000_000,"£"),
        ("Have {amount} vehicles running simultaneously",         15,      40,       "vehicles"),
        ("Service {amount} different towns",                      8,       16,       "towns"),
        ("Transport {amount} passengers",                         80_000,  400_000,  "passengers"),
        ("Deliver {amount} tons of {cargo} to one station",      8_000,   50_000,   "tons"),
        ("Maintain 75%+ station rating for {amount} months",     4,       10,       "months"),
        ("Earn £{amount} in one month",                           120_000, 600_000,  "£/month"),
        ("Have {amount} trains running simultaneously",           8,       20,       "trains"),
        ("Connect {amount} cities with rail",                     4,       8,        "cities"),
        ("Have {amount} ships running simultaneously",            2,       6,        "ships"),
        ("Have {amount} aircraft running simultaneously",         3,       8,        "aircraft"),
        ("Have {amount} road vehicles running simultaneously",    12,      30,       "road vehicles"),
        # Named-destination missions
        ("Deliver {amount} passengers to [Town]",                 5_000,   40_000,   "passengers_to_town"),
        ("Deliver {amount} mail to [Town]",                       3_000,   20_000,   "mail_to_town"),
        ("Supply {amount} tons to [Industry near Town]",          5_000,   40_000,   "cargo_to_industry"),
        ("Transport {amount} tons from [Industry near Town]",     5_000,   40_000,   "cargo_from_industry"),
    ],
    # ── HARD ─────────────────────────────────────────────────────────────
    # Large-scale operations across all transport modes.
    # Unique to hard: "Deliver X tons of goods in one year", "Maintain 90%".
    # Named missions at hard scale require significant dedicated infrastructure.
    "hard": [
        ("Transport {amount} units of {cargo}",                   300_000,  1_500_000,"units"),
        ("Earn £{amount} total profit",                           8_000_000,30_000_000,"£"),
        ("Have {amount} vehicles running simultaneously",         60,       120,      "vehicles"),
        ("Service {amount} different towns",                      20,       40,       "towns"),
        ("Transport {amount} passengers",                         800_000,  3_000_000,"passengers"),
        ("Deliver {amount} tons of goods in one year",            150_000,  700_000,  "tons"),
        ("Earn £{amount} in one month",                           800_000,  4_000_000,"£/month"),
        ("Have {amount} trains running simultaneously",           25,       60,       "trains"),
        ("Maintain 90%+ station rating for {amount} months",     8,        20,       "months"),
        ("Have {amount} ships running simultaneously",            8,        20,       "ships"),
        ("Have {amount} aircraft running simultaneously",         10,       25,       "aircraft"),
        ("Have {amount} road vehicles running simultaneously",    30,       70,       "road vehicles"),
        ("Connect {amount} cities with rail",                     12,       25,       "cities"),
        # Named-destination missions
        ("Deliver {amount} passengers to [Town]",                 80_000,   500_000,  "passengers_to_town"),
        ("Deliver {amount} mail to [Town]",                       40_000,   200_000,  "mail_to_town"),
        ("Supply {amount} tons to [Industry near Town]",          50_000,   300_000,  "cargo_to_industry"),
        ("Transport {amount} tons from [Industry near Town]",     50_000,   300_000,  "cargo_from_industry"),
    ],
    # ── EXTREME ──────────────────────────────────────────────────────────
    # Megacorporation-level targets. No generic "vehicles" count — only
    # mode-specific fleets to push players towards a diversified empire.
    # Unique to extreme: "Earn in a single month" (vs "in one month"), huge cargo haul.
    "extreme": [
        ("Transport {amount} units of {cargo}",                   3_000_000, 15_000_000,"units"),
        ("Earn £{amount} total profit",                           80_000_000,300_000_000,"£"),
        ("Service {amount} different towns",                      60,        100,       "towns"),
        ("Transport {amount} passengers",                         8_000_000, 30_000_000,"passengers"),
        ("Earn £{amount} in a single month",                      8_000_000, 30_000_000,"£/month"),
        ("Have {amount} trains running simultaneously",           100,       200,       "trains"),
        ("Have {amount} aircraft running simultaneously",         30,        80,        "aircraft"),
        ("Have {amount} road vehicles running simultaneously",    80,        200,       "road vehicles"),
        ("Have {amount} ships running simultaneously",            20,        60,        "ships"),
        ("Connect {amount} cities with rail",                     40,        80,        "cities"),
        ("Deliver {amount} tons of {cargo} to one station",      500_000,   2_000_000, "tons"),
        # Named-destination missions — extreme scale requires massive dedicated networks
        ("Deliver {amount} passengers to [Town]",                 500_000,   3_000_000,  "passengers_to_town"),
        ("Supply {amount} tons to [Industry near Town]",          300_000,   2_000_000,  "cargo_to_industry"),
        ("Transport {amount} tons from [Industry near Town]",     300_000,   2_000_000,  "cargo_from_industry"),
    ],
}

DIFFICULTY_DISTRIBUTION = {
    "easy":    0.30,
    "medium":  0.35,
    "hard":    0.25,
    "extreme": 0.10,
}


class OpenTTDLocationData(NamedTuple):
    code: int
    region: str
    progress_type: LocationProgressType = LocationProgressType.DEFAULT


def _build_location_table(mission_count: int = 100, shop_slots: int = 5) -> Dict[str, OpenTTDLocationData]:
    table: Dict[str, OpenTTDLocationData] = {}
    code = OPENTTD_LOC_BASE_ID

    for difficulty, fraction in DIFFICULTY_DISTRIBUTION.items():
        count = max(1, int(mission_count * fraction))
        for i in range(1, count + 1):
            name = f"Mission_{difficulty.capitalize()}_{i:03d}"
            pt = (LocationProgressType.PRIORITY
                  if difficulty == "extreme"
                  else LocationProgressType.DEFAULT)
            table[name] = OpenTTDLocationData(code, f"mission_{difficulty}", pt)
            code += 1

    for i in range(1, shop_slots * 20 + 1):
        name = f"Shop_Purchase_{i:04d}"
        table[name] = OpenTTDLocationData(code, "shop", LocationProgressType.DEFAULT)
        code += 1

    table["Goal_Victory"] = OpenTTDLocationData(code, "goal", LocationProgressType.PRIORITY)
    code += 1

    return table


LOCATION_TABLE: Dict[str, OpenTTDLocationData] = _build_location_table()


def get_location_table(mission_count: int, shop_slots: int) -> Dict[str, OpenTTDLocationData]:
    return _build_location_table(mission_count, shop_slots)
