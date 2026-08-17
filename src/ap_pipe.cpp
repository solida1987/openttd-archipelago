/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ap_pipe.cpp Implementation of the launcher pipe link. */

#include "stdafx.h"
#include "ap_pipe.h"

#include "3rdparty/fmt/format.h"

#if defined(_WIN32)
#	include <windows.h>
#endif

#include "safeguards.h"

#if defined(_WIN32)

/** Overlapped I/O so a read can time out without blocking the worker forever. */
struct ApPipeOverlapped {
	OVERLAPPED ov{};
	bool       pending{ false };
	char       chunk[4096]{};
};

static ApPipeOverlapped _ap_read;

ApPipe::~ApPipe()
{
	this->Close();
}

bool ApPipe::Open(const std::string &name, int timeout_ms)
{
	this->Close();
	this->last_error.clear();

	std::string path = "\\\\.\\pipe\\" + name;

	/* The launcher creates the pipe before it starts us, but "before" is a
	 * race on a loaded machine: retry rather than fail on the first miss. */
	const int step = 100;
	for (int waited = 0; ; waited += step) {
		HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
				0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			DWORD mode = PIPE_READMODE_BYTE;
			SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
			this->handle = h;
			this->buffer.clear();
			_ap_read.pending = false;
			return true;
		}

		DWORD err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PIPE_BUSY) {
			this->last_error = fmt::format("could not open {} (error {})", path, err);
			return false;
		}
		if (waited >= timeout_ms) {
			this->last_error = fmt::format("the launcher did not open {} in time", path);
			return false;
		}
		Sleep(step);
	}
}

void ApPipe::Close()
{
	if (this->handle != nullptr) {
		if (_ap_read.pending) {
			CancelIo(static_cast<HANDLE>(this->handle));
			_ap_read.pending = false;
		}
		CloseHandle(static_cast<HANDLE>(this->handle));
		this->handle = nullptr;
	}
	if (_ap_read.ov.hEvent != nullptr) {
		CloseHandle(_ap_read.ov.hEvent);
		_ap_read.ov.hEvent = nullptr;
	}
	this->buffer.clear();
}

bool ApPipe::SendLine(const std::string &line)
{
	if (this->handle == nullptr) return false;

	std::string out = line;
	out += '\n';

	const char *p = out.c_str();
	DWORD remaining = static_cast<DWORD>(out.size());
	while (remaining > 0) {
		DWORD written = 0;
		OVERLAPPED ov{};
		ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
		BOOL ok = WriteFile(static_cast<HANDLE>(this->handle), p, remaining, &written, &ov);
		if (!ok && GetLastError() == ERROR_IO_PENDING) {
			ok = GetOverlappedResult(static_cast<HANDLE>(this->handle), &ov, &written, TRUE);
		}
		CloseHandle(ov.hEvent);

		if (!ok || written == 0) {
			this->last_error = "the launcher closed the pipe";
			this->Close();
			return false;
		}
		p += written;
		remaining -= written;
	}
	return true;
}

bool ApPipe::ReadLine(std::string &out, int timeout_ms)
{
	if (this->handle == nullptr) return false;

	for (;;) {
		/* A whole line already in hand? Hand it over before touching the pipe. */
		size_t nl = this->buffer.find('\n');
		if (nl != std::string::npos) {
			out = this->buffer.substr(0, nl);
			this->buffer.erase(0, nl + 1);
			if (!out.empty() && out.back() == '\r') out.pop_back();
			return true;
		}

		if (_ap_read.ov.hEvent == nullptr) {
			_ap_read.ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
		}

		if (!_ap_read.pending) {
			ResetEvent(_ap_read.ov.hEvent);
			_ap_read.ov.Offset = _ap_read.ov.OffsetHigh = 0;
			DWORD read = 0;
			BOOL ok = ReadFile(static_cast<HANDLE>(this->handle), _ap_read.chunk,
					sizeof(_ap_read.chunk), &read, &_ap_read.ov);
			if (ok) {
				this->buffer.append(_ap_read.chunk, read);
				continue;                       /* may now hold a full line */
			}
			if (GetLastError() != ERROR_IO_PENDING) {
				this->last_error = "the launcher closed the pipe";
				this->Close();
				return false;
			}
			_ap_read.pending = true;
		}

		DWORD wait = WaitForSingleObject(_ap_read.ov.hEvent, static_cast<DWORD>(timeout_ms));
		if (wait == WAIT_TIMEOUT) return false;  /* still open, just quiet */

		DWORD read = 0;
		if (!GetOverlappedResult(static_cast<HANDLE>(this->handle), &_ap_read.ov, &read, FALSE)) {
			_ap_read.pending = false;
			this->last_error = "the launcher closed the pipe";
			this->Close();
			return false;
		}
		_ap_read.pending = false;
		if (read == 0) {
			this->last_error = "the launcher closed the pipe";
			this->Close();
			return false;
		}
		this->buffer.append(_ap_read.chunk, read);
	}
}

#else /* !_WIN32 */

/* Named pipes are a Windows facility and the launcher is Windows-only. On
 * other platforms the game keeps working; it simply never links up. */

ApPipe::~ApPipe() = default;

bool ApPipe::Open(const std::string &, int)
{
	this->last_error = "the launcher link is only available on Windows";
	return false;
}

void ApPipe::Close() {}
bool ApPipe::SendLine(const std::string &) { return false; }
bool ApPipe::ReadLine(std::string &, int) { return false; }

#endif /* _WIN32 */
