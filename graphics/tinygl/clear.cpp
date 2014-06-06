
#include "graphics/tinygl/zgl.h"

namespace TinyGL {

void glopClearColor(GLContext *c, GLParam *p) {
	c->clear_color.v[0] = p[1].f;
	c->clear_color.v[1] = p[2].f;
	c->clear_color.v[2] = p[3].f;
	c->clear_color.v[3] = p[4].f;
}

void glopClearDepth(GLContext *c, GLParam *p) {
	c->clear_depth = p[1].f;
}

void glopClear(GLContext *c, GLParam *p) {
	int mask = p[1].i;
	int z = 0;
	int r = (int)(c->clear_color.v[0] * 65535);
	int g = (int)(c->clear_color.v[1] * 65535);
	int b = (int)(c->clear_color.v[2] * 65535);

	// TODO : correct value of Z
	c->zb->clear(mask & TGL_DEPTH_BUFFER_BIT, z, mask & TGL_COLOR_BUFFER_BIT, r, g, b);
}

} // end of namespace TinyGL
