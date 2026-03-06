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
    "easy": [
        ("Transport {amount} units of {cargo}",         2_000,    15_000,   "units"),
        ("Earn £{amount} total profit",                 80_000,   300_000,  "£"),
        ("Have {amount} vehicles running simultaneously", 3,       6,        "vehicles"),
        ("Service {amount} different towns",            2,        4,        "towns"),
        ("Transport {amount} passengers",               3_000,    20_000,   "passengers"),
        ("Buy a vehicle from the shop",                 1,        1,        "purchase"),
        ("Build {amount} stations",                     3,        6,        "stations"),
        ("Earn £{amount} in one month",                 8_000,    40_000,   "£/month"),
    ],
    "medium": [
        ("Transport {amount} units of {cargo}",         30_000,   150_000,  "units"),
        ("Earn £{amount} total profit",                 800_000,  4_000_000,"£"),
        ("Have {amount} vehicles running simultaneously", 15,      40,       "vehicles"),
        ("Service {amount} different towns",            8,        16,       "towns"),
        ("Transport {amount} passengers",               80_000,   400_000,  "passengers"),
        ("Deliver {amount} tons of {cargo} to one station", 8_000, 50_000,  "tons"),
        ("Maintain 75%+ station rating for {amount} months", 4,   10,       "months"),
        ("Earn £{amount} in one month",                 120_000,  600_000,  "£/month"),
        ("Have {amount} trains running simultaneously", 8,        20,       "trains"),
        ("Connect {amount} cities with rail",           4,        8,        "cities"),
    ],
    "hard": [
        ("Transport {amount} units of {cargo}",         300_000,  1_500_000,"units"),
        ("Earn £{amount} total profit",                 8_000_000, 30_000_000,"£"),
        ("Have {amount} vehicles running simultaneously", 60,      120,      "vehicles"),
        ("Service {amount} different towns",            20,       40,       "towns"),
        ("Transport {amount} passengers",               800_000,  3_000_000,"passengers"),
        ("Deliver {amount} tons of goods in one year",  150_000,  700_000,  "tons"),
        ("Earn £{amount} in one month",                 800_000,  4_000_000,"£/month"),
        ("Have {amount} trains running simultaneously", 25,       60,       "trains"),
        ("Maintain 90%+ station rating for {amount} months", 8,   20,       "months"),
        ("Transport {amount} units of {cargo}",         15_000,   80_000,   "units"),  # replaces hardcoded valuables — cargo filtered by landscape
    ],
    "extreme": [
        ("Transport {amount} units of {cargo}",         3_000_000, 15_000_000,"units"),
        ("Earn £{amount} total profit",                 80_000_000, 300_000_000,"£"),
        ("Have {amount} vehicles running simultaneously", 150,     350,      "vehicles"),
        ("Service {amount} different towns",            60,       100,      "towns"),
        ("Transport {amount} passengers in total",      8_000_000, 30_000_000,"passengers"),
        ("Earn £{amount} in a single month",            8_000_000, 30_000_000,"£/month"),
        ("Have {amount} trains running simultaneously", 100,      200,      "trains"),
        ("Have {amount} aircraft running simultaneously", 30,     80,       "aircraft"),
        ("Transport {amount} units of {cargo}",         800_000,  3_000_000,"units"),  # replaces hardcoded steel — cargo filtered by landscape
        ("Have {amount} road vehicles running simultaneously", 80, 200,     "road vehicles"),
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
