/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file network_bridge.h Server-side handler for the AP Bridge controller.
 *
 * The AP Bridge is a standalone application that connects to the Archipelago
 * multiworld server and controls this dedicated OpenTTD server via a simple
 * JSON Lines protocol over TCP.  Only ONE bridge connection is accepted at a
 * time.  The bridge sends commands (engine unlocks, money changes, world
 * generation, etc.) and the server sends periodic stats and game events.
 */

#ifndef NETWORK_BRIDGE_H
#define NETWORK_BRIDGE_H

#include "core/tcp.h"
#include <string>
#include <deque>

/** Global flag: true when an AP Bridge is connected and controlling this server. */
extern bool _ap_bridge_mode;

/**
 * Load AP slot data from ap_slot_data.json next to the executable.
 * Called from dedicated_v.cpp BEFORE the first world generation.
 * Returns true if file was found and parsed — caller should then call
 * AP_ConsumeWorldStart() and use AP_GetWorldSeed() for world gen.
 */
bool BridgeLoadSlotDataFromFile();

/**
 * Server-side socket handler for the AP Bridge connection.
 * Uses JSON Lines (newline-delimited JSON) over raw TCP.
 * Only one instance exists at a time (singleton pattern).
 */
class ServerNetworkBridgeHandler : public NetworkTCPSocketHandler {
private:
	std::string recv_buffer;          ///< Accumulates incoming data until newlines are found
	std::deque<std::string> send_queue; ///< Outbound JSON lines waiting to be sent

	/** Process a single complete JSON line from the bridge. */
	void ProcessLine(const std::string &json_line);

	/* ── Command handlers ───────────────────────────────────────────── */
	void HandleHello(const std::string &json);
	void HandleGenWorld(const std::string &json);
	void HandleUnlockEngine(const std::string &json);
	void HandleChangeMoney(const std::string &json);
	void HandleApplyTrap(const std::string &json);
	void HandleUnlockInfra(const std::string &json);
	void HandleSetSpeed(const std::string &json);
	void HandleSpawnRuin(const std::string &json);
	void HandleSpawnDemigod(const std::string &json);
	void HandleDefeatDemigod(const std::string &json);
	void HandleTriggerColby(const std::string &json);
	void HandleSendChat(const std::string &json);
	void HandlePing();

public:
	ServerNetworkBridgeHandler(SOCKET s);
	~ServerNetworkBridgeHandler();

	/** Queue a JSON object for sending (adds trailing newline). */
	void SendJSON(const std::string &json);

	/** Flush the send queue to the socket. */
	void SendQueued();

	/** Read available data from socket into recv_buffer and process complete lines. */
	void ReceiveData();

	/* ── Periodic event senders (called from game loop) ─────────────── */
	void SendCompanyStats();
	void SendPopulation();

	/* ── Immediate event senders ────────────────────────────────────── */
	void SendCheckLocation(const std::string &location_name);
	void SendShopPurchase(const std::string &location_name);
	void SendDeathEvent(const std::string &cause);
	void SendClientJoined(uint32_t client_id, const std::string &name, int company);
	void SendClientLeft(uint32_t client_id);
	void SendWorldReady(uint32_t seed);

	/* ── Static management ──────────────────────────────────────────── */

	/** The single active bridge connection, or nullptr. */
	static ServerNetworkBridgeHandler *current;

	/** Listening socket for incoming bridge connections. */
	static SOCKET listen_socket;

	/** Start listening on the bridge port.  Returns false on failure. */
	static bool Listen(uint16_t port);

	/** Stop listening and close any active connection. */
	static void CloseAll();

	/** Accept a pending connection if one is waiting. */
	static void AcceptConnection();

	/** Poll for incoming data on the active connection. */
	static void Receive();

	/** Flush outgoing data on the active connection. */
	static void Send();

	/** Returns true if a bridge is currently connected. */
	static bool IsConnected() { return current != nullptr; }
};

#endif /* NETWORK_BRIDGE_H */
