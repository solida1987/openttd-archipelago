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
# ---------------------------------------------------------------------------
_TYPE_RULES = {
    "have trains":       lambda state, player: has_trains(state, player),
    "have aircraft":     lambda state, player: has_aircraft(state, player),
    "have ships":        lambda state, player: has_ships(state, player),
    "have road vehicles":lambda state, player: has_road_vehicles(state, player),
    "connect cities":    lambda state, player: has_trains(state, player) or has_road_vehicles(state, player),
}

_TRAIN_CARGO_KEYWORDS = {
    "coal", "iron ore", "steel", "goods", "grain", "wood",
    "livestock", "valuables",
}


def _rule_for_mission(mission: dict):
    """Return the correct access rule lambda for a generated mission dict."""
    mtype = mission.get("type", "")
    unit  = mission.get("unit", "")

    if mtype in _TYPE_RULES:
        return _TYPE_RULES[mtype]

    if mtype in ("transport cargo", "deliver tons to station", "deliver goods in year",
                 "cargo_from_industry", "cargo_to_industry"):
        cargo = mission.get("cargo", "").lower()
        if any(k in cargo for k in _TRAIN_CARGO_KEYWORDS):
            return lambda state, player: has_trains(state, player)

    if mtype in ("passengers_to_town", "mail_to_town"):
        return lambda state, player: has_any_vehicle(state, player)

    if unit == "trains":        return lambda state, player: has_trains(state, player)
    if unit == "aircraft":      return lambda state, player: has_aircraft(state, player)
    if unit == "ships":         return lambda state, player: has_ships(state, player)
    if unit == "road vehicles": return lambda state, player: has_road_vehicles(state, player)

    return lambda state, player: has_any_vehicle(state, player)


def set_rules(world: "OpenTTDWorld") -> None:
    player     = world.player
    multiworld = world.multiworld

    # Tier unlock threshold from options
    tier_count = world.options.mission_tier_unlock_count.value  # 0 = no gate

    # Build mission dict lookup
    missions_by_loc: Dict[str, dict] = {}
    for m in getattr(world, "_generated_missions", []):
        loc = m.get("location", "")
        if loc:
            missions_by_loc[loc] = m

    # Count easy missions available (for sphere gate calculation)
    easy_locs   = list(multiworld.get_region("mission_easy",    player).locations)
    medium_locs = list(multiworld.get_region("mission_medium",  player).locations)
    hard_locs   = list(multiworld.get_region("mission_hard",    player).locations)
    extreme_locs= list(multiworld.get_region("mission_extreme", player).locations)

    # ------------------------------------------------------------------
    # Easy: just need 1 vehicle + type-appropriate
    # ------------------------------------------------------------------
    for loc in easy_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (lambda r: lambda state: r(state, player))(rule)

    # ------------------------------------------------------------------
    # Medium: type-appropriate vehicle + tier_count easy missions worth
    # of vehicles (approximated as N vehicles unlocked for AP sphere calc)
    # Actual in-game gate is enforced by C++ AP_IsTierUnlocked().
    # ------------------------------------------------------------------
    medium_vehicle_req = max(1, tier_count) if tier_count > 0 else 1
    for loc in medium_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r, vr=medium_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
        )(rule)

    # ------------------------------------------------------------------
    # Hard: type vehicle + more vehicles unlocked
    # ------------------------------------------------------------------
    hard_vehicle_req = max(1, tier_count * 2) if tier_count > 0 else 3
    for loc in hard_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r, vr=hard_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
        )(rule)

    # ------------------------------------------------------------------
    # Extreme: most vehicles needed
    # ------------------------------------------------------------------
    extreme_vehicle_req = max(1, tier_count * 3) if tier_count > 0 else 6
    for loc in extreme_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        loc.access_rule = (
            lambda r, vr=extreme_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
        )(rule)

    # ------------------------------------------------------------------
    # Shop: require only 1 vehicle — C++ handles tier locking in-game
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("shop", player).locations:
        loc.access_rule = lambda state: has_any_vehicle(state, player)

    # ------------------------------------------------------------------
    # Victory
    # ------------------------------------------------------------------
    victory = multiworld.get_location("Goal_Victory", player)
    victory.access_rule = lambda state: has_transport_vehicles(state, player, 10)
