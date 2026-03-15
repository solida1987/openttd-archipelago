from enum import IntEnum
from typing import Dict, List, NamedTuple
from BaseClasses import ItemClassification

# Item IDs start at 6_200_000 — intentionally SEPARATE from location IDs (6_100_000+).
# This prevents "Unknown item (ID:...)" in AP console hints when other games
# have our items, because AP server's item_names dict keyed by game uses these IDs.
OPENTTD_BASE_ID = 6_200_000


class ItemType(IntEnum):
    VEHICLE = 0
    TRAP = 1
    UTILITY = 2
    VICTORY = 3


class OpenTTDItemData(NamedTuple):
    code: int
    classification: ItemClassification
    item_type: ItemType
    category: str


# ─────────────────────────────────────────────────────────────────────────────
#  ALL OPENTTD 15.2 VEHICLES — ALL CLIMATES
#  Names are EXACT matches to english.txt STR_VEHICLE_NAME strings.
# ─────────────────────────────────────────────────────────────────────────────

ALL_TRAINS: List[str] = [
    # Normal Rail — Steam
    "Wills 2-8-0 (Steam)",
    "Kirby Paul Tank (Steam)",
    "Ginzu 'A4' (Steam)",
    "SH '8P' (Steam)",
    "Chaney 'Jubilee' (Steam)",
    # Normal Rail — Diesel
    "MJS 250 (Diesel)",
    "Ploddyphut Diesel",
    "Powernaut Diesel",
    "Turner Turbo (Diesel)",
    "MJS 1000 (Diesel)",
    "SH/Hendry '25' (Diesel)",
    "Manley-Morel DMU (Diesel)",
    "'Dash' (Diesel)",
    "Kelling 3100 (Diesel)",
    "SH '125' (Diesel)",
    "Floss '47' (Diesel)",
    "UU '37' (Diesel)",
    "Centennial (Diesel)",
    "CS 2400 (Diesel)",
    "CS 4000 (Diesel)",
    # Normal Rail — Electric
    "SH '30' (Electric)",
    "SH '40' (Electric)",
    "'AsiaStar' (Electric)",
    # Toyland Rail
    "Ploddyphut Choo-Choo",
    "Powernaut Choo-Choo",
    "MightyMover Choo-Choo",
    # Monorail
    "Wizzowow Z99",
    "'X2001' (Electric)",
    "'T.I.M.' (Electric)",
    # Maglev
    "Lev1 'Leviathan' (Electric)",
    "Lev2 'Cyclops' (Electric)",
    "Lev3 'Pegasus' (Electric)",
    "Lev4 'Chimaera' (Electric)",
    "Wizzowow Rocketeer",
    "'Millennium Z1' (Electric)",
]

ALL_WAGONS: List[str] = [
    # Temperate
    "Passenger Carriage",
    "Mail Van",
    "Coal Truck",
    "Oil Tanker",
    "Goods Van",
    "Armoured Van",
    "Grain Hopper",
    "Wood Truck",
    "Iron Ore Hopper",
    "Steel Truck",
    "Livestock Van",
    # Arctic/Tropical
    "Paper Truck",
    "Copper Ore Hopper",
    "Rubber Truck",
    "Fruit Truck",
    "Water Tanker",
    "Food Van",
    # Toyland
    "Candyfloss Hopper",
    "Toffee Hopper",
    "Cola Tanker",
    "Plastic Truck",
    "Fizzy Drink Truck",
    "Sugar Truck",
    "Sweet Van",
    "Bubble Van",
    "Toy Van",
    "Battery Truck",
]

ALL_ROAD_VEHICLES: List[str] = [
    # Buses
    "MPS Regal Bus",
    "Hereford Leopard Bus",
    "Foster Bus",
    "Foster MkII Superbus",
    "Ploddyphut MkI Bus",
    "Ploddyphut MkII Bus",
    "Ploddyphut MkIII Bus",
    # Mail trucks
    "MPS Mail Truck",
    "Perry Mail Truck",
    "Reynard Mail Truck",
    "MightyMover Mail Truck",
    "Powernaught Mail Truck",
    "Wizzowow Mail Truck",
    # Coal
    "Balogh Coal Truck",
    "Uhl Coal Truck",
    "DW Coal Truck",
    # Grain
    "Hereford Grain Truck",
    "Goss Grain Truck",
    "Thomas Grain Truck",
    # Goods
    "Balogh Goods Truck",
    "Goss Goods Truck",
    "Craighead Goods Truck",
    # Oil
    "Witcombe Oil Tanker",
    "Perry Oil Tanker",
    "Foster Oil Tanker",
    # Wood
    "Witcombe Wood Truck",
    "Moreland Wood Truck",
    "Foster Wood Truck",
    # Iron Ore
    "MPS Iron Ore Truck",
    "Uhl Iron Ore Truck",
    "Chippy Iron Ore Truck",
    # Steel
    "Balogh Steel Truck",
    "Kelling Steel Truck",
    "Uhl Steel Truck",
    # Armoured
    "Balogh Armoured Truck",
    "Uhl Armoured Truck",
    "Foster Armoured Truck",
    # Livestock
    "Talbott Livestock Van",
    "Uhl Livestock Van",
    "Foster Livestock Van",
    # Arctic/Tropical
    "Balogh Paper Truck",
    "MPS Paper Truck",
    "Uhl Paper Truck",
    "Balogh Rubber Truck",
    "Uhl Rubber Truck",
    "RMT Rubber Truck",
    "Balogh Fruit Truck",
    "Uhl Fruit Truck",
    "Kelling Fruit Truck",
    "Balogh Water Tanker",
    "MPS Water Tanker",
    "Uhl Water Tanker",
    "MPS Copper Ore Truck",
    "Uhl Copper Ore Truck",
    "Goss Copper Ore Truck",
    "Perry Food Van",
    "Foster Food Van",
    "Chippy Food Van",
    # Toyland
    "MightyMover Candyfloss Truck",
    "Powernaught Candyfloss Truck",
    "Wizzowow Candyfloss Truck",
    "MightyMover Toffee Truck",
    "Powernaught Toffee Truck",
    "Wizzowow Toffee Truck",
    "MightyMover Cola Truck",
    "Powernaught Cola Truck",
    "Wizzowow Cola Truck",
    "MightyMover Plastic Truck",
    "Powernaught Plastic Truck",
    "Wizzowow Plastic Truck",
    "MightyMover Fizzy Drink Truck",
    "Powernaught Fizzy Drink Truck",
    "Wizzowow Fizzy Drink Truck",
    "MightyMover Sugar Truck",
    "Powernaught Sugar Truck",
    "Wizzowow Sugar Truck",
    "MightyMover Sweet Truck",
    "Powernaught Sweet Truck",
    "Wizzowow Sweet Truck",
    "MightyMover Battery Truck",
    "Powernaught Battery Truck",
    "Wizzowow Battery Truck",
    "MightyMover Bubble Truck",
    "Powernaught Bubble Truck",
    "Wizzowow Bubble Truck",
    "MightyMover Toy Van",
    "Powernaught Toy Van",
    "Wizzowow Toy Van",
]

ALL_AIRCRAFT: List[str] = [
    "Sampson U52",
    "Coleman Count",
    "FFP Dart",
    "Yate Haugan",
    "Bakewell Cotswald LB-3",
    "Dinger 100",
    "Darwin 100",
    "Bakewell Luckett LB-8",
    "Bakewell Luckett LB-9",
    "Yate Aerospace YAC 1-11",
    "Dinger 200",
    "Dinger 1000",
    "Airtaxi A21",
    "Airtaxi A31",
    "Airtaxi A32",
    "Airtaxi A33",
    "Yate Aerospace YAe46",
    "Darwin 200",
    "Darwin 300",
    "Darwin 400",
    "FFP Hyperdart 2",
    "Kelling K1",
    "Kelling K6",
    "Darwin 500",
    "Darwin 600",
    "Darwin 700",
    "Bakewell Luckett LB80",
    "Bakewell Luckett LB-10",
    "Bakewell Luckett LB-11",
    "Kelling K7",
    "AirTaxi A34-1000",
    "Yate Z-Shuttle",
    # Helicopters
    "Guru X2 Helicopter",
    "Tricario Helicopter",
    "Powernaut Helicopter",
    # Toyland
    "Ploddyphut 100",
    "Ploddyphut 500",
    "Flashbang X1",
    "Flashbang Wizzer",
    "Guru Galaxy",
    "Juggerplane M1",
]

ALL_SHIPS: List[str] = [
    "MPS Passenger Ferry",
    "FFP Passenger Ferry",
    "Chugger-Chug Passenger Ferry",
    "Shivershake Passenger Ferry",
    "MPS Oil Tanker",
    "CS-Inc. Oil Tanker",
    "Yate Cargo Ship",
    "Bakewell Cargo Ship",
    "Bakewell 300 Hovercraft",
    "MightyMover Cargo Ship",
    "Powernaut Cargo Ship",
]

TRAP_ITEMS: List[str] = [
    "Breakdown Wave",
    "Recession",
    "Maintenance Surge",
    "Signal Failure",
    "Fuel Shortage",
    "Bank Loan Forced",
    "Industry Closure",
    "Vehicle License Revoke",
]

# Speed Boost items — each unlocks +10% fast-forward speed (100% → 300% over 20 items)
SPEED_BOOST_ITEMS: List[str] = ["Speed Boost"] * 20

UTILITY_ITEMS: List[str] = [
    "Cash Injection £50,000",
    "Cash Injection £200,000",
    "Cash Injection £500,000",
    "Loan Reduction £100,000",
    "Cargo Bonus (2x payment, 60 days)",
    "Reliability Boost (all vehicles, 90 days)",
    "Town Growth Boost",
    "Free Station Upgrade",
]

# ─────────────────────────────────────────────────────────────────────────────
#  TRACK DIRECTION ITEMS — per rail type, 6 directions each = 24 total
#
#  Item name format: "<RailTypeName> Track: <Direction>"
#  Direction names map to Track enum values (track_type.h):
#    NE-SW = TRACK_X=0   NW-SE = TRACK_Y=1
#    N     = TRACK_UPPER=2   S = TRACK_LOWER=3
#    W     = TRACK_LEFT=4    E = TRACK_RIGHT=5
#
#  Rail type indices match C++ RailType enum (rail_type.h):
#    RAILTYPE_RAIL=0   RAILTYPE_ELECTRIC=1   RAILTYPE_MONO=2   RAILTYPE_MAGLEV=3
#
#  Unlock logic: all 6 directions of a type must be unlocked before you can
#  build that rail type in any direction → incentivises completing each set.
# ─────────────────────────────────────────────────────────────────────────────

_TRACK_DIR_SUFFIXES: List[str] = [
    "NE-SW",  # TRACK_X=0
    "NW-SE",  # TRACK_Y=1
    "N",      # TRACK_UPPER=2
    "S",      # TRACK_LOWER=3
    "W",      # TRACK_LEFT=4
    "E",      # TRACK_RIGHT=5
]

NORMAL_TRACK_ITEMS:   List[str] = [f"Normal Rail Track: {d}"  for d in _TRACK_DIR_SUFFIXES]
ELECTRIC_TRACK_ITEMS: List[str] = [f"Electrified Track: {d}"  for d in _TRACK_DIR_SUFFIXES]
MONORAIL_TRACK_ITEMS: List[str] = [f"Monorail Track: {d}"     for d in _TRACK_DIR_SUFFIXES]
MAGLEV_TRACK_ITEMS:   List[str] = [f"Maglev Track: {d}"       for d in _TRACK_DIR_SUFFIXES]

ALL_TRACK_DIRECTION_ITEMS: List[str] = (
    NORMAL_TRACK_ITEMS + ELECTRIC_TRACK_ITEMS +
    MONORAIL_TRACK_ITEMS + MAGLEV_TRACK_ITEMS
)  # 24 items total

# Back-compat alias (old 6-item global list — no longer used in pool, kept for any imports)
RAIL_DIRECTION_ITEMS: List[str] = ALL_TRACK_DIRECTION_ITEMS

# ─────────────────────────────────────────────────────────────────────────────
#  TRAIN → RAIL TYPE mapping
#  Used by __init__.py to determine which track direction items to precollect
#  when a starting vehicle is a train.
#  RailType: 0=Normal, 1=Electric, 2=Monorail, 3=Maglev
# ─────────────────────────────────────────────────────────────────────────────

TRAIN_TO_RAILTYPE: Dict[str, int] = {
    # Normal Rail (0) — Steam
    "Wills 2-8-0 (Steam)":          0,
    "Kirby Paul Tank (Steam)":       0,
    "Ginzu 'A4' (Steam)":            0,
    "SH '8P' (Steam)":               0,
    "Chaney 'Jubilee' (Steam)":      0,
    # Normal Rail (0) — Diesel
    "MJS 250 (Diesel)":              0,
    "Ploddyphut Diesel":             0,
    "Powernaut Diesel":              0,
    "Turner Turbo (Diesel)":         0,
    "MJS 1000 (Diesel)":             0,
    "SH/Hendry '25' (Diesel)":       0,
    "Manley-Morel DMU (Diesel)":     0,
    "'Dash' (Diesel)":               0,
    "Kelling 3100 (Diesel)":         0,
    "SH '125' (Diesel)":             0,
    "Floss '47' (Diesel)":           0,
    "UU '37' (Diesel)":              0,
    "Centennial (Diesel)":           0,
    "CS 2400 (Diesel)":              0,
    "CS 4000 (Diesel)":              0,
    # Normal Rail (0) — Toyland Steam/Diesel
    "Ploddyphut Choo-Choo":          0,
    "Powernaut Choo-Choo":           0,
    "MightyMover Choo-Choo":         0,
    # Electric Rail (1)
    "SH '30' (Electric)":            1,
    "SH '40' (Electric)":            1,
    "'AsiaStar' (Electric)":         1,
    # Monorail (2)
    "'X2001' (Electric)":            2,
    "'T.I.M.' (Electric)":           2,
    "'Millennium Z1' (Electric)":    3,  # Maglev, not Monorail
    "Wizzowow Z99":                  2,
    # Maglev (3)
    "Lev1 'Leviathan' (Electric)":   3,
    "Lev2 'Cyclops' (Electric)":     3,
    "Lev3 'Pegasus' (Electric)":     3,
    "Lev4 'Chimaera' (Electric)":    3,
    "Wizzowow Rocketeer":            3,
}
# IH engines: all run on Normal Rail (0). IH replaces vanilla normal-rail locos.
# Any "IH: *" name not in the dict above → assume Normal Rail.

# ─────────────────────────────────────────────────────────────────────────────
#  UNIVERSAL STARTER WAGONS
#  These wagons work on any climate and never require an industry to be useful.
#  Used as the guaranteed starting wagon when a train is the starting vehicle.
# ─────────────────────────────────────────────────────────────────────────────

UNIVERSAL_STARTER_WAGONS: List[str] = ["Passenger Carriage", "Mail Van"]

# Convenience: per-railtype track item lists indexed by RailType value 0-3
TRACK_ITEMS_BY_RAILTYPE: List[List[str]] = [
    NORMAL_TRACK_ITEMS,    # 0
    ELECTRIC_TRACK_ITEMS,  # 1
    MONORAIL_TRACK_ITEMS,  # 2
    MAGLEV_TRACK_ITEMS,    # 3
]

# ─────────────────────────────────────────────────────────────────────────────
#  INFRASTRUCTURE UNLOCK ITEMS — 7 categories, 42 items total
# ─────────────────────────────────────────────────────────────────────────────

# Combines with ALL_TRACK_DIRECTION_ITEMS (24) into TRACK_DIRECTION_ITEMS alias
TRACK_DIRECTION_ITEMS: List[str] = ALL_TRACK_DIRECTION_ITEMS

ROAD_DIRECTION_ITEMS: List[str] = ["Road: NE-SW", "Road: NW-SE"]

SIGNAL_ITEMS: List[str] = [
    "Signal: Block", "Signal: Entry", "Signal: Exit",
    "Signal: Combo", "Signal: Path", "Signal: One-Way Path",
]

BRIDGE_ITEMS: List[str] = [
    "Bridge: Wooden", "Bridge: Concrete", "Bridge: Girder Steel",
    "Bridge: Suspension Concrete",
    "Bridge: Suspension Steel (Short)", "Bridge: Suspension Steel (Long)",
    "Bridge: Cantilever Steel (Short)", "Bridge: Cantilever Steel (Medium)",
    "Bridge: Cantilever Steel (Long)",
    "Bridge: Girder Steel (High)",
    "Bridge: Tubular Steel (Short)", "Bridge: Tubular Steel (Long)",
    "Bridge: Tubular Silicon",
]

TUNNEL_ITEMS: List[str] = ["Tunnel Construction"]

AIRPORT_ITEMS: List[str] = [
    "Airport: Large", "Airport: Heliport", "Airport: Metropolitan",
    "Airport: International", "Airport: Commuter", "Airport: Helidepot",
    "Airport: Intercontinental", "Airport: Helistation",
]

TREE_ITEMS: List[str] = [
    "Trees: Temperate Pack 1", "Trees: Temperate Pack 2", "Trees: Temperate Pack 3",
    "Trees: Arctic Pack 1", "Trees: Arctic Pack 2", "Trees: Arctic Pack 3",
    "Trees: Tropical Pack 1", "Trees: Tropical Pack 2", "Trees: Tropical Pack 3",
    "Trees: Toyland Pack",
]

TERRAFORM_ITEMS: List[str] = ["Terraform: Raise Land", "Terraform: Lower Land"]

TOWN_ACTION_ITEMS: List[str] = [
    "Town Action: Advertise Small",
    "Town Action: Advertise Medium",
    "Town Action: Advertise Large",
    "Town Action: Fund Road Reconstruction",
    "Town Action: Build Statue",
    "Town Action: Fund Buildings",
    "Town Action: Buy Exclusive Transport Rights",
    "Town Action: Bribe Authority",
]

ALL_INFRASTRUCTURE_ITEMS: List[str] = (
    TRACK_DIRECTION_ITEMS + ROAD_DIRECTION_ITEMS + SIGNAL_ITEMS +
    BRIDGE_ITEMS + TUNNEL_ITEMS + AIRPORT_ITEMS + TREE_ITEMS + TERRAFORM_ITEMS +
    TOWN_ACTION_ITEMS
)


# ─────────────────────────────────────────────────────────────────────────────
#  IRON HORSE — Standard Gauge Engines only (GPL v2, by andythenorth)
#  Wagons are not items — they unlock automatically with their engine.
#  Prefix "IH: " distinguishes them from vanilla vehicles in the item table.
# ─────────────────────────────────────────────────────────────────────────────

IRON_HORSE_ENGINES: List[str] = [
    # ── Steam era — wheel arrangement locomotives ──────────────────────────────
    # Names verified directly from iron_horse.grf 4.14.1 NFO (Action 4 strings)
    "IH: 0-10-0 Decapod",
    "IH: 0-10-0 Girt Licker",
    "IH: 0-4-0+0-4-0 Pika",
    "IH: 0-4-4-0 Alfama",
    "IH: 0-4-4-0 Thor",
    "IH: 0-6-0 Fireball",
    "IH: 0-6-0 Hercules",
    "IH: 0-6-0 Lamia",
    "IH: 0-6-0+0-6-0 Keen",
    "IH: 0-6-0+0-6-0 Xerxes",
    "IH: 0-6-2 Buffalo",
    "IH: 0-6-4 Stag",
    "IH: 0-8-0 Eastern",
    "IH: 0-8-0 Haar",
    "IH: 0-8-0 Saxon",
    "IH: 0-8-2 Yak",
    "IH: 0-8-4 Abernant",
    "IH: 2-4-0 Reliance",
    "IH: 2-6-0 Braf",
    "IH: 2-6-0 Diablo",
    "IH: 2-6-0+0-6-2 Esk",
    "IH: 2-6-0+0-6-2 Nile",
    "IH: 2-6-2 Arrow",
    "IH: 2-6-2 Cheese Bug",
    "IH: 2-6-2 Merrylegs",
    "IH: 2-6-2 Ox",
    "IH: 2-6-2 Proper Job",
    "IH: 2-6-4 Bean Feast",
    "IH: 2-8-0 Mainstay",
    "IH: 2-8-0 Vigilant",
    "IH: 2-8-2 Backbone",
    "IH: 2-8-2 Pegasus",
    "IH: 4-2-2 Spinner",
    "IH: 4-4-0 Carrack",
    "IH: 4-4-0 Tencendur",
    "IH: 4-4-2 Lark",
    "IH: 4-4-2 Swift",
    "IH: 4-6-0 Strongbow",
    "IH: 4-6-0 Thunderer",
    "IH: 4-6-4 Satyr",
    "IH: 4-6-4 Streamer",
    "IH: 4-8-0 Lemon",
    "IH: 4-8-0 Tyrconnell",
    "IH: 4-8-2 Hawkinge",
    # ── Modern era — named locomotives ────────────────────────────────────────
    "IH: Alizé",
    "IH: Ares",
    "IH: Argus",
    "IH: Athena",
    "IH: Avenger",
    "IH: Bankside",
    "IH: Blaze HST",
    "IH: Boar Cat",
    "IH: Booster",
    "IH: Brash",
    "IH: Breeze",
    "IH: Brenner",
    "IH: Canary",
    "IH: Captain Steel",
    "IH: Centaur",
    "IH: Cheddar Valley",
    "IH: Chinook",
    "IH: Chronos",
    "IH: Chuggypig",
    "IH: Clipper",
    "IH: Cyclone",
    "IH: Daring",
    "IH: Deasil",
    "IH: Defiant",
    "IH: Doubletide",
    "IH: Dover",
    "IH: Dragon",
    "IH: Dryth",
    "IH: Falcon",
    "IH: Firebird",
    "IH: Flanders Storm",
    "IH: Flindermouse",
    "IH: Foxhound",
    "IH: Fury",
    "IH: Gargouille",
    "IH: General Endeavour",
    "IH: Geronimo",
    "IH: Golfinho",
    "IH: Goliath",
    "IH: Gowsty",
    "IH: Grid",
    "IH: Griffon",
    "IH: Gronk",
    "IH: Growler",
    "IH: Grub",
    "IH: Hammersmith",
    "IH: Happy Train",
    "IH: Helm Wind",
    "IH: High Flyer",
    "IH: Higuma",
    "IH: Hinterland",
    "IH: Hurly Burly",
    "IH: Intrepid",
    "IH: Jupiter",
    "IH: Kelpie",
    "IH: Kraken",
    "IH: Lion",
    "IH: Little Bear",
    "IH: Longwater",
    "IH: Lynx",
    "IH: Maelstrom",
    "IH: Magnum Vario",
    "IH: Merlion",
    "IH: Moor Gallop",
    "IH: Mumble",
    "IH: Nimbus",
    "IH: Olympic",
    "IH: Onslaught",
    "IH: Peasweep",
    "IH: Phoenix",
    "IH: Pikel",
    "IH: Pinhorse",
    "IH: Plastic Postbox",
    "IH: Poplar",
    "IH: Progress",
    "IH: Pylon",
    "IH: Rapid",
    "IH: Ravensbourne",
    "IH: Relentless",
    "IH: Resilient",
    "IH: Resistance",
    "IH: Roarer",
    "IH: Ruby",
    "IH: Scooby",
    "IH: Screamer",
    "IH: Serpentine",
    "IH: Shoebox",
    "IH: Shredder",
    "IH: Sizzler",
    "IH: Skeiron",
    "IH: Skipper",
    "IH: Slammer",
    "IH: Snapper",
    "IH: Stalwart",
    "IH: Stentor",
    "IH: Stoat",
    "IH: Sunshine Coast",
    "IH: Tenacious",
    "IH: Tideway",
    "IH: Tin Rocket",
    "IH: Toaster",
    "IH: Tornado",
    "IH: Trojan",
    "IH: Tyburn",
    "IH: Typhoon",
    "IH: Ultra Shoebox",
    "IH: Viking",
    "IH: Vulcan",
    "IH: Walbrook",
    "IH: Wandle",
    "IH: Westbourne",
    "IH: Wildfire",
    "IH: Withershins",
    "IH: Wyvern",
    "IH: Yillen",
    "IH: Zebedee",
    "IH: Zest",
    "IH: Zeus",
    "IH: Zorro",
]


# ─────────────────────────────────────────────────────────────────────────────
#  MILITARY ITEMS 1.2.0 — Aircraft (fighters, transports, recon, trainers,
#  helicopters) by MilitaryItems team.
#  Prefix "MIL: " distinguishes them from vanilla aircraft.
# ─────────────────────────────────────────────────────────────────────────────

MILITARY_ITEMS_AIRCRAFT: List[str] = [
    # ── Fighters (32) ────────────────────────────────────────────────────────
    "MIL: Avro 504",
    "MIL: Nieuport-Delage NiD 29",
    "MIL: Avia B.3",
    "MIL: Aero A.18",
    "MIL: Avia B.21",
    "MIL: Avia Ba.33",
    "MIL: Avia B.534",
    "MIL: Polikarpov I-16",
    "MIL: Hawker Hurricane IIC",
    "MIL: Avia B.35",
    "MIL: Mitsubishi A6M Zero",
    "MIL: Grumman F6F Hellcat",
    "MIL: Yakovlev Yak-9",
    "MIL: Avia S-199",
    "MIL: Hawker Sea Hawk",
    "MIL: Mikoyan-Gurevich MiG-15",
    "MIL: North American F-86 Sabre",
    "MIL: Aero S-103",
    "MIL: Mikoyan-Gurevich MiG-19",
    "MIL: Mikoyan-Gurevich MiG-21",
    "MIL: Dassault Mirage III",
    "MIL: Aero S-105",
    "MIL: McDonnell F-4 Phantom II",
    "MIL: General Dynamics F-16 Fighting Falcon",
    "MIL: Mikoyan-Gurevich MiG-29",
    "MIL: Dassault Mirage 2000",
    "MIL: McDonnell Douglas F/A-18 Hornet",
    "MIL: Saab JAS 39 Gripen",
    "MIL: Sukhoi Su-30",
    "MIL: Dassault Rafale",
    "MIL: Lockheed Martin F-35A Lightning II",
    "MIL: Sukhoi Su-57",
    # ── Transport / Airlifters (15) ──────────────────────────────────────────
    "MIL: Douglas C-74 Globemaster",
    "MIL: Antonov An-2",
    "MIL: Douglas C-124 Globemaster II",
    "MIL: Antonov An-12",
    "MIL: Antonov An-24",
    "MIL: Lockheed C-141 Starlifter",
    "MIL: Grumman C-2 Greyhound",
    "MIL: Antonov An-22",
    "MIL: Lockheed C-5 Galaxy",
    "MIL: Let L-410",
    "MIL: Antonov An-26",
    "MIL: Ilyushin Il-76",
    "MIL: Boeing C-17 Globemaster III",
    "MIL: Airbus A400M Atlas",
    "MIL: Let L-410NG",
    # ── Reconnaissance (2) ───────────────────────────────────────────────────
    "MIL: Northrop Grumman RQ-4 Global Hawk",
    "MIL: Lockheed Martin RQ-170 Sentinel",
    # ── Training / Courier / Light Bombers (8) ───────────────────────────────
    "MIL: Letov S.328",
    "MIL: Zlin Z.12",
    "MIL: Aero A.304",
    "MIL: Aero A.304 (courier)",
    "MIL: Aero L-29",
    "MIL: Aero L-39",
    "MIL: Aero L-159",
    "MIL: Aero L-39NG",
    # ── Helicopters (16) ─────────────────────────────────────────────────────
    "MIL: Sikorsky CH-37 Mojave",
    "MIL: Bell UH-1 Iroquois",
    "MIL: Mil Mi-6",
    "MIL: Aerospatiale SA 321 Super Frelon",
    "MIL: Aerospatiale SA 330 Puma",
    "MIL: Sikorsky CH-53E Super Stallion",
    "MIL: Mil Mi-26",
    "MIL: Eurocopter AS 532 Cougar",
    "MIL: Sikorsky UH-60 Black Hawk",
    "MIL: Mil Mi-17",
    "MIL: AgustaWestland AW101 Merlin",
    "MIL: AgustaWestland AW139",
    "MIL: Eurocopter EC725 Caracal",
    "MIL: Sikorsky CH-148 Cyclone",
    "MIL: Sikorsky CH-53K King Stallion",
    "MIL: Mil Mi-38",
]


# ─────────────────────────────────────────────────────────────────────────────
#  SHARK SHIP SET 1.0 — Ships (freighters, ferries, tankers, barges, etc.)
#  by andythenorth.  Prefix "SHARK: " distinguishes from vanilla ships.
# ─────────────────────────────────────────────────────────────────────────────

SHARK_SHIPS: List[str] = [
    "SHARK: Whitgift",
    "SHARK: Bernard",
    "SHARK: Lantau",
    "SHARK: Wellfleet",
    "SHARK: Harbour Point",
    "SHARK: Malin",
    "SHARK: Finisterre",
    "SHARK: Constance",
    "SHARK: Saint Marie",
    "SHARK: Gorky",
    "SHARK: Freshney",
    "SHARK: Volgoneft 270",
    "SHARK: Frisco Bay",
    "SHARK: Little Cumbrae",
    "SHARK: Volgoneft 540",
    "SHARK: Provincetown",
    "SHARK: Altamira",
    "SHARK: Friedrich",
    "SHARK: Quessant",
    "SHARK: Lampwick",
    "SHARK: Olympic",
    "SHARK: Roland",
    "SHARK: Lutschine",
    "SHARK: Bundaberg",
    "SHARK: Schipbeek",
    "SHARK: Tyree",
    "SHARK: Volgoneft 320",
    "SHARK: Volgoneft 630",
    "SHARK: Labrador",
    "SHARK: Island Trader",
    "SHARK: Taurus",
    "SHARK: Josephine",
    "SHARK: Shannon",
    "SHARK: Rosario",
    "SHARK: McClure",
    "SHARK: Grindavik",
    "SHARK: Eddystone",
    "SHARK: Meteor",
    "SHARK: Whetstone",
    "SHARK: Kagoshima",
    "SHARK: Marstein",
    "SHARK: Oran",
    "SHARK: Nagasaki",
    "SHARK: Provence Edibles",
    "SHARK: Stornoway",
    "SHARK: Tigershark",
    "SHARK: Santorini",
    "SHARK: Maddalena",
    "SHARK: Connor Freight",
    "SHARK: Helsinki",
    "SHARK: Nanaimo 70",
    "SHARK: Ohshima Freight",
    "SHARK: Duckitt 400",
    "SHARK: La Orchilla",
    "SHARK: Hitsuji",
    "SHARK: Cadiz",
    "SHARK: Kwangtung",
    "SHARK: Newport",
    "SHARK: Hammerhead",
    "SHARK: Yokohama",
    "SHARK: Lorraine Edibles",
    "SHARK: Matsushima",
    "SHARK: Munkegrund",
    "SHARK: Huanghai LNG",
    "SHARK: Port Jackson",
    "SHARK: Maspalomas",
    "SHARK: Great White",
    "SHARK: Winterhold",
    "SHARK: Dieze",
    "SHARK: Enoshima",
    "SHARK: Mako",
]


# ─────────────────────────────────────────────────────────────────────────────
#  HOVER VEHICLES 1 — Futuristic hover road vehicles.
#  Prefix "HV: " distinguishes from vanilla road vehicles.
# ─────────────────────────────────────────────────────────────────────────────

HOVER_VEHICLES: List[str] = [
    "HV: HZ Hover Bus",
    "HV: HZ Flatbed Hover Truck",
    "HV: HZ Hopper Hover Truck",
    "HV: HZ Hover Tanker",
    "HV: HZ Hi-Sec Hover Truck",
    "HV: HZ Refrigerated Hover Truck",
]


# ─────────────────────────────────────────────────────────────────────────────
#  HEQS Heavy Equipment Set 1.5.2 — Off-road crawlers, dump/mining trucks,
#  industrial trams, foundry transporters, tractors, logging trucks.
#  Prefix "HEQS: " distinguishes from vanilla road vehicles.
#  Articulated sub-parts excluded (not independently purchasable).
# ─────────────────────────────────────────────────────────────────────────────

HEQS_ROAD_VEHICLES: List[str] = [
    # Heavy crawlers
    "HEQS: No. 6 Crawler (General Purpose)",
    "HEQS: No. 6 Crawler (Logging / Mining)",
    "HEQS: No. 8 Crawler (Supply Train)",
    "HEQS: No. 8 Crawler (Logging / Mining)",
    "HEQS: No. 9 Crawler (Supply Train)",
    "HEQS: No. 9 Crawler (Logging / Mining)",
    "HEQS: Red Peak Articulated Crawler",
    # Logging trucks
    "HEQS: Cascade C10 Logging Truck",
    "HEQS: Cascade C16 Logging Truck",
    "HEQS: Mackenzie Logging Truck",
    # Dump trucks
    "HEQS: Mt. McKinley Dump Truck",
    "HEQS: Mt. Rainier Dump Truck",
    "HEQS: Wolfpen Ridge Dump Truck",
    "HEQS: Medium Belly Dump",
    "HEQS: Large Belly Dump",
    # Mining trucks
    "HEQS: Thunder Mountain Mining Truck",
    "HEQS: Harney Peak Mining Truck",
    "HEQS: Bear Mountain Mining Truck",
    "HEQS: Rockchuck Peak Mining Truck",
    "HEQS: Camelback Mountain Mining Truck",
    "HEQS: Kilimanjaro Unitised Mining Truck",
    # Utility / tractors / forklifts
    "HEQS: Gmund Mog",
    "HEQS: Willamette Forklift",
    "HEQS: Fourtrac",
    "HEQS: Speedytrac",
    "HEQS: Super Speedytrac",
    # Trailers
    "HEQS: Generic Medium Trailer",
    "HEQS: Kander Trailer",
    # Foundry transporters
    "HEQS: Grindelwald Foundry Transporter",
    "HEQS: Kander Foundry Transporter",
    # Tram wagons
    "HEQS: Tram Wagon 1",
    "HEQS: Tram Wagon 2",
    "HEQS: Tram Wagon 3",
    "HEQS: Express Tram Wagon 1",
    # Industrial trams (Steam)
    "HEQS: 0-4-0 Dorstfeld Industrial Tram (Steam)",
    "HEQS: 0-6-0 Chemnitz Industrial Tram (Steam)",
    "HEQS: 0-8-0 Kassel Industrial Tram (Steam)",
    "HEQS: 0-6-6-0 Afonside Industrial Tram (Steam)",
    # Industrial trams (Electric)
    "HEQS: Kreuzberg Industrial Tram (Electric)",
    "HEQS: Hennigsdorf Industrial Tram (Electric)",
    "HEQS: Ishizuchi Industrial Tram (Electric)",
    "HEQS: Dynamo Express Tram (Electric)",
    # Railmotors
    "HEQS: Yonkers Railmotor (Steam)",
    "HEQS: Winterthur Railmotor (Electric)",
    "HEQS: Port Jack Railmotor (Electric)",
]

HEQS_TRAINS: List[str] = [
    "HEQS: Gmund Mog Hi-Rail Truck",
]


# ─────────────────────────────────────────────────────────────────────────────
#  VACTRAIN SET 1.0.1 — Futuristic vacuum-tube trains (VACT railtype).
#  Prefix "VAC: " distinguishes from vanilla and Iron Horse trains.
# ─────────────────────────────────────────────────────────────────────────────

VACTRAIN_ENGINES: List[str] = [
    # Passenger locomotives
    "VAC: Hyperloop Pod",
    "VAC: Fenglong FL1 Prototype",
    "VAC: Swissmetro",
    "VAC: Alstom Train-Sous-Vide",
    "VAC: Alstom TSV Duplex",
    "VAC: Siemens Chimaera",
    "VAC: JVAC Shinkūsen 1000",
    "VAC: JVAC Shinkūsen 4000",
    "VAC: Fenglong FL2",
    'VAC: Fenglong FL5 "Gepard"',
    "VAC: Fenglong FL8",
    # Cargo locomotives
    "VAC: Siemens TransCargo",
    "VAC: Alstom TSV Fret",
    "VAC: F-Trainz FT001",
    'VAC: Juanlong "Silk Dragon"',
    # Wagons
    "VAC: Vactrain Unit Wagon (Powered)",
    "VAC: Vactrain Unit Wagon (Unpowered)",
    "VAC: Vactrain Universal Cargo Module",
]


# ─────────────────────────────────────────────────────────────────────────────
#  AIRCRAFTPACK 2025 by Illusive 1.6.0 — Real-world + futuristic aircraft.
#  Prefix "AP25: " distinguishes from vanilla and Military Items aircraft.
# ─────────────────────────────────────────────────────────────────────────────

AIRCRAFTPACK_AIRCRAFT: List[str] = [
    # Early era
    "AP25: Luftschiff",
    "AP25: Junkers Ju 52",
    "AP25: Douglas DC-3 Dakota",
    # Classic era
    "AP25: Vickers Viscount",
    "AP25: Boeing 707",
    "AP25: Aérospatiale SE-210 Caravelle",
    "AP25: BAC 1-11",
    "AP25: Boeing 727",
    "AP25: McDonnell Douglas DC-9",
    "AP25: Boeing 737",
    "AP25: Boeing 747",
    "AP25: McDonnell Douglas DC-10",
    "AP25: Airbus A300",
    "AP25: Lockheed L-1011 TriStar",
    "AP25: Aérospatiale/BAC Concorde",
    "AP25: Aérospatiale Hirondelle",
    # Modern era
    "AP25: Aérospatiale AS 332",
    "AP25: McDonnell Douglas MD-81",
    "AP25: Boeing 767",
    "AP25: Airbus A310",
    "AP25: Boeing 757",
    "AP25: BAe 146",
    "AP25: Mil Mi-26T",
    "AP25: Airbus A320",
    "AP25: Fokker 100",
    "AP25: McDonnell Douglas MD-11",
    "AP25: Airbus A340",
    "AP25: Learjet 60",
    "AP25: Boeing 777",
    "AP25: Airbus A330",
    "AP25: Sikorsky S-92",
    "AP25: Lockheed Martin P-791",
    "AP25: Airbus A380",
    "AP25: Boeing 787 Dreamliner",
    "AP25: Boeing 747-8I",
    "AP25: Airbus A350",
    "AP25: Kamov Ka-62",
    # Future era
    "AP25: Lockheed CL-2000",
    "AP25: Lilium Airtaxi",
    "AP25: Airbus RACER",
    "AP25: Boom Supersonic Overture",
    "AP25: Boeing 797 SonicCruiser",
    "AP25: Flying-V-2000",
    "AP25: Airbus A360",
    "AP25: Airbus A400",
    "AP25: Boeing 848 Skywhale",
    "AP25: Airbus A370",
]


# ─────────────────────────────────────────────────────────────────────────────
#  ESSENTIAL VEHICLES — the minimum set the player NEEDS for progression.
#  Only these are ItemClassification.progression; everything else is 'useful'.
#  This gives the Archipelago fill algorithm breathing room (~60 progression
#  items vs ~140 useful) and prevents FillError on high mission counts.
#
#  Design: the player needs one of each cargo-capable vehicle type to do
#  missions. Extras are nice-to-have but not required for logic.
# ─────────────────────────────────────────────────────────────────────────────
ESSENTIAL_VEHICLES: frozenset = frozenset({
    # ── Trains — Steam (all, early game workhorses) ──────────────────────
    "Wills 2-8-0 (Steam)", "Kirby Paul Tank (Steam)", "Ginzu 'A4' (Steam)",
    "SH '8P' (Steam)", "Chaney 'Jubilee' (Steam)",
    # ── Trains — Diesel (all, mid game backbone) ─────────────────────────
    "MJS 250 (Diesel)", "Ploddyphut Diesel", "Powernaut Diesel",
    "Turner Turbo (Diesel)", "MJS 1000 (Diesel)", "SH/Hendry '25' (Diesel)",
    "Manley-Morel DMU (Diesel)", "'Dash' (Diesel)", "Kelling 3100 (Diesel)",
    "SH '125' (Diesel)", "Floss '47' (Diesel)", "UU '37' (Diesel)",
    "Centennial (Diesel)", "CS 2400 (Diesel)", "CS 4000 (Diesel)",
    # ── Trains — Toyland (only trains available on Toyland) ──────────────
    "Ploddyphut Choo-Choo", "Powernaut Choo-Choo", "MightyMover Choo-Choo",
    # ── Wagons — ALL (essential for cargo transport) ─────────────────────
    "Passenger Carriage", "Mail Van", "Coal Truck", "Oil Tanker",
    "Goods Van", "Armoured Van", "Grain Hopper", "Wood Truck",
    "Iron Ore Hopper", "Steel Truck", "Livestock Van",
    "Paper Truck", "Copper Ore Hopper", "Rubber Truck", "Fruit Truck",
    "Water Tanker", "Food Van",
    "Candyfloss Hopper", "Toffee Hopper", "Cola Tanker", "Plastic Truck",
    "Fizzy Drink Truck", "Sugar Truck", "Sweet Van", "Bubble Van",
    "Toy Van", "Battery Truck",
    # ── Road — 1 bus + first-gen cargo trucks (one per cargo type) ───────
    "MPS Regal Bus",          # first bus
    "MPS Mail Truck",         # first mail
    "Balogh Coal Truck",      # first coal
    "Hereford Grain Truck",   # first grain
    "Balogh Goods Truck",     # first goods
    "Witcombe Oil Tanker",    # first oil
    "Witcombe Wood Truck",    # first wood
    "MPS Iron Ore Truck",     # first iron ore
    "Balogh Steel Truck",     # first steel
    "Balogh Armoured Truck",  # first armoured
    "Talbott Livestock Van",  # first livestock
    # ── Road — Arctic/Tropical first-gen ─────────────────────────────────
    "Balogh Paper Truck", "Balogh Rubber Truck", "Balogh Fruit Truck",
    "Balogh Water Tanker", "MPS Copper Ore Truck", "Perry Food Van",
    # ── Road — Toyland first-gen (MightyMover series) ────────────────────
    "Ploddyphut MkI Bus", "MightyMover Mail Truck",
    "MightyMover Candyfloss Truck", "MightyMover Toffee Truck",
    "MightyMover Cola Truck", "MightyMover Plastic Truck",
    "MightyMover Fizzy Drink Truck", "MightyMover Sugar Truck",
    "MightyMover Sweet Truck", "MightyMover Battery Truck",
    "MightyMover Bubble Truck", "MightyMover Toy Van",
    # ── Ships — oil tanker + passenger ferry + cargo ship ────────────────
    "MPS Passenger Ferry", "MPS Oil Tanker", "Yate Cargo Ship",
    # ── Ships — Toyland ──────────────────────────────────────────────────
    "Chugger-Chug Passenger Ferry", "MightyMover Cargo Ship",
    # ── Aircraft — small airport planes (prop era, can land everywhere) ──
    "Sampson U52", "Coleman Count", "FFP Dart", "Yate Haugan",
    "Bakewell Cotswald LB-3", "Dinger 100", "Airtaxi A21",
    # ── Aircraft — helicopter (can use heliports) ────────────────────────
    "Tricario Helicopter",
    # ── Aircraft — Toyland small ─────────────────────────────────────────
    "Ploddyphut 100", "Flashbang X1",
    # ── Aircraft — Toyland helicopter ────────────────────────────────────
    "Powernaut Helicopter",
    # ── Iron Horse — 10 early steam engines (replace vanilla when IH on) ─
    "IH: 0-6-0 Fireball", "IH: 0-6-0 Hercules", "IH: 0-6-0 Lamia",
    "IH: 0-8-0 Eastern", "IH: 0-8-0 Haar", "IH: 0-8-0 Saxon",
    "IH: 2-6-0 Braf", "IH: 2-6-0 Diablo", "IH: 2-6-2 Arrow",
    "IH: 2-4-0 Reliance",
    # ── SHARK — 5 essential ships (replace vanilla progression ships when SHARK on)
    "SHARK: Whitgift",       # passenger ferry replacement
    "SHARK: Lantau",         # oil tanker replacement
    "SHARK: Provincetown",   # cargo ship replacement
    "SHARK: Malin",          # early versatile ship
    "SHARK: Eddystone",      # early versatile ship
})


def _build_item_table() -> Dict[str, OpenTTDItemData]:
    table: Dict[str, OpenTTDItemData] = {}
    code = OPENTTD_BASE_ID

    def add(name: str, cls: ItemClassification, itype: ItemType, cat: str):
        nonlocal code
        if name in table:
            return  # Skip duplicates
        table[name] = OpenTTDItemData(code, cls, itype, cat)
        code += 1

    def vehicle_cls(name: str) -> ItemClassification:
        """Essential vehicles → progression; everything else → useful."""
        return ItemClassification.progression if name in ESSENTIAL_VEHICLES else ItemClassification.useful

    for name in ALL_TRAINS:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "train")
    for name in ALL_WAGONS:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "wagon")
    for name in ALL_ROAD_VEHICLES:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "road_vehicle")
    for name in ALL_AIRCRAFT:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "aircraft")
    for name in ALL_SHIPS:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "ship")
    for name in TRAP_ITEMS:
        add(name, ItemClassification.trap, ItemType.TRAP, "trap")
    for name in UTILITY_ITEMS:
        add(name, ItemClassification.useful, ItemType.UTILITY, "utility")
    # Speed Boost: only ONE entry needed — C++ accumulates each copy received
    add("Speed Boost", ItemClassification.useful, ItemType.UTILITY, "speed_boost")

    # Rail direction unlocks — 24 items (4 rail types × 6 directions).
    # Registered always so create_item() works; only enter pool when option enabled.
    for name in ALL_TRACK_DIRECTION_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "rail_direction")

    # Infrastructure unlock items — registered always, only enter pool when toggled on
    for name in ROAD_DIRECTION_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "road_direction")
    for name in SIGNAL_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "signal")
    for name in BRIDGE_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "bridge")
    for name in TUNNEL_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "tunnel")
    for name in AIRPORT_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "airport")
    for name in TREE_ITEMS:
        add(name, ItemClassification.filler, ItemType.UTILITY, "tree")
    for name in TERRAFORM_ITEMS:
        add(name, ItemClassification.progression, ItemType.UTILITY, "terraform")
    for name in TOWN_ACTION_ITEMS:
        add(name, ItemClassification.useful, ItemType.UTILITY, "town_action")
    # Iron Horse engines — registered here so create_item() works even when
    # the option is disabled. Items only enter the *pool* when enabled (see __init__.py).
    for name in IRON_HORSE_ENGINES:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "train")
    # Military Items aircraft — always registered, only pooled when enabled
    for name in MILITARY_ITEMS_AIRCRAFT:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "aircraft")
    # SHARK ships — always registered, only pooled when enabled
    # 5 SHARK ships are in ESSENTIAL_VEHICLES (progression), rest are useful
    for name in SHARK_SHIPS:
        add(name, vehicle_cls(name), ItemType.VEHICLE, "ship")
    # Hover Vehicles — always registered, only pooled when enabled
    for name in HOVER_VEHICLES:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "road_vehicle")
    # HEQS Heavy Equipment — always registered, only pooled when enabled
    for name in HEQS_ROAD_VEHICLES:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "road_vehicle")
    for name in HEQS_TRAINS:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "train")
    # Vactrain — always registered, only pooled when enabled
    for name in VACTRAIN_ENGINES:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "train")
    # Aircraftpack 2025 — always registered, only pooled when enabled
    for name in AIRCRAFTPACK_AIRCRAFT:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "aircraft")

    add("Victory", ItemClassification.progression, ItemType.VICTORY, "victory")
    return table


ITEM_TABLE: Dict[str, OpenTTDItemData] = _build_item_table()

ALL_VEHICLES: List[str] = (
    ALL_TRAINS + ALL_WAGONS +
    ALL_ROAD_VEHICLES + ALL_AIRCRAFT + ALL_SHIPS
)

# Backwards compat aliases
VANILLA_TRAINS = ALL_TRAINS
VANILLA_WAGONS = ALL_WAGONS

# Vanilla normal-rail engines (steam/diesel/electric loco).
# These do NOT exist in-game when Iron Horse GRF is enabled,
# because IH replaces them. Monorail and Maglev are unaffected.
VANILLA_RAIL_ENGINES: frozenset = frozenset({
    # Steam
    "Wills 2-8-0 (Steam)", "Kirby Paul Tank (Steam)", "Ginzu 'A4' (Steam)",
    "SH '8P' (Steam)", "Chaney 'Jubilee' (Steam)",
    # Diesel
    "MJS 250 (Diesel)", "Ploddyphut Diesel", "Powernaut Diesel",
    "Turner Turbo (Diesel)", "MJS 1000 (Diesel)", "SH/Hendry '25' (Diesel)",
    "Manley-Morel DMU (Diesel)", "'Dash' (Diesel)", "Kelling 3100 (Diesel)",
    "SH '125' (Diesel)", "Floss '47' (Diesel)", "UU '37' (Diesel)",
    "Centennial (Diesel)", "CS 2400 (Diesel)", "CS 4000 (Diesel)",
    # Electric
    "SH '30' (Electric)", "SH '40' (Electric)", "'AsiaStar' (Electric)",
})

VANILLA_ROAD_VEHICLES = ALL_ROAD_VEHICLES
VANILLA_AIRCRAFT = ALL_AIRCRAFT
VANILLA_SHIPS = ALL_SHIPS



# Trains only available on Arctic or Tropic maps (NOT on Temperate).
ARCTIC_TROPIC_ONLY_TRAINS: frozenset = frozenset({
    "Wills 2-8-0 (Steam)",
    "MJS 250 (Diesel)",
    "CS 4000 (Diesel)",
    "CS 2400 (Diesel)",
    "Centennial (Diesel)",
    "Kelling 3100 (Diesel)",
    "MJS 1000 (Diesel)",
})

# Trains only available on Temperate maps.
TEMPERATE_ONLY_TRAINS: frozenset = frozenset({
    "Kirby Paul Tank (Steam)",
    "Chaney 'Jubilee' (Steam)",
    "Ginzu 'A4' (Steam)",
    "SH '8P' (Steam)",
    "SH/Hendry '25' (Diesel)",
    "UU '37' (Diesel)",
    "Floss '47' (Diesel)",
    "SH '30' (Electric)",
    "SH '40' (Electric)",
})

# Wagons NOT available on Temperate maps (Arctic/Tropic cargo wagons).
NON_TEMPERATE_WAGONS: frozenset = frozenset({
    "Paper Truck",     # Arctic only
    "Food Van",        # Arctic + Tropic only
    "Copper Ore Hopper",  # Tropic only
    "Rubber Truck",    # Tropic only
    "Fruit Truck",     # Tropic only (Arctic uses Food Van)
    "Water Tanker",    # Tropic only
})

# Wagons NOT available on Arctic maps.
NON_ARCTIC_WAGONS: frozenset = frozenset({
    "Iron Ore Hopper",   # Temperate only
    "Steel Truck",       # Temperate only
    "Livestock Van",     # Temperate only
    "Copper Ore Hopper", # Tropic only
    "Rubber Truck",      # Tropic only
    "Fruit Truck",       # Tropic only
    "Water Tanker",      # Tropic only
})

# Wagons NOT available on Tropic maps.
NON_TROPIC_WAGONS: frozenset = frozenset({
    "Iron Ore Hopper",   # Temperate only
    "Steel Truck",       # Temperate only
    "Livestock Van",     # Temperate only
    "Coal Truck",        # Temperate + Arctic only
    "Grain Hopper",      # Temperate + Arctic only (Grain/Wheat)
    "Paper Truck",       # Arctic only
})

# Road vehicles NOT available on Temperate maps (Arctic/Tropic or Tropic-only).
NON_TEMPERATE_ROAD_VEHICLES: frozenset = frozenset({
    # Arctic-only
    "Uhl Paper Truck", "Balogh Paper Truck", "MPS Paper Truck",
    # Arctic + Tropic (not Temperate)
    "Foster Food Van", "Perry Food Van", "Chippy Food Van",
    # Tropic-only
    "MPS Copper Ore Truck", "Uhl Copper Ore Truck", "Goss Copper Ore Truck",
    "Uhl Water Tanker", "Balogh Water Tanker", "MPS Water Tanker",
    "Balogh Fruit Truck", "Uhl Fruit Truck", "Kelling Fruit Truck",
    "Balogh Rubber Truck", "Uhl Rubber Truck", "RMT Rubber Truck",
})

# Road vehicles NOT available on Arctic maps.
NON_ARCTIC_ROAD_VEHICLES: frozenset = frozenset({
    # Temperate-only
    "MPS Iron Ore Truck", "Uhl Iron Ore Truck", "Chippy Iron Ore Truck",
    "Balogh Steel Truck", "Uhl Steel Truck", "Kelling Steel Truck",
    # Tropic-only
    "MPS Copper Ore Truck", "Uhl Copper Ore Truck", "Goss Copper Ore Truck",
    "Uhl Water Tanker", "Balogh Water Tanker", "MPS Water Tanker",
    "Balogh Fruit Truck", "Uhl Fruit Truck", "Kelling Fruit Truck",
    "Balogh Rubber Truck", "Uhl Rubber Truck", "RMT Rubber Truck",
})

# Road vehicles NOT available on Tropic maps.
NON_TROPIC_ROAD_VEHICLES: frozenset = frozenset({
    # Temperate-only
    "MPS Iron Ore Truck", "Uhl Iron Ore Truck", "Chippy Iron Ore Truck",
    "Balogh Steel Truck", "Uhl Steel Truck", "Kelling Steel Truck",
    # Temperate + Arctic only (not Tropic)
    "Balogh Coal Truck", "Uhl Coal Truck", "DW Coal Truck",
    "Talbott Livestock Van", "Uhl Livestock Van", "Foster Livestock Van",
    # Arctic-only
    "Uhl Paper Truck", "Balogh Paper Truck", "MPS Paper Truck",
})

