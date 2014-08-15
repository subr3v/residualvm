/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "gui/error.h"

#include "common/stream.h"
#include "common/mutex.h"
#include "common/timer.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/mixer.h"
#include "engines/grim/debug.h"
#include "engines/grim/sound.h"
#include "engines/grim/grim.h"
#include "engines/grim/resource.h"
#include "engines/grim/savegame.h"
#include "engines/grim/textsplit.h"
#include "engines/grim/emi/sound/emisound.h"
#include "engines/grim/emi/sound/track.h"
#include "engines/grim/emi/sound/aifftrack.h"
#include "engines/grim/emi/sound/mp3track.h"
#include "engines/grim/emi/sound/scxtrack.h"
#include "engines/grim/emi/sound/vimatrack.h"
#include "engines/grim/movie/codecs/vima.h"

#define NUM_CHANNELS 32

namespace Grim {

EMISound *g_emiSound = nullptr;

extern uint16 imuseDestTable[];

void EMISound::timerHandler(void *refCon) {
	EMISound *emiSound = (EMISound *)refCon;
	emiSound->callback();
}

EMISound::EMISound(int fps) {
	_channels = new SoundTrack*[NUM_CHANNELS];
	for (int i = 0; i < NUM_CHANNELS; i++) {
		_channels[i] = nullptr;
	}
	_curMusicState = -1;
	_musicChannel = -1;
	_curTrackId = 0;
	_callbackFps = fps;
	vimaInit(imuseDestTable);
	initMusicTable();
	g_system->getTimerManager()->installTimerProc(timerHandler, 1000000 / _callbackFps, this, "emiSoundCallback");
}

EMISound::~EMISound() {
	g_system->getTimerManager()->removeTimerProc(timerHandler);
	freeAllChannels();
	freeLoadedSounds();
	delete[] _channels;
	delete[] _musicTable;
}

int32 EMISound::getFreeChannel() {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (_channels[i] == nullptr)
			return i;
	}
	return -1;
}

int32 EMISound::getChannelByName(const Common::String &name) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (_channels[i] && _channels[i]->getSoundName() == name)
			return i;
	}
	return -1;
}

void EMISound::freeChannel(int32 channel) {
	delete _channels[channel];
	_channels[channel] = nullptr;
}

void EMISound::freeAllChannels() {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		freeChannel(i);
	}
}

void EMISound::freeLoadedSounds() {
	for (TrackMap::iterator it = _preloadedTrackMap.begin(); it != _preloadedTrackMap.end(); ++it) {
		delete it->_value;
	}
	_preloadedTrackMap.clear();
}

bool EMISound::startVoice(const char *soundName, int volume, int pan) {
	return startSound(soundName, Audio::Mixer::kSpeechSoundType, volume, pan);
}

bool EMISound::startSfx(const char *soundName, int volume, int pan) {
	return startSound(soundName, Audio::Mixer::kSFXSoundType, volume, pan);
}

bool EMISound::startSound(const char *soundName, Audio::Mixer::SoundType soundType, int volume, int pan) {
	Common::StackLock lock(_mutex);
	int channel = getFreeChannel();
	assert(channel != -1);

	SoundTrack *track = initTrack(soundName, soundType);
	_channels[channel] = track;
	if (track) {
		track->setBalance(pan);
		track->setVolume(volume);
		track->play();
		return true;
	}
	freeChannel(channel);
	return false;
}

bool EMISound::getSoundStatus(const char *soundName) {
	int32 channel = getChannelByName(soundName);

	if (channel == -1)  // We have no such sound.
		return false;

	return _channels[channel]->isPlaying();
}

void EMISound::stopSound(const char *soundName) {
	Common::StackLock lock(_mutex);
	int32 channel = getChannelByName(soundName);
	if (channel == -1) {
		Debug::warning(Debug::Sound, "Sound track '%s' could not be found to stop", soundName);
	} else {
		freeChannel(channel);
	}
}

int32 EMISound::getPosIn16msTicks(const char *soundName) {
	int32 channel = getChannelByName(soundName);
	if (channel == -1) {
		Debug::warning(Debug::Sound, "Sound track '%s' could not be found to get ticks", soundName);
		return 0;
	} else {
		return _channels[channel]->getPos().msecs() / 16;
	}
}

void EMISound::setVolume(const char *soundName, int volume) {
	Common::StackLock lock(_mutex);
	int32 channel = getChannelByName(soundName);
	if (channel == -1) {
		Debug::warning(Debug::Sound, "Sound track '%s' could not be found to set volume", soundName);
	} else {
		_channels[channel]->setVolume(volume);
	}
}

void EMISound::setPan(const char *soundName, int pan) {
	Common::StackLock lock(_mutex);
	int32 channel = getChannelByName(soundName);
	if (channel == -1) {
		Debug::warning(Debug::Sound, "Sound track '%s' could not be found to set pan", soundName);
	} else {
		_channels[channel]->setBalance(pan * 2 - 127);
	}
}

bool EMISound::loadSfx(const char *soundName, int &id) {
	Common::StackLock lock(_mutex);
	SoundTrack *track = initTrack(soundName, Audio::Mixer::kSFXSoundType);
	if (track) {
		id = _curTrackId++;
		_preloadedTrackMap[id] = track;
		return true;
	} else {
		return false;
	}
}

void EMISound::playLoadedSound(int id) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		it->_value->play();
	} else {
		warning("EMISound::playLoadedSound called with invalid sound id");
	}
}

void EMISound::setLoadedSoundLooping(int id, bool looping) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		it->_value->setLooping(looping);
	} else {
		warning("EMISound::setLoadedSoundLooping called with invalid sound id");
	}
}

void EMISound::stopLoadedSound(int id) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		it->_value->stop();
	} else {
		warning("EMISound::stopLoadedSound called with invalid sound id");
	}
}

void EMISound::freeLoadedSound(int id) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		delete it->_value;
		_preloadedTrackMap.erase(it);
	} else {
		warning("EMISound::freeLoadedSound called with invalid sound id");
	}
}

void EMISound::setLoadedSoundVolume(int id, int volume) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		it->_value->setVolume(volume);
	} else {
		warning("EMISound::setLoadedSoundVolume called with invalid sound id");
	}
}

void EMISound::setLoadedSoundPan(int id, int pan) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		it->_value->setBalance(pan);
	} else {
		warning("EMISound::setLoadedSoundPan called with invalid sound id");
	}
}

bool EMISound::getLoadedSoundStatus(int id) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		return it->_value->isPlaying();
	}
	warning("EMISound::getLoadedSoundStatus called with invalid sound id");
	return false;
}

int EMISound::getLoadedSoundVolume(int id) {
	Common::StackLock lock(_mutex);
	TrackMap::iterator it = _preloadedTrackMap.find(id);
	if (it != _preloadedTrackMap.end()) {
		return it->_value->getVolume();
	}
	warning("EMISound::getLoadedSoundVolume called with invalid sound id");
	return false;
}

SoundTrack *EMISound::initTrack(const Common::String &soundName, Audio::Mixer::SoundType soundType, const Audio::Timestamp *start) const {
	SoundTrack *track;
	Common::String soundNameLower(soundName);
	soundNameLower.toLowercase();
	if (soundNameLower.hasSuffix(".scx")) {
		track = new SCXTrack(soundType);
	} else if (soundNameLower.hasSuffix(".m4b")) {
		track = new MP3Track(soundType);
	} else if (soundNameLower.hasSuffix(".aif")) {
		track = new AIFFTrack(soundType);
	} else {
		track = new VimaTrack();
	}

	Common::String filename;
	if (soundType == Audio::Mixer::kMusicSoundType)
		filename = _musicPrefix + soundName;
	else
		filename = soundName;

	if (track->openSound(filename, soundName, start)) {
		return track;
	}
	return nullptr;
}

bool EMISound::stateHasLooped(int stateId) {
	if (stateId == _curMusicState) {
		if (_musicChannel != -1 && _channels[_musicChannel] != nullptr) {
			return _channels[_musicChannel]->hasLooped();
		}
	} else {
		warning("EMISound::stateHasLooped called for a different music state than the current one");
	}
	return false;
}

bool EMISound::stateHasEnded(int stateId) {
	if (stateId == _curMusicState) {
		if (_musicChannel != -1 && _channels[_musicChannel] != nullptr) {
			return !_channels[_musicChannel]->isPlaying();
		}
	}
	return true;
}

void EMISound::setMusicState(int stateId) {
	Common::StackLock lock(_mutex);
	if (stateId == _curMusicState)
		return;

	Audio::Timestamp musicPos;
	int prevSync = -1;
	bool fadeMusicIn = false;
	if (_musicChannel != -1 && _channels[_musicChannel]) {
		SoundTrack *music = _channels[_musicChannel];
		if (music->isPlaying()) {
			musicPos = music->getPos();
			prevSync = music->getSync();
		}
		music->fadeOut();
		_musicChannel = -1;
		fadeMusicIn = true;
	}
	if (stateId == 0) {
		_curMusicState = 0;
		return;
	}
	if (_musicTable == nullptr) {
		Debug::debug(Debug::Sound, "No music table loaded");
		return;
	}
	if (_musicTable[stateId]._id != stateId) {
		Debug::debug(Debug::Sound, "Attempted to play track #%d, not found in music table!", stateId);
		return;
	}
	Common::String soundName;
	int sync = 0;
	if (g_grim->getGamePlatform() == Common::kPlatformPS2) {
		Debug::debug(Debug::Sound, "PS2 doesn't have musictable yet %d ignored, just playing 1195.SCX", stateId);
		// So, we just rig up the menu-song hardcoded for now, as a test of the SCX-code.
		soundName = "1195.SCX";
	} else {
		soundName = _musicTable[stateId]._filename;
		sync = _musicTable[stateId]._sync;
	}
	_curMusicState = stateId;

	_musicChannel = getFreeChannel();
	assert(_musicChannel != -1);

	Audio::Timestamp *start = nullptr;
	if (prevSync != 0 && sync != 0 && prevSync == sync)
		start = &musicPos;

	Debug::debug(Debug::Sound, "Loading music: %s", soundName.c_str());
	SoundTrack *music = initTrack(soundName, Audio::Mixer::kMusicSoundType, start);
	if (music) {
		_channels[_musicChannel] = music;
		music->play();
		music->setSync(sync);
		if (fadeMusicIn) {
			music->setFade(0.0f);
			music->fadeIn();
		}
	} else {
		freeChannel(_musicChannel);
		_musicChannel = -1;
	}
}

uint32 EMISound::getMsPos(int stateId) {
	if (_musicChannel == -1) {
		Debug::debug(Debug::Sound, "EMISound::getMsPos: No active music channel");
		return 0;
	}
	SoundTrack *music = _channels[_musicChannel];
	if (!music) {
		Debug::debug(Debug::Sound, "EMISound::getMsPos: Music track is null", stateId);
		return 0;
	}
	return music->getPos().msecs();
}

MusicEntry *initMusicTableDemo(const Common::String &filename) {
	Common::SeekableReadStream *data = g_resourceloader->openNewStreamFile(filename);

	if (!data)
		error("Couldn't open %s", filename.c_str());
	// FIXME, for now we use a fixed-size table, as I haven't looked at the retail-data yet.
	MusicEntry *musicTable = new MusicEntry[15];
	for (unsigned int i = 0; i < 15; i++)
		musicTable[i]._id = -1;

	TextSplitter *ts = new TextSplitter(filename, data);
	int id, x, y, sync;
	char musicfilename[64];
	char name[64];
	while (!ts->isEof()) {
		while (!ts->checkString("*/")) {
			while (!ts->checkString(".cuebutton"))
				ts->nextLine();

			ts->scanString(".cuebutton id %d x %d y %d sync %d \"%[^\"]64s", 5, &id, &x, &y, &sync, name);
			ts->scanString(".playfile \"%[^\"]64s", 1, musicfilename);
			musicTable[id]._id = id;
			musicTable[id]._x = x;
			musicTable[id]._y = y;
			musicTable[id]._sync = sync;
			musicTable[id]._name = name;
			musicTable[id]._filename = musicfilename;
		}
		ts->nextLine();
	}
	delete ts;
	delete data;
	return musicTable;
}

MusicEntry *initMusicTableRetail(MusicEntry *table, const Common::String &filename) {
	Common::SeekableReadStream *data = g_resourceloader->openNewStreamFile(filename);

	// Remember to check, in case we forgot to copy over those files from the CDs.
	if (!data) {
		warning("Couldn't open %s", filename.c_str());
		delete[] table;
		return nullptr;
	}
	
	MusicEntry *musicTable = table;
	if (!table) {
		musicTable = new MusicEntry[126];
		for (unsigned int i = 0; i < 126; i++) {
			musicTable[i]._id = -1;
		}
	}

	TextSplitter *ts = new TextSplitter(filename, data);
	int id, x, y, sync, trim;
	char musicfilename[64];
	char type[16];
	// Every block is followed by 3 lines of commenting/uncommenting, except the last.
	while (!ts->isEof()) {
		while (!ts->checkString("*/")) {
			while (!ts->checkString(".cuebutton"))
				ts->nextLine();

			ts->scanString(".cuebutton id %d x %d y %d sync %d type %16s", 5, &id, &x, &y, &sync, type);
			ts->scanString(".playfile trim %d \"%[^\"]64s", 2, &trim, musicfilename);
			if (musicfilename[1] == '\\')
				musicfilename[1] = '/';
			musicTable[id]._id = id;
			musicTable[id]._x = x;
			musicTable[id]._y = y;
			musicTable[id]._sync = sync;
			musicTable[id]._type = type;
			musicTable[id]._name = "";
			musicTable[id]._trim = trim;
			musicTable[id]._filename = musicfilename;
		}
		ts->nextLine();
	}
	delete ts;
	delete data;
	return musicTable;
}

void tableLoadErrorDialog(const char *filename) {
	const char *errorMessage = nullptr;
	errorMessage =  "ERROR: Missing file for music-support.\n"
	"Escape from Monkey Island has two versions of FullMonkeyMap.imt,\n"
	"you need to copy both files from both CDs to Textures/, and rename\n"
	"them as follows to get music-support in-game: \n"
	"CD 1: \"FullMonkeyMap.imt\" -> \"FullMonkeyMap1.imt\"\n"
	"CD 2: \"FullMonkeyMap.imt\" -> \"FullMonkeyMap2.imt\"";
	GUI::displayErrorDialog(errorMessage);
	error("Missing file %s", filename);
}

void EMISound::initMusicTable() {
	if (g_grim->getGameFlags() == ADGF_DEMO) {
		_musicTable = initMusicTableDemo("Music/FullMonkeyMap.imt");
		_musicPrefix = "Music/";
	} else if (g_grim->getGamePlatform() == Common::kPlatformPS2) {
		// TODO, fill this in, data is in the binary.
		//initMusicTablePS2()
		_musicTable = nullptr;
		_musicPrefix = "";
	} else {
		_musicTable = nullptr;
		_musicTable = initMusicTableRetail(_musicTable, "Textures/FullMonkeyMap1.imt");
		if (_musicTable == nullptr) {
			tableLoadErrorDialog("Textures/FullMonkeyMap1.imt");
		}
		_musicTable = initMusicTableRetail(_musicTable, "Textures/FullMonkeyMap2.imt");
		if (_musicTable == nullptr) {
			tableLoadErrorDialog("Textures/FullMonkeyMap2.imt");
		}
		_musicPrefix = "Textures/spago/"; // Default to high-quality music.
	}
}

void EMISound::selectMusicSet(int setId) {
	if (g_grim->getGamePlatform() == Common::kPlatformPS2) {
		assert(setId == 0);
		_musicPrefix = "";
		return;
	}
	if (setId == 0) {
		_musicPrefix = "Textures/spago/";
	} else if (setId == 1) {
		_musicPrefix = "Textures/mego/";
	} else {
		error("EMISound::selectMusicSet - Unknown setId %d", setId);
	}

	// Immediately switch all currently active music tracks to the new quality.
	for (uint32 i = 0; i < NUM_CHANNELS; ++i) {
		SoundTrack *track = _channels[i];
		if (track && track->getSoundType() == Audio::Mixer::kMusicSoundType) {
			_channels[i] = restartTrack(track);
			delete track;
		}
	}
	for (uint32 i = 0; i < _stateStack.size(); ++i) {
		SoundTrack *track = _stateStack[i]._track;
		if (track) {
			_stateStack[i]._track = restartTrack(track);
			delete track;
		}
	}
}

SoundTrack *EMISound::restartTrack(SoundTrack *track) {
	Audio::Timestamp pos = track->getPos();
	SoundTrack *newTrack = initTrack(track->getSoundName(), track->getSoundType(), &pos);
	if (newTrack) {
		newTrack->setVolume(track->getVolume());
		newTrack->setBalance(track->getBalance());
		newTrack->setFadeMode(track->getFadeMode());
		newTrack->setFade(track->getFade());
		if (track->isPlaying()) {
			newTrack->play();
		}
		if (track->isPaused()) {
			newTrack->pause();
		}
	}
	return newTrack;
}

void EMISound::pushStateToStack() {
	Common::StackLock lock(_mutex);
	if (_musicChannel != -1 && _channels[_musicChannel]) {
		_channels[_musicChannel]->fadeOut();
		StackEntry entry = { _curMusicState, _channels[_musicChannel] };
		_stateStack.push(entry);
		_channels[_musicChannel] = nullptr;
		_musicChannel = -1;
	} else {
		StackEntry entry = { _curMusicState, nullptr };
		_stateStack.push(entry);
	}
	_curMusicState = 0;
}

void EMISound::popStateFromStack() {
	Common::StackLock lock(_mutex);
	if (_musicChannel != -1 && _channels[_musicChannel]) {
		_channels[_musicChannel]->fadeOut();
	}

	_musicChannel = getFreeChannel();
	assert(_musicChannel != -1);

	//even pop state from stack if music isn't set
	StackEntry entry = _stateStack.pop();
	SoundTrack *track = entry._track;
	_channels[_musicChannel] = track;
	_curMusicState = entry._state;

	if (track) {
		if (track->isPaused()) {
			track->pause();
		}
		track->fadeIn();
	}
}

void EMISound::flushStack() {
	Common::StackLock lock(_mutex);
	while (!_stateStack.empty()) {
		SoundTrack *temp = _stateStack.pop()._track;
		delete temp;
	}
}

void EMISound::pause(bool paused) {
	Common::StackLock lock(_mutex);

	for (int i = 0; i < NUM_CHANNELS; i++) {
		SoundTrack *track = _channels[i];
		if (track == nullptr)
			continue;

		if (paused && track->isPaused())
			continue;
		if (!paused && !track->isPaused())
			continue;

		// Do not pause music.
		if (i == _musicChannel)
			continue;

		track->pause();
	}
}

void EMISound::callback() {
	Common::StackLock lock(_mutex);

	for (uint i = 0; i < _stateStack.size(); ++i) {
		SoundTrack *track = _stateStack[i]._track;
		if (track == nullptr || track->isPaused() || !track->isPlaying())
			continue;

		updateTrack(track);
		if (track->getFadeMode() == SoundTrack::FadeOut && track->getFade() == 0.0f) {
			track->pause();
		}
	}

	for (int i = 0; i < NUM_CHANNELS; i++) {
		SoundTrack *track = _channels[i];
		if (track == nullptr || track->isPaused() || !track->isPlaying())
			continue;

		updateTrack(track);
		if (track->getFadeMode() == SoundTrack::FadeOut && track->getFade() == 0.0f) {
			track->stop();
		}
	}
}

void EMISound::updateTrack(SoundTrack *track) {
	if (track->getFadeMode() != SoundTrack::FadeNone) {
		float fadeStep = 0.5f / _callbackFps;
		float fade = track->getFade();
		if (track->getFadeMode() == SoundTrack::FadeIn) {
			fade += fadeStep;
			if (fade > 1.0f)
				fade = 1.0f;
			track->setFade(fade);
		}
		else {
			fade -= fadeStep;
			if (fade < 0.0f)
				fade = 0.0f;
			track->setFade(fade);
		}
	}
}

void EMISound::flushTracks() {
	Common::StackLock lock(_mutex);
	for (int i = 0; i < NUM_CHANNELS; i++) {
		SoundTrack *track = _channels[i];
		if (track == nullptr)
			continue;

		if (!track->isPlaying()) {
			freeChannel(i);
		}
	}
}

void EMISound::restoreState(SaveGame *savedState) {
	Common::StackLock lock(_mutex);
	// Clear any current music
	flushStack();
	setMusicState(0);
	freeAllChannels();
	freeLoadedSounds();
	// Actually load:
	savedState->beginSection('SOUN');
	_musicPrefix = savedState->readString();
	if (savedState->saveMinorVersion() >= 21) {
		_curMusicState = savedState->readLESint32();
		_musicChannel = savedState->readLESint32();
	}

	// Stack:
	uint32 stackSize = savedState->readLEUint32();
	for (uint32 i = 0; i < stackSize; i++) {
		SoundTrack *track = nullptr;
		int state = 0;
		if (savedState->saveMinorVersion() >= 21) {
			state = savedState->readLESint32();
			bool hasTrack = savedState->readBool();
			if (hasTrack) {
				track = restoreTrack(savedState);
			}
		} else {
			Common::String soundName = savedState->readString();
			track = initTrack(soundName, Audio::Mixer::kMusicSoundType);
			if (track) {
				track->play();
				track->pause();
			}
		}
		StackEntry entry = { state, track };
		_stateStack.push(entry);
	}

	if (savedState->saveMinorVersion() < 21) {
		// Old savegame format stored the music channel separately.
		uint32 hasActiveTrack = savedState->readLEUint32();
		if (hasActiveTrack) {
			_musicChannel = getFreeChannel();
			assert(_musicChannel != -1);
			Common::String soundName = savedState->readString();
			_channels[_musicChannel] = initTrack(soundName, Audio::Mixer::kMusicSoundType);
			if (_channels[_musicChannel]) {
				_channels[_musicChannel]->play();
			} else {
				error("Couldn't reopen %s", soundName.c_str());
			}
		}
	}

	// Channels:
	uint32 numChannels = savedState->readLEUint32();
	if (numChannels > NUM_CHANNELS) {
		error("Save game made with more channels than we have now: %d > %d", numChannels, NUM_CHANNELS);
	}
	for (uint32 i = 0; i < numChannels; i++) {
		bool channelIsActive;
		if (savedState->saveMinorVersion() >= 21)
			channelIsActive = savedState->readBool();
		else
			channelIsActive = (savedState->readLESint32() != 0);
		if (channelIsActive) {
			_channels[i] = restoreTrack(savedState);
		}
	}

	// Preloaded sounds:
	if (savedState->saveMinorVersion() >= 21) {
		_curTrackId = savedState->readLESint32();
		uint32 numLoaded = savedState->readLEUint32();
		for (uint32 i = 0; i < numLoaded; ++i) {
			int id = savedState->readLESint32();
			_preloadedTrackMap[id] = restoreTrack(savedState);
		}
	}

	savedState->endSection();
}

void EMISound::saveState(SaveGame *savedState) {
	Common::StackLock lock(_mutex);
	savedState->beginSection('SOUN');
	savedState->writeString(_musicPrefix);
	savedState->writeLESint32(_curMusicState);
	savedState->writeLESint32(_musicChannel);

	// Stack:
	uint32 stackSize = _stateStack.size();
	savedState->writeLEUint32(stackSize);
	for (uint32 i = 0; i < stackSize; i++) {
		savedState->writeLESint32(_stateStack[i]._state);
		if (!_stateStack[i]._track) {
			savedState->writeBool(false);
		} else {
			savedState->writeBool(true);
			saveTrack(_stateStack[i]._track, savedState);
		}
	}

	// Channels:
	uint32 numChannels = NUM_CHANNELS;
	savedState->writeLEUint32(numChannels);
	for (uint32 i = 0; i < numChannels; i++) {
		if (!_channels[i]) {
			savedState->writeBool(false);
		} else {
			savedState->writeBool(true);
			saveTrack(_channels[i], savedState);
		}
	}

	// Preloaded sounds:
	savedState->writeLESint32(_curTrackId);
	uint32 numLoaded = _preloadedTrackMap.size();
	savedState->writeLEUint32(numLoaded);
	for (TrackMap::iterator it = _preloadedTrackMap.begin(); it != _preloadedTrackMap.end(); ++it) {
		savedState->writeLESint32(it->_key);
		saveTrack(it->_value, savedState);
	}

	savedState->endSection();
}

void EMISound::saveTrack(SoundTrack *track, SaveGame *savedState) {
	savedState->writeString(track->getSoundName());
	savedState->writeLEUint32(track->getVolume());
	savedState->writeLEUint32(track->getBalance());
	savedState->writeLEUint32(track->getPos().msecs());
	savedState->writeBool(track->isPlaying());
	savedState->writeBool(track->isPaused());
	savedState->writeLESint32((int)track->getSoundType());
	savedState->writeLESint32((int)track->getFadeMode());
	savedState->writeFloat(track->getFade());
	savedState->writeLESint32(track->getSync());
}

SoundTrack *EMISound::restoreTrack(SaveGame *savedState) {
	Common::String soundName = savedState->readString();
	int volume = savedState->readLESint32();
	int balance = savedState->readLESint32();
	Audio::Timestamp pos(savedState->readLESint32());
	bool playing = savedState->readBool();
	if (savedState->saveMinorVersion() < 21) {
		SoundTrack *track = initTrack(soundName, Audio::Mixer::kSpeechSoundType);
		if (track)
			track->play();
		return track;
	}
	bool paused = savedState->readBool();
	Audio::Mixer::SoundType soundType = (Audio::Mixer::SoundType)savedState->readLESint32();
	SoundTrack::FadeMode fadeMode = (SoundTrack::FadeMode)savedState->readLESint32();
	float fade = savedState->readFloat();
	int sync = savedState->readLESint32();

	SoundTrack *track = initTrack(soundName, soundType, &pos);
	track->setVolume(volume);
	track->setBalance(balance);
	if (playing)
		track->play();
	if (paused)
		track->pause();
	track->setFadeMode(fadeMode);
	track->setFade(fade);
	track->setSync(sync);
	return track;
}

} // end of namespace Grim
