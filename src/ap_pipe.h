/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ap_pipe.h Line-based named-pipe link to the Multiworld Launcher.
 *
 * Replaces the sockets, the two TLS backends and the WebSocket framing that
 * used to live in archipelago.cpp. The launcher holds the Archipelago
 * connection now; this only has to move lines of text between two processes.
 *
 * Protocol: docs/ap_pipe_protocol.md
 */

#ifndef AP_PIPE_H
#define AP_PIPE_H

#include <string>
#include <atomic>

/**
 * One end of the launcher pipe. Blocking, single-threaded: the caller owns it
 * from its worker thread, exactly as the socket code was owned before.
 */
class ApPipe {
public:
	ApPipe() = default;
	~ApPipe();

	ApPipe(const ApPipe &) = delete;
	ApPipe &operator=(const ApPipe &) = delete;

	/**
	 * Open \\.\pipe\<name>. Waits up to \a timeout_ms for the launcher to
	 * create it, because the game may start before the server is listening.
	 *
	 * @param abort optional stop flag, polled during the wait. Disconnect()
	 *        joins this thread from the main thread, so without it a shutdown
	 *        during the retry loop froze the game for the whole timeout.
	 * @return true on success; Error() explains a false.
	 */
	bool Open(const std::string &name, int timeout_ms = 10000,
			const std::atomic<bool> *abort = nullptr);

	void Close();
	bool IsOpen() const { return this->handle != nullptr; }

	/** Append "\n" and write. False means the link is gone. */
	bool SendLine(const std::string &line);

	/**
	 * One line without its terminator.
	 * @param out       receives the line when the result is true
	 * @param timeout_ms 0 polls, so a caller can check its own stop flag
	 * @return true when a line was read; false on timeout AND on failure —
	 *         IsOpen() separates the two, because a quiet link and a dead one
	 *         must not look the same to the caller.
	 */
	bool ReadLine(std::string &out, int timeout_ms = 100);

	const std::string &Error() const { return this->last_error; }

private:
	void       *handle{ nullptr };   ///< HANDLE, kept opaque to avoid windows.h here
	std::string buffer;              ///< bytes read but not yet a whole line
	std::string last_error;
};

#endif /* AP_PIPE_H */
