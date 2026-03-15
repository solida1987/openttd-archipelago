from typing import Dict, List, NamedTuple, Optional
from BaseClasses import LocationProgressType

# Location ID layout — fixed blocks per difficulty so IDs are STABLE regardless
# of how many missions the generator produces. The AP server data package
# (built at class level with max counts) and the actual multiworld (built at
# runtime with real counts) must agree on which ID = which location name.
#
# Block map (each block = 2000 slots):
#   Easy:    6100000 – 6101999   (max 2000 easy missions)
#   Medium:  6102000 – 6103999
#   Hard:    6104000 – 6105999
#   Extreme: 6106000 – 6107999
#   Shop:    6108000 – 6109999
#   Victory: 6110000
#   Ruins:   6112000 – 6113999
#
# Item IDs are at 6_200_000+ (items.py) — separate range, no overlap.
OPENTTD_LOC_BASE_ID = 6_100_000

DIFFICULTY_ID_OFFSET = {
    "easy":    6_100_000,
    "medium":  6_102_000,
    "hard":    6_104_000,
    "extreme": 6_106_000,
}
SHOP_ID_BASE    = 6_108_000
VICTORY_ID      = 6_110_000
RUIN_ID_BASE    = 6_112_000
DEMIGOD_ID_BASE = 6_114_000

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
        "Sugar", "Toys", "Batteries",
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

# ─────────────────────────────────────────────────────────────────────────────
#  PREDEFINED MISSION POOLS
#
#  Each difficulty has exactly 100 pre-written missions.  The generator
#  shuffles the pool and picks the first N entries — so no two missions of
#  the same session ever duplicate each other, and neighbouring amounts are
#  always well-spaced (because they were written that way up-front).
#
#  Format: (description_template, amount, unit, type_key)
#   - description_template: shown to player; {cargo} is filled at runtime
#   - amount: exact target value (already pre-spaced — no random roll needed)
#   - unit: used for C++ type matching / display
#   - type_key: canonical string C++ uses to identify the mission type
#
#  Named-destination missions ([Town] / [Industry near Town]) keep their
#  placeholder text; C++ resolves the actual name at session start.
#
#  EASY: 100 missions ordered by rough difficulty within each type group.
#        Types included: vehicles, trains, road vehicles, stations,
#        passengers, passengers_to_town, mail_to_town, towns, profit,
#        monthly profit, transport cargo, shop purchase.
# ─────────────────────────────────────────────────────────────────────────────

PREDEFINED_MISSION_POOLS: Dict[str, List] = {

    # ── EASY ─────────────────────────────────────────────────────────────────
    "easy": [
        # vehicles running simultaneously — start 10, +7 each step
        ("Have {amount} active vehicles running simultaneously",    10,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    17,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    24,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    31,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    38,      "vehicles",          "vehicles"),
        # trains running simultaneously — start 10, +7 each step
        ("Have {amount} active trains running simultaneously",      10,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      17,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      24,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      31,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      38,      "trains",            "trains"),
        # road vehicles running simultaneously — start 10, +7 each step
        ("Have {amount} active road vehicles running simultaneously", 10,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 17,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 24,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 31,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 38,    "road vehicles",     "road vehicles"),
        # active train stations — start 2, well separated
        ("Have {amount} active train stations",              2,       "train stations",    "train stations"),
        ("Have {amount} active train stations",              5,       "train stations",    "train stations"),
        ("Have {amount} active train stations",              10,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              18,      "train stations",    "train stations"),
        # active bus stops — start 2, well separated
        ("Have {amount} active bus stops",                   2,       "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   5,       "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   10,      "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   18,      "bus stops",         "bus stops"),
        # active truck stops
        ("Have {amount} active truck stops",                 2,       "truck stops",       "truck stops"),
        ("Have {amount} active truck stops",                 5,       "truck stops",       "truck stops"),
        ("Have {amount} active truck stops",                 10,      "truck stops",       "truck stops"),
        # total active stations (any type)
        ("Have {amount} active stations (any type)",         3,       "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         8,       "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         15,      "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         25,      "stations",          "build stations"),
        # transport passengers (7 steps)
        ("Transport {amount} passengers",                    1_000,   "passengers",        "passengers"),
        ("Transport {amount} passengers",                    3_000,   "passengers",        "passengers"),
        ("Transport {amount} passengers",                    6_000,   "passengers",        "passengers"),
        ("Transport {amount} passengers",                    12_000,  "passengers",        "passengers"),
        ("Transport {amount} passengers",                    25_000,  "passengers",        "passengers"),
        ("Transport {amount} passengers",                    50_000,  "passengers",        "passengers"),
        ("Transport {amount} passengers",                    80_000,  "passengers",        "passengers"),
        # passengers to town (8 steps)
        ("Deliver {amount} passengers to [Town]",            200,     "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            500,     "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            1_000,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            2_000,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            3_500,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            5_000,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            8_000,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            12_000,  "passengers_to_town","passengers_to_town"),
        # mail to town (7 steps)
        ("Deliver {amount} mail to [Town]",                  100,     "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  300,     "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  700,     "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  1_500,   "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  3_000,   "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  6_000,   "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  10_000,  "mail_to_town",      "mail_to_town"),
        # service towns (6 steps)
        ("Service {amount} different towns",                 2,       "towns",             "service towns"),
        ("Service {amount} different towns",                 3,       "towns",             "service towns"),
        ("Service {amount} different towns",                 5,       "towns",             "service towns"),
        ("Service {amount} different towns",                 8,       "towns",             "service towns"),
        ("Service {amount} different towns",                 12,      "towns",             "service towns"),
        ("Service {amount} different towns",                 18,      "towns",             "service towns"),
        # earn total profit (7 steps)
        ("Earn \xa3{amount} total profit",                   30_000,  "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   80_000,  "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   150_000, "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   300_000, "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   600_000, "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   1_200_000,"\xa3",             "earn"),
        ("Earn \xa3{amount} total profit",                   2_500_000,"\xa3",             "earn"),
        # earn monthly (7 steps)
        ("Earn \xa3{amount} in one month",                   3_000,   "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   8_000,   "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   15_000,  "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   30_000,  "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   60_000,  "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   120_000, "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   250_000, "\xa3/month",        "earn monthly"),
        # transport cargo (18 varied entries — 3 per cargo group, cargo filled at runtime)
        ("Transport {amount} units of {cargo}",              500,     "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              1_000,   "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              2_500,   "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              5_000,   "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              8_000,   "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              12_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              18_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              25_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              40_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              60_000,  "units",             "transport cargo"),
        # shop purchase (1 only)
        ("Buy a vehicle from the shop",                      1,       "purchase",          "purchase"),
    ],

    # ── MEDIUM ───────────────────────────────────────────────────────────────
    "medium": [
        # vehicles — start 45, +7 each step
        ("Have {amount} active vehicles running simultaneously",    45,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    52,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    59,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    66,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    73,      "vehicles",          "vehicles"),
        # trains — start 45, +7 each step
        ("Have {amount} active trains running simultaneously",      45,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      52,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      59,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      66,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      73,      "trains",            "trains"),
        # road vehicles — start 45, +7 each step
        ("Have {amount} active road vehicles running simultaneously", 45,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 52,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 59,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 66,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 73,    "road vehicles",     "road vehicles"),
        # ships — new in medium, start 10, +7 each step
        ("Have {amount} active ships running simultaneously",        10,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        17,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        24,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        31,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        38,     "ships",             "ships"),
        # aircraft — new in medium, start 10, +7 each step
        ("Have {amount} active aircraft running simultaneously",     10,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     17,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     24,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     31,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     38,     "aircraft",          "aircraft"),
        # active train stations
        ("Have {amount} active train stations",              15,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              25,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              40,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              60,      "train stations",    "train stations"),
        # active bus stops
        ("Have {amount} active bus stops",                   15,      "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   25,      "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   40,      "bus stops",         "bus stops"),
        # active harbours — introduced in medium
        ("Have {amount} active harbours",                    2,       "harbours",          "harbours"),
        ("Have {amount} active harbours",                    5,       "harbours",          "harbours"),
        ("Have {amount} active harbours",                    10,      "harbours",          "harbours"),
        # active airports — introduced in medium
        ("Have {amount} active airports",                    2,       "airports",          "airports"),
        ("Have {amount} active airports",                    5,       "airports",          "airports"),
        ("Have {amount} active airports",                    10,      "airports",          "airports"),
        # total active stations
        ("Have {amount} active stations (any type)",         20,      "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         40,      "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         65,      "stations",          "build stations"),
        # connect cities with rail (5)
        ("Connect {amount} cities with rail",                 4,      "cities",            "cities"),
        ("Connect {amount} cities with rail",                 6,      "cities",            "cities"),
        ("Connect {amount} cities with rail",                 9,      "cities",            "cities"),
        ("Connect {amount} cities with rail",                 13,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 18,     "cities",            "cities"),
        # transport passengers (6)
        ("Transport {amount} passengers",                    50_000,  "passengers",        "passengers"),
        ("Transport {amount} passengers",                    100_000, "passengers",        "passengers"),
        ("Transport {amount} passengers",                    200_000, "passengers",        "passengers"),
        ("Transport {amount} passengers",                    400_000, "passengers",        "passengers"),
        ("Transport {amount} passengers",                    700_000, "passengers",        "passengers"),
        ("Transport {amount} passengers",                    1_200_000,"passengers",       "passengers"),
        # passengers to town (5)
        ("Deliver {amount} passengers to [Town]",            5_000,   "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            10_000,  "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            20_000,  "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            40_000,  "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            80_000,  "passengers_to_town","passengers_to_town"),
        # mail to town (4)
        ("Deliver {amount} mail to [Town]",                  3_000,   "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  8_000,   "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  18_000,  "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  40_000,  "mail_to_town",      "mail_to_town"),
        # supply to industry (4)
        ("Supply {amount} tons to [Industry near Town]",     5_000,   "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     12_000,  "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     25_000,  "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     50_000,  "cargo_to_industry", "cargo_to_industry"),
        # transport from industry (4)
        ("Transport {amount} tons from [Industry near Town]",5_000,   "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",12_000,  "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",25_000,  "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",50_000,  "cargo_from_industry","cargo_from_industry"),
        # service towns (5)
        ("Service {amount} different towns",                 6,       "towns",             "service towns"),
        ("Service {amount} different towns",                 10,      "towns",             "service towns"),
        ("Service {amount} different towns",                 15,      "towns",             "service towns"),
        ("Service {amount} different towns",                 22,      "towns",             "service towns"),
        ("Service {amount} different towns",                 30,      "towns",             "service towns"),
        # earn total profit (6)
        ("Earn \xa3{amount} total profit",                   500_000, "\xa3",              "earn"),
        ("Earn \xa3{amount} total profit",                   1_200_000,"\xa3",             "earn"),
        ("Earn \xa3{amount} total profit",                   3_000_000,"\xa3",             "earn"),
        ("Earn \xa3{amount} total profit",                   6_000_000,"\xa3",             "earn"),
        ("Earn \xa3{amount} total profit",                   12_000_000,"\xa3",            "earn"),
        ("Earn \xa3{amount} total profit",                   25_000_000,"\xa3",            "earn"),
        # earn monthly (5)
        ("Earn \xa3{amount} in one month",                   80_000,  "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   200_000, "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   450_000, "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   900_000, "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   1_800_000,"\xa3/month",       "earn monthly"),
        # maintain station rating (4)
        ("Maintain 75%+ station rating for {amount} months", 4,      "months",            "maintain_75"),
        ("Maintain 75%+ station rating for {amount} months", 7,      "months",            "maintain_75"),
        ("Maintain 75%+ station rating for {amount} months", 12,     "months",            "maintain_75"),
        ("Maintain 75%+ station rating for {amount} months", 18,     "months",            "maintain_75"),
        # transport cargo (9)
        ("Transport {amount} units of {cargo}",              20_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              40_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              70_000,  "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              110_000, "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              160_000, "units",             "transport cargo"),
        ("Deliver {amount} tons of {cargo} to one station",  6_000,   "tons",              "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  15_000,  "tons",              "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  30_000,  "tons",              "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  55_000,  "tons",              "deliver to station"),
    ],

    # ── HARD ─────────────────────────────────────────────────────────────────
    "hard": [
        # vehicles — start 80, +7 each step
        ("Have {amount} active vehicles running simultaneously",    80,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    87,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    94,      "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    101,     "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    108,     "vehicles",          "vehicles"),
        # trains — start 80, +7 each step
        ("Have {amount} active trains running simultaneously",      80,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      87,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      94,      "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      101,     "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      108,     "trains",            "trains"),
        # road vehicles — start 80, +7 each step
        ("Have {amount} active road vehicles running simultaneously", 80,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 87,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 94,    "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 101,   "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 108,   "road vehicles",     "road vehicles"),
        # ships — start 45, +7 each step
        ("Have {amount} active ships running simultaneously",        45,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        52,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        59,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        66,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        73,     "ships",             "ships"),
        # aircraft — start 45, +7 each step
        ("Have {amount} active aircraft running simultaneously",     45,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     52,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     59,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     66,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     73,     "aircraft",          "aircraft"),
        # connect cities with rail (5)
        ("Connect {amount} cities with rail",                 12,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 18,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 26,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 36,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 48,     "cities",            "cities"),
        # transport passengers (5)
        ("Transport {amount} passengers",                    500_000, "passengers",        "passengers"),
        ("Transport {amount} passengers",                    1_000_000,"passengers",       "passengers"),
        ("Transport {amount} passengers",                    2_000_000,"passengers",       "passengers"),
        ("Transport {amount} passengers",                    4_000_000,"passengers",       "passengers"),
        ("Transport {amount} passengers",                    7_000_000,"passengers",       "passengers"),
        # passengers to town (4)
        ("Deliver {amount} passengers to [Town]",            80_000,  "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            180_000, "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            350_000, "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            700_000, "passengers_to_town","passengers_to_town"),
        # mail to town (4)
        ("Deliver {amount} mail to [Town]",                  40_000,  "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  100_000, "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  220_000, "mail_to_town",      "mail_to_town"),
        ("Deliver {amount} mail to [Town]",                  500_000, "mail_to_town",      "mail_to_town"),
        # supply/transport industry (6)
        ("Supply {amount} tons to [Industry near Town]",     30_000,  "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     80_000,  "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     180_000, "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     400_000, "cargo_to_industry", "cargo_to_industry"),
        ("Transport {amount} tons from [Industry near Town]",30_000,  "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",100_000, "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",250_000, "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",600_000, "cargo_from_industry","cargo_from_industry"),
        # active train stations
        ("Have {amount} active train stations",              50,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              80,      "train stations",    "train stations"),
        ("Have {amount} active train stations",              120,     "train stations",    "train stations"),
        # active bus stops
        ("Have {amount} active bus stops",                   40,      "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   70,      "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   100,     "bus stops",         "bus stops"),
        # active harbours
        ("Have {amount} active harbours",                    10,      "harbours",          "harbours"),
        ("Have {amount} active harbours",                    20,      "harbours",          "harbours"),
        ("Have {amount} active harbours",                    35,      "harbours",          "harbours"),
        # active airports
        ("Have {amount} active airports",                    8,       "airports",          "airports"),
        ("Have {amount} active airports",                    15,      "airports",          "airports"),
        ("Have {amount} active airports",                    25,      "airports",          "airports"),
        # total active stations
        ("Have {amount} active stations (any type)",         80,      "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         130,     "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         200,     "stations",          "build stations"),
        # service towns (5)
        ("Service {amount} different towns",                 15,      "towns",             "service towns"),
        ("Service {amount} different towns",                 22,      "towns",             "service towns"),
        ("Service {amount} different towns",                 32,      "towns",             "service towns"),
        ("Service {amount} different towns",                 45,      "towns",             "service towns"),
        ("Service {amount} different towns",                 60,      "towns",             "service towns"),
        # earn total profit (5)
        ("Earn \xa3{amount} total profit",                   6_000_000,"\xa3",             "earn"),
        ("Earn \xa3{amount} total profit",                   15_000_000,"\xa3",            "earn"),
        ("Earn \xa3{amount} total profit",                   35_000_000,"\xa3",            "earn"),
        ("Earn \xa3{amount} total profit",                   80_000_000,"\xa3",            "earn"),
        ("Earn \xa3{amount} total profit",                   160_000_000,"\xa3",           "earn"),
        # earn monthly (5)
        ("Earn \xa3{amount} in one month",                   500_000, "\xa3/month",        "earn monthly"),
        ("Earn \xa3{amount} in one month",                   1_200_000,"\xa3/month",       "earn monthly"),
        ("Earn \xa3{amount} in one month",                   2_500_000,"\xa3/month",       "earn monthly"),
        ("Earn \xa3{amount} in one month",                   5_000_000,"\xa3/month",       "earn monthly"),
        ("Earn \xa3{amount} in one month",                   10_000_000,"\xa3/month",      "earn monthly"),
        # maintain station rating (3)
        ("Maintain 90%+ station rating for {amount} months", 6,      "months",            "maintain_90"),
        ("Maintain 90%+ station rating for {amount} months", 12,     "months",            "maintain_90"),
        ("Maintain 90%+ station rating for {amount} months", 20,     "months",            "maintain_90"),
        # transport cargo / deliver to station (8)
        ("Transport {amount} units of {cargo}",              200_000, "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              450_000, "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              900_000, "units",             "transport cargo"),
        ("Transport {amount} units of {cargo}",              1_800_000,"units",            "transport cargo"),
        ("Deliver {amount} tons of goods in one year",       100_000, "tons",              "deliver goods"),
        ("Deliver {amount} tons of goods in one year",       250_000, "tons",              "deliver goods"),
        ("Deliver {amount} tons of goods in one year",       500_000, "tons",              "deliver goods"),
        ("Deliver {amount} tons of goods in one year",       1_000_000,"tons",             "deliver goods"),
    ],

    # ── EXTREME ───────────────────────────────────────────────────────────────
    "extreme": [
        # vehicles — start 150, +7 each step
        ("Have {amount} active vehicles running simultaneously",    150,     "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    157,     "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    164,     "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    171,     "vehicles",          "vehicles"),
        ("Have {amount} active vehicles running simultaneously",    178,     "vehicles",          "vehicles"),
        # trains — start 150, +7 each step
        ("Have {amount} active trains running simultaneously",      150,     "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      157,     "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      164,     "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      171,     "trains",            "trains"),
        ("Have {amount} active trains running simultaneously",      178,     "trains",            "trains"),
        # road vehicles — start 150, +7 each step
        ("Have {amount} active road vehicles running simultaneously", 150,   "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 157,   "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 164,   "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 171,   "road vehicles",     "road vehicles"),
        ("Have {amount} active road vehicles running simultaneously", 178,   "road vehicles",     "road vehicles"),
        # ships — start 80, +7 each step
        ("Have {amount} active ships running simultaneously",        80,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        87,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        94,     "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        101,    "ships",             "ships"),
        ("Have {amount} active ships running simultaneously",        108,    "ships",             "ships"),
        # aircraft — start 80, +7 each step
        ("Have {amount} active aircraft running simultaneously",     80,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     87,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     94,     "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     101,    "aircraft",          "aircraft"),
        ("Have {amount} active aircraft running simultaneously",     108,    "aircraft",          "aircraft"),
        # connect cities (4)
        ("Connect {amount} cities with rail",                 35,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 55,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 75,     "cities",            "cities"),
        ("Connect {amount} cities with rail",                 100,    "cities",            "cities"),
        # active train stations
        ("Have {amount} active train stations",              150,     "train stations",    "train stations"),
        ("Have {amount} active train stations",              220,     "train stations",    "train stations"),
        ("Have {amount} active train stations",              300,     "train stations",    "train stations"),
        # active bus stops
        ("Have {amount} active bus stops",                   120,     "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   180,     "bus stops",         "bus stops"),
        ("Have {amount} active bus stops",                   250,     "bus stops",         "bus stops"),
        # active harbours
        ("Have {amount} active harbours",                    30,      "harbours",          "harbours"),
        ("Have {amount} active harbours",                    55,      "harbours",          "harbours"),
        ("Have {amount} active harbours",                    80,      "harbours",          "harbours"),
        # active airports
        ("Have {amount} active airports",                    20,      "airports",          "airports"),
        ("Have {amount} active airports",                    40,      "airports",          "airports"),
        ("Have {amount} active airports",                    60,      "airports",          "airports"),
        # total active stations
        ("Have {amount} active stations (any type)",         250,     "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         400,     "stations",          "build stations"),
        ("Have {amount} active stations (any type)",         550,     "stations",          "build stations"),
        # service towns (4)
        ("Service {amount} different towns",                  40,     "towns",             "service towns"),
        ("Service {amount} different towns",                  60,     "towns",             "service towns"),
        ("Service {amount} different towns",                  80,     "towns",             "service towns"),
        ("Service {amount} different towns",                  100,    "towns",             "service towns"),
        # transport passengers (4)
        ("Transport {amount} passengers",                    5_000_000,"passengers",       "passengers"),
        ("Transport {amount} passengers",                    10_000_000,"passengers",      "passengers"),
        ("Transport {amount} passengers",                    20_000_000,"passengers",      "passengers"),
        ("Transport {amount} passengers",                    40_000_000,"passengers",      "passengers"),
        # passengers to town (3)
        ("Deliver {amount} passengers to [Town]",            500_000, "passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            1_500_000,"passengers_to_town","passengers_to_town"),
        ("Deliver {amount} passengers to [Town]",            4_000_000,"passengers_to_town","passengers_to_town"),
        # supply/transport industry (6)
        ("Supply {amount} tons to [Industry near Town]",     200_000, "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     600_000, "cargo_to_industry", "cargo_to_industry"),
        ("Supply {amount} tons to [Industry near Town]",     1_500_000,"cargo_to_industry","cargo_to_industry"),
        ("Transport {amount} tons from [Industry near Town]",200_000, "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",600_000, "cargo_from_industry","cargo_from_industry"),
        ("Transport {amount} tons from [Industry near Town]",1_500_000,"cargo_from_industry","cargo_from_industry"),
        # earn total profit (4)
        ("Earn \xa3{amount} total profit",                   80_000_000,"\xa3",            "earn"),
        ("Earn \xa3{amount} total profit",                   180_000_000,"\xa3",           "earn"),
        ("Earn \xa3{amount} total profit",                   400_000_000,"\xa3",           "earn"),
        ("Earn \xa3{amount} total profit",                   800_000_000,"\xa3",           "earn"),
        # earn in a single month (4)
        ("Earn \xa3{amount} in a single month",              8_000_000, "\xa3/month",      "earn monthly"),
        ("Earn \xa3{amount} in a single month",              18_000_000,"\xa3/month",      "earn monthly"),
        ("Earn \xa3{amount} in a single month",              40_000_000,"\xa3/month",      "earn monthly"),
        ("Earn \xa3{amount} in a single month",              80_000_000,"\xa3/month",      "earn monthly"),
        # transport cargo (8)
        ("Transport {amount} units of {cargo}",              2_000_000, "units",           "transport cargo"),
        ("Transport {amount} units of {cargo}",              5_000_000, "units",           "transport cargo"),
        ("Transport {amount} units of {cargo}",              10_000_000,"units",           "transport cargo"),
        ("Transport {amount} units of {cargo}",              20_000_000,"units",           "transport cargo"),
        ("Deliver {amount} tons of {cargo} to one station",  400_000,  "tons",             "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  1_000_000,"tons",             "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  2_500_000,"tons",             "deliver to station"),
        ("Deliver {amount} tons of {cargo} to one station",  5_000_000,"tons",             "deliver to station"),
    ],
}

# Keep the old name as an alias so existing code that references
# MISSION_TEMPLATES still works without modification.
MISSION_TEMPLATES = PREDEFINED_MISSION_POOLS


DIFFICULTY_DISTRIBUTION = {
    "easy":    0.30,
    "medium":  0.35,
    "hard":    0.25,
    "extreme": 0.10,
}

# Hard cap: at most this many missions per difficulty tier.
# Excess items are routed to the shop instead.
MAX_MISSIONS_PER_DIFFICULTY = 50


class OpenTTDLocationData(NamedTuple):
    code: int
    region: str
    progress_type: LocationProgressType = LocationProgressType.DEFAULT


def _build_location_table(mission_count: int = 100, shop_item_count: int = 100,
                          ruin_count: int = 0,
                          demigod_count: int = 0) -> Dict[str, OpenTTDLocationData]:
    """Build location table with FIXED per-difficulty ID blocks.

    All locations are DEFAULT.  Progression distribution is handled
    manually by pre_fill (40-40-10-10 split between missions, shop,
    ruins, and demigods).

    Block layout:
      Easy:     6100000 – 6101999
      Medium:   6102000 – 6103999
      Hard:     6104000 – 6105999
      Extreme:  6106000 – 6107999
      Shop:     6108000 – 6109999
      Victory:  6110000
      Ruins:    6112000 – 6113999
      Demigods: 6114000 – 6114999
    """
    table: Dict[str, OpenTTDLocationData] = {}

    # Compute per-tier counts from fractions, then distribute remainder
    # so the total exactly equals mission_count (avoids item/location mismatch).
    tier_counts: dict = {}
    for difficulty, fraction in DIFFICULTY_DISTRIBUTION.items():
        tier_counts[difficulty] = min(max(1, int(mission_count * fraction)), MAX_MISSIONS_PER_DIFFICULTY)

    actual_total = sum(tier_counts.values())
    remainder = mission_count - actual_total
    for difficulty in DIFFICULTY_DISTRIBUTION:
        if remainder <= 0:
            break
        room = MAX_MISSIONS_PER_DIFFICULTY - tier_counts[difficulty]
        add = min(remainder, room)
        tier_counts[difficulty] += add
        remainder -= add

    for difficulty in DIFFICULTY_DISTRIBUTION:
        count = tier_counts[difficulty]
        base = DIFFICULTY_ID_OFFSET[difficulty]
        assert count <= 2000, f"Too many {difficulty} missions ({count}); block only holds 2000"
        for i in range(1, count + 1):
            name = f"Mission_{difficulty.capitalize()}_{i:03d}"
            table[name] = OpenTTDLocationData(base + (i - 1), f"mission_{difficulty}")

    for i in range(1, shop_item_count + 1):
        name = f"Shop_Purchase_{i:04d}"
        assert i <= 2000, f"Too many shop slots ({i}); block only holds 2000"
        table[name] = OpenTTDLocationData(SHOP_ID_BASE + (i - 1), "shop")

    for i in range(1, ruin_count + 1):
        name = f"Ruin_{i:03d}"
        assert i <= 2000, f"Too many ruin slots ({i}); block only holds 2000"
        table[name] = OpenTTDLocationData(RUIN_ID_BASE + (i - 1), "ruin")

    for i in range(1, demigod_count + 1):
        name = f"Demigod_{i:03d}"
        assert i <= 1000, f"Too many demigod slots ({i}); block only holds 1000"
        table[name] = OpenTTDLocationData(DEMIGOD_ID_BASE + (i - 1), "demigod")

    table["Goal_Victory"] = OpenTTDLocationData(VICTORY_ID, "goal", LocationProgressType.PRIORITY)

    return table


LOCATION_TABLE: Dict[str, OpenTTDLocationData] = _build_location_table(demigod_count=10)


def get_location_table(mission_count: int, shop_item_count: int,
                       ruin_count: int = 0,
                       demigod_count: int = 0) -> Dict[str, OpenTTDLocationData]:
    return _build_location_table(mission_count, shop_item_count, ruin_count, demigod_count)
