/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#include "stdafx.h"
#include "archipelago.h"
#include "debug.h"
#include <cmath>

#include "3rdparty/nlohmann/json.hpp"
using json = nlohmann::json;

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define SOCK_INVALID INVALID_SOCKET
#  define sock_close(s) closesocket(s)
#  define sock_err()    WSAGetLastError()
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
   typedef int sock_t;
#  define SOCK_INVALID (-1)
#  define sock_close(s) ::close(s)
#  define sock_err()    errno
#endif

#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstring>
#ifdef WITH_ZLIB
#  include <zlib.h>
#endif

/* =========================================================================
 * Windows Schannel TLS wrapper
 * Used when use_ssl=true so the WebSocket layer operates over wss://.
 * Requires Secur32.lib (already linked by MSVC on Windows).
 * ========================================================================= */
#ifdef _WIN32
#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#pragma comment(lib, "Secur32.lib")

#include "console_func.h"
#include "core/string_consumer.hpp"

#include "safeguards.h"

/* Console output. These lived in the block that carried the TLS backends,
 * so they came out with it -- they belong here, not there. */
#define AP_LOG(msg)  IConsolePrint(CC_INFO,    "[AP] " + std::string(msg))
#define AP_OK(msg)   IConsolePrint(CC_WHITE,   "[AP] " + std::string(msg))
#define AP_WARN(msg) IConsolePrint(CC_WARNING, "[AP] WARNING: " + std::string(msg))
#define AP_ERR(msg)  IConsolePrint(CC_ERROR,   "[AP] ERROR: " + std::string(msg))

/* The one client. Declared extern in archipelago.h; every window and the
 * manager reach it through this. */
ArchipelagoClient *_ap_client = nullptr;

void InitArchipelago()
{
	if (_ap_client == nullptr) _ap_client = new ArchipelagoClient();
}

void UninitArchipelago()
{
	delete _ap_client;
	_ap_client = nullptr;
}

/* -------------------------------------------------------------------------
 * ArchipelagoClient
 * ---------------------------------------------------------------------- */

ArchipelagoClient::ArchipelagoClient() = default;

ArchipelagoClient::~ArchipelagoClient()
{
	Disconnect();
}

void ArchipelagoClient::Connect(const std::string &h, uint16_t p,
                                const std::string &slot, const std::string &pw,
                                const std::string &game, bool ssl)
{
	Disconnect();

	host      = h;
	port      = p;
	slot_name = slot;
	password  = pw;
	game_name = game;
	use_ssl   = ssl;

	has_slot_data.store(false);

	stop_requested.store(false);
	state.store(APState::CONNECTING);

	/* The worker reports the GRF list as its first act, so the snapshot has to
	 * exist before it starts -- this is the main thread, the worker is not. */
	AP_PublishGrfSnapshot();

	worker_thread = std::thread(&ArchipelagoClient::WorkerThread, this);
}

void ArchipelagoClient::Disconnect()
{
	stop_requested.store(true);
	if (worker_thread.joinable()) worker_thread.join();
	state.store(APState::DISCONNECTED);
}

/* --- Outbound: one line each, per docs/ap_pipe_protocol.md ---------------
 *
 * These used to build Archipelago JSON packets. The launcher holds the AP
 * connection now, so the game only reports what happened and the launcher
 * decides what that means on the wire.
 */

void ArchipelagoClient::SendCheck(int64_t location_id)
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ fmt::format("CHECK:{}", location_id) });
}

void ArchipelagoClient::SendCheckByName(const std::string &location_name)
{
	/* The launcher owns the name->id table, so the name goes out as-is. This
	 * used to try an id first from a table the game filled from the server;
	 * with the server on the other side of the pipe, nothing filled it. */
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ fmt::format("CHECKNAME:{}", location_name) });
}

void ArchipelagoClient::SendGoal()
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ "GOAL:" });
}

void ArchipelagoClient::SendScoutsForShop()
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ "SCOUT:" });
}

void ArchipelagoClient::SendSay(const std::string &text)
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ fmt::format("SAY:{}", text) });
}

void ArchipelagoClient::SendDeath(const std::string &cause)
{
	/* Stamped before sending so the echo coming back from the room can be
	 * recognised as our own and ignored. */
	last_death_link_time = std::chrono::duration<double>(
			std::chrono::system_clock::now().time_since_epoch()).count();

	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ fmt::format("DEATH:{}", cause) });
}

void ArchipelagoClient::Tick()
{
	/* Refresh the GRF snapshot the pipe worker reads. This is the main thread,
	 * which is the only one allowed near the GRF lists. */
	AP_PublishGrfSnapshot();

	std::lock_guard<std::mutex> lg(inbound_mutex);
	if (!inbound_queue.empty()) {
		AP_LOG(fmt::format("[Tick] Processing {} queued events", inbound_queue.size()));
	}
	while (!inbound_queue.empty()) {
		InboundEvent ev = std::move(inbound_queue.front());
		inbound_queue.pop_front();
		switch (ev.type) {
			case InboundEvent::CONNECTED:
				AP_LOG("[Tick] Dispatching CONNECTED event");
				if (callbacks.on_connected) callbacks.on_connected();
				else AP_ERR("[Tick] on_connected callback is NULL!");
				break;
			case InboundEvent::DISCONNECTED:
				AP_LOG(fmt::format("[Tick] Dispatching DISCONNECTED: {}", ev.text));
				if (callbacks.on_disconnected) callbacks.on_disconnected(ev.text);
				else AP_ERR("[Tick] on_disconnected callback is NULL!");
				break;
			case InboundEvent::ITEM:
				if (callbacks.on_item_received) callbacks.on_item_received(ev.item);
				break;
			case InboundEvent::PRINT:
				if (callbacks.on_print) callbacks.on_print(ev.text);
				break;
			case InboundEvent::SLOT_DATA:
				AP_LOG("[Tick] Dispatching SLOT_DATA event");
				if (callbacks.on_slot_data) callbacks.on_slot_data(ev.slot);
				else AP_ERR("[Tick] on_slot_data callback is NULL!");
				break;
			case InboundEvent::DEATH_RECEIVED:
				AP_LOG(fmt::format("[Tick] Dispatching DEATH_RECEIVED from {}", ev.text));
				if (callbacks.on_death_received) callbacks.on_death_received(ev.text);
				break;
			case InboundEvent::CHECKED_LOCATIONS:
				AP_LOG(fmt::format("[Tick] Dispatching CHECKED_LOCATIONS: {} locations", ev.locations.size()));
				if (callbacks.on_checked_locations) callbacks.on_checked_locations(ev.locations);
				break;
		}
	}
}

void ArchipelagoClient::PushEvent(InboundEvent ev)
{
	std::lock_guard<std::mutex> lg(inbound_mutex);
	inbound_queue.push_back(std::move(ev));
}

std::string ArchipelagoClient::PopOutbound()
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	if (outbound_queue.empty()) return {};
	std::string s = std::move(outbound_queue.front().json);
	outbound_queue.pop_front();
	return s;
}

/* -------------------------------------------------------------------------
 * Slot data parser
 * ---------------------------------------------------------------------- */

/* Not static: archipelago_pipe.cpp feeds it the SLOTDATA line. */
APSlotData ParseSlotData(const json &msg)
{
	APSlotData sd;

	if (!msg.contains("slot_data") || !msg["slot_data"].is_object()) {
		Debug(misc, 0, "[AP] ParseSlotData: NO slot_data field in Connected message!");
		return sd;
	}
	const json &d = msg["slot_data"];

	sd.game_version         = d.value("game_version", "15.2");
	sd.mission_count        = d.value("mission_count", 100);
	/* shop_item_count is the direct count (new). Fall back to shop_slots*20 for old saves. */
	if (d.contains("shop_item_count")) {
		sd.shop_slots = d.value("shop_item_count", 100);
	} else {
		sd.shop_slots = d.value("shop_slots", 5) * 20;
	}
	sd.starting_vehicle     = d.value("starting_vehicle", "");
	sd.starting_vehicle_type = d.value("starting_vehicle_type", "");
	/* starting_vehicles list — present for one_of_each mode.
	 * Falls back to a single-element list from starting_vehicle. */
	if (d.contains("starting_vehicles") && d["starting_vehicles"].is_array()) {
		for (const auto &v : d["starting_vehicles"]) {
			if (v.is_string()) sd.starting_vehicles.push_back(v.get<std::string>());
		}
	}
	if (sd.starting_vehicles.empty() && !sd.starting_vehicle.empty()) {
		sd.starting_vehicles.push_back(sd.starting_vehicle);
	}
	sd.enable_traps         = d.value("enable_traps", true);
	sd.start_year           = d.value("start_year", 1950);

	/* World generation parameters */
	sd.world_seed           = d.value("world_seed", (uint32_t)0);
	sd.map_x                = (uint8_t)d.value("map_x", 8);
	sd.map_y                = (uint8_t)d.value("map_y", 8);
	sd.landscape            = (uint8_t)d.value("landscape", 0);
	sd.land_generator       = (uint8_t)d.value("land_generator", 1);
	sd.terrain_type         = (uint8_t)d.value("terrain_type", 1);
	sd.sea_level            = (uint8_t)d.value("sea_level", 1);
	sd.rivers               = (uint8_t)d.value("rivers", 2);
	sd.smoothness           = (uint8_t)d.value("smoothness", 1);
	sd.variety              = (uint8_t)d.value("variety", 0);
	sd.number_towns         = (uint8_t)d.value("number_towns", 2);
	sd.town_name            = (uint8_t)d.value("town_name", 0);

	/* Win condition (multi-target — all 6 must be met simultaneously) */
	sd.win_target_company_value   = d.value("win_target_company_value",   (int64_t)8'000'000);
	sd.win_target_town_population = d.value("win_target_town_population", (int64_t)100'000);
	sd.win_target_vehicle_count   = d.value("win_target_vehicle_count",   (int64_t)30);
	sd.win_target_cargo_delivered = d.value("win_target_cargo_delivered", (int64_t)120'000);
	sd.win_target_monthly_profit  = d.value("win_target_monthly_profit",  (int64_t)100'000);
	sd.win_target_missions        = d.value("win_target_missions",        (int64_t)35);
	int diff_val = d.value("win_difficulty", 2);
	sd.win_difficulty = (diff_val >= 0 && diff_val <= 10)
	    ? static_cast<APWinDifficulty>(diff_val) : APWinDifficulty::NORMAL;

	/* ── Game settings (Accounting) ──────────────────────────────────── */
	sd.infinite_money            = d.value("infinite_money",       false);
	sd.inflation                 = d.value("inflation",            false);
	sd.max_loan                  = d.value("max_loan",             (uint32_t)300000);
	sd.infrastructure_maintenance = d.value("infrastructure_maintenance", false);
	sd.vehicle_costs             = (uint8_t)d.value("vehicle_costs",    1);
	sd.construction_cost         = (uint8_t)d.value("construction_cost", 1);

	/* ── Game settings (Vehicles / Limitations) ──────────────────────── */
	sd.max_trains                = (uint16_t)d.value("max_trains",        500);
	sd.max_roadveh               = (uint16_t)d.value("max_roadveh",       500);
	sd.max_aircraft              = (uint16_t)d.value("max_aircraft",      200);
	sd.max_ships                 = (uint16_t)d.value("max_ships",         300);
	sd.max_train_length          = (uint16_t)d.value("max_train_length",  7);
	sd.station_spread            = (uint16_t)d.value("station_spread",    12);
	sd.road_stop_on_town_road       = d.value("road_stop_on_town_road",       true);
	sd.road_stop_on_competitor_road = d.value("road_stop_on_competitor_road", true);
	sd.crossing_with_competitor     = d.value("crossing_with_competitor",     true);

	/* ── Game settings (Disasters / Accidents) ────────────────────────── */
	sd.disasters                 = d.value("disasters",            false);
	sd.plane_crashes             = (uint8_t)d.value("plane_crashes",    2);
	sd.vehicle_breakdowns        = (uint8_t)d.value("vehicle_breakdowns", 1);

	/* ── Game settings (Economy / Environment) ────────────────────────── */
	sd.economy_type              = (uint8_t)d.value("economy_type",     1);
	sd.bribe                     = d.value("bribe",                true);
	sd.exclusive_rights          = d.value("exclusive_rights",     true);
	sd.fund_buildings            = d.value("fund_buildings",       true);
	sd.fund_roads                = d.value("fund_roads",           true);
	sd.give_money                = d.value("give_money",           true);
	sd.town_growth_rate          = (uint8_t)d.value("town_growth_rate",  2);
	sd.found_town                = (uint8_t)d.value("found_town",        0);
	sd.town_cargo_scale          = (uint16_t)d.value("town_cargo_scale",    100);
	sd.industry_cargo_scale      = (uint16_t)d.value("industry_cargo_scale", 100);
	sd.industry_density          = (uint8_t)d.value("industry_density",  4);
	sd.allow_town_roads          = d.value("allow_town_roads",     true);
	sd.road_side                 = (uint8_t)d.value("road_side",         1);

	/* Death Link — from options.py; note: AP sends this as a top-level slot_data field */
	sd.death_link                = d.value("death_link",           false);
	sd.starting_cash_bonus       = d.value("starting_cash_bonus",  0);
	sd.starting_vehicle_count    = d.value("starting_vehicle_count", 1);
	sd.mission_difficulty        = d.value("mission_difficulty",   2);

	/* NewGRF options */
	sd.enable_iron_horse         = (bool)d.value("enable_iron_horse", 0);
	sd.enable_military_items     = (bool)d.value("enable_military_items", 0);
	sd.enable_shark_ships        = (bool)d.value("enable_shark_ships", 0);
	sd.enable_hover_vehicles     = (bool)d.value("enable_hover_vehicles", 0);
	sd.enable_heqs               = (bool)d.value("enable_heqs", 0);
	sd.enable_vactrain           = (bool)d.value("enable_vactrain", 0);
	sd.enable_aircraftpack       = (bool)d.value("enable_aircraftpack", 0);
	sd.enable_firs               = (bool)d.value("enable_firs", 0);
	sd.firs_economy              = (uint8_t)d.value("firs_economy", 0);

	/* Item Pool unlock options */
	sd.enable_rail_direction_unlocks = d.value("enable_rail_direction_unlocks", false);
	sd.enable_road_direction_unlocks = d.value("enable_road_direction_unlocks", false);
	sd.enable_signal_unlocks         = d.value("enable_signal_unlocks", false);
	sd.enable_bridge_unlocks         = d.value("enable_bridge_unlocks", false);
	sd.enable_tunnel_unlocks         = d.value("enable_tunnel_unlocks", false);
	sd.enable_airport_unlocks        = d.value("enable_airport_unlocks", false);
	sd.enable_tree_unlocks           = d.value("enable_tree_unlocks", false);
	sd.enable_terraform_unlocks      = d.value("enable_terraform_unlocks", false);
	sd.enable_town_action_unlocks    = d.value("enable_town_action_unlocks", false);
	sd.enable_wagon_unlocks          = d.value("enable_wagon_unlocks", false);

	/* Funny Stuff */
	sd.community_vehicle_names   = d.value("community_vehicle_names", true);

	/* Colby Event */
	sd.colby_event      = d.value("colby_event",      false);
	sd.colby_start_year = d.value("colby_start_year",  0);
	sd.colby_town_seed  = (uint32_t)d.value("colby_town_seed", (uint32_t)0);
	sd.colby_cargo      = d.value("colby_cargo",       std::string("coal"));

	/* Ruins */
	sd.ruin_pool_size   = d.value("ruin_pool_size", 0);
	sd.max_active_ruins = d.value("max_active_ruins", 6);
	sd.ruin_cargo_min   = std::max(1, d.value("ruin_cargo_types_min", 2));
	sd.ruin_cargo_max   = std::max(sd.ruin_cargo_min, d.value("ruin_cargo_types_max", 4));
	if (d.contains("ruin_locations") && d["ruin_locations"].is_array()) {
		for (const auto &v : d["ruin_locations"]) {
			if (v.is_string()) sd.ruin_locations.push_back(v.get<std::string>());
		}
	}

	/* Stars */
	sd.enable_stars    = d.value("enable_stars", true);
	sd.star_pool_size  = d.value("star_pool_size", 50);
	if (d.contains("star_locations") && d["star_locations"].is_array()) {
		for (const auto &v : d["star_locations"]) {
			if (v.is_string()) sd.star_locations.push_back(v.get<std::string>());
		}
	}

	/* Demigods (God of Wackens) */
	sd.demigod_enabled            = d.value("demigod_enabled", false);
	sd.demigod_count              = d.value("demigod_count", 0);
	sd.demigod_spawn_interval_min = d.value("demigod_spawn_interval_min", 5);
	sd.demigod_spawn_interval_max = d.value("demigod_spawn_interval_max", 15);
	if (d.contains("demigods") && d["demigods"].is_array()) {
		for (const auto &dg : d["demigods"]) {
			APDemigodDef def;
			def.location       = dg.value("location",     "");
			def.name           = dg.value("name",         "");
			def.president_name = dg.value("president",    "");
			def.theme          = dg.value("theme",        "mixed");
			def.tribute_cost   = dg.value("tribute_cost", (int64_t)500000);
			if (!def.location.empty()) sd.demigods.push_back(std::move(def));
		}
		Debug(misc, 0, "[AP] SlotData: {} demigods loaded (enabled={})", sd.demigods.size(), sd.demigod_enabled);
	}

	/* Wrath of the God of Wackens */
	sd.wrath_enabled       = d.value("wrath_enabled", false);
	sd.wrath_limit_houses  = d.value("wrath_limit_houses", 2);
	sd.wrath_limit_roads   = d.value("wrath_limit_roads", 2);
	sd.wrath_limit_terrain = d.value("wrath_limit_terrain", 25);
	sd.wrath_limit_trees   = d.value("wrath_limit_trees", 10);

	/* Multiplayer mode — disables ruins/colby/demigod/wrath for MP compatibility */
	sd.multiplayer_mode    = d.value("multiplayer_mode", false);

	/* Tier unlock requirements — how many of prev tier needed before next tier opens */
	if (d.contains("tier_unlock_requirements") && d["tier_unlock_requirements"].is_object()) {
		const auto &tu = d["tier_unlock_requirements"];
		for (const auto &[k, v] : tu.items()) {
			if (v.is_number_integer()) sd.tier_unlock_requirements[k] = v.get<int>();
		}
	}

	/* Verbose log — visible in OpenTTD debug console (press ~ in game) */
	Debug(misc, 0, "[AP] SlotData: version={} missions={} start_year={} vehicle='{}'",
	      sd.game_version, sd.mission_count, sd.start_year, sd.starting_vehicle);
	Debug(misc, 0, "[AP] SlotData: map={}x{} landscape={} seed={} traps={}",
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape,
	      sd.world_seed, sd.enable_traps);
	Debug(misc, 0, "[AP] SlotData: win_difficulty={} cv={} pop={} veh={} cargo={} profit={} missions={}",
	      (int)sd.win_difficulty, sd.win_target_company_value, sd.win_target_town_population,
	      sd.win_target_vehicle_count, sd.win_target_cargo_delivered,
	      sd.win_target_monthly_profit, sd.win_target_missions);

	/* item_id_to_name — APWorld sends this so we can resolve item IDs to names */
	if (d.contains("item_id_to_name") && d["item_id_to_name"].is_object()) {
		for (auto &[key, val] : d["item_id_to_name"].items()) {
			/* Manual parse — std::stoll is banned by safeguards.h */
			int64_t id = 0; bool valid = !key.empty();
			for (char c : key) { if (c < '0' || c > '9') { valid = false; break; } id = id * 10 + (int64_t)(c - '0'); }
			if (valid) sd.item_id_to_name[id] = val.get<std::string>();
		}
		Debug(misc, 0, "[AP] SlotData: {} item id->name mappings loaded", sd.item_id_to_name.size());
	} else {
		Debug(misc, 0, "[AP] SlotData: WARNING — no item_id_to_name! Items cannot be unlocked by name.");
	}

	/* shop_prices — APWorld sends {location_name: price_in_pounds} */
	if (d.contains("shop_prices") && d["shop_prices"].is_object()) {
		for (auto &[loc, price] : d["shop_prices"].items()) {
			if (price.is_number_integer()) {
				sd.shop_prices[loc] = price.get<int64_t>();
			}
		}
		Debug(misc, 0, "[AP] SlotData: {} shop prices loaded", sd.shop_prices.size());
	}

	/* shop_item_names — APWorld sends {location_name: item_name} */
	if (d.contains("shop_item_names") && d["shop_item_names"].is_object()) {
		for (auto &[loc, name] : d["shop_item_names"].items()) {
			if (name.is_string())
				sd.shop_item_names[loc] = name.get<std::string>();
		}
		Debug(misc, 0, "[AP] SlotData: {} shop item names loaded", sd.shop_item_names.size());
	}

	/* locked_vehicles — the exact set of vehicle names to lock at session start.
	 * Only engines whose English name is in this set are locked; others
	 * (e.g. Iron Horse engines when enable_iron_horse=false) stay freely available. */
	if (d.contains("locked_vehicles") && d["locked_vehicles"].is_array()) {
		for (const auto &v : d["locked_vehicles"]) {
			if (v.is_string()) sd.locked_vehicles.insert(v.get<std::string>());
		}
		Debug(misc, 0, "[AP] SlotData: {} locked_vehicles loaded", sd.locked_vehicles.size());
	} else {
		Debug(misc, 0, "[AP] SlotData: no locked_vehicles — will lock ALL non-wagon engines (legacy mode)");
	}

	/* Parse missions array */
	if (d.contains("missions") && d["missions"].is_array()) {
		for (const auto &m : d["missions"]) {
			APMission mission;
			mission.location    = m.value("location",    "");
			mission.description = m.value("description", "");
			mission.type        = m.value("type",        "");
			mission.difficulty  = m.value("difficulty",  "easy");
			mission.cargo       = m.value("cargo",       "");
			mission.unit        = m.value("unit",        "units");
			mission.amount      = m.value("amount",      (int64_t)0);
			mission.completed   = false;
			mission.current_value = 0;
			if (!mission.location.empty()) {
				sd.missions.push_back(std::move(mission));
			}
		}
	}

	return sd;
}

/* -------------------------------------------------------------------------
 * Base64 encode (for WebSocket handshake key)
 * ---------------------------------------------------------------------- */

/** Detect Wine by checking for its registry key.
 *  Wine's Schannel implementation is incomplete — AcquireCredentialsHandleA
 *  with UNISP_NAME_A (the SChannel SSL/TLS provider) either hangs or returns
 *  an error, making the WSS probe in WorkerThread crash the connection.
 *  When running under Wine we skip the WSS probe entirely and go straight
 *  to plain WS. */
#endif

/* The old transport: DNS, TCP, TLS probe, WebSocket. Kept compiling while
 * the pipe path is proven, then deleted along with everything it uses. */
