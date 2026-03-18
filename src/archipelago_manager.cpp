/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file archipelago_manager.cpp
 * Game-logic integration for the Archipelago randomizer.
 *
 * Engine unlock strategy:
 *   Vanilla OpenTTD releases vehicles to companies via NewVehicleAvailable()
 *   which is called from CalendarEnginesMonthlyLoop() when the current date
 *   passes an engine's intro_date.  In AP mode engine.cpp skips that call
 *   (AP_IsActive() returns true), so no engine ever becomes available through
 *   the normal date path.  Instead, when the AP server sends us an item we
 *   call EnableEngineForCompany() directly — the same internal function that
 *   NewVehicleAvailable() calls — which sets company_avail, updates railtypes,
 *   and refreshes all affected GUI windows.
 */

#include "stdafx.h"
#include <charconv>
#include "archipelago.h"
#include "archipelago_gui.h"
#include "base_media_graphics.h"
#include "base_media_sounds.h"
#include "base_media_music.h"
#include "engine_base.h"
#include "engine_cmd.h"
#include "engine_func.h"
#include "engine_type.h"
#include "company_func.h"
#include "company_base.h"
#include "company_cmd.h"
#include "vehicle_base.h"
#include "industry.h"
#include "town.h"
#include "town_type.h"
#include "town_cmd.h"
#include "command_func.h"
#include "misc_cmd.h"
#include "cargotype.h"
#include "core/random_func.hpp"
#include "rail.h"
#include "road_func.h"
#include "window_type.h"
#include "language.h"
#include "genworld.h"
#include "settings_type.h"
#include "newgrf_airport.h"
#include "object_base.h"
#include "object_map.h"
#include "debug.h"
#include "strings_func.h"
#include "news_type.h"
#include "news_func.h"
#include "statusbar_gui.h"
#include "string_func.h"
#include "table/strings.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_realtime.h"
#include "timer/timer.h"

#include <map>
#include <string>
#include <atomic>
#include <set>

#include "core/format.hpp"
#include "console_func.h"
#include "console_internal.h"
#include "window_func.h"
#include "gui.h"
#include "station_base.h"
#include "textbuf_gui.h"
#include "map_func.h"
#include "tile_map.h"
#include "cargomonitor.h"
#include "currency.h"
#include "signs_base.h"
#include "economy_func.h"
#include "network/network_func.h"   /* NetworkAddChatMessage — demigod chat */
#include "cargopacket.h"
#include "newgrf_config.h"
#include "newgrf_object.h"
#include "object_cmd.h"
#include "object_type.h"
#include "landscape.h"
#include "landscape_cmd.h"
#include "effectvehicle_func.h"
#include "terraform_cmd.h"
#include "gfxinit.h"
#include "fileio_func.h"
#include "network/network.h"
#include "network/network_func.h"
#include "saveload/saveload.h"
#include "fios.h"
#include "safeguards.h"

/* Bridge mode removed — variable kept as always-false for code that still references it. */
bool _ap_bridge_mode = false;

/* Console log helpers */
#define AP_LOG(msg)  IConsolePrint(CC_INFO,    "[AP] " + std::string(msg))
#define AP_OK(msg)   IConsolePrint(CC_WHITE,   "[AP] " + std::string(msg))
#define AP_WARN(msg) IConsolePrint(CC_WARNING, "[AP] WARNING: " + std::string(msg))
#define AP_ERR(msg)  IConsolePrint(CC_ERROR,   "[AP] ERROR: " + std::string(msg))

/** File-only trace log — writes to ap_console.log but NOT to the in-game
 *  console window.  Use for verbose diagnostics that would spam the player. */
static void AP_Trace(const std::string &msg)
{
	extern std::optional<FileHandle> _iconsole_output_file;
	if (_iconsole_output_file.has_value()) {
		try {
			FILE *f = *_iconsole_output_file;
			fmt::print(f, "{}{}\n", GetLogPrefix(), "[AP][TRACE] " + msg);
			fflush(f);
		} catch (const std::system_error &) {}
	}
}
#define AP_TRACE(msg) AP_Trace(msg)

/** Network-safe money change — uses CMD_CHANGE_BANK_BALANCE (deity).
 *  Propagates to all multiplayer clients via the command queue. */
static void AP_ChangeMoney(CompanyID cid, Money delta)
{
	AP_TRACE(fmt::format("ChangeMoney: company={} delta={}", cid.base(), (int64_t)delta));
	CompanyID old = _current_company;
	_current_company = OWNER_DEITY;
	Command<CMD_CHANGE_BANK_BALANCE>::Do(
		DoCommandFlags{DoCommandFlag::Execute},
		(TileIndex)0, delta, cid, EXPENSES_OTHER);
	_current_company = old;
}

/* -------------------------------------------------------------------------
 * Formatting helpers — currency respects player's chosen symbol/separator;
 * AP_Num adds thousand separators to plain numbers.
 * ---------------------------------------------------------------------- */
static std::string AP_Money(Money v) { return GetString(STR_JUST_CURRENCY_LONG, v); }
static std::string AP_Num(int64_t v) { return GetString(STR_JUST_COMMA, v); }

/* -------------------------------------------------------------------------
 * Cargo type lookup — maps AP cargo names (English) → CargoType index
 * Used by mission evaluation to check delivered_cargo[] by cargo type.
 * ---------------------------------------------------------------------- */

static std::map<std::string, CargoType> _ap_cargo_map;
static bool _ap_cargo_map_built = false;

/* -------------------------------------------------------------------------
 * Forward declarations — defined later in this file but needed by functions
 * that appear before their definition site.
 * ---------------------------------------------------------------------- */
static bool        _ap_towns_renamed     = false;
int                _ap_town_rename_mode  = 0;        ///< extern in archipelago_gui.cpp
std::string        _ap_town_custom_names;             ///< extern in archipelago_gui.cpp
static int64_t     _ap_items_received_count = 0;

static void BuildCargoMap()
{
	_ap_cargo_map.clear();

	/* Standard temperate cargo label constants from cargo_type.h */
	static const std::pair<const char *, CargoLabel> kNameLabel[] = {
		{ "passengers",  CT_PASSENGERS },
		{ "coal",        CT_COAL       },
		{ "mail",        CT_MAIL       },
		{ "oil",         CT_OIL        },
		{ "livestock",   CT_LIVESTOCK  },
		{ "goods",       CT_GOODS      },
		{ "grain",       CT_GRAIN      },
		{ "wood",        CT_WOOD       },
		{ "iron ore",    CT_IRON_ORE   },
		{ "steel",       CT_STEEL      },
		{ "valuables",   CT_VALUABLES  },
		/* Arctic/tropical extras (no-ops in temperate, safe to map anyway) */
		{ "wheat",       CT_WHEAT      },
		{ "paper",       CT_PAPER      },
		{ "gold",        CT_GOLD       },
		{ "food",        CT_FOOD       },
		{ "rubber",      CT_RUBBER     },
		{ "fruit",       CT_FRUIT      },
		{ "maize",       CT_MAIZE      },
		{ "copper ore",  CT_COPPER_ORE },
		{ "water",       CT_WATER      },
		{ "diamonds",    CT_DIAMONDS   },
		/* Toyland-only cargos — missing from this table caused AP_FindCargoType()
		 * to return INVALID_CARGO, making Toyland transport missions fall back
		 * to counting ALL cargo types instead of the specific one. */
		{ "sugar",       CT_SUGAR        },
		{ "toys",        CT_TOYS         },
		{ "batteries",   CT_BATTERIES    },
		{ "sweets",      CT_CANDY        },   /* in-game name "Sweets", label CT_CANDY */
		{ "toffee",      CT_TOFFEE       },
		{ "cola",        CT_COLA         },
		{ "candyfloss",  CT_COTTON_CANDY },   /* in-game name "Candyfloss", label CT_COTTON_CANDY */
		{ "bubbles",     CT_BUBBLES      },
		{ "plastic",     CT_PLASTIC      },
		{ "fizzy drinks",CT_FIZZY_DRINKS },
	};

	for (auto &[name, label] : kNameLabel) {
		CargoType ct = GetCargoTypeByLabel(label);
		if (IsValidCargoType(ct)) {
			_ap_cargo_map[name] = ct;
		}
	}
	_ap_cargo_map_built = true;
}

static CargoType AP_FindCargoType(const std::string &name)
{
	if (name.empty()) return INVALID_CARGO;
	if (!_ap_cargo_map_built) BuildCargoMap();
	std::string lower = name;
	for (char &c : lower) c = (char)tolower((unsigned char)c);
	auto it = _ap_cargo_map.find(lower);
	return (it != _ap_cargo_map.end()) ? it->second : INVALID_CARGO;
}

/* -------------------------------------------------------------------------
 * Town renaming — renames map towns using multiworld player names or a
 * user-supplied custom list.  Called once on first game tick of a new world.
 * ---------------------------------------------------------------------- */

/** Parse a comma-separated name string into a deduplicated vector. */
static std::vector<std::string> SplitNames(const std::string &csv)
{
	std::vector<std::string> result;
	std::set<std::string>    seen;
	std::string cur;
	for (char c : csv) {
		if (c == ',') {
			/* trim whitespace */
			size_t s = cur.find_first_not_of(" \t");
			size_t e = cur.find_last_not_of(" \t");
			if (s != std::string::npos) {
				std::string trimmed = cur.substr(s, e - s + 1);
				if (!trimmed.empty() && seen.insert(trimmed).second)
					result.push_back(trimmed);
			}
			cur.clear();
		} else {
			cur += c;
		}
	}
	/* last segment */
	size_t s = cur.find_first_not_of(" \t");
	size_t e = cur.find_last_not_of(" \t");
	if (s != std::string::npos) {
		std::string trimmed = cur.substr(s, e - s + 1);
		if (!trimmed.empty() && seen.insert(trimmed).second)
			result.push_back(trimmed);
	}
	return result;
}

/**
 * Rename map towns using either multiworld player names (mode 0) or a
 * comma-separated custom list (mode 1).  Mode 2 = off, do nothing.
 *
 * Each town gets at most one name.  If there are more towns than names,
 * the remaining towns keep their generated names.  Duplicate names are
 * skipped automatically.  Called once per new session; _ap_towns_renamed
 * guards against re-running on save/load.
 */
static void AP_RenameTowns()
{
	if (_ap_towns_renamed) return;         /* already done for this world */
	if (_ap_town_rename_mode == 2) {       /* OFF */
		_ap_towns_renamed = true;
		return;
	}

	/* Build name list */
	std::vector<std::string> names;
	if (_ap_town_rename_mode == 1) {
		/* Custom mode — use the user-supplied comma-separated string */
		names = SplitNames(_ap_town_custom_names);
	} else {
		/* Multiworld mode — fetch player aliases from the AP client */
		if (_ap_client != nullptr) {
			names = _ap_client->GetPlayerNames();
		}
	}

	if (names.empty()) {
		_ap_towns_renamed = true;
		return;
	}

	/* Collect all towns in a stable order (by TownID) */
	std::vector<Town *> towns;
	for (Town *t : Town::Iterate()) towns.push_back(t);
	/* Sort by index for deterministic assignment */
	std::sort(towns.begin(), towns.end(), [](const Town *a, const Town *b) {
		return (int)a->index.base() < (int)b->index.base();
	});

	/* Shuffle names array (InteractiveRandom — safe for MP, won't desync game RNG) */
	for (int i = (int)names.size() - 1; i > 0; i--) {
		int j = (int)(InteractiveRandomRange((uint32_t)(i + 1)));
		std::swap(names[i], names[j]);
	}

	/* Rename towns, one name per town, stop when we run out of names */
	int applied = 0;
	for (Town *t : towns) {
		if (applied >= (int)names.size()) break;
		const std::string &new_name = names[applied];

		/* CmdRenameTown requires DoCommandFlag::Execute and deity permission */
		CmdRenameTown(DoCommandFlags{DoCommandFlag::Execute}, t->index, new_name);
		applied++;
	}

	_ap_towns_renamed = true;
	AP_OK(fmt::format("Renamed {} town(s) using {} names.",
	      applied, (int)names.size()));
}

/** APST accessors for save/load */
bool AP_GetTownsRenamed()       { return _ap_towns_renamed; }
void AP_SetTownsRenamed(bool v) { _ap_towns_renamed = v; }

/* -------------------------------------------------------------------------
 * Session statistics — cumulative cargo delivery and profit accumulators.
 *
 * OpenTTD stores per-period stats in old_economy[0] (last completed period)
 * and cur_economy (current ongoing period).  We detect when old_economy[0]
 * refreshes (= a new financial period ended) and add the values to our
 * running totals so we can track "total cargo delivered this session".
 * ---------------------------------------------------------------------- */

static uint64_t _ap_cumul_cargo[NUM_CARGO]      = {};  ///< Cargo delivered in completed periods
static Money    _ap_cumul_profit                = 0;   ///< Profit earned in completed periods
static bool     _ap_stats_initialized          = false;
static bool     _ap_shop_purchased             = false; ///< True if player bought from shop this session
static std::set<std::string> _ap_sent_shop_locations; ///< Shop locations already sent to AP server

/** Snapshot of last-seen old_economy[0] values (for change detection) */
static uint32_t _ap_snap_cargo[NUM_CARGO]       = {};
static Money    _ap_snap_profit                 = 0;

static void AP_InitSessionStats()
{
	for (CargoType i = 0; i < NUM_CARGO; i++) { _ap_cumul_cargo[i] = 0; _ap_snap_cargo[i] = 0; }
	_ap_cumul_profit           = 0;
	_ap_snap_profit            = 0;
	_ap_stats_initialized      = false;
	_ap_shop_purchased         = false;
	_ap_items_received_count   = 0;   /* Reset for new games; loaded games restore from APST */
	_ap_sent_shop_locations.clear();
	_ap_towns_renamed          = false; /* Reset for new games; loaded games restore from APST */
	if (!_ap_cargo_map_built) BuildCargoMap();
}

/**
 * Called every ~5 s from the realtime timer.
 * If old_economy[0] has changed since last call, accumulate the values.
 */
static void AP_UpdateSessionStats()
{
	CompanyID cid = _local_company;
	const Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;

	if (!_ap_stats_initialized) {
		/* Take initial snapshot so the first "change" doesn't double-count */
		for (CargoType i = 0; i < NUM_CARGO; i++)
			_ap_snap_cargo[i] = c->old_economy[0].delivered_cargo[i];
		_ap_snap_profit       = c->old_economy[0].income + c->old_economy[0].expenses;
		_ap_stats_initialized = true;
		return;
	}

	/* Detect if a new financial period has ended (old_economy[0] changed) */
	bool refreshed = false;
	for (CargoType i = 0; i < NUM_CARGO; i++) {
		if (c->old_economy[0].delivered_cargo[i] != _ap_snap_cargo[i]) {
			refreshed = true;
			break;
		}
	}
	if (!refreshed) {
		Money new_p = c->old_economy[0].income + c->old_economy[0].expenses;
		if (new_p != _ap_snap_profit) refreshed = true;
	}

	if (refreshed) {
		for (CargoType i = 0; i < NUM_CARGO; i++) {
			_ap_cumul_cargo[i] += c->old_economy[0].delivered_cargo[i];
			_ap_snap_cargo[i]   = c->old_economy[0].delivered_cargo[i];
		}
		Money period_profit = c->old_economy[0].income + c->old_economy[0].expenses;
		if (period_profit > 0) _ap_cumul_profit += period_profit;  /* only add profitable periods; losses don't reduce progress */
		_ap_snap_profit   = period_profit;
		Debug(misc, 1, "[AP] Economy period snapped. Cumulative profit: £{}", (int64_t)_ap_cumul_profit);
	}
}

/**
 * Get total cargo delivered of a specific type: completed periods + current period.
 */
static uint64_t AP_GetTotalCargo(CargoType ct)
{
	if (ct == INVALID_CARGO || ct >= NUM_CARGO) return 0;
	const Company *c = Company::GetIfValid(_local_company);
	uint64_t cur = (c != nullptr) ? (uint64_t)c->cur_economy.delivered_cargo[ct] : 0;
	return _ap_cumul_cargo[ct] + cur;
}

/**
 * Get total profit this session: completed periods + current period.
 */
static Money AP_GetTotalProfit()
{
	const Company *c = Company::GetIfValid(_local_company);
	Money cur_raw = (c != nullptr) ? Money(c->cur_economy.income + c->cur_economy.expenses) : Money(0);
	Money cur = cur_raw > 0 ? cur_raw : Money(0);  /* don't count current period if it's a loss */
	Money total = _ap_cumul_profit + cur;
	return total > 0 ? total : Money(0);
}

/* -------------------------------------------------------------------------
 * Mission evaluation — called every ~5 s from the realtime timer.
 * For each incomplete mission, compute the current progress value and
 * check if the mission goal is met.  When met, send the location check
 * to the AP server and mark it completed.
 * ---------------------------------------------------------------------- */

/**
 * Count distinct towns served by at least one station owned by local company.
 */
static int AP_CountServicedTowns()
{
	CompanyID cid = _local_company;
	std::set<const Town *> towns;
	for (const Station *st : Station::Iterate()) {
		if (st->owner == cid && st->town != nullptr) {
			towns.insert(st->town);
		}
	}
	return (int)towns.size();
}

/**
 * Count real stations (not waypoints) owned by local company.
 */
/**
 * Count real stations (not waypoints) owned by local company.
 *
 * @param filter_facility  If true, only count stations with the given facility.
 * @param facility         Required facility (only used when filter_facility=true).
 * @param require_active   If true, only count stations that have received cargo.
 *
 * A station is "active" if at least one cargo type has EVER been delivered
 * to it for final delivery (GoodsEntry::State::EverAccepted).
 */
static int AP_CountStations(bool filter_facility = false, StationFacility facility = StationFacility::Train, bool require_active = false)
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Station *st : Station::Iterate()) {
		if (st->owner != cid) continue;
		if (filter_facility && !st->facilities.Test(facility)) continue;
		if (require_active) {
			bool any_delivered = false;
			for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
				if (st->goods[ct].status.Test(GoodsEntry::State::EverAccepted)) {
					any_delivered = true;
					break;
				}
			}
			if (!any_delivered) continue;
		}
		count++;
	}
	return count;
}

/**
 * Count active primary vehicles of a given type owned by local company.
 *
 * A vehicle is "active" if:
 *  1. It is a primary vehicle (not a wagon/trailer)
 *  2. Its calendar age >= 30 days (has been running for at least ~1 month)
 *  3. It has earned money (profit_this_year > 0 OR profit_last_year > 0),
 *     meaning it has completed at least one cargo delivery.
 *
 * @param type  Vehicle type filter. VEH_INVALID = all types.
 */
static int AP_CountActiveVehicles(VehicleType type)
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner != cid) continue;
		if (!v->IsPrimaryVehicle()) continue;
		if (type != VEH_INVALID && v->type != type) continue;
		/* Must have been around for at least 30 calendar days */
		if (v->age.base() < 30) continue;
		/* Must have made at least one delivery (earned money) */
		if (v->profit_this_year <= 0 && v->profit_last_year <= 0) continue;
		count++;
	}
	return count;
}


/**
 * Determine vehicle type to count based on mission unit field.
 * "trains"        → VEH_TRAIN
 * "aircraft"      → VEH_AIRCRAFT
 * "road vehicles" → VEH_ROAD
 * "ships"         → VEH_SHIP
 * anything else   → VEH_INVALID (= all types)
 */
static VehicleType AP_VehicleTypeFromUnit(const std::string &unit)
{
	if (unit == "trains")        return VEH_TRAIN;
	if (unit == "aircraft")      return VEH_AIRCRAFT;
	if (unit == "road vehicles") return VEH_ROAD;
	if (unit == "ships")         return VEH_SHIP;
	return VEH_INVALID;
}

/**
 * Evaluate a single mission and return the current progress value.
 * Returns true if the mission goal is now met.
 */
static bool EvaluateMission(APMission &m)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return false;

	/* Safety: a mission with amount <= 0 would auto-complete immediately.
	 * This should never happen after the Python fix, but guard here too. */
	if (m.amount <= 0) {
		Debug(misc, 1, "[AP] Mission '{}' has amount<=0, skipping evaluation", m.location);
		return false;
	}

	int64_t current = 0;

	/* ── "transport" type ──────────────────────────────────────────────
	 * "Transport {amount} units of {cargo}"
	 * "Transport {amount} passengers"
	 * Uses cumulative cargo delivered this session. */
	if (m.type == "transport") {
		/* If unit is "passengers" or cargo field is empty, use CT_PASSENGERS */
		CargoType ct = INVALID_CARGO;
		if (m.unit == "passengers" || m.cargo.empty()) {
			ct = AP_FindCargoType("passengers");
		} else {
			ct = AP_FindCargoType(m.cargo);
		}
		if (IsValidCargoType(ct)) {
			current = (int64_t)AP_GetTotalCargo(ct);
		} else {
			/* Fallback: sum all cargo types */
			uint64_t total = 0;
			for (CargoType i = 0; i < NUM_CARGO; i++) total += AP_GetTotalCargo(i);
			current = (int64_t)total;
		}
	}

	/* ── "earn" type ────────────────────────────────────────────────────
	 * Python now normalizes the type_key to:
	 *   "earn monthly"  for "Earn £X in one month"
	 *   "earn"          for "Earn £X total profit"
	 * Also handle legacy UTF-8 £ keys for old saves. */
	else if (m.type == "earn monthly" ||
	         m.unit == "\xC2\xA3/month" || m.unit == "£/month") {
		/* Monthly: income – expenses of last completed period.
		 * Guard against negative (startup costs showing as "earned") */
		int64_t monthly_net = (int64_t)(c->old_economy[0].income + c->old_economy[0].expenses);
		current = monthly_net > 0 ? monthly_net : 0;
	}
	else if (m.type == "earn" ||
	         m.type == "earn \xC2\xA3" ||
	         m.type == "earn (total)" || m.type == "earn total") {
		/* Cumulative total profit */
		current = (int64_t)AP_GetTotalProfit();
	}

	/* ── "have" type ───────────────────────────────────────────────────
	 * "Have {amount} active trains/road vehicles/ships/aircraft/vehicles"
	 * Only counts vehicles that have been running ≥30 days AND made a delivery.
	 * Handles both old "have" key and direct type_key strings from the pool. */
	else if (m.type == "have" ||
	         m.type == "trains" || m.type == "road vehicles" ||
	         m.type == "ships"  || m.type == "aircraft" || m.type == "vehicles") {
		/* For direct type_key strings, derive VehicleType from type itself */
		VehicleType vtype = VEH_INVALID;
		if      (m.type == "trains")        vtype = VEH_TRAIN;
		else if (m.type == "road vehicles") vtype = VEH_ROAD;
		else if (m.type == "ships")         vtype = VEH_SHIP;
		else if (m.type == "aircraft")      vtype = VEH_AIRCRAFT;
		else /* "have" legacy: derive from unit field */
			vtype = AP_VehicleTypeFromUnit(m.unit);
		current = (int64_t)AP_CountActiveVehicles(vtype);
	}

	/* ── "service" type ────────────────────────────────────────────────
	 * "Service {amount} different towns" */
	else if (m.type == "service towns") {
		current = (int64_t)AP_CountServicedTowns();
	}
	/* legacy key emitted by older APWorld versions */
	else if (m.type == "service") {
		current = (int64_t)AP_CountServicedTowns();
	}

	/* ── "build" / station-type missions ───────────────────────────────
	 * "Build {amount} active train stations / bus stops / harbours / airports"
	 * Only counts stations that have received at least one delivery. */
	else if (m.type == "build stations") {
		current = (int64_t)AP_CountStations(false, StationFacility::Train, true);
	}
	else if (m.type == "train stations") {
		current = (int64_t)AP_CountStations(true, StationFacility::Train, true);
	}
	else if (m.type == "bus stops") {
		current = (int64_t)AP_CountStations(true, StationFacility::BusStop, true);
	}
	else if (m.type == "truck stops") {
		current = (int64_t)AP_CountStations(true, StationFacility::TruckStop, true);
	}
	else if (m.type == "harbours") {
		current = (int64_t)AP_CountStations(true, StationFacility::Dock, true);
	}
	else if (m.type == "airports") {
		current = (int64_t)AP_CountStations(true, StationFacility::Airport, true);
	}
	/* legacy generic "build" key */
	else if (m.type == "build") {
		current = (int64_t)AP_CountStations(false, StationFacility::Train, true);
	}

	/* ── "deliver" / "deliver goods" / "deliver to station" types ──────
	 * "Deliver {amount} tons of {cargo} to one station"
	 * "Deliver {amount} tons of goods in one year"
	 * Approximated by cumulative cargo of that type. */
	else if (m.type == "deliver" || m.type == "deliver goods" || m.type == "deliver to station") {
		CargoType ct = AP_FindCargoType(m.cargo);
		if (IsValidCargoType(ct)) {
			current = (int64_t)AP_GetTotalCargo(ct);
		} else {
			/* "goods in one year" — check last period's goods delivery */
			CargoType goods_ct = AP_FindCargoType("goods");
			if (IsValidCargoType(goods_ct))
				current = (int64_t)c->old_economy[0].delivered_cargo[goods_ct];
		}
	}

	/* ── "connect" / "cities" types ────────────────────────────────────
	 * "Connect {amount} cities with rail" */
	else if (m.type == "connect" || m.type == "cities") {
		CompanyID cid = _local_company;
		std::set<const Town *> rail_towns;
		for (const Station *st : Station::Iterate()) {
			if (st->owner == cid && st->town != nullptr &&
			    st->facilities.Test(StationFacility::Train)) {
				rail_towns.insert(st->town);
			}
		}
		current = (int64_t)rail_towns.size();
	}

	/* ── "passengers" / "transport cargo" types ────────────────────────
	 * Pool emits type_key "passengers" and "transport cargo" directly. */
	else if (m.type == "passengers") {
		CargoType ct = AP_FindCargoType("passengers");
		if (IsValidCargoType(ct)) current = (int64_t)AP_GetTotalCargo(ct);
	}
	else if (m.type == "transport cargo") {
		CargoType ct = AP_FindCargoType(m.cargo);
		if (IsValidCargoType(ct)) {
			current = (int64_t)AP_GetTotalCargo(ct);
		} else {
			uint64_t total = 0;
			for (CargoType i = 0; i < NUM_CARGO; i++) total += AP_GetTotalCargo(i);
			current = (int64_t)total;
		}
	}

	/* ── "maintain_75" type ────────────────────────────────────────────
	 * Python emits normalized type "maintain_75".
	 * m.amount = number of consecutive months required.
	 * m.maintain_months_ok = counter managed by the monthly calendar timer.
	 * Legacy: also accept old raw template-prefix strings from saves <= beta 8. */
	else if (m.type == "maintain_75" ||
	         m.type.find("maintain") != std::string::npos) {
		current = (int64_t)m.maintain_months_ok;
	}

	/* ── "buy any item from the shop" ─────────────────────────────────
	 * Triggered when player buys anything from the AP shop. */
	else if (m.type == "buy any item from the shop" || m.type == "buy a vehicle from the shop" || m.type == "buy") {
		current = _ap_shop_purchased ? 1 : 0;
	}

	/* ── named-destination missions ────────────────────────────────────
	 * Progress is accumulated in real-time (every 250 ms) by AP_UpdateNamedMissions(). */
	else if (m.type == "passengers_to_town" || m.type == "mail_to_town" ||
	         m.type == "cargo_from_industry" || m.type == "cargo_to_industry") {
		current = (int64_t)m.named_entity.cumulative;
	}

	/* Update live progress on the mission (visible in missions window) */
	m.current_value = current;

	return current >= m.amount;
}

/* -------------------------------------------------------------------------
 * Stored connection credentials (for reconnect)
 * ---------------------------------------------------------------------- */

std::string _ap_last_host;
uint16_t    _ap_last_port     = 38281;
std::string _ap_last_slot;
std::string _ap_last_pass;
bool        _ap_last_ssl = false;

/* -------------------------------------------------------------------------
 * Town rename settings — chosen in the connect window before generating a world
 * ---------------------------------------------------------------------- */

/* NOTE: _ap_town_rename_mode, _ap_town_custom_names, _ap_towns_renamed are
 * declared at the top of this file (near _ap_cargo_map) so they are available
 * to AP_RenameTowns() and AP_InitSessionStats() which appear earlier. */

/* -------------------------------------------------------------------------
 * Engine name <-> EngineID map
 * Built once when we enter GM_NORMAL for the first time in a session.
 * Used only for name-based lookups when AP sends us an item.
 * ---------------------------------------------------------------------- */

static std::map<std::string, EngineID> _ap_engine_map;
/* Extra engines that share a name with an already-mapped engine (e.g. the three
 * "Oil Tanker" wagon variants: Rail, Monorail, Maglev).  The primary map stores
 * the first match; extras stores all subsequent ones so they all get unlocked. */
static std::map<std::string, std::vector<EngineID>> _ap_engine_extras;
static bool _ap_engine_map_built = false;

/** EngineIDs that AP has explicitly unlocked for the local company this session.
 *  The periodic re-lock sweep uses this to distinguish "AP-unlocked" from
 *  "re-introduced by StartupEngines()" — only engines in this set survive. */
static std::set<EngineID> _ap_unlocked_engine_ids;

/** Public API: check if a specific engine has been unlocked via AP items. */
bool AP_IsEngineUnlocked(uint32_t engine_id)
{
	return _ap_unlocked_engine_ids.count((EngineID)engine_id) > 0;
}

/* ── Track direction locks ──────────────────────────────────────────────── *
 * One bitmask per rail type (bits 0-5 = TRACK_X..TRACK_RIGHT).
 * AP index 0-3 = vanilla (Normal, Electric, Monorail, Maglev).
 * AP index 4-6 = NewGRF (Narrow Gauge/IH, Metro/IH, Vacuum Tube/Vactrain).
 * NewGRF types use runtime-detected RailType IDs mapped to AP indices.
 * When enable_rail_direction_unlocks is true in slot_data, all bytes are set
 * to 0x3F (all 6 bits) at session start. Each received item clears one bit.
 * Array is fully unlocked (all zeros) when option is disabled.
 */
static constexpr int AP_NUM_RAILTYPES = 7;  /* 4 vanilla + NG + Metro + VacTube */
static uint8_t _ap_locked_track_dirs[AP_NUM_RAILTYPES] = {};
/** True once session init has set the initial lock state.
 *  Reset to false by AP_OnSlotData so reconnects don't re-lock
 *  directions that were already restored from the savegame. */
static bool _ap_track_dirs_inited = false;

/* Discovered NewGRF rail type IDs (runtime — set by AP_DetectNewGRFRailTypes).
 * 0xFF = not present (GRF not loaded). */
static uint8_t _ap_ng_rt_id    = 0xFF;  /* Narrow Gauge — IH label 'NAAN' */
static uint8_t _ap_metro_rt_id = 0xFF;  /* Metro — IH label 'MTRO' */
static uint8_t _ap_vact_rt_id  = 0xFF;  /* Vacuum Tube — Vactrain label 'VACT' */

/** Map a runtime RailType to our fixed AP lock array index.
 *  Returns 0xFF for unknown/unhandled types (not locked). */
static uint8_t AP_MapRailTypeToIndex(uint8_t rt)
{
	if (rt < 4) return rt;                  /* vanilla */
	if (rt == _ap_ng_rt_id)    return 4;    /* Narrow Gauge */
	if (rt == _ap_metro_rt_id) return 5;    /* Metro */
	if (rt == _ap_vact_rt_id)  return 6;    /* Vacuum Tube */
	return 0xFF;
}

/** Detect NewGRF rail type IDs by scanning loaded GRF labels.
 *  Must be called AFTER GRFs are loaded (i.e. at session start, not before map gen). */
static void AP_DetectNewGRFRailTypes()
{
	RailType ng = GetRailTypeByLabel('NAAN');
	_ap_ng_rt_id = (ng != INVALID_RAILTYPE) ? (uint8_t)ng : 0xFF;

	RailType metro = GetRailTypeByLabel('MTRO');
	_ap_metro_rt_id = (metro != INVALID_RAILTYPE) ? (uint8_t)metro : 0xFF;

	RailType vact = GetRailTypeByLabel('VACT');
	_ap_vact_rt_id = (vact != INVALID_RAILTYPE) ? (uint8_t)vact : 0xFF;

	Debug(misc, 1, "[AP] NewGRF rail types detected: NG={} Metro={} VacTube={}",
	      (int)_ap_ng_rt_id, (int)_ap_metro_rt_id, (int)_ap_vact_rt_id);
}

/* Road direction locks: bit 0=X (NE-SW), bit 1=Y (NW-SE) */
static uint8_t _ap_locked_road_dirs = 0;
static bool    _ap_road_dirs_inited = false;

/* Tram direction locks: bit 0=X (NE-SW), bit 1=Y (NW-SE) */
static uint8_t _ap_locked_tram_dirs = 0;
static bool    _ap_tram_dirs_inited = false;

/* Signal locks: bits 0-5 map to SignalType enum */
static uint8_t _ap_locked_signals = 0;
static bool    _ap_signals_inited = false;

/* Bridge locks: bits 0-12 map to BridgeType */
static uint16_t _ap_locked_bridges = 0;
static bool     _ap_bridges_inited = false;

/* Tunnel lock: single bool */
static bool _ap_locked_tunnels = false;
static bool _ap_tunnels_inited = false;

/* Airport locks: bits 0-8 map to AirportTypes; bit 0 (AT_SMALL) always free */
static uint16_t _ap_locked_airports = 0;
static bool     _ap_airports_inited = false;

/* Tree locks: bits 0-9 map to tree packs */
static uint16_t _ap_locked_trees = 0;
static bool     _ap_trees_inited = false;

/* Terraform locks: bit 0=Raise, bit 1=Lower */
static uint8_t _ap_locked_terraform = 0;
static bool    _ap_terraform_inited = false;

/* Town Authority action locks: bits 0-7 map to TownAction enum */
static uint8_t _ap_locked_town_actions = 0;
static bool    _ap_town_actions_inited = false;

/* Iron Horse wagon bundle: IH wagons have no AP items, so when wagon
 * unlocks are enabled we piggyback them onto IH engine unlocks.
 * Each IH engine unlock auto-unlocks a proportional batch of IH wagons. */
static std::vector<EngineID> _ap_ih_wagon_queue;   ///< All IH wagon EngineIDs (sorted by ID)
static uint32_t              _ap_ih_grfid = 0;      ///< Iron Horse GRF ID (learned at runtime)
static int                   _ap_ih_engines_total = 0;
static bool                  _ap_ih_wagons_built = false;
static bool                  _ap_ih_wagon_unlocks = false; ///< Cached from slot data (set in AP_OnSlotData)

/* ── Ruin Objects ──────────────────────────────────────────────── */
/* GRFID "APRU" — stored little-endian as read by ReadDWord():
 *   file bytes  41 50 52 55  →  LE uint32  0x55525041              */
static constexpr uint32_t AP_RUINS_GRFID = 0x55525041;
static constexpr int AP_NUM_RUIN_TYPES = 6;

/* Cached ObjectType indices for the 6 ruin types (resolved after GRF load). */
static ObjectType _ap_ruin_types[AP_NUM_RUIN_TYPES] = {
	INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE,
	INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE
};
static bool _ap_ruin_types_resolved = false;

/* ── Star Objects ──────────────────────────────────────────────── */
/* GRFID "APST" — stored little-endian as read by ReadDWord():
 *   file bytes  41 50 53 54  →  LE uint32  0x54535041              */
static constexpr uint32_t AP_STARS_GRFID = 0x54535041;
static constexpr int AP_NUM_STAR_TYPES = 3;

/* Cached ObjectType indices for the 3 star types (resolved after GRF load). */
static ObjectType _ap_star_types[AP_NUM_STAR_TYPES] = {
	INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE, INVALID_OBJECT_TYPE
};
static bool _ap_star_types_resolved = false;
static std::vector<APStar> _ap_stars;
static int _ap_stars_collected = 0;
static bool _ap_stars_initial_spawned = false;

/** Resolve star ObjectTypes by scanning ObjectSpecs for our GRFID.
 *  Called once after world generation when the GRF is loaded. */
static void AP_ResolveStarTypes()
{
	if (_ap_star_types_resolved) return;
	_ap_star_types_resolved = true;

	for (int i = 0; i < AP_NUM_STAR_TYPES; i++) _ap_star_types[i] = INVALID_OBJECT_TYPE;

	int found = 0;
	const auto &specs = ObjectSpec::Specs();
	for (size_t idx = NEW_OBJECT_OFFSET; idx < specs.size(); idx++) {
		const ObjectSpec &s = specs[idx];
		if (!s.IsEnabled()) continue;
		if (s.grf_prop.grfid != AP_STARS_GRFID) continue;
		uint16_t local_id = s.grf_prop.local_id;
		if (local_id < AP_NUM_STAR_TYPES) {
			_ap_star_types[local_id] = static_cast<ObjectType>(idx);
			found++;
			AP_LOG(fmt::format("Resolved star type {} → ObjectType {}", local_id, idx));
		}
	}
	if (found == 0) {
		AP_WARN(fmt::format("No star ObjectTypes found! GRFID=0x{:08X} not in loaded GRFs", AP_STARS_GRFID));
	} else {
		AP_OK(fmt::format("Resolved {}/{} star ObjectTypes", found, AP_NUM_STAR_TYPES));
	}
}

/** Check if a tile is near a building (house, industry, station, non-AP object).
 *  Used to ensure stars spawn in wilderness, not near towns. */
static bool AP_IsTileNearBuilding(TileIndex center, int radius)
{
	int cx = (int)TileX(center);
	int cy = (int)TileY(center);
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			int tx = cx + dx, ty = cy + dy;
			if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) continue;
			TileIndex t = TileXY(tx, ty);
			if (IsTileType(t, MP_HOUSE)) return true;
			if (IsTileType(t, MP_INDUSTRY)) return true;
			if (IsTileType(t, MP_STATION)) return true;
			/* Allow AP objects (stars, ruins), block all other objects */
			if (IsTileType(t, MP_OBJECT)) {
				ObjectType ot = GetObjectType(t);
				bool is_ap = false;
				for (int i = 0; i < AP_NUM_STAR_TYPES; i++) if (ot == _ap_star_types[i]) is_ap = true;
				for (int i = 0; i < AP_NUM_RUIN_TYPES; i++) if (ot == _ap_ruin_types[i]) is_ap = true;
				if (!is_ap) return true;
			}
		}
	}
	return false;
}

/** Resolve ruin ObjectTypes by scanning ObjectSpecs for our GRFID.
 *  Called once after world generation when the GRF is loaded. */
static void AP_ResolveRuinTypes()
{
	if (_ap_ruin_types_resolved) return;
	_ap_ruin_types_resolved = true;

	for (int i = 0; i < AP_NUM_RUIN_TYPES; i++) _ap_ruin_types[i] = INVALID_OBJECT_TYPE;

	int found = 0;
	const auto &specs = ObjectSpec::Specs();
	Debug(misc, 1, "[AP] ResolveRuinTypes: scanning {} object specs (NEW_OBJECT_OFFSET={})",
	      specs.size(), NEW_OBJECT_OFFSET);
	for (size_t idx = NEW_OBJECT_OFFSET; idx < specs.size(); idx++) {
		const ObjectSpec &s = specs[idx];
		if (!s.IsEnabled()) continue;
		if (s.grf_prop.grfid != AP_RUINS_GRFID) continue;
		uint16_t local_id = s.grf_prop.local_id;
		if (local_id < AP_NUM_RUIN_TYPES) {
			_ap_ruin_types[local_id] = static_cast<ObjectType>(idx);
			found++;
			AP_LOG(fmt::format("Resolved ruin type {} → ObjectType {}", local_id, idx));
		}
	}
	if (found == 0) {
		AP_WARN(fmt::format("No ruin ObjectTypes found! GRFID=0x{:08X} not in loaded GRFs", AP_RUINS_GRFID));
	} else {
		AP_OK(fmt::format("Resolved {}/{} ruin ObjectTypes", found, AP_NUM_RUIN_TYPES));
	}
}

/** Place a ruin object on a random valid tile.
 *  @param ruin_index 0..5 (matches NML item IDs)
 *  @return true if successfully placed */
static bool AP_PlaceRuin(int ruin_index)
{
	AP_ResolveRuinTypes();

	if (ruin_index < 0 || ruin_index >= AP_NUM_RUIN_TYPES) return false;
	ObjectType ot = _ap_ruin_types[ruin_index];
	if (ot == INVALID_OBJECT_TYPE) {
		AP_WARN(fmt::format("Ruin type {} not resolved — GRF missing?", ruin_index));
		return false;
	}

	/* Try random tiles until we find one that works */
	for (int attempt = 0; attempt < 5000; attempt++) {
		TileIndex tile = RandomTile();

		/* Quick pre-checks: flat, clear land, not too close to edge */
		if (!IsValidTile(tile)) continue;
		if (TileX(tile) < 2 || TileY(tile) < 2) continue;
		if (TileX(tile) >= Map::SizeX() - 2 || TileY(tile) >= Map::SizeY() - 2) continue;

		/* Try to build the object (Deity command) */
		CompanyID old_company = _current_company;
		_current_company = OWNER_DEITY;
		CommandCost res = Command<CMD_BUILD_OBJECT>::Do(
			DoCommandFlags{DoCommandFlag::Execute},
			tile, ot, 0);
		_current_company = old_company;

		if (res.Succeeded()) {
			AP_OK(fmt::format("Ruin placed at tile {}", tile));
			return true;
		}
	}

	AP_WARN(fmt::format("Failed to place ruin type {} after 5000 attempts", ruin_index));
	return false;
}

/* ── Star Placement — forward declarations (defined after _ap_pending_sd) ── */
static bool AP_PlaceSingleStar(APStar &star);
static void AP_InitStarEntries();
static int  AP_PlaceStarBatch(int batch_size);

/* ── Ruin Gameplay System ─────────────────────────────────────────── */
/* Active ruins on the map, requiring cargo delivery to clear.        */
static std::vector<APRuin> _ap_active_ruins;
static int _ap_ruins_spawned   = 0;  ///< Total spawned this session
static int _ap_ruins_completed = 0;  ///< Total cleared this session
static int _ap_ruin_cooldown_months = 0; ///< Months until next spawn allowed
static constexpr int AP_RUIN_SPAWN_COOLDOWN = 6; ///< Months between new ruin spawns
static bool _ap_ruins_initial_spawned = false; ///< Have we done the initial batch?

/* Forward declarations for ruin functions (defined after _ap_pending_sd) */
static bool AP_SpawnRuin();
static void AP_UpdateRuinProgress(CompanyID cid, std::set<CargoMonitorID> &drained);
static void AP_RuinMonthlyTick();

/**
 * Check if a tile is an active (non-completed) ruin and fill cargo acceptance.
 * Called from object_cmd.cpp AddAcceptedCargo_Object so stations near ruins
 * correctly show "Accepts: Coal, Goods" etc. and cargo can actually be delivered.
 */
bool AP_GetRuinCargoAcceptance(uint32_t tile_index, CargoArray &acceptance, CargoTypes &always_accepted)
{
	/* Resolve to north tile for multi-tile objects */
	TileIndex ti{tile_index};
	uint32_t north_tile = tile_index;
	if (IsValidTile(ti) && IsTileType(ti, MP_OBJECT)) {
		Object *obj = Object::GetByTile(ti);
		if (obj != nullptr) north_tile = obj->location.tile.base();
	}

	for (const APRuin &ruin : _ap_active_ruins) {
		if (ruin.completed) continue;
		if (ruin.tile != north_tile) continue;

		/* Found an active ruin on this tile — add its cargo reqs as acceptance */
		for (const APRuinCargoReq &req : ruin.cargo_reqs) {
			if (req.delivered >= req.required) continue; /* already satisfied */
			if (req.cargo == 0xFF) continue;             /* invalid cargo */
			acceptance[req.cargo] += 8; /* 8/8 = full acceptance */
			SetBit(always_accepted, req.cargo);
		}
		return true;
	}
	return false;
}

/** Check if a tile has an active (non-completed) ruin.
 *  For 3x3 ruins, any of the 9 tiles returns true. We resolve
 *  the north (top-left) tile via Object::GetByTile and match
 *  against ruin.tile which stores the north tile. */
bool AP_IsRuinTile(uint32_t tile_index)
{
	TileIndex ti{tile_index};
	/* Resolve to north tile if this is part of a multi-tile object */
	uint32_t north_tile = tile_index;
	if (IsValidTile(ti) && IsTileType(ti, MP_OBJECT)) {
		Object *obj = Object::GetByTile(ti);
		if (obj != nullptr) north_tile = obj->location.tile.base();
	}

	for (const APRuin &ruin : _ap_active_ruins) {
		if (ruin.tile == north_tile && !ruin.completed) return true;
	}
	return false;
}

/** Fill an APRuinView snapshot for the ruin at the given tile.
 *  Resolves multi-tile objects to their north tile before matching. */
bool AP_GetRuinViewByTile(uint32_t tile_index, APRuinView &out)
{
	/* Resolve to north tile for multi-tile objects */
	TileIndex ti{tile_index};
	uint32_t north_tile = tile_index;
	if (IsValidTile(ti) && IsTileType(ti, MP_OBJECT)) {
		Object *obj = Object::GetByTile(ti);
		if (obj != nullptr) north_tile = obj->location.tile.base();
	}

	for (const APRuin &ruin : _ap_active_ruins) {
		if (ruin.tile != north_tile) continue;
		out.id            = ruin.id;
		out.location_name = ruin.location_name;
		out.tile          = ruin.tile;
		out.town_name     = ruin.town_name;
		out.completed     = ruin.completed;
		out.cargo.clear();
		for (const APRuinCargoReq &req : ruin.cargo_reqs) {
			APRuinView::CargoLine cl;
			cl.name      = req.cargo_name;
			cl.required  = req.required;
			cl.delivered = req.delivered;
			out.cargo.push_back(std::move(cl));
		}
		return true;
	}
	return false;
}

/** Get all ruins for display in the industry directory. */
std::vector<APRuinView> AP_GetAllRuinViews()
{
	std::vector<APRuinView> result;
	for (const APRuin &ruin : _ap_active_ruins) {
		APRuinView rv;
		rv.id            = ruin.id;
		rv.location_name = ruin.location_name;
		rv.tile          = ruin.tile;
		rv.town_name     = ruin.town_name;
		rv.completed     = ruin.completed;
		for (const APRuinCargoReq &req : ruin.cargo_reqs) {
			APRuinView::CargoLine cl;
			cl.name      = req.cargo_name;
			cl.required  = req.required;
			cl.delivered = req.delivered;
			rv.cargo.push_back(std::move(cl));
		}
		result.push_back(std::move(rv));
	}
	return result;
}

/* Fast-forward speed — controlled by Speed Boost items (100→300 in steps of 10) */
static int _ap_ff_speed = 100;

/* News filter: 0 = Off (no newspaper popups), 1 = Self only, 2 = All (default) */
int _ap_news_filter = 2;

/* Local Task System state */
static constexpr int AP_TASK_MAX_ACTIVE = 5;
static std::vector<APTask> _ap_tasks;
static int _ap_task_next_id          = 1;    ///< Auto-increment ID for new tasks
static int _ap_task_checks_completed = 0;    ///< Mission-check rewards collected; adds to shop counter
static std::string _ap_staging_tasks;        ///< Task string loaded from savegame, applied on first tick
static int  _ap_staging_task_checks  = 0;
static bool _ap_staging_has_data     = false;

/* Staging for cumulative stats — restored by SL before first tick, applied after AP_InitSessionStats */
static uint64_t _ap_staging_cargo[NUM_CARGO] = {};
static int64_t  _ap_staging_profit           = 0;
static int64_t  _ap_staging_items_received   = -1;  ///< -1 = not staged
static bool     _ap_staging_stats            = false;

/* Staging for shop sent locations — AP_InitSessionStats clears _ap_sent_shop_locations,
 * so the save/load sets this staging string which is applied after init. */
static std::string _ap_staging_shop_sent;
static bool        _ap_staging_shop_sent_valid = false;

/* Staging for AP-server checked locations — received via Connected/RoomUpdate.
 * Applied after session init (which clears _ap_sent_shop_locations and mission flags). */
static std::set<std::string> _ap_server_checked_locations;
static bool                  _ap_server_checked_pending = false;

/* Staging for completed missions — AP_OnSlotData overwrites _ap_pending_sd (including
 * all mission completion flags). The saveload stores the completed mission string here,
 * and it gets re-applied in first-tick setup after AP_OnSlotData has run. */
static std::string _ap_staging_completed_missions;
static bool        _ap_staging_completed_valid = false;

/* Staging for maintain counters — same timing issue as completed missions. */
static std::string _ap_staging_maintain_counters;
static bool        _ap_staging_maintain_valid  = false;

/* Staging for named entity cumulative progress — AP_OnSlotData overwrites
 * _ap_pending_sd (including named_entity.cumulative on all missions).
 * The saveload stores the string here and it gets re-applied AFTER
 * AP_AssignNamedEntities() in first-tick setup. */
static std::string _ap_staging_named_entities;
static bool        _ap_staging_named_valid = false;

/** Normalize typographic quotes to ASCII — GRFs may use U+2018/U+2019 (single)
 *  or U+201C/U+201D (double) while items.py uses plain ASCII ' and ". */
static std::string NormalizeQuotes(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ) {
		uint8_t c = static_cast<uint8_t>(s[i]);
		if (c == 0xE2 && i + 2 < s.size()) {
			uint8_t b1 = static_cast<uint8_t>(s[i+1]);
			uint8_t b2 = static_cast<uint8_t>(s[i+2]);
			if (b1 == 0x80 && (b2 == 0x98 || b2 == 0x99)) {
				out += '\''; i += 3; continue;  /* U+2018/U+2019 → ' */
			}
			if (b1 == 0x80 && (b2 == 0x9C || b2 == 0x9D)) {
				out += '"';  i += 3; continue;  /* U+201C/U+201D → " */
			}
		}
		out += s[i]; i++;
	}
	return out;
}

static void BuildEngineMap()
{
	_ap_engine_map.clear();
	_ap_engine_extras.clear();

	/* Military Items GRF: engine names are assigned by Action 4 during GRF
	 * loading, conditioned on param[0].  AP_ConsumeWorldStart sets the correct
	 * params ({1,1,4,4,0}) for NEW games.  Saves created before this fix have
	 * param[0]=0 → 123 empty aircraft engine slots with no names, no cargo,
	 * no speed — these are not real purchasable aircraft and cannot be fixed
	 * retroactively (engine slot allocation is baked into the save).
	 * For these old saves, MIL aircraft items are silently skipped. */

	int skipped_empty = 0;
	int recovered_invalid = 0;
	int recovered_newgrf = 0;
	for (const Engine *e : Engine::Iterate()) {
		std::string name;
		bool was_invalid = (e->info.string_id == INVALID_STRING_ID);

		/* Try every EngineNameContext.  The SCC_ENGINE_NAME handler is safe for
		 * all engines: it null-checks, tries user name, tries GRF callback,
		 * then checks INVALID_STRING_ID before resolving string_id.
		 * Some GRFs (Military Items) only respond to specific contexts. */
		static const EngineNameContext contexts[] = {
			EngineNameContext::PurchaseList,
			EngineNameContext::Generic,
			EngineNameContext::PreviewNews,
			EngineNameContext::VehicleDetails,
			EngineNameContext::AutoreplaceVehicleInUse,
		};
		for (auto ctx : contexts) {
			name = GetString(STR_ENGINE_NAME,
			    PackEngineNameDParam(e->index, ctx));
			if (!name.empty()) break;
		}

		/* Final fallback: string_id directly (works for vanilla & some GRFs) */
		if (name.empty() && e->info.string_id != INVALID_STRING_ID &&
		    e->info.string_id != STR_NEWGRF_INVALID_ENGINE) {
			name = GetString(e->info.string_id);
		}

		if (was_invalid && !name.empty()) recovered_invalid++;
		if (e->info.string_id == STR_NEWGRF_INVALID_ENGINE && !name.empty()) recovered_newgrf++;

		if (name.empty()) {
			skipped_empty++;
			continue;
		}

		/* Trim leading/trailing whitespace — some GRFs produce names with
		 * a leading space (e.g. " Douglas DC-3 Dakota" from Aircraft2025). */
		size_t start = name.find_first_not_of(" \t\r\n");
		size_t end = name.find_last_not_of(" \t\r\n");
		if (start == std::string::npos) {
			skipped_empty++;
			continue;
		}
		name = name.substr(start, end - start + 1);
		name = NormalizeQuotes(name);

		if (_ap_engine_map.count(name) == 0) {
			_ap_engine_map[name] = e->index;
		} else {
			/* Same name used by multiple engine instances (e.g. the three
			 * "Oil Tanker" wagon variants — Rail, Monorail, Maglev).
			 * Store extras so AP_UnlockEngineByName can unlock them all. */
			_ap_engine_extras[name].push_back(e->index);
		}
	}
	_ap_engine_map_built = true;
	Debug(misc, 1, "[AP] Engine map built: {} engines", _ap_engine_map.size());
	AP_LOG(fmt::format("Engine map built: {} engines indexed ({} with shared names), skipped_empty={}, recovered_invalid={}, recovered_newgrf={}",
	       _ap_engine_map.size(), _ap_engine_extras.size(), skipped_empty, recovered_invalid, recovered_newgrf));

	/* Dump full engine map to ap_engine_map.log for debugging name mismatches */
	{
		FILE *emap = fopen("ap_engine_map.log", "w");
		if (emap) {
			fmt::print(emap, "=== AP Engine Map ({} entries, {} extras, {} skipped) ===\n",
			           _ap_engine_map.size(), _ap_engine_extras.size(), skipped_empty);
			for (const auto &pair : _ap_engine_map) {
				const Engine *eng = Engine::GetIfValid(pair.second);
				fmt::print(emap, "  [{}] '{}' (type={} grfid={:08X})\n",
				           pair.second, pair.first,
				           eng ? (int)eng->type : -1,
				           eng && eng->grf_prop.grffile ? eng->grf_prop.grfid : 0);
			}
			fmt::print(emap, "\n=== Extras (shared names) ===\n");
			for (const auto &pair : _ap_engine_extras) {
				fmt::print(emap, "  '{}' => [", pair.first);
				for (size_t i = 0; i < pair.second.size(); i++) {
					if (i > 0) fmt::print(emap, ", ");
					fmt::print(emap, "{}", pair.second[i]);
				}
				fmt::print(emap, "]\n");
			}
			fclose(emap);
		}
	}
}

/**
 * Force the game language to English (en_GB).
 * AP item names are always English, so the engine name lookup map must be
 * built with English strings.  The simplest guarantee is to switch the
 * running game to English at AP session start and keep it there.
 */
static void ForceEnglishLanguage()
{
	/* Already English? */
	if (_current_language != nullptr) {
		std::string_view iso = _current_language->isocode;
		if (iso.starts_with("en")) return;
	}

	/* Find en_GB in the language list */
	const LanguageMetadata *english = nullptr;
	for (const LanguageMetadata &lng : _languages) {
		std::string_view iso = lng.isocode;
		if (iso == "en_GB") { english = &lng; break; }
		if (iso.starts_with("en") && english == nullptr) english = &lng;
	}

	if (english == nullptr) {
		AP_WARN("Could not find English language pack — engine names may not match AP items!");
		return;
	}

	if (!ReadLanguagePack(english)) {
		AP_WARN("Failed to load English language pack!");
		return;
	}

	AP_OK("Language forced to English (en_GB) for AP item name compatibility.");
}

/* -------------------------------------------------------------------------
 * AP_IsActive — called from engine.cpp to block vanilla date-introduction
 * ---------------------------------------------------------------------- */

bool AP_IsActive()
{
	/* Bridge mode: the Bridge controls AP, not a local client. */
	if (_ap_bridge_mode) return true;

	return _ap_client != nullptr &&
	       _ap_client->GetState() == APState::AUTHENTICATED;
}

/* -------------------------------------------------------------------------
 * AP_SendSay — forward a chat/command string to the AP server.
 * Used by the "ap" console command so players can type AP server commands
 * (e.g. !hint, !remaining) directly from the OpenTTD console.
 * ---------------------------------------------------------------------- */

void AP_SendSay(const std::string &text)
{
	if (_ap_client == nullptr) {
		IConsolePrint(CC_ERROR, "[AP] Not connected to Archipelago server.");
		return;
	}
	_ap_client->SendSay(text);
	IConsolePrint(CC_INFO, fmt::format("[AP] Sent: {}", text).c_str());
}

/** Console command: ap <message>
 *  Forwards the message to the Archipelago server as a Say packet.
 *  Examples:
 *    ap !hint "Forest of Magic"
 *    ap !remaining
 *    ap !getitem "Cash Injection £200,000"
 */
static bool ConCmdAP(std::span<std::string_view> argv)
{
	if (argv.size() < 2) {
		IConsolePrint(CC_HELP, "Usage: ap <message>");
		IConsolePrint(CC_HELP, "  Sends a message/command to the Archipelago server.");
		IConsolePrint(CC_HELP, "  Examples:  ap !hint Wills 2-8-0");
		IConsolePrint(CC_HELP, "             ap !remaining");
		IConsolePrint(CC_HELP, "             ap !status");
		return true;
	}
	/* Join all arguments after "ap" into one string */
	std::string msg;
	for (size_t i = 1; i < argv.size(); i++) {
		if (i > 1) msg += ' ';
		msg += std::string(argv[i]);
	}
	AP_SendSay(msg);
	return true;
}

/** Registers the "ap" console command. Called once at game init. */
void AP_RegisterConsoleCommands()
{
	IConsole::CmdRegister("ap", ConCmdAP);
}

/* -------------------------------------------------------------------------
 * Iron Horse wagon bundle helpers
 * ---------------------------------------------------------------------- */

/**
 * Build the IH wagon queue by scanning all engines from the same GRF as
 * the given IH engine.  Called lazily on the first IH engine unlock.
 */
static void AP_BuildIHWagonQueue(EngineID ih_engine_id)
{
	const Engine *ref = Engine::GetIfValid(ih_engine_id);
	if (ref == nullptr || ref->grf_prop.grffile == nullptr) return;

	_ap_ih_grfid = ref->grf_prop.grfid;
	_ap_ih_wagon_queue.clear();
	_ap_ih_engines_total = 0;

	for (const Engine *e : Engine::Iterate()) {
		if (e->type != VEH_TRAIN) continue;
		if (e->grf_prop.grffile == nullptr || e->grf_prop.grfid != _ap_ih_grfid) continue;

		bool is_wagon = (e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON);
		if (is_wagon) {
			_ap_ih_wagon_queue.push_back(e->index);
		} else {
			_ap_ih_engines_total++;
		}
	}

	/* Sort by EngineID so early/basic wagons come first */
	std::sort(_ap_ih_wagon_queue.begin(), _ap_ih_wagon_queue.end());
	_ap_ih_wagons_built = true;

	Debug(misc, 1, "[AP] IH wagon queue: {} wagons across {} engines (grfid={:08X})",
	      _ap_ih_wagon_queue.size(), _ap_ih_engines_total, _ap_ih_grfid);
}

/**
 * Unlock a proportional batch of IH wagons based on how many IH engines
 * have been unlocked so far.  Safe to call multiple times (idempotent).
 */
static void AP_UnlockIHWagonBatch()
{
	if (!_ap_ih_wagons_built || _ap_ih_wagon_queue.empty() || _ap_ih_engines_total == 0) return;
	if (!_ap_ih_wagon_unlocks) return;

	CompanyID cid = _ap_bridge_mode ? CompanyID(0) : _local_company;
	if (cid >= MAX_COMPANIES) return;

	/* Count how many IH engines are currently unlocked */
	int engines_unlocked = 0;
	for (const Engine *e : Engine::Iterate()) {
		if (e->type != VEH_TRAIN) continue;
		if (e->grf_prop.grffile == nullptr || e->grf_prop.grfid != _ap_ih_grfid) continue;
		if (e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) continue;
		if (_ap_unlocked_engine_ids.count(e->index)) engines_unlocked++;
	}

	int total_wagons = (int)_ap_ih_wagon_queue.size();
	/* Each IH engine unlock gives 1-4 wagons, cycling: 1,2,3,4,1,2,3,4...
	 * This feels like progression — some unlocks are small, some are big.
	 * Average 2.5 per engine, so all ~178 wagons unlock after ~71 engines. */
	int target = 0;
	for (int i = 1; i <= engines_unlocked; i++) {
		target += 1 + ((i - 1) % 4);  /* cycle: 1, 2, 3, 4, 1, 2, 3, 4 ... */
	}
	if (target > total_wagons) target = total_wagons;

	int newly_unlocked = 0;
	for (int i = 0; i < target; i++) {
		EngineID wid = _ap_ih_wagon_queue[i];
		if (_ap_unlocked_engine_ids.count(wid)) continue; /* already done */

		Engine *w = Engine::GetIfValid(wid);
		if (w == nullptr) continue;

		_ap_unlocked_engine_ids.insert(wid);
		w->flags.Set(EngineFlag::Available);
		if (_ap_bridge_mode) {
			w->company_avail.Set();
			for (const Company *co : Company::Iterate()) {
				CompanyID old = _current_company;
				_current_company = OWNER_DEITY;
				Command<CMD_ENGINE_CTRL>::Post(wid, co->index, true);
				_current_company = old;
			}
		} else {
			CompanyID old = _current_company;
			_current_company = OWNER_DEITY;
			Command<CMD_ENGINE_CTRL>::Do(DoCommandFlags{DoCommandFlag::Execute},
				wid, cid, true);
			_current_company = old;
		}
		newly_unlocked++;
	}

	if (newly_unlocked > 0) {
		InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
		Debug(misc, 1, "[AP] IH wagon batch: unlocked {} wagons ({}/{} total, {}/{} engines)",
		      newly_unlocked, target, total_wagons, engines_unlocked, _ap_ih_engines_total);
	}
}

/* -------------------------------------------------------------------------
 * AP_UnlockEngineByName
 * Calls OpenTTD's own EnableEngineForCompany() which handles:
 *   - company_avail.Set(company)
 *   - avail_railtypes / avail_roadtypes update
 *   - AddRemoveEngineFromAutoreplaceAndBuildWindows()
 *   - Toolbar invalidation
 * ---------------------------------------------------------------------- */

bool AP_UnlockEngineByName(const std::string &name)
{
	CompanyID cid = _local_company;
	/* Bridge mode: dedicated server has _local_company == COMPANY_SPECTATOR.
	 * Use Company 0 (first player's company) as the target for CMD_ENGINE_CTRL.
	 * If Company 0 doesn't exist yet, we still proceed — the company_avail bit
	 * and _ap_unlocked_engine_ids entry are set so the engine is available when
	 * a company IS created. */
	if (_ap_bridge_mode) cid = CompanyID(0);
	if (cid >= MAX_COMPANIES) return false;

	if (!_ap_engine_map_built) BuildEngineMap();

	/* Alias map: old/wrong AP item names → correct OpenTTD 15.2 engine string names.
	 * Needed for backward-compat with saves generated before items.py was corrected. */
	static const std::map<std::string, std::string> aliases = {
		/* Trains — wrong name → correct name */
		{"Wills 2-8-0",               "Wills 2-8-0 (Steam)"},
		{"Kirby Paul Tank Engine",    "Kirby Paul Tank (Steam)"},
		{"MJS 250",                   "MJS 250 (Diesel)"},
		{"Mightymover Choo-Choo",     "MightyMover Choo-Choo"},
		{"Turner Turbo",              "Turner Turbo (Diesel)"},
		{"MJS 1000",                  "MJS 1000 (Diesel)"},
		{"SH/Hendry 25",              "SH/Hendry '25' (Diesel)"},
		{"Manley-Morel DMU",          "Manley-Morel DMU (Diesel)"},
		{"Dash",                      "'Dash' (Diesel)"},
		{"AsiaStar",                  "'AsiaStar' (Electric)"},
		{"SH 8P",                     "SH '8P' (Steam)"},
		{"Ginzu 'A4'",                "Ginzu 'A4' (Steam)"},
		{"SH 'Chaperon'",             "SH '30' (Electric)"},
		{"Lev1 'Leviathan'",          "Lev1 'Leviathan' (Electric)"},
		{"Lev2 'Cyclops'",            "Lev2 'Cyclops' (Electric)"},
		{"Lev3 'Pegasus'",            "Lev3 'Pegasus' (Electric)"},
		{"Lev4 'Chimaera'",           "Lev4 'Chimaera' (Electric)"},
		{"Lev 'TransRapid'",          "Wizzowow Rocketeer"},
		{"Lev 'Fasttrack'",           "Wizzowow Z99"},
		{"Lev 'Bullet'",              "SH '40' (Electric)"},
		/* Wagons */
		{"Armored Van",               "Armoured Van"},
		{"Valuables Van",             "Armoured Van"},   /* doesn't exist — valuables use Armoured Van */
		/* Road vehicles */
		{"Foster MkII Bus",           "Foster MkII Superbus"},
		{"Ploddyphut 10-inch Bus",    "Ploddyphut MkI Bus"},
		{"Fellini 105 Bus",           "Ploddyphut MkII Bus"},
		{"MPS Magic Plus Bus",        "Ploddyphut MkIII Bus"},
		{"Balogh Mail Truck",         "MPS Mail Truck"},
		{"Uhl Mail Truck",            "Perry Mail Truck"},
		{"Kelling 3100 Mail Truck",   "Reynard Mail Truck"},
		{"Kelling 3100 Coal Truck",   "DW Coal Truck"},
		{"Balogh Grain Truck",        "Hereford Grain Truck"},
		{"Uhl Grain Truck",           "Goss Grain Truck"},
		{"Kelling 3100 Grain Truck",  "Thomas Grain Truck"},
		{"Uhl Goods Truck",           "Goss Goods Truck"},
		{"Kelling 3100 Goods Truck",  "Craighead Goods Truck"},
		{"Balogh Oil Tanker",         "Witcombe Oil Tanker"},
		{"Uhl Oil Tanker",            "Perry Oil Tanker"},
		{"Kelling 3100 Oil Tanker",   "Foster Oil Tanker"},
		{"Balogh Wood Truck",         "Witcombe Wood Truck"},
		{"Uhl Wood Truck",            "Moreland Wood Truck"},
		{"Kelling 3100 Wood Truck",   "Foster Wood Truck"},
		{"Balogh Iron Ore Truck",     "MPS Iron Ore Truck"},
		{"Kelling 3100 Iron Ore Truck","Chippy Iron Ore Truck"},
		{"Kelling 3100 Steel Truck",  "Kelling Steel Truck"},
		{"Balogh Valuables Truck",    "Balogh Armoured Truck"},
		{"Uhl Valuables Truck",       "Uhl Armoured Truck"},
		{"Kelling 3100 Valuables Truck","Foster Armoured Truck"},
		{"Balogh Livestock Truck",    "Talbott Livestock Van"},
		{"Uhl Livestock Truck",       "Uhl Livestock Van"},
		{"Kelling 3100 Livestock Truck","Foster Livestock Van"},
		/* Aircraft */
		{"Dinger 100-Series",         "Dinger 100"},
		{"Dinger 200-Series",         "Dinger 200"},
		{"Dinger 1000-Series",        "Dinger 1000"},
		{"AEI Super 49",              "Darwin 400"},
		{"Yate Aerospace YAC 1000",   "Yate Aerospace YAe46"},
		{"Kalmar Industries TK1",     "Kelling K1"},
		{"Chugsworth Academy Bullet", "Kelling K6"},
		/* Military Items Aircraft — items.py names vs in-game GRF names.
		 * Fuzzy (starts-with) matching handles nicknames like
		 *   "Avia B.3" → "Avia B.3 \"Bull\""
		 *   "Aero L-29" → "Aero L-29 \"Dolphin\""
		 * These aliases handle diacritics and naming differences. */
		{"Letov S.328",                      "Letov \xC5\xA0-328"},       /* Š-328; GRF uses diacritic + hyphen */
		{"Zlin Z.12",                        "Zl\xC3\xADn Z.12"},        /* í; GRF uses diacritic */
		/* MiG-15/19/21: GRF uses "Mikoyan-Gurevich" (full name) — NO alias needed.
		 * MiG-29: GRF uses "Mikoyan" (short modern name) — alias needed. */
		{"Mikoyan-Gurevich MiG-29",          "Mikoyan MiG-29"},
		{"Bell UH-1 Iroquois",               "Bell UH-1 Iroquois / HU-1 \"Huey\""},
		{"Aerospatiale SA 321 Super Frelon", "A\xC3\xA9rospatiale SA 321 Super Frelon"}, /* é */
		{"Aerospatiale SA 330 Puma",         "A\xC3\xA9rospatiale SA 330 Puma"},
		{"Aero A.304 (courier)",             "Aero K-75"},              /* different GRF designation */
		{"Kamov Ka-62",                      "Kamow Ka-62"},            /* GRF typo: 'w' instead of 'v' */
		/* HEQS wagons/trailers: GRF registers names in square brackets */
		{"Generic Medium Trailer",           "[Generic Medium Trailer]"},
		{"Kander Trailer",                   "[Kander Trailer]"},
		{"Tram Wagon 1",                     "[Tram Wagon 1]"},
		{"Tram Wagon 2",                     "[Tram Wagon 2]"},
		{"Tram Wagon 3",                     "[Tram Wagon 3]"},
		{"Express Tram Wagon 1",             "[Express Tram Wagon 1]"},
		/* Ships */
		{"Bakewell Lake Cruiser",        "Bakewell Cargo Ship"},
		{"Chugsworth Packet",            "Chugger-Chug Passenger Ferry"},
		{"Shackleton Hacker",            "Yate Cargo Ship"},
		{"Bakewell Cotswald Coal Barge", "MightyMover Cargo Ship"},
		{"Uhl Coal Tanker",              "CS-Inc. Oil Tanker"},
		{"Bakewell Wet Cargo Barge",     "Bakewell 300 Hovercraft"},
		{"Kelling VTOL",                 "Powernaut Cargo Ship"},
	};

	/* Resolve alias if needed */
	std::string resolved = name;

	/* NewGRF engines are prefixed in AP item names to avoid collisions with
	 * vanilla engines. Strip the prefix before looking up in the engine map —
	 * GRFs register their vehicles with plain names. */
	static const std::string ih_prefix = "IH: ";
	static const std::string mil_prefix = "MIL: ";
	static const std::string shark_prefix = "SHARK: ";
	static const std::string hv_prefix = "HV: ";
	static const std::string heqs_prefix = "HEQS: ";
	static const std::string vac_prefix = "VAC: ";
	static const std::string ap25_prefix = "AP25: ";

	if (resolved.size() > ih_prefix.size() &&
	    resolved.substr(0, ih_prefix.size()) == ih_prefix) {
		resolved = resolved.substr(ih_prefix.size());
		Debug(misc, 1, "[AP] Iron Horse prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > mil_prefix.size() &&
	           resolved.substr(0, mil_prefix.size()) == mil_prefix) {
		resolved = resolved.substr(mil_prefix.size());
		Debug(misc, 1, "[AP] Military Items prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > shark_prefix.size() &&
	           resolved.substr(0, shark_prefix.size()) == shark_prefix) {
		resolved = resolved.substr(shark_prefix.size());
		Debug(misc, 1, "[AP] SHARK prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > hv_prefix.size() &&
	           resolved.substr(0, hv_prefix.size()) == hv_prefix) {
		resolved = resolved.substr(hv_prefix.size());
		Debug(misc, 1, "[AP] Hover Vehicles prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > heqs_prefix.size() &&
	           resolved.substr(0, heqs_prefix.size()) == heqs_prefix) {
		resolved = resolved.substr(heqs_prefix.size());
		Debug(misc, 1, "[AP] HEQS prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > vac_prefix.size() &&
	           resolved.substr(0, vac_prefix.size()) == vac_prefix) {
		resolved = resolved.substr(vac_prefix.size());
		Debug(misc, 1, "[AP] Vactrain prefix stripped: '{}' → '{}'", name, resolved);
	} else if (resolved.size() > ap25_prefix.size() &&
	           resolved.substr(0, ap25_prefix.size()) == ap25_prefix) {
		resolved = resolved.substr(ap25_prefix.size());
		Debug(misc, 1, "[AP] Aircraftpack prefix stripped: '{}' → '{}'", name, resolved);
	}

	auto alias_it = aliases.find(resolved);
	if (alias_it != aliases.end()) {
		resolved = alias_it->second;
		Debug(misc, 1, "[AP] Engine alias: '{}' → '{}'", name, resolved);
	}

	/* Trim whitespace and normalize quotes to match engine map keys. */
	{
		size_t s = resolved.find_first_not_of(" \t\r\n");
		size_t e = resolved.find_last_not_of(" \t\r\n");
		if (s != std::string::npos) resolved = resolved.substr(s, e - s + 1);
	}
	resolved = NormalizeQuotes(resolved);

	auto it = _ap_engine_map.find(resolved);
	if (it == _ap_engine_map.end()) {
		/* Fuzzy match: some GRFs use full names (e.g. "Newport Container Feeder")
		 * while items.py only stores the short prefix (e.g. "Newport").
		 * Try starts-with matching as a fallback. */
		for (const auto &pair : _ap_engine_map) {
			if (pair.first.size() > resolved.size() &&
			    pair.first.compare(0, resolved.size(), resolved) == 0 &&
			    pair.first[resolved.size()] == ' ') {
				resolved = pair.first;  /* update resolved to the full name */
				it = _ap_engine_map.find(resolved);
				AP_LOG(fmt::format("FUZZY_MATCH: '{}' => '{}' (eid {})", name, resolved, pair.second));
				break;
			}
		}
	}
	if (it == _ap_engine_map.end()) {
		/* Engines that vanilla has but are completely replaced/removed by GRFs.
		 * Iron Horse (Vactrain) replaces Lev1/Lev2 engine slots entirely. */
		static const std::set<std::string> known_absent = {
			"Lev1 'Leviathan' (Electric)",
			"Lev2 'Cyclops' (Electric)",
		};
		if (known_absent.count(resolved)) {
			Debug(misc, 1, "[AP] UnlockEngine: '{}' replaced by GRF — skipping", resolved);
			AP_LOG(fmt::format("UNLOCK_SKIP_GRF: '{}'", resolved));
			return true;  /* not an error — GRF replaced it */
		}

		/* Non-vehicle items (traps, utilities, shop items) are expected to
		 * fail engine lookup — they'll be handled by the trap/utility chain.
		 * Don't log UNLOCK_FAIL for these to keep the log clean. */
		static const std::set<std::string> non_vehicle_prefixes = {
			"Breakdown Wave", "Maintenance Surge", "Signal Failure", "Fuel Shortage",
			"Cash Injection", "Loan Reduction", "Cargo Bonus", "Reliability Boost",
			"Town Growth Boost", "Free Station Upgrade", "Speed Boost",
			"License Revoke", "Station Downgrade", "Industry Decline",
		};
		for (const auto &prefix : non_vehicle_prefixes) {
			if (resolved.compare(0, prefix.size(), prefix) == 0) {
				return false;  /* silently — not a vehicle */
			}
		}

		Debug(misc, 1, "[AP] UnlockEngine: '{}' (resolved: '{}') not found in engine map",
		      name, resolved);
		AP_LOG(fmt::format("UNLOCK_FAIL: original='{}' resolved='{}' map_size={}", name, resolved, _ap_engine_map.size()));
		return false;
	}

	Engine *e = Engine::GetIfValid(it->second);
	if (e == nullptr) return false;

	/* Track that AP has explicitly unlocked this engine — the periodic
	 * re-lock sweep will not re-lock engines present in this set. */
	_ap_unlocked_engine_ids.insert(it->second);

	/* Set EngineFlag::Available so the engine appears in the build-vehicle list.
	 * EnableEngineForCompany() only sets company_avail, but the build-vehicle
	 * window also checks EngineFlag::Available before showing any engine. */
	e->flags.Set(EngineFlag::Available);

	/* Archipelago: force the current landscape climate bit so the engine is
	 * fully functional (IsEnabled, cargo capacity, buy command all check this).
	 * NewGRF engines may not have the climate set for this landscape, but AP
	 * explicitly chose them — they MUST work. */
	if (!e->info.climates.Test(_settings_game.game_creation.landscape)) {
		e->info.climates.Set(_settings_game.game_creation.landscape);
		AP_TRACE(fmt::format("ForceClimate: eid={} landscape={}", (int)it->second.base(), (int)_settings_game.game_creation.landscape));
	}
	if (_ap_bridge_mode) {
		/* Bridge mode: unlock for ALL companies.
		 * Use ::Post so the command propagates to all connected clients
		 * via OpenTTD's network command system — prevents desync. */
		e->company_avail.Set();
		for (const Company *co : Company::Iterate()) {
			CompanyID old = _current_company;
			_current_company = OWNER_DEITY;
			Command<CMD_ENGINE_CTRL>::Post(it->second, co->index, true);
			_current_company = old;
		}
	} else {
		/* Singleplayer: execute directly */
		CompanyID old = _current_company;
		_current_company = OWNER_DEITY;
		CommandCost res = Command<CMD_ENGINE_CTRL>::Do(DoCommandFlags{DoCommandFlag::Execute},
			it->second, cid, true);
		_current_company = old;
		int eid_val = (int)it->second.base();
		int cid_val = (int)cid.base();
		const char *res_str = res.Succeeded() ? "OK" : "FAIL";
		int avail_val = e->company_avail.Test(cid) ? 1 : 0;
		int enabled_val = e->IsEnabled() ? 1 : 0;
		int flag_val = e->flags.Test(EngineFlag::Available) ? 1 : 0;
		bool str_ok = e->info.string_id != STR_NEWGRF_INVALID_ENGINE;
		bool climate_ok = e->info.climates.Test(_settings_game.game_creation.landscape);
		AP_TRACE(fmt::format("CMD_ENGINE_CTRL: eid={} cid={} result={} avail={} enabled={} available={} str_ok={} climate_ok={}",
			eid_val, cid_val, res_str, avail_val, enabled_val, flag_val, str_ok ? 1 : 0, climate_ok ? 1 : 0));
	}

	/* Unlock any additional engines that share this name (e.g. all three
	 * "Oil Tanker" wagon variants — Rail, Monorail, Maglev). */
	auto extras_it = _ap_engine_extras.find(resolved);
	if (extras_it != _ap_engine_extras.end()) {
		for (EngineID extra_eid : extras_it->second) {
			Engine *extra_e = Engine::GetIfValid(extra_eid);
			if (extra_e == nullptr) continue;
			_ap_unlocked_engine_ids.insert(extra_eid);
			extra_e->flags.Set(EngineFlag::Available);
			if (!extra_e->info.climates.Test(_settings_game.game_creation.landscape)) {
				extra_e->info.climates.Set(_settings_game.game_creation.landscape);
			}
			if (_ap_bridge_mode) {
				extra_e->company_avail.Set();
				for (const Company *co : Company::Iterate()) {
					CompanyID old = _current_company;
					_current_company = OWNER_DEITY;
					Command<CMD_ENGINE_CTRL>::Post(extra_eid, co->index, true);
					_current_company = old;
				}
			} else {
				CompanyID old = _current_company;
				_current_company = OWNER_DEITY;
				Command<CMD_ENGINE_CTRL>::Do(DoCommandFlags{DoCommandFlag::Execute},
					extra_eid, cid, true);
				_current_company = old;
			}
		}
	}

	/* Explicitly invalidate the build-vehicle window so the new engine shows up */
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	Debug(misc, 0, "[AP] Engine unlocked: {}", resolved);

	/* Iron Horse wagon bundle: if this was an IH engine unlock and wagon
	 * unlocks are enabled, auto-unlock a proportional batch of IH wagons.
	 * The "IH: " prefix is still present in the original item name. */
	if (name.size() > 4 && name.substr(0, 4) == "IH: " && _ap_ih_wagon_unlocks) {
		if (!_ap_ih_wagons_built) AP_BuildIHWagonQueue(it->second);
		AP_UnlockIHWagonBatch();
	}

	return true;
}

/* -------------------------------------------------------------------------
 * In-game news/chat notification
 * ---------------------------------------------------------------------- */

/* =========================================================================
 * Colby Event — globals and constants (must be before any function that uses them)
 * ====================================================================== */
static bool        _ap_colby_enabled        = false;
static int         _ap_colby_start_year     = 0;
static uint32_t    _ap_colby_town_seed      = 0;
static TownID      _ap_colby_target_town    = (TownID)UINT16_MAX;
static std::string _ap_colby_target_name;
static std::string _ap_colby_cargo_name;
static CargoType   _ap_colby_cargo_type     = INVALID_CARGO;
static int         _ap_colby_step           = 0;
static int64_t     _ap_colby_step_delivered = 0;
static bool        _ap_colby_popup_shown    = false;
static bool        _ap_colby_popup_pending  = false;  ///< true = popup dismissed without choosing, awaiting reopen
static int         _ap_colby_popup_type     = 0;      ///< 1 = arrest/escape, 2 = imprison/sacrifice
static int         _ap_colby_reopen_ticks   = 0;      ///< countdown to auto-reopen dismissed popup
static bool        _ap_colby_escaped        = false;
static int         _ap_colby_escape_ticks   = 0;
static bool        _ap_colby_done           = false;
static constexpr int64_t COLBY_STEP_AMOUNT  = 200;
static constexpr int     COLBY_ESCAPE_TICKS = 6000;
static SignID      _ap_colby_source_sign    = SignID::Invalid();
static TileIndex   _ap_colby_source_tile    = INVALID_TILE;
static TileIndex   _ap_colby_stash_tile     = INVALID_TILE;  ///< Tile of physical stash object on map

/* =========================================================================
 * Colby Event — forward declarations
 * ====================================================================== */
static void AP_ColbyShowFinalQuery();
static void AP_ColbyShowEscapeQuery();
static std::string AP_TownName(const Town *t); ///< forward decl — defined near AP_GetSlotData
static void AP_ShowNews(const std::string &text, bool is_self = true); ///< forward decl — defined after saveload globals

/* Forward declaration — custom decision window shown from archipelago_gui.cpp */
void ShowColbyDecisionWindow(int popup_type);

/* =========================================================================
 * Colby Event — decision API (called from custom window buttons)
 * ====================================================================== */

/** Player chose ARREST in popup A. */
void AP_ColbyDecisionArrest()
{
	AP_TRACE("ColbyEvent: Player chose ARREST — £10M reward");
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c != nullptr) AP_ChangeMoney(cid, (Money)10000000LL);
	AP_ShowNews(fmt::format("[AP] Colby arrested! {} reward deposited into your account!", AP_Money((Money)10000000LL)));
	AP_ShowNews(fmt::format("[AP] The town of {} is now free of Colby's influence.", _ap_colby_target_name));
	_ap_colby_done = true;
	_ap_colby_popup_pending = false;
	_ap_colby_popup_type = 0;
}

/** Player chose LET ESCAPE in popup A. */
void AP_ColbyDecisionEscape()
{
	AP_TRACE("ColbyEvent: Player chose ESCAPE — sacrifice popup pending");
	_ap_colby_escaped      = true;
	_ap_colby_escape_ticks = COLBY_ESCAPE_TICKS;
	AP_ShowNews(fmt::format("[AP] Colby slips away into the night from {}... but he won't get far.", _ap_colby_target_name));
	_ap_colby_done = true;
	_ap_colby_popup_pending = false;
	_ap_colby_popup_type = 0;
}

/** Player chose IMPRISON in popup B. */
void AP_ColbyDecisionImprison()
{
	AP_TRACE("ColbyEvent: Player chose IMPRISON");
	AP_ShowNews("[AP] Colby has been handed over to the authorities. Justice is served.");
	_ap_colby_popup_pending = false;
	_ap_colby_popup_type = 0;
}

/** Player chose SACRIFICE in popup B. */
void AP_ColbyDecisionSacrifice()
{
	AP_TRACE("ColbyEvent: Player chose SACRIFICE — £2M + town growth burst");
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c != nullptr) AP_ChangeMoney(cid, (Money)2000000LL);
	for (Town *t : Town::Iterate()) {
		t->grow_counter = 0; /* immediate growth pulse in every town */
	}
	AP_ShowNews(fmt::format("[AP] Colby sacrificed to the Elder Gods! +{} and all towns are growing rapidly!", AP_Money((Money)2000000LL)));
	_ap_colby_popup_pending = false;
	_ap_colby_popup_type = 0;
}

/** Called from the Colby status window "Reopen Decision" button. */
void AP_ColbyReopenPopup()
{
	if (!_ap_colby_popup_pending || _ap_colby_popup_type == 0) return;
	ShowColbyDecisionWindow(_ap_colby_popup_type);
}

/* =========================================================================
 * Colby Event — core logic
 * ====================================================================== */

/** Show the final arrest/escape decision window (popup A).
 *  Uses a custom window so the X button just dismisses without choosing. */
static void AP_ColbyShowFinalQuery()
{
	if (_ap_colby_popup_shown && !_ap_colby_popup_pending) return;
	_ap_colby_popup_shown   = true;
	_ap_colby_popup_pending = true;
	_ap_colby_popup_type    = 1; /* arrest/escape */
	_ap_colby_reopen_ticks  = 0;
	ShowColbyDecisionWindow(1);
}

/** Show the second decision window (popup B — Colby re-captured). */
static void AP_ColbyShowEscapeQuery()
{
	_ap_colby_popup_pending = true;
	_ap_colby_popup_type    = 2; /* imprison/sacrifice */
	_ap_colby_reopen_ticks  = 0;
	ShowColbyDecisionWindow(2);
}

/** Resolve the Colby cargo type. Called once from AP_ColbyInit().
 *  On Tropical/Toyland the CLBY cargo doesn't exist (no void slot in the
 *  climate table), so we fall back to the slot_data cargo name which is
 *  "goods" for Tropical and "sweets" for Toyland. */
static CargoType AP_ResolveColbyCargo()
{
	CargoType ct = GetCargoTypeByLabel(CT_COLBY_PACKAGE);
	if (ct != INVALID_CARGO) return ct;

	/* Tropical / Toyland — CLBY slot unavailable, fall back to slot_data cargo */
	if (!_ap_colby_cargo_name.empty()) {
		ct = (CargoType)AP_FindCargoType(_ap_colby_cargo_name);
		if (ct != INVALID_CARGO) {
			AP_OK(fmt::format("[Colby] CLBY cargo unavailable in this climate, using fallback: {}", _ap_colby_cargo_name));
			return ct;
		}
	}
	/* Last resort — use goods (exists in Temperate/Arctic/Tropical) */
	ct = (CargoType)AP_FindCargoType("goods");
	if (ct != INVALID_CARGO) AP_OK("[Colby] Using 'goods' as last-resort Colby cargo");
	return ct;
}

/** Get the runtime CargoType for Colby packages (resolved once at init). */
static CargoType AP_GetColbyPackageCargo()
{
	return _ap_colby_cargo_type;
}

/** Remove the source sign and stash object from the previous step. */
static void AP_ColbyRemoveStash()
{
	/* Remove sign */
	if (Sign *si = Sign::GetIfValid(_ap_colby_source_sign); si != nullptr) {
		delete si;
	}
	_ap_colby_source_sign = SignID::Invalid();
	_ap_colby_source_tile = INVALID_TILE;

	/* Remove physical stash object from the map */
	if (_ap_colby_stash_tile != INVALID_TILE) {
		if (IsValidTile(_ap_colby_stash_tile) && IsTileType(_ap_colby_stash_tile, MP_OBJECT)) {
			CompanyID old_company = _current_company;
			_current_company = _local_company;
			Command<CMD_LANDSCAPE_CLEAR>::Do(
				DoCommandFlags{DoCommandFlag::Execute}, _ap_colby_stash_tile);
			_current_company = old_company;
		}
		_ap_colby_stash_tile = INVALID_TILE;
	}
}

/** Find the closest player-owned station within radius tiles of a given tile. */
static Station *AP_ColbyFindNearbyStation(TileIndex centre, int radius)
{
	if (centre == INVALID_TILE) return nullptr;
	Station *best = nullptr;
	uint best_dist = UINT_MAX;
	for (Station *st : Station::Iterate()) {
		if (st->owner != _local_company) continue;
		uint d = DistanceManhattan(centre, st->xy);
		if (d <= (uint)radius && d < best_dist) {
			best = st;
			best_dist = d;
		}
	}
	return best;
}

/** Force destination stations near Colby's target town to accept the cargo. */
static void AP_ColbyForceTargetAcceptance()
{
	if (_ap_colby_target_town == (TownID)UINT16_MAX) return;
	CargoType ct = AP_GetColbyPackageCargo();
	if (ct == INVALID_CARGO) return;
	const Town *target_t = Town::GetIfValid(_ap_colby_target_town);
	if (target_t == nullptr) return;
	for (Station *dest_st : Station::Iterate()) {
		if (dest_st->town == nullptr) continue;
		if (dest_st->town->index != _ap_colby_target_town) continue;
		dest_st->always_accepted |= (1ULL << ct);
		dest_st->goods[ct].status.Set(GoodsEntry::State::Rating);
	}
}

/**
 * Spawn a physical stash object on the map for the current step.
 * The object is placed at a random location 100-400 tiles from the target town.
 * A sign is placed at the object so the player can find it.
 * The player must build a station nearby — cargo is injected in AP_ColbyTick().
 */
static void AP_ColbySpawnStash()
{
	if (_ap_colby_target_town == (TownID)UINT16_MAX) return;
	const Town *town = Town::GetIfValid(_ap_colby_target_town);
	if (town == nullptr) return;

	AP_ResolveRuinTypes();

	/* Pick a ruin object type for visual appearance */
	int variant = InteractiveRandom() % AP_NUM_RUIN_TYPES;
	ObjectType ot = _ap_ruin_types[variant];
	if (ot == INVALID_OBJECT_TYPE) {
		for (int i = 0; i < AP_NUM_RUIN_TYPES; i++) {
			if (_ap_ruin_types[i] != INVALID_OBJECT_TYPE) { ot = _ap_ruin_types[i]; variant = i; break; }
		}
		if (ot == INVALID_OBJECT_TYPE) {
			AP_WARN("[Colby] Cannot spawn stash — no ruin ObjectTypes resolved (GRF missing?)");
			return;
		}
	}

	TileIndex center = town->xy;

	/* Try to place the stash object at a suitable distance from the target town */
	TileIndex placed_tile = INVALID_TILE;
	for (int attempt = 0; attempt < 5000; attempt++) {
		TileIndex tile = RandomTile();
		if (!IsValidTile(tile)) continue;
		uint x = TileX(tile), y = TileY(tile);
		if (x < 4 || y < 4 || x >= Map::SizeX() - 4 || y >= Map::SizeY() - 4) continue;

		uint d = DistanceManhattan(center, tile);
		if (d < 100 || d > 400) continue;

		CompanyID old_company = _current_company;
		_current_company = _local_company;
		CommandCost res = Command<CMD_BUILD_OBJECT>::Do(
			DoCommandFlags{DoCommandFlag::Execute}, tile, ot, 0);
		_current_company = old_company;

		if (res.Succeeded()) {
			placed_tile = tile;
			break;
		}
	}

	if (placed_tile == INVALID_TILE) {
		AP_WARN(fmt::format("[Colby] Failed to place stash object after 5000 attempts (step {})", _ap_colby_step));
		AP_ShowNews(fmt::format("[AP] Colby Event: Could not find a location for the stash. Retrying next tick..."));
		return;
	}

	_ap_colby_stash_tile  = placed_tile;
	_ap_colby_source_tile = placed_tile;

	/* Place sign at the stash location */
	if (Sign::CanAllocateItem()) {
		int px = (int)(TileX(placed_tile) * TILE_SIZE + TILE_SIZE / 2);
		int py = (int)(TileY(placed_tile) * TILE_SIZE + TILE_SIZE / 2);
		int pz = (int)(TileHeight(placed_tile) * TILE_HEIGHT);
		std::string label = fmt::format("\xe2\x98\x85 Colby's Stash \xe2\x80\x94 Step {} \xe2\x98\x85", _ap_colby_step);
		Sign *si = new Sign(OWNER_DEITY, px, py, pz, label);
		si->UpdateVirtCoord();
		_ap_colby_source_sign = si->index;
	}

	/* Force target town stations to accept the cargo */
	AP_ColbyForceTargetAcceptance();

	AP_ShowNews(fmt::format("[AP] Step {}/5: Colby's stash has appeared on the map! "
		"Build a station nearby, load the cargo, and deliver it to {}. "
		"Refit your vehicle to 'Packages' if needed (buses, mail trucks, and goods vans work). "
		"Use 'Unload all' at the destination.",
		_ap_colby_step, _ap_colby_target_name));

	AP_TRACE(fmt::format("ColbyStash: step={} tile={} target='{}'",
		_ap_colby_step, placed_tile, _ap_colby_target_name));
}

/**
 * Inject cargo into the nearest player station if one exists near the stash.
 * Called every tick (~5s) from AP_ColbyTick() while a step is active.
 */
static void AP_ColbyInjectCargo()
{
	if (_ap_colby_stash_tile == INVALID_TILE) return;
	CargoType ct = AP_GetColbyPackageCargo();
	if (ct == INVALID_CARGO) return;

	Station *st = AP_ColbyFindNearbyStation(_ap_colby_stash_tile, 10);
	if (st == nullptr) return; /* No station nearby yet — player needs to build one */

	/* Only top up if the station has fewer than COLBY_STEP_AMOUNT packages waiting */
	auto &cargo_data = st->goods[ct].GetOrCreateData();
	if (cargo_data.cargo.AvailableCount() >= (uint)COLBY_STEP_AMOUNT) return;

	uint16_t amount = (uint16_t)std::min((int64_t)COLBY_STEP_AMOUNT, (int64_t)UINT16_MAX);
	Source src{st->town->index, SourceType::Town};
	CargoPacket *cp = new CargoPacket(st->index, amount, src);
	cargo_data.cargo.Append(cp, StationID::Invalid());
	st->goods[ct].status.Set(GoodsEntry::State::Rating);
}

/** Announce the current active step via news. */
static void AP_ColbyAnnounceStep()
{
	AP_ShowNews(fmt::format("[AP] Colby Event [Step {}/5]: Deliver cargo to {}! "
		"Refit to 'Packages' and use 'Unload all' at destination.",
		_ap_colby_step, _ap_colby_target_name));
}

/** Start cargomonitor for the current step (consume any stale deliveries). */
static void AP_ColbyBeginStepMonitor()
{
	if (_ap_colby_target_town == (TownID)UINT16_MAX) return;
	CargoType ct = AP_GetColbyPackageCargo();
	if (ct == INVALID_CARGO) return;
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return;
	/* First call registers the monitor and resets the counter — intentional. */
	CargoMonitorID monitor = EncodeCargoTownMonitor(cid, ct, _ap_colby_target_town);
	GetDeliveryAmount(monitor, true); /* discard — start fresh from this point */
	_ap_colby_step_delivered = 0;
}

/** Initialise Colby Event at session start (called once from first-tick setup).
 *  Selects target town deterministically from colby_town_seed. */
static void AP_ColbyInit()
{
	if (!_ap_colby_enabled) return;

	/* Resolve cargo type — uses CLBY label on Temperate/Arctic, falls back to
	 * slot_data cargo name (goods/sweets) on Tropical/Toyland. */
	_ap_colby_cargo_type = AP_ResolveColbyCargo();
	if (_ap_colby_cargo_type == INVALID_CARGO) {
		AP_WARN("[Colby] No valid cargo type available — disabling event.");
		_ap_colby_enabled = false;
		return;
	}

	if (_ap_colby_step != 0) {
		/* Loaded from savegame — re-register cargomonitor if a step is active. */
		if (_ap_colby_step >= 1 && _ap_colby_step <= 5 &&
		    _ap_colby_target_town != (TownID)UINT16_MAX) {
			CompanyID cid = _local_company;
			if (cid < MAX_COMPANIES && _ap_colby_cargo_type != INVALID_CARGO) {
				CargoMonitorID monitor = EncodeCargoTownMonitor(cid, _ap_colby_cargo_type, _ap_colby_target_town);
				GetDeliveryAmount(monitor, true); /* register monitor, discard stale amount */
			}
			const Town *t = Town::GetIfValid(_ap_colby_target_town);
			if (t != nullptr) _ap_colby_target_name = AP_TownName(t);

			/* Re-place sign if stash tile was saved; spawn new stash if it wasn't */
			if (_ap_colby_stash_tile != INVALID_TILE &&
			    IsValidTile(_ap_colby_stash_tile) &&
			    IsTileType(_ap_colby_stash_tile, MP_OBJECT)) {
				/* Stash object survived save/load — just re-create the sign */
				_ap_colby_source_tile = _ap_colby_stash_tile;
				if (Sign::CanAllocateItem()) {
					int px = (int)(TileX(_ap_colby_stash_tile) * TILE_SIZE + TILE_SIZE / 2);
					int py = (int)(TileY(_ap_colby_stash_tile) * TILE_SIZE + TILE_SIZE / 2);
					int pz = (int)(TileHeight(_ap_colby_stash_tile) * TILE_HEIGHT);
					std::string label = fmt::format("\xe2\x98\x85 Colby's Stash \xe2\x80\x94 Step {} \xe2\x98\x85", _ap_colby_step);
					Sign *si = new Sign(OWNER_DEITY, px, py, pz, label);
					si->UpdateVirtCoord();
					_ap_colby_source_sign = si->index;
				}
				AP_ColbyForceTargetAcceptance();
			} else {
				/* Stash tile not saved or object gone — spawn fresh */
				AP_ColbySpawnStash();
			}

			AP_OK(fmt::format("[Colby] Resumed from save: step={} delivered={} target={} stash={}",
				_ap_colby_step, _ap_colby_step_delivered, _ap_colby_target_name,
				(_ap_colby_stash_tile != INVALID_TILE) ? (int)_ap_colby_stash_tile.base() : -1));
		}
		return;
	}

	/* Fresh session — pick Colby's town deterministically. */
	/* Collect all towns and sort by TownID for determinism */
	std::vector<const Town *> towns;
	for (const Town *t : Town::Iterate()) towns.push_back(t);
	if (towns.empty()) {
		AP_WARN("[Colby] No towns on map — event cannot start.");
		_ap_colby_enabled = false;
		return;
	}
	std::sort(towns.begin(), towns.end(), [](const Town *a, const Town *b) {
		return a->index < b->index;
	});

	/* xorshift32 seeded from colby_town_seed */
	uint32_t seed = (_ap_colby_town_seed != 0) ? _ap_colby_town_seed : 0xDEADBEEFu;
	auto xr = [&]() -> uint32_t {
		seed ^= seed << 13u;
		seed ^= seed >> 17u;
		seed ^= seed << 5u;
		return seed;
	};

	size_t idx            = xr() % towns.size();
	_ap_colby_target_town = towns[idx]->index;
	_ap_colby_target_name = AP_TownName(towns[idx]);

	AP_OK(fmt::format("[Colby] Event initialised. Target town: {} (ID {}) | Cargo: Packages | Triggers year: {}",
		_ap_colby_target_name, (int)_ap_colby_target_town.base(),
		_ap_colby_start_year));
}

/** Called every ~5 s from the polling loop.
 *  Drives event progression: waiting → active steps → final popup. */
static void AP_ColbyTick()
{
	if (!_ap_colby_enabled) return;
	if (_ap_colby_target_town == (TownID)UINT16_MAX) return;

	/* Handle dismissed popup auto-reopen (applies to both popup A and popup B) */
	if (_ap_colby_popup_pending && _ap_colby_popup_type > 0) {
		_ap_colby_reopen_ticks++;
		/* Auto-reopen after ~30 seconds (120 ticks at ~4 ticks/sec) */
		if (_ap_colby_reopen_ticks >= 120) {
			_ap_colby_reopen_ticks = 0;
			AP_ShowNews("[AP] You haven't decided Colby's fate yet! Check the Colby Event window to reopen the decision.");
			ShowColbyDecisionWindow(_ap_colby_popup_type);
		}
	}

	/* If escape countdown is running (popup B pending), tick it down */
	if (_ap_colby_escaped && !_ap_colby_popup_pending) {
		/* _ap_colby_done is true, escape was chosen, waiting for popup B */
		_ap_colby_escape_ticks--;
		if (_ap_colby_escape_ticks <= 0) {
			AP_ColbyShowEscapeQuery();
		}
		return;
	}

	if (_ap_colby_done) return;

	const int cur_year = (int)TimerGameCalendar::year.base();

	/* Step 0 — waiting for start year */
	if (_ap_colby_step == 0) {
		if (cur_year < _ap_colby_start_year) return;
		/* Time to start! */
		_ap_colby_step = 1;
		AP_ColbyBeginStepMonitor();
		AP_ColbySpawnStash();
		AP_ShowNews(fmt::format("[AP] A mysterious stranger named Colby has arrived in {}. "
			"He claims to need urgent deliveries...", _ap_colby_target_name));
		AP_ColbyAnnounceStep();
		return;
	}

	/* Steps 1-5 — track deliveries */
	if (_ap_colby_step >= 1 && _ap_colby_step <= 5) {
		CompanyID cid = _local_company;
		CargoType ct = AP_GetColbyPackageCargo();
		if (cid >= MAX_COMPANIES || ct == INVALID_CARGO) return;
		if (!Town::IsValidID(_ap_colby_target_town)) return;

		/* If stash wasn't placed yet (failed on prior tick), retry */
		if (_ap_colby_stash_tile == INVALID_TILE) {
			AP_ColbySpawnStash();
		}

		/* Inject cargo into nearby station so player can pick it up */
		AP_ColbyInjectCargo();

		/* Also refresh target town acceptance (handles newly-built stations) */
		AP_ColbyForceTargetAcceptance();

		CargoMonitorID monitor = EncodeCargoTownMonitor(cid, ct, _ap_colby_target_town);
		int32_t delta = GetDeliveryAmount(monitor, true); /* consume increment, keep monitoring */
		if (delta > 0) _ap_colby_step_delivered += (int64_t)delta;

		if (_ap_colby_step_delivered >= COLBY_STEP_AMOUNT) {
			int prev_step = _ap_colby_step;
			_ap_colby_step_delivered = 0;
			_ap_colby_step++;
			AP_TRACE(fmt::format("ColbyStepComplete: step {}/5 -> {} town='{}'", prev_step, _ap_colby_step, _ap_colby_target_name));

			AP_ColbyRemoveStash(); /* remove stash object + sign from previous step */

			if (_ap_colby_step > 5) {
				/* All 5 steps complete — show final popup */
				AP_ShowNews(fmt::format("[AP] Colby thanks you for the last delivery to {}. "
					"But something feels off...", _ap_colby_target_name));
				AP_ColbyShowFinalQuery();
			} else {
				AP_ShowNews(fmt::format("[AP] Step {}/5 complete! Colby nods approvingly.", prev_step));
				AP_ColbyBeginStepMonitor(); /* reset monitor baseline for new step */
				AP_ColbySpawnStash();       /* place new stash object for next step */
				AP_ColbyAnnounceStep();
			}
		}
		return;
	}
}

/** Accessor: write Colby state to output variables (for saveload). */
void AP_GetColbyState(int *step, int64_t *delivered, int *target_town,
                      bool *escaped, int *escape_ticks, bool *done, bool *popup_shown,
                      uint32_t *stash_tile)
{
	*step         = _ap_colby_step;
	*delivered    = _ap_colby_step_delivered;
	*target_town  = (int)_ap_colby_target_town.base();
	*escaped      = _ap_colby_escaped;
	*escape_ticks = _ap_colby_escape_ticks;
	*done         = _ap_colby_done;
	*popup_shown  = _ap_colby_popup_shown;
	*stash_tile   = (_ap_colby_stash_tile != INVALID_TILE) ? (uint32_t)_ap_colby_stash_tile.base() : UINT32_MAX;
}

/** Accessor: restore Colby state from saveload. */
void AP_SetColbyState(int step, int64_t delivered, int target_town,
                      bool escaped, int escape_ticks, bool done, bool popup_shown,
                      uint32_t stash_tile)
{
	_ap_colby_step           = step;
	_ap_colby_step_delivered = delivered;
	_ap_colby_target_town    = (TownID)target_town;
	_ap_colby_escaped        = escaped;
	_ap_colby_escape_ticks   = escape_ticks;
	_ap_colby_done           = done;
	_ap_colby_popup_shown    = popup_shown;
	_ap_colby_stash_tile     = (stash_tile != UINT32_MAX) ? (TileIndex)stash_tile : INVALID_TILE;
}


static void AP_ShowNews(const std::string &text, bool is_self)
{
	IConsolePrint(CC_INFO, "[AP] " + text);
	Debug(misc, 0, "[AP] {}", text);
	AP_TRACE(fmt::format("NEWS: is_self={} text='{}'", is_self, text));

	/* Filter: 0=Off (never), 1=Self only, 2=All */
	bool show = (_game_mode == GM_NORMAL) &&
	            ((_ap_news_filter >= 2) || (_ap_news_filter == 1 && is_self));
	if (show) {
		/* Push to the integrated AP status bar message log instead of popup news */
		AP_PushStatusMessage(text);
	}
}

/* Forward declaration — defined later in this file */
extern std::atomic<bool> _ap_status_dirty;

/* =========================================================================
 * Demigods (God of Wackens) — state and logic
 * ========================================================================= */
static bool        _ap_demigod_enabled        = false;
static std::vector<APDemigodDef> _ap_demigod_defs;
static std::set<std::string>     _ap_demigod_defeated;   ///< Location names of defeated demigods
static int         _ap_demigod_active_idx     = -1;      ///< Index into _ap_demigod_defs; -1 = none
static CompanyID   _ap_demigod_active_company = CompanyID::Invalid();
static int         _ap_demigod_next_spawn_year = 0;
static bool        _ap_demigod_pending_name   = false;   ///< Waiting one tick to name the new AI company
static int         _ap_demigod_interval_min   = 5;       ///< Min years between spawns (from slot_data)
static int         _ap_demigod_interval_max   = 15;      ///< Max years between spawns (from slot_data)
static int64_t     _ap_demigod_player_value_at_spawn = 0; ///< Frozen: player company value when demigod spawned
static int         _ap_demigod_chat_cooldown  = 0;        ///< Ticks until next chat message allowed
/* Saved vehicle restrictions (restored on defeat) */
static bool        _ap_demigod_veh_saved      = false;
static bool        _ap_demigod_saved_train    = false;
static bool        _ap_demigod_saved_roadveh  = false;
static bool        _ap_demigod_saved_aircraft = false;
static bool        _ap_demigod_saved_ship     = false;
/* Easter egg: befriend the demigod by giving it more money than its company value */
static bool        _ap_demigod_friendly       = false;  ///< true = AI has switched sides
static int64_t     _ap_demigod_money_given    = 0;      ///< cumulative money given to AI via Give Money
static int         _ap_demigod_last_transfer_year = 0;  ///< last year the friendly AI sent money to player

/** Apply vehicle type restrictions matching a demigod theme. */
static void AP_DemigodApplyTheme(const std::string &theme)
{
	/* Save player's original settings (only first time) */
	if (!_ap_demigod_veh_saved) {
		_ap_demigod_saved_train    = _settings_game.ai.ai_disable_veh_train;
		_ap_demigod_saved_roadveh  = _settings_game.ai.ai_disable_veh_roadveh;
		_ap_demigod_saved_aircraft = _settings_game.ai.ai_disable_veh_aircraft;
		_ap_demigod_saved_ship     = _settings_game.ai.ai_disable_veh_ship;
		_ap_demigod_veh_saved = true;
	}
	/* Default: all disabled, then enable themed type */
	bool t = true, r = true, a = true, s = true;
	if (theme == "trains")    { t = false; }
	else if (theme == "road")     { r = false; }
	else if (theme == "aircraft") { a = false; }
	else if (theme == "ships")    { s = false; }
	else { t = false; r = false; a = false; s = false; } /* freight/mixed: all enabled */
	_settings_game.ai.ai_disable_veh_train    = t;
	_settings_game.ai.ai_disable_veh_roadveh  = r;
	_settings_game.ai.ai_disable_veh_aircraft = a;
	_settings_game.ai.ai_disable_veh_ship     = s;
}

/** Restore original vehicle restrictions after a demigod is defeated/removed. */
static void AP_DemigodRestoreRestrictions()
{
	if (!_ap_demigod_veh_saved) return;
	_settings_game.ai.ai_disable_veh_train    = _ap_demigod_saved_train;
	_settings_game.ai.ai_disable_veh_roadveh  = _ap_demigod_saved_roadveh;
	_settings_game.ai.ai_disable_veh_aircraft = _ap_demigod_saved_aircraft;
	_settings_game.ai.ai_disable_veh_ship     = _ap_demigod_saved_ship;
	_ap_demigod_veh_saved = false;
}

/** Demigod trash-talk chat lines. */
static const char *kDemigodChatLines[] = {
	"Your pathetic little trains amuse me. Watch a REAL company at work!",
	"I've built more track in one year than you have in your entire career.",
	"The God of Wackens didn't send me here to lose. Get ready to cry!",
	"Oh, you built a bus route? How adorable. I'm building an EMPIRE.",
	"Is that the best you can do? My wagons carry more cargo than your whole fleet!",
	"You call that a station? I've seen better infrastructure in the stone age!",
	"The God of Wackens is watching. And he's laughing at your routes.",
	"I'll be taking that subsidy, thank you very much!",
	"Pro tip: try building something useful for once.",
	"Do you even know what a signal is? Asking for a friend.",
	"While you were reading this, I already built 3 new routes.",
	"My trains run on time. Can you say the same? Didn't think so.",
	"Cute little company you've got there. Would be a shame if someone... out-competed it.",
	"I was moving cargo before you even laid your first track.",
	"The God of Wackens sends his regards. And his freight trains.",
};

/** Friendly chat lines — used after the player befriends the demigod. */
static const char *kDemigodFriendlyLines[] = {
	"You know what, you're alright! Let me handle the freight for you.",
	"Best business partner I ever had. The God of Wackens can stuff it!",
	"I'm routing extra cargo your way. Don't tell the Elder Gods!",
	"Who needs divine orders when you have a friend with deep pockets?",
	"I'll keep the trains running. You keep being awesome!",
	"The God of Wackens is going to be SO angry when he finds out about us.",
	"Your generosity has won my loyalty. Here, have some of my profits!",
	"Working together beats working against each other. Who knew?",
	"I used to hate your routes. Now I think they're... adequate. That's high praise from me!",
	"Consider me your silent partner. Well, not so silent, but you get the idea.",
	"I'm sending my annual dividend your way. Friendship has its perks!",
	"The other demigods would never believe this. ME, friends with a mortal?",
};

/** Calculate dynamic tribute cost: frozen 50% player value + 50% AI current value, cap 300M.
 *  Returns 0 when the demigod is friendly (free dismiss). */
static int64_t AP_CalcDemigodTributeCost()
{
	if (_ap_demigod_active_idx < 0) return 0;
	/* Friendly demigod = free dismiss */
	if (_ap_demigod_friendly) return 0;
	/* Frozen part: 50% of player's company value at spawn time */
	int64_t frozen_half = _ap_demigod_player_value_at_spawn / 2;
	/* Dynamic part: 50% of AI's current company value */
	int64_t ai_half = 0;
	Company *ai_co = Company::GetIfValid(_ap_demigod_active_company);
	if (ai_co != nullptr) ai_half = CalculateCompanyValue(ai_co, true) / 2;
	int64_t total = frozen_half + ai_half;
	/* Cap at 300 million */
	return std::min(total, (int64_t)300000000);
}

/** Send periodic trash-talk (or friendly chat) from the demigod AI. */
static void AP_DemigodChat()
{
	if (_ap_demigod_active_idx < 0 || _ap_demigod_pending_name) return;
	if (_ap_demigod_chat_cooldown > 0) { _ap_demigod_chat_cooldown--; return; }
	/* Random chance ~1/300 per tick = roughly every 75 seconds */
	if (InteractiveRandomRange(300) != 0) return;

	const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
	const char *line;
	if (_ap_demigod_friendly) {
		int idx = (int)InteractiveRandomRange((uint32_t)(sizeof(kDemigodFriendlyLines) / sizeof(kDemigodFriendlyLines[0])));
		line = kDemigodFriendlyLines[idx];
	} else {
		int idx = (int)InteractiveRandomRange((uint32_t)(sizeof(kDemigodChatLines) / sizeof(kDemigodChatLines[0])));
		line = kDemigodChatLines[idx];
	}
	std::string msg = fmt::format("[{}] {}", def.name, line);
	NetworkAddChatMessage(CC_DEFAULT, 10, msg);
	_ap_demigod_chat_cooldown = 60; /* minimum ~15 seconds between messages */
}

/** Called when player gives money to any company via CmdGiveMoney.
 *  Checks if it's the demigod company and whether the threshold is crossed. */
void AP_OnMoneyGivenToCompany(int dest_company, int64_t amount)
{
	if (_ap_demigod_active_idx < 0 || _ap_demigod_friendly) return;
	if ((CompanyID)(uint8_t)dest_company != _ap_demigod_active_company) return;

	_ap_demigod_money_given += amount;

	/* Check threshold: total money given > AI's current company value */
	Company *ai_co = Company::GetIfValid(_ap_demigod_active_company);
	if (ai_co == nullptr) return;
	int64_t ai_value = CalculateCompanyValue(ai_co, true);

	if (_ap_demigod_money_given > ai_value) {
		/* Befriend! */
		_ap_demigod_friendly = true;
		const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];

		/* Announce the friendship in chat */
		std::string msg = fmt::format("[{}] Wait... you're actually being NICE to me? "
			"Nobody's ever done that before. I... I'm switching sides!", def.name);
		NetworkAddChatMessage(CC_DEFAULT, 15, msg);

		AP_ShowNews(fmt::format("[AP] The {} has been won over by your generosity! "
			"They now work for YOU!", def.name));

		_ap_demigod_chat_cooldown = 40; /* brief pause before friendly chat starts */

		Debug(misc, 0, "[AP] Demigod befriended! money_given={} ai_value={}", _ap_demigod_money_given, ai_value);
		AP_TRACE(fmt::format("DemigodBefriended: money_given={} ai_value={}", _ap_demigod_money_given, ai_value));

		_ap_status_dirty.store(true);
	}
}

/** Yearly transfer: friendly demigod sends half its account to the player. */
static void AP_DemigodFriendlyTransfer()
{
	if (!_ap_demigod_friendly || _ap_demigod_active_idx < 0) return;

	int cur_year = (int)TimerGameCalendar::year.base();
	if (_ap_demigod_last_transfer_year >= cur_year) return; /* already done this year */
	_ap_demigod_last_transfer_year = cur_year;

	Company *ai_co = Company::GetIfValid(_ap_demigod_active_company);
	if (ai_co == nullptr || ai_co->money <= 0) return;

	Money transfer = ai_co->money / 2;
	if (transfer <= 0) return;

	/* Take from AI */
	ai_co->money -= transfer;

	/* Give to player */
	CompanyID cid = _local_company;
	Company *player = Company::GetIfValid(cid);
	if (player != nullptr) {
		AP_ChangeMoney(cid, transfer);
	}

	const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
	std::string msg = fmt::format("[{}] Here's your annual dividend: {}! "
		"Friendship pays... literally.", def.name, AP_Money(transfer));
	NetworkAddChatMessage(CC_DEFAULT, 12, msg);

	Debug(misc, 0, "[AP] Demigod friendly transfer: {} to player", (int64_t)transfer);
}

/** God of Wackens newspaper lines for spawning. */
static const char *kWackensSpawnLines[] = {
	"The God of Wackens has grown bored... He summons the {} to humble the mortal transport companies!",
	"The God of Wackens has noticed your inefficient logistics. He sends the {} to show you how it's done.",
	"The God of Wackens has awakened after a 300 year nap. He is immediately disappointed in your logistics. The {} arrives!",
	"The God of Wackens is displeased. He sends forth the {}!",
	"A divine rumble echoes across the land. The God of Wackens conjures the {}!",
};

/** Spawn the next undefeated demigod. */
static void AP_DemigodSpawn()
{
	/* Pick next undefeated demigod */
	int pick = -1;
	for (int i = 0; i < (int)_ap_demigod_defs.size(); i++) {
		if (_ap_demigod_defeated.count(_ap_demigod_defs[i].location) == 0) {
			pick = i;
			break;
		}
	}
	if (pick < 0) return; /* all defeated */

	/* Check company slot availability (max 15) */
	if (!Company::CanAllocateItem()) {
		AP_ShowNews("[AP] The God of Wackens tries to send a demigod, but the world is too crowded! (Company limit reached)");
		_ap_demigod_next_spawn_year++; /* retry next year */
		return;
	}

	const APDemigodDef &def = _ap_demigod_defs[pick];

	/* Capture player's company value at spawn for tribute calculation + starting money */
	Company *player = Company::GetIfValid(_local_company);
	_ap_demigod_player_value_at_spawn = (player != nullptr)
	    ? (int64_t)CalculateCompanyValue(player, true) : (int64_t)0;

	/* Apply vehicle type restrictions for this theme */
	AP_DemigodApplyTheme(def.theme);

	/* Create AI company */
	Command<CMD_COMPANY_CTRL>::Post(CCA_NEW_AI, CompanyID::Invalid(), CRR_NONE, INVALID_CLIENT_ID);

	_ap_demigod_active_idx = pick;
	_ap_demigod_pending_name = true; /* will set name on next tick */
	_ap_demigod_chat_cooldown = 20;  /* 5 seconds grace before first chat */
	_ap_demigod_friendly = false;    /* reset friendship for new demigod */
	_ap_demigod_money_given = 0;
	_ap_demigod_last_transfer_year = 0;

	/* Show God of Wackens newspaper */
	int line_idx = (int)(InteractiveRandomRange((uint32_t)(sizeof(kWackensSpawnLines) / sizeof(kWackensSpawnLines[0]))));
	AP_ShowNews(fmt::format(fmt::runtime(kWackensSpawnLines[line_idx]), def.name));

	Debug(misc, 0, "[AP] Demigod spawned: {} (theme={}, tribute={})", def.name, def.theme, def.tribute_cost);
	AP_TRACE(fmt::format("DemigodSpawn: name='{}' theme='{}' tribute={} idx={}", def.name, def.theme, def.tribute_cost, pick));
}

/** Calculate the next spawn year from the current year + random interval. */
static void AP_DemigodScheduleNext()
{
	int cur = (int)TimerGameCalendar::year.base();
	int min_iv = std::max(1, _ap_demigod_interval_min);
	int max_iv = std::max(min_iv, _ap_demigod_interval_max);
	_ap_demigod_next_spawn_year = cur + min_iv + (int)InteractiveRandomRange((uint32_t)(max_iv - min_iv + 1));
	Debug(misc, 0, "[AP] Next demigod spawn scheduled for year {}", _ap_demigod_next_spawn_year);
	AP_TRACE(fmt::format("DemigodScheduleNext: year={}", _ap_demigod_next_spawn_year));
}

/** Called every realtime tick. Handles pending naming, company health, and spawn timer. */
static void AP_DemigodTick()
{
	if (!_ap_demigod_enabled) return;

	/* Handle pending naming — find the newest AI company and set its name */
	if (_ap_demigod_pending_name && _ap_demigod_active_idx >= 0 &&
	    _ap_demigod_active_idx < (int)_ap_demigod_defs.size()) {
		CompanyID newest = CompanyID::Invalid();
		for (const Company *c : Company::Iterate()) {
			if (c->is_ai && (newest == CompanyID::Invalid() || c->index > newest)) {
				newest = c->index;
			}
		}
		if (newest != CompanyID::Invalid()) {
			Company *c = Company::Get(newest);
			const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
			c->name = def.name;
			c->president_name = def.president_name;
			/* Give demigod 50% of player's company value as starting money */
			int64_t bonus = _ap_demigod_player_value_at_spawn / 2;
			if (bonus > 0) {
				c->money += bonus;
				Debug(misc, 0, "[AP] Demigod starting bonus: {}", bonus);
			}
			_ap_demigod_active_company = newest;
			_ap_demigod_pending_name = false;
			InvalidateWindowClassesData(WC_COMPANY);
			MarkWholeScreenDirty(); /* Force full redraw — CCA_NEW_AI doesn't do this, leaving dirty rects on toolbar/viewport */
			Debug(misc, 0, "[AP] Demigod company named: {} (CompanyID={}, bonus={})", def.name, (int)newest.base(), bonus);
		}
	}

	/* Check if active demigod's company was deleted externally (bankrupt, etc.) */
	if (_ap_demigod_active_idx >= 0 && _ap_demigod_active_idx < (int)_ap_demigod_defs.size() &&
	    !_ap_demigod_pending_name) {
		if (!Company::IsValidID(_ap_demigod_active_company)) {
			const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
			AP_ShowNews(fmt::format("[AP] The {} has gone bankrupt. The God of Wackens is disappointed.", def.name));
			AP_DemigodRestoreRestrictions();
			_ap_demigod_active_idx = -1;
			_ap_demigod_active_company = CompanyID::Invalid();
			/* Don't send AP check — not a proper defeat. Schedule next. */
			if ((int)_ap_demigod_defeated.size() < (int)_ap_demigod_defs.size()) {
				AP_DemigodScheduleNext();
			}
		}
	}

	/* Demigod trash-talk (or friendly chat) */
	AP_DemigodChat();

	/* Friendly demigod: yearly dividend transfer to player */
	AP_DemigodFriendlyTransfer();

	/* Spawn timer — check if it's time to spawn */
	if (_ap_demigod_active_idx < 0 && !_ap_demigod_pending_name && _ap_demigod_next_spawn_year > 0) {
		int cur_year = (int)TimerGameCalendar::year.base();
		if (cur_year >= _ap_demigod_next_spawn_year) {
			AP_DemigodSpawn();
		}
	}
}

/** Called during first-tick session setup. Takes slot_data by reference. */
static void AP_DemigodInit(const APSlotData &sd)
{
	_ap_demigod_enabled      = sd.demigod_enabled;
	_ap_demigod_defs         = sd.demigods;
	_ap_demigod_interval_min = sd.demigod_spawn_interval_min;
	_ap_demigod_interval_max = sd.demigod_spawn_interval_max;
	/* Don't clear _ap_demigod_defeated — loaded from savegame. */
	/* If enabled and no active demigod and no spawn scheduled, schedule the first one. */
	if (_ap_demigod_enabled && _ap_demigod_active_idx < 0 && _ap_demigod_next_spawn_year == 0) {
		AP_DemigodScheduleNext();
	}
	/* If an active demigod is loaded from save, re-apply vehicle restrictions */
	if (_ap_demigod_active_idx >= 0 && _ap_demigod_active_idx < (int)_ap_demigod_defs.size()) {
		if (Company::IsValidID(_ap_demigod_active_company)) {
			/* Vehicle restrictions are already saved in the APST chunk, just re-apply theme */
			const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
			/* Don't call AP_DemigodApplyTheme — saved restrictions were already persisted.
			 * Just set the settings directly based on theme. */
			bool t = true, r = true, a = true, s = true;
			if (def.theme == "trains")    { t = false; }
			else if (def.theme == "road")     { r = false; }
			else if (def.theme == "aircraft") { a = false; }
			else if (def.theme == "ships")    { s = false; }
			else { t = false; r = false; a = false; s = false; }
			_settings_game.ai.ai_disable_veh_train    = t;
			_settings_game.ai.ai_disable_veh_roadveh  = r;
			_settings_game.ai.ai_disable_veh_aircraft = a;
			_settings_game.ai.ai_disable_veh_ship     = s;
		} else {
			/* Company gone after load — clean up */
			AP_DemigodRestoreRestrictions();
			_ap_demigod_active_idx = -1;
			_ap_demigod_active_company = CompanyID::Invalid();
			if ((int)_ap_demigod_defeated.size() < (int)_ap_demigod_defs.size()) {
				AP_DemigodScheduleNext();
			}
		}
	}
	Debug(misc, 0, "[AP] Demigod init: enabled={} defs={} defeated={} active={} next_year={}",
	      _ap_demigod_enabled, _ap_demigod_defs.size(), _ap_demigod_defeated.size(),
	      _ap_demigod_active_idx, _ap_demigod_next_spawn_year);
}

/** Tribute payment or friendly dismiss — defeat/remove the active demigod. Called from GUI. */
void AP_DemigodDefeat()
{
	if (_ap_demigod_active_idx < 0 || _ap_demigod_active_idx >= (int)_ap_demigod_defs.size()) return;
	const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
	bool was_friendly = _ap_demigod_friendly;

	/* Check player has enough money — use dynamic tribute cost (0 when friendly) */
	int64_t tribute = AP_CalcDemigodTributeCost();
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr || c->money < (Money)tribute) return;

	/* Deduct tribute cost (0 when friendly = free) */
	if (tribute > 0) {
		AP_ChangeMoney(cid, -(Money)tribute);
	}

	/* Delete demigod company */
	if (Company::IsValidID(_ap_demigod_active_company)) {
		Command<CMD_COMPANY_CTRL>::Post(CCA_DELETE, _ap_demigod_active_company, CRR_MANUAL, INVALID_CLIENT_ID);
	}

	/* Restore vehicle restrictions */
	AP_DemigodRestoreRestrictions();

	/* Send AP location check */
	if (_ap_client != nullptr) {
		_ap_client->SendCheckByName(def.location);
	}

	/* Record defeat */
	_ap_demigod_defeated.insert(def.location);
	AP_TRACE(fmt::format("DemigodDefeated: name='{}' location='{}' tribute={} friendly={} defeated_count={}/{}",
		def.name, def.location, tribute, was_friendly, _ap_demigod_defeated.size(), _ap_demigod_defs.size()));

	/* Show news */
	if (was_friendly) {
		AP_ShowNews(fmt::format("[AP] You have dismissed the {}. They wave goodbye and ride off into the sunset.", def.name));
	} else {
		AP_ShowNews(fmt::format("[AP] You have paid tribute to the Elder Gods! The {} fades from existence.", def.name));
	}

	/* Clean up */
	_ap_demigod_active_idx = -1;
	_ap_demigod_active_company = CompanyID::Invalid();
	_ap_demigod_friendly = false;
	_ap_demigod_money_given = 0;
	_ap_demigod_last_transfer_year = 0;

	/* Check if all defeated */
	if ((int)_ap_demigod_defeated.size() >= (int)_ap_demigod_defs.size()) {
		AP_ShowNews("[AP] All demigods have been defeated! The God of Wackens acknowledges your supremacy!");
	} else {
		AP_DemigodScheduleNext();
	}

	_ap_status_dirty.store(true);
}

/** Status snapshot for the GUI. */
DemigodStatus AP_GetDemigodStatus()
{
	DemigodStatus s{};
	s.enabled        = _ap_demigod_enabled;
	s.active         = (_ap_demigod_active_idx >= 0 && !_ap_demigod_pending_name);
	s.defeated_count = (int)_ap_demigod_defeated.size();
	s.total_count    = (int)_ap_demigod_defs.size();
	s.active_idx     = _ap_demigod_active_idx;
	s.next_spawn_year = _ap_demigod_next_spawn_year;
	s.active_company = -1;
	s.friendly       = false;
	if (s.active && _ap_demigod_active_idx < (int)_ap_demigod_defs.size()) {
		const APDemigodDef &def = _ap_demigod_defs[_ap_demigod_active_idx];
		s.active_name    = def.name;
		s.active_theme   = def.theme;
		s.tribute_cost   = AP_CalcDemigodTributeCost();
		s.active_company = (int)_ap_demigod_active_company.base();
		s.friendly       = _ap_demigod_friendly;
	}
	return s;
}

bool AP_IsDemigodEnabled()
{
	return _ap_demigod_enabled && !_ap_demigod_defs.empty();
}

/** Save/load helpers for demigod state. */
void AP_GetDemigodState(std::string *defeated, int *active_idx, int *company,
                        int *next_year, bool *veh_saved, bool *sv_train,
                        bool *sv_road, bool *sv_air, bool *sv_ship)
{
	/* Serialize defeated set as comma-separated location names */
	std::string d;
	for (const auto &loc : _ap_demigod_defeated) {
		if (!d.empty()) d += ',';
		d += loc;
	}
	*defeated   = d;
	*active_idx = _ap_demigod_active_idx;
	*company    = (_ap_demigod_active_company != CompanyID::Invalid())
	              ? (int)_ap_demigod_active_company.base() : -1;
	*next_year  = _ap_demigod_next_spawn_year;
	*veh_saved  = _ap_demigod_veh_saved;
	*sv_train   = _ap_demigod_saved_train;
	*sv_road    = _ap_demigod_saved_roadveh;
	*sv_air     = _ap_demigod_saved_aircraft;
	*sv_ship    = _ap_demigod_saved_ship;
}

void AP_SetDemigodState(const std::string &defeated, int active_idx, int company,
                        int next_year, bool veh_saved, bool sv_train,
                        bool sv_road, bool sv_air, bool sv_ship)
{
	_ap_demigod_defeated.clear();
	if (!defeated.empty()) {
		const char *p   = defeated.data();
		const char *end = p + defeated.size();
		while (p < end) {
			const char *comma = (const char *)memchr(p, ',', (size_t)(end - p));
			if (comma == nullptr) comma = end;
			if (comma > p) _ap_demigod_defeated.insert(std::string(p, comma));
			p = (comma < end) ? comma + 1 : end;
		}
	}
	_ap_demigod_active_idx     = active_idx;
	_ap_demigod_active_company = (company >= 0) ? (CompanyID)(uint8_t)company : CompanyID::Invalid();
	_ap_demigod_next_spawn_year = next_year;
	_ap_demigod_veh_saved      = veh_saved;
	_ap_demigod_saved_train    = sv_train;
	_ap_demigod_saved_roadveh  = sv_road;
	_ap_demigod_saved_aircraft = sv_air;
	_ap_demigod_saved_ship     = sv_ship;
}

int64_t AP_GetDemigodPlayerSpawnValue() { return _ap_demigod_player_value_at_spawn; }
void    AP_SetDemigodPlayerSpawnValue(int64_t v) { _ap_demigod_player_value_at_spawn = v; }

bool    AP_GetDemigodFriendly() { return _ap_demigod_friendly; }
void    AP_SetDemigodFriendly(bool v) { _ap_demigod_friendly = v; }
int64_t AP_GetDemigodMoneyGiven() { return _ap_demigod_money_given; }
void    AP_SetDemigodMoneyGiven(int64_t v) { _ap_demigod_money_given = v; }
int     AP_GetDemigodLastTransferYear() { return _ap_demigod_last_transfer_year; }
void    AP_SetDemigodLastTransferYear(int v) { _ap_demigod_last_transfer_year = v; }

/* Day/Night cycle REMOVED — GfxLoadSprites()+StartupEngines() mid-game
 * broke economy (cargo payments zeroed, engine states corrupted). */

/** Open the current singleplayer game to multiplayer.
 *  Saves the game, then reloads it as a network server. */
void AP_OpenToMultiplayer()
{
	extern bool _is_network_server;
	extern SwitchMode _switch_mode;

	if (_networking && _network_server) {
		AP_ShowConsole("[AP] Already running as a multiplayer server.");
		return;
	}
	if (_game_mode != GM_NORMAL) {
		AP_ShowConsole("[AP] Can only open to multiplayer from a running game.");
		return;
	}

	/* Save the current game to a temporary file */
	static const std::string kMPSave = "ap_multiplayer.sav";
	if (SaveOrLoad(kMPSave, SLO_SAVE, DFT_GAME_FILE, SAVE_DIR) != SL_OK) {
		AP_ShowConsole("[AP] Failed to save game for multiplayer conversion.");
		return;
	}

	/* Set up the file-to-load structure so SM_LOAD_GAME knows what to load */
	_file_to_saveload.SetMode(FIOS_TYPE_FILE, SLO_LOAD);
	std::string full = FioFindFullPath(SAVE_DIR, kMPSave);
	_file_to_saveload.name = full.empty() ? kMPSave : full;

	/* Ensure the server has a valid client name for multiplayer.
	 * Without this, SERVER_CLIENT_INFO sends an empty name which
	 * causes joining clients to reject the packet as malformed. */
	if (_settings_client.network.client_name.empty()) {
		_settings_client.network.client_name = _ap_last_slot.empty() ? "Host" : _ap_last_slot;
	}

	/* Tell the engine we want to be a server on next mode switch */
	_is_network_server = true;

	/* Trigger a reload — SwitchToMode will see _is_network_server and call NetworkServerStart() */
	_switch_mode = SM_LOAD_GAME;

	AP_ShowConsole("[AP] Opening game to multiplayer... Reloading as server.");
	Debug(misc, 0, "[AP] OpenToMultiplayer: saving as '{}' and reloading as server", kMPSave);
}


/* =========================================================================
 * WRATH OF THE GOD OF WACKENS
 *
 * Tracks player destruction (houses, roads, terrain, trees) via yearly
 * counters.  When limits are exceeded the divine anger meter (0-5) rises
 * and escalating punishments are applied.  Anger decays 1 level per year
 * if the player behaves.
 * ========================================================================= */

static bool _ap_wrath_enabled          = false;
static int  _ap_wrath_anger            = 0;      ///< 0=Peaceful .. 5=Divine Wrath
static int  _ap_wrath_houses_year      = 0;
static int  _ap_wrath_roads_year       = 0;
static int  _ap_wrath_terrain_year     = 0;
static int  _ap_wrath_trees_year       = 0;
static int  _ap_wrath_last_eval_year   = 0;
static int  _ap_wrath_limit_houses     = 2;
static int  _ap_wrath_limit_roads      = 2;
static int  _ap_wrath_limit_terrain    = 25;
static int  _ap_wrath_limit_trees      = 10;

/* ── Tracking (called from *_cmd.cpp hooks) ─────────────────────── */

void AP_WrathTrackHouse()  { if (_ap_wrath_enabled) { _ap_wrath_houses_year++;  AP_TRACE(fmt::format("Wrath: house demolished (total this year: {})", _ap_wrath_houses_year)); } }
void AP_WrathTrackRoad()   { if (_ap_wrath_enabled) { _ap_wrath_roads_year++;   AP_TRACE(fmt::format("Wrath: road demolished (total this year: {})", _ap_wrath_roads_year)); } }
void AP_WrathTrackTerrain(){ if (_ap_wrath_enabled) { _ap_wrath_terrain_year++; AP_TRACE(fmt::format("Wrath: terrain modified (total this year: {})", _ap_wrath_terrain_year)); } }
void AP_WrathTrackTree()   { if (_ap_wrath_enabled) { _ap_wrath_trees_year++;   AP_TRACE(fmt::format("Wrath: tree removed (total this year: {})", _ap_wrath_trees_year)); } }

/* ── Newspaper messages ─────────────────────────────────────────── */

static const char *kWrathEscalateLines[] = {
	"The God of Wackens stirs in his slumber. He has noticed your... remodeling.",
	"The God of Wackens is growing irritated. He suggests you stop bulldozing everything.",
	"The God of Wackens clenches his holy fist! Your reckless destruction has angered him!",
	"The God of Wackens is FURIOUS! The ground trembles beneath your tracks!",
	"THE GOD OF WACKENS HAS UNLEASHED HIS DIVINE WRATH! You should have listened!",
};

static const char *kWrathDecayLines[] = {
	"The God of Wackens has calmed down. He approves of your restraint.",
	"The God of Wackens is less irritated. Perhaps there is hope for you yet.",
	"The God of Wackens takes a deep breath. His fury subsides... for now.",
	"The God of Wackens unclenches his fist. But he is still watching you.",
	"The God of Wackens returns to a peaceful meditation. Do not ruin it.",
};

static const char *kWrathPunishLines[] = {
	"",  /* level 0: never used */
	"",  /* level 1: escalation message is the warning */
	"The God of Wackens has turned the towns against you! Town ratings plummet!",
	"The God of Wackens smites your fleet! Breakdowns everywhere!",
	"The God of Wackens tears at your infrastructure! Roads and rails crumble!",
	"THE GOD OF WACKENS UNLEASHES TOTAL DESTRUCTION! Sinkholes! Vanishing stations! Signal chaos!",
};

/* ── Punishment helpers ─────────────────────────────────────────── */

/** Level 4: Remove a handful of random player-owned rail tiles. */
static void AP_WrathInfraDamage(CompanyID cid)
{
	std::vector<TileIndex> owned_rail;
	for (TileIndex t = TileIndex{}; t < Map::Size(); t++) {
		if (IsTileType(t, MP_RAILWAY) && GetTileOwner(t) == cid) {
			owned_rail.push_back(t);
		}
	}
	int to_remove = std::min((int)owned_rail.size(), 5 + (int)InteractiveRandomRange(4));
	for (int i = 0; i < to_remove && !owned_rail.empty(); i++) {
		int idx = (int)InteractiveRandomRange((uint32_t)owned_rail.size());
		Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlag::Execute, owned_rail[idx]);
		owned_rail[idx] = owned_rail.back();
		owned_rail.pop_back();
	}
}

/** Level 5: Create a crater by clearing and lowering a 5x5 area around random player infrastructure. */
static void AP_WrathSinkhole(CompanyID cid)
{
	std::vector<TileIndex> targets;
	for (TileIndex t = TileIndex{}; t < Map::Size(); t++) {
		if ((IsTileType(t, MP_RAILWAY) || IsTileType(t, MP_STATION)) &&
		    GetTileOwner(t) == cid) {
			targets.push_back(t);
		}
	}
	if (targets.empty()) return;
	TileIndex center = targets[InteractiveRandomRange((uint32_t)targets.size())];

	for (int dx = -2; dx <= 2; dx++) {
		for (int dy = -2; dy <= 2; dy++) {
			TileIndex t = TileAddWrap(center, dx, dy);
			if (t == INVALID_TILE) continue;
			Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlag::Execute, t);
			/* Lower terrain at each corner */
			Command<CMD_TERRAFORM_LAND>::Do(DoCommandFlag::Execute, t, SLOPE_N, false);
			Command<CMD_TERRAFORM_LAND>::Do(DoCommandFlag::Execute, t, SLOPE_S, false);
			Command<CMD_TERRAFORM_LAND>::Do(DoCommandFlag::Execute, t, SLOPE_E, false);
			Command<CMD_TERRAFORM_LAND>::Do(DoCommandFlag::Execute, t, SLOPE_W, false);
		}
	}
}

/** Level 5: Delete 1-2 random player stations. */
static void AP_WrathStationDelete(CompanyID cid)
{
	std::vector<StationID> player_stations;
	for (Station *st : Station::Iterate()) {
		if (st->owner == cid) player_stations.push_back(st->index);
	}
	int to_delete = std::min((int)player_stations.size(), 1 + (int)InteractiveRandomRange(2));
	for (int i = 0; i < to_delete && !player_stations.empty(); i++) {
		int idx = (int)InteractiveRandomRange((uint32_t)player_stations.size());
		Station *victim = Station::GetIfValid(player_stations[idx]);
		if (victim != nullptr) {
			AP_ShowNews(fmt::format("The earth swallows {}!", victim->GetCachedName()));
			Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlag::Execute, victim->xy);
		}
		player_stations[idx] = player_stations.back();
		player_stations.pop_back();
	}
}

/** Level 5: All trains break down + fuel shortage. */
static void AP_WrathSignalMadness(CompanyID cid)
{
	for (Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == cid && v->IsPrimaryVehicle() && v->type == VEH_TRAIN) {
			if (v->breakdown_ctr == 0) {
				v->breakdown_ctr   = 2;
				v->breakdown_delay = 255;
			}
		}
	}
	/* Also halve all vehicle speeds immediately */
	for (Vehicle *v2 : Vehicle::Iterate()) {
		if (v2->owner == cid && v2->IsPrimaryVehicle())
			v2->cur_speed = v2->cur_speed / 2;
	}
}

/** Apply punishment for the given anger level. */
static void AP_WrathPunish(int level)
{
	AP_TRACE(fmt::format("WrathPunish: level={}", level));
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;

	switch (level) {
		case 1: /* Newspaper warning only — already shown by escalation message */
			break;

		case 2: /* Town ratings drop */
			for (Town *t : Town::Iterate()) {
				ChangeTownRating(t, -200, RATING_MINIMUM, DoCommandFlag::Execute);
			}
			AP_ShowNews(kWrathPunishLines[2]);
			break;

		case 3: { /* Breakdowns + financial fine */
			for (Vehicle *v : Vehicle::Iterate()) {
				if (v->owner == cid && v->IsPrimaryVehicle()) {
					v->breakdown_chance = 255;
					v->reliability = 1;
				}
			}
			Money fine = std::max((Money)10000LL, c->money / 4);
			AP_ChangeMoney(cid, -fine);
			AP_ShowNews(fmt::format("{} Divine fine: {}!",
			    kWrathPunishLines[3], AP_Money(fine)));
			break;
		}

		case 4: /* Infrastructure damage */
			AP_WrathInfraDamage(cid);
			AP_ShowNews(kWrathPunishLines[4]);
			break;

		case 5: /* Full divine wrath */
			AP_WrathSinkhole(cid);
			AP_WrathStationDelete(cid);
			AP_WrathSignalMadness(cid);
			AP_ShowNews(kWrathPunishLines[5]);
			break;
	}

	_ap_status_dirty.store(true, std::memory_order_relaxed);
}

/* ── Yearly evaluation ──────────────────────────────────────────── */

/** Called from realtime timer every tick — detects year boundary and evaluates. */
static void AP_WrathYearlyEval()
{
	if (!_ap_wrath_enabled) return;

	int cur_year = (int)TimerGameCalendar::year.base();
	if (cur_year <= _ap_wrath_last_eval_year) return;
	_ap_wrath_last_eval_year = cur_year;

	/* Check if any category exceeded its limit */
	bool exceeded = (_ap_wrath_houses_year  > _ap_wrath_limit_houses) ||
	                (_ap_wrath_roads_year   > _ap_wrath_limit_roads)  ||
	                (_ap_wrath_terrain_year > _ap_wrath_limit_terrain)||
	                (_ap_wrath_trees_year   > _ap_wrath_limit_trees);

	int old_anger = _ap_wrath_anger;

	if (exceeded) {
		int over_count = 0;
		if (_ap_wrath_houses_year  > _ap_wrath_limit_houses)  over_count++;
		if (_ap_wrath_roads_year   > _ap_wrath_limit_roads)   over_count++;
		if (_ap_wrath_terrain_year > _ap_wrath_limit_terrain)  over_count++;
		if (_ap_wrath_trees_year   > _ap_wrath_limit_trees)   over_count++;
		int increase = (over_count >= 3) ? 2 : 1;
		_ap_wrath_anger = std::min(5, _ap_wrath_anger + increase);
	} else {
		if (_ap_wrath_anger > 0) _ap_wrath_anger--;
	}

	/* Newspaper on anger change */
	if (_ap_wrath_anger > old_anger && _ap_wrath_anger >= 1 && _ap_wrath_anger <= 5) {
		AP_ShowNews(kWrathEscalateLines[_ap_wrath_anger - 1]);
	} else if (_ap_wrath_anger < old_anger) {
		int idx = std::min(_ap_wrath_anger, 4);
		AP_ShowNews(kWrathDecayLines[idx]);
	}

	/* Punishment on escalation */
	if (_ap_wrath_anger > old_anger) {
		AP_WrathPunish(_ap_wrath_anger);
	}

	/* Log BEFORE resetting counters so we see actual values */
	Debug(misc, 0, "[AP] Wrath yearly eval: anger {} -> {} (exceeded={} H={}/{} R={}/{} T={}/{} Tr={}/{})",
	      old_anger, _ap_wrath_anger, exceeded,
	      _ap_wrath_houses_year, _ap_wrath_limit_houses,
	      _ap_wrath_roads_year, _ap_wrath_limit_roads,
	      _ap_wrath_terrain_year, _ap_wrath_limit_terrain,
	      _ap_wrath_trees_year, _ap_wrath_limit_trees);
	AP_TRACE(fmt::format("WrathYearlyEval: anger {} -> {} exceeded={} houses={}/{} roads={}/{} terrain={}/{} trees={}/{}",
		old_anger, _ap_wrath_anger, exceeded,
		_ap_wrath_houses_year, _ap_wrath_limit_houses,
		_ap_wrath_roads_year, _ap_wrath_limit_roads,
		_ap_wrath_terrain_year, _ap_wrath_limit_terrain,
		_ap_wrath_trees_year, _ap_wrath_limit_trees));

	/* Reset yearly counters AFTER logging */
	_ap_wrath_houses_year  = 0;
	_ap_wrath_roads_year   = 0;
	_ap_wrath_terrain_year = 0;
	_ap_wrath_trees_year   = 0;
}

/* ── Init / Saveload ────────────────────────────────────────────── */

static void AP_WrathInit(const APSlotData &sd)
{
	_ap_wrath_enabled       = sd.wrath_enabled;
	_ap_wrath_limit_houses  = sd.wrath_limit_houses;
	_ap_wrath_limit_roads   = sd.wrath_limit_roads;
	_ap_wrath_limit_terrain = sd.wrath_limit_terrain;
	_ap_wrath_limit_trees   = sd.wrath_limit_trees;
	if (_ap_wrath_last_eval_year == 0) {
		/* Set to year-1 so the first evaluation runs at the end of the starting year */
		_ap_wrath_last_eval_year = (int)TimerGameCalendar::year.base() - 1;
	}
	if (_ap_wrath_enabled) {
		Debug(misc, 0, "[AP] Wrath system enabled (limits: H={} R={} T={} Tr={})",
		      _ap_wrath_limit_houses, _ap_wrath_limit_roads,
		      _ap_wrath_limit_terrain, _ap_wrath_limit_trees);
	}
}

void AP_GetWrathState(int *anger, int *houses, int *roads, int *terrain, int *trees, int *last_eval_year)
{
	*anger          = _ap_wrath_anger;
	*houses         = _ap_wrath_houses_year;
	*roads          = _ap_wrath_roads_year;
	*terrain        = _ap_wrath_terrain_year;
	*trees          = _ap_wrath_trees_year;
	*last_eval_year = _ap_wrath_last_eval_year;
}

void AP_SetWrathState(int anger, int houses, int roads, int terrain, int trees, int last_eval_year)
{
	_ap_wrath_anger            = anger;
	_ap_wrath_houses_year      = houses;
	_ap_wrath_roads_year       = roads;
	_ap_wrath_terrain_year     = terrain;
	_ap_wrath_trees_year       = trees;
	_ap_wrath_last_eval_year   = last_eval_year;
}

WrathStatus AP_GetWrathStatus()
{
	return {
		_ap_wrath_enabled,
		_ap_wrath_anger,
		_ap_wrath_houses_year, _ap_wrath_roads_year,
		_ap_wrath_terrain_year, _ap_wrath_trees_year,
		_ap_wrath_limit_houses, _ap_wrath_limit_roads,
		_ap_wrath_limit_terrain, _ap_wrath_limit_trees,
	};
}

bool AP_IsWrathEnabled() { return _ap_wrath_enabled; }

/* -------------------------------------------------------------------------
 * Pending / deferred state
 * ---------------------------------------------------------------------- */

static APSlotData  _ap_pending_sd;
bool               _ap_pending_world_start         = false;
static bool        _ap_goal_sent                   = false;
static bool        _ap_session_started             = false; ///< True once we've done first-tick setup in GM_NORMAL
static int         _ap_breakdown_wave_ticks        = 0;     ///< >0 while breakdown wave is active (~60 seconds)
static int         _ap_fuel_shortage_ticks         = 0;     ///< >0 while fuel shortage slowdown is active
static int         _ap_cargo_bonus_ticks           = 0;     ///< >0 while 2x cargo payment is active (240 ticks = 60s)
static int         _ap_reliability_boost_ticks     = 0;     ///< >0 while reliability boost active (90 game-days)
static int         _ap_station_boost_ticks         = 0;     ///< >0 while station rating boost active (30 game-days)
static int         _ap_license_revoke_ticks        = 0;     ///< >0 while license revoke trap is active
static int         _ap_license_revoke_type         = -1;    ///< VehicleType cast to int, -1 = inactive

/* Mission tier completion counters — incremented when a check is sent for that tier */
static int         _ap_easy_completed              = 0;     ///< Easy missions completed this session
static int         _ap_medium_completed            = 0;     ///< Medium missions completed this session
static int         _ap_hard_completed              = 0;     ///< Hard missions completed this session
static int         _ap_extreme_completed           = 0;     ///< Extreme missions completed this session

bool AP_GetCargoBonusActive() { return _ap_cargo_bonus_ticks > 0; }

/* Bug B fix: reset per-connection, not global-ever */
static bool        _ap_world_started_this_session  = false;
static bool        _ap_pending_load_save            = false; ///< True when user clicked "Load Save" in connect window
static bool        _ap_waiting_for_start_choice     = false; ///< True while ShowQuery start-choice dialog is open
static bool        _ap_named_entity_refresh_needed = false; ///< Set after Load() — defer GetString calls to first game tick

/* Items received before we've entered GM_NORMAL are queued here */
static std::vector<APItem> _ap_pending_items;

/* Number of AP items we have fully processed (applied to game state).
 * Saved in APST chunk. On reconnect, AP resends all items from index 0 —
 * we skip any item whose server_index < this count to prevent double-apply.
 * NOTE: declared at the top of this file so it is available to AP_InitSessionStats(). */

/* Exposed for GUI polling */
std::atomic<bool> _ap_status_dirty{ false };

/* Public accessors */
const APSlotData &AP_GetSlotData() { return _ap_pending_sd; }
bool              AP_IsConnected()  { if (_ap_bridge_mode) return true;
                                     return _ap_client != nullptr &&
                                     _ap_client->GetState() == APState::AUTHENTICATED; }
bool              AP_IsColbyActive()     { return _ap_colby_enabled && !_ap_colby_done && _ap_colby_step >= 1; }
bool              AP_IsColbyConfigured() { return _ap_colby_enabled; }

ColbyStatus AP_GetColbyStatus() {
	ColbyStatus s;
	s.enabled       = _ap_colby_enabled;
	s.done          = _ap_colby_done;
	s.step          = _ap_colby_step;
	s.delivered     = _ap_colby_step_delivered;
	s.step_amount   = COLBY_STEP_AMOUNT;
	s.escaped       = _ap_colby_escaped;
	s.popup_shown   = _ap_colby_popup_shown;
	s.popup_pending = _ap_colby_popup_pending;
	s.escape_ticks  = _ap_colby_escape_ticks;
	s.town_name     = _ap_colby_target_name;
	s.town_id       = _ap_colby_target_town;
	s.source_tile   = _ap_colby_source_tile;
	s.cargo_name    = _ap_colby_cargo_name.empty() ? "packages" : _ap_colby_cargo_name;
	/* Source name: if stash is placed, show "Colby's Stash"; else show nearby station */
	if (_ap_colby_stash_tile != INVALID_TILE) {
		s.source_name = "Colby's Stash";
		Station *nearby = AP_ColbyFindNearbyStation(_ap_colby_stash_tile, 10);
		if (nearby != nullptr) {
			s.source_name = fmt::format("Colby's Stash (near {})", nearby->GetCachedName());
		}
	} else if (_ap_colby_source_tile != INVALID_TILE && IsTileType(_ap_colby_source_tile, MP_STATION)) {
		StationID sid = GetStationIndex(_ap_colby_source_tile);
		const Station *st = Station::GetIfValid(sid);
		if (st != nullptr) s.source_name = std::string(st->GetCachedName());
	}
	return s;
}

/* -------------------------------------------------------------------------
 * Callbacks
 * ---------------------------------------------------------------------- */

/* Forward declaration — defined later in this file */
static void AP_AssignNamedEntities();

/* MP Join state — declared here so AP_OnSlotData can access them.
 * Functions that use these are defined in the Multiplayer section below. */
static bool        _ap_mp_join_pending = false;  ///< Waiting for AP slot_data before joining MP
static std::string _ap_mp_join_server;            ///< Game server address to connect to
static std::string _ap_mp_join_slot;              ///< Slot name (used as player name)

static void AP_OnSlotData(const APSlotData &sd)
{
	AP_OK("[CALLBACK] AP_OnSlotData called on main thread!");
	Debug(misc, 1, "[AP] Slot data received. {} missions, win_diff={} cv={} start_year={}",
	      sd.missions.size(), (int)sd.win_difficulty, sd.win_target_company_value, sd.start_year);

	_ap_pending_sd      = sd;
	_ap_session_started = false; /* reset so first-tick setup runs again */
	_ap_goal_sent       = false;
	_ap_engine_map_built = false; /* rebuild map for new session */
	_ap_star_types_resolved = false; /* re-resolve star ObjectTypes on reconnect */
	_ap_ih_wagons_built  = false; /* rebuild IH wagon queue for new session */
	_ap_ih_wagon_unlocks = sd.enable_wagon_unlocks; /* cache for early-access functions */
	_ap_cargo_map_built  = false; /* rebuild cargo map for new session */
	_ap_track_dirs_inited = false; /* allow session init to re-evaluate lock state */
	_ap_road_dirs_inited  = false;
	_ap_signals_inited    = false;
	_ap_bridges_inited    = false;
	_ap_tunnels_inited    = false;
	_ap_airports_inited   = false;
	_ap_trees_inited      = false;
	_ap_terraform_inited  = false;
	_ap_town_actions_inited = false;
	_ap_status_dirty.store(true);

	/* ── MP Join path: apply NewGRFs from slot_data, then connect ── */
	if (_ap_mp_join_pending) {
		AP_OK("[OnSlotData] MP Join mode — applying NewGRFs from slot data...");
		_ap_pending_world_start = true;
		AP_ConsumeWorldStart();  /* Sets up _grfconfig_newgame with correct GRFs + params */
		_ap_mp_join_pending = false;

		/* Rescan GRFs so they're in _all_grfs for MP NewGRF check (step 2/6) */
		IConsolePrint(CC_WHITE, "[AP] Rescanning NewGRF files for multiplayer...");
		ScanNewGRFFiles(nullptr);

		/* Connect to the game server NOW */
		IConsolePrint(CC_WHITE, fmt::format("[AP] Connecting to game server: {}", _ap_mp_join_server));
		NetworkClientConnectGame(_ap_mp_join_server, COMPANY_SPECTATOR, "");
		AP_SetPendingJoinCompany0(true);
		AP_OK("[OnSlotData] MP Join complete — connecting to game server.");
		return;
	}

	/* Only auto-start world if we're on the main menu and haven't started yet */
	if (_game_mode == GM_MENU && !_ap_world_started_this_session) {
		_ap_world_started_this_session = true;
		_ap_pending_world_start        = true;
		AP_OK(fmt::format("Slot data ready — scheduling world generation (year={}, map={}x{})",
		      sd.start_year, (1 << sd.map_x), (1 << sd.map_y)));
	} else {
		AP_ERR(fmt::format("[OnSlotData] World start SKIPPED: game_mode={} world_started={}",
		      (int)_game_mode, _ap_world_started_this_session));

		/* If already in GM_NORMAL (reconnect/reload), assign named entities
		 * immediately — first-tick setup may have already fired before this
		 * slot_data arrived, so we can't rely on it to re-run. */
		if (_game_mode == GM_NORMAL) {
			AP_AssignNamedEntities();
			_ap_status_dirty.store(true);
		}

		ShowArchipelagoStatusWindow();
	}
}

static void AP_OnItemReceived(const APItem &item)
{
	Debug(misc, 1, "[AP] Item received: '{}' (id={} idx={})", item.item_name, item.item_id, item.server_index);
	AP_TRACE(fmt::format("ItemReceived: name='{}' id={} server_idx={} session_started={}",
		item.item_name, item.item_id, item.server_index, _ap_session_started));

	/* Queue items that arrive before we've entered GM_NORMAL */
	if (!_ap_session_started) {
		AP_TRACE(fmt::format("ItemQueued: '{}' (session not started, queue_size={})", item.item_name, _ap_pending_items.size() + 1));
		_ap_pending_items.push_back(item);
		return;
	}

	/* Deduplication: AP resends all items from index 0 on reconnect.
	 * server_index == -1 means index unknown (old saves / edge cases) — process anyway.
	 *
	 * Vehicle unlocks and infrastructure unlocks are IDEMPOTENT — safe to re-apply.
	 * Traps and consumables are NOT — they must be skipped on replay. */
	bool is_replay = (item.server_index >= 0 && item.server_index < _ap_items_received_count);

	/* Advance counter to one past this item's index (only for genuinely new items) */
	if (!is_replay && item.server_index >= 0) {
		_ap_items_received_count = item.server_index + 1;
	}

	if (item.item_name.empty()) {
		Debug(misc, 1, "[AP] WARNING: empty item name for id {}", item.item_id);
		return;
	}

	/* Vehicle unlock — ALWAYS re-apply, even on replay (idempotent).
	 * This ensures vehicles are restored after save/load reconnect. */
	if (AP_UnlockEngineByName(item.item_name)) {
		if (!is_replay) {
			AP_TRACE(fmt::format("VehicleUnlock: '{}' (NEW)", item.item_name));
			AP_ShowNews("[AP] Unlocked: " + item.item_name);
		} else {
			AP_TRACE(fmt::format("VehicleUnlock: '{}' (REPLAY re-apply)", item.item_name));
			Debug(misc, 1, "[AP] Re-applied vehicle unlock on reconnect: '{}'", item.item_name);
		}
		return;
	}

	/* Traps and consumables (money, Speed Boost) are NON-idempotent — skip on replay.
	 * Infrastructure unlocks (tracks, roads, signals, bridges, tunnels, airports,
	 * trees, terraform, town actions) are IDEMPOTENT and MUST re-apply on reconnect,
	 * because slot_data resets all lock states. */
	AP_TRACE(fmt::format("ProcessingItem: '{}' (not a vehicle — checking trap/utility)", item.item_name));
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;

	/* ── TRAPS (non-idempotent — skip on replay) ──────── */
	if (is_replay) {
		/* Jump past all traps to infrastructure unlocks below */
		goto infra_unlocks;
	}
	if (item.item_name == "Breakdown Wave") {
		/* 60-second breakdown timer (240 ticks × 250 ms).
		 * While active, the per-tick handler keeps reliability at 1.
		 * When it expires, vehicles are restored to normal reliability. */
		_ap_breakdown_wave_ticks = 240;
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == cid && v->IsPrimaryVehicle()) {
				v->breakdown_chance = 255;
				v->reliability = 1;
			}
		}
		AP_ShowNews("[AP] TRAP: Breakdown Wave! All vehicles unreliable for 60 seconds!");
	} else if (item.item_name == "Recession") {
		if (c->money >= 0) {
			/* Player has positive cash: halve it */
			AP_ChangeMoney(cid, -(c->money / 2));
			AP_ShowNews("[AP] TRAP: Recession! Money halved.");
		} else {
			/* Player already in debt: add 25% of max_loan as extra debt */
			Money penalty = (Money)((int64_t)_ap_pending_sd.max_loan / 4);
			AP_ChangeMoney(cid, -penalty);
			AP_ShowNews(fmt::format("[AP] TRAP: Recession! Extra debt: {}.", AP_Money(penalty)));
		}
	} else if (item.item_name == "Maintenance Surge") {
		/* Add a moderate fixed loan increment, capped at max_loan from slot_data
		 * so it stays proportional to the session's economy settings. */
		Money loan_cap = (Money)std::max((int64_t)_ap_pending_sd.max_loan,
		                                 (int64_t)300000LL);
		Money new_l = c->current_loan + (Money)(loan_cap / 4); /* +25% of max_loan */
		c->current_loan = std::min(new_l, loan_cap);
		AP_ShowNews("[AP] TRAP: Maintenance Surge! Emergency costs added to your loan.");
	} else if (item.item_name == "Bank Loan Forced") {
		/* Scale loan to the session's configured max_loan rather than a
		 * hardcoded 500 M that would be impossible to repay early-game. */
		Money forced_loan = (Money)_ap_pending_sd.max_loan;
		if (forced_loan <= 0) forced_loan = (Money)300000LL; /* sane fallback */
		c->current_loan = std::min(c->current_loan + forced_loan,
		                           forced_loan * 2); /* cap at 2× max_loan */
		AP_ShowNews(fmt::format("[AP] TRAP: Bank Loan Forced! +{}", AP_Money(forced_loan)));
	} else if (item.item_name == "Signal Failure") {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == cid && v->IsPrimaryVehicle() && v->type == VEH_TRAIN) {
				/* ctr=2 is the "about to break down" trigger state — this
				 * stops the train and plays the breakdown sound.
				 * ctr=1 is "already counting down" which has no visible effect. */
				if (v->breakdown_ctr == 0) {
					v->breakdown_ctr   = 2;
					v->breakdown_delay = 255;
				}
			}
		}
		AP_ShowNews("[AP] TRAP: Signal Failure! Trains are breaking down!");
	} else if (item.item_name == "Fuel Shortage") {
		/* Set a 60-second slowdown counter (240 ticks × 250 ms).
		 * The realtime timer applies the speed cap every 5 s while active. */
		_ap_fuel_shortage_ticks = 240;
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == cid && v->IsPrimaryVehicle())
				v->cur_speed = v->cur_speed / 2;
		}
		AP_ShowNews("[AP] TRAP: Fuel Shortage! Vehicles running at half speed for 60 seconds!");
	} else if (item.item_name == "Industry Closure") {
		/* Find industries actively serviced by the player (same logic as Death Link).
		 * Falls back to a random industry if none are connected yet. */
		std::vector<Industry *> active_industries;
		for (Station *st : Station::Iterate()) {
			if (st->owner != cid) continue;
			for (Industry *ind : Industry::Iterate()) {
				if (ind->location.tile == INVALID_TILE) continue;
				bool already = false;
				for (Industry *ai : active_industries) { if (ai == ind) { already = true; break; } }
				if (already) continue;
				for (const auto &produced : ind->produced) {
					if (!IsValidCargoType(produced.cargo) || produced.cargo >= NUM_CARGO) continue;
					if (st->goods[produced.cargo].HasRating()) {
						active_industries.push_back(ind);
						break;
					}
				}
			}
		}

		Industry *victim = nullptr;
		if (!active_industries.empty()) {
			victim = active_industries[InteractiveRandomRange((uint32_t)active_industries.size())];
		} else {
			/* Fallback: any random industry */
			int count = 0;
			for ([[maybe_unused]] Industry *ind : Industry::Iterate()) { count++; }
			if (count > 0) {
				int target_i = InteractiveRandomRange(count), i = 0;
				for (Industry *ind : Industry::Iterate()) {
					if (i++ == target_i) { victim = ind; break; }
				}
			}
		}

		if (victim != nullptr) {
			for (auto &produced : victim->produced) {
				produced.history[THIS_MONTH].production = 0;
			}
			victim->prod_level = 0;
			const Town *t = victim->town;
			std::string loc = (t != nullptr) ? std::string(" near ") + t->name : "";
			AP_ShowNews(fmt::format("[AP] TRAP: Industry Closure! An industry{} has shut down!", loc));
		} else {
			AP_ShowNews("[AP] TRAP: Industry Closure! (no industry found)");
		}

	} else if (item.item_name == "Vehicle License Revoke") {
		/* Pick a random vehicle category and block it for 1-2 in-game years.
		 * Timer is in realtime ticks (250 ms each).  At normal game speed
		 * 1 in-game year ≈ 13.5 min real time = 3240 realtime ticks. */
		static const VehicleType types[]     = { VEH_TRAIN, VEH_ROAD, VEH_AIRCRAFT, VEH_SHIP };
		static const char *const type_names[] = { "Trains", "Road Vehicles", "Aircraft", "Ships" };
		int idx = (int)InteractiveRandomRange(4);
		_ap_license_revoke_type  = (int)types[idx];
		_ap_license_revoke_ticks = 3240 + (int)InteractiveRandomRange(3240); /* 1–2 years realtime */
		int years_approx = (_ap_license_revoke_ticks / 3240) + 1;

		/* Immediately hide all engines of this category for the local company */
		for (Engine *e : Engine::Iterate()) {
			if ((int)e->type != _ap_license_revoke_type) continue;
			e->company_hidden.Set(cid);
		}
		MarkWholeScreenDirty();
		AP_TRACE(fmt::format("TRAP LicenseRevoke: type={} ({}) ticks={} ~{} years", _ap_license_revoke_type, type_names[idx], _ap_license_revoke_ticks, years_approx));
		AP_ShowNews(fmt::format("[AP] TRAP: Vehicle License Revoke! {} suspended for ~{} in-game year(s)!",
		    type_names[idx], years_approx));

	/* ── UTILITY ITEMS ─────────────────────────────────── */
	} else if (item.item_name == "Cash Injection £50,000") {
		AP_ChangeMoney(cid, (Money)50000LL);
		AP_ShowNews(fmt::format("[AP] Bonus: +{}!", AP_Money((Money)50000LL)));
	} else if (item.item_name == "Cash Injection £200,000") {
		AP_ChangeMoney(cid, (Money)200000LL);
		AP_ShowNews(fmt::format("[AP] Bonus: +{}!", AP_Money((Money)200000LL)));
	} else if (item.item_name == "Cash Injection £500,000") {
		AP_ChangeMoney(cid, (Money)500000LL);
		AP_ShowNews(fmt::format("[AP] Bonus: +{}!", AP_Money((Money)500000LL)));
	} else if (item.item_name == "Loan Reduction £100,000") {
		Money reduce = (Money)100000LL;
		c->current_loan = (c->current_loan > reduce) ? Money(c->current_loan - reduce) : Money(0);
		AP_ShowNews(fmt::format("[AP] Bonus: Loan reduced by {}!", AP_Money(reduce)));
	} else if (item.item_name == "Reliability Boost (all vehicles, 90 days)") {
		/* Start a 90-game-day reliability timer (90 days * ~80 ticks/day ≈ 7200 ticks).
		 * The per-tick handler re-applies max reliability every tick, countering the
		 * normal reliability_spd_dec decay that CheckVehicleBreakdown applies. */
		_ap_reliability_boost_ticks = 7200;
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == cid && v->IsPrimaryVehicle()) {
				const Engine *e = v->GetEngine();
				if (e != nullptr) v->reliability = e->reliability_max;
				v->breakdown_chance = 0;
			}
		}
		AP_ShowNews("[AP] Bonus: Reliability Boost! All vehicles at max reliability for 90 days.");
	} else if (item.item_name == "Cargo Bonus (2x payment, 60 days)") {
		/* Start a 60-second (240-tick) cargo payment multiplier.
		 * The hook in DeliverGoods (economy.cpp) doubles profit while this is active. */
		_ap_cargo_bonus_ticks = 240;
		AP_ShowNews("[AP] Bonus: Cargo Bonus! All cargo deliveries pay double for 60 seconds!");
	} else if (item.item_name == "Town Growth Boost") {
		/* Trigger an immediate growth pulse in every town by resetting
		 * grow_counter to 0.  The engine will schedule the next growth
		 * tick immediately.  We deliberately do NOT halve growth_rate —
		 * that mutation is permanent and, with 8+ copies in the pool,
		 * would eventually freeze all towns. */
		for (Town *t : Town::Iterate()) {
			t->grow_counter = 0;
		}
		AP_ShowNews("[AP] Bonus: Town Growth Boost! All towns growing faster.");
	} else if (item.item_name == "Free Station Upgrade") {
		/* Boost all player stations to MAX_STATION_RATING (255) for 30 game-days.
		 * The per-day timer re-applies the boost so normal decay doesn't reduce it. */
		_ap_station_boost_ticks = 2400; /* 30 days * ~80 ticks/day */
		for (Station *st : Station::Iterate()) {
			if (st->owner != cid) continue;
			for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
				if (st->goods[ct].HasRating()) {
					st->goods[ct].rating = MAX_STATION_RATING;
				}
			}
		}
		AP_ShowNews("[AP] Bonus: Free Station Upgrade! All your stations boosted to perfect rating for 30 days!");
	} else if (item.item_name == "Cash Bonus" || item.item_name == "Extra Funding") {
		/* Legacy names */
		AP_ChangeMoney(cid, (Money)100000LL);
		AP_ShowNews(fmt::format("[AP] Bonus: +{}!", AP_Money((Money)100000LL)));

	/* ── Speed Boost (non-idempotent: cumulative +10 per item) ────── */
	} else if (item.item_name == "Speed Boost") {
		if (_ap_ff_speed < 300) {
			_ap_ff_speed = std::min(_ap_ff_speed + 10, 300);
			_settings_client.gui.fast_forward_speed_limit = (uint16_t)_ap_ff_speed;
			AP_ShowNews(fmt::format("[AP] Speed Boost! Fast forward now {}% speed.", _ap_ff_speed));
		} else {
			AP_ShowNews("[AP] Speed Boost received (already at max 300%).");
		}
	}

	/* ── INFRASTRUCTURE UNLOCKS (idempotent — safe to re-apply on reconnect) ─ */
	infra_unlocks:
	/* ── TRACK DIRECTION UNLOCK ITEMS ───────────────────────────────────
	 * 24 items: 4 rail types × 6 track directions.
	 * RailType: 0=Normal, 1=Electric, 2=Monorail, 3=Maglev
	 * Track bits: 0=NE-SW, 1=NW-SE, 2=N, 3=S, 4=W, 5=E
	 */
	// Normal Rail (railtype=0)
	if (item.item_name == "Normal Rail Track: NE-SW") {
		_ap_locked_track_dirs[0] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track NE-SW!");
	} else if (item.item_name == "Normal Rail Track: NW-SE") {
		_ap_locked_track_dirs[0] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track NW-SE!");
	} else if (item.item_name == "Normal Rail Track: N") {
		_ap_locked_track_dirs[0] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track N!");
	} else if (item.item_name == "Normal Rail Track: S") {
		_ap_locked_track_dirs[0] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track S!");
	} else if (item.item_name == "Normal Rail Track: W") {
		_ap_locked_track_dirs[0] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track W!");
	} else if (item.item_name == "Normal Rail Track: E") {
		_ap_locked_track_dirs[0] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Normal Rail Track E!");
	// Electrified Rail (railtype=1)
	} else if (item.item_name == "Electrified Track: NE-SW") {
		_ap_locked_track_dirs[1] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Electrified Track NE-SW!");
	} else if (item.item_name == "Electrified Track: NW-SE") {
		_ap_locked_track_dirs[1] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Electrified Track NW-SE!");
	} else if (item.item_name == "Electrified Track: N") {
		_ap_locked_track_dirs[1] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Electrified Track N!");
	} else if (item.item_name == "Electrified Track: S") {
		_ap_locked_track_dirs[1] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Electrified Track S!");
	} else if (item.item_name == "Electrified Track: W") {
		_ap_locked_track_dirs[1] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Electrified Track W!");
	} else if (item.item_name == "Electrified Track: E") {
		_ap_locked_track_dirs[1] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Electrified Track E!");
	// Monorail (railtype=2)
	} else if (item.item_name == "Monorail Track: NE-SW") {
		_ap_locked_track_dirs[2] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Monorail Track NE-SW!");
	} else if (item.item_name == "Monorail Track: NW-SE") {
		_ap_locked_track_dirs[2] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Monorail Track NW-SE!");
	} else if (item.item_name == "Monorail Track: N") {
		_ap_locked_track_dirs[2] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Monorail Track N!");
	} else if (item.item_name == "Monorail Track: S") {
		_ap_locked_track_dirs[2] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Monorail Track S!");
	} else if (item.item_name == "Monorail Track: W") {
		_ap_locked_track_dirs[2] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Monorail Track W!");
	} else if (item.item_name == "Monorail Track: E") {
		_ap_locked_track_dirs[2] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Monorail Track E!");
	// Maglev (railtype=3)
	} else if (item.item_name == "Maglev Track: NE-SW") {
		_ap_locked_track_dirs[3] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Maglev Track NE-SW!");
	} else if (item.item_name == "Maglev Track: NW-SE") {
		_ap_locked_track_dirs[3] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Maglev Track NW-SE!");
	} else if (item.item_name == "Maglev Track: N") {
		_ap_locked_track_dirs[3] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Maglev Track N!");
	} else if (item.item_name == "Maglev Track: S") {
		_ap_locked_track_dirs[3] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Maglev Track S!");
	} else if (item.item_name == "Maglev Track: W") {
		_ap_locked_track_dirs[3] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Maglev Track W!");
	} else if (item.item_name == "Maglev Track: E") {
		_ap_locked_track_dirs[3] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Maglev Track E!");
	// Narrow Gauge (AP index=4, IH NewGRF)
	} else if (item.item_name == "Narrow Gauge Track: NE-SW") {
		_ap_locked_track_dirs[4] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track NE-SW!");
	} else if (item.item_name == "Narrow Gauge Track: NW-SE") {
		_ap_locked_track_dirs[4] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track NW-SE!");
	} else if (item.item_name == "Narrow Gauge Track: N") {
		_ap_locked_track_dirs[4] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track N!");
	} else if (item.item_name == "Narrow Gauge Track: S") {
		_ap_locked_track_dirs[4] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track S!");
	} else if (item.item_name == "Narrow Gauge Track: W") {
		_ap_locked_track_dirs[4] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track W!");
	} else if (item.item_name == "Narrow Gauge Track: E") {
		_ap_locked_track_dirs[4] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Narrow Gauge Track E!");
	// Metro (AP index=5, IH NewGRF)
	} else if (item.item_name == "Metro Track: NE-SW") {
		_ap_locked_track_dirs[5] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Metro Track NE-SW!");
	} else if (item.item_name == "Metro Track: NW-SE") {
		_ap_locked_track_dirs[5] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Metro Track NW-SE!");
	} else if (item.item_name == "Metro Track: N") {
		_ap_locked_track_dirs[5] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Metro Track N!");
	} else if (item.item_name == "Metro Track: S") {
		_ap_locked_track_dirs[5] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Metro Track S!");
	} else if (item.item_name == "Metro Track: W") {
		_ap_locked_track_dirs[5] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Metro Track W!");
	} else if (item.item_name == "Metro Track: E") {
		_ap_locked_track_dirs[5] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Metro Track E!");
	// Vacuum Tube (AP index=6, Vactrain NewGRF)
	} else if (item.item_name == "Vacuum Tube Track: NE-SW") {
		_ap_locked_track_dirs[6] &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track NE-SW!");
	} else if (item.item_name == "Vacuum Tube Track: NW-SE") {
		_ap_locked_track_dirs[6] &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track NW-SE!");
	} else if (item.item_name == "Vacuum Tube Track: N") {
		_ap_locked_track_dirs[6] &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track N!");
	} else if (item.item_name == "Vacuum Tube Track: S") {
		_ap_locked_track_dirs[6] &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track S!");
	} else if (item.item_name == "Vacuum Tube Track: W") {
		_ap_locked_track_dirs[6] &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track W!");
	} else if (item.item_name == "Vacuum Tube Track: E") {
		_ap_locked_track_dirs[6] &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Vacuum Tube Track E!");

	/* ── ROAD DIRECTION UNLOCK ITEMS ────────────────────────────────── */
	} else if (item.item_name == "Road: NE-SW") {
		_ap_locked_road_dirs &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Road NE-SW!");
	} else if (item.item_name == "Road: NW-SE") {
		_ap_locked_road_dirs &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Road NW-SE!");

	/* ── TRAM DIRECTION UNLOCK ITEMS ───────────────────────────────── */
	} else if (item.item_name == "Tram: NE-SW") {
		_ap_locked_tram_dirs &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Tram NE-SW!");
	} else if (item.item_name == "Tram: NW-SE") {
		_ap_locked_tram_dirs &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Tram NW-SE!");

	/* ── SIGNAL UNLOCK ITEMS ────────────────────────────────────────── */
	} else if (item.item_name == "Signal: Block") {
		_ap_locked_signals &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Block Signal!");
	} else if (item.item_name == "Signal: Entry") {
		_ap_locked_signals &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Entry Signal!");
	} else if (item.item_name == "Signal: Exit") {
		_ap_locked_signals &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Exit Signal!");
	} else if (item.item_name == "Signal: Combo") {
		_ap_locked_signals &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Combo Signal!");
	} else if (item.item_name == "Signal: Path") {
		_ap_locked_signals &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Path Signal!");
	} else if (item.item_name == "Signal: One-Way Path") {
		_ap_locked_signals &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: One-Way Path Signal!");

	/* ── BRIDGE UNLOCK ITEMS ────────────────────────────────────────── */
	} else if (item.item_name == "Bridge: Wooden") {
		_ap_locked_bridges &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Wooden Bridge!");
	} else if (item.item_name == "Bridge: Concrete") {
		_ap_locked_bridges &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Concrete Bridge!");
	} else if (item.item_name == "Bridge: Girder Steel") {
		_ap_locked_bridges &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Girder Steel Bridge!");
	} else if (item.item_name == "Bridge: Suspension Concrete") {
		_ap_locked_bridges &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Suspension Concrete Bridge!");
	} else if (item.item_name == "Bridge: Suspension Steel (Short)") {
		_ap_locked_bridges &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Suspension Steel Bridge (Short)!");
	} else if (item.item_name == "Bridge: Suspension Steel (Long)") {
		_ap_locked_bridges &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Suspension Steel Bridge (Long)!");
	} else if (item.item_name == "Bridge: Cantilever Steel (Short)") {
		_ap_locked_bridges &= ~(1u << 6);
		AP_ShowNews("[AP] Unlocked: Cantilever Steel Bridge (Short)!");
	} else if (item.item_name == "Bridge: Cantilever Steel (Medium)") {
		_ap_locked_bridges &= ~(1u << 7);
		AP_ShowNews("[AP] Unlocked: Cantilever Steel Bridge (Medium)!");
	} else if (item.item_name == "Bridge: Cantilever Steel (Long)") {
		_ap_locked_bridges &= ~(1u << 8);
		AP_ShowNews("[AP] Unlocked: Cantilever Steel Bridge (Long)!");
	} else if (item.item_name == "Bridge: Girder Steel (High)") {
		_ap_locked_bridges &= ~(1u << 9);
		AP_ShowNews("[AP] Unlocked: Girder Steel Bridge (High)!");
	} else if (item.item_name == "Bridge: Tubular Steel (Short)") {
		_ap_locked_bridges &= ~(1u << 10);
		AP_ShowNews("[AP] Unlocked: Tubular Steel Bridge (Short)!");
	} else if (item.item_name == "Bridge: Tubular Steel (Long)") {
		_ap_locked_bridges &= ~(1u << 11);
		AP_ShowNews("[AP] Unlocked: Tubular Steel Bridge (Long)!");
	} else if (item.item_name == "Bridge: Tubular Silicon") {
		_ap_locked_bridges &= ~(1u << 12);
		AP_ShowNews("[AP] Unlocked: Tubular Silicon Bridge!");

	/* ── TUNNEL UNLOCK ITEM ─────────────────────────────────────────── */
	} else if (item.item_name == "Tunnel Construction") {
		_ap_locked_tunnels = false;
		AP_ShowNews("[AP] Unlocked: Tunnel Construction!");

	/* ── AIRPORT UNLOCK ITEMS ───────────────────────────────────────── */
	// AT_SMALL (bit 0) is always free — not an item
	} else if (item.item_name == "Airport: Large") {
		_ap_locked_airports &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Large Airport!");
	} else if (item.item_name == "Airport: Heliport") {
		_ap_locked_airports &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Heliport!");
	} else if (item.item_name == "Airport: Metropolitan") {
		_ap_locked_airports &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Metropolitan Airport!");
	} else if (item.item_name == "Airport: International") {
		_ap_locked_airports &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: International Airport!");
	} else if (item.item_name == "Airport: Commuter") {
		_ap_locked_airports &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Commuter Airport!");
	} else if (item.item_name == "Airport: Helidepot") {
		_ap_locked_airports &= ~(1u << 6);
		AP_ShowNews("[AP] Unlocked: Helidepot!");
	} else if (item.item_name == "Airport: Intercontinental") {
		_ap_locked_airports &= ~(1u << 7);
		AP_ShowNews("[AP] Unlocked: Intercontinental Airport!");
	} else if (item.item_name == "Airport: Helistation") {
		_ap_locked_airports &= ~(1u << 8);
		AP_ShowNews("[AP] Unlocked: Helistation!");

	/* ── TREE UNLOCK ITEMS ──────────────────────────────────────────── */
	} else if (item.item_name == "Trees: Temperate Pack 1") {
		_ap_locked_trees &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Temperate Trees Pack 1!");
	} else if (item.item_name == "Trees: Temperate Pack 2") {
		_ap_locked_trees &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Temperate Trees Pack 2!");
	} else if (item.item_name == "Trees: Temperate Pack 3") {
		_ap_locked_trees &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Temperate Trees Pack 3!");
	} else if (item.item_name == "Trees: Arctic Pack 1") {
		_ap_locked_trees &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Arctic Trees Pack 1!");
	} else if (item.item_name == "Trees: Arctic Pack 2") {
		_ap_locked_trees &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Arctic Trees Pack 2!");
	} else if (item.item_name == "Trees: Arctic Pack 3") {
		_ap_locked_trees &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Arctic Trees Pack 3!");
	} else if (item.item_name == "Trees: Tropical Pack 1") {
		_ap_locked_trees &= ~(1u << 6);
		AP_ShowNews("[AP] Unlocked: Tropical Trees Pack 1!");
	} else if (item.item_name == "Trees: Tropical Pack 2") {
		_ap_locked_trees &= ~(1u << 7);
		AP_ShowNews("[AP] Unlocked: Tropical Trees Pack 2!");
	} else if (item.item_name == "Trees: Tropical Pack 3") {
		_ap_locked_trees &= ~(1u << 8);
		AP_ShowNews("[AP] Unlocked: Tropical Trees Pack 3!");
	} else if (item.item_name == "Trees: Toyland Pack") {
		_ap_locked_trees &= ~(1u << 9);
		AP_ShowNews("[AP] Unlocked: Toyland Trees Pack!");

	/* ── TERRAFORM UNLOCK ITEMS ─────────────────────────────────────── */
	} else if (item.item_name == "Terraform: Raise Land") {
		_ap_locked_terraform &= ~(1u << 0);
		InvalidateWindowClassesData(WC_SCEN_LAND_GEN);
		AP_ShowNews("[AP] Unlocked: Raise Land!");
	} else if (item.item_name == "Terraform: Lower Land") {
		_ap_locked_terraform &= ~(1u << 1);
		InvalidateWindowClassesData(WC_SCEN_LAND_GEN);
		AP_ShowNews("[AP] Unlocked: Lower Land!");

	/* ── TOWN AUTHORITY ACTION UNLOCK ITEMS ─────────────────────────── */
	} else if (item.item_name == "Town Action: Advertise Small") {
		_ap_locked_town_actions &= ~(1u << 0);
		AP_ShowNews("[AP] Unlocked: Town Action - Advertise Small!");
	} else if (item.item_name == "Town Action: Advertise Medium") {
		_ap_locked_town_actions &= ~(1u << 1);
		AP_ShowNews("[AP] Unlocked: Town Action - Advertise Medium!");
	} else if (item.item_name == "Town Action: Advertise Large") {
		_ap_locked_town_actions &= ~(1u << 2);
		AP_ShowNews("[AP] Unlocked: Town Action - Advertise Large!");
	} else if (item.item_name == "Town Action: Fund Road Reconstruction") {
		_ap_locked_town_actions &= ~(1u << 3);
		AP_ShowNews("[AP] Unlocked: Town Action - Fund Road Reconstruction!");
	} else if (item.item_name == "Town Action: Build Statue") {
		_ap_locked_town_actions &= ~(1u << 4);
		AP_ShowNews("[AP] Unlocked: Town Action - Build Statue!");
	} else if (item.item_name == "Town Action: Fund Buildings") {
		_ap_locked_town_actions &= ~(1u << 5);
		AP_ShowNews("[AP] Unlocked: Town Action - Fund Buildings!");
	} else if (item.item_name == "Town Action: Buy Exclusive Transport Rights") {
		_ap_locked_town_actions &= ~(1u << 6);
		AP_ShowNews("[AP] Unlocked: Town Action - Buy Exclusive Transport Rights!");
	} else if (item.item_name == "Town Action: Bribe Authority") {
		_ap_locked_town_actions &= ~(1u << 7);
		AP_ShowNews("[AP] Unlocked: Town Action - Bribe Authority!");

	} else if (!is_replay) {
		AP_WARN("Unknown item: '" + item.item_name + "' — not handled");
	}

	/* After any infrastructure unlock, refresh all construction toolbars
	 * so greyed-out buttons update immediately instead of staying grey
	 * until the player closes and reopens the toolbar. */
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
	InvalidateWindowClassesData(WC_BUILD_TREES);
}


static void AP_OnConnected()
{
	Debug(misc, 1, "[AP] Connected to Archipelago server.");
	AP_OK("Connected to Archipelago server");
	/* NOTE: ForceEnglishLanguage() was moved to AP_ConsumeWorldStart()
	 * to avoid a race condition in multiplayer — ReadLanguagePack()
	 * replaces _langpack while GUI threads call GetString(). */
	_ap_status_dirty.store(true);
}

static void AP_OnDisconnected(const std::string &reason)
{
	Debug(misc, 1, "[AP] Disconnected: {}", reason);
	AP_WARN("Disconnected: " + reason);
	_ap_world_started_this_session = false;
	_ap_pending_load_save          = false;
	_ap_waiting_for_start_choice   = false;
	_ap_pending_world_start = false;
	_ap_session_started = false;
	_ap_server_checked_locations.clear();
	_ap_server_checked_pending = false;
	AP_LOG("Session flags reset — next connect can start world");
	_ap_status_dirty.store(true);
}

static void AP_OnPrint(const std::string &msg)
{
	Debug(misc, 0, "[AP] Server: {}", msg);
	AP_ShowNews("[AP] " + msg, false); /* server broadcast = other players */
}

/* -------------------------------------------------------------------------
 * Death Link — outbound: called by crash hooks in train_cmd.cpp / roadveh_cmd.cpp
 * ---------------------------------------------------------------------- */

/** Cooldown between processing incoming deaths (in realtime ticks, 1 tick = 250 ms). */
static int _ap_death_cooldown_ticks = 0;

/** Send a death to all other Death-Link participants.
 *  Only fires if Death Link is enabled in slot_data. */
void AP_SendDeath(const std::string &cause)
{
	if (_ap_client == nullptr) return;
	if (!AP_GetSlotData().death_link) return;
	if (!_ap_session_started) return;
	_ap_client->SendDeath(cause);
	AP_TRACE(fmt::format("DeathLink SENT: cause='{}'", cause));
	Debug(misc, 0, "[AP] Death sent: {}", cause);
}

/* -------------------------------------------------------------------------
 * Death Link — inbound: find an industry connected to the player's stations
 * and shut it down as the "death" penalty.
 * ---------------------------------------------------------------------- */

static void AP_OnDeathReceived(const std::string &source)
{
	if (!_ap_session_started) return;
	if (!AP_GetSlotData().death_link) return; /* Death Link disabled in slot_data */
	if (_ap_death_cooldown_ticks > 0) {
		Debug(misc, 0, "[AP] Death from {} ignored — cooldown active", source);
		return;
	}
	/* Start 30-second cooldown (120 ticks × 250 ms) */
	_ap_death_cooldown_ticks = 120;

	/* Death penalty: lose 50% of current money */
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c != nullptr) {
		Money penalty = c->money / 2;
		AP_ChangeMoney(cid, -penalty);

		std::string msg = fmt::format("{} did not want to stay alive, "
		    "and they are punishing you. Lost {}!", source, AP_Money(penalty));
		AddNewsItem(
			GetEncodedString(STR_ARCHIPELAGO_NEWS, msg),
			NewsType::General,
			NewsStyle::Normal,
			{}
		);
		IConsolePrint(CC_ERROR, fmt::format("[AP] {}", msg));
		AP_TRACE(fmt::format("DeathLink RECEIVED: source='{}' penalty={}", source, (int64_t)penalty));
		Debug(misc, 0, "[AP] Death received from {} — 50% money penalty ({})", source, (int64_t)penalty);
	}
}

/* -------------------------------------------------------------------------
 * Handler registration
 * ---------------------------------------------------------------------- */

static bool _handlers_registered = false;
static void AP_OnCheckedLocations(const std::set<std::string> &locations); /* forward decl */
static void AP_ApplyServerCheckedLocations(); /* forward decl */

void EnsureHandlersRegistered()
{
	if (_handlers_registered || _ap_client == nullptr) return;
	_handlers_registered = true;

	_ap_client->callbacks.on_connected     = AP_OnConnected;
	_ap_client->callbacks.on_disconnected  = AP_OnDisconnected;
	_ap_client->callbacks.on_print         = AP_OnPrint;
	_ap_client->callbacks.on_slot_data     = AP_OnSlotData;
	_ap_client->callbacks.on_item_received = AP_OnItemReceived;
	_ap_client->callbacks.on_death_received = AP_OnDeathReceived;
	_ap_client->callbacks.on_checked_locations = AP_OnCheckedLocations;
}

/* -------------------------------------------------------------------------
 * Win-condition check (multi-target — ALL 6 must be met simultaneously)
 * ---------------------------------------------------------------------- */

APWinProgress AP_GetWinProgress()
{
	APWinProgress p;
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return p;
	const Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return p;

	p.company_value   = (int64_t)c->old_economy[0].company_value;
	p.town_population = (int64_t)GetWorldPopulation();

	int vcount = 0;
	for (const Vehicle *v : Vehicle::Iterate())
		if (v->owner == cid && v->IsPrimaryVehicle()) vcount++;
	p.vehicle_count = vcount;

	uint64_t total_cargo = 0;
	for (uint i = 0; i < NUM_CARGO; i++)
		total_cargo += AP_GetTotalCargo((CargoType)i);
	p.cargo_delivered = (int64_t)total_cargo;

	/* Use last COMPLETED period (old_economy[0]) — consistent with mission checks.
	 * cur_economy is the in-progress period and fluctuates mid-month. */
	int64_t net = (int64_t)(c->old_economy[0].income + c->old_economy[0].expenses);
	p.monthly_profit  = net > 0 ? net : 0;

	p.missions = (int64_t)AP_GetTotalMissionsCompleted();
	return p;
}

static bool CheckWinCondition(const APSlotData &sd)
{
	APWinProgress p = AP_GetWinProgress();
	return p.company_value   >= sd.win_target_company_value
	    && p.town_population >= sd.win_target_town_population
	    && p.vehicle_count   >= sd.win_target_vehicle_count
	    && p.cargo_delivered >= sd.win_target_cargo_delivered
	    && p.monthly_profit  >= sd.win_target_monthly_profit
	    && p.missions        >= sd.win_target_missions;
}

/* -------------------------------------------------------------------------
 * Public API — called by intro_gui.cpp to safely start the world
 * ---------------------------------------------------------------------- */

static uint32_t _ap_world_seed_to_use = 0;

bool AP_ShouldStartWorld()
{
	return _ap_pending_world_start && _game_mode == GM_MENU;
}

void AP_ConsumeWorldStart()
{
	if (!_ap_pending_world_start) return;
	_ap_pending_world_start = false;

	const APSlotData &sd = _ap_pending_sd;

	/* Debug: log AP slot data flags to file (stdout is DEVNULL in bridge mode) */
	{
		FILE *dbg = fopen("ap_debug.log", "a");
		if (dbg) {
			fmt::print(dbg, "[AP_ConsumeWorldStart] iron_horse={} military={} shark={} hover={} heqs={} vactrain={} aircraft={} firs={}\n",
				sd.enable_iron_horse, sd.enable_military_items, sd.enable_shark_ships,
				sd.enable_hover_vehicles, sd.enable_heqs, sd.enable_vactrain,
				sd.enable_aircraftpack, sd.enable_firs);
			fmt::print(dbg, "[AP_ConsumeWorldStart] missions={} shop_items={} locked_vehicles={} seed={}\n",
				sd.missions.size(), sd.shop_item_names.size(), sd.locked_vehicles.size(), sd.world_seed);
			fclose(dbg);
		}
	}

	/* ── World generation ─────────────────────────────────────────── */
	_settings_newgame.game_creation.starting_year =
	    TimerGameCalendar::Year(sd.start_year);
	if (sd.map_x >= 6 && sd.map_x <= 12)
		_settings_newgame.game_creation.map_x = sd.map_x;
	if (sd.map_y >= 6 && sd.map_y <= 12)
		_settings_newgame.game_creation.map_y = sd.map_y;
	if (sd.landscape <= 3)
		_settings_newgame.game_creation.landscape = (LandscapeType)sd.landscape;
	if (sd.land_generator <= 1)
		_settings_newgame.game_creation.land_generator = sd.land_generator;
	if (sd.terrain_type <= 4)
		_settings_newgame.difficulty.terrain_type = sd.terrain_type;
	if (sd.sea_level <= 3)
		_settings_newgame.difficulty.quantity_sea_lakes = sd.sea_level;
	if (sd.rivers <= 3)
		_settings_newgame.game_creation.amount_of_rivers = sd.rivers;
	if (sd.smoothness <= 3)
		_settings_newgame.game_creation.tgen_smoothness = sd.smoothness;
	if (sd.variety <= 5)
		_settings_newgame.game_creation.variety = sd.variety;
	if (sd.number_towns <= 4)
		_settings_newgame.difficulty.number_towns = sd.number_towns;
	_settings_newgame.game_creation.town_name = sd.town_name;

	_ap_world_seed_to_use = (sd.world_seed != 0) ? sd.world_seed : GENERATE_NEW_SEED;

	/* ── Accounting ───────────────────────────────────────────────── */
	_settings_newgame.difficulty.infinite_money        = sd.infinite_money;
	_settings_newgame.economy.inflation                = sd.inflation;
	_settings_newgame.difficulty.max_loan              = sd.max_loan;
	_settings_newgame.economy.infrastructure_maintenance = sd.infrastructure_maintenance;
	_settings_newgame.difficulty.vehicle_costs         = sd.vehicle_costs;
	_settings_newgame.difficulty.construction_cost     = sd.construction_cost;

	/* ── Vehicle Limitations ─────────────────────────────────────── */
	_settings_newgame.vehicle.max_trains               = sd.max_trains;
	_settings_newgame.vehicle.max_roadveh              = sd.max_roadveh;
	_settings_newgame.vehicle.max_aircraft             = sd.max_aircraft;
	_settings_newgame.vehicle.max_ships                = sd.max_ships;
	/* max_train_length and station_spread: settings_type.h uses vanilla uint8_t.
	 * AP slot_data may send up to uint16_t — clamp to 255. */
	_settings_newgame.vehicle.max_train_length = (uint8_t)std::min((uint16_t)255, sd.max_train_length);
	_settings_newgame.station.station_spread   = (uint8_t)std::min((uint16_t)255, sd.station_spread);
	_settings_newgame.construction.road_stop_on_town_road       = sd.road_stop_on_town_road;
	_settings_newgame.construction.road_stop_on_competitor_road = sd.road_stop_on_competitor_road;
	_settings_newgame.construction.crossing_with_competitor     = sd.crossing_with_competitor;

	/* ── Disasters / Accidents ───────────────────────────────────── */
	_settings_newgame.difficulty.disasters             = sd.disasters;
	_settings_newgame.vehicle.plane_crashes            = sd.plane_crashes;
	_settings_newgame.difficulty.vehicle_breakdowns    = sd.vehicle_breakdowns;

	/* ── AP: always disable vehicle/airport expiry so engine names resolve
	 * correctly for AP item matching. Set in _settings_newgame so it's
	 * part of the initial game state (prevents desync in multiplayer). */
	_settings_newgame.vehicle.never_expire_vehicles   = true;
	_settings_newgame.station.never_expire_airports   = true;

	/* ── Economy / Environment ───────────────────────────────────── */
	_settings_newgame.economy.type                     = (EconomyType)sd.economy_type;
	_settings_newgame.economy.bribe                    = sd.bribe;
	_settings_newgame.economy.exclusive_rights         = sd.exclusive_rights;
	_settings_newgame.economy.fund_buildings           = sd.fund_buildings;
	_settings_newgame.economy.fund_roads               = sd.fund_roads;
	_settings_newgame.economy.give_money               = sd.give_money;
	_settings_newgame.economy.town_growth_rate         = sd.town_growth_rate;
	_settings_newgame.economy.found_town               = (TownFounding)sd.found_town;
	_settings_newgame.economy.town_cargo_scale         = sd.town_cargo_scale;
	_settings_newgame.economy.industry_cargo_scale     = sd.industry_cargo_scale;
	_settings_newgame.difficulty.industry_density      = sd.industry_density;
	_settings_newgame.economy.allow_town_roads         = sd.allow_town_roads;

	/* ── Force English for engine-name matching ─────────────────── */
	/* Must run here (GM_MENU) — safe from concurrent GetString()
	 * calls that would race with ReadLanguagePack() in multiplayer. */
	ForceEnglishLanguage();

	/* ── Vehicles / Routing ──────────────────────────────────────── */
	_settings_newgame.vehicle.road_side                = sd.road_side;

	/* ── NewGRF list — start clean ──────────────────────────────────── */
	/* The user's openttd.cfg may already contain some of our GRFs (from a
	 * previous AP game or manual setup).  AppendToGRFConfigList removes
	 * duplicates by keeping the *first* occurrence, which would be the old
	 * entry WITHOUT the params we need (e.g. military-items.grf param[0]=1).
	 * Clearing the list ensures our versions (with correct params) win. */
	_grfconfig_newgame.clear();

	/* ── NewGRF: Archipelago Ruins ───────────────────────────────────── */
	{
		/* The ruins GRF is in baseset/ (copied by CMake). It defines 6
		 * object types used to visualise ruin items on the map. We always
		 * load it — the objects are inert unless AP sends ruin items. */
		static const std::string RUINS_FILENAME = "archipelago_ruins.grf";
		auto rg = std::make_unique<GRFConfig>(RUINS_FILENAME);
		rg->SetSuitablePalette();

		/* Try newgrf dir first (LoadNewGRF searches NEWGRF_DIR for non-baseset GRFs),
		 * then baseset dir as fallback */
		if (!FillGRFDetails(*rg, false, NEWGRF_DIR) &&
		    !FillGRFDetails(*rg, false, BASESET_DIR)) {
			AP_WARN("archipelago_ruins.grf not found — ruin objects will not appear.");
		} else if (rg->status != GCS_NOT_FOUND && rg->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(rg));
			AP_OK("Archipelago Ruins GRF activated for new game.");
		}
	}

	/* ── NewGRF: Archipelago Stars ──────────────────────────────────── */
	{
		/* The stars GRF is in baseset/ (copied by CMake). It defines 3
		 * object types used to visualise star collectibles on the map. */
		static const std::string STARS_FILENAME = "archipelago_stars.grf";
		auto sg = std::make_unique<GRFConfig>(STARS_FILENAME);
		sg->SetSuitablePalette();

		if (!FillGRFDetails(*sg, false, NEWGRF_DIR) &&
		    !FillGRFDetails(*sg, false, BASESET_DIR)) {
			AP_WARN("archipelago_stars.grf not found — star objects will not appear.");
		} else if (sg->status != GCS_NOT_FOUND && sg->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(sg));
			AP_OK("Archipelago Stars GRF activated for new game.");
		}
	}

	/* ── NewGRF: Iron Horse ──────────────────────────────────────────── */
	if (sd.enable_iron_horse) {
		/* iron_horse.grf is bundled in the {exe}/newgrf/ subfolder of the
		 * Archipelago patch release zip.  OpenTTD searches SP_BINARY_DIR
		 * last in its NewGRF scan, so if the user has not yet done a
		 * ScanNewGRFFiles() that picked it up, we copy it into the personal
		 * newgrf directory first so FillGRFDetails can find it. */

		static const std::string IH_FILENAME = "iron_horse.grf";

		/* 1) Does the file already exist anywhere OpenTTD would find it? */
		auto ih = std::make_unique<GRFConfig>(IH_FILENAME);
		ih->SetSuitablePalette();

		if (!FillGRFDetails(*ih, false, NEWGRF_DIR)) {
			/* Not found — try to copy from our bundle (next to the exe) */
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + IH_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + IH_FILENAME;

			bool copied = false;
			{
				/* std::filesystem is banned by safeguards — use fopen/fwrite */
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
							fwrite(buf, 1, n, fdst);
						}
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}

			if (copied) {
				AP_OK(fmt::format("Iron Horse GRF installed from bundle to: {}", dst));
				/* Re-try FillGRFDetails now that the file is in place */
				ih = std::make_unique<GRFConfig>(IH_FILENAME);
				ih->SetSuitablePalette();
				if (!FillGRFDetails(*ih, false, NEWGRF_DIR)) {
					/* Rescan so OpenTTD finds the newly copied file */
					ScanNewGRFFiles(nullptr);
					ih = std::make_unique<GRFConfig>(IH_FILENAME);
					ih->SetSuitablePalette();
					FillGRFDetails(*ih, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format(
					"iron_horse.grf not found in bundle ({}) — "
					"place it in your newgrf/ folder or re-download the patch zip.",
					src));
			}
		}

		/* 2) If the GRF is now resolved, add it to the new-game config */
		if (ih->status != GCS_NOT_FOUND && ih->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(ih));
			AP_OK("Iron Horse GRF activated for new game.");
		}
	}

	/* ── NewGRF: Military Items ─────────────────────────────────────── */
	if (sd.enable_military_items) {
		static const std::string MIL_FILENAME = "military-items.grf";
		auto mg = std::make_unique<GRFConfig>(MIL_FILENAME);
		mg->SetSuitablePalette();
		if (!FillGRFDetails(*mg, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + MIL_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + MIL_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("Military Items GRF installed from bundle to: {}", dst));
				mg = std::make_unique<GRFConfig>(MIL_FILENAME);
				mg->SetSuitablePalette();
				if (!FillGRFDetails(*mg, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					mg = std::make_unique<GRFConfig>(MIL_FILENAME);
					mg->SetSuitablePalette();
					FillGRFDetails(*mg, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("military-items.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (mg->status != GCS_NOT_FOUND && mg->status != GCS_DISABLED) {
			/* Parameters for military-items.grf (12 total):
			 *   [0]  Enable custom airports:  1 = All
			 *   [1]  Disable airport noise:   1 = yes
			 *   [2]  Purchase cost mult:      4 = 1x
			 *   [3]  Running cost mult:       4 = 1x
			 *   [4]  Aircraft ranges:         0 = disabled
			 *   [5]  Date restrictions:       0 = off
			 *   [6]  Czechoslovak aircraft:   1 = All
			 *   [7]  European aircraft:       1 = All
			 *   [8]  Soviet/Russian aircraft: 1 = All
			 *   [9]  US aircraft:             1 = All
			 *   [10] Other nations aircraft:  1 = All
			 *   [11] Disable default aircraft:0 = no
			 * Params 6-10 default to 0 (None) if unset, which hides ALL
			 * military aircraft and prevents Action 4 name assignment. */
			const uint32_t mil_params[] = {1, 1, 4, 4, 0, 0, 1, 1, 1, 1, 1, 0};
			mg->SetParams(mil_params);
			AppendToGRFConfigList(_grfconfig_newgame, std::move(mg));
			AP_OK("Military Items GRF activated for new game (airports=All, noise=off).");
		}
	}

	/* ── NewGRF: SHARK Ship Set ─────────────────────────────────────── */
	if (sd.enable_shark_ships) {
		static const std::string SHARK_FILENAME = "shark.grf";
		auto sg = std::make_unique<GRFConfig>(SHARK_FILENAME);
		sg->SetSuitablePalette();

		if (!FillGRFDetails(*sg, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + SHARK_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + SHARK_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("SHARK GRF installed from bundle to: {}", dst));
				sg = std::make_unique<GRFConfig>(SHARK_FILENAME);
				sg->SetSuitablePalette();
				if (!FillGRFDetails(*sg, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					sg = std::make_unique<GRFConfig>(SHARK_FILENAME);
					sg->SetSuitablePalette();
					FillGRFDetails(*sg, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("shark.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (sg->status != GCS_NOT_FOUND && sg->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(sg));
			AP_OK("SHARK Ship Set GRF activated for new game.");
		}
	}

	/* ── NewGRF: Hover Vehicles ─────────────────────────────────────── */
	if (sd.enable_hover_vehicles) {
		static const std::string HV_FILENAME = "hoverv.grf";
		auto hg = std::make_unique<GRFConfig>(HV_FILENAME);
		hg->SetSuitablePalette();

		if (!FillGRFDetails(*hg, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + HV_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + HV_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("Hover Vehicles GRF installed from bundle to: {}", dst));
				hg = std::make_unique<GRFConfig>(HV_FILENAME);
				hg->SetSuitablePalette();
				if (!FillGRFDetails(*hg, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					hg = std::make_unique<GRFConfig>(HV_FILENAME);
					hg->SetSuitablePalette();
					FillGRFDetails(*hg, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("hoverv.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (hg->status != GCS_NOT_FOUND && hg->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(hg));
			AP_OK("Hover Vehicles GRF activated for new game.");
		}
	}

	/* ── NewGRF: HEQS Heavy Equipment Set ──────────────────────────── */
	if (sd.enable_heqs) {
		static const std::string HEQS_FILENAME = "heqs.grf";
		auto hq = std::make_unique<GRFConfig>(HEQS_FILENAME);
		hq->SetSuitablePalette();

		if (!FillGRFDetails(*hq, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + HEQS_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + HEQS_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("HEQS GRF installed from bundle to: {}", dst));
				hq = std::make_unique<GRFConfig>(HEQS_FILENAME);
				hq->SetSuitablePalette();
				if (!FillGRFDetails(*hq, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					hq = std::make_unique<GRFConfig>(HEQS_FILENAME);
					hq->SetSuitablePalette();
					FillGRFDetails(*hq, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("heqs.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (hq->status != GCS_NOT_FOUND && hq->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(hq));
			AP_OK("HEQS Heavy Equipment GRF activated for new game.");
		}
	}

	/* ── NewGRF: Vactrain Set ──────────────────────────────────────── */
	if (sd.enable_vactrain) {
		static const std::string VAC_FILENAME = "vactrain_1.0.1.grf";
		auto vg = std::make_unique<GRFConfig>(VAC_FILENAME);
		vg->SetSuitablePalette();

		if (!FillGRFDetails(*vg, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + VAC_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + VAC_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("Vactrain GRF installed from bundle to: {}", dst));
				vg = std::make_unique<GRFConfig>(VAC_FILENAME);
				vg->SetSuitablePalette();
				if (!FillGRFDetails(*vg, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					vg = std::make_unique<GRFConfig>(VAC_FILENAME);
					vg->SetSuitablePalette();
					FillGRFDetails(*vg, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("vactrain_1.0.1.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (vg->status != GCS_NOT_FOUND && vg->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(vg));
			AP_OK("Vactrain Set GRF activated for new game.");
		}
	}

	/* ── NewGRF: Aircraftpack 2025 ─────────────────────────────────── */
	if (sd.enable_aircraftpack) {
		static const std::string AP25_FILENAME = "Aircraft2025.grf";
		auto ag = std::make_unique<GRFConfig>(AP25_FILENAME);
		ag->SetSuitablePalette();

		if (!FillGRFDetails(*ag, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + AP25_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + AP25_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("Aircraftpack 2025 GRF installed from bundle to: {}", dst));
				ag = std::make_unique<GRFConfig>(AP25_FILENAME);
				ag->SetSuitablePalette();
				if (!FillGRFDetails(*ag, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					ag = std::make_unique<GRFConfig>(AP25_FILENAME);
					ag->SetSuitablePalette();
					FillGRFDetails(*ag, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("Aircraft2025.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (ag->status != GCS_NOT_FOUND && ag->status != GCS_DISABLED) {
			AppendToGRFConfigList(_grfconfig_newgame, std::move(ag));
			AP_OK("Aircraftpack 2025 GRF activated for new game.");
		}
	}

	/* ── NewGRF: FIRS Industries ───────────────────────────────────── */
	if (sd.enable_firs) {
		static const std::string FIRS_FILENAME = "firs.grf";
		auto fg = std::make_unique<GRFConfig>(FIRS_FILENAME);
		fg->SetSuitablePalette();

		if (!FillGRFDetails(*fg, false, NEWGRF_DIR)) {
			std::string src = FioGetDirectory(SP_BINARY_DIR, NEWGRF_DIR) + FIRS_FILENAME;
			std::string dst = FioGetDirectory(SP_PERSONAL_DIR, NEWGRF_DIR) + FIRS_FILENAME;
			bool copied = false;
			{
				FILE *fsrc = fopen(src.c_str(), "rb");
				if (fsrc != nullptr) {
					FILE *fdst = fopen(dst.c_str(), "wb");
					if (fdst != nullptr) {
						char buf[65536];
						size_t n;
						while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) fwrite(buf, 1, n, fdst);
						fclose(fdst);
						copied = true;
					}
					fclose(fsrc);
				}
			}
			if (copied) {
				AP_OK(fmt::format("FIRS Industries GRF installed from bundle to: {}", dst));
				fg = std::make_unique<GRFConfig>(FIRS_FILENAME);
				fg->SetSuitablePalette();
				if (!FillGRFDetails(*fg, false, NEWGRF_DIR)) {
					ScanNewGRFFiles(nullptr);
					fg = std::make_unique<GRFConfig>(FIRS_FILENAME);
					fg->SetSuitablePalette();
					FillGRFDetails(*fg, false, NEWGRF_DIR);
				}
			} else {
				AP_WARN(fmt::format("firs.grf not found in bundle ({}) — place it in your newgrf/ folder.", src));
			}
		}
		if (fg->status != GCS_NOT_FOUND && fg->status != GCS_DISABLED) {
			/* FIRS parameter 0 = Economy type (0=Temperate Basic, 1=Arctic Basic,
			 * 2=Tropic Basic, 3=Steeltown, 4=In A Hot Country) */
			std::vector<uint32_t> firs_params = {sd.firs_economy};
			fg->SetParams(firs_params);
			AppendToGRFConfigList(_grfconfig_newgame, std::move(fg));
			AP_OK(fmt::format("FIRS Industries GRF activated (economy={}).", sd.firs_economy));
		}
	}

	/* Count how many GRFs were added to _grfconfig_newgame */
	{
		int grf_count = 0;
		for (const auto &g : _grfconfig_newgame) {
			(void)g;
			grf_count++;
		}
		Debug(misc, 0, "[AP] World start ready: seed={}, year={}, map={}x{}, landscape={}, grfs={}",
		      _ap_world_seed_to_use, sd.start_year,
		      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape, grf_count);

		/* Debug: log to file (stdout is DEVNULL in bridge mode) */
		FILE *dbg = fopen("ap_debug.log", "a");
		if (dbg) {
			fmt::print(dbg, "[AP_ConsumeWorldStart] DONE: {} GRFs in _grfconfig_newgame, seed={}\n",
				grf_count, _ap_world_seed_to_use);
			for (const auto &g : _grfconfig_newgame) {
				fmt::print(dbg, "  GRF: {} (status={})\n", g->filename, (int)g->status);
			}
			fclose(dbg);
		}
	}
	Debug(misc, 0, "[AP] Game settings: loans={} trains={} roadveh={} train_len={} station_spread={}",
	      sd.max_loan, sd.max_trains, sd.max_roadveh,
	      sd.max_train_length, sd.station_spread);
	AP_OK(fmt::format("Generating world: seed={} year={} size={}x{} landscape={}",
	      _ap_world_seed_to_use, sd.start_year,
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape));
}

uint32_t AP_GetWorldSeed()
{
	return _ap_world_seed_to_use;
}

/**
 * Populate _ap_pending_sd with slot data.
 * After calling this, the caller should invoke AP_ConsumeWorldStart()
 * to apply all settings and NewGRFs, then StartNewGameWithoutGUI().
 */
void AP_BridgeSetSlotData(const APSlotData &sd)
{
	_ap_pending_sd = sd;
	_ap_pending_world_start = true;
	_ap_session_started = false;
	_ap_engine_map_built = false;
	_ap_cargo_map_built = false;
	Debug(misc, 1, "[AP] Bridge set slot_data: {} missions, year={}, map={}x{}",
	      sd.missions.size(), sd.start_year, (1 << sd.map_x), (1 << sd.map_y));
}

/** Called from connect window when user clicks "Load Save".
 *  Sets the session flag immediately so AP_OnSlotData won't generate a new world. */
void AP_SetPendingLoadSave()
{
	_ap_pending_load_save          = true;
	_ap_world_started_this_session = true; /* suppress world generation on slot_data */
}

/** Returns true once — consumed by intro_gui to open the save/load dialog. */
bool AP_ShouldShowLoadDialog()
{
	if (!_ap_pending_load_save) return false;
	_ap_pending_load_save = false;
	return true;
}

bool AP_IsWaitingForStartChoice()        { return _ap_waiting_for_start_choice; }
void AP_SetWaitingForStartChoice(bool v) { _ap_waiting_for_start_choice = v; }

/* ── Multiplayer host flag (Phase 1+2) ─────────────────────────────── */
static bool _ap_host_multiplayer = false;

void AP_SetHostMultiplayer(bool v) { _ap_host_multiplayer = v; }
bool AP_IsHostMultiplayer()        { return _ap_host_multiplayer; }

/* ── Co-op auto-join: move spectator to Company 0 after map load ── */
static bool _ap_pending_join_company0 = false;

void AP_SetPendingJoinCompany0(bool v) { _ap_pending_join_company0 = v; }
bool AP_IsPendingJoinCompany0()        { return _ap_pending_join_company0; }

/* ── MP Join: deferred join — wait for slot_data + NewGRF setup ── */
/* Variables declared earlier (before AP_OnSlotData callback) */

void AP_RequestMPJoin(const std::string &game_server, const std::string &slot_name)
{
	_ap_mp_join_pending = true;
	_ap_mp_join_server  = game_server;
	_ap_mp_join_slot    = slot_name;
	IConsolePrint(CC_WHITE, "[AP] Waiting for slot data before joining game server...");
}

bool AP_IsMPJoinPending() { return _ap_mp_join_pending; }
bool AP_IsMultiplayerMode() { return _ap_pending_sd.multiplayer_mode; }

/* -------------------------------------------------------------------------
 * Shop and location check API
 * ---------------------------------------------------------------------- */

void AP_ShowConsole(const std::string &msg)
{
	IConsolePrint(CC_INFO, msg);
}

void AP_SendCheckByName(const std::string &location_name)
{
	if (_ap_client == nullptr) return;
	/* Track shop purchases so RebuildShopList can filter already-bought slots */
	if (location_name.rfind("Shop_Purchase_", 0) == 0) {
		_ap_sent_shop_locations.insert(location_name);
	}
	/* Track mission tier completion counts for tier gating */
	if      (location_name.rfind("Mission_Easy_",    0) == 0) _ap_easy_completed++;
	else if (location_name.rfind("Mission_Medium_",  0) == 0) _ap_medium_completed++;
	else if (location_name.rfind("Mission_Hard_",    0) == 0) _ap_hard_completed++;
	else if (location_name.rfind("Mission_Extreme_", 0) == 0) _ap_extreme_completed++;
	AP_TRACE(fmt::format("LocationCheck SENT: '{}'", location_name));
	_ap_client->SendCheckByName(location_name);
}

/** Returns how many missions of a given difficulty prefix have been completed. */
int AP_GetTierCompleted(const std::string &difficulty)
{
	if (difficulty == "easy")    return _ap_easy_completed;
	if (difficulty == "medium")  return _ap_medium_completed;
	if (difficulty == "hard")    return _ap_hard_completed;
	if (difficulty == "extreme") return _ap_extreme_completed;
	return 0;
}

/** Returns total missions completed across all difficulties. */
int AP_GetTotalMissionsCompleted()
{
	return _ap_easy_completed + _ap_medium_completed + _ap_hard_completed + _ap_extreme_completed;
}

/**
 * Shop tier locking: first SHOP_FREE_SLOTS items are always unlocked.
 * After that every SHOP_TIER_SIZE more items require SHOP_TIER_SIZE more
 * completed missions.
 *
 * Rule:
 *   slot_index 0..4  → always unlocked (first 5 items free)
 *   slot_index 5..9  → unlocked after 5  total missions completed
 *   slot_index 10..14→ unlocked after 10 total missions completed
 *   etc.
 *
 * @param slot_index  0-based position in the price-sorted shop list.
 */
static constexpr int SHOP_FREE_SLOTS  = 5;
static constexpr int SHOP_TIER_SIZE   = 5;

bool AP_IsShopSlotUnlocked(int slot_index)
{
	if (slot_index < SHOP_FREE_SLOTS) return true;
	int tier             = (slot_index - SHOP_FREE_SLOTS) / SHOP_TIER_SIZE + 1;
	int missions_needed  = tier * SHOP_TIER_SIZE;
	return AP_GetTotalMissionsCompleted() >= missions_needed;
}

int AP_GetShopSlotRequiredMissions(int slot_index)
{
	if (slot_index < SHOP_FREE_SLOTS) return 0;
	int tier = (slot_index - SHOP_FREE_SLOTS) / SHOP_TIER_SIZE + 1;
	return tier * SHOP_TIER_SIZE;
}

/** Returns the unlock threshold for a difficulty tier (0 = no gate). */
int AP_GetTierThreshold(const std::string &difficulty)
{
	const APSlotData &sd = AP_GetSlotData();
	auto it = sd.tier_unlock_requirements.find(difficulty);
	if (it != sd.tier_unlock_requirements.end()) return it->second;
	return 0;
}

/** Returns true if the given difficulty tier is currently unlocked. */
bool AP_IsTierUnlocked(const std::string &difficulty)
{
	if (difficulty == "easy")    return true;
	if (difficulty == "medium")  return _ap_easy_completed   >= AP_GetTierThreshold("medium");
	if (difficulty == "hard")    return _ap_medium_completed >= AP_GetTierThreshold("hard");
	if (difficulty == "extreme") return _ap_hard_completed  >= AP_GetTierThreshold("extreme");
	return true;
}

/* ─── Track direction lock API ──────────────────────────────────────────── */

bool AP_IsTrackDirLocked(uint8_t railtype, uint8_t track)
{
	uint8_t idx = AP_MapRailTypeToIndex(railtype);
	if (idx >= AP_NUM_RAILTYPES) return false;
	const uint8_t mask = _ap_locked_track_dirs[idx];
	if (mask == 0) return false; /* fast path: this rail type fully unlocked */

	/* Paired tracks share a toolbar button and unlock as a unit:
	 *   TRACK_UPPER(2) + TRACK_LOWER(3)  → BUILD_EW button
	 *   TRACK_LEFT(4)  + TRACK_RIGHT(5)  → BUILD_NS button
	 * If either in a pair is unlocked, treat both as unlocked. */
	if (track == 2 || track == 3) return (mask & (1u << 2)) != 0 && (mask & (1u << 3)) != 0;
	if (track == 4 || track == 5) return (mask & (1u << 4)) != 0 && (mask & (1u << 5)) != 0;
	return (mask & (1u << track)) != 0;
}

/** Returns locked track dir bitmask for a given runtime rail type.
 *  Maps the runtime RailType through AP_MapRailTypeToIndex so NewGRF
 *  types (NG, Metro, VacTube) resolve correctly regardless of load order.
 *  Use this from UI and command code where you have a runtime RailType. */
uint8_t AP_GetLockedTrackDirs(uint8_t railtype)
{
	uint8_t idx = AP_MapRailTypeToIndex(railtype);
	if (idx >= AP_NUM_RAILTYPES) return 0;
	return _ap_locked_track_dirs[idx];
}
/** Returns locked track dir bitmask by direct AP array index (0-6).
 *  Use this from saveload and network code where you already have the
 *  AP index, not a runtime RailType. */
uint8_t AP_GetLockedTrackDirsRaw(uint8_t ap_index)
{
	if (ap_index >= AP_NUM_RAILTYPES) return 0;
	return _ap_locked_track_dirs[ap_index];
}
/** Restores locked track dir bitmask by direct AP array index (0-6).
 *  Use this from saveload and network code. */
void AP_SetLockedTrackDirs(uint8_t ap_index, uint8_t mask)
{
	if (ap_index < AP_NUM_RAILTYPES) _ap_locked_track_dirs[ap_index] = mask;
}

// Back-compat shims — kept so any future callers don't break
bool AP_IsRailDirectionLocked(uint8_t track) { return false; } /* deprecated — use AP_IsTrackDirLocked */
uint8_t AP_GetLockedRailDirs()  { return 0; }
void    AP_SetLockedRailDirs(uint8_t) {}

/* ─── Road direction lock API ───────────────────────────────────────────── */

bool AP_IsRoadDirLocked(uint8_t axis)
{
	if (_ap_locked_road_dirs == 0) return false;
	return (_ap_locked_road_dirs & (1u << axis)) != 0;
}

uint8_t AP_GetLockedRoadDirs() { return _ap_locked_road_dirs; }
void    AP_SetLockedRoadDirs(uint8_t mask) { _ap_locked_road_dirs = mask; }

/* ─── Tram direction lock API ──────────────────────────────────────────── */

bool AP_IsTramDirLocked(uint8_t axis)
{
	if (_ap_locked_tram_dirs == 0) return false;
	return (_ap_locked_tram_dirs & (1u << axis)) != 0;
}

uint8_t AP_GetLockedTramDirs() { return _ap_locked_tram_dirs; }
void    AP_SetLockedTramDirs(uint8_t mask) { _ap_locked_tram_dirs = mask; }

/* ─── Signal lock API ───────────────────────────────────────────────────── */

bool AP_IsSignalLocked(uint8_t sigtype)
{
	if (sigtype > 5) return false;
	if (_ap_locked_signals == 0) return false;
	return (_ap_locked_signals & (1u << sigtype)) != 0;
}

uint8_t AP_GetLockedSignals() { return _ap_locked_signals; }
void    AP_SetLockedSignals(uint8_t mask) { _ap_locked_signals = mask; }

/* ─── Bridge lock API ───────────────────────────────────────────────────── */

bool AP_IsBridgeLocked(uint8_t bridge_type)
{
	if (bridge_type > 12) return false;
	if (_ap_locked_bridges == 0) return false;
	return (_ap_locked_bridges & (1u << bridge_type)) != 0;
}

bool AP_IsBridgeLocked() { return _ap_locked_bridges != 0; }
uint16_t AP_GetLockedBridges() { return _ap_locked_bridges; }
void     AP_SetLockedBridges(uint16_t mask) { _ap_locked_bridges = mask; }

/* ─── Industry protection API ──────────────────────────────────────────── */

bool AP_IsIndustryProtected(uint16_t industry_id)
{
	if (!AP_IsActive()) return false;
	for (const APMission &m : _ap_pending_sd.missions) {
		if (m.completed) continue;
		if (m.named_entity.id == (int32_t)industry_id) return true;
	}
	return false;
}

/* ─── Tunnel lock API ───────────────────────────────────────────────────── */

bool AP_IsTunnelLocked() { return _ap_locked_tunnels; }

bool AP_GetLockedTunnels() { return _ap_locked_tunnels; }
void AP_SetLockedTunnels(bool v) { _ap_locked_tunnels = v; }

/* ─── Airport lock API ──────────────────────────────────────────────────── */

bool AP_IsAirportLocked(uint8_t airport_type)
{
	if (airport_type == 0) return false; /* AT_SMALL always free */
	if (airport_type > 8) return false;
	if (_ap_locked_airports == 0) return false;
	return (_ap_locked_airports & (1u << airport_type)) != 0;
}

uint16_t AP_GetLockedAirports() { return _ap_locked_airports; }
void     AP_SetLockedAirports(uint16_t mask) { _ap_locked_airports = mask; }

/* ─── Tree lock API ─────────────────────────────────────────────────────── */

bool AP_IsTreeLocked(uint8_t tree_type)
{
	/* Map raw TreeType to pack index (0-9).
	 * Temperate: 0-6 → packs 0,1,2 (3 trees each except last=1)
	 * Sub-arctic: 7-13 → packs 3,4,5
	 * Tropical: 14-20 → packs 6,7,8
	 * Toyland: 21-32 → pack 9 */
	uint8_t pack;
	if (tree_type <= 6) {
		pack = tree_type / 3; /* 0-2→0, 3-5→1, 6→2 */
		if (pack > 2) pack = 2;
	} else if (tree_type <= 13) {
		pack = 3 + (tree_type - 7) / 3;
		if (pack > 5) pack = 5;
	} else if (tree_type <= 20) {
		pack = 6 + (tree_type - 14) / 3;
		if (pack > 8) pack = 8;
	} else {
		pack = 9; /* Toyland */
	}
	if (_ap_locked_trees == 0) return false;
	return (_ap_locked_trees & (1u << pack)) != 0;
}

uint16_t AP_GetLockedTrees() { return _ap_locked_trees; }
void     AP_SetLockedTrees(uint16_t mask) { _ap_locked_trees = mask; }

/* ─── Terraform lock API ────────────────────────────────────────────────── */

bool AP_IsTerraformRaiseLocked()
{
	return (_ap_locked_terraform & (1u << 0)) != 0;
}

bool AP_IsTerraformLowerLocked()
{
	return (_ap_locked_terraform & (1u << 1)) != 0;
}

uint8_t AP_GetLockedTerraform() { return _ap_locked_terraform; }
void    AP_SetLockedTerraform(uint8_t mask) { _ap_locked_terraform = mask; }

/* ── Town Authority action lock checks ─────────────────────────────── */
bool AP_IsTownActionLocked(uint8_t action_idx)
{
	if (action_idx > 7) return false;
	if (_ap_locked_town_actions == 0) return false;
	return (_ap_locked_town_actions & (1u << action_idx)) != 0;
}

bool AP_IsTownActionLocked() { return _ap_locked_town_actions != 0; }

uint8_t AP_GetLockedTownActions() { return _ap_locked_town_actions; }
void    AP_SetLockedTownActions(uint8_t mask) { _ap_locked_town_actions = mask; }

/* ── Speed Boost accessors ──────────────────────────────────────────── */
int  AP_GetFfSpeed()  { return _ap_ff_speed; }
void AP_SetFfSpeed(int v)
{
	_ap_ff_speed = std::clamp(v, 100, 300);
	_settings_client.gui.fast_forward_speed_limit = (uint16_t)_ap_ff_speed;
}

/* ── Task System accessors ──────────────────────────────────────────── */
std::vector<APTask> AP_GetTaskSnapshot()        { return _ap_tasks; }
int AP_GetTaskChecksCompleted()                  { return _ap_task_checks_completed; }
int AP_GetTaskChecksCompletedSaved()             { return _ap_task_checks_completed; }
void AP_SetTaskChecksCompleted(int v)            { _ap_staging_task_checks = v; _ap_staging_has_data = true; }

std::string AP_GetTasksStr()
{
	std::string out;
	for (const APTask &t : _ap_tasks) {
		if (!out.empty()) out += ';';
		out += fmt::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
		    t.id, t.type, t.amount, t.current_value,
		    (int)t.completed, (int)t.expired, (int)t.monitor_seeded,
		    t.deadline_year, (int)t.reward_type, t.reward_cash,
		    t.entity_id, (int)t.cargo, t.entity_tile, t.difficulty, t.removal_year);
	}
	return out;
}

void AP_SetTasksStr(const std::string &s)
{
	_ap_staging_tasks    = s;
	_ap_staging_has_data = true;
}

static void AP_ApplyTasksStr(const std::string &s)
{
	_ap_tasks.clear();
	if (s.empty()) return;

	auto split = [](const std::string &str, char delim) -> std::vector<std::string> {
		std::vector<std::string> v;
		std::string tok;
		for (char c : str) {
			if (c == delim) { v.push_back(tok); tok.clear(); }
			else tok += c;
		}
		if (!tok.empty()) v.push_back(tok);
		return v;
	};

	auto parse_i64 = [](const std::string &s2, int64_t def = 0) -> int64_t {
		int64_t r = def;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};
	auto parse_int = [](const std::string &s2, int def = 0) -> int {
		int r = def;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};

	for (const std::string &rec : split(s, ';')) {
		auto f = split(rec, ',');
		if (f.size() < 14) continue;
		APTask t;
		t.id              = parse_int(f[0]);
		t.type            = f[1];
		t.amount          = parse_i64(f[2]);
		t.current_value   = parse_i64(f[3]);
		t.completed       = parse_int(f[4]) != 0;
		t.expired         = parse_int(f[5]) != 0;
		t.monitor_seeded  = parse_int(f[6]) != 0;
		t.deadline_year   = parse_int(f[7]);
		t.reward_type     = (APTaskRewardType)parse_int(f[8]);
		t.reward_cash     = parse_i64(f[9]);
		t.entity_id       = parse_int(f[10]);
		t.cargo           = (uint8_t)parse_int(f[11]);
		t.entity_tile     = (uint32_t)parse_i64(f[12]);
		t.difficulty      = f[13];
		if (f.size() >= 15) t.removal_year = parse_int(f[14]);
		_ap_tasks.push_back(std::move(t));
	}

	for (const APTask &t : _ap_tasks)
		if (t.id >= _ap_task_next_id) _ap_task_next_id = t.id + 1;
}

/* ── Ruin System save/load ──────────────────────────────────────────── */

/** Pack ruin state into a single string for savegame.
 *  Format: "spawned,completed,cooldown,initial#id;tile;variant;town_id;cargo0_type:cargo0_req:cargo0_del,..." */
std::string AP_GetRuinsStr()
{
	/* Header: counters */
	std::string out = fmt::format("{},{},{},{}",
		_ap_ruins_spawned, _ap_ruins_completed, _ap_ruin_cooldown_months,
		_ap_ruins_initial_spawned ? 1 : 0);

	/* Active ruins separated by '#' */
	for (const APRuin &r : _ap_active_ruins) {
		out += '#';
		out += fmt::format("{};{};{};{}", r.id, r.tile, r.object_variant, (int)r.town_id);
		for (size_t i = 0; i < r.cargo_reqs.size(); i++) {
			out += ';';
			out += fmt::format("{}:{}:{}:{}", (int)r.cargo_reqs[i].cargo,
			       r.cargo_reqs[i].required, r.cargo_reqs[i].delivered,
			       r.cargo_reqs[i].cargo_name);
		}
	}
	return out;
}

void AP_SetRuinsStr(const std::string &s)
{
	_ap_active_ruins.clear();
	_ap_ruins_spawned   = 0;
	_ap_ruins_completed = 0;
	_ap_ruin_cooldown_months = 0;
	_ap_ruins_initial_spawned = false;
	if (s.empty()) return;

	auto split = [](const std::string &str, char delim) -> std::vector<std::string> {
		std::vector<std::string> v;
		std::string tok;
		for (char c : str) {
			if (c == delim) { v.push_back(tok); tok.clear(); }
			else tok += c;
		}
		if (!tok.empty()) v.push_back(tok);
		return v;
	};
	auto parse_int = [](const std::string &s2, int def = 0) -> int {
		int r = def;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};
	auto parse_i64 = [](const std::string &s2, int64_t def = 0) -> int64_t {
		int64_t r = def;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};

	auto parts = split(s, '#');
	if (parts.empty()) return;

	/* Parse header */
	auto header = split(parts[0], ',');
	if (header.size() >= 1) _ap_ruins_spawned        = parse_int(header[0]);
	if (header.size() >= 2) _ap_ruins_completed       = parse_int(header[1]);
	if (header.size() >= 3) _ap_ruin_cooldown_months  = parse_int(header[2]);
	if (header.size() >= 4) _ap_ruins_initial_spawned = (parse_int(header[3]) != 0);

	/* Parse active ruins */
	for (size_t pi = 1; pi < parts.size(); pi++) {
		auto fields = split(parts[pi], ';');
		if (fields.size() < 4) continue;

		APRuin r;
		r.id              = parse_int(fields[0]);
		r.tile            = (uint32_t)parse_i64(fields[1]);
		r.object_variant  = parse_int(fields[2]);
		r.town_id         = (uint16_t)parse_int(fields[3]);

		/* Resolve location name from slot_data */
		if (r.id >= 0 && r.id < (int)_ap_pending_sd.ruin_locations.size()) {
			r.location_name = _ap_pending_sd.ruin_locations[r.id];
		} else {
			r.location_name = fmt::format("Ruin_{:03d}", r.id + 1);
		}

		/* Resolve town name */
		Town *town = Town::GetIfValid((TownID)r.town_id);
		if (town != nullptr) {
			r.town_name = AP_TownName(town);
		} else {
			r.town_name = fmt::format("Town #{}", (int)r.town_id);
		}

		/* Parse cargo requirements (fields 4+) */
		for (size_t fi = 4; fi < fields.size(); fi++) {
			auto cargo_parts = split(fields[fi], ':');
			if (cargo_parts.size() < 3) continue;
			APRuinCargoReq req;
			req.cargo     = (uint8_t)parse_int(cargo_parts[0]);
			req.required  = parse_i64(cargo_parts[1]);
			req.delivered = parse_i64(cargo_parts[2]);
			/* New saves (exp-5.0+) include cargo name as 4th field.
			 * Old saves fall back to placeholder resolved by AP_RefreshRuinNames(). */
			if (cargo_parts.size() >= 4 && !cargo_parts[3].empty()) {
				req.cargo_name = cargo_parts[3];
			} else {
				req.cargo_name = fmt::format("cargo#{}", (int)req.cargo);
			}
			r.cargo_reqs.push_back(req);
		}

		_ap_active_ruins.push_back(std::move(r));
	}

	AP_OK(fmt::format("Loaded ruin state: {}/{} spawned, {} completed, {} active, {} cooldown months",
	       _ap_ruins_spawned, _ap_pending_sd.ruin_pool_size, _ap_active_ruins.size(),
	       _ap_ruins_completed, _ap_ruin_cooldown_months));
}

/* ── Star System API & save/load ───────────────────────────────────── */

/* ── Star Placement (implementations — after _ap_pending_sd) ────────── */

static bool AP_PlaceSingleStar(APStar &star)
{
	AP_ResolveStarTypes();
	ObjectType ot = _ap_star_types[star.object_variant % AP_NUM_STAR_TYPES];
	if (ot == INVALID_OBJECT_TYPE) return false;

	int skip_invalid = 0, skip_edge = 0, skip_type = 0, skip_building = 0, skip_cmd = 0;
	for (int attempt = 0; attempt < 5000; attempt++) {
		TileIndex tile = RandomTile();
		if (!IsValidTile(tile)) { skip_invalid++; continue; }
		if (TileX(tile) < 2 || TileY(tile) < 2 ||
		    TileX(tile) >= Map::SizeX() - 2 || TileY(tile) >= Map::SizeY() - 2) { skip_edge++; continue; }

		/* Quick pre-filter: only clear land or trees, away from towns */
		if (!IsTileType(tile, MP_CLEAR) && !IsTileType(tile, MP_TREES)) { skip_type++; continue; }
		if (AP_IsTileNearBuilding(tile, 4)) { skip_building++; continue; }

		CompanyID old = _current_company;
		_current_company = _local_company;
		auto ret = Command<CMD_BUILD_OBJECT>::Do(
			DoCommandFlags{DoCommandFlag::Execute, DoCommandFlag::Auto, DoCommandFlag::NoTestTownRating, DoCommandFlag::NoModifyTownRating},
			tile, ot, 0);
		_current_company = old;

		if (ret.Succeeded()) {
			star.tile = tile.base();
			return true;
		}
		skip_cmd++;
	}
	AP_WARN(fmt::format("Star placement stats: invalid={} edge={} type={} near_building={} cmd_fail={}",
		skip_invalid, skip_edge, skip_type, skip_building, skip_cmd));
	return false;
}

static int _ap_stars_revealed = 0;

/** Escalating cost: 50k, 500k, 1M, 5M, 10M, 20M, 50M */
static const int64_t _star_reveal_costs[] = {
    50000, 500000, 1000000, 5000000, 10000000, 20000000, 50000000
};
static constexpr int _star_reveal_cost_count = 7;

/** Initialise star entries (without placing them on map yet).
 *  Actual placement happens gradually via AP_StarMonthlyTick(). */
static void AP_InitStarEntries()
{
	auto &sd = _ap_pending_sd;
	if (!sd.enable_stars || sd.star_pool_size <= 0) return;

	_ap_stars.clear();
	_ap_stars_collected = 0;
	_ap_stars_revealed = 0;
	AP_ResolveStarTypes();

	for (int i = 0; i < sd.star_pool_size && i < (int)sd.star_locations.size(); i++) {
		APStar star;
		star.id = i;
		star.location_name = sd.star_locations[i];
		star.object_variant = i % AP_NUM_STAR_TYPES;
		/* tile stays UINT32_MAX — placed later by monthly tick */
		_ap_stars.push_back(star);
	}
	AP_OK(fmt::format("Initialised {} star entries (placement pending)", (int)_ap_stars.size()));
}

/** Try to place up to `batch_size` unplaced stars on the map.
 *  Returns number successfully placed this batch. */
static int AP_PlaceStarBatch(int batch_size)
{
	int placed = 0;
	for (APStar &star : _ap_stars) {
		if (placed >= batch_size) break;
		if (star.tile != UINT32_MAX) continue;  /* already placed */
		if (star.collected) continue;            /* already collected */
		if (AP_PlaceSingleStar(star)) {
			placed++;
		}
	}
	if (placed > 0) {
		int total_placed = (int)std::count_if(_ap_stars.begin(), _ap_stars.end(),
			[](const APStar &s){ return s.tile != UINT32_MAX; });
		AP_OK(fmt::format("Placed {} stars this batch ({}/{} total on map)",
			placed, total_placed, (int)_ap_stars.size()));
	}
	return placed;
}

/* ── Star System API ───────────────────────────────────────────────── */

bool AP_IsStarTile(uint32_t tile_index)
{
	for (const APStar &s : _ap_stars) {
		if (s.tile == tile_index && !s.collected) return true;
	}
	return false;
}

bool AP_CollectStar(uint32_t tile_index)
{
	for (APStar &star : _ap_stars) {
		if (star.tile != tile_index || star.collected) continue;
		star.collected = true;
		_ap_stars_collected++;

		/* Explosion effect */
		TileIndex ti{tile_index};
		CreateEffectVehicleAbove(
			TileX(ti) * TILE_SIZE + TILE_SIZE / 2,
			TileY(ti) * TILE_SIZE + TILE_SIZE / 2,
			2, EV_EXPLOSION_SMALL);

		/* Remove the object via command system using OWNER_WATER.
		 * OWNER_WATER bypasses ALL removal checks including OBJ_FLAG_IRREMOVABLE
		 * (see object_cmd.cpp ClearTile_Object: "Water can remove everything!"). */
		CompanyID old = _current_company;
		_current_company = OWNER_WATER;
		Command<CMD_LANDSCAPE_CLEAR>::Do(
			DoCommandFlags{DoCommandFlag::Execute}, ti);
		_current_company = old;

		/* If this star was revealed in the tracker, decrement the reveal count
		 * so the player doesn't get a free reveal of the next star. */
		int pos = 0;
		for (const APStar &s2 : _ap_stars) {
			if (&s2 == &star) break;
			if (!s2.collected) pos++;
		}
		if (pos < _ap_stars_revealed) {
			_ap_stars_revealed--;
		}

		/* Send AP check */
		AP_SendCheckByName(star.location_name);

		/* Refresh statusbar and star tracker window */
		InvalidateWindowData(WC_STATUS_BAR, 0, 0);
		InvalidateWindowClassesData(WC_ARCHIPELAGO_STAR_TRACKER, 0);

		AP_OK(fmt::format("Star {} collected at tile {}", star.location_name, tile_index));
		return true;
	}
	return false;
}

int AP_GetStarsCollected() { return _ap_stars_collected; }
int AP_GetStarPoolSize()   { return (int)_ap_stars.size(); }

int64_t AP_GetStarRevealCost()
{
    int idx = std::min(_ap_stars_revealed, _star_reveal_cost_count - 1);
    return _star_reveal_costs[idx];
}

int AP_GetStarsRevealed() { return _ap_stars_revealed; }

std::vector<APStarView> AP_GetRevealedStarViews()
{
    std::vector<APStarView> views;
    int reveal_count = 0;
    for (const APStar &s : _ap_stars) {
        if (s.collected) continue;
        if (reveal_count < _ap_stars_revealed) {
            APStarView v;
            v.id = s.id;
            v.location_name = s.location_name;
            v.tile = s.tile;
            v.collected = false;
            v.revealed = true;
            views.push_back(v);
            reveal_count++;
        }
    }
    return views;
}

bool AP_RevealNextStar()
{
    /* Find next uncollected, unrevealed star */
    int uncollected_count = 0;
    for (const APStar &s : _ap_stars) {
        if (!s.collected) uncollected_count++;
    }
    if (uncollected_count <= _ap_stars_revealed) return false; /* All already revealed or collected */

    int64_t cost = AP_GetStarRevealCost();

    /* Check if player can afford it */
    CompanyID company = _local_company;
    if (company >= MAX_COMPANIES) return false;
    const Company *c = Company::GetIfValid(company);
    if (!c) return false;
    if (c->money < (Money)cost) return false;

    /* Deduct money */
    SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, (Money)cost));

    _ap_stars_revealed++;
    return true;
}

std::string AP_GetStarsStr()
{
	std::string out = fmt::format("{},{},{}", _ap_stars_collected, _ap_stars_revealed,
		_ap_stars_initial_spawned ? 1 : 0);
	for (const APStar &s : _ap_stars) {
		out += '#';
		out += fmt::format("{};{};{};{}", s.id, s.tile, s.object_variant, s.collected ? 1 : 0);
	}
	return out;
}

void AP_SetStarsStr(const std::string &s)
{
	_ap_stars.clear();
	_ap_stars_collected = 0;
	_ap_stars_revealed = 0;
	_ap_stars_initial_spawned = false;
	if (s.empty()) return;

	auto split = [](const std::string &str, char delim) -> std::vector<std::string> {
		std::vector<std::string> v;
		std::string tok;
		for (char c : str) {
			if (c == delim) { v.push_back(tok); tok.clear(); }
			else tok += c;
		}
		if (!tok.empty()) v.push_back(tok);
		return v;
	};
	auto parse_int = [](const std::string &s2, int def = 0) -> int {
		int r = def;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};
	auto parse_u32 = [](const std::string &s2) -> uint32_t {
		uint32_t r = UINT32_MAX;
		std::from_chars(s2.data(), s2.data() + s2.size(), r);
		return r;
	};

	auto parts = split(s, '#');
	if (parts.empty()) return;
	/* Header: "collected,revealed" (revealed may be absent in older saves) */
	auto header_fields = split(parts[0], ',');
	_ap_stars_collected = parse_int(header_fields[0]);
	if (header_fields.size() >= 2) _ap_stars_revealed = parse_int(header_fields[1]);
	if (header_fields.size() >= 3) _ap_stars_initial_spawned = (parse_int(header_fields[2]) != 0);

	for (size_t i = 1; i < parts.size(); i++) {
		auto fields = split(parts[i], ';');
		if (fields.size() < 4) continue;
		APStar star;
		star.id = parse_int(fields[0]);
		star.tile = parse_u32(fields[1]);
		star.object_variant = parse_int(fields[2]);
		star.collected = (fields[3] == "1");
		if (star.id >= 0 && star.id < (int)_ap_pending_sd.star_locations.size()) {
			star.location_name = _ap_pending_sd.star_locations[star.id];
		}
		_ap_stars.push_back(star);
	}
	AP_OK(fmt::format("Loaded star state: {} collected, {} total", _ap_stars_collected, _ap_stars.size()));
}

/** Re-resolve ruin location_name and cargo_name after slot_data/cargo-map
 *  become available.  Called once after AP connect for loaded games. */
static void AP_RefreshRuinNames()
{
	if (!_ap_cargo_map_built) BuildCargoMap();
	for (auto &ruin : _ap_active_ruins) {
		/* Fix location_name from slot_data */
		if (ruin.id >= 0 && ruin.id < (int)_ap_pending_sd.ruin_locations.size()) {
			ruin.location_name = _ap_pending_sd.ruin_locations[ruin.id];
		}
		/* Fix cargo_name — skip if already resolved (exp-5.0+ saves include names).
		 * Only fix placeholder names that start with "cargo#". */
		for (auto &req : ruin.cargo_reqs) {
			if (req.cargo_name.substr(0, 6) != "cargo#") continue;
			/* Try reverse cargo map lookup first */
			bool found = false;
			for (const auto &[name, ct] : _ap_cargo_map) {
				if (ct == (CargoType)req.cargo) { req.cargo_name = name; found = true; break; }
			}
			/* Fallback: scan all CargoSpecs directly (handles FIRS and other NewGRFs) */
			if (!found) {
				for (const CargoSpec *cs : CargoSpec::Iterate()) {
					if (cs->Index() == (CargoType)req.cargo) {
						req.cargo_name = GetString(cs->name);
						/* Lowercase for consistency with _ap_cargo_map keys */
						for (char &ch : req.cargo_name) ch = (char)tolower((unsigned char)ch);
						found = true;
						break;
					}
				}
			}
		}
	}
	if (!_ap_active_ruins.empty()) {
		AP_OK(fmt::format("Refreshed {} active ruin names from slot data", _ap_active_ruins.size()));
	}
}

/* ── Task System implementation ─────────────────────────────────────── */

/* Forward declaration needed by task system (defined later in this file) */
static std::string AP_IndustryLabel(const Industry *ind);

/** Compact money string for task descriptions and news messages. */
static std::string AP_FormatMoneyCompact_Task(int64_t amount)
{
	const CurrencySpec &cs = GetCurrency();
	int64_t scaled = amount * cs.rate;
	std::string num;
	if (scaled >= 1000000) num = fmt::format("{:.0f}M", scaled / 1000000.0);
	else if (scaled >= 1000) num = fmt::format("{}k", scaled / 1000);
	else num = fmt::format("{}", scaled);
	if (cs.symbol_pos == 0) return cs.prefix + num + cs.suffix;
	return num + cs.suffix;
}

/** Re-derive entity names + descriptions for tasks loaded from savegame. */
static void AP_RefreshTaskNames()
{
	for (APTask &t : _ap_tasks) {
		if (!t.entity_name.empty()) continue; /* already set */
		if (t.entity_id < 0) continue;

		if (t.type == "cargo_from_industry") {
			const Industry *ind = Industry::GetIfValid((IndustryID)t.entity_id);
			if (ind && ind->town) {
				t.entity_name = AP_IndustryLabel(ind);
				/* Build cargo name */
				std::string cargo_n = "cargo";
				if (IsValidCargoType((CargoType)t.cargo)) {
					const CargoSpec *cs = CargoSpec::Get((CargoType)t.cargo);
					if (cs) cargo_n = GetString(cs->name);
				}
				if (t.reward_type == APTaskRewardType::MISSION_CHECK)
					t.description = fmt::format("Pick up {} t of {} from {} [REWARD: Mission Check]", AP_Num(t.amount), cargo_n, t.entity_name);
				else
					t.description = fmt::format("Pick up {} t of {} from {} [Reward: +{}]",
					    AP_Num(t.amount), cargo_n, t.entity_name, AP_FormatMoneyCompact_Task(t.reward_cash));
			}
		} else if (t.type == "passengers_to_town") {
			const Town *town = Town::GetIfValid((TownID)t.entity_id);
			if (town) {
				t.entity_name = GetString(STR_TOWN_NAME, town->index);
				if (t.reward_type == APTaskRewardType::MISSION_CHECK)
					t.description = fmt::format("Deliver {} passengers to {} [REWARD: Mission Check]", AP_Num(t.amount), t.entity_name);
				else
					t.description = fmt::format("Deliver {} passengers to {} [Reward: +{}]",
					    AP_Num(t.amount), t.entity_name, AP_FormatMoneyCompact_Task(t.reward_cash));
			}
		}
	}
}

/** Seed (drain stale data from) the cargo monitor for a task.
 *  Called once per task after creation so progress only counts from task start. */
static void AP_SeedTaskMonitor(const APTask &t)
{
	CompanyID cid = _local_company;
	if (!Company::IsValidID(cid)) {
		AP_WARN(fmt::format("[Task] Seed SKIPPED (no company) type={} eid={}", t.type, t.entity_id));
		return;
	}

	if (t.type == "cargo_from_industry" && t.entity_id >= 0 && IsValidCargoType((CargoType)t.cargo)) {
		IndustryID iid = (IndustryID)t.entity_id;
		CargoMonitorID mon = EncodeCargoIndustryMonitor(cid, (CargoType)t.cargo, iid);
		GetPickupAmount(mon, true); /* discard — start fresh */
		AP_OK(fmt::format("[Task] Seeded pickup monitor: industry={} cargo={} monid={:08X}", t.entity_id, (int)t.cargo, mon));
	} else if (t.type == "passengers_to_town" && t.entity_id >= 0) {
		CargoType ct = AP_FindCargoType("passengers");
		if (!IsValidCargoType(ct)) return;
		TownID tid = (TownID)t.entity_id;
		CargoMonitorID mon = EncodeCargoTownMonitor(cid, ct, tid);
		GetDeliveryAmount(mon, true); /* discard */
		AP_OK(fmt::format("[Task] Seeded delivery monitor: town={} cargo={} monid={:08X}", t.entity_id, (int)ct, mon));
	}
}

/** Try to generate one new task from the current map.
 *  Returns false if no valid candidate could be found. */
static bool AP_TryMakeTask(APTask &out)
{
	CompanyID cid = _local_company;
	if (!Company::IsValidID(cid)) return false;

	/* Build sets of entity IDs already in active tasks (avoid duplicates) */
	std::set<int32_t> used_ids;
	for (const APTask &t : _ap_tasks)
		if (!t.completed && !t.expired) used_ids.insert(t.entity_id);

	/* Collect valid producing industries */
	std::vector<const Industry *> prod_inds;
	for (const Industry *ind : Industry::Iterate()) {
		if (ind->location.tile == INVALID_TILE) continue;
		if (used_ids.count((int32_t)ind->index.base())) continue;
		for (const auto &slot : ind->produced)
			if (IsValidCargoType(slot.cargo)) { prod_inds.push_back(ind); break; }
	}

	/* Collect towns with >= 200 population */
	std::vector<const Town *> towns;
	CargoType pass_ct = AP_FindCargoType("passengers");
	if (IsValidCargoType(pass_ct)) {
		for (const Town *t : Town::Iterate()) {
			if ((uint32_t)t->cache.population < 200) continue;
			if (used_ids.count((int32_t)t->index.base())) continue;
			towns.push_back(t);
		}
	}

	bool can_cargo = !prod_inds.empty();
	bool can_pass  = !towns.empty();
	if (!can_cargo && !can_pass) return false;

	/* Pick type: 60% cargo, 40% passengers (if available) */
	bool use_cargo;
	if (can_cargo && can_pass)
		use_cargo = (InteractiveRandomRange(100) < 60);
	else
		use_cargo = can_cargo;

	/* Pick difficulty */
	int dr = (int)InteractiveRandomRange(100);
	std::string diff;
	int64_t amount, reward_cash;
	int deadline_years;
	if (dr < 50)       { diff = "easy";   amount = 500;  reward_cash = 25000;  deadline_years = 1; }
	else if (dr < 80)  { diff = "medium"; amount = 2000; reward_cash = 150000; deadline_years = 2; }
	else               { diff = "hard";   amount = 8000; reward_cash = 500000; deadline_years = 3; }

	/* 30% chance of Mission Check reward */
	APTaskRewardType rtype = (InteractiveRandomRange(100) < 30)
	    ? APTaskRewardType::MISSION_CHECK
	    : APTaskRewardType::CASH;

	int32_t cur_year = (int32_t)TimerGameCalendar::year.base();

	if (use_cargo) {
		/* Shuffle for randomness */
		for (size_t i = prod_inds.size(); i > 1; i--) {
			size_t j = InteractiveRandomRange((uint32_t)i);
			if (j < i) std::swap(prod_inds[i - 1], prod_inds[j]);
		}
		const Industry *src = nullptr;
		CargoType ct = INVALID_CARGO;
		for (const Industry *ind : prod_inds) {
			for (const auto &slot : ind->produced) {
				if (IsValidCargoType(slot.cargo)) { ct = slot.cargo; break; }
			}
			if (ct != INVALID_CARGO) { src = ind; break; }
		}
		if (!src) return false;

		std::string cargo_n = "cargo";
		const CargoSpec *cs = CargoSpec::Get(ct);
		if (cs) cargo_n = GetString(cs->name);

		out.id             = _ap_task_next_id++;
		out.type           = "cargo_from_industry";
		out.amount         = amount;
		out.current_value  = 0;
		out.completed      = false;
		out.expired        = false;
		out.monitor_seeded = false;
		out.deadline_year  = cur_year + deadline_years;
		out.deadline_month = 1;
		out.reward_type    = rtype;
		out.reward_cash    = reward_cash;
		out.entity_id      = (int32_t)src->index.base();
		out.cargo          = (uint8_t)ct;
		out.entity_tile    = src->location.tile.base();
		out.entity_name    = AP_IndustryLabel(src);
		out.difficulty     = diff;
		if (rtype == APTaskRewardType::MISSION_CHECK)
			out.description = fmt::format("Pick up {} t of {} from {} [REWARD: Mission Check]", AP_Num(amount), cargo_n, out.entity_name);
		else
			out.description = fmt::format("Pick up {} t of {} from {} [Reward: +{}]", AP_Num(amount), cargo_n, out.entity_name, AP_FormatMoneyCompact_Task(reward_cash));
		return true;

	} else {
		/* Passenger task */
		for (size_t i = towns.size(); i > 1; i--) {
			size_t j = InteractiveRandomRange((uint32_t)i);
			if (j < i) std::swap(towns[i - 1], towns[j]);
		}
		const Town *dst = towns[0];

		int64_t pass_amount = amount; /* reuse same amounts */
		std::string town_name = GetString(STR_TOWN_NAME, dst->index);

		out.id             = _ap_task_next_id++;
		out.type           = "passengers_to_town";
		out.amount         = pass_amount;
		out.current_value  = 0;
		out.completed      = false;
		out.expired        = false;
		out.monitor_seeded = false;
		out.deadline_year  = cur_year + deadline_years;
		out.deadline_month = 1;
		out.reward_type    = rtype;
		out.reward_cash    = reward_cash;
		out.entity_id      = (int32_t)dst->index.base();
		out.cargo          = (uint8_t)pass_ct;
		out.entity_tile    = dst->xy.base();
		out.entity_name    = town_name;
		out.difficulty     = diff;
		if (rtype == APTaskRewardType::MISSION_CHECK)
			out.description = fmt::format("Deliver {} passengers to {} [REWARD: Mission Check]", AP_Num(pass_amount), town_name);
		else
			out.description = fmt::format("Deliver {} passengers to {} [Reward: +{}]", AP_Num(pass_amount), town_name, AP_FormatMoneyCompact_Task(reward_cash));
		return true;
	}
}

/** Generate/replenish tasks up to AP_TASK_MAX_ACTIVE.
 *  Safe to call multiple times — only adds tasks if there's room. */
void AP_GenerateTasks()
{
	/* Count active (not done, not expired) */
	auto active_count = [&]() -> int {
		int n = 0;
		for (const APTask &t : _ap_tasks)
			if (!t.completed && !t.expired) n++;
		return n;
	};

	int attempts = 0;
	while (active_count() < AP_TASK_MAX_ACTIVE && attempts < 20) {
		APTask t;
		if (AP_TryMakeTask(t)) {
			AP_SeedTaskMonitor(t);
			t.monitor_seeded = true;
			_ap_tasks.push_back(std::move(t));
		}
		attempts++;
	}
}

/** Apply task reward and mark completed. */
static void AP_CompleteTask(APTask &t)
{
	t.completed    = true;
	t.removal_year = (int32_t)TimerGameCalendar::year.base() + 1; /* remove ~1 in-game year after completion */
	AP_TRACE(fmt::format("TaskComplete: id={} desc='{}' reward_type={} reward_cash={}",
		t.id, t.description.substr(0, 60), (int)t.reward_type, t.reward_cash));
	CompanyID cid = _local_company;
	if (!Company::IsValidID(cid)) return;
	Company *c = Company::Get(cid);

	if (t.reward_type == APTaskRewardType::CASH) {
		AP_ChangeMoney(cid, (Money)t.reward_cash);
		AP_ShowNews(fmt::format("[Task] Completed! Reward: +{}",
		    AP_FormatMoneyCompact_Task(t.reward_cash)));
	} else {
		_ap_task_checks_completed++;
		AP_ShowNews("[Task] Completed! Reward: Mission Check earned!");
	}
}

/** Monthly update: advance task progress, detect completion/expiry, replenish. */
static void AP_UpdateTasks()
{
	if (!AP_IsConnected()) return;
	CompanyID cid = _local_company;
	if (!Company::IsValidID(cid)) return;

	int32_t cur_year  = (int32_t)TimerGameCalendar::year.base();

	/* Remove completed/expired tasks whose removal_year has passed */
	_ap_tasks.erase(
		std::remove_if(_ap_tasks.begin(), _ap_tasks.end(),
			[cur_year](const APTask &t) {
				return (t.completed || t.expired) &&
				       t.removal_year > 0 &&
				       cur_year >= t.removal_year;
			}),
		_ap_tasks.end());

	for (APTask &t : _ap_tasks) {
		if (t.completed || t.expired) continue;

		/* Seed monitor on first update if not yet done */
		if (!t.monitor_seeded) {
			AP_SeedTaskMonitor(t);
			t.monitor_seeded = true;
			continue; /* skip progress this tick — start counting next month */
		}

		/* Check expiry */
		if (cur_year > t.deadline_year) {
			t.expired = true;
			t.removal_year = cur_year + 1; /* remove ~1 in-game year after expiry */
			AP_TRACE(fmt::format("TaskExpired: id={} desc='{}' deadline={}", t.id, t.description.substr(0, 60), t.deadline_year));
			AP_ShowNews(fmt::format("[Task] Expired: {}", t.description.substr(0, 50)));
			continue;
		}

		/* Progress is accumulated in AP_UpdateNamedMissions() every 250 ms.
		 * Here we only check expiry and completion. */

		/* Validate entity still exists (industry tasks only) */
		if (t.type == "cargo_from_industry") {
			IndustryID iid = (IndustryID)t.entity_id;
			if (!Industry::IsValidID(iid)) { t.expired = true; t.removal_year = cur_year + 1; continue; }
		} else if (t.type == "passengers_to_town") {
			TownID tid = (TownID)t.entity_id;
			if (!Town::IsValidID(tid)) { t.expired = true; t.removal_year = cur_year + 1; continue; }
		}

		/* Check completion */
		if (t.current_value >= t.amount) {
			AP_CompleteTask(t);
		}
	}

	/* Replenish active tasks */
	AP_GenerateTasks();
}

int AP_GetShopSlots()
{
	/* Returns the total number of shop purchase locations for this game. */
	const APSlotData &sd = AP_GetSlotData();
	return sd.shop_slots > 0 ? sd.shop_slots : 100;
}

std::string AP_GetShopLocationLabel(const std::string &location_name)
{
	/* First: check slot_data for the actual item name at this location */
	const APSlotData &sd = AP_GetSlotData();
	auto it = sd.shop_item_names.find(location_name);
	if (it != sd.shop_item_names.end() && !it->second.empty())
		return it->second;

	/* Fallback: LocationScouts hint (returns "player (game)" for multi-world) */
	if (_ap_client != nullptr) {
		std::string hint = _ap_client->GetLocationHint(location_name);
		if (!hint.empty()) return hint;
	}
	/* Last resort: extract slot number and show "Slot #N" instead of raw ID */
	const std::string prefix = "Shop_Purchase_";
	if (location_name.rfind(prefix, 0) == 0) {
		std::string num = location_name.substr(prefix.size());
		/* Strip leading zeros */
		size_t first_nonzero = num.find_first_not_of('0');
		if (first_nonzero != std::string::npos) num = num.substr(first_nonzero);
		return "Shop Slot #" + num;
	}
	return location_name;
}

int64_t AP_GetShopPrice(const std::string &location_name)
{
	const APSlotData &sd = AP_GetSlotData();
	auto it = sd.shop_prices.find(location_name);
	if (it != sd.shop_prices.end()) return it->second;
	return 50000LL; /* Fallback £50k if no price defined */
}

bool AP_CanAffordShopItem(const std::string &location_name)
{
	CompanyID cid = _local_company;
	const Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return false;
	return (int64_t)c->money >= AP_GetShopPrice(location_name);
}

bool AP_IsShopLocationSent(const std::string &location_name)
{
	return _ap_sent_shop_locations.count(location_name) > 0;
}

std::string AP_GetSentShopStr()
{
	std::string out;
	for (const std::string &loc : _ap_sent_shop_locations) {
		if (!out.empty()) out += ',';
		out += loc;
	}
	return out;
}

void AP_SetSentShopStr(const std::string &s)
{
	/* Stage for application after AP_InitSessionStats (which clears _ap_sent_shop_locations) */
	_ap_staging_shop_sent = s;
	_ap_staging_shop_sent_valid = !s.empty();
}

static void AP_ApplyStagedShopSent()
{
	if (!_ap_staging_shop_sent_valid) return;
	std::string token;
	for (char c : _ap_staging_shop_sent) {
		if (c == ',') { if (!token.empty()) _ap_sent_shop_locations.insert(token); token.clear(); }
		else token += c;
	}
	if (!token.empty()) _ap_sent_shop_locations.insert(token);
	Debug(misc, 1, "[AP] Restored {} shop locations from savegame", _ap_sent_shop_locations.size());
	_ap_staging_shop_sent.clear();
	_ap_staging_shop_sent_valid = false;
}

void AP_DeductShopPrice(const std::string &location_name)
{
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;
	int64_t price = AP_GetShopPrice(location_name);
	AP_ChangeMoney(cid, -(Money)price);
}

/* -------------------------------------------------------------------------
 * REAL-TIME TIMER — 250ms
 *
 * Responsibilities:
 *   1. Pump the AP network client (Tick)
 *   2. First-tick session setup when we enter GM_NORMAL:
 *        - Build engine name map
 *        - Unlock the starting vehicle via EnableEngineForCompany
 *        - Flush any items that arrived before GM_NORMAL
 *        - Open the AP status window
 *   3. Win-condition polling every ~5 s
 *
 * NOTE: Engine locking is no longer done here.  engine.cpp blocks the
 * vanilla date-based introduction loop when AP_IsActive() is true.
 * Engines are locked by default (company_avail is empty until an engine's
 * intro_date would have fired), and we unlock them on demand.
 * ---------------------------------------------------------------------- */

static uint32_t _ap_realtime_ticks = 0;

/* -------------------------------------------------------------------------
 * Shop page offset / day counter — kept for savegame compatibility only.
 * Shop rotation has been removed; the full shop list is always visible.
 * ---------------------------------------------------------------------- */

static int _ap_shop_day_counter  = 0;   ///< Unused — kept for savegame compat
static int _ap_shop_page_offset  = 0;   ///< Unused — kept for savegame compat

/* ── Savegame accessor functions (used by archipelago_sl.cpp) ────── */
int  AP_GetShopPageOffset()  { return _ap_shop_page_offset; }
void AP_SetShopPageOffset(int v) { _ap_shop_page_offset = v; }
int  AP_GetShopDayCounter()  { return _ap_shop_day_counter; }
void AP_SetShopDayCounter(int v) { _ap_shop_day_counter = v; }
bool AP_GetGoalSent()        { return _ap_goal_sent; }
void AP_SetGoalSent(bool v)  { _ap_goal_sent = v; }
int64_t AP_GetItemsReceivedCount()         { return _ap_items_received_count; }

int AP_GetCheckedLocationCount()
{
	return (int)_ap_server_checked_locations.size();
}

int AP_GetTotalLocationCount()
{
	if (_ap_client == nullptr) return 0;
	return _ap_client->GetLocationCount();
}
void    AP_SetItemsReceivedCount(int64_t v){ _ap_staging_items_received = v; _ap_staging_stats = true; }

/* ── Day/Night toggle ─────────────────────────────────────── */
static bool _ap_daynight_disabled = false;

void AP_ToggleDayNight()
{
	_ap_daynight_disabled = !_ap_daynight_disabled;
}

bool AP_IsDayNightDisabled()
{
	return _ap_daynight_disabled;
}

void AP_GetEffectTimers(int *fuel, int *cargo, int *reliability, int *station, int *license_ticks, int *license_type, int *breakdown)
{
	*fuel         = _ap_fuel_shortage_ticks;
	*cargo        = _ap_cargo_bonus_ticks;
	*reliability  = _ap_reliability_boost_ticks;
	*station      = _ap_station_boost_ticks;
	*license_ticks = _ap_license_revoke_ticks;
	*license_type  = _ap_license_revoke_type;
	*breakdown     = _ap_breakdown_wave_ticks;
}

void AP_SetEffectTimers(int fuel, int cargo, int reliability, int station, int license_ticks, int license_type, int breakdown)
{
	_ap_fuel_shortage_ticks      = fuel;
	_ap_cargo_bonus_ticks        = cargo;
	_ap_reliability_boost_ticks  = reliability;
	_ap_station_boost_ticks      = station;
	_ap_license_revoke_ticks     = license_ticks;
	_ap_license_revoke_type      = license_type;
	_ap_breakdown_wave_ticks     = breakdown;
}

std::string AP_GetCompletedMissionsStr()
{
    std::string out;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (!m.completed) continue;
        if (!out.empty()) out += ',';
        out += m.location;
    }
    return out;
}

void AP_SetCompletedMissionsStr(const std::string &s)
{
    /* Stage for application after AP_OnSlotData overwrites _ap_pending_sd.
     * During savegame load, _ap_pending_sd is stale (previous session).
     * The staged string is applied in first-tick setup when the fresh
     * mission list from the server is already in place. */
    _ap_staging_completed_missions = s;
    _ap_staging_completed_valid    = !s.empty();
}

static void AP_ApplyStagedCompletedMissions()
{
    if (!_ap_staging_completed_valid) return;
    const std::string &s = _ap_staging_completed_missions;

    /* Split by comma and mark matching missions as completed */
    std::set<std::string> done;
    std::string token;
    for (char c : s) {
        if (c == ',') { if (!token.empty()) done.insert(token); token.clear(); }
        else token += c;
    }
    if (!token.empty()) done.insert(token);
    for (APMission &m : _ap_pending_sd.missions) {
        if (done.count(m.location)) m.completed = true;
    }

    /* Rebuild per-difficulty counters from the now-updated mission structs. */
    _ap_easy_completed    = 0;
    _ap_medium_completed  = 0;
    _ap_hard_completed    = 0;
    _ap_extreme_completed = 0;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (!m.completed) continue;
        if      (m.location.rfind("Mission_Easy_",    0) == 0) _ap_easy_completed++;
        else if (m.location.rfind("Mission_Medium_",  0) == 0) _ap_medium_completed++;
        else if (m.location.rfind("Mission_Hard_",    0) == 0) _ap_hard_completed++;
        else if (m.location.rfind("Mission_Extreme_", 0) == 0) _ap_extreme_completed++;
    }

    Debug(misc, 1, "[AP] Restored {} completed missions from savegame", done.size());
    _ap_staging_completed_missions.clear();
    _ap_staging_completed_valid = false;
}

void AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out)
{
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        cargo_out[i] = _ap_cumul_cargo[i];
    *profit_out = (int64_t)_ap_cumul_profit;
}

void AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in)
{
    /* Stage for application after AP_InitSessionStats (which zeroes everything) */
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        _ap_staging_cargo[i] = cargo_in[i];
    _ap_staging_profit = profit_in;
    _ap_staging_stats  = true;
}

/* Returns "location=N:P,..." for all maintain missions.
 * N = consecutive months OK, P = 1 if first-month guard is pending, else 0. */
std::string AP_GetMaintainCountersStr()
{
    std::string out;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (m.type.find("maintain") == std::string::npos) continue;
        if (m.maintain_months_ok == 0 && !m.maintain_first_month_pending) continue;
        if (!out.empty()) out += ',';
        out += m.location + '=' + fmt::format("{}:{}", m.maintain_months_ok,
               m.maintain_first_month_pending ? 1 : 0);
    }
    return out;
}

void AP_SetMaintainCountersStr(const std::string &s)
{
    /* Stage for application after AP_OnSlotData — same timing issue as missions. */
    _ap_staging_maintain_counters = s;
    _ap_staging_maintain_valid    = !s.empty();
}

static void AP_ApplyStagedMaintainCounters()
{
    if (!_ap_staging_maintain_valid) return;
    const std::string &s = _ap_staging_maintain_counters;

    /* Parse "loc=N:P,..." — P is optional for backwards compat with old saves */
    std::string token;
    int restored = 0;
    auto apply = [&](const std::string &t) {
        auto eq = t.find('=');
        if (eq == std::string::npos) return;
        std::string loc = t.substr(0, eq);
        std::string val = t.substr(eq + 1);
        int n = 0;
        bool pending = false;
        auto colon = val.find(':');
        auto parse_int = [](const std::string &str) {
            int v = 0;
            for (char c : str) if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
            return v;
        };
        if (colon != std::string::npos) {
            n       = parse_int(val.substr(0, colon));
            pending = (parse_int(val.substr(colon + 1)) != 0);
        } else {
            n = parse_int(val);
        }
        for (APMission &m : _ap_pending_sd.missions) {
            if (m.location == loc) {
                m.maintain_months_ok           = n;
                m.maintain_first_month_pending = pending;
                restored++;
                break;
            }
        }
    };
    for (char c : s) {
        if (c == ',') { apply(token); token.clear(); }
        else token += c;
    }
    apply(token);

    Debug(misc, 1, "[AP] Restored {} maintain counters from savegame", restored);
    _ap_staging_maintain_counters.clear();
    _ap_staging_maintain_valid = false;
}
std::string AP_GetNamedEntityStr()
{
	std::string out;
	for (const APMission &m : _ap_pending_sd.missions) {
		if (m.named_entity.id < 0) continue;
		if (!out.empty()) out += ';';
		out += m.location + ':' +
		       fmt::format("{}", m.named_entity.id) + ':' +
		       fmt::format("{}", m.named_entity.cumulative);
	}
	return out;
}

/** Restore named entity assignments from save/load string.
 *  Format: "location:entity_id:cumulative;..." (semicolon-separated)
 *  Uses std::from_chars — no sscanf/strchr (both forbidden by safeguards.h). */
/* Forward declarations for helpers defined later in this file */
static std::string AP_TownName(const Town *t);
static std::string AP_IndustryLabel(const Industry *ind);
static void AP_StrReplace(std::string &s, const std::string &from, const std::string &to);

void AP_SetNamedEntityStr(const std::string &s)
{
	/* Stage for application after AP_OnSlotData overwrites _ap_pending_sd
	 * and after AP_AssignNamedEntities() has set up the entity IDs.
	 * The staged string is applied in first-tick setup. */
	_ap_staging_named_entities = s;
	_ap_staging_named_valid    = !s.empty();
}

/** Apply staged named entity cumulative progress.
 *  Must run AFTER AP_AssignNamedEntities() so entity IDs are already set. */
static void AP_ApplyStagedNamedEntities()
{
	if (!_ap_staging_named_valid) return;
	const std::string &s = _ap_staging_named_entities;

	std::string_view sv(s);
	while (!sv.empty()) {
		/* Find the ';' that ends this entry (or end-of-string) */
		auto semi = sv.find(';');
		std::string_view entry = sv.substr(0, semi);
		if (semi == std::string_view::npos) sv = {}; else sv = sv.substr(semi + 1);

		/* Split entry into "loc : eid : cum" */
		auto c1 = entry.find(':');
		if (c1 == std::string_view::npos) continue;
		auto c2 = entry.find(':', c1 + 1);
		if (c2 == std::string_view::npos) continue;

		std::string_view loc_sv  = entry.substr(0, c1);
		std::string_view eid_sv  = entry.substr(c1 + 1, c2 - c1 - 1);
		std::string_view cum_sv  = entry.substr(c2 + 1);

		int32_t  eid = -1;
		uint64_t cum = 0;
		std::from_chars(eid_sv.data(), eid_sv.data() + eid_sv.size(), eid);
		std::from_chars(cum_sv.data(), cum_sv.data() + cum_sv.size(), cum);

		std::string loc_str(loc_sv);
		for (APMission &m : _ap_pending_sd.missions) {
			if (m.location != loc_str) continue;
			/* Only restore cumulative — entity ID is set by AP_AssignNamedEntities() */
			m.named_entity.cumulative = cum;
			break;
		}
	}

	Debug(misc, 1, "[AP] Restored named entity cumulative progress from savegame");
	_ap_named_entity_refresh_needed = true;

	/* Clear staging */
	_ap_staging_named_entities.clear();
	_ap_staging_named_valid = false;
}
/* -------------------------------------------------------------------------
 * Named-destination missions: assign map entities and accumulate progress.
 * Placed after all AP state variables so _ap_pending_sd is accessible.
 * ---------------------------------------------------------------------- */

/** Get town name using OTTDv15 variadic GetString API. */
static std::string AP_TownName(const Town *t)
{
	if (t == nullptr) return "Unknown";
	return GetString(STR_TOWN_NAME, t->index);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Ruin Gameplay System — implementation
 * Placed here (after _ap_pending_sd, AP_ShowNews, AP_TownName) so all
 * dependencies are available.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Cargo types available per landscape (including passengers, mail, and goods). */
static const std::vector<std::string> _ap_ruin_cargo_temperate = {
	"passengers", "mail", "coal", "oil", "livestock", "goods", "grain", "wood", "iron ore", "steel", "valuables"
};
static const std::vector<std::string> _ap_ruin_cargo_arctic = {
	"passengers", "mail", "coal", "oil", "livestock", "goods", "wheat", "wood", "paper", "food", "gold"
};
static const std::vector<std::string> _ap_ruin_cargo_tropical = {
	"passengers", "mail", "oil", "goods", "maize", "wood", "rubber", "fruit", "copper ore", "water", "food", "diamonds"
};
static const std::vector<std::string> _ap_ruin_cargo_toyland = {
	"passengers", "mail", "sugar", "toys", "batteries", "sweets", "cola", "candyfloss", "bubbles", "plastic", "fizzy drinks", "toffee"
};

/** Build a cargo list dynamically from what's actually available in-game.
 *  This automatically supports FIRS and any other industry NewGRF — we simply
 *  enumerate all cargo types that exist and have a label. Falls back to the
 *  hardcoded vanilla lists only if no cargos are found (shouldn't happen). */
static std::vector<std::string> AP_RuinCargoListDynamic()
{
	std::vector<std::string> result;
	/* Use the cargo map built during AP session start — it maps English
	 * cargo names (lowercase) to CargoType indices. Same names the AP
	 * server uses, guaranteed to match what's actually loaded. */
	if (!_ap_cargo_map_built) BuildCargoMap();
	for (const auto &[cargo_name, _ct] : _ap_cargo_map) {
		result.push_back(cargo_name);
	}
	if (!result.empty()) return result;

	/* Fallback: static vanilla lists */
	switch (_ap_pending_sd.landscape) {
		case 1:  return std::vector<std::string>(_ap_ruin_cargo_arctic.begin(), _ap_ruin_cargo_arctic.end());
		case 2:  return std::vector<std::string>(_ap_ruin_cargo_tropical.begin(), _ap_ruin_cargo_tropical.end());
		case 3:  return std::vector<std::string>(_ap_ruin_cargo_toyland.begin(), _ap_ruin_cargo_toyland.end());
		default: return std::vector<std::string>(_ap_ruin_cargo_temperate.begin(), _ap_ruin_cargo_temperate.end());
	}
}

/** Difficulty-scaled cargo amount for a ruin requirement. */
static int64_t AP_RuinCargoAmount()
{
	int diff = (int)_ap_pending_sd.win_difficulty;
	struct Range { int64_t lo; int64_t hi; };
	static const Range ranges[] = {
		{  1000,   4000 }, // 0: casual
		{  2000,   6000 }, // 1: easy
		{  3000,   8000 }, // 2: normal
		{  5000,  12000 }, // 3: medium
		{  8000,  20000 }, // 4: hard
		{ 12000,  30000 }, // 5: very hard
		{ 18000,  45000 }, // 6: extreme
		{ 25000,  60000 }, // 7: insane
		{ 35000,  90000 }, // 8: nutcase
		{ 50000, 150000 }, // 9: madness
	};
	int idx = std::clamp(diff, 0, 9);
	int64_t lo = ranges[idx].lo;
	int64_t hi = ranges[idx].hi;
	return lo + (int64_t)(InteractiveRandom() % (uint32_t)(hi - lo + 1));
}

/** Spawn a new ruin on the map with random cargo requirements. */
static bool AP_SpawnRuin()
{
	if (_ap_pending_sd.ruin_pool_size <= 0) return false;
	if (_ap_ruins_spawned >= _ap_pending_sd.ruin_pool_size) return false;
	if ((int)_ap_active_ruins.size() >= _ap_pending_sd.max_active_ruins) return false;
	if (_ap_ruins_spawned >= (int)_ap_pending_sd.ruin_locations.size()) return false;

	AP_ResolveRuinTypes();

	int variant = InteractiveRandom() % AP_NUM_RUIN_TYPES;
	ObjectType ot = _ap_ruin_types[variant];
	if (ot == INVALID_OBJECT_TYPE) {
		for (int i = 0; i < AP_NUM_RUIN_TYPES; i++) {
			if (_ap_ruin_types[i] != INVALID_OBJECT_TYPE) { ot = _ap_ruin_types[i]; variant = i; break; }
		}
		if (ot == INVALID_OBJECT_TYPE) {
			AP_WARN("Cannot spawn ruin — no ruin ObjectTypes resolved (GRF missing?)");
			return false;
		}
	}

	/* Spawn a 3×3 cluster of ruin objects so they're visible on the map.
	 * We try to place the centre tile first, then fill the 3×3 around it. */
	static constexpr int RUIN_RADIUS = 1; /* 1 = 3×3 cluster */
	TileIndex placed_tile = INVALID_TILE;
	for (int attempt = 0; attempt < 5000; attempt++) {
		TileIndex tile = RandomTile();
		if (!IsValidTile(tile)) continue;
		uint x = TileX(tile), y = TileY(tile);
		if (x < 6 || y < 6 || x >= Map::SizeX() - 6 || y >= Map::SizeY() - 6) continue;

		bool too_close = false;
		for (const auto &r : _ap_active_ruins) {
			if (r.tile != UINT32_MAX && DistanceManhattan(tile, (TileIndex)r.tile) < 20) {
				too_close = true; break;
			}
		}
		if (too_close) continue;

		/* OBJ_FLAG_ONLY_INGAME requires _current_company <= MAX_COMPANIES. */
		CompanyID old_company = _current_company;
		_current_company = _local_company;
		CommandCost res = Command<CMD_BUILD_OBJECT>::Do(
			DoCommandFlags{DoCommandFlag::Execute}, tile, ot, 0);
		_current_company = old_company;

		if (res.Succeeded()) {
			placed_tile = tile;
			/* Fill the surrounding ring with ruin objects (best-effort) */
			for (int dx = -RUIN_RADIUS; dx <= RUIN_RADIUS; dx++) {
				for (int dy = -RUIN_RADIUS; dy <= RUIN_RADIUS; dy++) {
					if (dx == 0 && dy == 0) continue; /* centre already placed */
					TileIndex t2 = TileXY(x + dx, y + dy);
					if (!IsValidTile(t2)) continue;
					ObjectType ot2 = _ap_ruin_types[InteractiveRandom() % AP_NUM_RUIN_TYPES];
					if (ot2 == INVALID_OBJECT_TYPE) ot2 = ot;
					_current_company = _local_company;
					Command<CMD_BUILD_OBJECT>::Do(
						DoCommandFlags{DoCommandFlag::Execute}, t2, ot2, 0);
					_current_company = old_company;
				}
			}
			break;
		}
	}

	if (placed_tile == INVALID_TILE) {
		AP_WARN("Failed to spawn ruin after 5000 attempts");
		return false;
	}

	Town *town = ClosestTownFromTile(placed_tile, UINT_MAX);
	if (town == nullptr) { AP_WARN("No town found near ruin tile"); return false; }

	APRuin ruin;
	ruin.id = _ap_ruins_spawned;
	ruin.location_name = _ap_pending_sd.ruin_locations[_ap_ruins_spawned];
	ruin.tile = placed_tile.base();
	ruin.object_variant = variant;
	ruin.town_id = town->index.base();
	ruin.town_name = AP_TownName(town);

	const auto cargo_list = AP_RuinCargoListDynamic();
	int cargo_min = std::max(1, _ap_pending_sd.ruin_cargo_min);
	int cargo_max = std::max(cargo_min, _ap_pending_sd.ruin_cargo_max);
	int num_reqs = cargo_min + (int)(InteractiveRandom() % (uint32_t)(cargo_max - cargo_min + 1));
	if (num_reqs > (int)cargo_list.size()) num_reqs = (int)cargo_list.size();

	std::vector<int> indices(cargo_list.size());
	for (int i = 0; i < (int)indices.size(); i++) indices[i] = i;
	for (int i = (int)indices.size() - 1; i > 0; i--) {
		int j = InteractiveRandom() % (uint32_t)(i + 1);
		std::swap(indices[i], indices[j]);
	}

	/* Soft-lock prevention: guarantee at least one "basic" cargo (passengers
	 * or mail) so the player can always make partial progress with starter
	 * vehicles even before unlocking specialised cargo transport. */
	{
		int basic_idx = -1; /* index in cargo_list for passengers or mail */
		bool found_basic = false;
		for (int i = 0; i < num_reqs; i++) {
			const std::string &name = cargo_list[indices[i]];
			if (name == "passengers" || name == "mail") { found_basic = true; break; }
		}
		if (!found_basic) {
			/* Find a basic cargo in the un-selected tail and swap it in */
			for (int i = num_reqs; i < (int)indices.size(); i++) {
				const std::string &name = cargo_list[indices[i]];
				if (name == "passengers" || name == "mail") { basic_idx = i; break; }
			}
			if (basic_idx >= 0) {
				/* Swap it with a random non-first slot in the selected range */
				int swap_pos = (num_reqs > 1) ? (int)(InteractiveRandom() % (uint32_t)num_reqs) : 0;
				std::swap(indices[swap_pos], indices[basic_idx]);
			}
		}
	}

	for (int i = 0; i < num_reqs; i++) {
		APRuinCargoReq req;
		const std::string &cargo_name = cargo_list[indices[i]];
		req.cargo = (uint8_t)AP_FindCargoType(cargo_name);
		req.cargo_name = cargo_name;
		req.required = AP_RuinCargoAmount();
		req.delivered = 0;
		if (IsValidCargoType((CargoType)req.cargo)) ruin.cargo_reqs.push_back(req);
	}

	if (ruin.cargo_reqs.empty()) { AP_WARN("No valid cargo types for ruin"); return false; }

	_ap_active_ruins.push_back(ruin);
	_ap_ruins_spawned++;
	_ap_ruin_cooldown_months = AP_RUIN_SPAWN_COOLDOWN;

	std::string cargo_text;
	for (size_t i = 0; i < ruin.cargo_reqs.size(); i++) {
		if (i > 0) cargo_text += (i == ruin.cargo_reqs.size() - 1) ? " and " : ", ";
		cargo_text += fmt::format("{} {}", AP_Num(ruin.cargo_reqs[i].required), ruin.cargo_reqs[i].cargo_name);
	}
	AP_ShowNews(fmt::format("The God of Wacken has cursed the land near {}! Deliver {} to lift the curse.",
	            ruin.town_name, cargo_text));
	AP_OK(fmt::format("Ruin {} spawned at tile {} near {} ({})", ruin.location_name, placed_tile,
	      ruin.town_name, cargo_text));
	AP_TRACE(fmt::format("RuinSpawned: loc='{}' tile={} town='{}' cargo=[{}] spawned_total={} active={}",
		ruin.location_name, placed_tile, ruin.town_name, cargo_text, _ap_ruins_spawned, _ap_active_ruins.size()));
	return true;
}

/** Check ruin cargo delivery progress.  Called from AP_UpdateNamedMissions(). */
static void AP_UpdateRuinProgress(CompanyID cid, std::set<CargoMonitorID> &drained)
{
	for (auto it = _ap_active_ruins.begin(); it != _ap_active_ruins.end(); ) {
		APRuin &ruin = *it;
		if (ruin.completed) { ++it; continue; }

		for (auto &req : ruin.cargo_reqs) {
			if (req.delivered >= req.required) continue;
			if (!IsValidCargoType((CargoType)req.cargo)) continue;

			TownID tid = (TownID)ruin.town_id;
			if (!Town::IsValidID(tid)) continue;

			CargoMonitorID monitor = EncodeCargoTownMonitor(cid, (CargoType)req.cargo, tid);
			int32_t delivered = 0;
			if (!drained.count(monitor)) {
				delivered = GetDeliveryAmount(monitor, true);
				drained.insert(monitor);
			}
			if (delivered > 0) req.delivered += (int64_t)delivered;
		}

		bool all_met = true;
		for (const auto &req : ruin.cargo_reqs) {
			if (req.delivered < req.required) { all_met = false; break; }
		}

		if (all_met) {
			ruin.completed = true;
			AP_SendCheckByName(ruin.location_name);

			/* Clear the 3×3 cluster of ruin objects around the centre tile */
			TileIndex centre = (TileIndex)ruin.tile;
			uint cx = TileX(centre), cy = TileY(centre);
			CompanyID old_company = _current_company;
			_current_company = _local_company;
			for (int dx = -1; dx <= 1; dx++) {
				for (int dy = -1; dy <= 1; dy++) {
					TileIndex t = TileXY(cx + dx, cy + dy);
					if (!IsValidTile(t)) continue;
					if (IsTileType(t, MP_OBJECT)) {
						Command<CMD_LANDSCAPE_CLEAR>::Do(
							DoCommandFlags{DoCommandFlag::Execute}, t);
					}
				}
			}
			_current_company = old_company;

			AP_ShowNews(fmt::format("Ruins near {} cleared! The God of Wacken retreats.", ruin.town_name));
			AP_OK(fmt::format("Ruin {} completed and removed", ruin.location_name));
			AP_TRACE(fmt::format("RuinCompleted: loc='{}' town='{}' completed={}/{} active_remaining={}",
				ruin.location_name, ruin.town_name, _ap_ruins_completed + 1, _ap_pending_sd.ruin_pool_size, _ap_active_ruins.size() - 1));
			_ap_ruins_completed++;
			it = _ap_active_ruins.erase(it);

			if (_ap_ruins_completed >= _ap_pending_sd.ruin_pool_size) {
				AP_ShowNews("All ruins have been banished! The land is free from the God of Wacken's curse.");
			}
		} else {
			++it;
		}
	}
}

/** Monthly tick for ruin spawn control. */
static void AP_RuinMonthlyTick()
{
	if (_ap_pending_sd.ruin_pool_size <= 0) return;
	if (!_ap_session_started) return;

	if (!_ap_ruins_initial_spawned) {
		_ap_ruins_initial_spawned = true;
		int initial = std::min(_ap_pending_sd.max_active_ruins, _ap_pending_sd.ruin_pool_size - _ap_ruins_spawned);
		for (int i = 0; i < initial; i++) AP_SpawnRuin();
		return;
	}

	if (_ap_ruin_cooldown_months > 0) { _ap_ruin_cooldown_months--; return; }

	if ((int)_ap_active_ruins.size() < _ap_pending_sd.max_active_ruins &&
	    _ap_ruins_spawned < _ap_pending_sd.ruin_pool_size) {
		AP_SpawnRuin();
	}
}

/** Monthly tick for star spawn control.
 *  First tick: initialise star entries.
 *  Every tick after: place up to 10 unplaced stars per month.
 *  This spreads placement over time, avoiding a big lag spike. */
static void AP_StarMonthlyTick()
{
	if (!_ap_pending_sd.enable_stars || _ap_pending_sd.star_pool_size <= 0) return;
	if (!_ap_session_started) return;

	/* First tick: create all star entries (no map placement yet) */
	if (!_ap_stars_initial_spawned) {
		_ap_stars_initial_spawned = true;
		if (_ap_stars.empty()) {
			AP_InitStarEntries();
		}
		/* Place first batch immediately */
		AP_PlaceStarBatch(10);
		return;
	}

	/* Subsequent ticks: place 10 more unplaced stars per month */
	bool any_unplaced = std::any_of(_ap_stars.begin(), _ap_stars.end(),
		[](const APStar &s){ return s.tile == UINT32_MAX && !s.collected; });
	if (any_unplaced) {
		AP_PlaceStarBatch(10);
	}
}

/* ═══════════════════════════════════════════════════════════════════════ */

/** Get "IndustryType near TownName" label. */
static std::string AP_IndustryLabel(const Industry *ind)
{
	if (ind == nullptr) return "Unknown Industry";
	std::string ind_name  = GetString(STR_INDUSTRY_NAME, ind->index);
	std::string town_name = GetString(STR_TOWN_NAME,     ind->town->index);
	return ind_name + " near " + town_name;
}

/**
 * Re-resolve name/tile/cargo for all named-entity missions that have a valid
 * eid but an empty name.  Called on first game tick after a savegame load,
 * when AfterLoadGame() has fully resolved all pool pointers (ind->town etc.).
 */
static void AP_RefreshNamedEntityNames()
{
	for (APMission &m : _ap_pending_sd.missions) {
		int32_t eid = m.named_entity.id;
		if (eid < 0) continue; /* not yet assigned */
		if (!m.named_entity.name.empty()) continue; /* already resolved */

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			const Town *t = Town::GetIfValid((TownID)eid);
			if (t != nullptr) {
				m.named_entity.name       = AP_TownName(t);
				m.named_entity.tile       = t->xy.base();
				m.named_entity.tae        = (m.type == "passengers_to_town") ? TAE_PASSENGERS : TAE_MAIL;
				m.named_entity.cargo_type = (uint8_t)((m.type == "passengers_to_town")
				    ? AP_FindCargoType("passengers")
				    : AP_FindCargoType("mail"));
				AP_StrReplace(m.description, "[Town]", m.named_entity.name);
			}
		} else if (m.type == "cargo_from_industry") {
			const Industry *ind = Industry::GetIfValid((IndustryID)eid);
			if (ind != nullptr && ind->town != nullptr) {
				m.named_entity.name       = AP_IndustryLabel(ind);
				m.named_entity.tile       = ind->location.tile.base();
				uint8_t first_ct = 0xFF;
				for (const auto &slot : ind->produced) {
					if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; }
				}
				m.named_entity.cargo_type = first_ct;
				AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
			}
		} else if (m.type == "cargo_to_industry") {
			const Industry *ind = Industry::GetIfValid((IndustryID)eid);
			if (ind != nullptr && ind->town != nullptr) {
				m.named_entity.name       = AP_IndustryLabel(ind);
				m.named_entity.tile       = ind->location.tile.base();
				uint8_t first_ct = 0xFF;
				for (const auto &slot : ind->accepted) {
					if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; }
				}
				m.named_entity.cargo_type = first_ct;
				AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
			}
		}
	}
	_ap_named_entity_refresh_needed = false;
}

/** Replace first occurrence of 'from' in 's' with 'to'. */
static void AP_StrReplace(std::string &s, const std::string &from, const std::string &to)
{
	size_t pos = s.find(from);
	if (pos != std::string::npos) s.replace(pos, from.size(), to);
}

/**
 * At session start: assign real map towns/industries to named-destination
 * missions (type "passengers_to_town", "mail_to_town", "cargo_to_industry",
 * "cargo_from_industry").  Assignments are seed-deterministic.
 */
static void AP_AssignNamedEntities()
{
	/* Collect candidates */
	std::vector<const Town     *> towns;
	std::vector<const Industry *> prod_inds;
	std::vector<const Industry *> acc_inds;
	for (const Town     *t   : Town::Iterate())     towns.push_back(t);
	for (const Industry *ind : Industry::Iterate()) {
		if (!ind->produced.empty()) prod_inds.push_back(ind);
		if (!ind->accepted.empty()) acc_inds.push_back(ind);
	}
	if (towns.empty()) return;

	/* XOR-shift RNG seeded from world seed */
	/* Use the actual map generation seed for deterministic town/industry
	 * assignment.  _ap_pending_sd.world_seed is 0 (Python sends 0 and lets
	 * OpenTTD pick its own seed), so we fall back to the real game seed. */
	const uint32_t map_seed = (_ap_pending_sd.world_seed != 0)
		? _ap_pending_sd.world_seed
		: _settings_game.game_creation.generation_seed;
	uint32_t rng = map_seed ^ 0xDEADBEEFu;
	auto next_rng = [&]() -> uint32_t {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
	};
	auto shuffle = [&](auto &v) {
		for (size_t i = v.size(); i > 1; --i) std::swap(v[i-1], v[next_rng() % i]);
	};
	shuffle(towns); shuffle(prod_inds); shuffle(acc_inds);

	std::set<int32_t> used_towns, used_inds;
	size_t ti = 0, pi = 0, ai = 0;

	for (APMission &m : _ap_pending_sd.missions) {
		if (m.named_entity.id >= 0 && !m.named_entity.name.empty()) continue; /* already fully resolved */

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			while (ti < towns.size() && used_towns.count((int32_t)towns[ti]->index.base())) ti++;
			if (ti >= towns.size()) { ti = 0; used_towns.clear(); /* allow reuse on wrap */ }
			const Town *t           = towns[ti++];
			m.named_entity.id       = (int32_t)t->index.base();
			m.named_entity.name     = AP_TownName(t);
			m.named_entity.tile     = t->xy.base();
			m.named_entity.tae      = (m.type == "passengers_to_town") ? TAE_PASSENGERS : TAE_MAIL;
			m.named_entity.cargo_type = (uint8_t)((m.type == "passengers_to_town")
			    ? AP_FindCargoType("passengers")
			    : AP_FindCargoType("mail"));
			used_towns.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Town]", m.named_entity.name);

		} else if (m.type == "cargo_from_industry") {
			while (pi < prod_inds.size() && used_inds.count((int32_t)prod_inds[pi]->index.base())) pi++;
			if (pi >= prod_inds.size()) { pi = 0; used_inds.clear(); }
			const Industry *ind        = prod_inds[pi++];
			m.named_entity.id          = (int32_t)ind->index.base();
			m.named_entity.name        = AP_IndustryLabel(ind);
			m.named_entity.tile        = ind->location.tile.base();
			m.named_entity.cargo_slot  = 0;
			/* First VALID produced slot — slot[0] may be INVALID_CARGO */
			{ uint8_t first_ct = 0xFF;
			  for (const auto &slot : ind->produced) { if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; } }
			  m.named_entity.cargo_type = first_ct; }
			used_inds.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);

		} else if (m.type == "cargo_to_industry") {
			while (ai < acc_inds.size() && used_inds.count((int32_t)acc_inds[ai]->index.base())) ai++;
			if (ai >= acc_inds.size()) { ai = 0; /* used_inds already cleared above or allow reuse */ }
			const Industry *ind        = acc_inds[ai++];
			m.named_entity.id          = (int32_t)ind->index.base();
			m.named_entity.name        = AP_IndustryLabel(ind);
			m.named_entity.tile        = ind->location.tile.base();
			m.named_entity.cargo_slot  = 0;
			/* First VALID accepted slot — slot[0] may be INVALID_CARGO */
			{ uint8_t first_ct = 0xFF;
			  for (const auto &slot : ind->accepted) { if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; } }
			  m.named_entity.cargo_type = first_ct; }
			used_inds.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
		}
	}
}

/**
 * Called monthly: accumulate named-entity progress and protect industries
 * from random closure while their mission is active.
 */
/**
 * Called every 250 ms: accumulate named-entity progress for missions AND tasks,
 * and protect mission industries from random closure.
 *
 * Root-cause fix for task progress bug:
 *   Missions and tasks share the same CargoMonitorID key
 *   (company + cargo + town/industry).  GetDeliveryAmount() resets the
 *   counter to 0 on every call.  AP_UpdateNamedMissions() runs every 250 ms
 *   and drained mission monitors before the monthly AP_UpdateTasks() could
 *   read them — tasks always saw 0.
 *   Fix: accumulate BOTH mission and task progress in one pass so each
 *   monitor is only drained once per tick.
 */
static void AP_UpdateNamedMissions()
{
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return;

	/* Track monitors already drained this tick so tasks don't double-drain. */
	std::set<CargoMonitorID> drained;

	/* ── Mission progress ─────────────────────────────────────────────── */
	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed)           continue;
		if (m.named_entity.id < 0) continue;
		if (m.named_entity.cargo_type == 0xFF) continue;

		CargoType ct = (CargoType)m.named_entity.cargo_type;

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			TownID tid = (TownID)m.named_entity.id;
			if (!Town::IsValidID(tid)) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			CargoMonitorID monitor = EncodeCargoTownMonitor(cid, ct, tid);
			int32_t delivered = GetDeliveryAmount(monitor, true);
			drained.insert(monitor);
			if (delivered > 0) {
				m.named_entity.cumulative += (uint64_t)delivered;
				/* Credit any tasks sharing this exact monitor (same town + cargo). */
				for (APTask &t : _ap_tasks) {
					if (t.completed || t.expired || !t.monitor_seeded) continue;
					if (t.type == "passengers_to_town" && t.entity_id == m.named_entity.id)
						t.current_value += delivered;
				}
			}

		} else if (m.type == "cargo_from_industry") {
			IndustryID iid = (IndustryID)m.named_entity.id;
			Industry *ind = Industry::GetIfValid(iid);
			if (ind == nullptr) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			if (ind->prod_level < PRODLEVEL_DEFAULT) ind->prod_level = PRODLEVEL_DEFAULT;
			for (const auto &slot : ind->produced) {
				if (!IsValidCargoType(slot.cargo)) continue;
				CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, slot.cargo, iid);
				int32_t picked_up = GetPickupAmount(monitor, true);
				drained.insert(monitor);
				if (picked_up > 0) {
					m.named_entity.cumulative += (uint64_t)picked_up;
					for (APTask &t : _ap_tasks) {
						if (t.completed || t.expired || !t.monitor_seeded) continue;
						if (t.type == "cargo_from_industry" && t.entity_id == m.named_entity.id)
							t.current_value += picked_up;
					}
				}
			}

		} else if (m.type == "cargo_to_industry") {
			IndustryID iid = (IndustryID)m.named_entity.id;
			Industry *ind = Industry::GetIfValid(iid);
			if (ind == nullptr) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			if (ind->prod_level < PRODLEVEL_DEFAULT) ind->prod_level = PRODLEVEL_DEFAULT;
			for (const auto &slot : ind->accepted) {
				if (!IsValidCargoType(slot.cargo)) continue;
				CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, slot.cargo, iid);
				int32_t delivered = GetDeliveryAmount(monitor, true);
				drained.insert(monitor);
				if (delivered > 0) m.named_entity.cumulative += (uint64_t)delivered;
			}
		}
	}

	/* ── Task progress (monitors NOT already drained by a mission above) ── */
	CargoType pass_ct = AP_FindCargoType("passengers");
	bool any_task_updated = false;
	for (APTask &t : _ap_tasks) {
		if (t.completed || t.expired || !t.monitor_seeded) continue;

		if (t.type == "passengers_to_town") {
			if (!IsValidCargoType(pass_ct)) continue;
			TownID tid = (TownID)t.entity_id;
			if (!Town::IsValidID(tid)) continue;
			CargoMonitorID mon = EncodeCargoTownMonitor(cid, pass_ct, tid);
			if (drained.count(mon)) continue; /* already credited above */
			int32_t delivered = GetDeliveryAmount(mon, true);
			if (delivered > 0) {
				t.current_value += delivered;
				any_task_updated = true;
				AP_OK(fmt::format("[Task] passengers_to_town id={}: +{} -> {}/{}", t.entity_id, AP_Num(delivered), AP_Num(t.current_value), AP_Num(t.amount)));
			}

		} else if (t.type == "cargo_from_industry") {
			if (!IsValidCargoType((CargoType)t.cargo)) continue;
			IndustryID iid = (IndustryID)t.entity_id;
			Industry *ind = Industry::GetIfValid(iid);
			if (!ind) continue;
			for (const auto &slot : ind->produced) {
				if (!IsValidCargoType(slot.cargo)) continue;
				CargoMonitorID mon = EncodeCargoIndustryMonitor(cid, slot.cargo, iid);
				if (drained.count(mon)) continue;
				int32_t picked_up = GetPickupAmount(mon, true);
				if (picked_up > 0) {
					t.current_value += picked_up;
					any_task_updated = true;
					AP_OK(fmt::format("[Task] cargo_from_industry id={} cargo={}: +{} -> {}/{}", t.entity_id, (int)slot.cargo, AP_Num(picked_up), AP_Num(t.current_value), AP_Num(t.amount)));
				}
			}
		}
	}

	/* Check task completion here (250 ms cadence) so rewards fire promptly.
	 * AP_UpdateTasks (monthly) also checks, but that's too slow to be responsive. */
	for (APTask &t : _ap_tasks) {
		if (t.completed || t.expired) continue;
		if (t.current_value >= t.amount) AP_CompleteTask(t);
	}

	/* ── Ruin cargo delivery progress ──────────────────────────────── */
	AP_UpdateRuinProgress(cid, drained);
}


/* -------------------------------------------------------------------------
 * Monthly timer: advance "maintain rating" mission counters.
 * For each incomplete maintain-mission, check if ALL rated stations owned by
 * the player currently meet the threshold. If yes, increment the counter;
 * if any station falls below, reset it to zero.
 * ---------------------------------------------------------------------- */
static const IntervalTimer<TimerGameCalendar> _ap_calendar_maintain_check(
	{ TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE },
	[](auto) {
		if (!_ap_session_started) return;

		CompanyID cid = _local_company;

		for (APMission &m : _ap_pending_sd.missions) {
			if (m.completed) continue;
			/* Accept both normalized type (maintain_75) and legacy raw strings */
			bool is_maintain = (m.type == "maintain_75" ||
			                    m.type.find("maintain") != std::string::npos);
			if (!is_maintain) continue;

			/* Threshold: 191/255 ≈ 75% */
			uint8_t threshold = 191;

			/* Check every rated station — ALL must pass */
			int rated_count = 0;
			for (const Station *st : Station::Iterate()) {
				if (st->owner != cid) continue;
				for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
					if (!st->goods[ct].HasRating()) continue;
					rated_count++;
					if (st->goods[ct].rating < threshold) {
						/* This station failed — reset counter and absorb next fire */
						m.maintain_months_ok = 0;
						m.maintain_first_month_pending = true;
						goto next_mission;
					}
				}
			}
			/* Only count progress if player actually has rated stations.
			 * Guard: skip the very first timer fire after a station first
			 * gets a rating — the station may have been rated only moments
			 * before the month boundary, which would give a free increment.
			 * We use a per-mission flag to absorb exactly one "first fire". */
			if (rated_count > 0) {
				if (m.maintain_first_month_pending) {
					/* Absorb this fire — station was new this month */
					m.maintain_first_month_pending = false;
				} else {
					m.maintain_months_ok++;
					Debug(misc, 1, "[AP] Maintain mission '{}': {}/{} months OK",
					      m.location, m.maintain_months_ok, m.amount);
				}
			}
			next_mission:;
		}
		/* Refresh mission window so maintain progress is visible immediately */
		SetWindowClassesDirty(WC_ARCHIPELAGO);

		/* ── Monthly task update ── */
		AP_UpdateTasks();

		/* ── Ruin spawn control ── */
		AP_RuinMonthlyTick();

		/* ── Star spawn control ── */
		AP_StarMonthlyTick();
	}
);

/* Shop rotation removed — the full shop list is always visible.
 * _ap_calendar_shop_refresh timer is no longer needed. */

/* -------------------------------------------------------------------------
 * Mission evaluation — calls EvaluateMission() for all incomplete missions
 * and fires AP_SendCheckByName when a mission is satisfied.
 * Placed here (after AP_ShowNews and all state variables) to avoid
 * forward-declaration issues.
 * ---------------------------------------------------------------------- */

/**
 * Iterate all incomplete missions and send checks for any that are now met.
 * Called from the realtime timer every ~5 s.
 */
/**
 * Called when the AP server reports which locations have been checked.
 * Marks matching missions as completed so the UI stays in sync — critical
 * after save/load and in multiplayer when another player checks a location.
 */
static void AP_OnCheckedLocations(const std::set<std::string> &locations)
{
	/* Stage the checked locations for application after session init.
	 * AP_InitSessionStats() clears _ap_sent_shop_locations and AP_OnSlotData()
	 * resets mission completion flags, so we can't apply directly here. */
	for (const auto &loc : locations) {
		_ap_server_checked_locations.insert(loc);
	}
	_ap_server_checked_pending = true;
	Debug(misc, 0, "[AP] Staged {} checked locations from AP server (total staged: {})",
	      locations.size(), _ap_server_checked_locations.size());

	/* If session is already started, apply immediately (RoomUpdate during play) */
	if (_ap_session_started) {
		AP_ApplyServerCheckedLocations();
	}
}

/** Apply staged checked locations from AP server — runs after session init. */
static void AP_ApplyServerCheckedLocations()
{
	if (!_ap_server_checked_pending) return;
	_ap_server_checked_pending = false;

	int missions_synced = 0;
	int shop_synced = 0;

	/* ── Sync missions ─────────────────────────────────────────── */
	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed) continue;
		if (_ap_server_checked_locations.count(m.location)) {
			m.completed = true;
			missions_synced++;
			Debug(misc, 0, "[AP] Synced completion from server: {} ({})", m.location, m.description);
		}
	}

	/* ── Sync stars ───────────────────────────────────────────── */
	int stars_synced = 0;
	for (APStar &star : _ap_stars) {
		if (!star.collected && _ap_server_checked_locations.count(star.location_name)) {
			star.collected = true;
			_ap_stars_collected++;
			stars_synced++;
			/* Remove object from map if present */
			if (star.tile != UINT32_MAX && IsValidTile(TileIndex{star.tile})) {
				CompanyID old = _current_company;
				_current_company = OWNER_DEITY;
				Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlags{DoCommandFlag::Execute}, TileIndex{star.tile});
				_current_company = old;
			}
		}
	}

	/* ── Sync shop purchases ───────────────────────────────────── */
	for (const std::string &loc : _ap_server_checked_locations) {
		if (loc.rfind("Shop_Purchase_", 0) == 0) {
			if (_ap_sent_shop_locations.find(loc) == _ap_sent_shop_locations.end()) {
				_ap_sent_shop_locations.insert(loc);
				shop_synced++;
			}
		}
	}

	if (missions_synced > 0) {
		IConsolePrint(CC_WHITE, fmt::format("[AP] Synced {} completed missions from AP server.", missions_synced));
	}
	if (shop_synced > 0) {
		IConsolePrint(CC_WHITE, fmt::format("[AP] Synced {} shop purchases from AP server.", shop_synced));
	}
	if (stars_synced > 0) {
		IConsolePrint(CC_WHITE, fmt::format("[AP] Synced {} collected stars from AP server.", stars_synced));
	}
	if (missions_synced > 0 || shop_synced > 0 || stars_synced > 0) {
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
	}
}

static void CheckMissions()
{
	if (_ap_client == nullptr) return;
	if (!_ap_session_started) return;

	int completed_this_pass = 0;
	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed) continue;
		/* Tier gate: do not evaluate missions in a locked tier */
		if (!AP_IsTierUnlocked(m.difficulty)) continue;
		if (EvaluateMission(m)) {
			m.completed = true;
			completed_this_pass++;
			AP_TRACE(fmt::format("MissionComplete: loc='{}' desc='{}' difficulty='{}'", m.location, m.description, m.difficulty));
			AP_SendCheckByName(m.location);
			AP_ShowNews("[AP] Mission complete: " + m.description);
			Debug(misc, 0, "[AP] Mission completed: {} ({})", m.location, m.description);
		}
	}

	if (completed_this_pass > 0) {
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
	}
}

/** Called by the shop window when the player completes a purchase. */
void AP_NotifyShopPurchased()
{
	_ap_shop_purchased = true;
}

static IntervalTimer<TimerGameRealtime> _ap_realtime_timer(
	{ std::chrono::milliseconds(250), TimerGameRealtime::ALWAYS },
	[](auto) {
		/* ── Co-op auto-join: move spectator → Company 0 after map load ── */
		if (_ap_pending_join_company0 &&
		    _game_mode == GM_NORMAL &&
		    _local_company == COMPANY_SPECTATOR) {
			_ap_pending_join_company0 = false;
			NetworkClientRequestMove(CompanyID(0));
			IConsolePrint(CC_WHITE, "[AP] Auto-joining host's company (Company 0)...");
		}

		/* ── MP Join: pump AP client until slot_data arrives ── */
		if (_ap_mp_join_pending && _ap_client != nullptr) {
			EnsureHandlersRegistered();
			_ap_client->Tick();
			/* AP_OnSlotData callback handles everything when slot_data arrives:
			 * NewGRF setup + ScanNewGRFFiles + NetworkClientConnectGame.
			 * We just need to keep pumping Tick() until that happens.
			 * Fallback: if client already had cached slot_data, trigger manually. */
			if (_ap_mp_join_pending && _ap_client->HasSlotData()) {
				/* Re-feed slot_data to our callback manually */
				AP_OnSlotData(_ap_client->GetSlotData());
			}
			return;
		}

		/* In multiplayer, only the server/host processes AP events.
		 * Clients receive state via network commands and map download. */
		if (_networking && !_network_server) return;

		/* Bridge mode: the dedicated server has no _ap_client — session
		 * management is driven by the bridge TCP connection instead.
		 * Skip _ap_client processing but fall through to session setup. */
		if (!_ap_bridge_mode) {
			if (_ap_client == nullptr) return;
			EnsureHandlersRegistered();

			/* Dispatch inbound AP events */
			_ap_client->Tick();

			/* Deferred named-entity name resolution */
			if (_ap_named_entity_refresh_needed &&
			    _game_mode == GM_NORMAL &&
			    _local_company < MAX_COMPANIES) {
				AP_RefreshNamedEntityNames();
				_ap_status_dirty.store(true);
			}
		} else {
			/* Bridge mode: deferred named-entity refresh uses Company 0 */
			if (_ap_named_entity_refresh_needed &&
			    _game_mode == GM_NORMAL &&
			    Company::GetIfValid(CompanyID(0)) != nullptr) {
				AP_RefreshNamedEntityNames();
				_ap_status_dirty.store(true);
			}
		}

		/* First-tick session setup when we enter GM_NORMAL.
		 * Bridge mode: no _ap_client, use Company 0 (first player's company).
		 * Singleplayer: uses _ap_client->GetState() and _local_company. */
		bool session_ready = false;
		if (_ap_bridge_mode) {
			/* Bridge mode: run session start immediately when GM_NORMAL.
			 * Do NOT wait for Company 0 — lock engines before any client connects.
			 * This prevents desync (client downloads map with locked engines). */
			session_ready = !_ap_session_started &&
			    _game_mode == GM_NORMAL;
		} else {
			session_ready = !_ap_session_started &&
			    _game_mode == GM_NORMAL &&
			    _local_company < MAX_COMPANIES &&
			    _ap_client->GetState() == APState::AUTHENTICATED;
		}

		if (session_ready) {

			_ap_session_started = true;

			/* Auto-open console log file so every IConsolePrint() line is
			 * captured to ap_console.log for post-game debugging.  Uses the
			 * same mechanism as the in-game "script" command. */
			{
				extern std::optional<FileHandle> _iconsole_output_file;
				if (!_iconsole_output_file.has_value()) {
					_iconsole_output_file = FileHandle::Open(std::string("ap_console.log"), std::string_view("wb"));
					if (_iconsole_output_file.has_value()) {
						IConsolePrint(CC_INFO, "[AP] Console log opened: ap_console.log");
					}
				}
			}

			/* Autosave — always save on exit during AP sessions */
			_settings_client.gui.autosave_on_exit = true;

			CompanyID cid = _ap_bridge_mode ? CompanyID(0) : _local_company;
			Company *c = Company::GetIfValid(cid);

			/* Debug: log session start to file */
			{
				FILE *dbg = fopen("ap_debug.log", "a");
				if (dbg) {
					fmt::print(dbg, "[SessionStart] bridge_mode={} cid={} company_valid={} missions={} locked_vehicles={}\n",
						(int)_ap_bridge_mode, (int)cid.base(), c != nullptr ? 1 : 0,
						_ap_pending_sd.missions.size(), _ap_pending_sd.locked_vehicles.size());
					fclose(dbg);
				}
			}

			/* English was forced in AP_ConsumeWorldStart() — safe there
			 * because _game_mode == GM_MENU (no concurrent string lookups). */

			/* Disable vehicle/airport expiry NOW — before building the engine
			 * map.  EngineNameContext::PurchaseList only returns a name for
			 * engines that are currently available; without this flag some
			 * engines (especially early steam locos that have expired by 1950)
			 * return an empty string and never make it into _ap_engine_map,
			 * causing "Unknown item" warnings when AP tries to unlock them. */
			_settings_game.vehicle.never_expire_vehicles = true;
			_settings_game.station.never_expire_airports = true;

			/* Force percentage-based service intervals (default 30%).
			 * This ensures vehicles go to depot when reliability drops
			 * below 30%, which is critical for traps like Breakdown Wave
			 * that set reliability to 1. Day-based intervals miss this. */
			if (c != nullptr) {
				c->settings.vehicle.servint_ispercent = true;
			}

			/* Build the engine name → ID lookup map (uses current language = English) */
			BuildEngineMap();

			/* Build the cargo name → type map for mission evaluation */
			BuildCargoMap();

			/* Reset session statistics for mission evaluation */
			AP_InitSessionStats();

				/* Apply cumul stats staged by saveload (must run after AP_InitSessionStats) */
				if (_ap_staging_stats) {
					for (CargoType ci = 0; ci < NUM_CARGO; ci++)
						_ap_cumul_cargo[ci] = _ap_staging_cargo[ci];
					_ap_cumul_profit = (Money)_ap_staging_profit;
					if (_ap_staging_items_received >= 0)
						_ap_items_received_count = _ap_staging_items_received;
					_ap_stats_initialized = true;
					/* Clear staging */
					for (CargoType ci = 0; ci < NUM_CARGO; ci++) _ap_staging_cargo[ci] = 0;
					_ap_staging_profit = 0;
					_ap_staging_items_received = -1;
					_ap_staging_stats = false;
					Debug(misc, 1, "[AP] Restored cumul stats from savegame (profit={}, items={})",
					      (int64_t)_ap_cumul_profit, _ap_items_received_count);
				}

				/* Apply shop sent locations staged by saveload (must run after AP_InitSessionStats) */
				AP_ApplyStagedShopSent();

				/* Apply completed missions staged by saveload (must run after AP_OnSlotData
				 * has overwritten _ap_pending_sd with the fresh mission list from the server) */
				AP_ApplyStagedCompletedMissions();
				AP_ApplyStagedMaintainCounters();

				/* Apply checked locations received from the AP server (Connected/RoomUpdate).
				 * Must run AFTER AP_InitSessionStats (which clears shop set) and AFTER
				 * AP_ApplyStagedCompletedMissions (savegame data first, server overrides). */
				AP_ApplyServerCheckedLocations();

				/* Apply task data staged by saveload (must run after AP_InitSessionStats) */
				if (_ap_staging_has_data) {
					AP_ApplyTasksStr(_ap_staging_tasks);
					if (_ap_staging_task_checks >= 0)
						_ap_task_checks_completed = _ap_staging_task_checks;
					_ap_staging_tasks.clear();
					_ap_staging_task_checks = -1;
					_ap_staging_has_data = false;
				}

				/* Assign named map entities to named-destination missions */
				AP_AssignNamedEntities();
				/* Restore cumulative progress staged by saveload (must run AFTER
				 * AP_AssignNamedEntities sets up entity IDs, and AFTER AP_OnSlotData
				 * has overwritten _ap_pending_sd with fresh server data) */
				AP_ApplyStagedNamedEntities();
				/* Refresh names for missions restored from savegame (deferred from Load()) */
				if (_ap_named_entity_refresh_needed) AP_RefreshNamedEntityNames();
				/* Refresh task entity names (deferred from savegame load) */
				AP_RefreshTaskNames();
				/* Refresh ruin location_name and cargo_name (deferred from savegame load) */
				AP_RefreshRuinNames();
				/* Generate initial tasks (safe to call: only adds if room) */
				AP_GenerateTasks();
				_ap_status_dirty.store(true); /* refresh GUI to show resolved [Town]/[Industry] names */

				/* Rename towns using multiworld player names or custom list */
				AP_RenameTowns();

				/* Multiplayer mode — disable features that cause desync in MP.
				 * These use Command::Do(DC_EXEC) directly and RandomRange(),
				 * which diverges server/client game state. */
				if (_ap_pending_sd.multiplayer_mode) {
					IConsolePrint(CC_WHITE, "[AP] Multiplayer mode: disabling ruins, stars, Colby, Demigod, Wrath.");
					_ap_pending_sd.ruin_pool_size   = 0;
					_ap_pending_sd.enable_stars     = false;
					_ap_pending_sd.colby_event      = false;
					_ap_pending_sd.demigod_enabled  = false;
					_ap_pending_sd.wrath_enabled    = false;
				}

				/* Stars — spawned via AP_StarMonthlyTick() on first monthly tick
				 * (same pattern as ruins — ensures GRF objects are fully loaded) */

				/* Colby Event — initialise town selection and cargo type */
				_ap_colby_enabled    = _ap_pending_sd.colby_event;
				_ap_colby_start_year = _ap_pending_sd.colby_start_year;
				_ap_colby_town_seed  = _ap_pending_sd.colby_town_seed;
				_ap_colby_cargo_name = _ap_pending_sd.colby_cargo;
				AP_ColbyInit();

				/* Demigod system (God of Wackens) */
				AP_DemigodInit(_ap_pending_sd);

				/* Wrath of the God of Wackens */
				AP_WrathInit(_ap_pending_sd);

			/* AP settings: vehicle/airport expiry already disabled above (before
			 * BuildEngineMap).  No additional setting needed here. */

			/* Strip the local company from every ENGINE that was auto-unlocked
			 * by StartupOneEngine() at game start.
			 * WAGONS are excluded by default — they are always freely available so the
			 * player can use any locomotive they receive from AP.
			 * BUT: if enable_wagon_unlocks is true, wagons are also locked and must
			 * be unlocked via AP items.
			 *
			 * SELECTIVE LOCKING: if the APWorld sent a locked_vehicles list,
			 * only lock engines whose English name is in that list.  Engines
			 * NOT in the list (e.g. Iron Horse engines when enable_iron_horse=false)
			 * remain available so the player can use them freely.
			 * Legacy fallback: if no locked_vehicles list, lock everything (old behaviour). */
			_ap_unlocked_engine_ids.clear();

			const bool has_lock_list = !_ap_pending_sd.locked_vehicles.empty();
			const bool lock_wagons  = _ap_pending_sd.enable_wagon_unlocks;
			int locked_count = 0;
			for (Engine *e : Engine::Iterate()) {
				bool is_wagon = (e->type == VEH_TRAIN &&
				                 e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON);
				if (is_wagon && !lock_wagons) { continue; }
				if (_ap_bridge_mode) {
					/* Bridge mode: lock for ALL companies (company-agnostic).
					 * This runs before any client connects, so no desync risk.
					 * The map state already has locked engines when clients download it. */
					e->company_avail = CompanyMask{};
				} else {
					if (!e->company_avail.Test(cid)) { continue; }
					e->company_avail.Reset(cid);
				}
				locked_count++;
			}
			Debug(misc, 1, "[AP] Session lock complete: {} engines locked (has_lock_list={}, bridge={})",
			      locked_count, has_lock_list, _ap_bridge_mode);
			AP_TRACE(fmt::format("SessionLock: {} engines locked (has_lock_list={}, bridge_mode={})", locked_count, has_lock_list, _ap_bridge_mode));

			/* Unlock ALL rail and road types unconditionally — ensures any
			 * AP-randomised engine's track/road type is always buildable. */
			if (c != nullptr) {
				c->avail_railtypes = GetRailTypes(true);
				c->avail_roadtypes = GetRoadTypes(true);
			}

			/* Unlock ALL airports immediately regardless of in-game year.
			 * AirportSpec::IsAvailable() checks min_year before never_expire_airports,
			 * so setting min_year=0 on every enabled spec is the only reliable fix.
			 * We leave max_year alone — never_expire_airports handles expiry. */
			{
				int airport_count = 0;
				for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
					AirportSpec *as = AirportSpec::GetWithoutOverride(i);
					if (as == nullptr || !as->enabled) continue;
					if (as->min_year > TimerGameCalendar::Year{0}) {
						as->min_year = TimerGameCalendar::Year{0};
					}
					airport_count++;
				}
				InvalidateWindowData(WC_BUILD_STATION, TRANSPORT_AIR);
				Debug(misc, 0, "[AP] All {} airports unlocked (min_year reset to 0)", airport_count);
			}

			InvalidateWindowClassesData(WC_BUILD_VEHICLE);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
			AP_OK(fmt::format("AP session started: {} engines locked, all railtypes/roadtypes unlocked.", locked_count));
			AP_TRACE(fmt::format("=== SESSION START SUMMARY === missions={} starting_vehicles={} pending_items={} wrath={} colby={} demigod={} ruin_pool={} stars={}",
				(int)_ap_pending_sd.missions.size(), (int)_ap_pending_sd.starting_vehicles.size(),
				(int)_ap_pending_items.size(), (int)_ap_pending_sd.wrath_enabled, (int)_ap_pending_sd.colby_event,
				(int)_ap_pending_sd.demigod_enabled, _ap_pending_sd.ruin_pool_size, (int)_ap_stars.size()));

			/* Unlock all starting vehicles.
			 * The APWorld is responsible for ensuring only climate-compatible
			 * vehicles appear in starting_vehicles — no fallback substitution. */
			std::string sv_list;
			for (const std::string &sv : _ap_pending_sd.starting_vehicles) {
				if (sv.empty()) continue;
				if (!_ap_engine_map_built) BuildEngineMap();
				bool ok = AP_UnlockEngineByName(sv);
				if (!ok) {
					/* Retry after map rebuild (edge case: map built before NewGRFs finished) */
					_ap_engine_map_built = false;
					BuildEngineMap();
					ok = AP_UnlockEngineByName(sv);
				}
				if (ok) {
					if (!sv_list.empty()) sv_list += ", ";
					sv_list += sv;
				} else {
					AP_ERR("Starting vehicle '" + sv + "' not found in engine map!");
				}
			}
			if (!sv_list.empty()) {
				AP_OK("[AP] Starting vehicles for this session: " + sv_list);
				AP_ShowNews("[AP] Starting vehicles: " + sv_list);
			}

			/* Apply starting cash bonus if configured */
			if (_ap_pending_sd.starting_cash_bonus > 0 && c != nullptr) {
				static const Money bonus_amounts[] = {
					0, 50000LL, 200000LL, 500000LL, 2000000LL
				};
				int tier = std::clamp(_ap_pending_sd.starting_cash_bonus, 0, 4);
				if (tier > 0) {
					AP_ChangeMoney(cid, bonus_amounts[tier]);
					AP_ShowNews(fmt::format("[AP] Starting bonus: {} added to your account!",
					    AP_Money(bonus_amounts[tier])));
					AP_OK(fmt::format("[AP] Starting cash bonus tier {} = +{}",
					    tier, AP_Money(bonus_amounts[tier])));
				}
			}

			/* Flush items that arrived before GM_NORMAL */
			/* ── Track direction locks: initialise BEFORE flushing pending items.
			 * _ap_track_dirs_inited guards against re-locking on reconnect:
			 *   - First session start: flag is false → lock all types → set flag.
			 *   - Save/load restore: SL chunk sets bytes → flag still false, but
			 *     at least one byte will be non-zero → 'any_loaded' path skips reset.
			 *   - Reconnect (AP_OnSlotData resets _ap_session_started to false but
			 *     does NOT reset the flag): flag is true → skip init entirely.
			 *     Item replay via flush restores any missing unlocks automatically. */
			AP_DetectNewGRFRailTypes();

			if (_ap_pending_sd.enable_rail_direction_unlocks && !_ap_track_dirs_inited) {
				bool any_loaded = false;
				for (int rt = 0; rt < AP_NUM_RAILTYPES; rt++) {
					if (_ap_locked_track_dirs[rt] != 0) { any_loaded = true; break; }
				}
				if (!any_loaded) {
					/* Fresh start: lock vanilla types (0-3) always */
					for (int rt = 0; rt < 4; rt++)
						_ap_locked_track_dirs[rt] = 0x3Fu;
				}
				/* Always ensure NewGRF types are locked if their GRF is active
				 * and they haven't been touched yet (handles saves created before
				 * NewGRF rail locking was implemented — vanilla locks exist but
				 * NG/Metro/VacTube were 0x00). */
				if (_ap_pending_sd.enable_iron_horse && _ap_ng_rt_id != 0xFF
				    && _ap_locked_track_dirs[4] == 0)
					_ap_locked_track_dirs[4] = 0x3Fu;  /* Narrow Gauge */
				if (_ap_pending_sd.enable_iron_horse && _ap_metro_rt_id != 0xFF
				    && _ap_locked_track_dirs[5] == 0)
					_ap_locked_track_dirs[5] = 0x3Fu;  /* Metro */
				if (_ap_pending_sd.enable_vactrain && _ap_vact_rt_id != 0xFF
				    && _ap_locked_track_dirs[6] == 0)
					_ap_locked_track_dirs[6] = 0x3Fu;  /* Vacuum Tube */
				_ap_track_dirs_inited = true;
				Debug(misc, 1, "[AP] Track dir locks init: Normal=0x{:02X} Elec=0x{:02X} Mono=0x{:02X} Maglev=0x{:02X} NG=0x{:02X} Metro=0x{:02X} VacTube=0x{:02X}",
				      (unsigned)_ap_locked_track_dirs[0], (unsigned)_ap_locked_track_dirs[1],
				      (unsigned)_ap_locked_track_dirs[2], (unsigned)_ap_locked_track_dirs[3],
				      (unsigned)_ap_locked_track_dirs[4], (unsigned)_ap_locked_track_dirs[5],
				      (unsigned)_ap_locked_track_dirs[6]);
			} else if (!_ap_pending_sd.enable_rail_direction_unlocks) {
				for (int rt = 0; rt < AP_NUM_RAILTYPES; rt++)
					_ap_locked_track_dirs[rt] = 0;
				_ap_track_dirs_inited = false;
			}

			/* ── Road direction locks ────────────────────────────────────── */
			if (_ap_pending_sd.enable_road_direction_unlocks && !_ap_road_dirs_inited) {
				if (_ap_locked_road_dirs == 0) _ap_locked_road_dirs = 0x03u; /* bits 0-1 */
				_ap_road_dirs_inited = true;
				Debug(misc, 1, "[AP] Road dir locks init: 0x{:02X}", (unsigned)_ap_locked_road_dirs);
			} else if (!_ap_pending_sd.enable_road_direction_unlocks) {
				_ap_locked_road_dirs = 0;
				_ap_road_dirs_inited = false;
			}

			/* ── Tram direction locks ────────────────────────────────────── */
			if (_ap_pending_sd.enable_road_direction_unlocks && !_ap_tram_dirs_inited) {
				if (_ap_locked_tram_dirs == 0) _ap_locked_tram_dirs = 0x03u; /* bits 0-1 */
				_ap_tram_dirs_inited = true;
				Debug(misc, 1, "[AP] Tram dir locks init: 0x{:02X}", (unsigned)_ap_locked_tram_dirs);
			} else if (!_ap_pending_sd.enable_road_direction_unlocks) {
				_ap_locked_tram_dirs = 0;
				_ap_tram_dirs_inited = false;
			}

			/* ── Signal locks ────────────────────────────────────────────── */
			if (_ap_pending_sd.enable_signal_unlocks && !_ap_signals_inited) {
				if (_ap_locked_signals == 0) _ap_locked_signals = 0x3Fu; /* bits 0-5 */
				_ap_signals_inited = true;
				Debug(misc, 1, "[AP] Signal locks init: 0x{:02X}", (unsigned)_ap_locked_signals);
			} else if (!_ap_pending_sd.enable_signal_unlocks) {
				_ap_locked_signals = 0;
				_ap_signals_inited = false;
			}

			/* ── Bridge locks ────────────────────────────────────────────── */
			if (_ap_pending_sd.enable_bridge_unlocks && !_ap_bridges_inited) {
				if (_ap_locked_bridges == 0) _ap_locked_bridges = 0x1FFFu; /* bits 0-12 */
				_ap_bridges_inited = true;
				Debug(misc, 1, "[AP] Bridge locks init: 0x{:04X}", (unsigned)_ap_locked_bridges);
			} else if (!_ap_pending_sd.enable_bridge_unlocks) {
				_ap_locked_bridges = 0;
				_ap_bridges_inited = false;
			}

			/* ── Tunnel lock ─────────────────────────────────────────────── */
			if (_ap_pending_sd.enable_tunnel_unlocks && !_ap_tunnels_inited) {
				if (!_ap_locked_tunnels) _ap_locked_tunnels = true;
				_ap_tunnels_inited = true;
				Debug(misc, 1, "[AP] Tunnel lock init: {}", _ap_locked_tunnels ? "LOCKED" : "unlocked");
			} else if (!_ap_pending_sd.enable_tunnel_unlocks) {
				_ap_locked_tunnels = false;
				_ap_tunnels_inited = false;
			}

			/* ── Airport locks ───────────────────────────────────────────── */
			if (_ap_pending_sd.enable_airport_unlocks && !_ap_airports_inited) {
				/* AT_SMALL (bit 0) is always free; lock bits 1-8 */
				if (_ap_locked_airports == 0) _ap_locked_airports = 0x01FEu; /* bits 1-8 */
				_ap_airports_inited = true;
				Debug(misc, 1, "[AP] Airport locks init: 0x{:04X}", (unsigned)_ap_locked_airports);
			} else if (!_ap_pending_sd.enable_airport_unlocks) {
				_ap_locked_airports = 0;
				_ap_airports_inited = false;
			}

			/* ── Tree locks ──────────────────────────────────────────────── */
			if (_ap_pending_sd.enable_tree_unlocks && !_ap_trees_inited) {
				if (_ap_locked_trees == 0) _ap_locked_trees = 0x03FFu; /* bits 0-9 */
				_ap_trees_inited = true;
				Debug(misc, 1, "[AP] Tree locks init: 0x{:04X}", (unsigned)_ap_locked_trees);
			} else if (!_ap_pending_sd.enable_tree_unlocks) {
				_ap_locked_trees = 0;
				_ap_trees_inited = false;
			}

			/* ── Terraform locks ─────────────────────────────────────────── */
			if (_ap_pending_sd.enable_terraform_unlocks && !_ap_terraform_inited) {
				if (_ap_locked_terraform == 0) _ap_locked_terraform = 0x03u; /* bits 0-1 */
				_ap_terraform_inited = true;
				Debug(misc, 1, "[AP] Terraform locks init: 0x{:02X}", (unsigned)_ap_locked_terraform);
			} else if (!_ap_pending_sd.enable_terraform_unlocks) {
				_ap_locked_terraform = 0;
				_ap_terraform_inited = false;
			}

			/* ── Town Authority action locks ─────────────────────────── */
			if (_ap_pending_sd.enable_town_action_unlocks && !_ap_town_actions_inited) {
				if (_ap_locked_town_actions == 0) _ap_locked_town_actions = 0xFFu; /* bits 0-7 */
				_ap_town_actions_inited = true;
				Debug(misc, 1, "[AP] Town Action locks init: 0x{:02X}", (unsigned)_ap_locked_town_actions);
			} else if (!_ap_pending_sd.enable_town_action_unlocks) {
				_ap_locked_town_actions = 0;
				_ap_town_actions_inited = false;
			}

			AP_TRACE(fmt::format("FlushPendingItems: processing {} queued items", _ap_pending_items.size()));
			for (const APItem &item : _ap_pending_items) AP_OnItemReceived(item);
			_ap_pending_items.clear();

			/* Open the status overlay (skip on dedicated server — no GUI) */
			if (!_network_dedicated) ShowArchipelagoStatusWindow();

			AP_OK(fmt::format("AP session started. {} engines in map. Mission evaluation active.",
			      _ap_engine_map.size()));
		}

		/* Realtime tracking: economy stats, missions and Colby run every tick (250 ms).
		 * Engine lock sweep runs every ~5 s (20 × 250 ms) — iterates all engines, kept slower.
		 * Win condition runs every ~1 s (4 × 250 ms) — rarely true, cheap guard. */
		_ap_realtime_ticks++;

		/* Tick down cooldowns every 250 ms */
		if (_ap_death_cooldown_ticks > 0) _ap_death_cooldown_ticks--;

		/* Timed effects only count down and apply while actually playing.
		 * Paused game or being in a menu must not drain effect duration. */
		if (_game_mode == GM_NORMAL) {
			/* Bridge mode uses Company 0 (host company) on dedicated server */
			CompanyID tick_cid = _ap_bridge_mode ? CompanyID(0) : _local_company;

			/* Breakdown Wave: keep reliability at 1 while active, restore when expired.
			 * NOTE: Does NOT gate on _ap_session_started — must restore even if
			 * AP is disconnected, otherwise vehicles stay broken forever. */
			if (_ap_breakdown_wave_ticks > 0) {
				_ap_breakdown_wave_ticks--;
				CompanyID cid = tick_cid;
				if (_ap_breakdown_wave_ticks > 0) {
					/* Still active: keep vehicles broken */
					for (Vehicle *v : Vehicle::Iterate()) {
						if (v->owner == cid && v->IsPrimaryVehicle()) {
							v->breakdown_chance = 255;
							v->reliability = 1;
						}
					}
				} else {
					/* Expired: restore normal reliability from engine specs */
					for (Vehicle *v : Vehicle::Iterate()) {
						if (v->owner == cid && v->IsPrimaryVehicle()) {
							const Engine *e = v->GetEngine();
							if (e != nullptr) {
								v->reliability = e->reliability;
								v->breakdown_chance = 128;
							}
						}
					}
					AP_ShowNews("[AP] Breakdown Wave ended! Vehicles returning to normal reliability.");
				}
			}

			/* Fuel Shortage: re-apply speed cap every tick while active */
			if (_ap_fuel_shortage_ticks > 0) {
				_ap_fuel_shortage_ticks--;
				CompanyID cid = tick_cid;
				for (Vehicle *v : Vehicle::Iterate()) {
					if (v->owner == cid && v->IsPrimaryVehicle()) {
						const Engine *e = v->GetEngine();
						if (e != nullptr) {
							uint16_t half_speed = e->GetDisplayMaxSpeed() / 2;
							if (v->cur_speed > half_speed) v->cur_speed = half_speed;
						}
					}
				}
			}

			/* Cargo Bonus: tick down 2x payment multiplier */
			if (_ap_cargo_bonus_ticks > 0) _ap_cargo_bonus_ticks--;

			/* Reliability Boost: re-apply max reliability every tick to counter decay */
			if (_ap_reliability_boost_ticks > 0) {
				_ap_reliability_boost_ticks--;
				CompanyID cid = tick_cid;
				for (Vehicle *v : Vehicle::Iterate()) {
					if (v->owner == cid && v->IsPrimaryVehicle()) {
						const Engine *e = v->GetEngine();
						if (e != nullptr) {
							v->reliability = e->reliability_max;
							v->breakdown_chance = 0;
						}
					}
				}
			}

			/* Station Boost: re-apply MAX_STATION_RATING to all player stations */
			if (_ap_station_boost_ticks > 0) {
				_ap_station_boost_ticks--;
				CompanyID cid = tick_cid;
				for (Station *st : Station::Iterate()) {
					if (st->owner != cid) continue;
					for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
						if (st->goods[ct].HasRating()) {
							st->goods[ct].rating = MAX_STATION_RATING;
						}
					}
				}
			}
			/* License Revoke: tick down and restore when expired.
			 * NOTE: Does NOT require _ap_session_started — ticks must
			 * count down even if AP is temporarily disconnected, otherwise
			 * the revoke becomes effectively permanent. */
			if (_ap_license_revoke_ticks > 0) {
				_ap_license_revoke_ticks--;
				if (_ap_license_revoke_ticks == 0) {
					CompanyID cid = tick_cid;
					/* Unhide ALL engines of the revoked type unconditionally.
					 * Previously only unhid engines in _ap_unlocked_engine_ids,
					 * which left aircraft (etc.) permanently hidden if the player
					 * had no unlocked engines of that type when the trap fired. */
					for (Engine *e : Engine::Iterate()) {
						if ((int)e->type != _ap_license_revoke_type) continue;
						e->company_hidden.Reset(cid);
					}
					_ap_license_revoke_type = -1;
					MarkWholeScreenDirty();
					AP_TRACE("LicenseRestored: vehicle type restored after revoke expired");
					AP_ShowNews("[AP] Vehicle License restored! You may build that vehicle type again.");
				}
			}

			/* Colby escape timer: countdown to second popup */
			if (_ap_colby_escape_ticks > 0 && _ap_session_started) {
				_ap_colby_escape_ticks--;
				if (_ap_colby_escape_ticks == 0) {
					AP_ColbyShowEscapeQuery();
				}
			}
		}

		/* ── Engine lock sweep: every ~5 s ─────────────────────────────── */
		/* DISABLED in bridge mode: causes desync because the realtime timer
		 * is NOT synchronised between server and client.  Direct state
		 * modifications (company_avail, railtypes) at different ticks → crash.
		 * In bridge mode this sweep is also unnecessary because:
		 *   - AP_IsActive() blocks vanilla engine introduction
		 *   - Initial locking happens in session-start (part of map state)
		 *   - Unlocks go through CMD_ENGINE_CTRL::Post (network-distributed)
		 *   - DoStartupNewCompany gives all railtypes in bridge mode */
		if (!_ap_bridge_mode &&
		    _ap_realtime_ticks % 20 == 0 &&
		    _ap_session_started &&
		    _game_mode == GM_NORMAL) {

			/* Re-apply engine locks every ~5 s as a safety net against
			 * OpenTTD's engine introduction system (StartupEngines/StartupOneEngine)
			 * which runs on NewGRF changes and re-sets EngineFlag::Available and
			 * company_avail for all engines whose intro_date has passed.
			 * We use _ap_unlocked_engine_ids (set by AP_UnlockEngineByName) to
			 * distinguish "AP-unlocked" from "re-introduced by StartupEngines". */
			CompanyID lock_cid = _local_company;
			if (lock_cid < MAX_COMPANIES) {
				bool need_invalidate = false;
				const bool lock_wagons = _ap_pending_sd.enable_wagon_unlocks;
				for (Engine *e : Engine::Iterate()) {
					/* Wagons stay available unless wagon unlocks are enabled */
					if (e->type == VEH_TRAIN &&
					    e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON &&
					    !lock_wagons) continue;

					/* If AP has explicitly unlocked this engine this session, leave it alone */
					if (_ap_unlocked_engine_ids.count(e->index) > 0) continue;

					/* Lock everything else — robust against GRF name mismatches */
					if (e->company_avail.Test(lock_cid)) {
						e->company_avail.Reset(lock_cid);
						need_invalidate = true;
					}
				}
				if (need_invalidate) InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
			}
		}

		/* ── Real-time tracking: every 250 ms ──────────────────────────── */
		if (_ap_session_started && _game_mode == GM_NORMAL) {

			/* Accumulate cargo/profit from completed economy periods */
			AP_UpdateSessionStats();

			/* Accumulate named-destination progress (town/industry deliveries).
			 * NOTE: In multiplayer, skip this because it contains Ruin spawning
			 * which calls RandomRange() and Command::Do(Execute), causing desync
			 * with clients that don't run AP code. */
			AP_UpdateNamedMissions();

			/* Evaluate all incomplete missions and refresh the mission window */
			CheckMissions();
			SetWindowClassesDirty(WC_ARCHIPELAGO);

			/* The following subsystems modify game state (tiles, companies, vehicles)
			 * using direct Command::Do(Execute) and RandomRange() calls.
			 * In multiplayer these MUST be skipped to avoid desync — clients don't
			 * run AP code, so their game state would diverge from the server's. */
			/* Drive Colby Event progression */
			if (_ap_colby_enabled && !_ap_colby_done) AP_ColbyTick();

			/* Drive Demigod (God of Wackens) system */
			AP_DemigodTick();

			/* Wrath yearly evaluation (checks for year boundary) */
			AP_WrathYearlyEval();
		}

		if (_ap_realtime_ticks >= 40 &&
		    _ap_session_started && !_ap_goal_sent &&
		    _game_mode == GM_NORMAL) {

			_ap_realtime_ticks = 0;
			if (CheckWinCondition(_ap_pending_sd)) {
				_ap_goal_sent = true;
				if (_ap_client != nullptr) {
					_ap_client->SendGoal();
				}
				/* Bridge mode: victory is detected by the bridge via win_progress */
				Debug(misc, 0, "[AP] Win condition reached! Goal sent.");
				AP_TRACE("WIN_CONDITION_REACHED: Goal sent to AP server!");
				AP_OK("*** WIN CONDITION REACHED! Goal sent to server! ***");
				AP_ShowNews("[AP] WIN CONDITION REACHED! Goal sent to server!");
				ShowAPVictoryScreen();
			}
		}
	}
);

/* ---------------------------------------------------------------------------
 * AP Connection Config — persist server/slot between game sessions
 * Written to <personal_dir>/ap_connection.cfg (simple key=value format).
 * Called by GUI on successful connect (save) and on window open (load).
 * -------------------------------------------------------------------------- */

void AP_SaveConnectionConfig()
{
	std::string path = _personal_dir + "ap_connection.cfg";
	FILE *f = fopen(path.c_str(), "w");
	if (f == nullptr) return;
	fmt::print(f, "host={}\n", _ap_last_host);
	fmt::print(f, "port={}\n", (unsigned)_ap_last_port);
	fmt::print(f, "slot={}\n", _ap_last_slot);
	fmt::print(f, "pass={}\n", _ap_last_pass);
	fmt::print(f, "ssl={}\n", _ap_last_ssl ? 1 : 0);
	fclose(f);
}

void AP_LoadConnectionConfig()
{
	std::string path = _personal_dir + "ap_connection.cfg";
	FILE *f = fopen(path.c_str(), "r");
	if (f == nullptr) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		/* Strip trailing newline */
		size_t len = strlen(line);
		while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
		std::string s(line);
		auto eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string key = s.substr(0, eq);
		std::string val = s.substr(eq + 1);
		if (key == "host" && !val.empty()) _ap_last_host = val;
		else if (key == "port") { uint16_t p = 0; auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), p); if (ec == std::errc{} && p > 0) _ap_last_port = p; }
		else if (key == "slot") _ap_last_slot = val;
		else if (key == "pass") _ap_last_pass = val;
		else if (key == "ssl") _ap_last_ssl = (val == "1");
	}
	fclose(f);
}

/* ---------------------------------------------------------------------------
 * AP_EnsureBasesets — activate bundled OpenGFX/OpenSFX/OpenMSX if the engine
 * is currently running with fallback (silent / missing-sprite) sets.
 * Called once from intro_gui.cpp OnInit(), after FindSets() has already run.
 * Only switches a set if (a) the current set is marked fallback AND
 * (b) the named set is actually present on disk.
 * The ini_set write makes the choice persist to openttd.cfg on shutdown.
 * -------------------------------------------------------------------------- */
void AP_EnsureBasesets()
{
	/* Graphics — only switch if current is fallback */
	const GraphicsSet *gfx = BaseGraphics::GetUsedSet();
	if (gfx == nullptr || gfx->fallback) {
		if (BaseGraphics::SetSetByName("OpenGFX")) {
			BaseGraphics::ini_data.name = "OpenGFX";
		}
	}
	/* Sound */
	const SoundsSet *sfx = BaseSounds::GetUsedSet();
	if (sfx == nullptr || sfx->fallback) {
		if (BaseSounds::SetSetByName("OpenSFX")) {
			BaseSounds::ini_set = "OpenSFX";
		}
	}
	/* Music */
	const MusicSet *msx = BaseMusic::GetUsedSet();
	if (msx == nullptr || msx->fallback) {
		if (BaseMusic::SetSetByName("OpenMSX")) {
			BaseMusic::ini_set = "OpenMSX";
		}
	}
}

/* =========================================================================
 * Community Vehicle Names
 * When AP is active and the local company builds a vehicle, give it a random
 * name drawn from the Discord community pool.
 * ========================================================================= */

/** Discord community name pool — base names (no suffix). */
static const char * const kCommunityNames[] = {
	/* Core community — Discord regulars */
	"Rafcor",      "Boo",         "DainBread",   "Game Falor",  "Solida",
	"Aginah",      "WackenGuard", "Kiri",         "Mira",        "Fenix",
	"Cobalt",      "Dusk",        "Oryn",         "Patch",       "Tinker",
	"Rusk",        "Sable",       "Lumen",        "Ash",         "Flint",
	"Vex",         "Cog",         "Drift",        "Pebble",      "Grit",
	"Slate",       "Briar",       "Hollow",       "Stave",       "Brine",
	/* More community names */
	"Zephyr",      "Torque",      "Wren",         "Sprocket",    "Ferrum",
	"Nadir",       "Volta",       "Lark",         "Smolt",       "Cinder",
	"Gravel",      "Mote",        "Haze",         "Tallow",      "Silt",
	"Crest",       "Fern",        "Bivouac",      "Caulk",       "Shunt",
	"Buffer",      "Rivet",       "Bogie",        "Axle",        "Coupling",
	"Hopper",      "Tender",      "Footplate",    "Firebox",      "Smokebox",
	/* OpenTTD-themed handles */
	"Bridgemaster","Signalman",   "Stationmaster","Tracklay",    "Pointsman",
	"Dispatcher",  "Conductor",   "Brakesman",    "Shunter",     "Platelayer",
	/* Beta testers & thread regulars */
	"Killeroid",   "Wizard Brandon","Ty",          "Lil David",   "Leftywalle",
	"Arathorrn",   "Nixill",      "Buchstabensalat","Energymaster","Kbab",
	"Sanron",      "RoobyRoo",    "Crazycolbster","Super Star Earth","Dogveloper",
};

/** Transport suffixes appended with ~60% probability. */
static const char * const kNameSuffixes[] = {
	" Express",  " Freight",  " Logistics", " Transport",
	" Rail",     " Lines",    " Co.",       " Ltd.",
	" Services", " Haulage",  " Transit",   " Cargo",
	" Junction", " & Sons",
};

/** Rare special names (~5% chance, drawn from separate pool). */
static const char * const kRareNames[] = {
	"The Wagon God",          "DeathLink Express",      "The Bug Finder",
	"Curse of WackenGuard",   "Speed Boost Special",    "AP Test Vehicle",
	"Archipelago One",        "The Colby Incident",     "404 Train Not Found",
	"NaN Express",            "Undefined Behaviour",    "Segfault Special",
	"Off By One Express",     "This Is Fine",           "Please Rebase",
	"Git Blame Engine",       "Works On My Machine",    "The Loan Shark",
	"Cascading Failure",      "Feature Not Bug",
};

/**
 * Shuffled index pool — we exhaust the community list before repeating.
 * Rebuilt automatically when empty.
 */
static std::vector<int> _ap_name_pool;
static std::set<int>    _ap_used_rare;   ///< Indices into kRareNames already assigned

/** Next suffix index (cycles round-robin, offset by current vehicle count). */
static int _ap_suffix_index = 0;

/** Build / rebuild the name pool (shuffled order, no repeats until exhausted). */
static void RebuildNamePool()
{
	constexpr int N = (int)(sizeof(kCommunityNames) / sizeof(kCommunityNames[0]));
	_ap_name_pool.resize(N);
	for (int i = 0; i < N; i++) _ap_name_pool[i] = i;
	/* Fisher-Yates using InteractiveRandom (safe for multiplayer — won't desync game RNG) */
	for (int i = N - 1; i > 0; i--) {
		int j = (int)(InteractiveRandomRange((uint32_t)(i + 1)));
		std::swap(_ap_name_pool[i], _ap_name_pool[j]);
	}
}

/**
 * Called by each vehicle build command (train / road vehicle / ship / aircraft)
 * after the vehicle has been fully constructed.
 *
 * Only applies when:
 *  - AP is connected (session active)
 *  - The vehicle belongs to the local company (not AI or multiplayer opponent)
 *  - The vehicle is a front engine / primary vehicle (not a wagon or shadow unit)
 */
void AP_OnVehicleCreated(Vehicle *v)
{
	if (v == nullptr)                         return;
	if (!AP_IsConnected())                    return;
	if (v->owner != _local_company)           return;
	/* Skip wagons, tenders, shadows, rotors — only rename the primary unit */
	if (!v->IsPrimaryVehicle())               return;
	/* Respect the "Community Vehicle Names" option */
	if (!AP_GetSlotData().community_vehicle_names) return;

	/* ~5% chance of a rare/funny name */
	constexpr int kRareCount   = (int)(sizeof(kRareNames)   / sizeof(kRareNames[0]));
	constexpr int kSuffixCount = (int)(sizeof(kNameSuffixes) / sizeof(kNameSuffixes[0]));

	std::string chosen;

	if (InteractiveRandomRange(100) < 5 && (int)_ap_used_rare.size() < kRareCount) {
		/* Rare name — no suffix, no repeats */
		int ri = InteractiveRandomRange(kRareCount);
		int tries = kRareCount;
		while (_ap_used_rare.count(ri) && tries-- > 0) ri = (ri + 1) % kRareCount;
		if (!_ap_used_rare.count(ri)) {
			_ap_used_rare.insert(ri);
			chosen = kRareNames[ri];
		}
	}
	if (chosen.empty()) {
		/* Draw from community pool (no-repeat until exhausted) */
		if (_ap_name_pool.empty()) RebuildNamePool();
		int idx = _ap_name_pool.back();
		_ap_name_pool.pop_back();
		chosen = kCommunityNames[idx];

		/* ~60% chance: append a suffix (round-robin to spread them evenly) */
		if (InteractiveRandomRange(100) < 60) {
			chosen += kNameSuffixes[_ap_suffix_index % kSuffixCount];
			_ap_suffix_index++;
		}
	}

	v->name = chosen;
}

/* Save/load helpers for name pool persistence */
std::string AP_GetNamePoolStr()
{
	std::string s;
	s += fmt::format("{}", _ap_suffix_index);
	for (int idx : _ap_name_pool) {
		s += ',';
		s += fmt::format("{}", idx);
	}
	return s;
}

void AP_SetNamePoolStr(const std::string &s)
{
	_ap_name_pool.clear();
	_ap_suffix_index = 0;
	if (s.empty()) return;

	const char *p   = s.data();
	const char *end = p + s.size();

	/* First value = suffix index */
	auto [p2, ec] = std::from_chars(p, end, _ap_suffix_index);
	if (ec != std::errc()) return;
	p = p2;
	if (p < end && *p == ',') p++;

	/* Remaining values = pool indices */
	while (p < end) {
		int idx = 0;
		auto [p3, ec2] = std::from_chars(p, end, idx);
		if (ec2 != std::errc()) break;
		_ap_name_pool.push_back(idx);
		p = p3;
		if (p < end && *p == ',') p++;
	}
}
