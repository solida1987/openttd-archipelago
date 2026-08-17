/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ogg_music.cpp OGG Vorbis music streaming playback via Windows waveOut API. */

#include "../stdafx.h"
#include "ogg_music.h"
#include "../debug.h"

#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

/* stb_vorbis header-only include (implementation compiled in stb_vorbis_impl.cpp) */
#define STB_VORBIS_HEADER_ONLY
#include "../3rdparty/stb_vorbis/stb_vorbis.c"

#include "../safeguards.h"

namespace OggMusic {

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/
static constexpr int NUM_BUFFERS     = 3;      ///< Triple-buffered for smooth streaming
static constexpr int SAMPLES_PER_BUF = 8192;   ///< Samples per channel per buffer

/* ---------------------------------------------------------------------------
 * State
 * ---------------------------------------------------------------------------*/
static std::mutex              _lock;
static std::condition_variable _cv;
static std::thread             _thread;
static std::atomic<bool>       _playing{false};
static std::atomic<bool>       _stop_requested{false};
static std::atomic<uint8_t>    _volume{127};

/* ---------------------------------------------------------------------------
 * waveOut callback -- signals the streaming thread when a buffer completes
 * ---------------------------------------------------------------------------*/
static void CALLBACK WaveOutCallback(HWAVEOUT, UINT uMsg, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
	if (uMsg == WOM_DONE) {
		_cv.notify_one();
	}
}

/* ---------------------------------------------------------------------------
 * Apply volume to waveOut device (0-127 -> 0-0xFFFF stereo)
 * ---------------------------------------------------------------------------*/
static void ApplyVolume(HWAVEOUT hwo)
{
	uint16_t vol16 = static_cast<uint16_t>(_volume.load() * 0xFFFF / 127);
	DWORD dw = MAKELONG(vol16, vol16);
	waveOutSetVolume(hwo, dw);
}

/* ---------------------------------------------------------------------------
 * Streaming thread -- decodes OGG and feeds waveOut
 * ---------------------------------------------------------------------------*/
static void StreamThread(std::string filename, bool loop)
{
	Debug(driver, 2, "OGG Music: opening '{}'", filename);

	int vorbis_error = 0;
	stb_vorbis *vorbis = stb_vorbis_open_filename(filename.c_str(), &vorbis_error, nullptr);
	if (vorbis == nullptr) {
		Debug(driver, 0, "OGG Music: failed to open '{}' (error {})", filename, vorbis_error);
		_playing.store(false);
		return;
	}

	stb_vorbis_info info = stb_vorbis_get_info(vorbis);
	int channels = info.channels;
	int sample_rate = info.sample_rate;

	Debug(driver, 2, "OGG Music: {}Hz, {} channels", sample_rate, channels);

	/* Set up waveOut format */
	WAVEFORMATEX wfx{};
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = static_cast<WORD>(channels);
	wfx.nSamplesPerSec  = static_cast<DWORD>(sample_rate);
	wfx.wBitsPerSample  = 16;
	wfx.nBlockAlign     = static_cast<WORD>(channels * 2);
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize          = 0;

	HWAVEOUT hwo = nullptr;
	MMRESULT res = waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD_PTR)WaveOutCallback, 0, CALLBACK_FUNCTION);
	if (res != MMSYSERR_NOERROR) {
		Debug(driver, 0, "OGG Music: waveOutOpen failed (error {})", res);
		stb_vorbis_close(vorbis);
		_playing.store(false);
		return;
	}

	ApplyVolume(hwo);

	/* Allocate buffers */
	size_t buf_samples = SAMPLES_PER_BUF * channels;
	size_t buf_bytes   = buf_samples * sizeof(short);
	WAVEHDR headers[NUM_BUFFERS]{};
	std::vector<short> pcm_data(NUM_BUFFERS * buf_samples);

	for (int i = 0; i < NUM_BUFFERS; i++) {
		headers[i].lpData         = reinterpret_cast<LPSTR>(&pcm_data[i * buf_samples]);
		headers[i].dwBufferLength = static_cast<DWORD>(buf_bytes);
		headers[i].dwFlags        = 0;
	}

	/* Fill and queue initial buffers */
	int queued = 0;
	for (int i = 0; i < NUM_BUFFERS && !_stop_requested.load(); i++) {
		int samples = stb_vorbis_get_samples_short_interleaved(
			vorbis, channels,
			reinterpret_cast<short *>(headers[i].lpData),
			SAMPLES_PER_BUF * channels
		);
		if (samples == 0) {
			if (loop) {
				stb_vorbis_seek_start(vorbis);
				samples = stb_vorbis_get_samples_short_interleaved(
					vorbis, channels,
					reinterpret_cast<short *>(headers[i].lpData),
					SAMPLES_PER_BUF * channels
				);
			}
			if (samples == 0) break;
		}
		headers[i].dwBufferLength = static_cast<DWORD>(samples * channels * sizeof(short));
		waveOutPrepareHeader(hwo, &headers[i], sizeof(WAVEHDR));
		waveOutWrite(hwo, &headers[i], sizeof(WAVEHDR));
		queued++;
	}

	/* Streaming loop */
	while (!_stop_requested.load() && queued > 0) {
		/* Wait for a buffer to complete */
		{
			std::unique_lock<std::mutex> lk(_lock);
			_cv.wait_for(lk, std::chrono::milliseconds(100));
		}

		/* Update volume if changed */
		ApplyVolume(hwo);

		/* Refill completed buffers */
		for (int i = 0; i < NUM_BUFFERS && !_stop_requested.load(); i++) {
			if (!(headers[i].dwFlags & WHDR_DONE)) continue;

			waveOutUnprepareHeader(hwo, &headers[i], sizeof(WAVEHDR));
			headers[i].dwFlags = 0;
			queued--;

			int samples = stb_vorbis_get_samples_short_interleaved(
				vorbis, channels,
				reinterpret_cast<short *>(headers[i].lpData),
				SAMPLES_PER_BUF * channels
			);
			if (samples == 0) {
				if (loop) {
					stb_vorbis_seek_start(vorbis);
					samples = stb_vorbis_get_samples_short_interleaved(
						vorbis, channels,
						reinterpret_cast<short *>(headers[i].lpData),
						SAMPLES_PER_BUF * channels
					);
				}
				if (samples == 0) continue; /* End of file, no loop */
			}

			headers[i].dwBufferLength = static_cast<DWORD>(samples * channels * sizeof(short));
			waveOutPrepareHeader(hwo, &headers[i], sizeof(WAVEHDR));
			waveOutWrite(hwo, &headers[i], sizeof(WAVEHDR));
			queued++;
		}
	}

	/* Cleanup */
	waveOutReset(hwo);
	for (int i = 0; i < NUM_BUFFERS; i++) {
		if (headers[i].dwFlags & WHDR_PREPARED) {
			waveOutUnprepareHeader(hwo, &headers[i], sizeof(WAVEHDR));
		}
	}
	waveOutClose(hwo);
	stb_vorbis_close(vorbis);

	_playing.store(false);
	Debug(driver, 2, "OGG Music: playback ended");
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/
bool Play(const std::string &filename, bool loop)
{
	Stop(); /* Stop any current playback first */

	_stop_requested.store(false);
	_playing.store(true);

	_thread = std::thread(StreamThread, filename, loop);
	_thread.detach();

	Debug(driver, 2, "OGG Music: Play() started for '{}'", filename);
	return true;
}

void Stop()
{
	if (_playing.load()) {
		_stop_requested.store(true);
		_cv.notify_all();

		/* Wait briefly for the thread to finish cleanup */
		for (int i = 0; i < 50 && _playing.load(); i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		Debug(driver, 2, "OGG Music: Stop() complete");
	}
}

bool IsPlaying()
{
	return _playing.load();
}

void SetVolume(uint8_t vol)
{
	_volume.store(vol);
}

} /* namespace OggMusic */
