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
#  MISSION OPTIONS
# ═══════════════════════════════════════════════════════════════

class MissionTierUnlockCount(Range):
    """How many missions of the previous tier must be completed before the next tier unlocks.
    Example: setting 5 means you need 5 easy missions done before any medium missions are evaluated.
    Set to 0 to disable tier gating entirely (all tiers always available)."""
    display_name = "Mission Tier Unlock Count"
    range_start = 0
    range_end   = 20
    default     = 5


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


# ═══════════════════════════════════════════════════════════════
#  SHOP & ITEMS OPTIONS
# ═══════════════════════════════════════════════════════════════

class UtilityCount(Range):
    """How many utility items (cash injections, loan reductions, boosts) to include.
    The remainder of the item pool is filled with vehicles for your landscape."""
    display_name = "Utility Count"
    range_start = 5
    range_end   = 100
    default     = 20



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
#  ITEM POOL
# ═══════════════════════════════════════════════════════════════

class EnableWagonUnlocks(Toggle):
    """When enabled, wagons are locked at session start and must be unlocked via Archipelago items.
    When disabled, all wagons are immediately available. Starting trains guarantee at least one
    usable wagon precollected."""
    display_name = "Enable Wagon Unlocks"
    default = 0


class EnableRailDirectionUnlocks(Toggle):
    """When enabled, each rail type (Normal, Electrified, Monorail, Maglev) has 6 track
    directions locked at session start. You must unlock all 6 for a rail type before you
    can build that type in any direction. Adds 24 progression items to the pool
    (4 rail types × 6 directions: NE-SW, NW-SE, N, S, W, E).
    Starting trains guarantee at least one direction unlock and a usable wagon precollected."""
    display_name = "Enable Rail Direction Unlocks"
    default = 0


class EnableRoadDirectionUnlocks(Toggle):
    """When enabled, road directions (NE-SW and NW-SE) are locked at session start.
    Adds 2 progression items to the pool. Starting road vehicles guarantee one direction."""
    display_name = "Enable Road Direction Unlocks"
    default = 0


class EnableSignalUnlocks(Toggle):
    """When enabled, all 6 signal types are locked at session start.
    Adds 6 progression items to the pool."""
    display_name = "Enable Signal Unlocks"
    default = 0


class EnableBridgeUnlocks(Toggle):
    """When enabled, all 13 bridge types are locked at session start.
    Adds 13 useful items to the pool."""
    display_name = "Enable Bridge Unlocks"
    default = 0


class EnableTunnelUnlocks(Toggle):
    """When enabled, tunnel construction is locked at session start.
    Adds 1 progression item to the pool."""
    display_name = "Enable Tunnel Unlocks"
    default = 0


class EnableAirportUnlocks(Toggle):
    """When enabled, all airport types except Small Airport are locked at session start.
    Small Airport is always free. Adds 8 useful items to the pool."""
    display_name = "Enable Airport Unlocks"
    default = 0


class EnableTreeUnlocks(Toggle):
    """When enabled, tree types are locked by climate pack at session start.
    Adds 10 filler items to the pool (3 packs per climate + 1 Toyland)."""
    display_name = "Enable Tree Unlocks"
    default = 0


class EnableTerraformUnlocks(Toggle):
    """When enabled, Raise Land and Lower Land are locked at session start.
    Level Land auto-unlocks when both raise and lower are unlocked.
    Adds 2 progression items to the pool."""
    display_name = "Enable Terraform Unlocks"
    default = 0


class EnableTownActionUnlocks(Toggle):
    """When enabled, all 8 town authority actions are locked at session start.
    Adds 8 useful items to the pool. Actions: Advertise Small/Medium/Large,
    Fund Road Reconstruction, Build Statue, Fund Buildings,
    Buy Exclusive Transport Rights, Bribe Authority."""
    display_name = "Enable Town Action Unlocks"
    default = 0


class RuinPoolSize(Range):
    """Total number of ruins that will spawn over the course of the game.
    Ruins appear on the map and require cargo delivery to clear them.
    Clearing a ruin sends a check (items come from the shop pool).
    Set to 0 to disable ruins entirely."""
    display_name = "Ruin Pool Size"
    range_start = 0
    range_end = 100
    default = 25


class MaxActiveRuins(Range):
    """Maximum number of ruins that can exist on the map at the same time.
    New ruins spawn after existing ones are cleared (with a cooldown)."""
    display_name = "Max Active Ruins"
    range_start = 1
    range_end = 10
    default = 6


class RuinCargoTypesMin(Range):
    """Minimum number of different cargo types each ruin requires.
    Includes all cargo types for the landscape (passengers, mail, goods, coal, etc.)."""
    display_name = "Ruin Cargo Types (Min)"
    range_start = 1
    range_end = 5
    default = 2


class RuinCargoTypesMax(Range):
    """Maximum number of different cargo types each ruin requires.
    The actual number is randomly picked between min and max for each ruin."""
    display_name = "Ruin Cargo Types (Max)"
    range_start = 2
    range_end = 7
    default = 4


# ═══════════════════════════════════════════════════════════════
#  TRAPS
# ═══════════════════════════════════════════════════════════════

class EnableTraps(Toggle):
    """Whether trap items can be sent to you by other players."""
    display_name = "Enable Traps"
    default = 1


class TrapCount(Range):
    """How many trap items to include in the item pool.
    The total pool size is determined automatically from the available vehicles
    for your chosen landscape and GRFs. Traps are distributed across locations
    alongside vehicles and utility items."""
    display_name = "Trap Count"
    range_start = 0
    range_end   = 50
    default     = 10


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
#  EVENTS
# ═══════════════════════════════════════════════════════════════

class ColbyEvent(Toggle):
    """Enable the Colby Event — a multi-step smuggling storyline.\
    A mysterious stranger named Colby asks you to deliver cargo to his town over 5 steps.\
    After the final delivery you must choose: arrest him for £10M, or let him escape?\
    The cargo type is chosen automatically based on your landscape."""
    display_name = "Colby Event"
    default = 0


class EnableDemigods(Toggle):
    """Enable the Demigod system — the God of Wackens periodically sends rival AI
    transport companies to challenge you. Each demigod has a theme (trains, road,
    aircraft, ships, or mixed) and must be defeated by paying a tribute.
    Defeating a demigod sends an Archipelago check."""
    display_name = "Demigods (God of Wackens)"
    default = 0


class DemigodCount(Range):
    """How many demigods will appear over the course of the game.
    Each one is an AP check — defeating them sends items into the multiworld."""
    display_name = "Demigod Count"
    range_start = 1
    range_end = 10
    default = 3


class DemigodSpawnIntervalMin(Range):
    """Minimum number of in-game years between demigod spawns."""
    display_name = "Demigod Spawn Interval (Min Years)"
    range_start = 1
    range_end = 30
    default = 3


class DemigodSpawnIntervalMax(Range):
    """Maximum number of in-game years between demigod spawns."""
    display_name = "Demigod Spawn Interval (Max Years)"
    range_start = 2
    range_end = 50
    default = 8


# ═══════════════════════════════════════════════════════════════
#  FUNNY STUFF
# ═══════════════════════════════════════════════════════════════

class CommunityVehicleNames(Toggle):
    """When enabled, vehicles you build are automatically named after members of
    the OpenTTD Archipelago community — with a small chance of a rare funny name.
    Turn this off if you prefer to name your own vehicles."""
    display_name = "Community Vehicle Names"
    default = 1


# ═══════════════════════════════════════════════════════════════
#  OPTION GROUPS — defines the categories in the Options Creator
# ═══════════════════════════════════════════════════════════════

OPTION_GROUPS = [
    OptionGroup("Randomizer", [
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
    OptionGroup("Missions", [
        MissionTierUnlockCount,
        MissionDifficulty,
    ]),
    OptionGroup("Shop & Items", [
        UtilityCount,
        ShopPriceTier,
        StartingCashBonus,
    ]),
    OptionGroup("Item Pool", [
        EnableWagonUnlocks,
        EnableRailDirectionUnlocks,
        EnableRoadDirectionUnlocks,
        EnableSignalUnlocks,
        EnableBridgeUnlocks,
        EnableTunnelUnlocks,
        EnableAirportUnlocks,
        EnableTreeUnlocks,
        EnableTerraformUnlocks,
        EnableTownActionUnlocks,
        RuinPoolSize,
        MaxActiveRuins,
    ]),
    OptionGroup("Traps", [
        EnableTraps,
        TrapCount,
        TrapBreakdownWave,
        TrapRecession,
        TrapMaintenanceSurge,
        TrapSignalFailure,
        TrapFuelShortage,
        TrapBankLoan,
        TrapIndustryClosure,
        TrapLicenseRevoke,
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
    OptionGroup("Events", [
        ColbyEvent,
        EnableDemigods,
        DemigodCount,
        DemigodSpawnIntervalMin,
        DemigodSpawnIntervalMax,
    ]),
    OptionGroup("Funny Stuff", [
        CommunityVehicleNames,
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
    win_difficulty:                  WinDifficulty
    win_custom_company_value:        WinCustomCompanyValue
    win_custom_town_population:      WinCustomTownPopulation
    win_custom_vehicle_count:        WinCustomVehicleCount
    win_custom_cargo_delivered:      WinCustomCargoDelivered
    win_custom_monthly_profit:       WinCustomMonthlyProfit
    win_custom_missions_completed:   WinCustomMissionsCompleted
    # Missions
    mission_tier_unlock_count:       MissionTierUnlockCount
    mission_difficulty:              MissionDifficulty
    # Shop & Items
    utility_count:                   UtilityCount
    shop_price_tier:                 ShopPriceTier
    starting_cash_bonus:             StartingCashBonus
    # Item Pool
    enable_wagon_unlocks:            EnableWagonUnlocks
    enable_rail_direction_unlocks:   EnableRailDirectionUnlocks
    enable_road_direction_unlocks:   EnableRoadDirectionUnlocks
    enable_signal_unlocks:           EnableSignalUnlocks
    enable_bridge_unlocks:           EnableBridgeUnlocks
    enable_tunnel_unlocks:           EnableTunnelUnlocks
    enable_airport_unlocks:          EnableAirportUnlocks
    enable_tree_unlocks:             EnableTreeUnlocks
    enable_terraform_unlocks:        EnableTerraformUnlocks
    enable_town_action_unlocks:      EnableTownActionUnlocks
    ruin_pool_size:                  RuinPoolSize
    max_active_ruins:                MaxActiveRuins
    ruin_cargo_types_min:            RuinCargoTypesMin
    ruin_cargo_types_max:            RuinCargoTypesMax
    # Traps
    enable_traps:                    EnableTraps
    trap_count:                      TrapCount
    trap_breakdown_wave:             TrapBreakdownWave
    trap_recession:                  TrapRecession
    trap_maintenance_surge:          TrapMaintenanceSurge
    trap_signal_failure:             TrapSignalFailure
    trap_fuel_shortage:              TrapFuelShortage
    trap_bank_loan:                  TrapBankLoan
    trap_industry_closure:           TrapIndustryClosure
    trap_license_revoke:             TrapLicenseRevoke
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
    # Events
    colby_event:                     ColbyEvent
    enable_demigods:                 EnableDemigods
    demigod_count:                   DemigodCount
    demigod_spawn_interval_min:      DemigodSpawnIntervalMin
    demigod_spawn_interval_max:      DemigodSpawnIntervalMax
    # Funny Stuff
    community_vehicle_names:         CommunityVehicleNames
    # Death Link
    death_link:                      OpenTTDDeathLink
