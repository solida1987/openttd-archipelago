"""
OpenTTD Archipelago World
Version: 1.0.0
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
    VANILLA_TRAINS, VANILLA_WAGONS, VANILLA_ROAD_VEHICLES,
    VANILLA_AIRCRAFT, VANILLA_SHIPS, STARTING_VEHICLES, OpenTTDItemData
)
from .locations import (
    get_location_table, DIFFICULTY_DISTRIBUTION,
    MISSION_TEMPLATES, CARGO_TYPES
)
from .options import OpenTTDOptions
from .rules import set_rules


class OpenTTDWeb(WebWorld):
    theme = "ocean"
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
    # Pre-build with max possible config so AP can read locations at class level
    location_name_to_id: Dict[str, int] = {
        name: data.code
        for name, data in get_location_table(mission_count=300, shop_slots=10).items()
    }

    # Slot data stored during generation
    _slot_data: Dict[str, Any] = {}
    _generated_missions: List[Dict] = []

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._generated_missions = []
        self._slot_data = {}

    def _get_location_table(self):
        mc, ss = self._compute_pool_size()
        return get_location_table(mc, ss)

    def _compute_pool_size(self) -> tuple:
        """Dynamically compute (mission_count, shop_slots) based on player count.

        Goals:
        - Solo:    all 202 vehicles + good traps/utility always fit
        - 8 players: each world contributes ~600 checks to the shared pool
        - Scales smoothly; player YAML choices act as a ±multiplier on top

        Formula:
            base_total  = 350  (enough for all vehicles + traps + utility solo)
            per_player  = +50 per additional player beyond 1
            cap         = 1 200 (avoids absurdly long sessions)

        Player YAML option 'mission_count' is reinterpreted as a scale factor:
            default (300) → ×1.0
            higher        → proportionally more locations
            lower         → proportionally fewer (min 250 total)

        Split: 65% missions, 35% shop locations (rounded to shop_slots of 20)
        """
        # How many players are in this multiworld?
        try:
            player_count = len(self.multiworld.player_ids)
        except Exception:
            player_count = 1

        # Base total locations
        base_total = 350 + (player_count - 1) * 50
        base_total = min(base_total, 1200)

        # Apply player's mission_count as a relative scale
        # The option default is 300; treat that as ×1.0
        yaml_mc = self.options.mission_count.value
        scale = yaml_mc / 300.0
        total = max(250, int(base_total * scale))

        # Split into missions (65%) and shop locations (35%)
        mission_count = int(total * 0.65)
        shop_loc_count = total - mission_count
        shop_slots = max(3, shop_loc_count // 20)  # each shop_slot = 20 locations

        return mission_count, shop_slots

    def generate_early(self) -> None:
        """Generate mission content before items are placed."""
        try:
            player_count = len(self.multiworld.player_ids)
        except Exception:
            player_count = 1
        mc, ss = self._compute_pool_size()
        total = mc + ss * 20
        print(f"[OpenTTD] {player_count} player(s) → {mc} missions + {ss} shop slots = {total} total locations")
        self._generate_missions()

    def _generate_missions(self) -> None:
        """Generate random mission content for each mission location.

        Rules enforced:
        1. No exact duplicate (type + amount) within a difficulty level.
        2. For numeric missions of the same type: each successive amount must
           be at least 5× the previous one. This prevents "have 3 vehicles /
           have 4 vehicles / have 5 vehicles" chains.
        3. "Buy a vehicle from the shop" appears at most once per difficulty.
        """
        rng = self.random
        mission_count, _shop_slots = self._compute_pool_size()
        missions = []

        # Minimum multiplier between two missions of the same type
        MIN_SPACING_FACTOR = 5

        for difficulty, fraction in DIFFICULTY_DISTRIBUTION.items():
            count = max(1, int(mission_count * fraction))
            templates = MISSION_TEMPLATES[difficulty]

            # Track per-type: set of used amounts, and the current max amount
            used: Dict[str, set] = {}          # type_key -> set of amounts
            type_max: Dict[str, int] = {}       # type_key -> highest amount used so far
            shop_used = False                   # cap "Buy a vehicle from the shop" at 1

            generated = []
            max_attempts = count * 50           # safety cap on retries

            attempts = 0
            while len(generated) < count and attempts < max_attempts:
                attempts += 1

                template_data = rng.choice(templates)
                template, amt_min, amt_max, unit = template_data

                # Extract a stable type key from the template text
                # Normalize type key: strip amount placeholder, lowercase, replace £ with ascii
                # so C++ type matching (which also normalizes) can find it reliably.
                raw_key = template.split("{amount}")[0].strip().lower()
                type_key = raw_key.replace("\u00a3", "").replace("\xc2\xa3", "").replace("£", "").strip().rstrip(",").rstrip()
                # map "earn  total profit" -> "earn", "earn  in one month" -> "earn monthly" etc.
                if type_key.startswith("earn") and "month" in template.lower():
                    type_key = "earn monthly"
                elif type_key.startswith("earn"):
                    type_key = "earn"

                # "Buy a vehicle from the shop" — only once per difficulty
                if unit == "purchase":
                    if shop_used:
                        continue
                    shop_used = True
                    amount = 1
                else:
                    # Determine the minimum allowed amount for this slot
                    floor = amt_min
                    if type_key in type_max:
                        floor = max(floor, type_max[type_key] * MIN_SPACING_FACTOR)

                    # If the floor already exceeds the template max, skip this
                    # template entirely for this difficulty (pool exhausted)
                    if floor > amt_max:
                        continue

                    amount = rng.randint(floor, amt_max)
                    amount = self._round_to_nice(amount)
                    amount = max(floor, amount)  # rounding must not go below floor

                    # Deduplicate: if this exact amount was already used, skip
                    if type_key in used and amount in used[type_key]:
                        continue

                cargo = rng.choice(CARGO_TYPES) if "{cargo}" in template else ""
                description = (
                    template.format(amount=f"{amount:,}", cargo=cargo)
                    if cargo else
                    template.format(amount=f"{amount:,}")
                )

                # Register this mission
                if type_key not in used:
                    used[type_key] = set()
                used[type_key].add(amount)
                if unit != "purchase":
                    type_max[type_key] = max(type_max.get(type_key, 0), amount)

                generated.append({
                    "location":    f"Mission_{difficulty.capitalize()}_{len(generated)+1:03d}",
                    "difficulty":  difficulty,
                    "description": description,
                    "type":        type_key,
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

        loc_table = self._get_location_table()

        # Create all regions
        region_names = ["Menu", "mission_easy", "mission_medium",
                        "mission_hard", "mission_extreme", "shop", "goal"]
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

        # ── Determine starting vehicle ────────────────────────────────────
        start_type = self.options.starting_vehicle_type.value
        type_names = {1: "train", 2: "road_vehicle", 3: "aircraft", 4: "ship"}
        if start_type == 0:
            chosen_type = self.random.choice(list(type_names.values()))
        else:
            chosen_type = type_names[start_type]

        starting_pool = STARTING_VEHICLES[chosen_type]
        starting_vehicle = self.random.choice(starting_pool)
        self._slot_data["starting_vehicle"] = starting_vehicle
        self._slot_data["starting_vehicle_type"] = chosen_type
        self.multiworld.push_precollected(self.create_item(starting_vehicle))

        # ── Reserve slots for traps and utility ──────────────────────────
        # Traps: up to 15% of total pool (minimum 0)
        # Utility: up to 20% of total pool (minimum 10)
        if enabled_traps:
            trap_target = max(len(enabled_traps), int(total_locations * 0.15))
            trap_pool = (enabled_traps * 20)[:trap_target]
        else:
            trap_pool = []

        utility_target = max(10, int(total_locations * 0.20))
        utility_pool = (UTILITY_ITEMS * 20)[:utility_target]

        reserved = len(trap_pool) + len(utility_pool)

        # ── Vehicles fill remaining slots ─────────────────────────────────
        vehicle_slots = total_locations - reserved
        all_vehicles_shuffled = [v for v in ALL_VEHICLES if v != starting_vehicle]
        # Shuffle so trimming removes random vehicles rather than always the last ones
        self.random.shuffle(all_vehicles_shuffled)
        vehicle_pool = all_vehicles_shuffled[:vehicle_slots]

        # ── Assemble pool ─────────────────────────────────────────────────
        items_to_create: List[str] = vehicle_pool + utility_pool + trap_pool

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

    def fill_slot_data(self) -> Dict[str, Any]:
        """Data sent to the game client via the bridge."""
        win_cond = self.options.win_condition.value
        win_values = {
            0: self.options.win_condition_company_value.value,
            1: self.options.win_condition_town_population.value,
            2: self.options.win_condition_vehicle_count.value,
            3: self.options.win_condition_cargo_delivered.value,
            4: self.options.win_condition_monthly_profit.value,
        }
        win_condition_names = {
            0: "company_value",
            1: "town_population",
            2: "vehicle_count",
            3: "cargo_delivered",
            4: "monthly_profit",
        }

        computed_mc, computed_ss = self._compute_pool_size()

        # Build item_id_to_name so the C++ client can resolve item IDs to names
        item_id_to_name = {str(data.code): name for name, data in ITEM_TABLE.items()}

        self._slot_data.update({
            "game_version": "15.2",
            "mission_count": computed_mc,
            "shop_slots": computed_ss,
            "shop_refresh_days": self.options.shop_refresh_days.value,
            "missions": self._generated_missions,
            "win_condition": win_condition_names[win_cond],
            "win_condition_value": win_values[win_cond],
            "enable_traps": bool(self.options.enable_traps.value),
            "start_year": self.options.start_year.value,
            "world_seed": 0,
            "map_x": self.options.map_size_x.map_bits,
            "map_y": self.options.map_size_y.map_bits,
            "landscape": self.options.landscape.value,
            "land_generator": self.options.land_generator.value,
            "item_id_to_name": item_id_to_name,
            "shop_prices": self._generate_shop_prices(),
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
        })
        return self._slot_data

    # Price ranges per tier: (min, max) in pounds
    SHOP_PRICE_RANGES = {
        0: (10_000,      500_000),      # easy
        1: (100_000,   5_000_000),      # normal
        2: (1_000_000, 50_000_000),     # hard
        3: (10_000_000, 300_000_000),   # extreme
    }

    def _generate_shop_prices(self) -> Dict[str, int]:
        """Assign a random price to every shop location based on the chosen tier."""
        tier = self.options.shop_price_tier.value
        price_min, price_max = self.SHOP_PRICE_RANGES[tier]
        rng = self.random
        _mc, computed_ss = self._compute_pool_size()
        shop_total = computed_ss * 20
        prices: Dict[str, int] = {}
        for i in range(1, shop_total + 1):
            loc = f"Shop_Purchase_{i:04d}"
            raw = rng.randint(price_min, price_max)
            # Round to a "nice" number
            prices[loc] = self._round_to_nice(raw)
        return prices

    def generate_output(self, output_directory: str) -> None:
        """Nothing extra to generate — all config goes via slot_data."""
        pass

    def get_filler_item_name(self) -> str:
        return self.random.choice(UTILITY_ITEMS)
