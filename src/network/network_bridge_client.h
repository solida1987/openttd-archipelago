/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file network_bridge_client.h Client-side handler for connecting to the AP Bridge.
 *
 * When playing multiplayer via the Bridge architecture, each OpenTTD client
 * connects to the Bridge application on port 3980 to receive AP state
 * (items, missions, shop, infra locks, win progress, etc.) and to send
 * client actions (shop purchases, demigod tributes, chat).
 *
 * This is completely separate from the normal game server connection
 * (which still happens on port 3979 for the actual gameplay).
 */

#ifndef NETWORK_BRIDGE_CLIENT_H
#define NETWORK_BRIDGE_CLIENT_H

#include "core/tcp.h"
#include <string>
#include <deque>
#include <functional>

/**
 * Client-side TCP handler for the AP Bridge connection.
 * Uses JSON Lines over raw TCP, same as the server-side bridge socket.
 */
class BridgeClientHandler : public NetworkTCPSocketHandler {
private:
	std::string recv_buffer;           ///< Accumulates incoming data until newlines
	std::deque<std::string> send_queue; ///< Outbound JSON lines waiting to be sent

	/** Process a single complete JSON line from the Bridge. */
	void ProcessLine(const std::string &json_line);

	/* ── Event handlers (Bridge → Client) ─────────────────────────── */
	void HandleWelcome(const std::string &json);
	void HandleItem(const std::string &json);
	void HandleInfraLocks(const std::string &json);
	void HandleWinProgress(const std::string &json);
	void HandlePrint(const std::string &json);
	void HandleCheckLocation(const std::string &json);
	void HandleSlotData(const std::string &json);
	void HandleVictory();

public:
	BridgeClientHandler(SOCKET s);
	~BridgeClientHandler();

	/** True after receiving the welcome message from Bridge. */
	bool is_ready = false;

	/** True when we need to connect to the game server (deferred from HandleWelcome). */
	bool pending_game_connect = false;

	/** The game server address provided by the Bridge. */
	std::string game_server_address;

	/** Queue a JSON object for sending. */
	void SendJSON(const std::string &json);

	/** Flush the send queue. */
	void SendQueued();

	/** Read data from socket and process lines. */
	void ReceiveData();

	/* ── Commands (Client → Bridge) ───────────────────────────────── */
	void SendHello(const std::string &player_name);
	void SendShopBuy(const std::string &location_name);
	void SendDemigodTribute();
	void SendSay(const std::string &text);

	/* ── Static management ────────────────────────────────────────── */

	/** The single bridge client connection, or nullptr. */
	static BridgeClientHandler *current;

	/**
	 * Connect to the AP Bridge.
	 * @param host Bridge hostname/IP.
	 * @param port Bridge client port (default 3980).
	 * @return true on success.
	 */
	static bool Connect(const std::string &host, uint16_t port);

	/** Close the bridge client connection. */
	static void Close();

	/** Poll for data on the active connection. */
	static void Receive();

	/** Flush outgoing data. */
	static void Send();

	/** Returns true if connected to a Bridge. */
	static bool IsConnected() { return current != nullptr && current->sock != INVALID_SOCKET; }
};

#endif /* NETWORK_BRIDGE_CLIENT_H */
