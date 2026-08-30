"""
OpenTTD Archipelago World
Version: 2.5.0
Supports: OpenTTD 15.2

A full Archipelago integration for OpenTTD.
All vanilla vehicles are randomized as individual items.
Missions are randomly generated as checks.
"""

import random
from typing import Dict, Any, List, Optional
from BaseClasses import Region, Item, ItemClassification, Tutorial, MultiWorld
from worlds.AutoWorld import World, WebWorld

from .items import (
    ITEM_TABLE, ALL_VEHICLES, TRAP_ITEMS, UTILITY_ITEMS,
    ALL_TRAINS, ALL_WAGONS, ALL_ROAD_VEHICLES, ALL_AIRCRAFT, ALL_SHIPS,
    VANILLA_TRAINS, VANILLA_WAGONS, VANILLA_ROAD_VEHICLES,
    VANILLA_AIRCRAFT, VANILLA_SHIPS, IRON_HORSE_ENGINES,
    VANILLA_RAIL_ENGINES,
    MILITARY_ITEMS_AIRCRAFT, SHARK_SHIPS, HOVER_VEHICLES,
    HEQS_ROAD_VEHICLES, HEQS_TRAINS, VACTRAIN_ENGINES, AIRCRAFTPACK_AIRCRAFT,
    ARCTIC_TROPIC_ONLY_TRAINS, TEMPERATE_ONLY_TRAINS,
    NON_TEMPERATE_ROAD_VEHICLES, NON_ARCTIC_ROAD_VEHICLES, NON_TROPIC_ROAD_VEHICLES,
    NON_TEMPERATE_WAGONS, NON_ARCTIC_WAGONS, NON_TROPIC_WAGONS,
    ALL_TRACK_DIRECTION_ITEMS, TRACK_ITEMS_BY_RAILTYPE,
    NARROW_GAUGE_TRACK_ITEMS, METRO_TRACK_ITEMS, VACTUBE_TRACK_ITEMS,
    TRAIN_TO_RAILTYPE, UNIVERSAL_STARTER_WAGONS,
    SMALL_AIRCRAFT, UNSAFE_STARTER_ROAD_VEHICLES,
    SAFE_STARTER_SHIPS, SAFE_STARTER_SHARK,
    IH_NON_STANDARD_ENGINES, VANILLA_SAFE_STARTER_TRAINS,
    SMALL_AIRCRAFT_AP25, SMALL_AIRCRAFT_MIL, LARGE_AIRCRAFT_MIL,
    ROAD_DIRECTION_ITEMS, TRAM_DIRECTION_ITEMS,
    SIGNAL_ITEMS, BRIDGE_ITEMS, TUNNEL_ITEMS,
    AIRPORT_ITEMS, TREE_ITEMS, TERRAFORM_ITEMS, TOWN_ACTION_ITEMS,
    ESSENTIAL_VEHICLES,
    OpenTTDItemData
)
from .locations import (
    get_location_table, DIFFICULTY_DISTRIBUTION, MAX_MISSIONS_PER_DIFFICULTY,
    MISSION_TEMPLATES, PREDEFINED_MISSION_POOLS, CARGO_TYPES, CARGO_BY_LANDSCAPE,
    FIRS_CARGO_BY_ECONOMY, RUIN_ID_BASE, STAR_ID_BASE, get_cargo_list
)
from .options import OpenTTDOptions, OPTION_GROUPS
from .rules import set_rules


# ─────────────────────────────────────────────────────────────────────────────
#  LANDSCAPE VEHICLE FILTER — module-level so _compute_pool_size and
#  create_items both use the exact same set (no drift between the two).
# ─────────────────────────────────────────────────────────────────────────────
_TOYLAND_ONLY_VEHICLES: frozenset = frozenset({
    # Trains — engines
    "Ploddyphut Choo-Choo", "Powernaut Choo-Choo", "MightyMover Choo-Choo",
    "Wizzowow Z99",         # Monorail
    "Wizzowow Rocketeer",   # Maglev
    # Trains — wagons (Toyland-only cargo)
    "Candyfloss Hopper", "Toffee Hopper", "Cola Tanker", "Plastic Truck",
    "Fizzy Drink Truck", "Sugar Truck",   "Sweet Van",   "Bubble Van",
    "Toy Van", "Battery Truck",
    # Road — buses
    "Ploddyphut MkI Bus", "Ploddyphut MkII Bus", "Ploddyphut MkIII Bus",
    # Road — mail trucks
    "MightyMover Mail Truck", "Powernaught Mail Truck", "Wizzowow Mail Truck",
    # Road — cargo trucks
    "MightyMover Candyfloss Truck", "Powernaught Candyfloss Truck", "Wizzowow Candyfloss Truck",
    "MightyMover Toffee Truck",     "Powernaught Toffee Truck",     "Wizzowow Toffee Truck",
    "MightyMover Cola Truck",       "Powernaught Cola Truck",       "Wizzowow Cola Truck",
    "MightyMover Plastic Truck",    "Powernaught Plastic Truck",    "Wizzowow Plastic Truck",
    "MightyMover Fizzy Drink Truck","Powernaught Fizzy Drink Truck","Wizzowow Fizzy Drink Truck",
    "MightyMover Sugar Truck",      "Powernaught Sugar Truck",      "Wizzowow Sugar Truck",
    "MightyMover Sweet Truck",      "Powernaught Sweet Truck",      "Wizzowow Sweet Truck",
    "MightyMover Battery Truck",    "Powernaught Battery Truck",    "Wizzowow Battery Truck",
    "MightyMover Bubble Truck",     "Powernaught Bubble Truck",     "Wizzowow Bubble Truck",
    "MightyMover Toy Van",          "Powernaught Toy Van",          "Wizzowow Toy Van",
    # Ships
    "Chugger-Chug Passenger Ferry", "Shivershake Passenger Ferry",
    "MightyMover Cargo Ship",       "Powernaut Cargo Ship",
    # Aircraft
    "Ploddyphut 100", "Ploddyphut 500", "Flashbang X1", "Flashbang Wizzer",
    "Juggerplane M1", "Powernaut Helicopter",
    # NOTE: Guru Galaxy is Temperate/Arctic/Tropic — NOT Toyland-only
})

# Universal vehicles — available on ALL climates including Toyland.
# These are included in the Toyland pool even though they are not in _TOYLAND_ONLY_VEHICLES.
_UNIVERSAL_VEHICLES: frozenset = frozenset({
    "Passenger Carriage", "Mail Van",
    "Guru Galaxy",  # Helicopter available on all climates
})

# ─────────────────────────────────────────────────────────────────────────────
#  REGISTRY SIZES — how many names of each kind the class-level
#  location_name_to_id carries. A seed may never ask for more than this of any
#  kind, or the location it builds has no id in the data package.
#
#  ⚠⚠ The shop used to stop at 600 while the runtime shop is
#  `base_items - mission_count`, which reaches 858 at the far end of the
#  ranges (tropic + every GRF + utility_count 300 + trap_count 50 +
#  speed_boost_count 100 + every unlock toggle + enable_stars off, which hands
#  the whole remainder to the shop instead of halving it with the stars).
#  Shop_Purchase_0601 and up then existed in the multiworld and nowhere in the
#  data package.
#
#  1000 is safe on both counts: it is above the 858 ceiling, and it still fits
#  the 2000-slot shop id block (SHOP_ID_BASE 6_108_000, Victory at 6_110_000),
#  so it only ADDS names -- every existing Shop_Purchase_NNNN keeps the id it
#  has always had and old seeds and trackers still resolve.
# ─────────────────────────────────────────────────────────────────────────────
SHOP_REGISTRY_SIZE = 1000


class OpenTTDWeb(WebWorld):
    theme = "ocean"
    option_groups = OPTION_GROUPS
    tutorials = [Tutorial(
        "OpenTTD Setup Guide",
        "A guide to setting up OpenTTD Archipelago.",
        "English",
        "setup_en.md",
        "setup/en",
        ["OpenTTD AP Team"],
    )]


class OpenTTDItem(Item):
    game = "OpenTTD"


class OpenTTDWorld(World):
    """
    OpenTTD is an open-source transport simulation game.
    Build transport networks using trains, road vehicles, aircraft and ships.
    All vehicles are randomized — unlock them through Archipelago checks!
    """
    game = "OpenTTD"
    options_dataclass = OpenTTDOptions
    options: OpenTTDOptions
    web = OpenTTDWeb()

    item_name_to_id = {name: data.code for name, data in ITEM_TABLE.items()}
    # Pre-build with max possible config so AP can read locations at class level.
    #
    # ⭐ This is the REGISTRY, not a seed: every location name this world could
    # ever hand out. It has to be at least as large as the biggest seed any
    # option combination can produce, or generation fails on a missing name --
    # ruin_count here must therefore keep up with RuinPoolSize.range_end.
    #
    # ⚠ Growing it is safe only because _build_location_table assigns ids from
    # a FIXED per-category base (DIFFICULTY_ID_OFFSET / SHOP_ID_BASE /
    # RUIN_ID_BASE / ...). Ruin_100 keeps id RUIN_ID_BASE+99 whatever else
    # changes, so seeds and trackers built against the old registry still
    # resolve. Sequential ids across categories would have made this a
    # breaking change.
    location_name_to_id: Dict[str, int] = {
        name: data.code
        for name, data in get_location_table(mission_count=600, shop_item_count=SHOP_REGISTRY_SIZE, ruin_count=500, demigod_count=10, star_count=1000).items()
    }

    # Highest year the engine can represent (timer_game_common.h MAX_YEAR).
    _MAX_GAME_YEAR = 5_000_000

    # Slot data stored during generation
    _slot_data: Dict[str, Any] = {}
    _generated_missions: List[Dict] = []

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._generated_missions = []
        self._slot_data = {}
        self._shop_prices_cache: Dict[str, int] = {}

    def _get_location_table(self):
        mc, shop, ruin, dg, star = self._compute_pool_size()
        return get_location_table(mc, shop, ruin, dg, star)

    def _eligible_vehicle_names(self) -> List[str]:
        """Every vehicle this seed's landscape and GRFs allow, as a list.

        ⚠⚠ ONE definition, used by both _compute_pool_size and create_items.
        They used to compute it twice -- a count on one side, a list on the
        other -- and the two disagreed: the count subtracted the climate's
        wagons through _climate_exclude and then subtracted every non-Toyland
        wagon a second time. Measured on Temperate with enable_wagon_unlocks
        off (the DEFAULT): 93 counted against 99 actually built. The five or
        six climate-valid vehicles that difference trimmed away were still
        listed in locked_vehicles, so the game locked them for good with no
        item anywhere that could unlock them.
        """
        landscape = self.options.landscape.value
        is_toyland = (landscape == 3)

        if is_toyland:
            eligible = [v for v in ALL_VEHICLES if v in _TOYLAND_ONLY_VEHICLES
                        or v in _UNIVERSAL_VEHICLES]
        else:
            if landscape == 0:    # Temperate
                climate_exclude = (NON_TEMPERATE_ROAD_VEHICLES
                                   | ARCTIC_TROPIC_ONLY_TRAINS | NON_TEMPERATE_WAGONS)
            elif landscape == 1:  # Arctic
                climate_exclude = (NON_ARCTIC_ROAD_VEHICLES
                                   | TEMPERATE_ONLY_TRAINS | NON_ARCTIC_WAGONS)
            else:                 # Tropic
                climate_exclude = (NON_TROPIC_ROAD_VEHICLES
                                   | TEMPERATE_ONLY_TRAINS | NON_TROPIC_WAGONS)
            full_exclude = _TOYLAND_ONLY_VEHICLES | climate_exclude
            eligible = [v for v in ALL_VEHICLES if v not in full_exclude]

        # NewGRF sets. None of them exist on Toyland, which has no GRF variants.
        if bool(self.options.enable_iron_horse.value) and not is_toyland:
            eligible = eligible + IRON_HORSE_ENGINES
            # IH replaces the vanilla normal-rail engines in-game; Monorail and
            # Maglev are untouched and stay in the pool.
            eligible = [v for v in eligible if v not in VANILLA_RAIL_ENGINES]
        if bool(self.options.enable_military_items.value) and not is_toyland:
            eligible = eligible + MILITARY_ITEMS_AIRCRAFT
        if bool(self.options.enable_shark_ships.value) and not is_toyland:
            eligible = eligible + SHARK_SHIPS
            eligible = [v for v in eligible if v not in ALL_SHIPS]
        if bool(self.options.enable_hover_vehicles.value) and not is_toyland:
            eligible = eligible + HOVER_VEHICLES
        if bool(self.options.enable_heqs.value) and not is_toyland:
            eligible = eligible + HEQS_ROAD_VEHICLES + HEQS_TRAINS
        if bool(self.options.enable_vactrain.value) and not is_toyland:
            eligible = eligible + VACTRAIN_ENGINES
        if bool(self.options.enable_aircraftpack.value) and not is_toyland:
            eligible = eligible + AIRCRAFTPACK_AIRCRAFT

        # Wagons disabled: they are free in-game, so no items for them.
        if not bool(self.options.enable_wagon_unlocks.value):
            eligible = [v for v in eligible if v not in ALL_WAGONS]

        return eligible

    def obtainable_vehicle_count(self) -> int:
        """How many distinct vehicles this seed can ever hand the player.

        ⚠⚠ ONLY THE ONES THE LOGIC CAN SEE.

        has_transport_vehicles asks state.has() for each vehicle, and the fill
        sweep only ever collects PROGRESSION items — every vehicle outside
        ESSENTIAL_VEHICLES is classified `useful` and is invisible to it. So
        the ceiling for a gate is the count of essential vehicles this seed can
        hand out, not the count of vehicles.

        Counting the whole pool is what made "temperate + every GRF" fail: the
        pool is large enough that a third of it ran past every essential
        vehicle in the seed, and the fill ran out of places to put the
        progression items that were left.

        Starting vehicles are precollected, so they count. Before create_items
        has run there is no pool yet; the eligible list is its upper bound.
        """
        pool = getattr(self, "_vehicle_pool", None)
        if pool is None:
            names = set(self._eligible_vehicle_names())
        else:
            names = set(pool) | set(getattr(self, "_starting_vehicles", ()))
        return len(names & ESSENTIAL_VEHICLES)

    def _active_cargo_list(self) -> List[str]:
        """The cargoes that exist on this seed's map (landscape or FIRS economy)."""
        landscape = self.options.landscape.value
        firs_enabled = bool(self.options.enable_firs.value) and landscape != 3
        return get_cargo_list(landscape, firs_enabled,
                              self.options.firs_economy.value)

    def _colby_cargo_name(self) -> str:
        """The cargo Colby's deliveries fall back to, lower-case for the client.

        ⚠ "goods" was hardcoded for every landscape but Toyland. Toyland has no
        Goods, and neither do the FIRS Arctic Basic and Steeltown economies —
        AP_FindCargoType would return INVALID_CARGO and the event could not be
        measured. The order below keeps the old answer wherever the old answer
        was right, and finds a real one where it was not.
        """
        cargo_list = self._active_cargo_list()
        available = {c.lower() for c in cargo_list}
        for preferred in ("goods", "sweets", "food", "metal", "steel", "mail"):
            if preferred in available:
                return preferred
        return cargo_list[0].lower() if cargo_list else "goods"

    def _compute_pool_size(self) -> tuple:
        """Dynamically compute (mission_count, shop_item_count, ruin_count, demigod_count, star_count).

        Pool size is derived automatically from:
          - Available vehicles for the chosen landscape + active GRFs
          - trap_count  (explicit YAML option)
          - utility_count  (explicit YAML option)

        Total items = vehicles + traps + utility.
        Split exactly 50/50: mission_count = shop_item_count = total // 2.
        The player never controls mission_count or shop size directly.
        Ruin and demigod locations are additional — they need items from the pool.
        """
        landscape = self.options.landscape.value
        is_toyland = (landscape == 3)
        ih_enabled = bool(self.options.enable_iron_horse.value) and not is_toyland
        vac_enabled = bool(self.options.enable_vactrain.value) and not is_toyland

        # The vehicles create_items will actually build items for — the same
        # list, not a second count of it.
        eligible_count = len(self._eligible_vehicle_names())

        trap_count   = self.options.trap_count.value
        utility_count = self.options.utility_count.value

        # Ruin locations — each one needs an item placed on it
        ruin_count = self.options.ruin_pool_size.value

        # Demigod locations — each one needs an item placed on it
        demigod_count = self.options.demigod_count.value if bool(self.options.enable_demigods.value) else 0

        # Star locations — computed dynamically below (50/50 split with shop)
        stars_enabled = bool(self.options.enable_stars.value)

        # Infrastructure unlock items — count based on enabled toggles
        infra_count = 0
        if bool(self.options.enable_rail_direction_unlocks.value):
            infra_count += len(ALL_TRACK_DIRECTION_ITEMS)  # 24 vanilla
            if ih_enabled:
                infra_count += len(NARROW_GAUGE_TRACK_ITEMS) + len(METRO_TRACK_ITEMS)  # +12
            if vac_enabled:
                infra_count += len(VACTUBE_TRACK_ITEMS)  # +6
        if bool(self.options.enable_road_direction_unlocks.value):
            infra_count += len(ROAD_DIRECTION_ITEMS)  # 2
            infra_count += len(TRAM_DIRECTION_ITEMS)   # 2
        if bool(self.options.enable_signal_unlocks.value):
            infra_count += len(SIGNAL_ITEMS)  # 6
        if bool(self.options.enable_bridge_unlocks.value):
            infra_count += len(BRIDGE_ITEMS)  # 13
        if bool(self.options.enable_tunnel_unlocks.value):
            infra_count += len(TUNNEL_ITEMS)  # 1
        if bool(self.options.enable_airport_unlocks.value):
            infra_count += len(AIRPORT_ITEMS)  # 8
        if bool(self.options.enable_tree_unlocks.value):
            infra_count += len(TREE_ITEMS)  # 10
        if bool(self.options.enable_terraform_unlocks.value):
            infra_count += len(TERRAFORM_ITEMS)  # 2
        if bool(self.options.enable_town_action_unlocks.value):
            infra_count += len(TOWN_ACTION_ITEMS)  # 8

        # Wagons are already out of _eligible_vehicle_names when their unlock
        # toggle is off — do NOT subtract them a second time here.

        speed_boost_count = self.options.speed_boost_count.value

        # ── Total item budget ────────────────────────────────────────────
        # All items that need locations (vehicles + traps + utility + infra + speed
        # + ruins + demigods).  Stars and shop are computed from the remainder.
        base_items = (eligible_count + trap_count + utility_count
                      + infra_count + speed_boost_count)
        total_items = base_items + ruin_count + demigod_count

        # ── Distribution: missions ~25%, rest split between stars/shop ──
        # Missions get Easy 10% + Medium 5% + Hard 5% + Extreme 5% = 25% of items.
        # Ruins and demigods are fixed-count (taken off the top).
        # Stars + shop share whatever is left (50/50 split).
        #
        # We compute missions from the tier fractions applied to total_items,
        # then cap so missions never consume more than ~40% of base_items,
        # leaving enough room for stars + shop.
        MAX_MISSION_FRACTION = 0.60

        # Compute mission count from difficulty distribution
        mission_count = 0
        for _diff, fraction in DIFFICULTY_DISTRIBUTION.items():
            tier = min(max(1, int(total_items * fraction)), MAX_MISSIONS_PER_DIFFICULTY)
            mission_count += tier

        # Cap missions to leave room for stars + shop
        max_missions = max(10, int(base_items * MAX_MISSION_FRACTION))
        if mission_count > max_missions:
            mission_count = max_missions

        # Also cap to never exceed base_items (can't have more locations than items)
        if mission_count > base_items:
            mission_count = base_items

        # Remainder after missions = pool for stars + shop
        remainder = max(0, base_items - mission_count)

        # Split remainder 50/50 between stars and shop
        if stars_enabled and remainder >= 2:
            star_count = remainder // 2
            shop_item_count = remainder - star_count
        else:
            star_count = 0
            shop_item_count = remainder

        # ⚠⚠ Never ask for a shop slot the class-level registry has no name
        # for. The registry is sized above the measured ceiling, so this
        # clamp should never bite; it is here so that widening an option
        # range later cannot quietly produce a location with no id. Items
        # beyond the clamp are trimmed by create_items' existing pad/trim.
        shop_item_count = min(shop_item_count, SHOP_REGISTRY_SIZE)

        # Total locations = mission_count + shop_item_count + ruin_count + demigod_count + star_count
        # This equals base_items + ruin_count + demigod_count = total_items. Balanced.

        return mission_count, shop_item_count, ruin_count, demigod_count, star_count

    def generate_early(self) -> None:
        """Generate mission content before items are placed."""
        # ── Sphere master toggle → auto-set all infrastructure sub-options ──
        if bool(self.options.enable_sphere_progression.value):
            from Options import Toggle
            for attr in ("enable_rail_direction_unlocks", "enable_road_direction_unlocks",
                         "enable_signal_unlocks", "enable_bridge_unlocks",
                         "enable_tunnel_unlocks", "enable_airport_unlocks",
                         "enable_terraform_unlocks", "enable_wagon_unlocks",
                         "enable_tree_unlocks", "enable_town_action_unlocks"):
                getattr(self.options, attr).value = 1

        # ── Multiplayer mode → switch off what the game switches off ──────
        # ⚠⚠ MultiplayerMode promises Ruins, Colby and the Demigod system are
        # disabled, because they edit the map directly and desync. The game
        # duly refuses to run them — but the pool computation and the location
        # table never consulted the flag, so a multiplayer seed still carried
        # Ruin_ and Demigod_ locations that nothing in the session could ever
        # check. The multiworld hangs on them.
        #
        # Forcing the option values here, not at every reader, means
        # _compute_pool_size, the location table and slot_data all see the
        # same answer. Same shape as the sphere master toggle above.
        # ⚠ The list must match archipelago_manager.cpp's own multiplayer block
        # exactly. The game turns off FIVE things there, and stars were the one
        # missing here -- a multiplayer seed would still have carried up to a
        # thousand Star_ locations that the session refuses to place.
        if self.options.multiplayer_mode.value:
            self.options.ruin_pool_size.value = 0
            self.options.enable_stars.value = 0
            self.options.enable_demigods.value = 0
            self.options.colby_event.value = 0
            self.options.enable_wrath.value = 0

        try:
            player_count = len(self.multiworld.player_ids)
        except Exception:
            player_count = 1
        mc, shop, ruin, dg, star = self._compute_pool_size()
        total = mc + shop + ruin + dg + star
        print(f"[OpenTTD] {player_count} player(s) → {mc} missions + {shop} shop + {ruin} ruins + {dg} demigods + {star} stars = {total} total locations")
        self._generate_missions()

    def _generate_missions(self) -> None:
        """Generate mission content by drawing from predefined pools.

        Each difficulty has a pre-written pool (PREDEFINED_MISSION_POOLS) of
        77-98 missions with well-spaced amounts.  The generator shuffles the pool
        and picks the first N entries, so:
          - No two missions in the same session share the same type+amount.
          - Amounts within the same type are always well-spaced (designed up-front).
          - If more missions are requested than the pool contains, the pool is
            reused from the beginning (shuffle-wrap), still guaranteeing no
            back-to-back duplicates.
          - {cargo} placeholders are filled with a climate-appropriate cargo at
            runtime, so cargo missions still feel varied across sessions.
        """
        rng = self.random
        mission_count, _shop_item_count, _ruin_count, _dg_count, _star_count = self._compute_pool_size()
        missions: List = []

        # Climate-appropriate cargo list — use FIRS cargo if FIRS is enabled
        cargo_list = self._active_cargo_list()

        # ⚠⚠ "Deliver {amount} tons of goods in one year" names Goods in its
        # text and the game measures it against the Goods cargo. Toyland has
        # no Goods (CARGO_BY_LANDSCAPE[3]), and neither do the FIRS Arctic
        # Basic and Steeltown economies — AP_FindCargoType("goods") returns
        # INVALID_CARGO there and the mission can never report progress. Drop
        # those entries instead of handing out a mission nobody can finish.
        has_goods = any(c.lower() == "goods" for c in cargo_list)

        # ⚠⚠ Realistic town ceiling, from BOTH the map size and the town
        # density option. The old formula was min(120, (1 << (bx+by-8)) * 10),
        # which is 120 for EVERY allowed map size — the two smallest terms are
        # already 10240 — and it was applied only to "towns", never to
        # "cities". A 512x512 map at "very low" density holds about 20 towns,
        # so it happily asked for "Service 100 different towns".
        #
        # The engine's own numbers (town_cmd.cpp GetDefaultTownsForMapSize):
        #   num_initial_towns[density] = {5, 11, 23, 46} at 256x256, then
        #   Map::ScaleBySize doubles it for every doubling of the map area.
        # GenerateTowns then scales that by the land proportion and gives up
        # on towns it cannot place, so the real count is always lower — hence
        # the 0.6 margin.
        _INITIAL_TOWNS_AT_256 = (5, 11, 23, 46)   # very low, low, normal, high
        bits_x = self.options.map_size_x.map_bits
        bits_y = self.options.map_size_y.map_bits
        _density = min(max(0, self.options.number_towns.value),
                       len(_INITIAL_TOWNS_AT_256) - 1)
        _scaled_towns = (_INITIAL_TOWNS_AT_256[_density]
                         * (1 << max(0, bits_x + bits_y - 16)))
        max_towns = max(4, int(_scaled_towns * 0.6))

        # Compute per-tier counts with remainder distribution (same as _build_location_table)
        _tier_counts: dict = {}
        for d, f in DIFFICULTY_DISTRIBUTION.items():
            _tier_counts[d] = min(max(1, int(mission_count * f)), MAX_MISSIONS_PER_DIFFICULTY)
        _actual = sum(_tier_counts.values())
        _rem = mission_count - _actual
        for d in DIFFICULTY_DISTRIBUTION:
            if _rem <= 0:
                break
            _room = MAX_MISSIONS_PER_DIFFICULTY - _tier_counts[d]
            _add = min(_rem, _room)
            _tier_counts[d] += _add
            _rem -= _add

        for difficulty, fraction in DIFFICULTY_DISTRIBUTION.items():
            count = _tier_counts[difficulty]
            base_pool = [e for e in PREDEFINED_MISSION_POOLS[difficulty]
                         if has_goods or e[3] != "deliver goods"]
            pool = list(base_pool)

            # Shuffle once to randomise order — gives every session a different
            # subset when count < len(pool), and a different sequence otherwise.
            rng.shuffle(pool)

            # If we need more missions than the pool holds, cycle through the
            # shuffled pool repeatedly (avoiding direct re-use of the same
            # adjacent entry by re-shuffling on each wrap).
            while len(pool) < count:
                extra = list(base_pool)
                rng.shuffle(extra)
                pool.extend(extra)

            generated = []
            cargo_assignments: Dict[str, str] = {}  # type_key -> last cargo used

            for template, amount, unit, type_key in pool[:count]:
                # Skip "Buy from shop" if we'd generate more than 1 per difficulty
                if unit == "purchase" and any(m["unit"] == "purchase" for m in generated):
                    continue

                # Cap town-count missions to what the map can hold. "cities"
                # counts towns too — the game's check is "towns I have a rail
                # station in", not cities in the larger-towns sense.
                if unit in ("towns", "cities") and amount > max_towns:
                    amount = max(2, max_towns)

                # Fill {cargo} placeholder — rotate through available cargos so
                # successive cargo missions use different cargo types where possible.
                if "{cargo}" in template:
                    # Pick a cargo different from the last one used for this type
                    last = cargo_assignments.get(type_key, "")
                    available = [c for c in cargo_list if c != last]
                    if not available:
                        available = cargo_list
                    cargo = rng.choice(available)
                    cargo_assignments[type_key] = cargo
                    description = template.format(amount=f"{amount:,}", cargo=cargo)
                else:
                    cargo = ""
                    description = template.format(amount=f"{amount:,}")

                # Map type_key to C++ effective type for named-destination missions
                effective_type = unit if unit in {
                    "passengers_to_town", "mail_to_town",
                    "cargo_to_industry", "cargo_from_industry",
                } else type_key

                generated.append({
                    "location":    f"Mission_{difficulty.capitalize()}_{len(generated)+1:03d}",
                    "difficulty":  difficulty,
                    "description": description,
                    "type":        effective_type,
                    "amount":      amount,
                    "cargo":       cargo,
                    "unit":        unit,
                })

            missions.extend(generated)

        self._generated_missions = missions

    @staticmethod
    def _round_to_nice(n: int) -> int:
        """Round a number to a 'nice' human-readable value. Never returns 0."""
        if n <= 0:
            return 1
        if n < 100:
            # Small numbers (vehicles, towns, stations): don't round at all,
            # rounding to nearest 100 would produce 0 for any value < 50.
            return n
        elif n < 1_000:
            return max(1, round(n / 100) * 100)
        elif n < 10_000:
            return max(1, round(n / 500) * 500)
        elif n < 100_000:
            return max(1, round(n / 1_000) * 1_000)
        elif n < 1_000_000:
            return max(1, round(n / 10_000) * 10_000)
        else:
            return max(1, round(n / 100_000) * 100_000)

    def create_regions(self) -> None:
        from BaseClasses import Location as APLocation

        class OpenTTDLocation(APLocation):
            game = "OpenTTD"
            _hint_text_override: str = ""

            @property
            def hint_text(self) -> str:
                if self._hint_text_override:
                    return self._hint_text_override
                return super().hint_text

            @hint_text.setter
            def hint_text(self, value: str) -> None:
                self._hint_text_override = value

        loc_table = self._get_location_table()

        # Pre-generate shop prices so we can annotate hint text
        shop_prices = self._generate_shop_prices()

        # Build mission description lookup for hint text
        mission_hints: Dict[str, str] = {}
        for m in self._generated_missions:
            loc  = m.get("location", "")
            desc = m.get("description", "")
            mtyp = m.get("type", "")
            if loc and (desc or mtyp):
                mission_hints[loc] = desc if desc else mtyp

        # Create all regions
        region_names = ["Menu", "mission_easy", "mission_medium",
                        "mission_hard", "mission_extreme", "shop", "ruin", "demigod", "star", "goal"]
        regions: Dict[str, Region] = {}
        for rname in region_names:
            regions[rname] = Region(rname, self.player, self.multiworld)

        # Add locations to regions
        for loc_name, loc_data in loc_table.items():
            region = regions[loc_data.region]
            # Goal_Victory is an event (address=None), Victory item placed directly on it
            address = None if loc_name == "Goal_Victory" else loc_data.code
            location = OpenTTDLocation(self.player, loc_name, address, region)
            location.progress_type = loc_data.progress_type

            # Hint text: shop locations show price, missions show description/type
            if loc_name in shop_prices:
                price = shop_prices[loc_name]
                location.hint_text = f"costs £{price:,}"
            elif loc_name in mission_hints:
                location.hint_text = mission_hints[loc_name]


            region.locations.append(location)

        # Connect Menu → everything
        menu = regions["Menu"]
        for rname, region in regions.items():
            if rname != "Menu":
                menu.connect(region)

        # Add all regions to multiworld
        for region in regions.values():
            self.multiworld.regions.append(region)

    def create_items(self) -> None:
        """Create and place all items.

        Priority order:
        1. Traps  (15% of pool, if enabled)
        2. Utility items (20% of pool)
        3. Vehicles (fill remaining slots — trimmed if needed)

        This ensures traps and utility items always appear even when the
        vehicle pool exceeds the location count.
        """
        loc_table = self._get_location_table()
        # -1 because Goal_Victory is an event location, not a real item slot
        total_locations = len(loc_table) - 1

        enabled_traps = self._get_enabled_traps()

        # ── Determine starting vehicle(s) ────────────────────────────────
        # No restricted "starter pool" — any climate-appropriate vehicle is valid.
        # start_type 0=any, 1=train, 2=road_vehicle, 3=aircraft, 4=ship
        start_type = self.options.starting_vehicle_type.value
        type_names = {1: "train", 2: "road_vehicle", 3: "aircraft", 4: "ship"}

        is_toyland = (self.options.landscape.value == 3)
        landscape = self.options.landscape.value

        # Build climate exclusion set for starting vehicle pool.
        # For Toyland: exclude non-Toyland vehicles (all vehicles NOT in _TOYLAND_ONLY_VEHICLES).
        # For others: exclude Toyland-only vehicles + climate-inappropriate trains.
        climate_exclude: set = set()
        if is_toyland:
            # Toyland: keep only Toyland-specific vehicles
            climate_exclude = set(v for v in ALL_TRAINS + ALL_ROAD_VEHICLES + ALL_AIRCRAFT + ALL_SHIPS
                                  if v not in _TOYLAND_ONLY_VEHICLES)
        else:
            climate_exclude = set(_TOYLAND_ONLY_VEHICLES)
            if landscape == 0:   # Temperate
                climate_exclude |= ARCTIC_TROPIC_ONLY_TRAINS
                climate_exclude |= NON_TEMPERATE_ROAD_VEHICLES
                climate_exclude |= NON_TEMPERATE_WAGONS
            elif landscape == 1:  # Arctic
                climate_exclude |= TEMPERATE_ONLY_TRAINS
                climate_exclude |= NON_ARCTIC_ROAD_VEHICLES
                climate_exclude |= NON_ARCTIC_WAGONS
            elif landscape == 2:  # Tropic
                climate_exclude |= TEMPERATE_ONLY_TRAINS
                climate_exclude |= NON_TROPIC_ROAD_VEHICLES
                climate_exclude |= NON_TROPIC_WAGONS

        # Type-specific pools (engines + wagons for trains, rest vehicle-only)
        # When Iron Horse is enabled, replace vanilla normal-rail engines with IH engines
        # in the starting vehicle pool — vanilla engines don't exist in-game with IH active.
        ih_enabled = bool(self.options.enable_iron_horse.value) and not is_toyland
        if ih_enabled:
            train_pool = [v for v in ALL_TRAINS if v not in climate_exclude and v not in VANILLA_RAIL_ENGINES]
            train_pool += list(IRON_HORSE_ENGINES)
        else:
            train_pool = [v for v in ALL_TRAINS if v not in climate_exclude]

        mil_enabled = bool(self.options.enable_military_items.value) and not is_toyland
        aircraft_pool = [v for v in ALL_AIRCRAFT if v not in climate_exclude]
        if mil_enabled:
            aircraft_pool += list(MILITARY_ITEMS_AIRCRAFT)

        shark_enabled = bool(self.options.enable_shark_ships.value) and not is_toyland
        if shark_enabled:
            ship_pool = list(SHARK_SHIPS)  # SHARK replaces vanilla ships
        else:
            ship_pool = [v for v in ALL_SHIPS if v not in climate_exclude]

        hv_enabled = bool(self.options.enable_hover_vehicles.value) and not is_toyland
        rv_pool = [v for v in ALL_ROAD_VEHICLES if v not in climate_exclude]
        if hv_enabled:
            rv_pool += list(HOVER_VEHICLES)

        heqs_enabled = bool(self.options.enable_heqs.value) and not is_toyland
        if heqs_enabled:
            rv_pool += list(HEQS_ROAD_VEHICLES)
            train_pool += list(HEQS_TRAINS)

        vac_enabled = bool(self.options.enable_vactrain.value) and not is_toyland
        if vac_enabled:
            train_pool += list(VACTRAIN_ENGINES)

        ap25_enabled = bool(self.options.enable_aircraftpack.value) and not is_toyland
        if ap25_enabled:
            aircraft_pool += list(AIRCRAFTPACK_AIRCRAFT)

        type_pools = {
            "train":        train_pool,
            "road_vehicle": rv_pool,
            "aircraft":     aircraft_pool,
            "ship":         ship_pool,
        }

        # ── Filter pools to safe starters only ────────────────────────
        # TRAINS: starting trains MUST run on Normal Rail (railtype 0) with
        # steam or diesel power.  Electric, Monorail, Maglev, Narrow Gauge,
        # Metro, and Vactrain engines are excluded — the player starts with
        # only Normal Rail track and cannot build electrified/special rails.
        _vanilla_safe = set(VANILLA_SAFE_STARTER_TRAINS)
        _ih_safe = set(IRON_HORSE_ENGINES) - IH_NON_STANDARD_ENGINES
        _heqs_safe = set(HEQS_TRAINS)  # HEQS Hi-Rail Truck runs on Normal Rail
        # Vactrain engines are NEVER safe starters (need VACT rail)
        train_pool_start = []
        for v in train_pool:
            if v in _vanilla_safe:
                train_pool_start.append(v)
            elif v.startswith("IH: ") and v in _ih_safe:
                train_pool_start.append(v)
            elif v.startswith("HEQS: ") and v in _heqs_safe:
                train_pool_start.append(v)
            # VAC: engines, monorail, maglev, electric — all excluded
        if not train_pool_start:
            # Fallback: if somehow empty, allow all trains
            train_pool_start = train_pool

        # Aircraft: only small-airport-compatible planes (Small Airport
        # is always available).  Large jets need "Airport: Large" unlock.
        _small_safe = set(SMALL_AIRCRAFT)
        if mil_enabled:
            _small_safe |= SMALL_AIRCRAFT_MIL
        if ap25_enabled:
            _small_safe |= SMALL_AIRCRAFT_AP25
        aircraft_pool_start = [v for v in aircraft_pool if v in _small_safe]
        if not aircraft_pool_start:
            aircraft_pool_start = aircraft_pool

        # Road vehicles: exclude those that carry processed cargo
        rv_pool_start = [v for v in rv_pool
                         if v not in UNSAFE_STARTER_ROAD_VEHICLES]
        if not rv_pool_start:
            rv_pool_start = rv_pool

        # Ships: only passenger ferries / versatile early ships
        if shark_enabled:
            ship_pool_start = [v for v in ship_pool
                               if v in SAFE_STARTER_SHARK]
        else:
            ship_pool_start = [v for v in ship_pool
                               if v in SAFE_STARTER_SHIPS]
        if not ship_pool_start:
            ship_pool_start = ship_pool

        # Use filtered pools for starting vehicle selection only.
        # The original *_pool variables are kept for item creation.
        type_pools_start = {
            "train":        train_pool_start,
            "road_vehicle": rv_pool_start,
            "aircraft":     aircraft_pool_start,
            "ship":         ship_pool_start,
        }

        count = max(1, self.options.starting_vehicle_count.value)

        if start_type == 0:
            # any: combine all safe-starter pools
            chosen_type = "any"
            all_starters: List[str] = []
            for pool in type_pools_start.values():
                all_starters.extend(pool)
        else:
            chosen_type = type_names[start_type]
            all_starters = list(type_pools_start[chosen_type])

        # Deduplicate, shuffle
        seen: set = set()
        unique_starters: List[str] = []
        for v in all_starters:
            if v not in seen:
                seen.add(v)
                unique_starters.append(v)

        if not unique_starters:
            # Fallback: should never happen, but guard anyway
            unique_starters = list(ALL_SHIPS)

        self.random.shuffle(unique_starters)

        # ── Diversity-aware selection for "any" with count > 1 ──────────
        # When the player picks "any" type and wants multiple starters,
        # we try to give them different vehicle types (train, road, aircraft,
        # ship) instead of e.g. two trains. Each subsequent pick has a 50%
        # chance to force a different type than what was already chosen.
        if start_type == 0 and count > 1:
            vehicle_to_type: Dict[str, str] = {}
            for tkey, tpool in type_pools.items():
                for v in tpool:
                    vehicle_to_type[v] = tkey

            remaining = list(unique_starters)
            starting_vehicles: List[str] = []

            # First pick: completely random
            first = remaining.pop(0)
            starting_vehicles.append(first)
            types_used = {vehicle_to_type.get(first, "unknown")}

            for _ in range(min(count, len(remaining) + 1) - 1):
                if not remaining:
                    break
                # 50% chance to force a different vehicle type
                if self.random.random() < 0.5:
                    different = [v for v in remaining
                                 if vehicle_to_type.get(v, "unknown") not in types_used]
                    if different:
                        pick = different[0]  # remaining already shuffled
                        remaining.remove(pick)
                        starting_vehicles.append(pick)
                        types_used.add(vehicle_to_type.get(pick, "unknown"))
                        continue
                # Otherwise: next from shuffled list
                pick = remaining.pop(0)
                starting_vehicles.append(pick)
                types_used.add(vehicle_to_type.get(pick, "unknown"))
        else:
            # Specific type or count == 1: simple slice
            starting_vehicles = unique_starters[:count]

        starting_vehicle = starting_vehicles[0]
        for sv in starting_vehicles:
            self.multiworld.push_precollected(self.create_item(sv))

        self._slot_data["starting_vehicle"] = starting_vehicle
        self._slot_data["starting_vehicle_type"] = chosen_type
        # Extra starters list (C++ client reads this to unlock all starting vehicles)
        self._slot_data["starting_vehicles"] = starting_vehicles

        # ── Track direction guarantee ───────────────────────────────────
        # When EnableRailDirectionUnlocks is active: for each starting train,
        # precollect one random track direction for its rail type so the
        # player can immediately lay track.
        if self.options.enable_rail_direction_unlocks.value:
            seen_railtypes: set = set()
            for sv in starting_vehicles:
                if sv in TRAIN_TO_RAILTYPE:
                    rt = TRAIN_TO_RAILTYPE[sv]
                elif sv.startswith("IH: "):
                    rt = 0  # Safe starters are always Normal Rail IH engines
                else:
                    continue  # Not a train — skip

                if rt not in seen_railtypes:
                    seen_railtypes.add(rt)
                    dir_items = list(TRACK_ITEMS_BY_RAILTYPE[rt])
                    self.random.shuffle(dir_items)
                    self.multiworld.push_precollected(self.create_item(dir_items[0]))

        # ── Wagon guarantee ─────────────────────────────────────────────
        # When wagon unlocks are enabled and ANY starting vehicle is a train,
        # precollect exactly one universal starter wagon (Passenger Carriage
        # or Mail Van — both work on all climates without industry chains).
        # Only 1 wagon total, regardless of how many trains are in the set.
        if self.options.enable_wagon_unlocks.value:
            has_train = any(
                sv in TRAIN_TO_RAILTYPE or sv.startswith("IH: ")
                or sv.startswith("HEQS: ") or sv.startswith("VAC: ")
                for sv in starting_vehicles
            )
            if has_train:
                wagon_choices = list(UNIVERSAL_STARTER_WAGONS)
                self.random.shuffle(wagon_choices)
                self.multiworld.push_precollected(self.create_item(wagon_choices[0]))

        # ── Road direction guarantee ──────────────────────────────────────
        # If road direction unlocks are enabled and a starting vehicle is a road vehicle,
        # precollect one road direction so the player can immediately build roads.
        if self.options.enable_road_direction_unlocks.value:
            _all_rv = set(ALL_ROAD_VEHICLES) | set(HOVER_VEHICLES) | set(HEQS_ROAD_VEHICLES)
            has_road = any(sv in _all_rv for sv in starting_vehicles)
            if has_road:
                road_dirs = list(ROAD_DIRECTION_ITEMS)
                self.random.shuffle(road_dirs)
                self.multiworld.push_precollected(self.create_item(road_dirs[0]))

        # ── Airport guarantee ──────────────────────────────────────────
        # If airport unlocks are enabled and a starting aircraft is NOT
        # small-airport-compatible, precollect "Airport: Large" so the
        # player can actually use it.  (Normally the safe-starter filter
        # prevents this, but the fallback path may still pick a large jet.)
        if self.options.enable_airport_unlocks.value:
            _all_ac = set(ALL_AIRCRAFT) | set(MILITARY_ITEMS_AIRCRAFT) | set(AIRCRAFTPACK_AIRCRAFT)
            _all_small = set(SMALL_AIRCRAFT) | SMALL_AIRCRAFT_MIL | SMALL_AIRCRAFT_AP25
            has_large_aircraft = any(
                sv in _all_ac and sv not in _all_small
                for sv in starting_vehicles
            )
            if has_large_aircraft:
                self.multiworld.push_precollected(self.create_item("Airport: Large"))

        # ── Reserve slots for traps and utility ──────────────────────────
        # Traps: up to 15% of total pool (minimum 0)
        # ── Trap pool — exact count from YAML option ─────────────────────
        trap_target = self.options.trap_count.value
        if trap_target > 0 and enabled_traps:
            trap_pool = (enabled_traps * (trap_target // len(enabled_traps) + 1))[:trap_target]
        else:
            trap_pool = []

        # ── Utility pool — exact count from YAML option ───────────────────
        utility_target = self.options.utility_count.value
        utility_pool: List[str] = []
        while len(utility_pool) < utility_target:
            batch = list(UTILITY_ITEMS)
            self.random.shuffle(batch)
            utility_pool.extend(batch)
        utility_pool = utility_pool[:utility_target]

        speed_boost_count = self.options.speed_boost_count.value
        reserved = len(trap_pool) + len(utility_pool) + speed_boost_count

        # Infrastructure unlock items — added to pool when their toggles are enabled.
        infra_items: List[str] = []
        if self.options.enable_rail_direction_unlocks.value:
            infra_items += list(ALL_TRACK_DIRECTION_ITEMS)  # 24 vanilla
            if ih_enabled:
                infra_items += list(NARROW_GAUGE_TRACK_ITEMS)   # +6 NG
                infra_items += list(METRO_TRACK_ITEMS)           # +6 Metro
            if vac_enabled:
                infra_items += list(VACTUBE_TRACK_ITEMS)         # +6 VacTube
        if self.options.enable_road_direction_unlocks.value:
            infra_items += list(ROAD_DIRECTION_ITEMS)    # 2 roads
            infra_items += list(TRAM_DIRECTION_ITEMS)     # 2 trams
        if self.options.enable_signal_unlocks.value:
            infra_items += list(SIGNAL_ITEMS)
        if self.options.enable_bridge_unlocks.value:
            infra_items += list(BRIDGE_ITEMS)
        if self.options.enable_tunnel_unlocks.value:
            infra_items += list(TUNNEL_ITEMS)
        if self.options.enable_airport_unlocks.value:
            infra_items += list(AIRPORT_ITEMS)
        if self.options.enable_tree_unlocks.value:
            infra_items += list(TREE_ITEMS)
        if self.options.enable_terraform_unlocks.value:
            infra_items += list(TERRAFORM_ITEMS)
        if self.options.enable_town_action_unlocks.value:
            infra_items += list(TOWN_ACTION_ITEMS)
        reserved += len(infra_items)

        # ── Vehicles fill remaining slots ─────────────────────────────────
        # ⭐ The eligible list comes from _eligible_vehicle_names(), the same
        # call _compute_pool_size counts, so the pool and the location count
        # can never drift apart again.
        vehicle_slots = total_locations - reserved
        eligible_vehicles = self._eligible_vehicle_names()

        # NewGRF flags for the client. The vehicles themselves are already in
        # the list above; these tell the game which sets to load.
        # None of them apply on Toyland, which has no GRF variants.
        ih_enabled = bool(self.options.enable_iron_horse.value) and not is_toyland
        self._slot_data["enable_iron_horse"] = 1 if ih_enabled else 0

        mil_enabled = bool(self.options.enable_military_items.value) and not is_toyland
        self._slot_data["enable_military_items"] = 1 if mil_enabled else 0

        shark_enabled = bool(self.options.enable_shark_ships.value) and not is_toyland
        self._slot_data["enable_shark_ships"] = 1 if shark_enabled else 0

        hv_enabled = bool(self.options.enable_hover_vehicles.value) and not is_toyland
        self._slot_data["enable_hover_vehicles"] = 1 if hv_enabled else 0

        heqs_enabled = bool(self.options.enable_heqs.value) and not is_toyland
        self._slot_data["enable_heqs"] = 1 if heqs_enabled else 0

        vac_enabled = bool(self.options.enable_vactrain.value) and not is_toyland
        self._slot_data["enable_vactrain"] = 1 if vac_enabled else 0

        ap25_enabled = bool(self.options.enable_aircraftpack.value) and not is_toyland
        self._slot_data["enable_aircraftpack"] = 1 if ap25_enabled else 0

        # ── FIRS Industries: flag only — no vehicles, just tells C++ to load GRF
        firs_enabled = bool(self.options.enable_firs.value) and not is_toyland
        self._slot_data["enable_firs"] = 1 if firs_enabled else 0

        # Which sets this seed was actually generated against, so the
        # game can refuse to start rather than hand out items for
        # vehicles the player does not have. GRFID, not name: names are
        # translated and files get renamed.
        _grf_requirements = [
            ("enable_iron_horse", "43411223", "Iron Horse", 8948),
            ("enable_firs", "f1250009", "FIRS", 7366),
            ("enable_shark_ships", "4a44bbb1", "SHARK", 1720),
            ("enable_heqs", "41501202", "HEQS", 5199),
            ("enable_military_items", "41440101", "Military items", 12),
            ("enable_vactrain", "444a5901", "Vactrain Set", 80),
            ("enable_aircraftpack", "4c480101", "Aircraftpack 2025", 6),
            ("enable_hover_vehicles", "485a0101", "Hover Vehicles", 0),
        ]
        self._slot_data["required_newgrf"] = [
            {"grfid": grfid, "name": name, "min_version": minver}
            for key, grfid, name, minver in _grf_requirements
            if self._slot_data.get(key)
        ]

        # Wagons are already out of the eligible list when their unlock toggle
        # is off — _eligible_vehicle_names() drops them there.

        # Starting vehicles are REMOVED from the random pool — the player already
        # has them, so they must not appear as items to unlock again.
        _sv_set = set(starting_vehicles)
        eligible_vehicles = [v for v in eligible_vehicles if v not in _sv_set]

        self.random.shuffle(eligible_vehicles)
        vehicle_pool = eligible_vehicles[:vehicle_slots]
        # Store for fill_slot_data (needed to build locked_vehicles list)
        self._vehicle_pool = vehicle_pool
        self._starting_vehicles = starting_vehicles
        # ALL eligible engines (not just pool) — C++ locks every engine in this
        # list.  Engines outside the pool stay permanently locked; only AP-given
        # engines get unlocked.  This prevents "free" engines leaking through.
        self._all_eligible_vehicles = list(eligible_vehicles)

        # ── Assemble pool ─────────────────────────────────────────────────
        items_to_create: List[str] = vehicle_pool + infra_items + utility_pool + trap_pool

        # Add Speed Boost items (each gives +10% FF speed)
        items_to_create += ["Speed Boost"] * speed_boost_count

        # Final pad/trim to exact location count (should be exact already)
        target = total_locations
        if len(items_to_create) < target:
            padding = (UTILITY_ITEMS * 100)[:target - len(items_to_create)]
            items_to_create.extend(padding)
        elif len(items_to_create) > target:
            items_to_create = items_to_create[:target]

        for item_name in items_to_create:
            self.multiworld.itempool.append(self.create_item(item_name))

    def _get_enabled_traps(self) -> List[str]:
        if not self.options.enable_traps.value:
            return []
        traps = []
        trap_map = {
            "Breakdown Wave": self.options.trap_breakdown_wave.value,
            "Recession": self.options.trap_recession.value,
            "Maintenance Surge": self.options.trap_maintenance_surge.value,
            "Signal Failure": self.options.trap_signal_failure.value,
            "Fuel Shortage": self.options.trap_fuel_shortage.value,
            "Bank Loan Forced": self.options.trap_bank_loan.value,
            "Industry Closure": self.options.trap_industry_closure.value,
            "Vehicle License Revoke": self.options.trap_license_revoke.value,
        }
        return [name for name, enabled in trap_map.items() if enabled]

    def create_item(self, name: str) -> OpenTTDItem:
        data = ITEM_TABLE[name]
        return OpenTTDItem(name, data.classification, data.code, self.player)

    def set_rules(self) -> None:
        set_rules(self)
        # Place Victory item directly on the Goal_Victory event location
        goal_location = self.multiworld.get_location("Goal_Victory", self.player)
        goal_location.place_locked_item(self.create_item("Victory"))
        self.multiworld.completion_condition[self.player] = lambda state: \
            state.has("Victory", self.player)


    def pre_fill(self) -> None:
        """Manually distribute ALL progression, trap, and utility items.

        Fill order:
          1. Traps + utility → missions / ruins / demigods (NEVER shop)
          2. Progression items → distributed by fixed percentages:
             - Missions 40% (filled easy→medium→hard→extreme)
             - Shop 40%
             - Ruins 10%  (if enabled, else redistributed)
             - Demigods 10% (if enabled, else redistributed)
          3. AP fill handles remaining useful/filler → all locations freely

        This guarantees:
          - Traps NEVER appear in the shop
          - Progression is evenly split across pools
          - Easy missions get progression first (early finds)
        """
        # ── Step 1: Lock traps + utility into non-shop locations ──────────
        trap_utility_names = frozenset(TRAP_ITEMS) | frozenset(UTILITY_ITEMS)

        trap_utility_items = [item for item in self.multiworld.itempool
                               if item.player == self.player
                               and item.name in trap_utility_names]
        for item in trap_utility_items:
            self.multiworld.itempool.remove(item)

        if trap_utility_items:
            non_shop_locs: list = []
            for rname in ("mission_easy", "mission_medium", "mission_hard",
                          "mission_extreme", "ruin", "demigod", "star"):
                region = self.multiworld.get_region(rname, self.player)
                non_shop_locs.extend(loc for loc in region.locations if not loc.item)

            self.multiworld.random.shuffle(non_shop_locs)
            self.multiworld.random.shuffle(trap_utility_items)

            # ⚠⚠ Traps first, and give back whatever will not fit.
            #
            # Every trap and utility item was taken out of the pool above, but
            # this loop used to place them only "while non_shop_locs" -- so a
            # surplus was silently dropped, and each dropped item left behind
            # a location nothing could ever fill. Generation then died in AP's
            # Fill with "Unable to fill all locations", listing shop slots that
            # were never the cause.
            #
            # It hid for as long as the pools were small: 20 utility + 10 traps
            # against 225 non-shop locations never overflowed. It appears the
            # moment utility_count grows, because the padding create_items adds
            # to reach the location count is ALSO drawn from UTILITY_ITEMS and
            # so also lands in this list. Measured at utility_count=300 with
            # 400 ruins: 730 items for 600 places, 130 dropped, plus the 5
            # starting vehicles held back as precollected = exactly the 135
            # locations Fill reported.
            #
            # Traps go first because they are the ones with a real rule -- they
            # must never end up in the shop. A utility item bought in a shop is
            # just a cash injection you paid for, which is no problem at all.
            trap_names = frozenset(TRAP_ITEMS)
            trap_utility_items.sort(key=lambda it: it.name not in trap_names)

            for item in trap_utility_items:
                if non_shop_locs:
                    loc = non_shop_locs.pop()
                    loc.place_locked_item(item)
                else:
                    # No room left. Back into the pool, never into the bin:
                    # an item and a location have to stay in step.
                    self.multiworld.itempool.append(item)

        # ── Step 2: Distribute progression items by fixed percentages ─────
        # Each pool gets a fixed % of progression items.
        # Missions are split per difficulty tier for granular control.
        _, _, ruin_count, demigod_count, star_count = self._compute_pool_size()
        ruins_on = ruin_count > 0
        demigods_on = demigod_count > 0
        stars_on = star_count > 0

        # Fixed percentages per pool:
        #   Easy 10%, Medium 5%, Hard 5%, Extreme 5% = 25% missions total
        #   Ruins 10%, Demigods 10% = 20% side pools (when all active)
        #   Stars and Shop split the remainder equally
        pct_easy    = 0.10
        pct_medium  = 0.05
        pct_hard    = 0.05
        pct_extreme = 0.05
        pct_ruin    = 0.10 if ruins_on else 0.0
        pct_demigod = 0.10 if demigods_on else 0.0
        remaining   = 1.0 - pct_easy - pct_medium - pct_hard - pct_extreme - pct_ruin - pct_demigod
        if stars_on:
            pct_star = remaining / 2
            pct_shop = remaining / 2
        else:
            pct_star = 0.0
            pct_shop = remaining

        # Extract all OWN progression items from the pool
        prog_items = [item for item in self.multiworld.itempool
                       if item.player == self.player and item.advancement]
        for item in prog_items:
            self.multiworld.itempool.remove(item)
        self.random.shuffle(prog_items)

        # Calculate target counts per pool
        total_prog = len(prog_items)
        easy_target    = round(total_prog * pct_easy)
        medium_target  = round(total_prog * pct_medium)
        hard_target    = round(total_prog * pct_hard)
        extreme_target = round(total_prog * pct_extreme)
        shop_target    = round(total_prog * pct_shop)
        ruin_target    = round(total_prog * pct_ruin)
        star_target    = round(total_prog * pct_star)
        demigod_target = max(0, total_prog - easy_target - medium_target - hard_target
                             - extreme_target - shop_target - ruin_target - star_target)

        # Collect unfilled locations per pool (each tier separately)
        easy_locs = [loc for loc in self.multiworld.get_region("mission_easy", self.player).locations if not loc.item]
        self.random.shuffle(easy_locs)
        medium_locs = [loc for loc in self.multiworld.get_region("mission_medium", self.player).locations if not loc.item]
        self.random.shuffle(medium_locs)
        hard_locs = [loc for loc in self.multiworld.get_region("mission_hard", self.player).locations if not loc.item]
        self.random.shuffle(hard_locs)
        extreme_locs = [loc for loc in self.multiworld.get_region("mission_extreme", self.player).locations if not loc.item]
        self.random.shuffle(extreme_locs)

        shop_locs = [loc for loc in self.multiworld.get_region("shop", self.player).locations if not loc.item]
        self.random.shuffle(shop_locs)

        ruin_locs = [loc for loc in self.multiworld.get_region("ruin", self.player).locations if not loc.item]
        self.random.shuffle(ruin_locs)

        demigod_locs = [loc for loc in self.multiworld.get_region("demigod", self.player).locations if not loc.item]
        self.random.shuffle(demigod_locs)

        star_locs = [loc for loc in self.multiworld.get_region("star", self.player).locations if not loc.item]
        self.random.shuffle(star_locs)

        # ⚠⚠ AN INFRASTRUCTURE ITEM MUST NEVER SIT BEHIND THE TIER IT OPENS.
        #
        # place_locked_item bypasses the logic completely, so nothing catches a
        # circular placement. The Extreme rule requires terraform; put the
        # terraform unlock in an Extreme mission and every Extreme mission is
        # unreachable, and with accessibility: full the generator dies --
        # listing a dozen Extreme locations and some ordinary vehicles it could
        # not place, which points nowhere near the cause.
        #
        # MEASURED: wagons-off-arctic at seed 1 failed exactly this way. The
        # same case generated with enable_terraform_unlocks off, and generated
        # again unchanged at seed 7. A lottery, not a property of arctic --
        # every configuration with tier infrastructure could draw it.
        #
        # Easy missions, the shop, ruins, stars and demigods gate on vehicles
        # or cargo and never on infrastructure, so they are safe homes.
        _TIER_GATING_INFRA = (frozenset(ALL_TRACK_DIRECTION_ITEMS)
                              | frozenset(ROAD_DIRECTION_ITEMS)
                              | frozenset(TRAM_DIRECTION_ITEMS)
                              | frozenset(AIRPORT_ITEMS)
                              | frozenset(BRIDGE_ITEMS)
                              | frozenset(TUNNEL_ITEMS)
                              | frozenset(TERRAFORM_ITEMS))

        gating_q = [i for i in prog_items if i.name in _TIER_GATING_INFRA]
        other_q  = [i for i in prog_items if i.name not in _TIER_GATING_INFRA]

        # (target, locations, safe for tier-gating infrastructure)
        pools = [
            (easy_target,    easy_locs,    True),
            (medium_target,  medium_locs,  False),
            (hard_target,    hard_locs,    False),
            (extreme_target, extreme_locs, False),
            (shop_target,    shop_locs,    True),
            (star_target,    star_locs,    True),
            (ruin_target,    ruin_locs,    True),
            (demigod_target, demigod_locs, True),
        ]

        # Pass 1 — the gating infrastructure, into infra-free pools only.
        gi = 0
        for _target, locs, safe in pools:
            if not safe:
                continue
            for loc in locs:
                if gi >= len(gating_q):
                    break
                if loc.item:
                    continue
                loc.place_locked_item(gating_q[gi])
                gi += 1
            if gi >= len(gating_q):
                break

        # Pass 2 — everything else, by the percentage targets.
        idx = 0
        for target, locs, _safe in pools:
            placed = 0
            for loc in locs:
                if placed >= target or idx >= len(other_q):
                    break
                if loc.item:
                    continue
                loc.place_locked_item(other_q[idx])
                idx += 1
                placed += 1

        # Any remaining progression items (overflow) → place in any unfilled location
        if idx < len(other_q):
            all_remaining = []
            for _t, pool_locs, _s in pools:
                all_remaining.extend(loc for loc in pool_locs if not loc.item)
            for loc in all_remaining:
                if idx >= len(other_q):
                    break
                loc.place_locked_item(other_q[idx])
                idx += 1

        # Gating items with no safe home left go back to AP's own fill, which
        # places them with the logic in hand. Never into an unsafe pool here.
        for leftover in gating_q[gi:]:
            self.multiworld.itempool.append(leftover)

        # ⚠ Same rule as the trap/utility half above: an item that finds no
        # location goes BACK in the pool, never on the floor. This branch
        # should be unreachable -- progression items were drawn out of a pool
        # that is already location-sized -- but the identical shape one screen
        # up was also "unreachable" until utility_count grew, and it cost a
        # whole afternoon to trace from the error AP reports, which names shop
        # slots that had nothing to do with the cause.
        for leftover in other_q[idx:]:
            self.multiworld.itempool.append(leftover)

    def fill_slot_data(self) -> Dict[str, Any]:
        """Data sent to the game client via the bridge."""
        # Win difficulty presets: (company_value, town_population, vehicle_count, cargo_tons, monthly_profit, missions)
        WIN_PRESETS = {
            0:  (1_500_000,    150_000,    40,   15_000_000,   1_000_000,   20),   # Casual
            1:  (5_000_000,    200_000,    80,   30_000_000,   3_000_000,   40),   # Easy
            2:  (10_000_000,   300_000,   120,   45_000_000,  10_000_000,   50),   # Normal
            3:  (20_000_000,   500_000,   160,   60_000_000,  20_000_000,   60),   # Medium
            4:  (30_000_000,   700_000,   200,   75_000_000,  25_000_000,   70),   # Hard
            5:  (40_000_000,   900_000,   240,   90_000_000,  30_000_000,   80),   # Very Hard
            6:  (50_000_000, 1_100_000,   280,  105_000_000,  35_000_000,   80),   # Extreme
            7:  (60_000_000, 1_300_000,   320,  120_000_000,  40_000_000,   80),   # Insane
            8:  (70_000_000, 1_500_000,   360,  135_000_000,  45_000_000,   80),   # Nutcase
            9:  (80_000_000, 1_700_000,   400,  150_000_000,  50_000_000,   80),   # Madness
            10: None,  # Custom — use sliders
        }

        diff = self.options.win_difficulty.value
        if diff == 10:  # Custom
            preset = (
                self.options.win_custom_company_value.value,
                self.options.win_custom_town_population.value,
                self.options.win_custom_vehicle_count.value,
                self.options.win_custom_cargo_delivered.value,
                self.options.win_custom_monthly_profit.value,
                self.options.win_custom_missions_completed.value,
            )
        else:
            preset = WIN_PRESETS[diff]
        (win_cv, win_pop, win_veh, win_cargo, win_profit, win_missions) = preset

        computed_mc, computed_shop, _computed_ruin, computed_dg, _computed_star = self._compute_pool_size()

        # ⚠⚠ The win target cannot ask for more missions than the seed has.
        # The presets want 20-80 and a custom target may ask for 500, but the
        # mission count follows the item pool: Toyland with trap_count 0,
        # utility_count 5 and no ruins produces 46 missions, so "complete 70"
        # is a goal nobody can reach and the seed can never be finished.
        win_missions = min(win_missions, computed_mc)

        # Build item_id_to_name so the C++ client can resolve item IDs to names
        item_id_to_name = {str(data.code): name for name, data in ITEM_TABLE.items()}

        # locked_vehicles: every vehicle the C++ engine-locking system should lock.
        # ALL eligible engines are locked (not just the AP pool).  Engines not in
        # the AP item pool will stay permanently locked — the player only gets
        # engines that AP gives them.  Starting vehicles are included so they
        # start locked then get immediately unlocked by precollected items.
        locked_vehicle_set: set = set(getattr(self, "_all_eligible_vehicles", [])) | set(getattr(self, "_starting_vehicles", []))
        locked_vehicles_list = sorted(locked_vehicle_set)  # deterministic order

        self._slot_data.update({
            "game_version": "15.2",
            "mission_count": computed_mc,
            "shop_item_count": computed_shop,

            "missions": self._generated_missions,
            "tier_unlock_requirements": {
                "medium":  self.options.mission_tier_unlock_count.value,
                "hard":    self.options.mission_tier_unlock_count.value,
                "extreme": self.options.mission_tier_unlock_count.value,
            },
            "win_target_company_value":   win_cv,
            "win_target_town_population": win_pop,
            "win_target_vehicle_count":   win_veh,
            "win_target_cargo_delivered": win_cargo,
            "win_target_monthly_profit":  win_profit,
            "win_target_missions":        win_missions,
            "win_difficulty":             diff,
            "enable_traps": bool(self.options.enable_traps.value),
            "start_year": self.options.start_year.value,
            "world_seed": 0,
            "map_x": self.options.map_size_x.map_bits,
            "map_y": self.options.map_size_y.map_bits,
            "landscape": self.options.landscape.value,
            "land_generator": self.options.land_generator.value,
            "terrain_type": self.options.terrain_type.value,
            "sea_level": self.options.sea_level.value,
            "rivers": self.options.rivers.value,
            "smoothness": self.options.smoothness.value,
            "variety": self.options.variety.value,
            "number_towns": self.options.number_towns.value,
            "town_name": self.options.town_names.value,
            "item_id_to_name": item_id_to_name,
            "locked_vehicles": locked_vehicles_list,
            "shop_prices": self._generate_shop_prices(),
            # Shop labels. A slot may legitimately be missing here — AP's free
            # fill can put a utility item on one, and the guard below keeps
            # those (and traps) out. The client handles a missing key: it falls
            # back to the LocationScouts hint and then to "Slot #N"
            # (AP_GetShopLocationLabel), so no slot is ever left blank.
            "shop_item_names": {
                loc: self.multiworld.get_location(loc, self.player).item.name
                for loc in self._get_location_table()
                if loc.startswith("Shop_Purchase_")
                and self.multiworld.get_location(loc, self.player).item is not None
                # Safety guard: traps and utility items must never show in shop
                and self.multiworld.get_location(loc, self.player).item.name
                    not in (frozenset(TRAP_ITEMS) | frozenset(UTILITY_ITEMS))
            },
            # ── Game settings: Accounting ──────────────────────────
            "infinite_money":             bool(self.options.infinite_money.value),
            "inflation":                  bool(self.options.inflation.value),
            "max_loan":                   self.options.max_loan.value,
            "infrastructure_maintenance": bool(self.options.infrastructure_maintenance.value),
            "vehicle_costs":              self.options.vehicle_costs.value,
            "construction_cost":          self.options.construction_cost.value,
            # ── Game settings: Vehicle Limits ──────────────────────
            "max_trains":                 self.options.max_trains.value,
            "max_roadveh":                self.options.max_roadveh.value,
            "max_aircraft":               self.options.max_aircraft.value,
            "max_ships":                  self.options.max_ships.value,
            "max_train_length":           self.options.max_train_length.value,
            "station_spread":             self.options.station_spread.value,
            "road_stop_on_town_road":     bool(self.options.road_stop_on_town_road.value),
            "road_stop_on_competitor_road": bool(self.options.road_stop_on_competitor_road.value),
            "crossing_with_competitor":   bool(self.options.crossing_with_competitor.value),
            # ── Game settings: Disasters / Accidents ───────────────
            "disasters":                  bool(self.options.disasters.value),
            "plane_crashes":              self.options.plane_crashes.value,
            "vehicle_breakdowns":         self.options.vehicle_breakdowns.value,
            # ── Game settings: Economy / Environment ───────────────
            "economy_type":               self.options.economy_type.value,
            "bribe":                      bool(self.options.bribe.value),
            "exclusive_rights":           bool(self.options.exclusive_rights.value),
            "fund_buildings":             bool(self.options.fund_buildings.value),
            "fund_roads":                 bool(self.options.fund_roads.value),
            "give_money":                 bool(self.options.give_money.value),
            "town_growth_rate":           self.options.town_growth_rate.value,
            "found_town":                 self.options.found_town.value,
            "town_cargo_scale":           self.options.town_cargo_scale.value,
            "industry_cargo_scale":       self.options.industry_cargo_scale.value,
            "industry_density":           self.options.industry_density.value,
            "allow_town_roads":           bool(self.options.allow_town_roads.value),
            "road_side":                  self.options.road_side.value,
            "firs_economy":               self.options.firs_economy.value,
            # ── Wagon pool mode (backward compat: 0=all wagons, 1=no wagons)
            # ── Item Pool unlocks ──────────────────────────────────
            "enable_rail_direction_unlocks": bool(self.options.enable_rail_direction_unlocks.value),
            "enable_road_direction_unlocks": bool(self.options.enable_road_direction_unlocks.value),
            "enable_signal_unlocks":         bool(self.options.enable_signal_unlocks.value),
            "enable_bridge_unlocks":         bool(self.options.enable_bridge_unlocks.value),
            "enable_tunnel_unlocks":         bool(self.options.enable_tunnel_unlocks.value),
            "enable_airport_unlocks":        bool(self.options.enable_airport_unlocks.value),
            "enable_tree_unlocks":           bool(self.options.enable_tree_unlocks.value),
            "enable_terraform_unlocks":      bool(self.options.enable_terraform_unlocks.value),
            "enable_town_action_unlocks":    bool(self.options.enable_town_action_unlocks.value),
            "enable_wagon_unlocks":          bool(self.options.enable_wagon_unlocks.value),
            # ── Ruins ─────────────────────────────────────────────
            "ruin_pool_size":                self.options.ruin_pool_size.value,
            "max_active_ruins":              self.options.max_active_ruins.value,
            "ruin_cargo_types_min":          self.options.ruin_cargo_types_min.value,
            "ruin_cargo_types_max":          max(self.options.ruin_cargo_types_min.value, self.options.ruin_cargo_types_max.value),
            "ruin_locations":                [f"Ruin_{i:03d}" for i in range(1, self.options.ruin_pool_size.value + 1)],
            # ── Stars ──────────────────────────────────────────────
            "enable_stars":                  bool(self.options.enable_stars.value),
            "star_pool_size":                self._compute_pool_size()[4],
            "star_locations":                [f"Star_{i:03d}" for i in range(1, self._compute_pool_size()[4] + 1)],
            # ── DeathLink ──────────────────────────────────────────
            "death_link":                 bool(self.options.death_link.value),
            # ── Funny Stuff ──────────────────────────────────────────
            "community_vehicle_names": bool(self.options.community_vehicle_names.value),
            # ── Events ───────────────────────────────────────────────
            "colby_event":        bool(self.options.colby_event.value),
            # ⚠ MAX_YEAR is 5,000,000 (timer_game_common.h) and StartYear may
            # be set to exactly that, so +2 has to be clamped.
            "colby_start_year":   min(self.options.start_year.value + 2,
                                      self._MAX_GAME_YEAR),
            "colby_town_seed":    (self.multiworld.seed ^ self.player) & 0xFFFFFFFF,
            # Colby cargo: the engine prefers its own CLBY cargo slot and only
            # falls back to this name, so it has to name a cargo the active
            # landscape and FIRS economy really have.
            "colby_cargo":        self._colby_cargo_name(),
            # ── Demigods (God of Wackens) ────────────────────────────
            "demigod_enabled":            bool(self.options.enable_demigods.value),
            "demigod_count":              computed_dg,
            "demigod_spawn_interval_min": self.options.demigod_spawn_interval_min.value,
            "demigod_spawn_interval_max": max(self.options.demigod_spawn_interval_min.value,
                                              self.options.demigod_spawn_interval_max.value),
            "demigods":                   self._generate_demigod_defs(computed_dg),
            # ── God of Wackens (Wrath) ─────────────────────────────
            "wrath_enabled":              bool(self.options.enable_wrath.value),
            "wrath_limit_houses":         self.options.wrath_limit_houses.value,
            "wrath_limit_roads":          self.options.wrath_limit_roads.value,
            "wrath_limit_terrain":        self.options.wrath_limit_terrain.value,
            "wrath_limit_trees":          self.options.wrath_limit_trees.value,
            # ── Multiplayer ────────────────────────────────────────
            "multiplayer_mode":           bool(self.options.multiplayer_mode.value),
            # ── Difficulty / balance ───────────────────────────────
            "starting_cash_bonus":        self.options.starting_cash_bonus.value,
            "starting_vehicle_count":     self.options.starting_vehicle_count.value,
            "mission_difficulty":         self.options.mission_difficulty.value,
        })
        return self._slot_data

    # Price ranges per tier: (min, max) in pounds — must match ShopPriceTier options
    SHOP_PRICE_RANGES = {
        0: (    10_000,     500_000),   # Tier 1: £10K – £500K
        1: (    50_000,   1_000_000),   # Tier 2: £50K – £1M
        2: (   100_000,   5_000_000),   # Tier 3: £100K – £5M
        3: (   500_000,  15_000_000),   # Tier 4: £500K – £15M
        4: ( 1_000_000,  50_000_000),   # Tier 5: £1M – £50M
        5: ( 5_000_000, 150_000_000),   # Tier 6: £5M – £150M
        6: (10_000_000, 500_000_000),   # Tier 7: £10M – £500M
    }

    def _generate_shop_prices(self) -> Dict[str, int]:
        """Assign a random price to every shop location using the shop_price_tier setting."""
        if self._shop_prices_cache:
            return self._shop_prices_cache

        tier = self.options.shop_price_tier.value
        price_min, price_max = self.SHOP_PRICE_RANGES[tier]

        rng = self.random
        _mc, computed_shop, _ruin, _dg, _star = self._compute_pool_size()
        shop_total = computed_shop
        # Generate all prices randomly, then sort ascending so the shop
        # naturally shows affordable items first and expensive ones last.
        # This means the first shop rotation is always cheapest, later ones
        # progressively more expensive — a natural difficulty ramp.
        import math as _math
        log_min = _math.log10(max(1, price_min))
        log_max = _math.log10(max(price_min + 1, price_max))
        raw_prices = [
            self._round_to_nice(int(10 ** rng.uniform(log_min, log_max)))
            for _ in range(shop_total)
        ]
        raw_prices.sort()  # cheapest → most expensive

        prices: Dict[str, int] = {}
        for i, price in enumerate(raw_prices, start=1):
            loc = f"Shop_Purchase_{i:04d}"
            prices[loc] = price

        self._shop_prices_cache = prices
        return prices

    # ── Demigod definition generator ─────────────────────────────────────
    # Thematic names and presidents for the God of Wackens system.
    _DEMIGOD_POOL = [
        # (company_name, president_name, theme)
        ("The Iron Serpent",       "Lord Railsworth",       "trains"),
        ("Asphalt Dominion",      "Baron Roadkill",        "road"),
        ("Sky Tyrant Airways",    "Duchess Jetstream",     "aircraft"),
        ("Abyssal Fleet",         "Captain Deepwater",     "ships"),
        ("Steel Thunder Corp",    "General Irontrack",     "trains"),
        ("Midnight Express Co",   "Countess Locomotive",   "trains"),
        ("Fury Road Logistics",   "Mad Trucker McGee",     "road"),
        ("Highway Marauders",     "Rex Roadrage",          "road"),
        ("Cloud Conquerors",      "Admiral Skyfall",       "aircraft"),
        ("Tempest Airlines",      "Wing Commander Storm",  "aircraft"),
        ("Kraken Shipping Co",    "Old Man Tidecrusher",   "ships"),
        ("Poseidon Transport",    "The Harbourmaster",     "ships"),
        ("Hellfire Rail Corp",    "Infernal Engineer",     "trains"),
        ("Thunderbolt Transit",   "Professor Voltage",     "road"),
        ("Phantom Freight Inc",   "The Ghost Conductor",   "trains"),
        ("Stormchaser Cargo",     "Navigator Gale",        "ships"),
        ("Valkyrie Air Express",  "Commander Valkyria",    "aircraft"),
        ("Doomtrack Railways",    "The Rail Reaper",       "trains"),
        ("Warlord Wheels Ltd",    "Generalissimo Diesel",  "road"),
        ("Leviathan Liners",     "Commodore Whalebone",   "ships"),
    ]

    def _generate_demigod_defs(self, count: int) -> list:
        """Generate demigod definitions for slot_data.

        Returns a list of dicts, each with: location, name, president, theme, tribute_cost.
        The C++ side reads these to spawn themed AI competitors the player must pay tribute to defeat.
        """
        if count <= 0:
            return []

        rng = self.random
        pool = list(self._DEMIGOD_POOL)
        rng.shuffle(pool)

        # If more demigods requested than pool size, cycle through
        while len(pool) < count:
            extra = list(self._DEMIGOD_POOL)
            rng.shuffle(extra)
            pool.extend(extra)

        # Tribute costs scale up with each demigod (later ones are harder)
        base_tribute = 250_000
        tribute_step = 150_000

        defs = []
        for i in range(count):
            company_name, president, theme = pool[i]
            tribute = base_tribute + (i * tribute_step)
            # Add some randomness to tribute cost (±30%)
            tribute = int(tribute * rng.uniform(0.7, 1.3))
            # Round to nearest 10k
            tribute = max(50_000, (tribute // 10_000) * 10_000)

            defs.append({
                "location":     f"Demigod_{i + 1:03d}",
                "name":         company_name,
                "president":    president,
                "theme":        theme,
                "tribute_cost": tribute,
            })

        return defs

    def generate_output(self, output_directory: str) -> None:
        """Nothing extra to generate — all config goes via slot_data."""
        pass

    def get_filler_item_name(self) -> str:
        return self.random.choice(UTILITY_ITEMS)
