from enum import IntEnum
from typing import Dict, List, NamedTuple
from BaseClasses import ItemClassification

OPENTTD_BASE_ID = 6_100_000


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
]

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


def _build_item_table() -> Dict[str, OpenTTDItemData]:
    table: Dict[str, OpenTTDItemData] = {}
    code = OPENTTD_BASE_ID

    def add(name: str, cls: ItemClassification, itype: ItemType, cat: str):
        nonlocal code
        if name in table:
            return  # Skip duplicates
        table[name] = OpenTTDItemData(code, cls, itype, cat)
        code += 1

    for name in ALL_TRAINS:
        add(name, ItemClassification.progression, ItemType.VEHICLE, "train")
    for name in ALL_WAGONS:
        add(name, ItemClassification.useful, ItemType.VEHICLE, "wagon")
    for name in ALL_ROAD_VEHICLES:
        add(name, ItemClassification.progression, ItemType.VEHICLE, "road_vehicle")
    for name in ALL_AIRCRAFT:
        add(name, ItemClassification.progression, ItemType.VEHICLE, "aircraft")
    for name in ALL_SHIPS:
        add(name, ItemClassification.progression, ItemType.VEHICLE, "ship")
    for name in TRAP_ITEMS:
        add(name, ItemClassification.trap, ItemType.TRAP, "trap")
    for name in UTILITY_ITEMS:
        add(name, ItemClassification.useful, ItemType.UTILITY, "utility")

    # Iron Horse engines — registered here so create_item() works even when
    # the option is disabled. Items only enter the *pool* when enabled (see __init__.py).
    for name in IRON_HORSE_ENGINES:
        add(name, ItemClassification.progression, ItemType.VEHICLE, "train")

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
VANILLA_ROAD_VEHICLES = ALL_ROAD_VEHICLES
VANILLA_AIRCRAFT = ALL_AIRCRAFT
VANILLA_SHIPS = ALL_SHIPS


STARTING_VEHICLES = {
    # Trains: steam and early diesel only.
    # These can haul passengers and mail which are always available regardless
    # of map industries. Toyland Choo-Choos are filtered out at world-gen time
    # when the landscape is not Toyland (see __init__.py).
    "train": [
        "Wills 2-8-0 (Steam)",
        "Kirby Paul Tank (Steam)",
        "MJS 250 (Diesel)",
        "Ploddyphut Diesel",
        "Powernaut Diesel",
        # Toyland — only selectable when landscape == Toyland
        "Ploddyphut Choo-Choo",
        "Powernaut Choo-Choo",
        "MightyMover Choo-Choo",
    ],
    # Road vehicles: buses and mail trucks ONLY.
    # Cargo trucks (goods, coal, grain, oil, etc.) require specific industries
    # to be present near a depot — not guaranteed at game start and will leave
    # the player with a vehicle they cannot use to earn money.
    "road_vehicle": [
        "MPS Regal Bus",
        "Hereford Leopard Bus",
        "Foster Bus",
        "MPS Mail Truck",
        "Perry Mail Truck",
    ],
    # Aircraft: first five small/cheap planes only.
    # Later planes are large jets requiring large airports the player cannot
    # afford at game start. The first five are all small props that work
    # on a basic airport from day one.
    "aircraft": [
        "Sampson U52",
        "Coleman Count",
        "FFP Dart",
        "Yate Haugan",
        "Bakewell Cotswald LB-3",
    ],
    # Ships: passenger ferries only.
    # Oil tankers and cargo ships require specific industries adjacent to water,
    # which is not guaranteed. Passenger ferries can always operate between
    # any two coastal or river towns.
    "ship": [
        "MPS Passenger Ferry",
        "FFP Passenger Ferry",
        "Chugger-Chug Passenger Ferry",
    ],
}
