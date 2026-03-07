from typing import TYPE_CHECKING, Dict
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


# ---------------------------------------------------------------------------
# Mission type → access rule mapping
# Used so that e.g. a "have trains" mission actually requires a train.
# Falls back to has_any_vehicle for generic types.
# ---------------------------------------------------------------------------
_TYPE_RULES = {
    "have trains":       lambda state, player: has_trains(state, player),
    "have aircraft":     lambda state, player: has_aircraft(state, player),
    "have ships":        lambda state, player: has_ships(state, player),
    "have road vehicles":lambda state, player: has_road_vehicles(state, player),
    "connect cities":    lambda state, player: has_trains(state, player) or has_road_vehicles(state, player),
}

# Mission types that require a train specifically (cargo types typically move by rail)
_TRAIN_CARGO_KEYWORDS = {
    "coal", "iron ore", "steel", "goods", "grain", "wood",
    "livestock", "valuables",
}


def _rule_for_mission(mission: dict):
    """Return the correct access rule lambda for a generated mission dict."""
    mtype = mission.get("type", "")
    unit  = mission.get("unit", "")

    # Explicit type mapping
    if mtype in _TYPE_RULES:
        return _TYPE_RULES[mtype]

    # Cargo missions: if unit mentions a train-only cargo, require trains
    if mtype in ("transport cargo", "deliver tons to station", "deliver goods in year",
                 "cargo_from_industry", "cargo_to_industry"):
        cargo = mission.get("cargo", "").lower()
        if any(k in cargo for k in _TRAIN_CARGO_KEYWORDS):
            return lambda state, player: has_trains(state, player)

    # Named passenger/mail missions only need any vehicle (bus/train both fine)
    if mtype in ("passengers_to_town", "mail_to_town"):
        return lambda state, player: has_any_vehicle(state, player)

    # Unit-based fallback
    if unit == "trains":
        return lambda state, player: has_trains(state, player)
    if unit == "aircraft":
        return lambda state, player: has_aircraft(state, player)
    if unit == "ships":
        return lambda state, player: has_ships(state, player)
    if unit == "road vehicles":
        return lambda state, player: has_road_vehicles(state, player)

    return lambda state, player: has_any_vehicle(state, player)


def set_rules(world: "OpenTTDWorld") -> None:
    player    = world.player
    multiworld = world.multiworld

    # Build a name → mission dict lookup from _generated_missions
    missions_by_loc: Dict[str, dict] = {}
    for m in getattr(world, "_generated_missions", []):
        loc = m.get("location", "")
        if loc:
            missions_by_loc[loc] = m

    # ------------------------------------------------------------------
    # Easy missions: type-appropriate vehicle required
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("mission_easy", player).locations:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        # Capture rule in closure correctly
        loc.access_rule = (lambda r: lambda state: r(state, player))(rule)

    # ------------------------------------------------------------------
    # Medium missions: type-appropriate vehicle + at least 3 total
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("mission_medium", player).locations:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r: lambda state: r(state, player) and has_transport_vehicles(state, player, 3)
        )(rule)

    # ------------------------------------------------------------------
    # Hard missions: type-appropriate vehicle + at least 6 total
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("mission_hard", player).locations:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r: lambda state: r(state, player) and has_transport_vehicles(state, player, 6)
        )(rule)

    # ------------------------------------------------------------------
    # Extreme missions: type-appropriate vehicle + at least 12 total
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("mission_extreme", player).locations:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r: lambda state: r(state, player) and has_transport_vehicles(state, player, 12)
        )(rule)

    # ------------------------------------------------------------------
    # Shop: split on price — cheap half accessible with 1 vehicle,
    # expensive half requires 4 vehicles (ensures sphere 2 exists)
    # ------------------------------------------------------------------
    shop_prices: Dict[str, int] = world._generate_shop_prices()

    if shop_prices:
        sorted_locs = sorted(shop_prices.keys(), key=lambda k: shop_prices[k])
        midpoint    = len(sorted_locs) // 2
        cheap_set   = set(sorted_locs[:midpoint])
    else:
        cheap_set = set()

    for loc in multiworld.get_region("shop", player).locations:
        if loc.name in cheap_set or not shop_prices:
            # Cheap tier: just need any vehicle
            loc.access_rule = lambda state: has_any_vehicle(state, player)
        else:
            # Expensive tier: need at least 4 vehicles
            loc.access_rule = lambda state: has_transport_vehicles(state, player, 4)

    # ------------------------------------------------------------------
    # Victory
    # ------------------------------------------------------------------
    victory = multiworld.get_location("Goal_Victory", player)
    victory.access_rule = lambda state: has_transport_vehicles(state, player, 10)
