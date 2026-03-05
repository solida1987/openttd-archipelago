from dataclasses import dataclass
from Options import (
    Choice, Range, Toggle, DeathLink, PerGameCommonOptions, OptionSet
)


class MissionCount(Range):
    """How many missions to generate as checks."""
    display_name = "Mission Count"
    range_start = 50
    range_end = 300
    default = 100


class ShopSlots(Range):
    """How many shop slots are available at a time."""
    display_name = "Shop Slots"
    range_start = 3
    range_end = 10
    default = 5


class ShopRefreshDays(Range):
    """How many in-game days between shop refreshes."""
    display_name = "Shop Refresh (in-game days)"
    range_start = 30
    range_end = 365
    default = 90


class ShopPriceTier(Choice):
    """
    How expensive shop purchases are.
    Easy:    £10,000 – £500,000
    Normal:  £100,000 – £5,000,000
    Hard:    £1,000,000 – £50,000,000
    Extreme: £10,000,000 – £300,000,000
    """
    display_name = "Shop Price Tier"
    option_easy    = 0
    option_normal  = 1
    option_hard    = 2
    option_extreme = 3
    default = 1


class EnableTraps(Toggle):
    """Whether trap items can be sent to you by other players."""
    display_name = "Enable Traps"
    default = 1


class StartingVehicleType(Choice):
    """Which vehicle type you start with."""
    display_name = "Starting Vehicle Type"
    option_random = 0
    option_train = 1
    option_road_vehicle = 2
    option_aircraft = 3
    option_ship = 4
    default = 0


class WinCondition(Choice):
    """What you need to do to win."""
    display_name = "Win Condition"
    option_company_value = 0
    option_town_population = 1
    option_vehicle_count = 2
    option_cargo_delivered = 3
    option_monthly_profit = 4
    default = 0


class WinConditionCompanyValue(Range):
    """Target company value in pounds (win condition: company value)."""
    display_name = "Target Company Value (£)"
    range_start = 1_000_000
    range_end = 10_000_000_000
    default = 50_000_000


class WinConditionTownPopulation(Range):
    """Target town population (win condition: town population)."""
    display_name = "Target Town Population"
    range_start = 10_000
    range_end = 500_000
    default = 20_000


class WinConditionVehicleCount(Range):
    """Target number of vehicles running simultaneously (win condition: vehicle count)."""
    display_name = "Target Vehicle Count"
    range_start = 10
    range_end = 500
    default = 50


class WinConditionCargoDelivered(Range):
    """Total tons of cargo to deliver (win condition: cargo delivered)."""
    display_name = "Target Cargo Delivered (tons)"
    range_start = 100_000
    range_end = 100_000_000
    default = 1_000_000


class WinConditionMonthlyProfit(Range):
    """Monthly profit target in pounds (win condition: monthly profit)."""
    display_name = "Target Monthly Profit (£)"
    range_start = 100_000
    range_end = 100_000_000
    default = 1_000_000


# ─────────────────────────────────────────────
#  WORLD GENERATION OPTIONS  (Bug #6 fix)
# ─────────────────────────────────────────────

class StartYear(Range):
    """
    The starting year for the game world.
    Default is 1950 (classic OpenTTD era).
    Range: 1 – 5000000
    """
    display_name = "Start Year"
    range_start = 1
    range_end = 5_000_000
    default = 1950


class MapSizeX(Choice):
    """Width of the generated map (as power of 2)."""
    display_name = "Map Width"
    option_64   = 0
    option_128  = 1
    option_256  = 2
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 2  # 256

    @property
    def map_bits(self) -> int:
        """Returns the log2 value used by OpenTTD's game_creation.map_x."""
        return self.value + 6   # 0->6, 1->7, ..., 5->11


class MapSizeY(Choice):
    """Height of the generated map (as power of 2)."""
    display_name = "Map Height"
    option_64   = 0
    option_128  = 1
    option_256  = 2
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 2  # 256

    @property
    def map_bits(self) -> int:
        return self.value + 6


class Landscape(Choice):
    """The climate / landscape type for the game world."""
    display_name = "Landscape"
    option_temperate = 0
    option_arctic    = 1
    option_tropical  = 2
    option_toyland   = 3
    default = 0


class LandGenerator(Choice):
    """Which terrain generator to use."""
    display_name = "Land Generator"
    option_original    = 0
    option_terragenesis = 1
    default = 1


# ─────────────────────────────────────────────
#  TRAP TOGGLES
# ─────────────────────────────────────────────

class TrapBreakdownWave(Toggle):
    """Enable 'Breakdown Wave' trap - all vehicles break down."""
    display_name = "Trap: Breakdown Wave"
    default = 1


class TrapRecession(Toggle):
    """Enable 'Recession' trap - company money halved."""
    display_name = "Trap: Recession"
    default = 1


class TrapMaintenanceSurge(Toggle):
    """Enable 'Maintenance Surge' trap - large loan added."""
    display_name = "Trap: Maintenance Surge"
    default = 1


class TrapSignalFailure(Toggle):
    """Enable 'Signal Failure' trap - trains disrupted."""
    display_name = "Trap: Signal Failure"
    default = 1


class TrapFuelShortage(Toggle):
    """Enable 'Fuel Shortage' trap - vehicles run slower."""
    display_name = "Trap: Fuel Shortage"
    default = 1


class TrapBankLoan(Toggle):
    """Enable 'Bank Loan' trap - forced maximum loan."""
    display_name = "Trap: Bank Loan"
    default = 1


class TrapIndustryClosure(Toggle):
    """Enable 'Industry Closure' trap - a serviced industry closes."""
    display_name = "Trap: Industry Closure"
    default = 0


# ─────────────────────────────────────────────
#  GAME SETTINGS — ACCOUNTING
# ─────────────────────────────────────────────

class InfiniteMoney(Toggle):
    """Allow spending despite negative balance (cheat mode)."""
    display_name = "Infinite Money"
    default = 0


class Inflation(Toggle):
    """Enable inflation over time."""
    display_name = "Inflation"
    default = 0


class MaxLoan(Range):
    """Maximum initial loan available to the player, in pounds."""
    display_name = "Maximum Initial Loan (£)"
    range_start = 100_000
    range_end = 500_000_000
    default = 300_000


class InfrastructureMaintenance(Toggle):
    """Monthly maintenance fee for owned infrastructure."""
    display_name = "Infrastructure Maintenance"
    default = 0


class VehicleCosts(Choice):
    """Running cost multiplier for vehicles."""
    display_name = "Vehicle Running Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


class ConstructionCost(Choice):
    """Construction cost multiplier."""
    display_name = "Construction Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


# ─────────────────────────────────────────────
#  GAME SETTINGS — VEHICLE LIMITS
# ─────────────────────────────────────────────

class MaxTrains(Range):
    """Maximum number of trains per company."""
    display_name = "Max Trains"
    range_start = 0
    range_end = 65535
    default = 500


class MaxRoadVehicles(Range):
    """Maximum number of road vehicles per company."""
    display_name = "Max Road Vehicles"
    range_start = 0
    range_end = 65535
    default = 500


class MaxAircraft(Range):
    """Maximum number of aircraft per company."""
    display_name = "Max Aircraft"
    range_start = 0
    range_end = 65535
    default = 200


class MaxShips(Range):
    """Maximum number of ships per company."""
    display_name = "Max Ships"
    range_start = 0
    range_end = 65535
    default = 300


class MaxTrainLength(Range):
    """Maximum length for trains in tiles. Extended beyond vanilla maximum of 64."""
    display_name = "Max Train Length (tiles)"
    range_start = 1
    range_end = 1000
    default = 7


class StationSpread(Range):
    """
    How many tiles apart station parts may be and still join.
    Extended beyond vanilla maximum of 64 — set to 1024 for virtually unlimited.
    """
    display_name = "Station Spread (tiles)"
    range_start = 4
    range_end = 1024
    default = 12


class RoadStopOnTownRoad(Toggle):
    """Allow drive-through road stops on roads owned by towns."""
    display_name = "Road Stops on Town Roads"
    default = 1


class RoadStopOnCompetitorRoad(Toggle):
    """Allow drive-through road stops on roads owned by competitors."""
    display_name = "Road Stops on Competitor Roads"
    default = 1


class CrossingWithCompetitor(Toggle):
    """Allow level crossings with roads or rails owned by competitors."""
    display_name = "Level Crossings with Competitors"
    default = 1


# ─────────────────────────────────────────────
#  GAME SETTINGS — DISASTERS / ACCIDENTS
# ─────────────────────────────────────────────

class Disasters(Toggle):
    """Enable random disasters (floods, UFOs, etc.)."""
    display_name = "Disasters"
    default = 0


class PlaneCrashes(Choice):
    """Frequency of plane crashes."""
    display_name = "Plane Crashes"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 2


class VehicleBreakdowns(Choice):
    """Likelihood of vehicle breakdowns."""
    display_name = "Vehicle Breakdowns"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 1


# ─────────────────────────────────────────────
#  GAME SETTINGS — ECONOMY / ENVIRONMENT
# ─────────────────────────────────────────────

class EconomyType(Choice):
    """Economy volatility type."""
    display_name = "Economy Type"
    option_original = 0
    option_smooth   = 1
    option_frozen   = 2
    default = 1


class Bribe(Toggle):
    """Allow bribing the local authority."""
    display_name = "Allow Bribing"
    default = 1


class ExclusiveRights(Toggle):
    """Allow buying exclusive transport rights."""
    display_name = "Exclusive Rights"
    default = 1


class FundBuildings(Toggle):
    """Allow funding new buildings in towns."""
    display_name = "Fund Buildings"
    default = 1


class FundRoads(Toggle):
    """Allow funding local road reconstruction."""
    display_name = "Fund Roads"
    default = 1


class GiveMoney(Toggle):
    """Allow giving money to other companies."""
    display_name = "Give Money to Competitors"
    default = 1


class TownGrowthRate(Choice):
    """How fast towns grow."""
    display_name = "Town Growth Rate"
    option_none      = 0
    option_slow      = 1
    option_normal    = 2
    option_fast      = 3
    option_very_fast = 4
    default = 2


class FoundTown(Choice):
    """Whether players can found new towns."""
    display_name = "Town Founding"
    option_forbidden      = 0
    option_allowed        = 1
    option_custom_layout  = 2
    default = 0


class TownCargoScale(Range):
    """Scale cargo production of towns (percent)."""
    display_name = "Town Cargo Scale (%)"
    range_start = 15
    range_end = 500
    default = 100


class IndustryCargoScale(Range):
    """Scale cargo production of industries (percent)."""
    display_name = "Industry Cargo Scale (%)"
    range_start = 15
    range_end = 500
    default = 100


class IndustryDensity(Choice):
    """Number of industries generated at game start."""
    display_name = "Industry Density"
    option_fund_only  = 0
    option_minimal    = 1
    option_very_low   = 2
    option_low        = 3
    option_normal     = 4
    option_high       = 5
    default = 4


class AllowTownRoads(Toggle):
    """Allow towns to build their own roads."""
    display_name = "Towns Build Roads"
    default = 1


class RoadSide(Choice):
    """Which side of the road vehicles drive on."""
    display_name = "Drive Side"
    option_left  = 0
    option_right = 1
    default = 1


@dataclass
class OpenTTDOptions(PerGameCommonOptions):
    # ── Core AP options ──────────────────────────────────────────────
    mission_count: MissionCount
    shop_slots: ShopSlots
    shop_refresh_days: ShopRefreshDays
    shop_price_tier: ShopPriceTier
    enable_traps: EnableTraps
    starting_vehicle_type: StartingVehicleType
    win_condition: WinCondition
    win_condition_company_value: WinConditionCompanyValue
    win_condition_town_population: WinConditionTownPopulation
    win_condition_vehicle_count: WinConditionVehicleCount
    win_condition_cargo_delivered: WinConditionCargoDelivered
    win_condition_monthly_profit: WinConditionMonthlyProfit
    # ── Trap toggles ─────────────────────────────────────────────────
    trap_breakdown_wave: TrapBreakdownWave
    trap_recession: TrapRecession
    trap_maintenance_surge: TrapMaintenanceSurge
    trap_signal_failure: TrapSignalFailure
    trap_fuel_shortage: TrapFuelShortage
    trap_bank_loan: TrapBankLoan
    trap_industry_closure: TrapIndustryClosure
    # ── World generation ─────────────────────────────────────────────
    start_year: StartYear
    map_size_x: MapSizeX
    map_size_y: MapSizeY
    landscape: Landscape
    land_generator: LandGenerator
    # ── Game settings: Accounting ────────────────────────────────────
    infinite_money: InfiniteMoney
    inflation: Inflation
    max_loan: MaxLoan
    infrastructure_maintenance: InfrastructureMaintenance
    vehicle_costs: VehicleCosts
    construction_cost: ConstructionCost
    # ── Game settings: Vehicle Limits ────────────────────────────────
    max_trains: MaxTrains
    max_roadveh: MaxRoadVehicles
    max_aircraft: MaxAircraft
    max_ships: MaxShips
    max_train_length: MaxTrainLength
    station_spread: StationSpread
    road_stop_on_town_road: RoadStopOnTownRoad
    road_stop_on_competitor_road: RoadStopOnCompetitorRoad
    crossing_with_competitor: CrossingWithCompetitor
    # ── Game settings: Disasters / Accidents ─────────────────────────
    disasters: Disasters
    plane_crashes: PlaneCrashes
    vehicle_breakdowns: VehicleBreakdowns
    # ── Game settings: Economy / Environment ─────────────────────────
    economy_type: EconomyType
    bribe: Bribe
    exclusive_rights: ExclusiveRights
    fund_buildings: FundBuildings
    fund_roads: FundRoads
    give_money: GiveMoney
    town_growth_rate: TownGrowthRate
    found_town: FoundTown
    town_cargo_scale: TownCargoScale
    industry_cargo_scale: IndustryCargoScale
    industry_density: IndustryDensity
    allow_town_roads: AllowTownRoads
    road_side: RoadSide
    # ── Death Link ───────────────────────────────────────────────────
    death_link: DeathLink
