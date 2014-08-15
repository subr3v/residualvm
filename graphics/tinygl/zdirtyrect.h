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

#ifndef GRAPHICS_TINYGL_ZRECT_H_
#define GRAPHICS_TINYGL_ZRECT_H_

#include "common/rect.h"
#include "graphics/tinygl/zblit.h"
#include "common/array.h"

namespace TinyGL {
	struct GLContext;
	struct GLVertex;
	struct GLTexture;
}

namespace Internal {
	void *allocateFrame(int size);
}

namespace Graphics {


enum DrawCallType {
	DrawCall_Rasterization,
	DrawCall_Blitting,
	DrawCall_Clear,
};

class DrawCall {
public:
	DrawCall(DrawCallType type);
	virtual ~DrawCall() { }
	bool operator==(const DrawCall &other) const;
	virtual void execute(bool restoreState) const = 0;
	virtual void execute(const Common::Rect &clippingRectangle, bool restoreState) const = 0;
	DrawCallType getType() const { return _type; }
	virtual const Common::Rect getDirtyRegion() const = 0;
private:
	DrawCallType _type;
};

class ClearBufferDrawCall : public DrawCall {
public:
	ClearBufferDrawCall(bool clearZBuffer, int zValue, bool clearColorBuffer, int rValue, int gValue, int bValue);
	bool operator==(const ClearBufferDrawCall &other) const;
	virtual void execute(bool restoreState) const;
	virtual void execute(const Common::Rect &clippingRectangle, bool restoreState) const;
	virtual const Common::Rect getDirtyRegion() const;

	void* operator new (size_t size) {
		return ::Internal::allocateFrame(size);
	}

	void operator delete (void *p) { }
private:
	bool _clearZBuffer, _clearColorBuffer;
	int _rValue, _gValue, _bValue, _zValue;
};

class RasterizationDrawCall : public DrawCall {
public:
	RasterizationDrawCall();
	~RasterizationDrawCall();
	bool operator==(const RasterizationDrawCall &other) const;
	virtual void execute(bool restoreState) const;
	virtual void execute(const Common::Rect &clippingRectangle, bool restoreState) const;
	virtual const Common::Rect getDirtyRegion() const;

	void* operator new (size_t size) {
		return ::Internal::allocateFrame(size);
	}

	void operator delete (void *p) { }
private:
	typedef void (*gl_draw_triangle_func_ptr)(TinyGL::GLContext *c, TinyGL::GLVertex *p0, TinyGL::GLVertex *p1, TinyGL::GLVertex *p2);
	void computeDirtyRegion();
	Common::Rect _dirtyRegion;
	int _vertexCount;
	TinyGL::GLVertex *_vertex;
	gl_draw_triangle_func_ptr _drawTriangleFront, _drawTriangleBack; 

	struct RasterizationState {
		int beginType;
		int currentFrontFace;
		int cullFaceEnabled;
		int colorMask;
		int depthTest;
		int depthFunction;
		int depthWrite;
		int shadowMode;
		int texture2DEnabled;
		int currentShadeModel;
		int polygonModeBack;
		int polygonModeFront;
		int lightingEnabled;
		bool enableBlending;
		int sfactor, dfactor;
		int textureVersion;
		unsigned int currentColor[4];
		float viewportTranslation[3];
		float viewportScaling[3];
		bool alphaTest;
		int alphaFunc, alphaRefValue;
		TinyGL::GLTexture *texture;
		unsigned char *shadowMaskBuf;

		bool operator==(const RasterizationState &other) const;
	};

	RasterizationState _state;

	RasterizationState loadState() const;
	void applyState(const RasterizationState &state) const;
};

class BlittingDrawCall : public DrawCall {
public:
	enum BlittingMode {
		BlitMode_Regular,
		BlitMode_NoBlend,
		BlitMode_Fast,
		BlitMode_ZBuffer,
	};

	BlittingDrawCall(BlitImage *image, const BlitTransform &transform, BlittingMode blittingMode);
	bool operator==(const BlittingDrawCall &other) const;
	virtual void execute(bool restoreState) const;
	virtual void execute(const Common::Rect &clippingRectangle, bool restoreState) const;
	virtual const Common::Rect getDirtyRegion() const;

	BlittingMode getBlittingMode() const { return _mode; }
	
	void* operator new (size_t size) {
		return ::Internal::allocateFrame(size);
	}

	void operator delete (void *p) { }
private:
	BlitImage *_image;
	BlitTransform _transform;
	BlittingMode _mode;
	int _imageVersion;

	struct BlittingState {
		bool enableBlending;
		int sfactor, dfactor;
		bool alphaTest;
		int alphaFunc, alphaRefValue;

		bool operator==(const BlittingState &other) const {
			return	enableBlending == other.enableBlending &&
					sfactor == other.sfactor &&
					dfactor == other.dfactor &&
					alphaTest == other.alphaTest &&
					alphaFunc == other.alphaFunc && 
					alphaRefValue == other.alphaRefValue;
		}
	};

	BlittingState loadState() const;
	void applyState(const BlittingState &state) const;

	BlittingState _blitState;
};

} // end of namespace Graphics

#endif
