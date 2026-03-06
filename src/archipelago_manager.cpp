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
#include "archipelago.h"
#include "archipelago_gui.h"
#include "engine_base.h"
#include "engine_func.h"
#include "engine_type.h"
#include "company_func.h"
#include "company_base.h"
#include "vehicle_base.h"
#include "industry.h"
#include "town.h"
#include "town_type.h"
#include "cargotype.h"
#include "core/random_func.hpp"
#include "rail.h"
#include "road_func.h"
#include "window_type.h"
#include "language.h"
#include "genworld.h"
#include "settings_type.h"
#include "debug.h"
#include "strings_func.h"
#include "news_type.h"
#include "news_func.h"
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
#include "window_func.h"
#include "station_base.h"
#include "safeguards.h"

/* Console log helpers */
#define AP_LOG(msg)  IConsolePrint(CC_INFO,    "[AP] " + std::string(msg))
#define AP_OK(msg)   IConsolePrint(CC_WHITE,   "[AP] " + std::string(msg))
#define AP_WARN(msg) IConsolePrint(CC_WARNING, "[AP] WARNING: " + std::string(msg))
#define AP_ERR(msg)  IConsolePrint(CC_ERROR,   "[AP] ERROR: " + std::string(msg))

/* -------------------------------------------------------------------------
 * Cargo type lookup — maps AP cargo names (English) → CargoType index
 * Used by mission evaluation to check delivered_cargo[] by cargo type.
 * ---------------------------------------------------------------------- */

static std::map<std::string, CargoType> _ap_cargo_map;
static bool _ap_cargo_map_built = false;

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
	_ap_cumul_profit      = 0;
	_ap_snap_profit       = 0;
	_ap_stats_initialized = false;
	_ap_shop_purchased    = false;
	_ap_sent_shop_locations.clear();
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
		_ap_snap_profit       = c->old_economy[0].income - c->old_economy[0].expenses;
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
		Money new_p = c->old_economy[0].income - c->old_economy[0].expenses;
		if (new_p != _ap_snap_profit) refreshed = true;
	}

	if (refreshed) {
		for (CargoType i = 0; i < NUM_CARGO; i++) {
			_ap_cumul_cargo[i] += c->old_economy[0].delivered_cargo[i];
			_ap_snap_cargo[i]   = c->old_economy[0].delivered_cargo[i];
		}
		Money period_profit = c->old_economy[0].income - c->old_economy[0].expenses;
		_ap_cumul_profit += period_profit;
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
	Money cur = (c != nullptr) ? Money(c->cur_economy.income - c->cur_economy.expenses) : Money(0);
	return _ap_cumul_profit + cur;
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
static int AP_CountStations()
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Station *st : Station::Iterate()) {
		if (st->owner == cid) count++;
	}
	return count;
}

/**
 * Count primary vehicles of a given type owned by local company.
 * type == VEH_INVALID means all types.
 */
static int AP_CountVehicles(VehicleType type)
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == cid && v->IsPrimaryVehicle()) {
			if (type == VEH_INVALID || v->type == type) count++;
		}
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
		/* Monthly: income – expenses of last completed period */
		current = (int64_t)(c->old_economy[0].income - c->old_economy[0].expenses);
	}
	else if (m.type == "earn" ||
	         m.type == "earn \xC2\xA3" ||
	         m.type == "earn (total)" || m.type == "earn total") {
		/* Cumulative total profit */
		current = (int64_t)AP_GetTotalProfit();
	}

	/* ── "have" type ───────────────────────────────────────────────────
	 * "Have {amount} vehicles/trains/aircraft/road vehicles running simultaneously" */
	else if (m.type == "have") {
		VehicleType vtype = AP_VehicleTypeFromUnit(m.unit);
		current = (int64_t)AP_CountVehicles(vtype);
	}

	/* ── "service" type ────────────────────────────────────────────────
	 * "Service {amount} different towns" */
	else if (m.type == "service") {
		current = (int64_t)AP_CountServicedTowns();
	}

	/* ── "build" type ──────────────────────────────────────────────────
	 * "Build {amount} stations" */
	else if (m.type == "build") {
		current = (int64_t)AP_CountStations();
	}

	/* ── "deliver" type ────────────────────────────────────────────────
	 * "Deliver {amount} tons of {cargo} to one station"
	 * "Deliver {amount} tons of goods in one year"
	 * Approximated by cumulative cargo of that type. */
	else if (m.type == "deliver") {
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

	/* ── "connect" type ────────────────────────────────────────────────
	 * "Connect {amount} cities with rail"
	 * Approximated by towns with at least one rail station. */
	else if (m.type == "connect") {
		/* Count towns that have at least one station with train facilities */
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

	/* ── "maintain..." type ────────────────────────────────────────────
	 * "Maintain 75%/90%+ station rating for {amount} months"
	 * m.amount = number of consecutive months required.
	 * m.maintain_months_ok = counter, incremented monthly by the calendar timer
	 *   if ALL rated stations held the threshold that month, reset to 0 otherwise.
	 * Here we just expose the current counter as the live progress value. */
	else if (m.type.find("maintain") != std::string::npos) {
		current = (int64_t)m.maintain_months_ok;
	}

	/* ── "buy a vehicle from the shop" ────────────────────────────────
	 * Triggered when player buys anything from the AP shop. */
	else if (m.type == "buy a vehicle from the shop" || m.type == "buy") {
		current = _ap_shop_purchased ? 1 : 0;
	}

	/* Update live progress on the mission (visible in missions window) */
	m.current_value = current;

	return current >= m.amount;
}

/**
 * Iterate all incomplete missions and send checks for any that are now met.
/* -------------------------------------------------------------------------
 * Stored connection credentials (for reconnect)
 * ---------------------------------------------------------------------- */

std::string _ap_last_host;
uint16_t    _ap_last_port     = 38281;
std::string _ap_last_slot;
std::string _ap_last_pass;
bool        _ap_last_ssl = false;

/* -------------------------------------------------------------------------
 * Engine name <-> EngineID map
 * Built once when we enter GM_NORMAL for the first time in a session.
 * Used only for name-based lookups when AP sends us an item.
 * ---------------------------------------------------------------------- */

static std::map<std::string, EngineID> _ap_engine_map;
static bool _ap_engine_map_built = false;

static void BuildEngineMap()
{
	_ap_engine_map.clear();
	for (const Engine *e : Engine::Iterate()) {
		/* Always use the English engine name regardless of the player's
		 * language setting.  AP item names are always English, so the
		 * lookup key must also be English.
		 * GetString() uses _current_language; we temporarily switch to
		 * en_GB, build the string, then restore — or we rely on the fact
		 * that we force English at session-start (see below).
		 * Using EngineNameContext::PurchaseList gives the same name that
		 * the APWorld's items.py was generated from. */
		std::string name = GetString(STR_ENGINE_NAME,
		    PackEngineNameDParam(e->index, EngineNameContext::PurchaseList));
		if (!name.empty()) {
			_ap_engine_map[name] = e->index;
		}
	}
	_ap_engine_map_built = true;
	Debug(misc, 1, "[AP] Engine map built: {} engines", _ap_engine_map.size());
	AP_LOG(fmt::format("Engine map built: {} engines indexed", _ap_engine_map.size()));
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
	return _ap_client != nullptr &&
	       _ap_client->GetState() == APState::AUTHENTICATED;
}

/* -------------------------------------------------------------------------
 * AP_UnlockEngineByName
 * Calls OpenTTD's own EnableEngineForCompany() which handles:
 *   - company_avail.Set(company)
 *   - avail_railtypes / avail_roadtypes update
 *   - AddRemoveEngineFromAutoreplaceAndBuildWindows()
 *   - Toolbar invalidation
 * ---------------------------------------------------------------------- */

static bool AP_UnlockEngineByName(const std::string &name)
{
	CompanyID cid = _local_company;
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
	auto alias_it = aliases.find(name);
	if (alias_it != aliases.end()) {
		resolved = alias_it->second;
		Debug(misc, 1, "[AP] Engine alias: '{}' → '{}'", name, resolved);
	}

	auto it = _ap_engine_map.find(resolved);
	if (it == _ap_engine_map.end()) {
		Debug(misc, 1, "[AP] UnlockEngine: '{}' (resolved: '{}') not found in engine map",
		      name, resolved);
		return false;
	}

	Engine *e = Engine::GetIfValid(it->second);
	if (e == nullptr) return false;

	/* Set EngineFlag::Available so the engine appears in the build-vehicle list.
	 * EnableEngineForCompany() only sets company_avail, but the build-vehicle
	 * window also checks EngineFlag::Available before showing any engine. */
	e->flags.Set(EngineFlag::Available);
	EnableEngineForCompany(it->second, cid);
	/* Explicitly invalidate the build-vehicle window so the new engine shows up */
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	Debug(misc, 0, "[AP] Engine unlocked: {}", resolved);
	return true;
}

/* -------------------------------------------------------------------------
 * In-game news/chat notification
 * ---------------------------------------------------------------------- */

static void AP_ShowNews(const std::string &text)
{
	IConsolePrint(CC_INFO, "[AP] " + text);
	Debug(misc, 0, "[AP] {}", text);

	if (_game_mode == GM_NORMAL) {
		AddNewsItem(
			GetEncodedString(STR_ARCHIPELAGO_NEWS, text),
			NewsType::General,
			NewsStyle::Small,
			{}
		);
	}
}

/* -------------------------------------------------------------------------
 * Pending / deferred state
 * ---------------------------------------------------------------------- */

static APSlotData  _ap_pending_sd;
static bool        _ap_pending_world_start         = false;
static bool        _ap_goal_sent                   = false;
static bool        _ap_session_started             = false; ///< True once we've done first-tick setup in GM_NORMAL
static int         _ap_fuel_shortage_ticks         = 0;     ///< >0 while fuel shortage slowdown is active
static int         _ap_cargo_bonus_ticks           = 0;     ///< >0 while 2x cargo payment is active (240 ticks = 60s)
static int         _ap_reliability_boost_ticks     = 0;     ///< >0 while reliability boost active (90 game-days)
static int         _ap_station_boost_ticks         = 0;     ///< >0 while station rating boost active (30 game-days)

bool AP_GetCargoBonusActive() { return _ap_cargo_bonus_ticks > 0; }

/* Bug B fix: reset per-connection, not global-ever */
static bool        _ap_world_started_this_session  = false;

/* Items received before we've entered GM_NORMAL are queued here */
static std::vector<APItem> _ap_pending_items;

/* Exposed for GUI polling */
std::atomic<bool> _ap_status_dirty{ false };

/* Public accessors */
const APSlotData &AP_GetSlotData() { return _ap_pending_sd; }
bool              AP_IsConnected()  { return _ap_client != nullptr &&
                                     _ap_client->GetState() == APState::AUTHENTICATED; }

/* -------------------------------------------------------------------------
 * Callbacks
 * ---------------------------------------------------------------------- */

static void AP_OnSlotData(const APSlotData &sd)
{
	AP_OK("[CALLBACK] AP_OnSlotData called on main thread!");
	Debug(misc, 1, "[AP] Slot data received. {} missions, win={} target={}, start_year={}",
	      sd.missions.size(), (int)sd.win_condition, sd.win_condition_value, sd.start_year);

	_ap_pending_sd      = sd;
	_ap_session_started = false; /* reset so first-tick setup runs again */
	_ap_goal_sent       = false;
	_ap_engine_map_built = false; /* rebuild map for new session */
	_ap_cargo_map_built  = false; /* rebuild cargo map for new session */
	_ap_status_dirty.store(true);

	/* Only auto-start world if we're on the main menu and haven't started yet */
	if (_game_mode == GM_MENU && !_ap_world_started_this_session) {
		_ap_world_started_this_session = true;
		_ap_pending_world_start        = true;
		AP_OK(fmt::format("Slot data ready — scheduling world generation (year={}, map={}x{})",
		      sd.start_year, (1 << sd.map_x), (1 << sd.map_y)));
	} else {
		AP_ERR(fmt::format("[OnSlotData] World start SKIPPED: game_mode={} world_started={}",
		      (int)_game_mode, _ap_world_started_this_session));
		ShowArchipelagoStatusWindow();
	}
}

static void AP_OnItemReceived(const APItem &item)
{
	Debug(misc, 1, "[AP] Item received: '{}' (id={})", item.item_name, item.item_id);

	/* Queue items that arrive before we've entered GM_NORMAL */
	if (!_ap_session_started) {
		_ap_pending_items.push_back(item);
		return;
	}

	if (item.item_name.empty()) {
		Debug(misc, 1, "[AP] WARNING: empty item name for id {}", item.item_id);
		return;
	}

	/* Vehicle unlock — delegate entirely to EnableEngineForCompany */
	if (AP_UnlockEngineByName(item.item_name)) {
		AP_ShowNews("[AP] Unlocked: " + item.item_name);
		return;
	}

	/* Trap / utility items */
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;

	/* ── TRAPS ─────────────────────────────────────────── */
	if (item.item_name == "Breakdown Wave") {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == cid && v->IsPrimaryVehicle()) {
				v->breakdown_chance = 255;
				v->reliability = 1;
			}
		}
		AP_ShowNews("[AP] TRAP: Breakdown Wave hit!");
	} else if (item.item_name == "Recession") {
		c->money = c->money / 2;
		AP_ShowNews("[AP] TRAP: Recession! Money halved.");
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
		{
			int64_t fl = (int64_t)forced_loan;
			std::string loan_str = (fl >= 1000000)
			    ? fmt::format("\xC2\xA3{:.1f}M", fl / 1000000.0)
			    : fmt::format("\xC2\xA3{}k",     fl / 1000);
			AP_ShowNews("[AP] TRAP: Bank Loan Forced! +" + loan_str);
		}
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
			victim = active_industries[RandomRange((uint32_t)active_industries.size())];
		} else {
			/* Fallback: any random industry */
			int count = 0;
			for ([[maybe_unused]] Industry *ind : Industry::Iterate()) { count++; }
			if (count > 0) {
				int target_i = RandomRange(count), i = 0;
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

	/* ── UTILITY ITEMS ─────────────────────────────────── */
	} else if (item.item_name == "Cash Injection £50,000") {
		c->money += (Money)50000LL;
		AP_ShowNews("[AP] Bonus: +£50,000!");
	} else if (item.item_name == "Cash Injection £200,000") {
		c->money += (Money)200000LL;
		AP_ShowNews("[AP] Bonus: +£200,000!");
	} else if (item.item_name == "Cash Injection £500,000") {
		c->money += (Money)500000LL;
		AP_ShowNews("[AP] Bonus: +£500,000!");
	} else if (item.item_name == "Loan Reduction £100,000") {
		Money reduce = (Money)100000LL;
		c->current_loan = (c->current_loan > reduce) ? Money(c->current_loan - reduce) : Money(0);
		AP_ShowNews("[AP] Bonus: Loan reduced by £100,000!");
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
		/* Accelerate all town growth timers */
		for (Town *t : Town::Iterate()) {
			t->grow_counter = 0;
			if (t->growth_rate > 0 && t->growth_rate != TOWN_GROWTH_RATE_NONE)
				t->growth_rate = static_cast<uint16_t>(std::max(1, (int)t->growth_rate / 2));
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
		c->money += (Money)100000LL;
		AP_ShowNews("[AP] Bonus: +£100,000!");
	} else {
		AP_WARN("Unknown item: '" + item.item_name + "' — not handled");
	}
}

static void AP_OnConnected()
{
	Debug(misc, 1, "[AP] Connected to Archipelago server.");
	AP_OK("Connected to Archipelago server");
	/* Force English immediately on connect so any subsequent string lookups
	 * (including engine name map building) use English names. */
	ForceEnglishLanguage();
	_ap_status_dirty.store(true);
}

static void AP_OnDisconnected(const std::string &reason)
{
	Debug(misc, 1, "[AP] Disconnected: {}", reason);
	AP_WARN("Disconnected: " + reason);
	_ap_world_started_this_session = false;
	_ap_pending_world_start = false;
	_ap_session_started = false;
	AP_LOG("Session flags reset — next connect can start world");
	_ap_status_dirty.store(true);
}

static void AP_OnPrint(const std::string &msg)
{
	Debug(misc, 0, "[AP] Server: {}", msg);
	AP_ShowNews("[AP] " + msg);
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
	Debug(misc, 0, "[AP] Death sent: {}", cause);
}

/* -------------------------------------------------------------------------
 * Death Link — inbound: find an industry connected to the player's stations
 * and shut it down as the "death" penalty.
 * ---------------------------------------------------------------------- */

static void AP_OnDeathReceived(const std::string &source)
{
	if (!_ap_session_started) return;
	if (_ap_death_cooldown_ticks > 0) {
		Debug(misc, 0, "[AP] Death from {} ignored — cooldown active", source);
		return;
	}
	/* Start 30-second cooldown (120 ticks × 250 ms) */
	_ap_death_cooldown_ticks = 120;

	CompanyID cid = _local_company;

	/* Step 1 — Build a list of industries actively serviced by the player.
	 * An industry is "active" if at least one of the player's stations has a
	 * GoodsEntry rating for a cargo that this industry produces. */
	std::vector<Industry *> active_industries;

	for (Station *st : Station::Iterate()) {
		if (st->owner != cid) continue;
		for (Industry *ind : Industry::Iterate()) {
			/* Check if industry is within station catchment area */
			if (ind->location.tile == INVALID_TILE) continue;
			bool already_added = false;
			for (Industry *ai : active_industries) { if (ai == ind) { already_added = true; break; } }
			if (already_added) continue;

			/* Check if any cargo from this industry has a rating at this station */
			for (const auto &produced : ind->produced) {
				if (!IsValidCargoType(produced.cargo)) continue;
				if (produced.cargo >= NUM_CARGO) continue;
				const GoodsEntry &ge = st->goods[produced.cargo];
				if (ge.HasRating()) {
					active_industries.push_back(ind);
					break;
				}
			}
		}
	}

	if (!active_industries.empty()) {
		/* Step 2 — Pick a random active industry and halt it */
		size_t idx = RandomRange((uint32_t)active_industries.size());
		Industry *victim = active_industries[idx];

		/* Zero out production on all outputs */
		for (auto &produced : victim->produced) {
			produced.history[THIS_MONTH].production = 0;
		}
		victim->prod_level = 0;

		/* Find the town name for the news message */
		const Town *t = victim->town;
		std::string town_name = (t != nullptr) ? "near " + std::string(t->name) : "";
		/* Large newspaper-style news for death events so player can't miss it */
		AddNewsItem(
			GetEncodedString(STR_ARCHIPELAGO_NEWS,
			    fmt::format("DEATH LINK from {}! {} industry shut down!", source, town_name)),
			NewsType::General,
			NewsStyle::Normal,
			{}
		);
		IConsolePrint(CC_ERROR, fmt::format("[AP] DEATH LINK from {}! Industry closure {}!", source, town_name));
		Debug(misc, 0, "[AP] Death received from {} — shut down industry {}", source, victim->index.base());
	} else {
		/* Fallback: no active industries yet — take a money penalty */
		Company *c = Company::GetIfValid(cid);
		if (c != nullptr) {
			c->money -= (Money)std::max((int64_t)50000LL, (int64_t)(c->money / 10));
		}
		AddNewsItem(
			GetEncodedString(STR_ARCHIPELAGO_NEWS,
			    fmt::format("DEATH LINK from {}! Emergency costs drained your funds!", source)),
			NewsType::General,
			NewsStyle::Normal,
			{}
		);
		IConsolePrint(CC_ERROR, fmt::format("[AP] DEATH LINK from {}! Emergency costs drained your funds!", source));
		Debug(misc, 0, "[AP] Death received from {} — no active industries, money penalty applied", source);
	}
}

/* -------------------------------------------------------------------------
 * Handler registration
 * ---------------------------------------------------------------------- */

static bool _handlers_registered = false;

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
}

/* -------------------------------------------------------------------------
 * Win-condition check
 * ---------------------------------------------------------------------- */

static bool CheckWinCondition(const APSlotData &sd)
{
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return false;
	const Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return false;

	int64_t current = 0;
	switch (sd.win_condition) {
		case APWinCondition::COMPANY_VALUE:
			current = (int64_t)c->old_economy[0].company_value;
			break;
		case APWinCondition::MONTHLY_PROFIT:
			current = (int64_t)c->old_economy[0].income - (int64_t)c->old_economy[0].expenses;
			break;
		case APWinCondition::VEHICLE_COUNT: {
			int count = 0;
			for (const Vehicle *v : Vehicle::Iterate())
				if (v->owner == cid && v->IsPrimaryVehicle()) count++;
			current = count;
			break;
		}
		case APWinCondition::TOWN_POPULATION:
			/* Sum population of all towns on the map */
			current = (int64_t)GetWorldPopulation();
			break;
		case APWinCondition::CARGO_DELIVERED: {
			/* Sum all cumulative cargo delivered across all types this session.
			 * Uses the same accumulator as mission evaluation — NOT old_economy[0]
			 * which only covers one quarter. */
			uint64_t total = 0;
			for (uint i = 0; i < NUM_CARGO; i++)
				total += AP_GetTotalCargo((CargoType)i);
			current = (int64_t)total;
			break;
		}
		default:
			current = (int64_t)c->old_economy[0].company_value;
			break;
	}
	return current >= sd.win_condition_value;
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
	/* max_train_length and station_spread are uint16_t in our patched
	 * settings_type.h — no truncation occurs. */
	_settings_newgame.vehicle.max_train_length         = sd.max_train_length;
	_settings_newgame.station.station_spread           = sd.station_spread;
	_settings_newgame.construction.road_stop_on_town_road       = sd.road_stop_on_town_road;
	_settings_newgame.construction.road_stop_on_competitor_road = sd.road_stop_on_competitor_road;
	_settings_newgame.construction.crossing_with_competitor     = sd.crossing_with_competitor;

	/* ── Disasters / Accidents ───────────────────────────────────── */
	_settings_newgame.difficulty.disasters             = sd.disasters;
	_settings_newgame.vehicle.plane_crashes            = sd.plane_crashes;
	_settings_newgame.difficulty.vehicle_breakdowns    = sd.vehicle_breakdowns;

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

	/* ── Vehicles / Routing ──────────────────────────────────────── */
	_settings_newgame.vehicle.road_side                = sd.road_side;

	Debug(misc, 0, "[AP] World start ready: seed={}, year={}, map={}x{}, landscape={}",
	      _ap_world_seed_to_use, sd.start_year,
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape);
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
	_ap_client->SendCheckByName(location_name);
}

int AP_GetShopSlots()
{
	return AP_GetSlotData().shop_slots;
}

int AP_GetShopRefreshDays()
{
	return AP_GetSlotData().shop_refresh_days;
}

std::string AP_GetShopLocationLabel(const std::string &location_name)
{
	if (_ap_client == nullptr) return location_name;
	return _ap_client->GetLocationHint(location_name);
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

void AP_DeductShopPrice(const std::string &location_name)
{
	CompanyID cid = _local_company;
	Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;
	int64_t price = AP_GetShopPrice(location_name);
	c->money -= (Money)price;
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
 * Shop Refresh — calendar day timer
 * Every shop_refresh_days in-game days, cycle the visible shop slots.
 * Each refresh shifts the window of visible slots forward by shop_slots
 * positions (wrapping around the full pool of shop_slots * 20 locations).
 * ---------------------------------------------------------------------- */

static int _ap_shop_day_counter  = 0;   ///< Days elapsed since last shop refresh
static int _ap_shop_page_offset  = 0;   ///< Current page offset into the full shop list

/* ── Savegame accessor functions (used by archipelago_sl.cpp) ────── */
int  AP_GetShopPageOffset()  { return _ap_shop_page_offset; }
void AP_SetShopPageOffset(int v) { _ap_shop_page_offset = v; }
int  AP_GetShopDayCounter()  { return _ap_shop_day_counter; }
void AP_SetShopDayCounter(int v) { _ap_shop_day_counter = v; }
bool AP_GetGoalSent()        { return _ap_goal_sent; }
void AP_SetGoalSent(bool v)  { _ap_goal_sent = v; }

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
    if (s.empty()) return;
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
}

void AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out)
{
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        cargo_out[i] = _ap_cumul_cargo[i];
    *profit_out = (int64_t)_ap_cumul_profit;
}

void AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in)
{
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        _ap_cumul_cargo[i] = cargo_in[i];
    _ap_cumul_profit = (Money)profit_in;
    _ap_stats_initialized = true;
}

/* Returns "location=N,location=N,..." for all maintain missions with N>0 */
std::string AP_GetMaintainCountersStr()
{
    std::string out;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (m.type.find("maintain") == std::string::npos) continue;
        if (m.maintain_months_ok == 0) continue;
        if (!out.empty()) out += ',';
        out += m.location + '=' + fmt::format("{}", m.maintain_months_ok);
    }
    return out;
}

void AP_SetMaintainCountersStr(const std::string &s)
{
    if (s.empty()) return;
    /* Parse "loc=N,loc=N,..." */
    std::string token;
    auto apply = [&](const std::string &t) {
        auto eq = t.find('=');
        if (eq == std::string::npos) return;
        std::string loc = t.substr(0, eq);
        int n = 0;
        for (char c : t.substr(eq + 1)) {
            if (c >= '0' && c <= '9') n = n * 10 + (c - '0');
        }
        for (APMission &m : _ap_pending_sd.missions) {
            if (m.location == loc) { m.maintain_months_ok = n; break; }
        }
    };
    for (char c : s) {
        if (c == ',') { apply(token); token.clear(); }
        else token += c;
    }
    apply(token);
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
			if (m.type.find("maintain") == std::string::npos) continue;

			/* Determine threshold: 75% → 191/255, 90% → 229/255 */
			uint8_t threshold = 191;
			if (m.type.find("90") != std::string::npos) threshold = 229;

			/* Check every rated station — ALL must pass */
			int rated_count = 0;
			for (const Station *st : Station::Iterate()) {
				if (st->owner != cid) continue;
				for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
					if (!st->goods[ct].HasRating()) continue;
					rated_count++;
					if (st->goods[ct].rating < threshold) {
						/* This station failed — reset and stop checking */
						m.maintain_months_ok = 0;
						goto next_mission;
					}
				}
			}
			/* Only count progress if player actually has rated stations */
			if (rated_count > 0) {
				m.maintain_months_ok++;
				Debug(misc, 1, "[AP] Maintain mission '{}': {}/{} months OK",
				      m.location, m.maintain_months_ok, m.amount);
			}
			next_mission:;
		}
	}
);

static const IntervalTimer<TimerGameCalendar> _ap_calendar_shop_refresh(
	{ TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE },
	[](auto) {
		if (!_ap_session_started) return;
		const APSlotData &sd = AP_GetSlotData();
		if (sd.shop_refresh_days <= 0) return;

		_ap_shop_day_counter++;
		if (_ap_shop_day_counter >= sd.shop_refresh_days) {
			_ap_shop_day_counter = 0;
			int total_slots = sd.shop_slots * 20;
			if (total_slots > 0) {
				_ap_shop_page_offset = (_ap_shop_page_offset + sd.shop_slots) % total_slots;
				/* Mark shop window dirty so it rebuilds its list */
				SetWindowClassesDirty(WC_ARCHIPELAGO);
				Debug(misc, 0, "[AP] Shop refreshed — new page offset: {}", _ap_shop_page_offset);
			}
		}
	}
);

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
static void CheckMissions()
{
	if (_ap_client == nullptr) return;
	if (!_ap_session_started) return;

	int completed_this_pass = 0;
	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed) continue;
		if (EvaluateMission(m)) {
			m.completed = true;
			completed_this_pass++;
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
		if (_ap_client == nullptr) return;
		EnsureHandlersRegistered();

		/* Dispatch inbound AP events */
		_ap_client->Tick();

		/* First-tick session setup when we enter GM_NORMAL */
		if (!_ap_session_started &&
		    _game_mode == GM_NORMAL &&
		    _local_company < MAX_COMPANIES &&
		    _ap_client->GetState() == APState::AUTHENTICATED) {

			_ap_session_started = true;

			CompanyID cid = _local_company;
			Company *c = Company::GetIfValid(cid);

			/* Force English so that engine names match AP item names */
			ForceEnglishLanguage();

			/* Build the engine name → ID lookup map (uses current language = English) */
			BuildEngineMap();

			/* Build the cargo name → type map for mission evaluation */
			BuildCargoMap();

			/* Reset session statistics for mission evaluation */
			AP_InitSessionStats();

			/* AP settings: never expire vehicles or airports — ensures all
			 * AP-unlocked engines/airports remain usable regardless of in-game year. */
			_settings_game.vehicle.never_expire_vehicles = true;
			_settings_game.station.never_expire_airports = true;

			/* Strip the local company from every ENGINE that was auto-unlocked
			 * by StartupOneEngine() at game start.
			 * WAGONS are excluded — they are always freely available so the
			 * player can use any locomotive they receive from AP. */
			int locked_count = 0;
			for (Engine *e : Engine::Iterate()) {
				if (!e->company_avail.Test(cid)) continue;

				/* Keep wagons always available */
				bool is_wagon = (e->type == VEH_TRAIN &&
				                 e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON);
				if (is_wagon) continue;

				e->company_avail.Reset(cid);
				locked_count++;
			}

			/* Unlock ALL rail and road types unconditionally — ensures any
			 * AP-randomised engine's track/road type is always buildable. */
			if (c != nullptr) {
				c->avail_railtypes = GetRailTypes(true);
				c->avail_roadtypes = GetRoadTypes(true);
			}

			InvalidateWindowClassesData(WC_BUILD_VEHICLE);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
			AP_OK(fmt::format("AP session started: {} engines locked, all railtypes/roadtypes unlocked.", locked_count));

			/* Unlock all starting vehicles.
			 * For one_of_each mode this is one per transport type.
			 * For normal modes it is a single vehicle. */
			for (const std::string &sv : _ap_pending_sd.starting_vehicles) {
				if (sv.empty()) continue;
				if (AP_UnlockEngineByName(sv)) {
					AP_OK("Starting vehicle unlocked: " + sv);
					AP_ShowNews("[AP] Starting vehicle: " + sv);
				} else {
					AP_WARN("Starting vehicle '" + sv + "' not found in engine map!");
				}
			}

			/* Flush items that arrived before GM_NORMAL */
			for (const APItem &item : _ap_pending_items) AP_OnItemReceived(item);
			_ap_pending_items.clear();

			/* Open the status overlay */
			ShowArchipelagoStatusWindow();

			AP_OK(fmt::format("AP session started. {} engines in map. Mission evaluation active.",
			      _ap_engine_map.size()));
		}

		/* Every ~5 s (20 × 250 ms): update economy stats and check missions.
		 * Every ~10 s (40 × 250 ms): also check win condition. */
		_ap_realtime_ticks++;

		/* Tick down cooldowns every 250 ms */
		if (_ap_death_cooldown_ticks > 0) _ap_death_cooldown_ticks--;

		/* Fuel Shortage: re-apply speed cap every tick while active */
		if (_ap_fuel_shortage_ticks > 0) {
			_ap_fuel_shortage_ticks--;
			if (_ap_session_started && _game_mode == GM_NORMAL) {
				CompanyID cid = _local_company;
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
		}

		/* Cargo Bonus: tick down 2x payment multiplier */
		if (_ap_cargo_bonus_ticks > 0) _ap_cargo_bonus_ticks--;

		/* Reliability Boost: re-apply max reliability every tick to counter decay */
		if (_ap_reliability_boost_ticks > 0) {
			_ap_reliability_boost_ticks--;
			if (_ap_session_started && _game_mode == GM_NORMAL) {
				CompanyID cid = _local_company;
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
		}

		/* Station Boost: re-apply MAX_STATION_RATING to all player stations */
		if (_ap_station_boost_ticks > 0) {
			_ap_station_boost_ticks--;
			if (_ap_session_started && _game_mode == GM_NORMAL) {
				CompanyID cid = _local_company;
				for (Station *st : Station::Iterate()) {
					if (st->owner != cid) continue;
					for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
						if (st->goods[ct].HasRating()) {
							st->goods[ct].rating = MAX_STATION_RATING;
						}
					}
				}
			}
		}

		if (_ap_realtime_ticks % 20 == 0 &&
		    _ap_session_started &&
		    _game_mode == GM_NORMAL) {

			/* Accumulate cargo/profit from completed economy periods */
			AP_UpdateSessionStats();

			/* Evaluate all incomplete missions */
			CheckMissions();
		}

		if (_ap_realtime_ticks >= 40 &&
		    _ap_session_started && !_ap_goal_sent &&
		    _game_mode == GM_NORMAL) {

			_ap_realtime_ticks = 0;
			if (CheckWinCondition(_ap_pending_sd)) {
				_ap_goal_sent = true;
				_ap_client->SendGoal();
				Debug(misc, 0, "[AP] Win condition reached! Goal sent.");
				AP_OK("*** WIN CONDITION REACHED! Goal sent to server! ***");
				AP_ShowNews("[AP] WIN CONDITION REACHED! Goal sent to server!");
			}
		}
	}
);
