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
    "train":        ["Wills 2-8-0 (Steam)", "Kirby Paul Tank (Steam)",
                     "Ploddyphut Choo-Choo", "Powernaut Choo-Choo",
                     "MJS 250 (Diesel)", "MightyMover Choo-Choo"],
    "road_vehicle": ["MPS Regal Bus", "Balogh Goods Truck", "Balogh Coal Truck"],
    "aircraft":     ["Sampson U52", "Coleman Count"],
    "ship":         ["MPS Passenger Ferry", "FFP Passenger Ferry"],
}
