from dataclasses import dataclass
from Options import (
    Choice, Range, Toggle, DeathLink, PerGameCommonOptions,
    OptionGroup
)


# ═══════════════════════════════════════════════════════════════
#  RANDOMIZER OPTIONS
# ═══════════════════════════════════════════════════════════════

class StartingVehicleType(Choice):
    """Which vehicle type you start with.
    'random' picks randomly from all transport types.
    For all options, Starting Vehicle Count controls how many vehicles you receive."""
    display_name = "Starting Vehicle Type"
    option_any           = 0
    option_train         = 1
    option_road_vehicle  = 2
    option_aircraft      = 3
    option_ship          = 4
    default = 0


class StartingVehicleCount(Range):
    """How many starting vehicles to receive.
    For 'random', vehicles are drawn from all transport types.
    For a specific type, all vehicles come from that type."""
    display_name = "Starting Vehicle Count"
    range_start = 1
    range_end   = 5
    default     = 2


class MissionTierUnlockCount(Range):
    """How many missions of the previous tier must be completed before the next tier unlocks.
    Example: setting 5 means you need 5 easy missions done before any medium missions are evaluated.
    Set to 0 to disable tier gating entirely (all tiers always available)."""
    display_name = "Mission Tier Unlock Count"
    range_start = 0
    range_end   = 20
    default     = 5


class WinDifficulty(Choice):
    """Overall win difficulty. Sets ALL six victory targets at once.
    You must meet EVERY target simultaneously to win.

    Casual:    Very forgiving — great for first-time players or short sessions.
    Easy:      A relaxed challenge. Takes a few hours of play.
    Normal:    The balanced default experience. Moderate effort required.
    Medium:    Noticeably harder. Requires solid network planning.
    Hard:      Serious challenge. Efficient routes and smart vehicle use needed.
    Very Hard: Expert level. Requires optimised play throughout.
    Extreme:   Near-maximum challenge. Marginal error tolerance.
    Insane:    Almost nothing forgiven. For veteran players only.
    Nutcase:   Maximum standard difficulty. Near-impossible on default map size.
    Madness:   Absurd targets. Only for challenge runs with custom settings.
    Custom:    Use the sliders below to set your own targets."""
    display_name = "Win Difficulty"
    option_casual    = 0
    option_easy      = 1
    option_normal    = 2
    option_medium    = 3
    option_hard      = 4
    option_very_hard = 5
    option_extreme   = 6
    option_insane    = 7
    option_nutcase   = 8
    option_madness   = 9
    option_custom    = 10
    default = 2  # normal


# Custom targets (only used when WinDifficulty == custom)

class WinCustomCompanyValue(Range):
    """[Custom only] Target company value in pounds."""
    display_name = "Custom: Target Company Value (GBP)"
    range_start = 100_000
    range_end   = 10_000_000_000
    default     = 8_000_000


class WinCustomTownPopulation(Range):
    """[Custom only] Target total world population across all towns."""
    display_name = "Custom: Target Town Population"
    range_start = 1_000
    range_end   = 5_000_000
    default     = 100_000


class WinCustomVehicleCount(Range):
    """[Custom only] Target number of active vehicles you own simultaneously."""
    display_name = "Custom: Target Vehicle Count"
    range_start = 1
    range_end   = 500
    default     = 30


class WinCustomCargoDelivered(Range):
    """[Custom only] Total cumulative tons of cargo to deliver this session."""
    display_name = "Custom: Target Cargo Delivered (tons)"
    range_start = 1_000
    range_end   = 500_000_000
    default     = 120_000


class WinCustomMonthlyProfit(Range):
    """[Custom only] Monthly net profit target in pounds."""
    display_name = "Custom: Target Monthly Profit (GBP)"
    range_start = 1_000
    range_end   = 100_000_000
    default     = 100_000


class WinCustomMissionsCompleted(Range):
    """[Custom only] Number of AP missions (location checks) that must be completed."""
    display_name = "Custom: Target Missions Completed"
    range_start = 0
    range_end   = 500
    default     = 35


# ═══════════════════════════════════════════════════════════════
#  SHOP OPTIONS
# ═══════════════════════════════════════════════════════════════

class TrapCount(Range):
    """How many trap items to include in the item pool.
    The total pool size is determined automatically from the available vehicles
    for your chosen landscape and GRFs. Traps are distributed across locations
    alongside vehicles and utility items."""
    display_name = "Trap Count"
    range_start = 0
    range_end   = 50
    default     = 10


class UtilityCount(Range):
    """How many utility items (cash injections, loan reductions, boosts) to include.
    The remainder of the item pool is filled with vehicles for your landscape."""
    display_name = "Utility Count"
    range_start = 5
    range_end   = 100
    default     = 20


class ShopRefreshDays(Range):
    """How many in-game days between shop refreshes."""
    display_name = "Shop Refresh (in-game days)"
    range_start = 30
    range_end   = 365
    default     = 90


class ShopPriceTier(Choice):
    """
    How expensive shop purchases are. Seven tiers from cheapest to most expensive.
    If Shop Price Min or Shop Price Max are set to non-zero values below,
    those sliders override this setting and this option is ignored.

    Tier 1: £10,000 – £500,000
    Tier 2: £50,000 – £1,000,000
    Tier 3: £100,000 – £5,000,000
    Tier 4: £500,000 – £15,000,000
    Tier 5: £1,000,000 – £50,000,000
    Tier 6: £5,000,000 – £150,000,000
    Tier 7: £10,000,000 – £500,000,000
    """
    display_name = "Shop Price Tier"
    option_tier_1_10k_500k         = 0
    option_tier_2_50k_1m           = 1
    option_tier_3_100k_5m          = 2
    option_tier_4_500k_15m         = 3
    option_tier_5_1m_50m           = 4
    option_tier_6_5m_150m          = 5
    option_tier_7_10m_500m         = 6
    default = 0


class ShopPriceMin(Range):
    """
    Custom minimum price (in pounds) for a shop purchase.
    Set to 0 (default) to use the Shop Price Tier setting above.
    If this or Shop Price Max is non-zero, the tier setting is disabled.
    Range: £0 – £100,000,000
    """
    display_name = "Shop Price Minimum (£)  [overrides Tier if non-zero]"
    range_start  = 0
    range_end    = 100_000_000
    default      = 0


class ShopPriceMax(Range):
    """
    Custom maximum price (in pounds) for a shop purchase.
    Set to 0 (default) to use the Shop Price Tier setting above.
    If this or Shop Price Min is non-zero, the tier setting is disabled.
    Must be greater than Shop Price Min.
    Range: £0 – £500,000,000
    """
    display_name = "Shop Price Maximum (£)  [overrides Tier if non-zero]"
    range_start  = 0
    range_end    = 500_000_000
    default      = 0

class MissionDifficulty(Choice):
    """Scales the target amounts in all generated missions up or down.
    Does not affect vehicle/town/station counts — only monetary and cargo targets.

    Very Easy: amounts × 0.25  — great for first-timers or short sessions
    Easy:      amounts × 0.5
    Normal:    amounts × 1.0  — the balanced default experience
    Hard:      amounts × 2.0
    Very Hard: amounts × 4.0  — for veteran players who want a serious challenge"""
    display_name   = "Mission Difficulty"
    option_very_easy = 0
    option_easy      = 1
    option_normal    = 2
    option_hard      = 3
    option_very_hard = 4
    default = 2  # normal


class StartingCashBonus(Choice):
    """Extra cash given to your company at the start of a session,
    on top of whatever loan you take. Helps new players build their
    first routes without going bankrupt.
    None:        £0          (default — no bonus)
    Small:       £50,000
    Medium:      £200,000
    Large:       £500,000
    Very Large:  £2,000,000"""
    display_name       = "Starting Cash Bonus"
    option_none        = 0
    option_small       = 1
    option_medium      = 2
    option_large       = 3
    option_very_large  = 4
    default = 0





# ═══════════════════════════════════════════════════════════════
#  TRAPS
# ═══════════════════════════════════════════════════════════════

class EnableTraps(Toggle):
    """Whether trap items can be sent to you by other players."""
    display_name = "Enable Traps"
    default = 1



class TrapBreakdownWave(Toggle):
    """Enable 'Breakdown Wave' trap — all vehicles break down simultaneously."""
    display_name = "Trap: Breakdown Wave"
    default = 1


class TrapRecession(Toggle):
    """Enable 'Recession' trap — company money is halved."""
    display_name = "Trap: Recession"
    default = 0


class TrapMaintenanceSurge(Toggle):
    """Enable 'Maintenance Surge' trap — a large forced loan is added."""
    display_name = "Trap: Maintenance Surge"
    default = 1


class TrapSignalFailure(Toggle):
    """Enable 'Signal Failure' trap — trains are disrupted."""
    display_name = "Trap: Signal Failure"
    default = 1


class TrapFuelShortage(Toggle):
    """Enable 'Fuel Shortage' trap — vehicles run at reduced speed."""
    display_name = "Trap: Fuel Shortage"
    default = 1


class TrapBankLoan(Toggle):
    """Enable 'Bank Loan' trap — player is forced to take maximum loan."""
    display_name = "Trap: Bank Loan"
    default = 0


class TrapIndustryClosure(Toggle):
    """Enable 'Industry Closure' trap — a serviced industry closes."""
    display_name = "Trap: Industry Closure"
    default = 0


class TrapLicenseRevoke(Toggle):
    """Enable 'Vehicle License Revoke' trap — a random vehicle category (trains/road/aircraft/ships) \
is suspended for 1–2 in-game years. All engines of that type are hidden until the ban expires."""
    display_name = "Trap: Vehicle License Revoke"
    default = 0




class ColbyEvent(Toggle):
    """Enable the Colby Event — a multi-step smuggling storyline.

    A mysterious stranger named Colby asks you to deliver cargo to his
    town over 5 steps. After the final delivery you must choose: arrest
    him for £10M, or let him escape?

    The cargo type is chosen automatically based on your landscape."""
    display_name = "Colby Event"
    default = 0

class WagonPoolMode(Choice):
    """Controls whether wagons appear as unlockable items in the item pool.

    - **all_wagons** (default): All wagons are in the item pool and must be unlocked.
    - **no_wagons**: Wagons are excluded from the pool — all wagons are immediately available.
    - **start_with_one**: One random wagon per cargo group is pre-unlocked; rest are in the pool.
    """
    display_name = "Wagon Pool Mode"
    option_all_wagons     = 0
    option_no_wagons      = 1
    option_start_with_one = 2
    default = 0


# ═══════════════════════════════════════════════════════════════
#  WORLD GENERATION
# ═══════════════════════════════════════════════════════════════

class StartYear(Range):
    """The starting year for the game world. Default is 1950."""
    display_name = "Start Year"
    range_start = 1
    range_end   = 5_000_000
    default     = 1950


class MapSizeX(Choice):
    """Width of the generated map. Minimum 512 — smaller maps don't have enough towns and industries for named missions."""
    display_name = "Map Width"
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 3  # 512

    @property
    def map_bits(self) -> int:
        return self.value + 6


class MapSizeY(Choice):
    """Height of the generated map. Minimum 512 — smaller maps don't have enough towns and industries for named missions."""
    display_name = "Map Height"
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 3  # 512

    @property
    def map_bits(self) -> int:
        return self.value + 6


class Landscape(Choice):
    """The climate / landscape type for the game world."""
    display_name  = "Landscape"
    option_temperate = 0
    option_arctic    = 1
    option_tropical  = 2
    option_toyland   = 3
    default = 0


class LandGenerator(Choice):
    """Which terrain generator to use."""
    display_name        = "Land Generator"
    option_original     = 0
    option_terragenesis = 1
    default = 1


class IndustryDensity(Choice):
    """Number of industries generated at game start."""
    display_name     = "Industry Density"
    option_fund_only = 0
    option_minimal   = 1
    option_very_low  = 2
    option_low       = 3
    option_normal    = 4
    option_high      = 5
    default = 4


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — ECONOMY & FINANCE
# ═══════════════════════════════════════════════════════════════

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
    range_end   = 500_000_000
    default     = 300_000


class InfrastructureMaintenance(Toggle):
    """Monthly maintenance fee for owned infrastructure."""
    display_name = "Infrastructure Maintenance"
    default = 0


class VehicleCosts(Choice):
    """Running cost multiplier for vehicles."""
    display_name  = "Vehicle Running Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


class ConstructionCost(Choice):
    """Construction cost multiplier."""
    display_name  = "Construction Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


class EconomyType(Choice):
    """Economy volatility type."""
    display_name    = "Economy Type"
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


class TownCargoScale(Range):
    """Scale cargo production of towns (percent)."""
    display_name = "Town Cargo Scale (%)"
    range_start = 15
    range_end   = 500
    default     = 100


class IndustryCargoScale(Range):
    """Scale cargo production of industries (percent)."""
    display_name = "Industry Cargo Scale (%)"
    range_start = 15
    range_end   = 500
    default     = 100


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — VEHICLES & INFRASTRUCTURE
# ═══════════════════════════════════════════════════════════════

class MaxTrains(Range):
    """Maximum number of trains per company."""
    display_name = "Max Trains"
    range_start = 0
    range_end   = 65535
    default     = 500


class MaxRoadVehicles(Range):
    """Maximum number of road vehicles per company."""
    display_name = "Max Road Vehicles"
    range_start = 0
    range_end   = 65535
    default     = 500


class MaxAircraft(Range):
    """Maximum number of aircraft per company."""
    display_name = "Max Aircraft"
    range_start = 0
    range_end   = 65535
    default     = 200


class MaxShips(Range):
    """Maximum number of ships per company."""
    display_name = "Max Ships"
    range_start = 0
    range_end   = 65535
    default     = 300


class MaxTrainLength(Range):
    """Maximum length for trains in tiles. Extended beyond vanilla limit of 64."""
    display_name = "Max Train Length (tiles)"
    range_start = 1
    range_end   = 1000
    default     = 7


class StationSpread(Range):
    """
    How many tiles apart station parts may be and still join.
    Set to 1024 for virtually unlimited spread.
    """
    display_name = "Station Spread (tiles)"
    range_start = 4
    range_end   = 1024
    default     = 12


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


class RoadSide(Choice):
    """Which side of the road vehicles drive on."""
    display_name = "Drive Side"
    option_left  = 0
    option_right = 1
    default = 1


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — TOWNS & ENVIRONMENT
# ═══════════════════════════════════════════════════════════════

class TownGrowthRate(Choice):
    """How fast towns grow."""
    display_name     = "Town Growth Rate"
    option_none      = 0
    option_slow      = 1
    option_normal    = 2
    option_fast      = 3
    option_very_fast = 4
    default = 2


class FoundTown(Choice):
    """Whether players can found new towns."""
    display_name          = "Town Founding"
    option_forbidden      = 0
    option_allowed        = 1
    option_custom_layout  = 2
    default = 0


class AllowTownRoads(Toggle):
    """Allow towns to build their own roads."""
    display_name = "Towns Build Roads"
    default = 1


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — DISASTERS & ACCIDENTS
# ═══════════════════════════════════════════════════════════════

class Disasters(Toggle):
    """Enable random disasters (floods, UFOs, etc.)."""
    display_name = "Disasters"
    default = 0


class PlaneCrashes(Choice):
    """Frequency of plane crashes."""
    display_name   = "Plane Crashes"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 2


class VehicleBreakdowns(Choice):
    """Likelihood of vehicle breakdowns."""
    display_name   = "Vehicle Breakdowns"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 1


# ═══════════════════════════════════════════════════════════════
#  NEWGRF OPTIONS
# ═══════════════════════════════════════════════════════════════

class EnableIronHorse(Toggle):
    """Enable Iron Horse train set (GPL v2, by andythenorth).
    When enabled, ~100 additional British-inspired locomotives are added to
    the item pool. The GRF is bundled with the patch and loaded automatically
    at new game start — no manual installation required.
    Iron Horse vehicles work on Temperate, Arctic and Tropical maps.
    They are NOT available on Toyland maps."""
    display_name = "Enable Iron Horse"
    default = 0


# ═══════════════════════════════════════════════════════════════
#  OPTION GROUPS — defines the categories in the Options Creator
# ═══════════════════════════════════════════════════════════════

OPTION_GROUPS = [
    OptionGroup("Randomizer", [
        TrapCount,
        UtilityCount,
        StartingVehicleType,
        StartingVehicleCount,
        WinDifficulty,
        WinCustomCompanyValue,
        WinCustomTownPopulation,
        WinCustomVehicleCount,
        WinCustomCargoDelivered,
        WinCustomMonthlyProfit,
        WinCustomMissionsCompleted,
    ]),
    OptionGroup("Shop", [
        ShopRefreshDays,
        ShopPriceTier,
        ShopPriceMin,
        ShopPriceMax,
        MissionDifficulty,
        StartingCashBonus,
    ]),
    OptionGroup("Traps", [
        EnableTraps,
        TrapBreakdownWave,
        TrapRecession,
        TrapMaintenanceSurge,
        TrapSignalFailure,
        TrapFuelShortage,
        TrapBankLoan,
        TrapIndustryClosure,
    ]),
    OptionGroup("World Generation", [
        StartYear,
        MapSizeX,
        MapSizeY,
        Landscape,
        LandGenerator,
        IndustryDensity,
    ]),
    OptionGroup("Economy & Finance", [
        InfiniteMoney,
        Inflation,
        MaxLoan,
        InfrastructureMaintenance,
        VehicleCosts,
        ConstructionCost,
        EconomyType,
        Bribe,
        ExclusiveRights,
        FundBuildings,
        FundRoads,
        GiveMoney,
        TownCargoScale,
        IndustryCargoScale,
    ]),
    OptionGroup("Vehicles & Infrastructure", [
        MaxTrains,
        MaxRoadVehicles,
        MaxAircraft,
        MaxShips,
        MaxTrainLength,
        StationSpread,
        RoadStopOnTownRoad,
        RoadStopOnCompetitorRoad,
        CrossingWithCompetitor,
        RoadSide,
    ]),
    OptionGroup("Towns & Environment", [
        TownGrowthRate,
        FoundTown,
        AllowTownRoads,
    ]),
    OptionGroup("Disasters & Accidents", [
        Disasters,
        PlaneCrashes,
        VehicleBreakdowns,
    ]),
    OptionGroup("NewGRFs", [
        EnableIronHorse,
    ]),
]


# ═══════════════════════════════════════════════════════════════
#  MAIN OPTIONS DATACLASS
# ═══════════════════════════════════════════════════════════════

class OpenTTDDeathLink(DeathLink):
    """When you die, everyone dies. When anyone else dies, you die.
    Death is triggered by vehicle crashes. Off by default."""
    default = 0


@dataclass
class OpenTTDOptions(PerGameCommonOptions):
    # Randomizer
    starting_vehicle_type:           StartingVehicleType
    starting_vehicle_count:          StartingVehicleCount
    mission_tier_unlock_count:        MissionTierUnlockCount
    win_difficulty:                  WinDifficulty
    win_custom_company_value:        WinCustomCompanyValue
    win_custom_town_population:      WinCustomTownPopulation
    win_custom_vehicle_count:        WinCustomVehicleCount
    win_custom_cargo_delivered:      WinCustomCargoDelivered
    win_custom_monthly_profit:       WinCustomMonthlyProfit
    win_custom_missions_completed:   WinCustomMissionsCompleted
    # Shop
    trap_count:                      TrapCount
    utility_count:                   UtilityCount
    shop_refresh_days:               ShopRefreshDays
    shop_price_tier:                 ShopPriceTier
    shop_price_min:                  ShopPriceMin
    shop_price_max:                  ShopPriceMax
    mission_difficulty:              MissionDifficulty
    starting_cash_bonus:             StartingCashBonus
    # Traps
    enable_traps:                    EnableTraps
    trap_breakdown_wave:             TrapBreakdownWave
    trap_recession:                  TrapRecession
    trap_maintenance_surge:          TrapMaintenanceSurge
    trap_signal_failure:             TrapSignalFailure
    trap_fuel_shortage:              TrapFuelShortage
    trap_bank_loan:                  TrapBankLoan
    trap_industry_closure:           TrapIndustryClosure
    trap_license_revoke:             TrapLicenseRevoke
    wagon_pool_mode:                 WagonPoolMode
    # World Generation
    start_year:                      StartYear
    map_size_x:                      MapSizeX
    map_size_y:                      MapSizeY
    landscape:                       Landscape
    land_generator:                  LandGenerator
    industry_density:                IndustryDensity
    # Economy & Finance
    infinite_money:                  InfiniteMoney
    inflation:                       Inflation
    max_loan:                        MaxLoan
    infrastructure_maintenance:      InfrastructureMaintenance
    vehicle_costs:                   VehicleCosts
    construction_cost:               ConstructionCost
    economy_type:                    EconomyType
    bribe:                           Bribe
    exclusive_rights:                ExclusiveRights
    fund_buildings:                  FundBuildings
    fund_roads:                      FundRoads
    give_money:                      GiveMoney
    town_cargo_scale:                TownCargoScale
    industry_cargo_scale:            IndustryCargoScale
    # Vehicles & Infrastructure
    max_trains:                      MaxTrains
    max_roadveh:                     MaxRoadVehicles
    max_aircraft:                    MaxAircraft
    max_ships:                       MaxShips
    max_train_length:                MaxTrainLength
    station_spread:                  StationSpread
    road_stop_on_town_road:          RoadStopOnTownRoad
    road_stop_on_competitor_road:    RoadStopOnCompetitorRoad
    crossing_with_competitor:        CrossingWithCompetitor
    road_side:                       RoadSide
    # Towns & Environment
    town_growth_rate:                TownGrowthRate
    found_town:                      FoundTown
    allow_town_roads:                AllowTownRoads
    # Disasters & Accidents
    disasters:                       Disasters
    plane_crashes:                   PlaneCrashes
    vehicle_breakdowns:              VehicleBreakdowns
    # NewGRFs
    enable_iron_horse:               EnableIronHorse
    # Death Link
    death_link:                      OpenTTDDeathLink
    colby_event:                     ColbyEvent
