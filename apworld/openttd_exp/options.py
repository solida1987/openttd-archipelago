from dataclasses import dataclass
from Options import (
    Choice, Range, Toggle, DeathLink, PerGameCommonOptions,
    OptionGroup, Visibility
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
#  PROGRESSION BALANCING — ITEM PLACEMENT CONTROL
# ═══════════════════════════════════════════════════════════════
#
#  These options control WHERE the Archipelago fill algorithm places
#  progression items (vehicles, infrastructure, other players' key items).
#
#  Each location type can be set to:
#    Priority  = fill algorithm PREFERS placing progression items here
#    Default   = neutral — fill algorithm treats normally
#    Excluded  = fill algorithm will NEVER place progression items here
#
#  The fill algorithm ("balanced") works by first placing all progression
#  items into PRIORITY locations.  If there are more progression items than
#  PRIORITY slots, it overflows into DEFAULT locations automatically.
#  EXCLUDED locations only ever receive filler or useful items.
#
#  TL;DR: PRIORITY = "important items land here first",
#         DEFAULT  = "overflow / neutral",
#         EXCLUDED = "never progression items".
# ═══════════════════════════════════════════════════════════════


class VictoryVehicleRequirement(Range):
    """Minimum number of vehicles the logic requires before Victory is reachable.

    The Archipelago logic uses this to determine which sphere Victory falls in.
    Higher values push Victory into later spheres (more items needed first).
    Lower values allow Victory to appear earlier in the spoiler log.

    The actual in-game win condition is set by Win Difficulty — this only
    affects the logical SPHERE placement. If you set this too low, Victory
    may appear reachable in sphere 2-3 which means other players' items
    could be locked behind your early completion.

    Default 15 ensures Victory sits comfortably in sphere 4+.
    If you have many NewGRFs enabled, consider raising this."""
    display_name = "Victory Vehicle Requirement (Logic)"
    range_start = 5
    range_end   = 50
    default     = 15


class HardTierVehicleMultiplier(Range):
    """Vehicle count multiplier for Hard mission access rules.

    The number of vehicles needed to access Hard missions is calculated as:
    Mission Tier Unlock Count × this multiplier.

    Default 2 means if Tier Unlock Count is 5, you need 10 vehicles for Hard.
    Set higher to make Hard missions require a larger fleet.
    Set to 1 to make Hard missions accessible with fewer vehicles.

    Note: If this results in fewer than 1, the minimum is always 1."""
    display_name = "Hard Tier Vehicle Multiplier"
    range_start = 1
    range_end   = 5
    default     = 2


class ExtremeTierVehicleMultiplier(Range):
    """Vehicle count multiplier for Extreme mission access rules.

    The number of vehicles needed to access Extreme missions is calculated as:
    Mission Tier Unlock Count × this multiplier.

    Default 3 means if Tier Unlock Count is 5, you need 15 vehicles for Extreme.
    Set higher for a longer grind before endgame content.
    Set to 1 to make Extreme missions accessible much earlier.

    Note: If this results in fewer than 1, the minimum is always 1."""
    display_name = "Extreme Tier Vehicle Multiplier"
    range_start = 1
    range_end   = 10
    default     = 3


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


class SpeedBoostCount(Range):
    """How many Speed Boost items to place in the multiworld pool.
    Each Speed Boost gives +10% fast-forward speed.
    20 items = 100% → 300% max speed (default).
    100 items = 100% → 1100% max speed.
    More items means more chances to go faster, but takes up item slots."""
    display_name = "Speed Boost Count"
    range_start = 20
    range_end   = 100
    default     = 20


# ═══════════════════════════════════════════════════════════════
#  ITEM POOL
# ═══════════════════════════════════════════════════════════════

class EnableWagonUnlocks(Toggle):
    """Lock wagons at session start — they must be received as Archipelago items.
    Without this, all wagons are available from the start.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Wagon Unlocks"
    default = 0


class EnableSphereProgression(Toggle):
    """Enable sphere-based progression. When ON, all unlock items (rail directions,
    road directions, signals, bridges, tunnels, airports, terraform, wagons, trees,
    town actions) are locked at session start and must be found as Archipelago items.
    This creates proper progression spheres:

    Sphere 0: Starting vehicles + precollected infra → Easy missions
    Sphere 1: More vehicles + basic infra (rail/road/airports) → Medium missions
    Sphere 2: Even more vehicles + crossing infra (bridge/tunnel) → Hard missions
    Sphere 3: Full fleet + terraform → Extreme missions

    When OFF, all infrastructure is available from the start (flat progression).
    This is a master toggle — it enables ALL unlock categories at once."""
    display_name = "Enable Sphere Progression"
    default = 0


class EnableRailDirectionUnlocks(Toggle):
    """Lock rail track directions (Normal, Electrified, Monorail, Maglev x 6 dirs each = 24 items).
    You must find direction unlocks before you can build track in that orientation.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Rail Direction Unlocks"
    default = 0


class EnableRoadDirectionUnlocks(Toggle):
    """Lock road directions (NE-SW, NW-SE). You must find direction unlocks before
    you can build roads in that orientation.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Road Direction Unlocks"
    default = 0


class EnableSignalUnlocks(Toggle):
    """Lock signal types (Block, Entry, Exit, Combo, Path, One-Way Path).
    You must find signal unlocks before you can place them.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Signal Unlocks"
    default = 0


class EnableBridgeUnlocks(Toggle):
    """Lock bridge types (Wooden through Tubular Silicon = 13 items).
    You must find bridge unlocks before you can build them.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Bridge Unlocks"
    default = 0


class EnableTunnelUnlocks(Toggle):
    """Lock tunnel construction. You must find the Tunnel Construction item
    before you can dig tunnels.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Tunnel Unlocks"
    default = 0


class EnableAirportUnlocks(Toggle):
    """Lock airport types (Large, Heliport, Metropolitan, International, Commuter,
    Helidepot, Intercontinental, Helistation = 8 items). Small Airport is always free.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Airport Unlocks"
    default = 0


class EnableTreeUnlocks(Toggle):
    """Lock tree planting — 10 tree packs (3 per climate + Toyland) must be
    received as items before you can plant those tree types.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Tree Unlocks"
    default = 0


class EnableTerraformUnlocks(Toggle):
    """Lock terraform abilities (Raise Land, Lower Land). You must find terraform
    unlocks before you can modify terrain height.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
    display_name = "Enable Terraform Unlocks"
    default = 0


class EnableTownActionUnlocks(Toggle):
    """Lock town authority actions (advertise, bribe, fund buildings, etc. = 8 items).
    You must find action unlocks before you can use them.
    NOTE: Ignored when Sphere Progression is ON (auto-enabled)."""
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
#  STARS (map collectibles)
# ═══════════════════════════════════════════════════════════════

class EnableStars(Toggle):
    """Enable star collectibles scattered across the map.
    Small star outlines are hidden in open fields. Click one to collect it
    and send an Archipelago check. Stars never spawn near buildings."""
    display_name = "Enable Stars"
    default = 1


class StarPoolSize(Range):
    """Internal — kept for option compatibility but hidden.
    Star count is computed dynamically (50/50 split with shop)."""
    display_name = "Star Pool Size"
    visibility = Visibility.none
    range_start = 10
    range_end = 2000
    default = 50


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


class TerrainType(Choice):
    """How mountainous the landscape is."""
    display_name       = "Terrain Type"
    option_very_flat   = 0
    option_flat        = 1
    option_hilly       = 2
    option_mountainous = 3
    option_alpinist    = 4
    default = 1


class SeaLevel(Choice):
    """Amount of water on the map."""
    display_name     = "Sea Level"
    option_very_low  = 0
    option_low       = 1
    option_medium    = 2
    option_high      = 3
    default = 1


class Rivers(Choice):
    """Amount of rivers generated on the map."""
    display_name  = "Rivers"
    option_none     = 0
    option_few      = 1
    option_moderate = 2
    option_many     = 3
    default = 2


class Smoothness(Choice):
    """How smooth or rough the terrain is (TerraGenesis only)."""
    display_name       = "Smoothness"
    option_very_smooth = 0
    option_smooth      = 1
    option_rough       = 2
    option_very_rough  = 3
    default = 1


class VarietyDistribution(Choice):
    """How varied the terrain height distribution is."""
    display_name     = "Variety Distribution"
    option_none      = 0
    option_very_low  = 1
    option_low       = 2
    option_medium    = 3
    option_high      = 4
    option_very_high = 5
    default = 0


class NumberOfTowns(Choice):
    """How many towns are generated at game start."""
    display_name     = "Number of Towns"
    option_very_low  = 0
    option_low       = 1
    option_normal    = 2
    option_high      = 3
    default = 2


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


class TownNames(Choice):
    """Town name generator style."""
    display_name          = "Town Names"
    option_english        = 0
    option_french         = 1
    option_german         = 2
    option_american       = 3
    option_latin          = 4
    option_silly          = 5
    option_swedish        = 6
    option_dutch          = 7
    option_finnish        = 8
    option_polish         = 9
    option_slovak         = 10
    option_norwegian      = 11
    option_hungarian      = 12
    option_austrian       = 13
    option_romanian       = 14
    option_czech          = 15
    option_swiss          = 16
    option_danish         = 17
    option_turkish        = 18
    option_italian        = 19
    option_catalan        = 20
    default = 0


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


class EnableMilitaryItems(Toggle):
    """Enable Military Items aircraft set (69 military aircraft + helicopters).
    Adds fighters, transports, reconnaissance planes, trainers, and helicopters
    spanning from 1913 to 2015. All aircraft are small-airport compatible
    except the Lockheed C-5 Galaxy (large). Works on all climates.
    NOT available on Toyland maps."""
    display_name = "Enable Military Items"
    default = 0


class EnableSharkShips(Toggle):
    """Enable SHARK Ship Set (GPL v2, by andythenorth).
    Adds 70 ships including freighters, ferries, tankers, barges, hovercrafts,
    hydrofoils, and container ships spanning from 1850 to 2008.
    When enabled, vanilla ships are replaced by SHARK ships.
    Works on all climates. NOT available on Toyland maps."""
    display_name = "Enable SHARK Ships"
    default = 0


class EnableHoverVehicles(Toggle):
    """Enable Hover Vehicles set (6 futuristic hover road vehicles).
    Adds hover buses and trucks available from 2075 onwards.
    These are additional road vehicles — they do NOT replace vanilla ones.
    Works on all climates. NOT available on Toyland maps."""
    display_name = "Enable Hover Vehicles"
    default = 0


class EnableHEQS(Toggle):
    """Enable HEQS Heavy Equipment Set (46 heavy-duty road vehicles + 1 train).
    Adds off-road crawlers, industrial trams, dump trucks, mining trucks,
    logging trucks, foundry transporters, tractors, and railmotors.
    These are additional vehicles — they do NOT replace vanilla ones.
    Works on all climates. NOT available on Toyland maps."""
    display_name = "Enable HEQS Heavy Equipment"
    default = 0


class EnableVactrain(Toggle):
    """Enable Vactrain Set (18 futuristic vacuum-tube trains).
    Adds VACT railtype with extreme-speed passenger and cargo locomotives.
    Introduces a new rail type (Vacuum Tube). Trains appear from 2022 onwards.
    These are additional trains — they do NOT replace vanilla or Iron Horse ones.
    Works on all climates. NOT available on Toyland maps."""
    display_name = "Enable Vactrain Set"
    default = 0


class EnableAircraftpack(Toggle):
    """Enable Aircraftpack 2025 (47 real-world + futuristic aircraft).
    Adds planes from early aviation (1928) through futuristic designs (2042).
    Includes commercial airliners, helicopters, and concept aircraft.
    These are additional aircraft — they do NOT replace vanilla or Military ones.
    Works on all climates. NOT available on Toyland maps."""
    display_name = "Enable Aircraftpack 2025"
    default = 0


class EnableFIRS(Toggle):
    """Enable FIRS Industry Replacement Set (41 industries, 96 cargo types).
    Completely replaces vanilla industries and cargos with a detailed industrial
    economy. Multiple economy presets: Temperate Basic, Arctic Basic, Tropic Basic,
    Steeltown, In A Hot Country. WARNING: This fundamentally changes the game economy.
    Cargo types for vehicle refits and missions will be different from vanilla.
    NOT available on Toyland maps."""
    display_name = "Enable FIRS Industries"
    default = 0


class FIRSEconomy(Choice):
    """Which FIRS economy preset to use. Only applies when FIRS is enabled.
    Each economy has different industries and cargo chains.
    Temperate Basic: Standard temperate industries.
    Arctic Basic: Cold-climate industries.
    Tropic Basic: Tropical industries.
    Steeltown: Heavy industry focus (steel, chemicals, engineering supplies).
    In A Hot Country: Tropical plantation and mining focus."""
    display_name = "FIRS Economy"
    option_temperate_basic   = 0
    option_arctic_basic      = 1
    option_tropic_basic      = 2
    option_steeltown         = 3
    option_in_a_hot_country  = 4
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
#  GOD OF WACKENS — WRATH SYSTEM
# ═══════════════════════════════════════════════════════════════

class EnableWrath(Toggle):
    """Enable the God of Wackens wrath system.
    When ON, the God of Wackens watches your destruction. Bulldozing houses,
    removing town roads, terraforming terrain, and clearing trees all count
    against yearly limits. Exceed the limits and his anger escalates through
    5 levels — from a gentle warning up to divine wrath (infrastructure
    destruction, station deletion, vehicle breakdowns, and sinkholes).
    Each year of good behavior reduces anger by 1 level.
    When OFF, you can destroy freely with no consequences."""
    display_name = "God of Wackens (Wrath System)"
    default = 1


class WrathLimitHouses(Range):
    """How many town buildings you can bulldoze per year before the God of Wackens
    notices. Exceeding this limit counts as one category violation.
    If 3+ categories are exceeded in the same year, anger jumps by +2 instead of +1."""
    display_name = "Wrath: House Demolition Limit (per year)"
    range_start = 1
    range_end = 100
    default = 2


class WrathLimitRoads(Range):
    """How many town-owned road tiles you can remove per year before the God of Wackens
    notices. Only roads owned by towns count — your own roads are free to remove."""
    display_name = "Wrath: Town Road Removal Limit (per year)"
    range_start = 1
    range_end = 100
    default = 2


class WrathLimitTerrain(Range):
    """How many terrain tiles you can terraform (raise/lower) per year before the
    God of Wackens notices. Each terraform action counts as one tile."""
    display_name = "Wrath: Terraform Limit (tiles per year)"
    range_start = 1
    range_end = 1000
    default = 25



# ═══════════════════════════════════════════════════════════════
#  MULTIPLAYER
# ═══════════════════════════════════════════════════════════════

class MultiplayerMode(Toggle):
    """Enable multiplayer compatibility mode.
    When ON, features that cause desync in multiplayer are disabled:
    - Ruins (God of Wackens curses)
    - Colby Event
    - Demigod system (God of Wackens bosses)
    - Wrath system (God of Wackens anger)

    These features modify the game map directly and cannot be synced
    between host and clients. With this OFF, the 'Open to Multiplayer'
    button in-game will be greyed out.

    Turn this ON if you want to play cooperatively with other players."""
    display_name = "Multiplayer Mode"
    default = 0


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
    OptionGroup("Progression Balancing", [
        EnableSphereProgression,
        MissionTierUnlockCount,
        MissionDifficulty,
        VictoryVehicleRequirement,
        HardTierVehicleMultiplier,
        ExtremeTierVehicleMultiplier,
    ]),
    OptionGroup("Items & Shop", [
        UtilityCount,
        SpeedBoostCount,
        ShopPriceTier,
        StartingCashBonus,
    ]),
    OptionGroup("Ruins", [
        RuinPoolSize,
        MaxActiveRuins,
        RuinCargoTypesMin,
        RuinCargoTypesMax,
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
        TerrainType,
        SeaLevel,
        Rivers,
        Smoothness,
        VarietyDistribution,
        NumberOfTowns,
        IndustryDensity,
        TownNames,
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
        EnableMilitaryItems,
        EnableSharkShips,
        EnableHoverVehicles,
        EnableHEQS,
        EnableVactrain,
        EnableAircraftpack,
        EnableFIRS,
        FIRSEconomy,
    ]),
    OptionGroup("Events", [
        ColbyEvent,
        EnableDemigods,
        DemigodCount,
        DemigodSpawnIntervalMin,
        DemigodSpawnIntervalMax,
    ]),
    OptionGroup("God of Wackens (Wrath)", [
        EnableWrath,
        WrathLimitHouses,
        WrathLimitRoads,
        WrathLimitTerrain,
    ]),
    OptionGroup("Multiplayer", [
        MultiplayerMode,
    ]),
    OptionGroup("Funny Stuff", [
        CommunityVehicleNames,
    ]),
    OptionGroup("Manual Unlock Toggles (IGNORED when Sphere Progression is ON)", [
        EnableRailDirectionUnlocks,
        EnableRoadDirectionUnlocks,
        EnableSignalUnlocks,
        EnableBridgeUnlocks,
        EnableTunnelUnlocks,
        EnableAirportUnlocks,
        EnableTerraformUnlocks,
        EnableWagonUnlocks,
        EnableTreeUnlocks,
        EnableTownActionUnlocks,
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
    # Progression
    enable_sphere_progression:       EnableSphereProgression
    mission_tier_unlock_count:       MissionTierUnlockCount
    mission_difficulty:              MissionDifficulty
    victory_vehicle_requirement:     VictoryVehicleRequirement
    hard_tier_vehicle_multiplier:    HardTierVehicleMultiplier
    extreme_tier_vehicle_multiplier: ExtremeTierVehicleMultiplier
    # Shop & Items
    utility_count:                   UtilityCount
    speed_boost_count:               SpeedBoostCount
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
    # Stars
    enable_stars:                    EnableStars
    star_pool_size:                  StarPoolSize
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
    terrain_type:                    TerrainType
    sea_level:                       SeaLevel
    rivers:                          Rivers
    smoothness:                      Smoothness
    variety:                         VarietyDistribution
    number_towns:                    NumberOfTowns
    industry_density:                IndustryDensity
    town_names:                      TownNames
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
    enable_military_items:           EnableMilitaryItems
    enable_shark_ships:              EnableSharkShips
    enable_hover_vehicles:           EnableHoverVehicles
    enable_heqs:                     EnableHEQS
    enable_vactrain:                 EnableVactrain
    enable_aircraftpack:             EnableAircraftpack
    enable_firs:                     EnableFIRS
    firs_economy:                    FIRSEconomy
    # Events
    colby_event:                     ColbyEvent
    enable_demigods:                 EnableDemigods
    demigod_count:                   DemigodCount
    demigod_spawn_interval_min:      DemigodSpawnIntervalMin
    demigod_spawn_interval_max:      DemigodSpawnIntervalMax
    # God of Wackens (Wrath)
    enable_wrath:                    EnableWrath
    wrath_limit_houses:              WrathLimitHouses
    wrath_limit_roads:               WrathLimitRoads
    wrath_limit_terrain:             WrathLimitTerrain
    # Multiplayer
    multiplayer_mode:                MultiplayerMode
    # Funny Stuff
    community_vehicle_names:         CommunityVehicleNames
    # Death Link
    death_link:                      OpenTTDDeathLink
