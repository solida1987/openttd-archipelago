from typing import TYPE_CHECKING
from BaseClasses import MultiWorld, CollectionState
from .items import ALL_VEHICLES

if TYPE_CHECKING:
    from . import OpenTTDWorld


def has_any_vehicle(state: CollectionState, player: int) -> bool:
    """Player must have at least one vehicle unlocked."""
    return any(state.has(v, player) for v in ALL_VEHICLES)


def has_transport_vehicles(state: CollectionState, player: int, count: int = 3) -> bool:
    """Player must have at least N vehicles unlocked."""
    return sum(1 for v in ALL_VEHICLES if state.has(v, player)) >= count


def has_trains(state: CollectionState, player: int) -> bool:
    from .items import VANILLA_TRAINS
    return any(state.has(t, player) for t in VANILLA_TRAINS)


def has_road_vehicles(state: CollectionState, player: int) -> bool:
    from .items import VANILLA_ROAD_VEHICLES
    return any(state.has(rv, player) for rv in VANILLA_ROAD_VEHICLES)


def has_aircraft(state: CollectionState, player: int) -> bool:
    from .items import VANILLA_AIRCRAFT
    return any(state.has(a, player) for a in VANILLA_AIRCRAFT)


def has_ships(state: CollectionState, player: int) -> bool:
    from .items import VANILLA_SHIPS
    return any(state.has(s, player) for s in VANILLA_SHIPS)


def set_rules(world: "OpenTTDWorld") -> None:
    player = world.player
    multiworld = world.multiworld

    # Easy missions: just need any vehicle
    for loc in multiworld.get_region("mission_easy", player).locations:
        loc.access_rule = lambda state: has_any_vehicle(state, player)

    # Medium missions: need at least 3 vehicles total
    for loc in multiworld.get_region("mission_medium", player).locations:
        loc.access_rule = lambda state: has_transport_vehicles(state, player, 3)

    # Hard missions: need at least 8 vehicles total
    for loc in multiworld.get_region("mission_hard", player).locations:
        loc.access_rule = lambda state: has_transport_vehicles(state, player, 8)

    # Extreme missions: need at least 15 vehicles total
    for loc in multiworld.get_region("mission_extreme", player).locations:
        loc.access_rule = lambda state: has_transport_vehicles(state, player, 15)

    # Shop: just need any vehicle
    for loc in multiworld.get_region("shop", player).locations:
        loc.access_rule = lambda state: has_any_vehicle(state, player)

    # Victory: depends on win condition (checked server-side, but needs enough vehicles)
    victory = multiworld.get_location("Goal_Victory", player)
    victory.access_rule = lambda state: has_transport_vehicles(state, player, 10)
