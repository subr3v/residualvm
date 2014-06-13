
#include "graphics/tinygl/zbuffer.h"

namespace TinyGL {

#define ZCMP(z,zpix) ((z) >= (zpix))

void ZBuffer::plot(ZBufferPoint *p) {
	unsigned int *pz;
	PIXEL *pp;

	pz = zbuf + (p->y * xsize + p->x);
	pp = (PIXEL *)((char *) pbuf.getRawBuffer() + linesize * p->y + p->x * PSZB);
	if (ZCMP((unsigned int)p->z, *pz)) {
		*pp = RGB_TO_PIXEL(p->r, p->g, p->b);
		*pz = p->z;
	}
}

#define INTERP_Z
void ZBuffer::line_flat_z(ZBufferPoint *p1, ZBufferPoint *p2, int color) {
#include "graphics/tinygl/zline.h"
}

// line with color interpolation
#define INTERP_Z
#define INTERP_RGB
void ZBuffer::line_interp_z(ZBufferPoint *p1, ZBufferPoint *p2) {
#include "graphics/tinygl/zline.h"
}

// no Z interpolation
void ZBuffer::line_flat(ZBufferPoint *p1, ZBufferPoint *p2, int color) {
#include "graphics/tinygl/zline.h"
}

#define INTERP_RGB
void ZBuffer::line_interp(ZBufferPoint *p1, ZBufferPoint *p2) {
#include "graphics/tinygl/zline.h"
}

void ZBuffer::line_z(ZBufferPoint *p1, ZBufferPoint *p2) {
	int color1, color2;

	color1 = RGB_TO_PIXEL(p1->r, p1->g, p1->b);
	color2 = RGB_TO_PIXEL(p2->r, p2->g, p2->b);

	// choose if the line should have its color interpolated or not
	if (color1 == color2) {
		line_flat_z(p1, p2, color1);
	} else {
		line_interp_z(p1, p2);
	}
}

void ZBuffer::line(ZBufferPoint *p1, ZBufferPoint *p2) {
	int color1, color2;

	color1 = RGB_TO_PIXEL(p1->r, p1->g, p1->b);
	color2 = RGB_TO_PIXEL(p2->r, p2->g, p2->b);

	// choose if the line should have its color interpolated or not
	if (color1 == color2) {
		line_flat(p1, p2, color1);
	} else {
		line_interp(p1, p2);
	}
}

} // end of namespace TinyGL
