/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file network_bridge.cpp Server-side handler for the AP Bridge controller.
 *
 * Implements a simple JSON Lines protocol over a raw TCP socket.
 * The Bridge application connects here to send commands (engine unlocks,
 * money changes, world generation, etc.) and receive periodic stats
 * and game events.  Only ONE bridge connection at a time.
 */

#include "../stdafx.h"
#include "../debug.h"
#include "../company_base.h"
#include "../town.h"
#include "../vehicle_type.h"
#include "../genworld.h"
#include "../command_func.h"
#include "../engine_cmd.h"
#include "../misc_cmd.h"
#include "../archipelago.h"
#include "../archipelago_gui.h"
#include "../settings_type.h"
#include "../landscape_type.h"
#include "network_bridge.h"
#include "network_func.h"
#include "network_server.h"
#include "network_internal.h"
#include "core/config.h"
#include "core/os_abstraction.h"

#include "../3rdparty/nlohmann/json.hpp"

/* NOTE: safeguards.h is intentionally NOT included here.
 * It redefines to_string as a compile error, which conflicts with
 * nlohmann::json's internal use of std::to_string during dump(). */

using json = nlohmann::json;

/* ── Static members ────────────────────────────────────────────────── */

bool _ap_bridge_mode = false;

ServerNetworkBridgeHandler *ServerNetworkBridgeHandler::current = nullptr;
SOCKET ServerNetworkBridgeHandler::listen_socket = INVALID_SOCKET;

/* ── Constructor / Destructor ──────────────────────────────────────── */

ServerNetworkBridgeHandler::ServerNetworkBridgeHandler(SOCKET s)
	: NetworkTCPSocketHandler(s)
{
	Debug(net, 1, "[bridge] Client connected");
	_ap_bridge_mode = true;
}

ServerNetworkBridgeHandler::~ServerNetworkBridgeHandler()
{
	Debug(net, 1, "[bridge] Client disconnected");
	_ap_bridge_mode = false;
}

/* ── Queue / Flush ─────────────────────────────────────────────────── */

/**
 * Queue a JSON string for sending.  Appends a newline.
 * @param json_str A complete JSON object (no trailing newline).
 */
void ServerNetworkBridgeHandler::SendJSON(const std::string &json_str)
{
	this->send_queue.push_back(json_str + "\n");
}

/**
 * Flush as much of the send queue as the socket will accept.
 * Called from the network loop every tick.
 */
void ServerNetworkBridgeHandler::SendQueued()
{
	if (this->sock == INVALID_SOCKET) return;

	while (!this->send_queue.empty()) {
		const std::string &line = this->send_queue.front();
		ssize_t sent = send(this->sock,
			reinterpret_cast<const char *>(line.data()),
			static_cast<int>(line.size()), 0);

		if (sent < 0) {
			NetworkError err = NetworkError::GetLast();
			if (err.WouldBlock()) return; /* Try again next tick */
			Debug(net, 0, "[bridge] Send failed: {}", err.AsString());
			this->CloseConnection();
			return;
		}
		if (sent == 0) {
			this->CloseConnection();
			return;
		}
		if (static_cast<size_t>(sent) < line.size()) {
			/* Partial send — keep the remainder */
			this->send_queue.front() = line.substr(sent);
			return;
		}
		this->send_queue.pop_front();
	}
}

/* ── Receive ───────────────────────────────────────────────────────── */

/**
 * Read available data from socket into recv_buffer,
 * then process every complete line (delimited by '\n').
 */
void ServerNetworkBridgeHandler::ReceiveData()
{
	if (this->sock == INVALID_SOCKET) return;

	char buf[4096];
	for (;;) {
		ssize_t n = recv(this->sock, buf, sizeof(buf), 0);
		if (n < 0) {
			NetworkError err = NetworkError::GetLast();
			if (err.WouldBlock()) break;
			Debug(net, 0, "[bridge] Recv failed: {}", err.AsString());
			this->CloseConnection();
			return;
		}
		if (n == 0) {
			/* Peer closed */
			this->CloseConnection();
			return;
		}
		this->recv_buffer.append(buf, n);
	}

	/* Process complete lines */
	size_t pos;
	while ((pos = this->recv_buffer.find('\n')) != std::string::npos) {
		std::string line = this->recv_buffer.substr(0, pos);
		this->recv_buffer.erase(0, pos + 1);

		/* Trim \r if present (Windows line endings) */
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		this->ProcessLine(line);
	}
}

/* ── Line dispatch ─────────────────────────────────────────────────── */

void ServerNetworkBridgeHandler::ProcessLine(const std::string &json_line)
{
	Debug(net, 5, "[bridge] << {}", json_line);

	json j;
	try {
		j = json::parse(json_line);
	} catch (const json::exception &e) {
		Debug(net, 0, "[bridge] JSON parse error: {}", e.what());
		return;
	}

	if (!j.contains("cmd") || !j["cmd"].is_string()) {
		Debug(net, 0, "[bridge] Message missing 'cmd' field");
		return;
	}

	std::string cmd = j["cmd"].get<std::string>();

	if (cmd == "hello")           { HandleHello(json_line);        return; }
	if (cmd == "gen_world")       { HandleGenWorld(json_line);     return; }
	if (cmd == "unlock_engine")   { HandleUnlockEngine(json_line); return; }
	if (cmd == "change_money")    { HandleChangeMoney(json_line);  return; }
	if (cmd == "apply_trap")      { HandleApplyTrap(json_line);    return; }
	if (cmd == "unlock_infra")    { HandleUnlockInfra(json_line);  return; }
	if (cmd == "set_speed")       { HandleSetSpeed(json_line);     return; }
	if (cmd == "spawn_ruin")      { HandleSpawnRuin(json_line);    return; }
	if (cmd == "spawn_demigod")   { HandleSpawnDemigod(json_line); return; }
	if (cmd == "defeat_demigod")  { HandleDefeatDemigod(json_line);return; }
	if (cmd == "trigger_colby")   { HandleTriggerColby(json_line); return; }
	if (cmd == "send_chat")       { HandleSendChat(json_line);     return; }
	if (cmd == "ping")            { HandlePing();                  return; }

	Debug(net, 0, "[bridge] Unknown command: '{}'", cmd);
}

/* ── Command handlers ──────────────────────────────────────────────── */

void ServerNetworkBridgeHandler::HandleHello(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string ver = j.value("version", "unknown");
	Debug(net, 1, "[bridge] Hello from bridge, version {}", ver);

	json ack;
	ack["evt"] = "hello_ack";
	ack["version"] = "1.0";
	SendJSON(ack.dump());
}

/**
 * Read a JSON value as bool, accepting both true/false and 0/1.
 * The AP server sometimes sends booleans as integers.
 */
static bool jbool(const json &j, const char *key, bool def)
{
	if (!j.contains(key)) return def;
	const auto &v = j[key];
	if (v.is_boolean()) return v.get<bool>();
	if (v.is_number())  return v.get<int>() != 0;
	return def;
}

/**
 * Parse a JSON slot_data object into an APSlotData struct.
 * Used by both HandleGenWorld (TCP) and BridgeLoadSlotDataFromFile (file).
 */
static APSlotData ParseSlotDataFromJSON(const json &sd)
{
	APSlotData ap_sd;

	/* World generation */
	ap_sd.start_year       = sd.value("start_year", 1950);
	ap_sd.world_seed       = sd.value("world_seed", 0u);
	ap_sd.map_x            = (uint8_t)sd.value("map_x", 8);
	ap_sd.map_y            = (uint8_t)sd.value("map_y", 8);
	ap_sd.landscape        = (uint8_t)sd.value("landscape", 0);
	ap_sd.land_generator   = (uint8_t)sd.value("land_generator", 1);
	ap_sd.terrain_type     = (uint8_t)sd.value("terrain_type", 1);
	ap_sd.sea_level        = (uint8_t)sd.value("sea_level", 1);
	ap_sd.rivers           = (uint8_t)sd.value("rivers", 2);
	ap_sd.smoothness       = (uint8_t)sd.value("smoothness", 1);
	ap_sd.variety          = (uint8_t)sd.value("variety", 0);
	ap_sd.number_towns     = (uint8_t)sd.value("number_towns", 2);
	ap_sd.town_name        = (uint8_t)sd.value("town_name", 0);

	/* Accounting */
	ap_sd.infinite_money            = jbool(sd, "infinite_money", false);
	ap_sd.inflation                 = jbool(sd, "inflation", false);
	ap_sd.max_loan                  = sd.value("max_loan", 300000u);
	ap_sd.infrastructure_maintenance = jbool(sd, "infrastructure_maintenance", false);
	ap_sd.vehicle_costs             = (uint8_t)sd.value("vehicle_costs", 1);
	ap_sd.construction_cost         = (uint8_t)sd.value("construction_cost", 1);

	/* Vehicle limits */
	ap_sd.max_trains       = (uint16_t)sd.value("max_trains", 500);
	ap_sd.max_roadveh      = (uint16_t)sd.value("max_roadveh", 500);
	ap_sd.max_aircraft     = (uint16_t)sd.value("max_aircraft", 200);
	ap_sd.max_ships        = (uint16_t)sd.value("max_ships", 300);
	ap_sd.max_train_length = (uint16_t)sd.value("max_train_length", 7);
	ap_sd.station_spread   = (uint16_t)sd.value("station_spread", 12);
	ap_sd.road_stop_on_town_road       = jbool(sd, "road_stop_on_town_road", true);
	ap_sd.road_stop_on_competitor_road = jbool(sd, "road_stop_on_competitor_road", true);
	ap_sd.crossing_with_competitor     = jbool(sd, "crossing_with_competitor", true);

	/* Disasters / Accidents */
	ap_sd.disasters          = jbool(sd, "disasters", false);
	ap_sd.plane_crashes      = (uint8_t)sd.value("plane_crashes", 2);
	ap_sd.vehicle_breakdowns = (uint8_t)sd.value("vehicle_breakdowns", 1);

	/* Economy / Environment */
	ap_sd.economy_type       = (uint8_t)sd.value("economy_type", 1);
	ap_sd.bribe              = jbool(sd, "bribe", true);
	ap_sd.exclusive_rights   = jbool(sd, "exclusive_rights", true);
	ap_sd.fund_buildings     = jbool(sd, "fund_buildings", true);
	ap_sd.fund_roads         = jbool(sd, "fund_roads", true);
	ap_sd.give_money         = jbool(sd, "give_money", true);
	ap_sd.town_growth_rate   = (uint8_t)sd.value("town_growth_rate", 2);
	ap_sd.found_town         = (uint8_t)sd.value("found_town", 0);
	ap_sd.town_cargo_scale   = (uint16_t)sd.value("town_cargo_scale", 100);
	ap_sd.industry_cargo_scale = (uint16_t)sd.value("industry_cargo_scale", 100);
	ap_sd.industry_density   = (uint8_t)sd.value("industry_density", 4);
	ap_sd.allow_town_roads   = jbool(sd, "allow_town_roads", true);
	ap_sd.road_side          = (uint8_t)sd.value("road_side", 1);

	/* NewGRF flags */
	ap_sd.enable_iron_horse      = jbool(sd, "enable_iron_horse", false);
	ap_sd.enable_military_items  = jbool(sd, "enable_military_items", false);
	ap_sd.enable_shark_ships     = jbool(sd, "enable_shark_ships", false);
	ap_sd.enable_hover_vehicles  = jbool(sd, "enable_hover_vehicles", false);
	ap_sd.enable_heqs            = jbool(sd, "enable_heqs", false);
	ap_sd.enable_vactrain        = jbool(sd, "enable_vactrain", false);
	ap_sd.enable_aircraftpack    = jbool(sd, "enable_aircraftpack", false);
	ap_sd.enable_firs            = jbool(sd, "enable_firs", false);
	ap_sd.firs_economy           = (uint8_t)sd.value("firs_economy", 0);

	/* Win targets */
	ap_sd.win_target_company_value   = sd.value("win_target_company_value", (int64_t)8000000);
	ap_sd.win_target_town_population = sd.value("win_target_town_population", (int64_t)100000);
	ap_sd.win_target_vehicle_count   = sd.value("win_target_vehicle_count", (int64_t)30);
	ap_sd.win_target_cargo_delivered = sd.value("win_target_cargo_delivered", (int64_t)120000);
	ap_sd.win_target_monthly_profit  = sd.value("win_target_monthly_profit", (int64_t)100000);
	ap_sd.win_target_missions        = sd.value("win_target_missions", (int64_t)35);

	/* Item Pool unlock options */
	ap_sd.enable_rail_direction_unlocks = jbool(sd, "enable_rail_direction_unlocks", false);
	ap_sd.enable_road_direction_unlocks = jbool(sd, "enable_road_direction_unlocks", false);
	ap_sd.enable_signal_unlocks         = jbool(sd, "enable_signal_unlocks", false);
	ap_sd.enable_bridge_unlocks         = jbool(sd, "enable_bridge_unlocks", false);
	ap_sd.enable_tunnel_unlocks         = jbool(sd, "enable_tunnel_unlocks", false);
	ap_sd.enable_airport_unlocks        = jbool(sd, "enable_airport_unlocks", false);
	ap_sd.enable_tree_unlocks           = jbool(sd, "enable_tree_unlocks", false);
	ap_sd.enable_terraform_unlocks      = jbool(sd, "enable_terraform_unlocks", false);
	ap_sd.enable_town_action_unlocks    = jbool(sd, "enable_town_action_unlocks", false);
	ap_sd.enable_wagon_unlocks          = jbool(sd, "enable_wagon_unlocks", false);

	/* Community names */
	ap_sd.community_vehicle_names = jbool(sd, "community_vehicle_names", true);

	/* Misc */
	ap_sd.enable_traps       = jbool(sd, "enable_traps", true);
	ap_sd.death_link         = jbool(sd, "death_link", false);
	ap_sd.mission_count      = sd.value("mission_count", 0);
	ap_sd.starting_cash_bonus = sd.value("starting_cash_bonus", 0);
	ap_sd.starting_vehicle_count = sd.value("starting_vehicle_count", 1);
	ap_sd.mission_difficulty  = sd.value("mission_difficulty", 2);

	/* Win difficulty enum */
	int wd = sd.value("win_difficulty", 2);
	if (wd >= 0 && wd <= 10) ap_sd.win_difficulty = (APWinDifficulty)wd;

	/* Colby Event */
	ap_sd.colby_event      = jbool(sd, "colby_event", false);
	ap_sd.colby_start_year = sd.value("colby_start_year", 0);
	ap_sd.colby_town_seed  = (uint32_t)sd.value("colby_town_seed", (uint32_t)0);
	if (sd.contains("colby_cargo") && sd["colby_cargo"].is_string())
		ap_sd.colby_cargo = sd["colby_cargo"].get<std::string>();

	/* Ruins */
	ap_sd.ruin_pool_size   = sd.value("ruin_pool_size", 0);
	ap_sd.max_active_ruins = sd.value("max_active_ruins", 6);
	ap_sd.ruin_cargo_min   = std::max(1, sd.value("ruin_cargo_types_min", 2));
	ap_sd.ruin_cargo_max   = std::max(ap_sd.ruin_cargo_min, sd.value("ruin_cargo_types_max", 4));
	if (sd.contains("ruin_locations") && sd["ruin_locations"].is_array()) {
		for (const auto &v : sd["ruin_locations"]) {
			if (v.is_string()) ap_sd.ruin_locations.push_back(v.get<std::string>());
		}
	}

	/* Demigods (God of Wackens) */
	ap_sd.demigod_enabled            = jbool(sd, "demigod_enabled", false);
	ap_sd.demigod_count              = sd.value("demigod_count", 0);
	ap_sd.demigod_spawn_interval_min = sd.value("demigod_spawn_interval_min", 5);
	ap_sd.demigod_spawn_interval_max = sd.value("demigod_spawn_interval_max", 15);
	if (sd.contains("demigods") && sd["demigods"].is_array()) {
		for (const auto &dg : sd["demigods"]) {
			APDemigodDef def;
			def.location       = dg.value("location",     "");
			def.name           = dg.value("name",         "");
			def.president_name = dg.value("president",    "");
			def.theme          = dg.value("theme",        "mixed");
			def.tribute_cost   = dg.value("tribute_cost", (int64_t)500000);
			if (!def.location.empty()) ap_sd.demigods.push_back(std::move(def));
		}
	}

	/* Wrath */
	ap_sd.wrath_enabled       = jbool(sd, "wrath_enabled", false);
	ap_sd.wrath_limit_houses  = sd.value("wrath_limit_houses", 2);
	ap_sd.wrath_limit_roads   = sd.value("wrath_limit_roads", 2);
	ap_sd.wrath_limit_terrain = sd.value("wrath_limit_terrain", 25);
	ap_sd.wrath_limit_trees   = sd.value("wrath_limit_trees", 10);

	/* Tier unlock requirements */
	if (sd.contains("tier_unlock_requirements") && sd["tier_unlock_requirements"].is_object()) {
		for (const auto &[k, v] : sd["tier_unlock_requirements"].items()) {
			if (v.is_number_integer()) ap_sd.tier_unlock_requirements[k] = v.get<int>();
		}
	}

	/* item_id_to_name — APWorld sends this so we can resolve item IDs to names */
	if (sd.contains("item_id_to_name") && sd["item_id_to_name"].is_object()) {
		for (auto &[key, val] : sd["item_id_to_name"].items()) {
			int64_t id = 0; bool valid = !key.empty();
			for (char c : key) { if (c < '0' || c > '9') { valid = false; break; } id = id * 10 + (int64_t)(c - '0'); }
			if (valid && val.is_string()) ap_sd.item_id_to_name[id] = val.get<std::string>();
		}
		Debug(net, 1, "[bridge] {} item id->name mappings loaded", ap_sd.item_id_to_name.size());
	}

	/* shop_prices — {location_name: price_in_pounds} */
	if (sd.contains("shop_prices") && sd["shop_prices"].is_object()) {
		for (auto &[loc, price] : sd["shop_prices"].items()) {
			if (price.is_number_integer()) ap_sd.shop_prices[loc] = price.get<int64_t>();
		}
		Debug(net, 1, "[bridge] {} shop prices loaded", ap_sd.shop_prices.size());
	}

	/* shop_item_names — {location_name: item_name} */
	if (sd.contains("shop_item_names") && sd["shop_item_names"].is_object()) {
		for (auto &[loc, name] : sd["shop_item_names"].items()) {
			if (name.is_string()) ap_sd.shop_item_names[loc] = name.get<std::string>();
		}
		Debug(net, 1, "[bridge] {} shop item names loaded", ap_sd.shop_item_names.size());
	}

	/* locked_vehicles — set of engine names to lock at session start */
	if (sd.contains("locked_vehicles") && sd["locked_vehicles"].is_array()) {
		for (const auto &v : sd["locked_vehicles"]) {
			if (v.is_string()) ap_sd.locked_vehicles.insert(v.get<std::string>());
		}
		Debug(net, 1, "[bridge] {} locked_vehicles loaded", ap_sd.locked_vehicles.size());
	}

	/* missions array */
	if (sd.contains("missions") && sd["missions"].is_array()) {
		for (const auto &m : sd["missions"]) {
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
			if (!mission.location.empty()) ap_sd.missions.push_back(std::move(mission));
		}
		Debug(net, 1, "[bridge] {} missions loaded", ap_sd.missions.size());
	}

	Debug(net, 1, "[bridge] Parsed APSlotData: year={} map={}x{} landscape={} "
		"iron_horse={} military={} shark={} firs={} missions={} shop_items={}",
		ap_sd.start_year, (1 << ap_sd.map_x), (1 << ap_sd.map_y),
		(int)ap_sd.landscape, ap_sd.enable_iron_horse,
		ap_sd.enable_military_items, ap_sd.enable_shark_ships,
		ap_sd.enable_firs, ap_sd.missions.size(), ap_sd.shop_item_names.size());

	return ap_sd;
}

/* ── File-based AP slot data loading ─────────────────────────────────── */

/**
 * Load AP slot data from ap_slot_data.json next to the executable.
 * Called from dedicated_v.cpp BEFORE the first world generation.
 * This eliminates the need for gen_world via TCP (which caused server
 * reboot and connection loss).
 *
 * @return true if the file was found and parsed successfully.
 */
bool BridgeLoadSlotDataFromFile()
{
	/* Look for ap_slot_data.json next to the executable */
	std::string path;

	/* Try the working directory first (where openttd.exe is) */
	FILE *f = fopen("ap_slot_data.json", "rb");
	if (f == nullptr) {
		Debug(net, 3, "[bridge] No ap_slot_data.json found — normal dedicated server mode");
		return false;
	}
	path = "ap_slot_data.json";

	/* Read entire file */
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0 || size > 10 * 1024 * 1024) {
		Debug(net, 0, "[bridge] ap_slot_data.json has invalid size: {}", size);
		fclose(f);
		return false;
	}

	std::string content(size, '\0');
	size_t read = fread(&content[0], 1, size, f);
	fclose(f);

	if ((long)read != size) {
		Debug(net, 0, "[bridge] Failed to read ap_slot_data.json (read {} of {} bytes)", read, size);
		return false;
	}

	Debug(net, 1, "[bridge] Loading AP slot data from file ({} bytes)...", size);

	try {
		json j = json::parse(content);
		json sd = j.value("slot_data", json::object());

		APSlotData ap_sd = ParseSlotDataFromJSON(sd);

		/* Also pick up the seed from the wrapper object */
		uint32_t file_seed = j.value("seed", 0u);
		if (file_seed != 0 && ap_sd.world_seed == 0) {
			ap_sd.world_seed = file_seed;
		}

		AP_BridgeSetSlotData(ap_sd);
		_ap_bridge_mode = true;

		Debug(net, 1, "[bridge] AP slot data loaded from file successfully (seed={})",
			ap_sd.world_seed);
		return true;

	} catch (const json::exception &e) {
		Debug(net, 0, "[bridge] Failed to parse ap_slot_data.json: {}", e.what());
		return false;
	}
}

/* ── TCP command handlers ────────────────────────────────────────────── */

void ServerNetworkBridgeHandler::HandleGenWorld([[maybe_unused]] const std::string &json_str)
{
	/* DISABLED: gen_world via TCP causes server reboot (NetworkReboot) which
	 * kills the bridge TCP connection.  AP slot data is now loaded from
	 * ap_slot_data.json BEFORE the first world generation — no reboot needed.
	 * See BridgeLoadSlotDataFromFile() and dedicated_v.cpp. */
	Debug(net, 0, "[bridge] gen_world via TCP is DISABLED — slot data loaded from file on startup");

	/* Send world_ready immediately since the world is already generated */
	if (ServerNetworkBridgeHandler::current != nullptr) {
		ServerNetworkBridgeHandler::current->SendWorldReady(
			_settings_game.game_creation.generation_seed);
	}
}

void ServerNetworkBridgeHandler::HandleUnlockEngine(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string engine_name = j.value("engine_name", "");
	bool all_companies = j.value("all_companies", true);

	Debug(net, 3, "[bridge] unlock_engine: '{}' all_companies={}", engine_name, all_companies);

	if (engine_name.empty()) {
		Debug(net, 0, "[bridge] unlock_engine: empty engine_name");
		return;
	}

	if (all_companies) {
		/* Unlock for every existing company */
		for (const Company *c : Company::Iterate()) {
			/* We reuse the AP engine-unlock logic via the existing function.
			 * AP_UnlockEngineByName uses _local_company, so we temporarily set it. */
			CompanyID old = _local_company;
			_local_company = c->index;
			AP_UnlockEngineByName(engine_name);
			_local_company = old;
		}
	} else {
		CompanyID cid = (CompanyID)j.value("company", 0);
		CompanyID old = _local_company;
		_local_company = cid;
		AP_UnlockEngineByName(engine_name);
		_local_company = old;
	}
}

void ServerNetworkBridgeHandler::HandleChangeMoney(const std::string &json_str)
{
	json j = json::parse(json_str);
	int company = j.value("company", -1);
	int64_t delta = j.value("delta", (int64_t)0);

	Debug(net, 3, "[bridge] change_money: company={} delta={}", company, delta);

	if (company < 0) {
		/* Apply to all companies */
		for (const Company *c : Company::Iterate()) {
			CompanyID old = _current_company;
			_current_company = OWNER_DEITY;
			Command<CMD_CHANGE_BANK_BALANCE>::Do(
				DoCommandFlags{DoCommandFlag::Execute},
				(TileIndex)0, (Money)delta, c->index, EXPENSES_OTHER);
			_current_company = old;
		}
	} else {
		CompanyID cid = (CompanyID)company;
		CompanyID old = _current_company;
		_current_company = OWNER_DEITY;
		Command<CMD_CHANGE_BANK_BALANCE>::Do(
			DoCommandFlags{DoCommandFlag::Execute},
			(TileIndex)0, (Money)delta, cid, EXPENSES_OTHER);
		_current_company = old;
	}
}

void ServerNetworkBridgeHandler::HandleApplyTrap(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string trap_name = j.value("trap_name", "");

	Debug(net, 3, "[bridge] apply_trap: '{}'", trap_name);

	/* TODO: Implement trap application.  The bridge will send specific
	 * trap commands; for now this is a placeholder that logs the request.
	 * Traps like "Breakdown Wave", "Tax Bill", "Speed Limit" etc. will
	 * need dedicated handlers based on what archipelago_manager.cpp does. */
}

void ServerNetworkBridgeHandler::HandleUnlockInfra(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string type = j.value("type", "");

	Debug(net, 3, "[bridge] unlock_infra: type='{}'", type);

	if (type == "track_dir") {
		uint8_t railtype = j.value("railtype", (uint8_t)0);
		uint8_t mask = j.value("mask", (uint8_t)0);
		AP_SetLockedTrackDirs(railtype, mask);
	} else if (type == "road_dir") {
		uint8_t mask = j.value("mask", (uint8_t)0);
		AP_SetLockedRoadDirs(mask);
	} else if (type == "signals") {
		uint8_t mask = j.value("mask", (uint8_t)0);
		AP_SetLockedSignals(mask);
	} else if (type == "bridges") {
		uint16_t mask = j.value("mask", (uint16_t)0);
		AP_SetLockedBridges(mask);
	} else if (type == "tunnels") {
		bool locked = j.value("locked", false);
		AP_SetLockedTunnels(locked);
	} else if (type == "airports") {
		uint16_t mask = j.value("mask", (uint16_t)0);
		AP_SetLockedAirports(mask);
	} else if (type == "trees") {
		uint16_t mask = j.value("mask", (uint16_t)0);
		AP_SetLockedTrees(mask);
	} else if (type == "terraform") {
		uint8_t mask = j.value("mask", (uint8_t)0);
		AP_SetLockedTerraform(mask);
	} else if (type == "town_actions") {
		uint8_t mask = j.value("mask", (uint8_t)0);
		AP_SetLockedTownActions(mask);
	} else {
		Debug(net, 0, "[bridge] unlock_infra: unknown type '{}'", type);
	}
}

void ServerNetworkBridgeHandler::HandleSetSpeed(const std::string &json_str)
{
	json j = json::parse(json_str);
	int speed = j.value("speed", 100);
	Debug(net, 3, "[bridge] set_speed: {}", speed);
	AP_SetFfSpeed(speed);
}

void ServerNetworkBridgeHandler::HandleSpawnRuin([[maybe_unused]] const std::string &json_str)
{
	Debug(net, 3, "[bridge] spawn_ruin (TODO)");
	/* TODO: Parse ruin definition and spawn via AP ruin system */
}

void ServerNetworkBridgeHandler::HandleSpawnDemigod([[maybe_unused]] const std::string &json_str)
{
	Debug(net, 3, "[bridge] spawn_demigod (TODO)");
	/* TODO: Parse demigod definition and activate via AP demigod system */
}

void ServerNetworkBridgeHandler::HandleDefeatDemigod([[maybe_unused]] const std::string &json_str)
{
	Debug(net, 3, "[bridge] defeat_demigod");
	AP_DemigodDefeat();
}

void ServerNetworkBridgeHandler::HandleTriggerColby([[maybe_unused]] const std::string &json_str)
{
	Debug(net, 3, "[bridge] trigger_colby (TODO)");
	/* TODO: Trigger Colby event if configured */
}

void ServerNetworkBridgeHandler::HandleSendChat(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string text = j.value("text", "");

	if (text.empty()) return;
	Debug(net, 3, "[bridge] send_chat: '{}'", text);

	/* Broadcast to all connected clients as a server message */
	NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST,
		0, text, CLIENT_ID_SERVER);
}

void ServerNetworkBridgeHandler::HandlePing()
{
	json pong;
	pong["evt"] = "pong";
	SendJSON(pong.dump());
}

/* ── Periodic event senders ────────────────────────────────────────── */

/**
 * Build and send a company_stats event with data from all companies.
 * Called periodically from the game loop (e.g. every 74 ticks / monthly).
 */
void ServerNetworkBridgeHandler::SendCompanyStats()
{
	json evt;
	evt["evt"] = "company_stats";
	evt["companies"] = json::array();

	for (const Company *c : Company::Iterate()) {
		json co;
		co["id"] = (int)c->index.base();
		co["money"] = (int64_t)c->money;
		co["loan"] = (int64_t)c->current_loan;
		co["value"] = (int64_t)CalculateCompanyValue(c, true);
		co["income"] = (int64_t)c->cur_economy.income;
		co["expenses"] = (int64_t)c->cur_economy.expenses;
		co["performance"] = c->cur_economy.performance_history;

		/* Vehicle counts by type */
		json vehicles;
		vehicles["train"] = c->group_all[VEH_TRAIN].num_vehicle;
		vehicles["road"] = c->group_all[VEH_ROAD].num_vehicle;
		vehicles["ship"] = c->group_all[VEH_SHIP].num_vehicle;
		vehicles["air"] = c->group_all[VEH_AIRCRAFT].num_vehicle;
		co["vehicles"] = vehicles;

		/* Delivered cargo — array of per-cargo-type totals */
		json cargo = json::array();
		for (size_t i = 0; i < NUM_CARGO; i++) {
			cargo.push_back(c->cur_economy.delivered_cargo[i]);
		}
		co["delivered_cargo"] = cargo;

		evt["companies"].push_back(co);
	}

	SendJSON(evt.dump());
}

/**
 * Build and send a population event with total town population.
 * Called periodically alongside company stats.
 */
void ServerNetworkBridgeHandler::SendPopulation()
{
	uint32_t total = 0;
	for (const Town *t : Town::Iterate()) {
		total += t->cache.population;
	}

	json evt;
	evt["evt"] = "population";
	evt["total"] = total;
	SendJSON(evt.dump());
}

/* ── Immediate event senders ───────────────────────────────────────── */

void ServerNetworkBridgeHandler::SendCheckLocation(const std::string &location_name)
{
	json evt;
	evt["evt"] = "check_location";
	evt["location_name"] = location_name;
	SendJSON(evt.dump());
}

void ServerNetworkBridgeHandler::SendShopPurchase(const std::string &location_name)
{
	json evt;
	evt["evt"] = "shop_purchase";
	evt["location_name"] = location_name;
	SendJSON(evt.dump());
}

void ServerNetworkBridgeHandler::SendDeathEvent(const std::string &cause)
{
	json evt;
	evt["evt"] = "death_event";
	evt["cause"] = cause;
	SendJSON(evt.dump());
}

void ServerNetworkBridgeHandler::SendClientJoined(uint32_t client_id, const std::string &name, int company)
{
	json evt;
	evt["evt"] = "client_joined";
	evt["client_id"] = client_id;
	evt["name"] = name;
	evt["company"] = company;
	SendJSON(evt.dump());
}

void ServerNetworkBridgeHandler::SendClientLeft(uint32_t client_id)
{
	json evt;
	evt["evt"] = "client_left";
	evt["client_id"] = client_id;
	SendJSON(evt.dump());
}

void ServerNetworkBridgeHandler::SendWorldReady(uint32_t seed)
{
	json evt;
	evt["evt"] = "world_ready";
	evt["seed"] = seed;
	SendJSON(evt.dump());
}

/* ── Static socket management ──────────────────────────────────────── */

/**
 * Start listening for bridge connections on the given port.
 * Uses a single raw TCP socket (not the TCPListenHandler template).
 */
bool ServerNetworkBridgeHandler::Listen(uint16_t port)
{
	/* If already listening (e.g. during server restart for gen_world),
	 * skip re-creation.  The existing listener is still valid. */
	if (listen_socket != INVALID_SOCKET) {
		Debug(net, 1, "[bridge] Already listening on port {} — keeping existing connection", port);
		return true;
	}

	struct addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;      /* IPv4 only for simplicity */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	std::string port_str = std::to_string(port);
	int err = getaddrinfo(nullptr, port_str.c_str(), &hints, &res);
	if (err != 0 || res == nullptr) {
		Debug(net, 0, "[bridge] getaddrinfo failed for port {}", port);
		return false;
	}

	listen_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (listen_socket == INVALID_SOCKET) {
		Debug(net, 0, "[bridge] Failed to create socket");
		freeaddrinfo(res);
		return false;
	}

	/* Allow port reuse */
	SetReusePort(listen_socket);

	if (bind(listen_socket, res->ai_addr, (int)res->ai_addrlen) != 0) {
		Debug(net, 0, "[bridge] Failed to bind to port {}: {}",
			port, NetworkError::GetLast().AsString());
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		freeaddrinfo(res);
		return false;
	}

	freeaddrinfo(res);

	if (listen(listen_socket, 1) != 0) {
		Debug(net, 0, "[bridge] Failed to listen on port {}: {}",
			port, NetworkError::GetLast().AsString());
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		return false;
	}

	SetNonBlocking(listen_socket);

	Debug(net, 1, "[bridge] Listening on port {}", port);
	return true;
}

/**
 * Stop listening and close any active bridge connection.
 */
void ServerNetworkBridgeHandler::CloseAll()
{
	if (current != nullptr) {
		current->CloseConnection();
		delete current;
		current = nullptr;
	}

	if (listen_socket != INVALID_SOCKET) {
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
	}

	_ap_bridge_mode = false;
	Debug(net, 1, "[bridge] All connections closed");
}

/**
 * Accept a pending connection if one is waiting on the listen socket.
 * Only one bridge connection is allowed at a time.
 */
void ServerNetworkBridgeHandler::AcceptConnection()
{
	if (listen_socket == INVALID_SOCKET) return;

	struct sockaddr_storage addr{};
	socklen_t addr_len = sizeof(addr);
	SOCKET s = accept(listen_socket, (struct sockaddr *)&addr, &addr_len);
	if (s == INVALID_SOCKET) return; /* No pending connection */

	SetNonBlocking(s);
	SetNoDelay(s);

	if (current != nullptr) {
		/* Already have a bridge connected — reject this one */
		Debug(net, 1, "[bridge] Rejecting second connection (already have one)");
		closesocket(s);
		return;
	}

	current = new ServerNetworkBridgeHandler(s);
}

/**
 * Poll for incoming data on the active bridge connection.
 * Called every tick from the network loop.
 */
void ServerNetworkBridgeHandler::Receive()
{
	AcceptConnection();

	if (current == nullptr) return;

	current->ReceiveData();

	/* Check if the connection was closed during receive */
	if (current != nullptr && current->sock == INVALID_SOCKET) {
		delete current;
		current = nullptr;
	}
}

/**
 * Flush outgoing data on the active bridge connection.
 * Called every tick from the network loop.
 */
void ServerNetworkBridgeHandler::Send()
{
	if (current == nullptr) return;

	current->SendQueued();

	/* Check if the connection was closed during send */
	if (current != nullptr && current->sock == INVALID_SOCKET) {
		delete current;
		current = nullptr;
	}
}
