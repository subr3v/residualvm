/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef SUBTITLES_H_
#define SUBTITLES_H_

#include "engines/myst3/gfx.h"

#include "common/array.h"

namespace Myst3 {

class Myst3Engine;

class Subtitles : public Drawable {
public:
	Subtitles(Myst3Engine *vm, uint32 id);
	virtual ~Subtitles();

	void setFrame(int32 frame);
	void drawOverlay();

private:
	struct Phrase {
		uint32 offset;
		int32 frame;
		Common::String string;
	};

	Myst3Engine *_vm;
	uint32 _id;

	Common::Array<Phrase> _phrases;

	int32 _frame;
	Graphics::Surface *_surface;
	Texture *_texture;
};

} /* namespace Myst3 */
#endif /* SUBTITLES_H_ */
