from typing import TYPE_CHECKING, Dict
from BaseClasses import MultiWorld, CollectionState
from .items import (
    ALL_VEHICLES, IRON_HORSE_ENGINES,
    MILITARY_ITEMS_AIRCRAFT, SHARK_SHIPS, HOVER_VEHICLES,
    HEQS_ROAD_VEHICLES, HEQS_TRAINS, VACTRAIN_ENGINES, AIRCRAFTPACK_AIRCRAFT,
    VANILLA_TRAINS, VANILLA_WAGONS, VANILLA_ROAD_VEHICLES, VANILLA_AIRCRAFT, VANILLA_SHIPS,
    # Infrastructure item lists for sphere/progression logic
    ALL_TRACK_DIRECTION_ITEMS, ROAD_DIRECTION_ITEMS, SIGNAL_ITEMS,
    BRIDGE_ITEMS, TUNNEL_ITEMS, AIRPORT_ITEMS, TERRAFORM_ITEMS,
)

if TYPE_CHECKING:
    from . import OpenTTDWorld

# Combined vehicle lists including all GRF vehicles for rule checks
_ALL_VEHICLES_FULL = (ALL_VEHICLES + IRON_HORSE_ENGINES + MILITARY_ITEMS_AIRCRAFT
                      + SHARK_SHIPS + HOVER_VEHICLES + HEQS_ROAD_VEHICLES + HEQS_TRAINS
                      + VACTRAIN_ENGINES + AIRCRAFTPACK_AIRCRAFT)

_ALL_TRAINS_FULL = list(VANILLA_TRAINS) + list(IRON_HORSE_ENGINES) + list(HEQS_TRAINS) + list(VACTRAIN_ENGINES)
_ALL_ROAD_VEHICLES_FULL = list(VANILLA_ROAD_VEHICLES) + list(HOVER_VEHICLES) + list(HEQS_ROAD_VEHICLES)
_ALL_AIRCRAFT_FULL = list(VANILLA_AIRCRAFT) + list(MILITARY_ITEMS_AIRCRAFT) + list(AIRCRAFTPACK_AIRCRAFT)
_ALL_SHIPS_FULL = list(VANILLA_SHIPS) + list(SHARK_SHIPS)

# Vactrain wagons count as wagons for rule purposes
_VACTRAIN_WAGONS = [v for v in VACTRAIN_ENGINES if "Wagon" in v or "Cargo Module" in v]
_ALL_WAGONS_FULL = list(VANILLA_WAGONS) + _VACTRAIN_WAGONS


def has_any_vehicle(state: CollectionState, player: int) -> bool:
    """Player must have at least one vehicle unlocked (including all GRF vehicles)."""
    return any(state.has(v, player) for v in _ALL_VEHICLES_FULL)


def has_transport_vehicles(state: CollectionState, player: int, count: int = 3) -> bool:
    """Player must have at least N vehicles unlocked (including all GRF vehicles)."""
    return sum(1 for v in _ALL_VEHICLES_FULL if state.has(v, player)) >= count


def has_trains(state: CollectionState, player: int) -> bool:
    return any(state.has(t, player) for t in _ALL_TRAINS_FULL)


def has_wagons(state: CollectionState, player: int) -> bool:
    """Player must have at least one wagon unlocked (including Vactrain wagons)."""
    return any(state.has(w, player) for w in _ALL_WAGONS_FULL)


def has_cargo_train(state: CollectionState, player: int) -> bool:
    """Player must have a train engine AND a wagon (i.e. can actually carry cargo by rail)."""
    return has_trains(state, player) and has_wagons(state, player)


def has_road_vehicles(state: CollectionState, player: int) -> bool:
    return any(state.has(rv, player) for rv in _ALL_ROAD_VEHICLES_FULL)


def has_aircraft(state: CollectionState, player: int) -> bool:
    return any(state.has(a, player) for a in _ALL_AIRCRAFT_FULL)


def has_ships(state: CollectionState, player: int) -> bool:
    return any(state.has(s, player) for s in _ALL_SHIPS_FULL)


def has_cargo_capability(state: CollectionState, player: int) -> bool:
    """Player can actually transport cargo: train+wagon, road vehicle, ship, or aircraft."""
    return (has_cargo_train(state, player)
            or has_road_vehicles(state, player)
            or has_ships(state, player)
            or has_aircraft(state, player))


# ---------------------------------------------------------------------------
# Infrastructure availability checks (for sphere/progression logic)
# ---------------------------------------------------------------------------

def has_any_rail_direction(state: CollectionState, player: int) -> bool:
    """Player has at least one rail track direction unlock (any rail type)."""
    return any(state.has(item, player) for item in ALL_TRACK_DIRECTION_ITEMS)


def has_any_road_direction(state: CollectionState, player: int) -> bool:
    """Player has at least one road direction unlock."""
    return any(state.has(item, player) for item in ROAD_DIRECTION_ITEMS)


def has_any_signal(state: CollectionState, player: int) -> bool:
    """Player has at least one signal type unlock."""
    return any(state.has(item, player) for item in SIGNAL_ITEMS)


def has_any_bridge(state: CollectionState, player: int) -> bool:
    """Player has at least one bridge type unlock."""
    return any(state.has(item, player) for item in BRIDGE_ITEMS)


def has_tunnel(state: CollectionState, player: int) -> bool:
    """Player has tunnel construction unlock."""
    return state.has("Tunnel Construction", player)


def has_any_airport(state: CollectionState, player: int) -> bool:
    """Player has at least one airport type unlock."""
    return any(state.has(item, player) for item in AIRPORT_ITEMS)


def has_any_terraform(state: CollectionState, player: int) -> bool:
    """Player has at least one terraform ability unlock."""
    return any(state.has(item, player) for item in TERRAFORM_ITEMS)


# ---------------------------------------------------------------------------
# Mission type -> access rule mapping
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
    """Return the correct access rule lambda for a generated mission dict.

    Cargo-transport missions require has_cargo_train (train + wagon) instead of
    just has_trains, so the player is never stuck with engines that cannot carry
    anything.  Non-train cargo vehicles (road, ship, aircraft) are self-
    contained and don't need a separate wagon check.
    """
    mtype = mission.get("type", "")
    unit  = mission.get("unit", "")

    if mtype in _TYPE_RULES:
        return _TYPE_RULES[mtype]

    if mtype in ("transport cargo", "deliver tons to station", "deliver goods in year",
                 "cargo_from_industry", "cargo_to_industry"):
        cargo = mission.get("cargo", "").lower()
        if any(k in cargo for k in _TRAIN_CARGO_KEYWORDS):
            # Train-hauled cargo: need engine + wagon
            return lambda state, player: has_cargo_train(state, player)
        # Generic cargo: any vehicle that can carry stuff
        return lambda state, player: has_cargo_capability(state, player)

    if mtype in ("passengers_to_town", "mail_to_town"):
        return lambda state, player: has_cargo_capability(state, player)

    if unit == "trains":        return lambda state, player: has_cargo_train(state, player)
    if unit == "aircraft":      return lambda state, player: has_aircraft(state, player)
    if unit == "ships":         return lambda state, player: has_ships(state, player)
    if unit == "road vehicles": return lambda state, player: has_road_vehicles(state, player)

    return lambda state, player: has_cargo_capability(state, player)


# ---------------------------------------------------------------------------
# Infrastructure-based sphere gating
# ---------------------------------------------------------------------------

def _mission_vehicle_type(mission: dict) -> str:
    """Determine the primary vehicle type a mission requires.

    Returns one of: 'train', 'road', 'aircraft', 'ship', 'train_or_road', 'any'
    """
    mtype = mission.get("type", "")
    unit  = mission.get("unit", "")

    if mtype == "have trains" or unit == "trains":
        return "train"
    if mtype == "have road vehicles" or unit == "road vehicles":
        return "road"
    if mtype == "have aircraft" or unit == "aircraft":
        return "aircraft"
    if mtype == "have ships" or unit == "ships":
        return "ship"
    if mtype == "connect cities":
        return "train_or_road"
    return "any"


def _build_infra_rule(difficulty: str, mission: dict, world: "OpenTTDWorld"):
    """Build an infrastructure access rule for the given difficulty tier.

    Returns a callable ``(state, player) -> bool`` that is AND-ed with the
    existing vehicle rule, or *None* if no infrastructure gating applies.

    Tier logic (each check only applied when the YAML option is enabled):
      easy:    no infrastructure required
      medium:  type-appropriate basic infra (rail dirs / road dirs / airports)
      hard:    medium + crossing infra (bridge OR tunnel)
      extreme: hard + terraform
    """
    if difficulty == "easy":
        return None

    opts  = world.options
    vtype = _mission_vehicle_type(mission)

    # ── Collect basic (type-appropriate) infra checks ──────────────
    basic = []  # these will be OR-ed for "any"-type missions
    if vtype in ("train", "train_or_road", "any"):
        if opts.enable_rail_direction_unlocks.value:
            basic.append(has_any_rail_direction)
    if vtype in ("road", "train_or_road", "any"):
        if opts.enable_road_direction_unlocks.value:
            basic.append(has_any_road_direction)
    if vtype in ("aircraft", "any"):
        if opts.enable_airport_unlocks.value:
            basic.append(has_any_airport)

    # ── Collect advanced (tier-specific) infra checks ─────────────
    advanced = []  # these are always AND-ed
    if difficulty in ("hard", "extreme"):
        bridge_on = bool(opts.enable_bridge_unlocks.value)
        tunnel_on = bool(opts.enable_tunnel_unlocks.value)
        if bridge_on and tunnel_on:
            advanced.append(lambda state, player: has_any_bridge(state, player) or has_tunnel(state, player))
        elif bridge_on:
            advanced.append(has_any_bridge)
        elif tunnel_on:
            advanced.append(has_tunnel)

    if difficulty == "extreme":
        if opts.enable_terraform_unlocks.value:
            advanced.append(has_any_terraform)

    # ── Nothing to gate on ─────────────────────────────────────────
    if not basic and not advanced:
        return None

    # ── Build combined rule ────────────────────────────────────────
    # For specific vehicle types, basic has at most 1 entry → simple AND.
    # For "any" / "train_or_road", basic entries are OR-ed (player needs
    # at least ONE matching infra, not all of them).
    if len(basic) > 1:
        def _basic_or(state, player, _fns=tuple(basic)):
            return any(fn(state, player) for fn in _fns)
        all_checks = [_basic_or] + advanced
    else:
        all_checks = basic + advanced

    if len(all_checks) == 1:
        return all_checks[0]

    def _combined(state, player, _fns=tuple(all_checks)):
        return all(fn(state, player) for fn in _fns)
    return _combined


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
    # Medium: type-appropriate vehicle + tier_count vehicles + infra
    # Actual in-game tier gate enforced by C++ AP_IsTierUnlocked().
    # ------------------------------------------------------------------
    medium_vehicle_req = max(1, tier_count) if tier_count > 0 else 1
    for loc in medium_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        infra   = _build_infra_rule("medium", mission, world)
        if infra is not None:
            loc.access_rule = (
                lambda r, ir, vr=medium_vehicle_req: lambda state: (
                    r(state, player) and has_transport_vehicles(state, player, vr) and ir(state, player)
                )
            )(rule, infra)
        else:
            loc.access_rule = (
                lambda r, vr=medium_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
            )(rule)

    # ------------------------------------------------------------------
    # Hard: type vehicle + more vehicles + bridge/tunnel infra
    # ------------------------------------------------------------------
    hard_vehicle_req = max(1, tier_count * 2) if tier_count > 0 else 3
    for loc in hard_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        infra   = _build_infra_rule("hard", mission, world)
        if infra is not None:
            loc.access_rule = (
                lambda r, ir, vr=hard_vehicle_req: lambda state: (
                    r(state, player) and has_transport_vehicles(state, player, vr) and ir(state, player)
                )
            )(rule, infra)
        else:
            loc.access_rule = (
                lambda r, vr=hard_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
            )(rule)

    # ------------------------------------------------------------------
    # Extreme: most vehicles + full infrastructure (+ terraform)
    # ------------------------------------------------------------------
    extreme_vehicle_req = max(1, tier_count * 3) if tier_count > 0 else 6
    for loc in extreme_locs:
        mission = missions_by_loc.get(loc.name, {})
        rule    = _rule_for_mission(mission)
        infra   = _build_infra_rule("extreme", mission, world)
        if infra is not None:
            loc.access_rule = (
                lambda r, ir, vr=extreme_vehicle_req: lambda state: (
                    r(state, player) and has_transport_vehicles(state, player, vr) and ir(state, player)
                )
            )(rule, infra)
        else:
            loc.access_rule = (
                lambda r, vr=extreme_vehicle_req: lambda state: r(state, player) and has_transport_vehicles(state, player, vr)
            )(rule)

    # ------------------------------------------------------------------
    # Shop: require cargo capability — player needs to earn money
    # ------------------------------------------------------------------
    for loc in multiworld.get_region("shop", player).locations:
        loc.access_rule = lambda state: has_cargo_capability(state, player)

    # ------------------------------------------------------------------
    # Ruins: require cargo capability — ruins need cargo deliveries
    # ------------------------------------------------------------------
    ruin_region = multiworld.get_region("ruin", player)
    for loc in ruin_region.locations:
        loc.access_rule = lambda state: has_cargo_capability(state, player)

    # ------------------------------------------------------------------
    # Demigods: require multiple vehicles + transport capability
    # Defeating demigods requires tribute (money), which means the player
    # needs a well-established transport network — so we gate on vehicle count.
    # ------------------------------------------------------------------
    demigod_region = multiworld.get_region("demigod", player)
    demigod_locs = list(demigod_region.locations)
    for idx, loc in enumerate(demigod_locs):
        # Progressive requirement: later demigods need more vehicles
        veh_req = max(3, 3 + idx * 2)
        loc.access_rule = (
            lambda vr=veh_req: lambda state: has_transport_vehicles(state, player, vr)
        )(veh_req)

    # ------------------------------------------------------------------
    # Victory
    # ------------------------------------------------------------------
    victory = multiworld.get_location("Goal_Victory", player)
    victory.access_rule = lambda state: has_cargo_capability(state, player) and has_transport_vehicles(state, player, 10)
