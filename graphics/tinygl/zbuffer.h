#ifndef GRAPHICS_TINYGL_ZBUFFER_H_
#define GRAPHICS_TINYGL_ZBUFFER_H_

#include "graphics/pixelbuffer.h"
#include "graphics/tinygl/gl.h"

namespace TinyGL {

// Z buffer

#define ZB_Z_BITS 16

#define ZB_POINT_Z_FRAC_BITS 14

#define ZB_POINT_S_MIN ( (1 << 13) )
#define ZB_POINT_S_MAX ( (1 << 22) - (1 << 13) )
#define ZB_POINT_T_MIN ( (1 << 21) )
#define ZB_POINT_T_MAX ( (1 << 30) - (1 << 21) )

#define ZB_POINT_RED_MIN ( (1 << 10) )
#define ZB_POINT_RED_MAX ( (1 << 16) - (1 << 10) )
#define ZB_POINT_GREEN_MIN ( (1 << 9) )
#define ZB_POINT_GREEN_MAX ( (1 << 16) - (1 << 9) )
#define ZB_POINT_BLUE_MIN ( (1 << 10) )
#define ZB_POINT_BLUE_MAX ( (1 << 16) - (1 << 10) )
#define ZB_POINT_ALPHA_MIN ( (1 << 10) )
#define ZB_POINT_ALPHA_MAX ( (1 << 16) - (1 << 10) )

// display modes
#define ZB_MODE_5R6G5B  1  // true color 16 bits

#define RGB_TO_PIXEL(r, g, b) cmode.ARGBToColor(255, r, g, b) // Default to 255 alpha aka solid colour.
typedef byte PIXEL;

#define PSZSH 4

extern uint8 PSZB;

struct Buffer {
	byte *pbuf;
	unsigned int *zbuf;
	bool used;
};

struct ZBufferPoint {
	int x, y, z;   // integer coordinates in the zbuffer
	int s, t;      // coordinates for the mapping
	int r, g, b, a;   // color indexes

	float sz, tz;  // temporary coordinates for mapping
};

struct FrameBuffer {
	FrameBuffer(int xsize, int ysize, const Graphics::PixelBuffer &frame_buffer);
	~FrameBuffer();

	Buffer *genOffscreenBuffer();
	void delOffscreenBuffer(Buffer *buffer);
	void clear(int clear_z, int z, int clear_color, int r, int g, int b);

	byte *getPixelBuffer() {
		return pbuf.getRawBuffer(0);
	}

	FORCEINLINE void readPixelRGB(int pixel, byte &r, byte &g, byte &b) {
		pbuf.getRGBAt(pixel, r, g, b);
	}

	FORCEINLINE void writePixel(int pixel, int value) {
		if (_blendingEnabled == false) {
			this->pbuf.setPixelAt(pixel, value);
		} else {
			byte rSrc, gSrc, bSrc, aSrc;
			this->pbuf.getFormat().colorToARGB(value, aSrc, rSrc, gSrc, bSrc);
			writePixel(pixel,aSrc,rSrc,gSrc,bSrc);
		}
	}

	FORCEINLINE void writePixel(int pixel, byte rSrc, byte gSrc, byte bSrc) {
		writePixel(pixel, 255, rSrc, gSrc, bSrc);
	}

	FORCEINLINE void writePixel(int pixel, byte aSrc, byte rSrc, byte gSrc, byte bSrc) {
		if (_blendingEnabled == false) {
			this->pbuf.setPixelAt(pixel, aSrc, rSrc, gSrc, bSrc);
		} else {
			byte rDst, gDst, bDst, aDst;
			switch (_sourceBlendingFactor) {
			case TGL_ZERO:
				rSrc = gSrc = bSrc = 0;
				break;
			case TGL_ONE:
				break;
			case TGL_DST_COLOR:
				this->pbuf.getRGBAt(pixel, rDst, gDst, bDst);
				rSrc = (rDst * rSrc) >> 8;
				gSrc = (gDst * gSrc) >> 8;
				bSrc = (bDst * bSrc) >> 8;
				break;
			case TGL_ONE_MINUS_DST_COLOR:
				this->pbuf.getRGBAt(pixel, rDst, gDst, bDst);
				rSrc = (rSrc * (255 - rDst)) >> 8;
				gSrc = (gSrc * (255 - gDst)) >> 8;
				bSrc = (bSrc * (255 - bDst)) >> 8;
				break;
			case TGL_SRC_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rSrc = (rSrc * aSrc) >> 8;
				gSrc = (gSrc * aSrc) >> 8;
				bSrc = (bSrc * aSrc) >> 8;
				break;
			case TGL_ONE_MINUS_SRC_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rSrc = (rSrc * (255 - aSrc)) >> 8;
				gSrc = (gSrc * (255 - aSrc)) >> 8;
				bSrc = (bSrc * (255 - aSrc)) >> 8;
				break;
			case TGL_DST_ALPHA:
				rSrc = (rSrc * aDst) >> 8;
				gSrc = (gSrc * aDst) >> 8;
				bSrc = (bSrc * aDst) >> 8;
				break;
			case TGL_ONE_MINUS_DST_ALPHA:
				rSrc = (rSrc * (255 - aDst)) >> 8;
				gSrc = (gSrc * (255 - aDst)) >> 8;
				bSrc = (bSrc * (255 - aDst)) >> 8;
				break;
			default:
				break;
			}

			switch (_destinationBlendingFactor) {
			case TGL_ZERO:
				rDst = gDst = bDst = 0;
				break;
			case TGL_ONE:
				this->pbuf.getRGBAt(pixel, rDst, gDst, bDst);
				break;
			case TGL_DST_COLOR:
				this->pbuf.getRGBAt(pixel, rDst, gDst, bDst);
				rDst = (rDst * rSrc) >> 8;
				gDst = (gDst * gSrc) >> 8;
				bDst = (bDst * bSrc) >> 8;
				break;
			case TGL_ONE_MINUS_DST_COLOR:
				this->pbuf.getRGBAt(pixel, rDst, gDst, bDst);
				rDst = (rDst * (255 - rSrc)) >> 8;
				gDst = (gDst * (255 - gSrc)) >> 8;
				bDst = (bDst * (255 - bSrc)) >> 8;
				break;
			case TGL_SRC_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rDst = (rDst * aSrc) >> 8;
				gDst = (gDst * aSrc) >> 8;
				bDst = (bDst * aSrc) >> 8;
				break;
			case TGL_ONE_MINUS_SRC_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rDst = (rDst * (255 - aSrc)) >> 8;
				gDst = (gDst * (255 - aSrc)) >> 8;
				bDst = (bDst * (255 - aSrc)) >> 8;
				break;
			case TGL_DST_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rDst = (rDst * aDst) >> 8;
				gDst = (gDst * aDst) >> 8;
				bDst = (bDst * aDst) >> 8;
				break;
			case TGL_ONE_MINUS_DST_ALPHA:
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				rDst = (rDst * (255 - aDst)) >> 8;
				gDst = (gDst * (255 - aDst)) >> 8;
				bDst = (bDst * (255 - aDst)) >> 8;
				break;
			case TGL_SRC_ALPHA_SATURATE: {
				this->pbuf.getARGBAt(pixel, aDst, rDst, gDst, bDst);
				int factor = aSrc < 1 - aDst ? aSrc : 1 - aDst;
				rDst = (rDst * factor) >> 8;
				gDst = (gDst * factor) >> 8;
				bDst = (bDst * factor) >> 8;
				}
				break;
			default:
				break;
			}
			this->pbuf.setPixelAt(pixel, 255, rDst + rSrc, gDst + gSrc, bDst + bSrc);
		}
	}

	void copyToBuffer(Graphics::PixelBuffer &buffer) {
		buffer.copyBuffer(0, xsize * ysize, pbuf);
	}

	void copyFromBuffer(Graphics::PixelBuffer &buffer) {
		pbuf.copyBuffer(0, xsize * ysize, buffer);
	}

	void enableBlending(bool enableBlending);
	void setBlendingFactors(int sfactor, int dfactor);

	/**
	* Blit the buffer to the screen buffer, checking the depth of the pixels.
	* Eack pixel is copied if and only if its depth value is bigger than the
	* depth value of the screen pixel, so if it is 'above'.
	*/
	void blitOffscreenBuffer(Buffer *buffer);
	void selectOffscreenBuffer(Buffer *buffer);
	void clearOffscreenBuffer(Buffer *buffer);
	void setTexture(const Graphics::PixelBuffer &texture);

	template <bool interpRGB, bool interpZ, bool interpST, bool interpSTZ, int drawLogic>
	void fillTriangle(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);

	template <bool interpRGB, bool interpZ>
	void fillLine(ZBufferPoint *p1, ZBufferPoint *p2, int color);

	void fillTriangleMappingPerspective(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleDepthOnly(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleFlat(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleFlatShadowMask(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleFlatShadow(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleSmooth(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);
	void fillTriangleMapping(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2);

	void plot(ZBufferPoint *p);
	void line(ZBufferPoint *p1, ZBufferPoint *p2);
	void line_z(ZBufferPoint *p1, ZBufferPoint *p2);
	void line_flat_z(ZBufferPoint *p1, ZBufferPoint *p2, int color);
	void line_interp_z(ZBufferPoint *p1, ZBufferPoint *p2);
	void line_flat(ZBufferPoint *p1, ZBufferPoint *p2, int color);
	void line_interp(ZBufferPoint *p1, ZBufferPoint *p2);

	int xsize, ysize;
	int linesize; // line size, in bytes
	Graphics::PixelFormat cmode;
	int pixelbits;
	int pixelbytes;

	Buffer buffer;

	unsigned int *zbuf;
	unsigned char *shadow_mask_buf;
	int shadow_color_r;
	int shadow_color_g;
	int shadow_color_b;
	int frame_buffer_allocated;

	unsigned char *dctable;
	int *ctable;
	Graphics::PixelBuffer current_texture;
private:
	Graphics::PixelBuffer pbuf;
	bool _blendingEnabled;
	int _sourceBlendingFactor;
	int _destinationBlendingFactor;
};

// memory.c
void gl_free(void *p);
void *gl_malloc(int size);
void *gl_zalloc(int size);

} // end of namespace TinyGL

#endif
