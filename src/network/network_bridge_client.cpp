/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file network_bridge_client.cpp Client-side AP Bridge connection handler.
 *
 * Connects to the Bridge application on port 3980 to receive AP state
 * and send client actions.  This runs alongside the normal game connection.
 */

#include "../stdafx.h"
#include "../debug.h"
#include "../archipelago.h"
#include "../archipelago_gui.h"
#include "../console_func.h"
#include "network_bridge_client.h"
#include "network_func.h"
#include "core/os_abstraction.h"

#include "../3rdparty/nlohmann/json.hpp"

/* NOTE: safeguards.h is intentionally NOT included here.
 * It redefines to_string/stoi as compile errors, which conflicts with
 * nlohmann::json's internal use of std::to_string during dump(). */

using json = nlohmann::json;

/* ── Forward declarations from archipelago_manager.cpp ──────────────── */
extern bool _ap_bridge_mode;

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
 * Mirrors ParseSlotDataFromJSON in network_bridge.cpp for client-side use.
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
	}

	/* shop_prices — {location_name: price_in_pounds} */
	if (sd.contains("shop_prices") && sd["shop_prices"].is_object()) {
		for (auto &[loc, price] : sd["shop_prices"].items()) {
			if (price.is_number_integer()) ap_sd.shop_prices[loc] = price.get<int64_t>();
		}
	}

	/* shop_item_names — {location_name: item_name} */
	if (sd.contains("shop_item_names") && sd["shop_item_names"].is_object()) {
		for (auto &[loc, name] : sd["shop_item_names"].items()) {
			if (name.is_string()) ap_sd.shop_item_names[loc] = name.get<std::string>();
		}
	}

	/* locked_vehicles — set of engine names to lock at session start */
	if (sd.contains("locked_vehicles") && sd["locked_vehicles"].is_array()) {
		for (const auto &v : sd["locked_vehicles"]) {
			if (v.is_string()) ap_sd.locked_vehicles.insert(v.get<std::string>());
		}
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
	}

	return ap_sd;
}

/* ── Static members ────────────────────────────────────────────────── */

BridgeClientHandler *BridgeClientHandler::current = nullptr;

/* ── Constructor / Destructor ──────────────────────────────────────── */

BridgeClientHandler::BridgeClientHandler(SOCKET s)
	: NetworkTCPSocketHandler(s)
{
	Debug(net, 1, "[bridge-client] Connected to Bridge");
}

BridgeClientHandler::~BridgeClientHandler()
{
	Debug(net, 1, "[bridge-client] Disconnected from Bridge");
}

/* ── Queue / Flush ─────────────────────────────────────────────────── */

void BridgeClientHandler::SendJSON(const std::string &json_str)
{
	this->send_queue.push_back(json_str + "\n");
}

void BridgeClientHandler::SendQueued()
{
	if (this->sock == INVALID_SOCKET) return;

	while (!this->send_queue.empty()) {
		const std::string &line = this->send_queue.front();
		ssize_t sent = send(this->sock,
			reinterpret_cast<const char *>(line.data()),
			static_cast<int>(line.size()), 0);

		if (sent < 0) {
			NetworkError err = NetworkError::GetLast();
			if (err.WouldBlock()) return;
			Debug(net, 0, "[bridge-client] Send failed: {}", err.AsString());
			this->CloseConnection();
			return;
		}
		if (sent == 0) {
			this->CloseConnection();
			return;
		}
		if (static_cast<size_t>(sent) < line.size()) {
			this->send_queue.front() = line.substr(sent);
			return;
		}
		this->send_queue.pop_front();
	}
}

/* ── Receive ───────────────────────────────────────────────────────── */

void BridgeClientHandler::ReceiveData()
{
	if (this->sock == INVALID_SOCKET) return;

	char buf[4096];
	for (;;) {
		ssize_t n = recv(this->sock, buf, sizeof(buf), 0);
		if (n < 0) {
			NetworkError err = NetworkError::GetLast();
			if (err.WouldBlock()) break;
			Debug(net, 0, "[bridge-client] Recv failed: {}", err.AsString());
			this->CloseConnection();
			return;
		}
		if (n == 0) {
			Debug(net, 1, "[bridge-client] Server closed connection (recv=0)");
			this->CloseConnection();
			return;
		}
		this->recv_buffer.append(buf, n);
		Debug(net, 3, "[bridge-client] Received {} bytes, buffer now {} bytes",
			n, this->recv_buffer.size());
	}

	/* Process complete lines */
	size_t pos;
	while ((pos = this->recv_buffer.find('\n')) != std::string::npos) {
		std::string line = this->recv_buffer.substr(0, pos);
		this->recv_buffer.erase(0, pos + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;
		Debug(net, 1, "[bridge-client] Complete line received ({} bytes)", line.size());
		this->ProcessLine(line);
	}

	if (!this->recv_buffer.empty()) {
		Debug(net, 3, "[bridge-client] Partial data in buffer: {} bytes", this->recv_buffer.size());
	}
}

/* ── Line dispatch ─────────────────────────────────────────────────── */

void BridgeClientHandler::ProcessLine(const std::string &json_line)
{
	Debug(net, 1, "[bridge-client] << ({} bytes) evt={}", json_line.size(),
		json_line.substr(0, 80));

	json j;
	try {
		j = json::parse(json_line);
	} catch (const json::exception &e) {
		Debug(net, 0, "[bridge-client] JSON parse error: {}", e.what());
		return;
	}

	if (!j.contains("evt") || !j["evt"].is_string()) {
		Debug(net, 0, "[bridge-client] Message missing 'evt' field");
		return;
	}

	std::string evt = j["evt"].get<std::string>();
	Debug(net, 1, "[bridge-client] Processing event: '{}'", evt);

	if (evt == "welcome")           { HandleWelcome(json_line);       return; }
	if (evt == "item")              { HandleItem(json_line);          return; }
	if (evt == "items_batch")       { HandleItem(json_line);          return; }
	if (evt == "infra_locks")       { HandleInfraLocks(json_line);    return; }
	if (evt == "win_progress")      { HandleWinProgress(json_line);   return; }
	if (evt == "print")             { HandlePrint(json_line);         return; }
	if (evt == "check_location")    { HandleCheckLocation(json_line); return; }
	if (evt == "slot_data")         { HandleSlotData(json_line);      return; }
	if (evt == "checked_locations") { /* Client display only */       return; }
	if (evt == "victory")           { HandleVictory();                return; }

	Debug(net, 3, "[bridge-client] Unknown event: '{}'", evt);
}

/* ── Event handlers ────────────────────────────────────────────────── */

void BridgeClientHandler::HandleWelcome(const std::string &json_str)
{
	json j = json::parse(json_str);

	this->game_server_address = j.value("game_server", "");
	this->is_ready = true;

	Debug(net, 1, "[bridge-client] Welcome received, game_server='{}'",
		this->game_server_address);

	/* TODO: Apply received slot_data and items to local AP state.
	 * This will be done via the bridge-mode accessor functions. */

	/* Mark that we need to connect to the game server.
	 * We CANNOT call NetworkClientConnectGame from inside ReceiveData
	 * because it triggers NetworkDisconnect/NetworkClose which disrupts
	 * the socket state while we're still processing.
	 * The connection is deferred to the next Receive() tick. */
	if (!this->game_server_address.empty()) {
		Debug(net, 1, "[bridge-client] Will connect to game server: {} (deferred)",
			this->game_server_address);
		this->pending_game_connect = true;
	}
}

void BridgeClientHandler::HandleItem(const std::string &json_str)
{
	json j = json::parse(json_str);

	/* Handle both single item ("item") and batch ("items_batch") */
	if (j.contains("items") && j["items"].is_array()) {
		/* Batch of items */
		for (const auto &it : j["items"]) {
			std::string name = it.value("item_name", "");
			if (!name.empty()) {
				Debug(net, 3, "[bridge-client] Item (batch): '{}'", name);
				IConsolePrint(CC_INFO, fmt::format("[AP] Received: {}", name));
			}
		}
	} else {
		/* Single item */
		std::string item_name = j.value("item_name", "");
		std::string player = j.value("player", "");
		if (!item_name.empty()) {
			Debug(net, 3, "[bridge-client] Item received: '{}' from '{}'", item_name, player);
			if (!player.empty()) {
				IConsolePrint(CC_INFO, fmt::format("[AP] Received: {} (from {})", item_name, player));
			} else {
				IConsolePrint(CC_INFO, fmt::format("[AP] Received: {}", item_name));
			}
		}
	}

	_ap_status_dirty.store(true);
}

void BridgeClientHandler::HandleInfraLocks(const std::string &json_str)
{
	json j = json::parse(json_str);
	Debug(net, 3, "[bridge-client] Infra locks update received");

	/* Apply all infrastructure lock types — mirrors HandleUnlockInfra in network_bridge.cpp */
	if (j.contains("track_dirs") && j["track_dirs"].is_object()) {
		for (auto &[key, val] : j["track_dirs"].items()) {
			uint8_t railtype = (uint8_t)std::stoi(key);
			uint8_t mask = val.get<uint8_t>();
			AP_SetLockedTrackDirs(railtype, mask);
		}
	}
	if (j.contains("road_dirs"))     AP_SetLockedRoadDirs(j.value("road_dirs", (uint8_t)0));
	if (j.contains("signals"))       AP_SetLockedSignals(j.value("signals", (uint8_t)0));
	if (j.contains("bridges"))       AP_SetLockedBridges(j.value("bridges", (uint16_t)0));
	if (j.contains("tunnels"))       AP_SetLockedTunnels(j.value("tunnels", false));
	if (j.contains("airports"))      AP_SetLockedAirports(j.value("airports", (uint16_t)0));
	if (j.contains("trees"))         AP_SetLockedTrees(j.value("trees", (uint16_t)0));
	if (j.contains("terraform"))     AP_SetLockedTerraform(j.value("terraform", (uint8_t)0));
	if (j.contains("town_actions"))  AP_SetLockedTownActions(j.value("town_actions", (uint8_t)0));

	_ap_status_dirty.store(true);
}

void BridgeClientHandler::HandleWinProgress(const std::string &json_str)
{
	json j = json::parse(json_str);
	Debug(net, 3, "[bridge-client] Win progress update received");

	/* The bridge sends win_progress as {"evt":"win_progress","progress":{...}}
	 * We don't need to store this separately — AP_GetWinProgress() reads
	 * live game state. But we trigger a GUI refresh so the status window updates. */
	_ap_status_dirty.store(true);
}

void BridgeClientHandler::HandlePrint(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string text = j.value("text", "");

	if (!text.empty()) {
		Debug(net, 1, "[bridge-client] {}", text);
		IConsolePrint(CC_INFO, fmt::format("[AP] {}", text));
	}
}

void BridgeClientHandler::HandleCheckLocation(const std::string &json_str)
{
	json j = json::parse(json_str);
	std::string location = j.value("location_name", "");
	Debug(net, 3, "[bridge-client] Location checked: '{}'", location);
	/* Location tracking is server-side; client just refreshes GUI */
	_ap_status_dirty.store(true);
}

void BridgeClientHandler::HandleSlotData(const std::string &json_str)
{
	json j = json::parse(json_str);
	Debug(net, 1, "[bridge-client] Slot data received");

	json sd = j.value("slot_data", json::object());
	APSlotData ap_sd = ParseSlotDataFromJSON(sd);
	AP_BridgeSetSlotData(ap_sd);
	/* Client does NOT generate a world — it joins the server's world.
	 * AP_BridgeSetSlotData sets _ap_pending_world_start = true, but the
	 * client must NOT act on it (no AP_ConsumeWorldStart, no StartNewGame).
	 * Without this reset, intro_gui would show the "Single Player / Load"
	 * choice dialog instead of connecting to the game server. */
	_ap_pending_world_start = false;
	_ap_bridge_mode = true;

	Debug(net, 1, "[bridge-client] Slot data applied: year={} diff={} map={}x{}",
		ap_sd.start_year, (int)ap_sd.win_difficulty,
		(1 << ap_sd.map_x), (1 << ap_sd.map_y));

	IConsolePrint(CC_WHITE, "[AP] Slot data received from Bridge");
	_ap_status_dirty.store(true);
}

void BridgeClientHandler::HandleVictory()
{
	Debug(net, 1, "[bridge-client] *** VICTORY! ***");
	IConsolePrint(CC_WHITE, "[AP] *** VICTORY! All win conditions met! ***");
	ShowAPVictoryScreen();
}

/* ── Client → Bridge commands ──────────────────────────────────────── */

void BridgeClientHandler::SendHello(const std::string &player_name)
{
	json msg;
	msg["cmd"] = "hello";
	msg["player_name"] = player_name;
	SendJSON(msg.dump());
}

void BridgeClientHandler::SendShopBuy(const std::string &location_name)
{
	json msg;
	msg["cmd"] = "shop_buy";
	msg["location_name"] = location_name;
	SendJSON(msg.dump());
}

void BridgeClientHandler::SendDemigodTribute()
{
	json msg;
	msg["cmd"] = "demigod_tribute";
	SendJSON(msg.dump());
}

void BridgeClientHandler::SendSay(const std::string &text)
{
	json msg;
	msg["cmd"] = "say";
	msg["text"] = text;
	SendJSON(msg.dump());
}

/* ── Static management ─────────────────────────────────────────────── */

bool BridgeClientHandler::Connect(const std::string &host, uint16_t port)
{
	Close(); /* Close any existing connection first */

	struct addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	std::string port_str = std::to_string(port);
	int err = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
	if (err != 0 || res == nullptr) {
		Debug(net, 0, "[bridge-client] getaddrinfo failed for {}:{}", host, port);
		return false;
	}

	SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (s == INVALID_SOCKET) {
		Debug(net, 0, "[bridge-client] Failed to create socket");
		freeaddrinfo(res);
		return false;
	}

	if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
		Debug(net, 0, "[bridge-client] Failed to connect to {}:{}: {}",
			host, port, NetworkError::GetLast().AsString());
		closesocket(s);
		freeaddrinfo(res);
		return false;
	}

	freeaddrinfo(res);

	SetNonBlocking(s);
	SetNoDelay(s);

	current = new BridgeClientHandler(s);
	return true;
}

void BridgeClientHandler::Close()
{
	if (current != nullptr) {
		current->CloseConnection();
		delete current;
		current = nullptr;
	}
}

void BridgeClientHandler::Receive()
{
	if (current == nullptr) return;

	current->ReceiveData();

	if (current != nullptr && current->sock == INVALID_SOCKET) {
		delete current;
		current = nullptr;
		return;
	}

	/* Handle deferred game server connection.
	 * This MUST happen OUTSIDE of ReceiveData() because
	 * NetworkClientConnectGame triggers NetworkDisconnect/NetworkClose
	 * which would corrupt state if called during recv processing. */
	if (current != nullptr && current->pending_game_connect) {
		current->pending_game_connect = false;
		std::string addr = current->game_server_address;
		Debug(net, 1, "[bridge-client] Now connecting to game server: {}", addr);
		/* Join Company 0 (created by the server in bridge mode) instead of
		 * creating a new company.  All AP clients share Company 0. */
		NetworkClientConnectGame(addr, CompanyID(0));
	}
}

void BridgeClientHandler::Send()
{
	if (current == nullptr) return;

	current->SendQueued();

	if (current != nullptr && current->sock == INVALID_SOCKET) {
		delete current;
		current = nullptr;
	}
}
