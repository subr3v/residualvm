/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "common/mutex.h"
#include "common/str.h"
#include "common/stream.h"
#include "audio/mixer.h"
#include "engines/grim/emisound/track.h"

namespace Grim {

SoundTrack::SoundTrack() {
	_stream = NULL;
}
	
Common::String SoundTrack::getSoundName() {
	return _soundName;
}

void SoundTrack::setSoundName(Common::String name) {
	_soundName = name;
}
	
bool SoundTrack::play() {
	if (_stream) {
		g_system->getMixer()->playStream(_soundType, _handle, _stream);
		return true;
	}
	return false;
}

void SoundTrack::stop() {
	if (_handle)
		g_system->getMixer()->stopHandle(*_handle);
}
} // end of namespace Grim
