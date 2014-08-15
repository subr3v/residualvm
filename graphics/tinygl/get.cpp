#define FORBIDDEN_SYMBOL_EXCEPTION_fprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr

#include "graphics/tinygl/zgl.h"

void tglGetIntegerv(int pname, int *params) {
	TinyGL::GLContext *c = TinyGL::gl_get_context();

	switch (pname) {
	case TGL_VIEWPORT:
		params[0] = c->viewport.xmin;
		params[1] = c->viewport.ymin;
		params[2] = c->viewport.xsize;
		params[3] = c->viewport.ysize;
		break;
	case TGL_MAX_MODELVIEW_STACK_DEPTH:
		*params = MAX_MODELVIEW_STACK_DEPTH;
		break;
	case TGL_MAX_PROJECTION_STACK_DEPTH:
		*params = MAX_PROJECTION_STACK_DEPTH;
		break;
	case TGL_MAX_LIGHTS:
		*params = T_MAX_LIGHTS;
		break;
	case TGL_MAX_TEXTURE_SIZE:
		*params = c->_textureSize;
		break;
	case TGL_MAX_TEXTURE_STACK_DEPTH:
		*params = MAX_TEXTURE_STACK_DEPTH;
		break;
	case TGL_BLEND:
		*params = c->fb->isBlendingEnabled();
		break;
	case TGL_ALPHA_TEST:
		*params = c->fb->isAplhaTestEnabled();
		break;
	default:
		error("tglGet: option not implemented");
		break;
	}
}

void tglGetFloatv(int pname, float *v) {
	int i;
	int mnr = 0; // just a trick to return the correct matrix
	TinyGL::GLContext *c = TinyGL::gl_get_context();
	switch (pname) {
	case TGL_TEXTURE_MATRIX:
		mnr++;
	case TGL_PROJECTION_MATRIX:
		mnr++;
	case TGL_MODELVIEW_MATRIX: {
		float *p = &c->matrix_stack_ptr[mnr]->_m[0][0];
		for (i = 0; i < 4; i++) {
			*v++ = p[0];
			*v++ = p[4];
			*v++ = p[8];
			*v++ = p[12];
			p++;
		}
	}
	break;
	case TGL_LINE_WIDTH:
		*v = 1.0f;
		break;
	case TGL_LINE_WIDTH_RANGE:
		v[0] = v[1] = 1.0f;
		break;
	case TGL_POINT_SIZE:
		*v = 1.0f;
		break;
	case TGL_POINT_SIZE_RANGE:
		v[0] = v[1] = 1.0f;
		break;
	default:
		fprintf(stderr, "warning: unknown pname in glGetFloatv()\n");
		break;
	}
}
