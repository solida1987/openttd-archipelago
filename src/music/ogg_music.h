/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ogg_music.h OGG Vorbis music playback via Windows waveOut. */

#ifndef OGG_MUSIC_H
#define OGG_MUSIC_H

#include <string>

namespace OggMusic {
	bool Play(const std::string &filename, bool loop);
	void Stop();
	bool IsPlaying();
	void SetVolume(uint8_t vol);
}

#endif /* OGG_MUSIC_H */
