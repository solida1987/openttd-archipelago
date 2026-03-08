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
    VANILLA_AIRCRAFT, VANILLA_SHIPS, STARTING_VEHICLES, IRON_HORSE_ENGINES,
    ARCTIC_TROPIC_ONLY_TRAINS, TEMPERATE_ONLY_TRAINS,
    OpenTTDItemData
)
from .locations import (
    get_location_table, DIFFICULTY_DISTRIBUTION,
    MISSION_TEMPLATES, CARGO_TYPES, CARGO_BY_LANDSCAPE
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
    "Ploddyphut Diesel",    "Powernaut Diesel",
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
    # Pre-build with max possible config so AP can read locations at class level
    location_name_to_id: Dict[str, int] = {
        name: data.code
        for name, data in get_location_table(mission_count=600, shop_item_count=600).items()
    }

    # Slot data stored during generation
    _slot_data: Dict[str, Any] = {}
    _generated_missions: List[Dict] = []

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._generated_missions = []
        self._slot_data = {}
        self._shop_prices_cache: Dict[str, int] = {}

    def _get_location_table(self):
        mc, shop = self._compute_pool_size()
        return get_location_table(mc, shop)

    def _compute_pool_size(self) -> tuple:
        """Dynamically compute (mission_count, shop_item_count).

        Pool size is derived automatically from:
          - Available vehicles for the chosen landscape + active GRFs
          - trap_count  (explicit YAML option)
          - utility_count  (explicit YAML option)

        Total items = vehicles + traps + utility.
        Split exactly 50/50: mission_count = shop_item_count = total // 2.
        The player never controls mission_count or shop size directly.
        """
        landscape = self.options.landscape.value
        is_toyland = (landscape == 3)
        ih_enabled = bool(self.options.enable_iron_horse.value) and not is_toyland

        # Count available vehicles for this landscape (same logic as create_items)
        if is_toyland:
            eligible_count = len(ALL_VEHICLES)
        else:
            eligible_count = sum(1 for v in ALL_VEHICLES if v not in _TOYLAND_ONLY_VEHICLES)
        if ih_enabled:
            eligible_count += len(IRON_HORSE_ENGINES)

        trap_count   = self.options.trap_count.value
        utility_count = self.options.utility_count.value

        total_items = eligible_count + trap_count + utility_count
        # Ensure even so the 50/50 split is clean
        if total_items % 2 != 0:
            total_items += 1

        half = total_items // 2
        mission_count   = max(10, min(half, 1140))
        shop_item_count = max(5, half)

        return mission_count, shop_item_count

    def generate_early(self) -> None:
        """Generate mission content before items are placed."""
        try:
            player_count = len(self.multiworld.player_ids)
        except Exception:
            player_count = 1
        mc, shop = self._compute_pool_size()
        total = mc + shop
        print(f"[OpenTTD] {player_count} player(s) → {mc} missions + {shop} shop items = {total} total locations")
        self._generate_missions()

    def _generate_missions(self) -> None:
        """Generate random mission content for each mission location.

        Rules enforced:
        1. No exact duplicate (type + amount) within a difficulty level.
        2. For numeric missions of the same type: each successive amount must
           be at least 5x the previous one.
        3. "Buy a vehicle from the shop" appears at most once per difficulty.
        4. Cargo types are filtered to only those that exist on the chosen landscape.
        5. "Service X towns" amounts are capped to a realistic maximum based
           on map size so the mission cannot be impossible to complete.
        """
        rng = self.random
        mission_count, _shop_item_count = self._compute_pool_size()
        missions = []

        # Difficulty multiplier — only applied to monetary / cargo amounts.
        # Vehicle counts, town counts, station counts, months, etc. are NOT scaled
        # so missions stay logically completable regardless of difficulty setting.
        _DIFFICULTY_SCALES = {0: 0.25, 1: 0.5, 2: 1.0, 3: 2.0, 4: 4.0}
        diff_scale = _DIFFICULTY_SCALES.get(self.options.mission_difficulty.value, 1.0)
        _SCALE_UNITS = {"£", "£/month", "units", "passengers", "tons", "passengers_to_town", "mail_to_town", "cargo_to_industry", "cargo_from_industry"}

        # Climate-appropriate cargo list
        landscape = self.options.landscape.value
        cargo_list = CARGO_BY_LANDSCAPE.get(landscape, CARGO_TYPES)

        # Estimate max serviceable towns from map dimensions.
        # A 256×256 map typically generates ~50-80 towns on default settings.
        # Formula: 2^(bits_x + bits_y - 8) * 10, capped at 120.
        bits_x = self.options.map_size_x.map_bits
        bits_y = self.options.map_size_y.map_bits
        max_towns = min(120, max(4, (1 << (bits_x + bits_y - 8)) * 10))

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
                elif type_key.startswith("maintain") and "75" in type_key:
                    type_key = "maintain_75"
                elif type_key.startswith("maintain") and "90" in type_key:
                    type_key = "maintain_90"

                # "Buy a vehicle from the shop" — only once per difficulty
                if unit == "purchase":
                    if shop_used:
                        continue
                    shop_used = True
                    amount = 1
                else:
                    # Determine the minimum allowed amount for this slot
                    # Apply difficulty scaling to monetary/cargo units only
                    scaled_min = max(1, int(amt_min * diff_scale)) if unit in _SCALE_UNITS else amt_min
                    scaled_max = max(scaled_min, int(amt_max * diff_scale)) if unit in _SCALE_UNITS else amt_max

                    floor = scaled_min
                    if type_key in type_max:
                        floor = max(floor, type_max[type_key] * MIN_SPACING_FACTOR)

                    # If the floor already exceeds the template max, skip this
                    # template entirely for this difficulty (pool exhausted)
                    if floor > scaled_max:
                        continue

                    amount = rng.randint(floor, scaled_max)
                    amount = self._round_to_nice(amount)
                    amount = max(floor, amount)  # rounding must not go below floor

                    # Deduplicate: if this exact amount was already used, skip
                    if type_key in used and amount in used[type_key]:
                        continue

                # Cap "Service X towns" missions to realistic map maximum
                if "towns" in unit and amount > max_towns:
                    amount = max(2, int(max_towns * rng.uniform(0.4, 0.9)))
                    amount = self._round_to_nice(amount)
                    if type_key in used and amount in used[type_key]:
                        continue

                cargo = rng.choice(cargo_list) if "{cargo}" in template else ""
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

                # For named-destination missions (town/industry), the unit IS the
                # canonical type identifier that C++ uses for matching.
                # Override the template-derived type_key with the unit string.
                effective_type = unit if unit in {
                    "passengers_to_town", "mail_to_town",
                    "cargo_to_industry", "cargo_from_industry"
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

            # If spacing rules prevented reaching `count`, do a relaxed second pass
            # with no minimum spacing (MIN_SPACING_FACTOR = 1) to fill the gap.
            remaining = count - len(generated)
            if remaining > 0:
                extra_attempts = remaining * 100
                for _ in range(extra_attempts):
                    if len(generated) >= count:
                        break
                    template_data = rng.choice(templates)
                    template, amt_min, amt_max, unit = template_data
                    if unit == "purchase":
                        if shop_used:
                            continue
                        shop_used = True
                        amount = 1
                    else:
                        scaled_min = max(1, int(amt_min * diff_scale)) if unit in _SCALE_UNITS else amt_min
                        scaled_max = max(scaled_min, int(amt_max * diff_scale)) if unit in _SCALE_UNITS else amt_max
                        amount = rng.randint(scaled_min, scaled_max)
                        amount = self._round_to_nice(amount)
                        amount = max(1, amount)
                        if type_key in used and amount in used[type_key]:
                            amount = max(1, amount + 1)
                    cargo = rng.choice(cargo_list) if "{cargo}" in template else ""
                    description = (
                        template.format(amount=f"{amount:,}", cargo=cargo)
                        if cargo else
                        template.format(amount=f"{amount:,}")
                    )
                    raw_key = template.split("{amount}")[0].strip().lower()
                    type_key = raw_key.replace("\u00a3","").replace("\xc2\xa3","").replace("£","").strip().rstrip(",").rstrip()
                    if type_key.startswith("earn") and "month" in template.lower():
                        type_key = "earn monthly"
                    elif type_key.startswith("earn"):
                        type_key = "earn"
                    effective_type = unit if unit in {
                        "passengers_to_town", "mail_to_town",
                        "cargo_to_industry", "cargo_from_industry"
                    } else type_key
                    if type_key not in used:
                        used[type_key] = set()
                    used[type_key].add(amount)
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
        start_type = self.options.starting_vehicle_type.value
        type_names = {1: "train", 2: "road_vehicle", 3: "aircraft", 4: "ship"}

        is_toyland = (self.options.landscape.value == 3)
        # Build exclusion set for current landscape:
        # - Always exclude Toyland-only vehicles on non-Toyland maps
        # - On Temperate: also exclude Arctic/Tropic-only trains
        # - On Arctic/Tropic: also exclude Temperate-only trains
        TOYLAND_ONLY_STARTERS = set(_TOYLAND_ONLY_VEHICLES)
        if not is_toyland:
            if self.options.landscape.value == 0:  # Temperate
                TOYLAND_ONLY_STARTERS |= ARCTIC_TROPIC_ONLY_TRAINS
            elif self.options.landscape.value in (1, 2):  # Arctic, Tropic
                TOYLAND_ONLY_STARTERS |= TEMPERATE_ONLY_TRAINS

        def _pick_starter(vtype: str) -> str:
            pool = STARTING_VEHICLES[vtype]
            if not is_toyland:
                pool = [v for v in pool if v not in TOYLAND_ONLY_STARTERS]
            # Fallback: if filter emptied the pool (shouldn't happen), use full pool
            if not pool:
                pool = STARTING_VEHICLES[vtype]
            return self.random.choice(pool)

        # Random or specific type: give the player `starting_vehicle_count` vehicles.
        # For 'random' we pick from all transport types combined.
        # For a specific type we pick only from that type's starter pool.
        count = max(1, self.options.starting_vehicle_count.value)

        if start_type == 0:
            # Random: pool = all starter-safe vehicles across every transport type
            chosen_type = "random"
            all_starters: List[str] = []
            for vtype in type_names.values():
                pool = STARTING_VEHICLES[vtype]
                if not is_toyland:
                    pool = [v for v in pool if v not in TOYLAND_ONLY_STARTERS]
                all_starters.extend(pool)
        else:
            chosen_type = type_names[start_type]
            all_starters = list(STARTING_VEHICLES[chosen_type])
            if not is_toyland:
                all_starters = [v for v in all_starters if v not in TOYLAND_ONLY_STARTERS]

        # Deduplicate while preserving deterministic order, then shuffle
        seen: set = set()
        unique_starters: List[str] = []
        for v in all_starters:
            if v not in seen:
                seen.add(v)
                unique_starters.append(v)
        self.random.shuffle(unique_starters)

        # Pick `count` vehicles; wrap around if count > pool size
        raw: List[str] = [unique_starters[i % len(unique_starters)] for i in range(count)]

        # Deduplicate (wrapping can produce repeats if count > pool size)
        seen2: set = set()
        starting_vehicles = []
        for v in raw:
            if v not in seen2:
                seen2.add(v)
                starting_vehicles.append(v)

        starting_vehicle = starting_vehicles[0]
        for sv in starting_vehicles:
            self.multiworld.push_precollected(self.create_item(sv))

        self._slot_data["starting_vehicle"] = starting_vehicle
        self._slot_data["starting_vehicle_type"] = chosen_type
        # Extra starters list (C++ client reads this to unlock all starting vehicles)
        self._slot_data["starting_vehicles"] = starting_vehicles

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

        reserved = len(trap_pool) + len(utility_pool)

        # ── Vehicles fill remaining slots ─────────────────────────────────
        # Uses the module-level _TOYLAND_ONLY_VEHICLES constant (same set as
        # _compute_pool_size) so the vehicle count is always consistent.
        vehicle_slots = total_locations - reserved
        eligible_vehicles = ALL_VEHICLES if is_toyland else [
            v for v in ALL_VEHICLES if v not in _TOYLAND_ONLY_VEHICLES
        ]

        # ── Iron Horse: add engines to pool if enabled ────────────────────
        # Iron Horse vehicles don't exist on Toyland maps (no Toyland GRF
        # variants). If Iron Horse is enabled but landscape is Toyland,
        # the GRF is still loaded but no IH items enter the pool.
        ih_enabled = bool(self.options.enable_iron_horse.value) and not is_toyland
        self._slot_data["enable_iron_horse"] = 1 if ih_enabled else 0
        if ih_enabled:
            eligible_vehicles = eligible_vehicles + IRON_HORSE_ENGINES
        # Exclude ALL starting vehicles from the randomised pool (
        # this is 4 vehicles, for other modes just 1).
        starting_set = set(starting_vehicles)
        all_vehicles_shuffled = [v for v in eligible_vehicles if v not in starting_set]
        # Shuffle so trimming removes random vehicles rather than always the last ones
        self.random.shuffle(all_vehicles_shuffled)
        vehicle_pool = all_vehicles_shuffled[:vehicle_slots]
        # Store for fill_slot_data (needed to build locked_vehicles list)
        self._vehicle_pool = vehicle_pool
        self._starting_vehicles = starting_vehicles

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

        computed_mc, computed_shop = self._compute_pool_size()

        # Build item_id_to_name so the C++ client can resolve item IDs to names
        item_id_to_name = {str(data.code): name for name, data in ITEM_TABLE.items()}

        # locked_vehicles: every vehicle item the C++ engine-locking system should
        # lock at session start.  Only these engines will be locked; any GRF engine
        # NOT in this list (e.g. Iron Horse engines when enable_iron_horse=False)
        # is left untouched and remains freely available to the player.
        locked_vehicle_set: set = set(getattr(self, "_vehicle_pool", [])) | set(getattr(self, "_starting_vehicles", []))
        locked_vehicles_list = sorted(locked_vehicle_set)  # deterministic order

        self._slot_data.update({
            "game_version": "15.2",
            "mission_count": computed_mc,
            "shop_item_count": computed_shop,
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
            "locked_vehicles": locked_vehicles_list,
            "shop_prices": self._generate_shop_prices(),
            "shop_item_names": {
                loc: self.multiworld.get_location(loc, self.player).item.name
                for loc in self._get_location_table()
                if loc.startswith("Shop_Purchase_")
                and self.multiworld.get_location(loc, self.player).item is not None
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
            # ── DeathLink ──────────────────────────────────────────
            "death_link":                 bool(self.options.death_link.value),
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
        """Assign a random price to every shop location.

        Uses shop_price_min / shop_price_max if both are non-zero.
        Otherwise falls back to the shop_price_tier ranges.
        Prices are sorted ascending so early shop rotations are cheapest.
        Result is cached so repeated calls return identical prices.
        """
        if self._shop_prices_cache:
            return self._shop_prices_cache

        price_min_opt = self.options.shop_price_min.value
        price_max_opt = self.options.shop_price_max.value

        if price_min_opt > 0 or price_max_opt > 0:
            # Custom range — clamp and validate
            price_min = max(1, price_min_opt)
            price_max = max(price_min + 1, price_max_opt) if price_max_opt > 0 else price_min * 10
        else:
            # Tier fallback
            tier = self.options.shop_price_tier.value
            price_min, price_max = self.SHOP_PRICE_RANGES[tier]

        rng = self.random
        _mc, computed_shop = self._compute_pool_size()
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

    def generate_output(self, output_directory: str) -> None:
        """Nothing extra to generate — all config goes via slot_data."""
        pass

    def get_filler_item_name(self) -> str:
        return self.random.choice(UTILITY_ITEMS)
