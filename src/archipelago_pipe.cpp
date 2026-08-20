/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* @file archipelago_pipe.cpp ArchipelagoClient over the launcher pipe. */

#include "stdafx.h"
#include "archipelago.h"
#include "ap_pipe.h"
#include "newgrf_config.h"
#include "openttd.h"

#include "3rdparty/fmt/format.h"
#include "3rdparty/nlohmann/json.hpp"

#include <map>
#include <chrono>
#include <thread>

#include "safeguards.h"

using json = nlohmann::json;

/* Lives in archipelago.cpp; turns AP's slot_data into APSlotData. The
 * shape it expects is the Connected message, so the raw object gets
 * wrapped before it goes in. */
extern APSlotData ParseSlotData(const json &msg);

/** Set from the command line by the launcher: -ap-pipe <name>. */
std::string _ap_pipe_name;

/** Digits to int64. safeguards.h rules out atoll and friends. */
static int64_t ParseInt64(const std::string &s)
{
	int64_t v = 0;
	bool neg = false;
	size_t i = 0;
	if (i < s.size() && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); i++; }
	for (; i < s.size(); i++) {
		if (s[i] < '0' || s[i] > '9') break;
		v = v * 10 + (s[i] - '0');
	}
	return neg ? -v : v;
}

/* Report loaded NewGRFs by GRFID, never filename -- content ids survive
 * renames. Then pick the list that matters: _grfconfig is EMPTY on the
 * intro screen, so ActiveGrfList() chooses by _game_mode. */
static const GRFConfigList &ActiveGrfList()
{
	return _game_mode == GM_MENU ? _grfconfig_newgame : _grfconfig;
}

void ArchipelagoClient::SendLoadedGrfList()
{
	for (const auto &c : ActiveGrfList()) {
		if (c == nullptr) continue;
		/* Disabled or missing sets are not loaded, so reporting them would
		 * tell the launcher the seed is playable when it is not. */
		if (c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND) continue;

		this->outbound_queue.push_back({ fmt::format("GRF:{:08x}:{}",
				std::byteswap(c->ident.grfid), c->version) });
	}
	this->outbound_queue.push_back({ "GRFEND:" });
}

/**
 * What the game can see, for when it has just refused a seed.
 *
 * "You do not have Iron Horse" is unhelpful to a player who does have it, and
 * the two GRF lists look identical from the launcher's side. This says which
 * list was read and what the file scan turned up, so the launcher can show it
 * instead of the player and I guessing.
 */
void ArchipelagoClient::ReportGrfState()
{
	std::lock_guard<std::mutex> lg(this->outbound_mutex);
	this->outbound_queue.push_back({ fmt::format(
			"LOG:grf lists -- game {}, newgame {}, scanned {}, mode {}",
			_grfconfig.size(), _grfconfig_newgame.size(),
			_all_grfs.size(), (int)_game_mode) });
	for (const auto &c : _all_grfs) {
		if (c == nullptr) continue;
		this->outbound_queue.push_back({ fmt::format("LOG:on disk {:08x} v{} {}",
				std::byteswap(c->ident.grfid), c->version, c->filename) });
	}
}

/**
 * Does the player have what this seed was generated from?
 *
 * The launcher checks this too, before we even start, and that is the check
 * that matters for a normal launch. This one is the second line: standalone
 * has no launcher behind it, and a player can change their NewGRF selection
 * after connecting. Cheap to run, and the failure it prevents -- items for
 * vehicles that do not exist -- has no symptom until hours in.
 *
 * @return empty when everything needed is loaded, otherwise what is wrong.
 */
static std::string CheckRequiredGrfs(const json &slot)
{
	if (!slot.contains("required_newgrf") || !slot["required_newgrf"].is_array()) return "";

	/* What is loaded, by GRFID. */
	std::map<uint32_t, uint32_t> have;
	for (const auto &c : ActiveGrfList()) {
		if (c == nullptr) continue;
		if (c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND) continue;
		have[c->ident.grfid] = c->version;
	}

	std::string problems;
	for (const auto &req : slot["required_newgrf"]) {
		if (!req.is_object()) continue;

		std::string id_text = req.value("grfid", "");
		if (id_text.empty()) continue;
		std::string name = req.value("name", id_text);
		uint32_t min_version = req.value("min_version", 0u);

		/* The id travels as hex text; the engine holds it byte-swapped. */
		uint32_t id = 0;
		for (char ch : id_text) {
			int d = (ch >= '0' && ch <= '9') ? ch - '0'
			      : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10
			      : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
			if (d < 0) { id = 0; break; }
			id = (id << 4) | (uint32_t)d;
		}
		if (id == 0) continue;
		id = std::byteswap(id);

		auto it = have.find(id);
		if (it == have.end()) {
			problems += fmt::format("{} is not loaded. ", name);
		} else if (min_version != 0 && it->second < min_version) {
			problems += fmt::format("{} is version {}, this seed needs {}. ",
					name, it->second, min_version);
		}
	}

	if (problems.empty()) return "";
	return problems + "Open Check Online Content, install what is missing, then reconnect.";
}

/**
 * One line from the launcher. Unknown messages are ignored on purpose: it
 * lets either side gain a message before the other learns about it.
 */
void ArchipelagoClient::HandleLine(const std::string &line)
{
	if (line.empty()) return;

	size_t colon = line.find(':');
	if (colon == std::string::npos) return;

	const std::string tag  = line.substr(0, colon);
	const std::string body = line.substr(colon + 1);

	if (tag == "STATE") {
		switch (body.empty() ? -1 : body[0] - '0') {
			case 0: this->state.store(APState::DISCONNECTED); break;
			case 1: this->state.store(APState::CONNECTING); break;
			case 2:
				/* Fully logged in. AUTHENTICATED, not CONNECTED: the session
				 * gate, item flush and every live update key on it. */
				this->state.store(APState::AUTHENTICATED);
				this->PushEvent({ InboundEvent::CONNECTED, "", {}, {} });
				break;
			default: this->state.store(APState::AP_ERROR); break;
		}
		return;
	}

	if (tag == "ERROR") {
		{
			std::lock_guard<std::mutex> lg(this->slot_mutex);
			this->last_error = body;
		}
		this->state.store(APState::AP_ERROR);
		this->PushEvent({ InboundEvent::DISCONNECTED, body, {}, {} });
		return;
	}

	/* The launcher has decided the seed cannot be played -- a NewGRF the pool
	 * was built from is missing or too old. Final: we show it and stop, rather
	 * than let the player discover it hours in. */
	if (tag == "REJECT") {
		{
			std::lock_guard<std::mutex> lg(this->outbound_mutex);
			this->outbound_queue.push_back({ "LOG:refusal accepted, not starting play" });
		}
		{
			std::lock_guard<std::mutex> lg(this->slot_mutex);
			this->last_error = body;
		}
		this->state.store(APState::AP_ERROR);
		this->PushEvent({ InboundEvent::DISCONNECTED, body, {}, {} });
		return;
	}

	/* The multiworld's seed name -- the per-seed savegame key. Sent before
	 * SLOTDATA so the key exists when the start decision is made. */
	if (tag == "SEED") {
		AP_SetSeedKey(body);
		return;
	}

	if (tag == "SLOTDATA") {
		/* An old launcher sends no SEED: line; the slot_data text is stable
		 * per seed and serves as the key then. */
		AP_SetSeedKeyFallback(body);
		InboundEvent ev;
		ev.type = InboundEvent::SLOT_DATA;
		try {
			json wrapper = json::object();
			wrapper["slot_data"] = json::parse(body);
			ev.slot = ParseSlotData(wrapper);
		} catch (const json::exception &e) {
			/* A seed we cannot read is a seed we cannot play. Say so instead
			 * of starting with an empty slot_data and failing later. */
			{
				std::lock_guard<std::mutex> lg(this->slot_mutex);
				this->last_error = fmt::format("could not read slot data: {}", e.what());
			}
			this->state.store(APState::AP_ERROR);
			this->PushEvent({ InboundEvent::DISCONNECTED, this->last_error, {}, {} });
			return;
		}
		/* Second line of defence, after the launcher's own check. */
		std::string grf_problem;
		try {
			grf_problem = CheckRequiredGrfs(json::parse(body));
		} catch (const json::exception &) { /* already reported above */ }

		if (!grf_problem.empty()) {
			{
				std::lock_guard<std::mutex> lg(this->outbound_mutex);
				this->outbound_queue.push_back({ "LOG:GRF check failed: " + grf_problem });
			}
			this->ReportGrfState();
			{
				std::lock_guard<std::mutex> lg(this->slot_mutex);
				this->last_error = grf_problem;
			}
			this->state.store(APState::AP_ERROR);
			this->PushEvent({ InboundEvent::DISCONNECTED, grf_problem, {}, {} });
			return;
		}

		this->PushEvent(std::move(ev));
		this->has_slot_data.store(true);
		return;
	}

	if (tag == "ITEM") {
		/* ITEM:<id>:<index> -- index is AP's resume position, so a replayed
		 * item can be recognised as one already handled. */
		size_t sep = body.find(':');
		InboundEvent ev;
		ev.type = InboundEvent::ITEM;
		ev.item.item_id = ParseInt64(body.substr(0, sep));
		ev.item.server_index = (sep == std::string::npos)
				? -1 : ParseInt64(body.substr(sep + 1));
		this->PushEvent(std::move(ev));
		return;
	}

	/* Names of locations ALREADY checked -- resume sync for missions,
	 * stars and shop slots. */
	if (tag == "CHECKED") {
		std::set<std::string> names;
		size_t start = 0;
		while (start <= body.size()) {
			size_t comma = body.find(',', start);
			std::string one = body.substr(start, comma - start);
			if (!one.empty()) names.insert(one);
			if (comma == std::string::npos) break;
			start = comma + 1;
		}
		this->PushEvent({ InboundEvent::CHECKED_LOCATIONS, "", {}, {}, names });
		return;
	}

	if (tag == "DEATHLINK") {
		this->PushEvent({ InboundEvent::DEATH_RECEIVED, body, {}, {} });
		return;
	}

	if (tag == "PRINT" || tag == "SAY") {
		this->PushEvent({ InboundEvent::PRINT, body, {}, {} });
		return;
	}

	/* HINT:<location name>:<"player (game)"> -- the answer to SCOUT:, shown on
	 * the shop slot. Keyed by name: the game has no id table. */
	if (tag == "HINT") {
		size_t sep = body.find(':');
		if (sep == std::string::npos) return;

		std::lock_guard<std::mutex> lg(this->slot_mutex);
		this->location_hints[body.substr(0, sep)] = body.substr(sep + 1);
		return;
	}

	/* How many locations this seed has, for the "x of y" counter. */
	if (tag == "LOCCOUNT") {
		this->total_locations.store((int)ParseInt64(body));
		return;
	}

	if (tag == "PLAYERS") {
		std::lock_guard<std::mutex> lg(this->slot_mutex);
		int64_t n = 0;
		size_t start = 0;
		while (start <= body.size()) {
			size_t comma = body.find(',', start);
			std::string one = body.substr(start, comma - start);
			if (!one.empty()) this->player_id_to_name[n++] = one;
			if (comma == std::string::npos) break;
			start = comma + 1;
		}
		return;
	}
}

/**
 * The worker. Opens the pipe, announces itself, then moves lines both ways
 * until asked to stop or the launcher goes away.
 */
void ArchipelagoClient::WorkerThread()
{
	ApPipe pipe;

	const std::string name = _ap_pipe_name.empty() ? "openttd_archipelago" : _ap_pipe_name;
	if (!pipe.Open(name)) {
		{
			std::lock_guard<std::mutex> lg(this->slot_mutex);
			this->last_error = pipe.Error();
		}
		this->state.store(APState::AP_ERROR);
		this->PushEvent({ InboundEvent::DISCONNECTED, pipe.Error(), {}, {} });
		return;
	}

	pipe.SendLine("HELLO:1");

	/* The GRF list goes out before anything else, because the launcher's
	 * answer to it decides whether there is a game to play at all. */
	{
		std::lock_guard<std::mutex> lg(this->outbound_mutex);
		this->SendLoadedGrfList();
	}

	this->state.store(APState::AUTHENTICATING);

	while (!this->stop_requested.load()) {
		for (;;) {
			std::string out = this->PopOutbound();
			if (out.empty()) break;
			if (!pipe.SendLine(out)) break;
		}
		if (this->stop_requested.load()) break;

		std::string line;
		if (pipe.ReadLine(line, 100)) {
			this->HandleLine(line);
			continue;
		}
		/* No line. Quiet and dead look the same to ReadLine, so ask. */
		if (!pipe.IsOpen()) {
			{
				std::lock_guard<std::mutex> lg(this->slot_mutex);
				this->last_error = pipe.Error();
			}
			this->PushEvent({ InboundEvent::DISCONNECTED, pipe.Error(), {}, {} });
			break;
		}
	}

	pipe.Close();
	if (this->state.load() != APState::AP_ERROR) this->state.store(APState::DISCONNECTED);
}
