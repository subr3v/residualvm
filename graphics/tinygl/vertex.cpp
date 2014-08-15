
#include "graphics/tinygl/zgl.h"
#include "graphics/tinygl/zrect.h"

namespace TinyGL {

void glopNormal(GLContext *c, GLParam *p) {
	c->current_normal.X = p[1].f;
	c->current_normal.Y = p[2].f;
	c->current_normal.Z = p[3].f;
	c->current_normal.W = 0.0f;
}

void glopTexCoord(GLContext *c, GLParam *p) {
	c->current_tex_coord.X = p[1].f;
	c->current_tex_coord.Y = p[2].f;
	c->current_tex_coord.Z = p[3].f;
	c->current_tex_coord.W = p[4].f;
}

void glopEdgeFlag(GLContext *c, GLParam *p) {
	c->current_edge_flag = p[1].i;
}

void glopColor(GLContext *c, GLParam *p) {
	c->current_color.X = p[1].f;
	c->current_color.Y = p[2].f;
	c->current_color.Z = p[3].f;
	c->current_color.W = p[4].f;
	c->longcurrent_color[0] = p[5].ui;
	c->longcurrent_color[1] = p[6].ui;
	c->longcurrent_color[2] = p[7].ui;
	c->longcurrent_color[3] = p[8].ui;

	if (c->color_material_enabled) {
		GLParam q[7];
		q[0].op = OP_Material;
		q[1].i = c->current_color_material_mode;
		q[2].i = c->current_color_material_type;
		q[3].f = p[1].f;
		q[4].f = p[2].f;
		q[5].f = p[3].f;
		q[6].f = p[4].f;
		glopMaterial(c, q);
	}
}

void gl_eval_viewport(GLContext *c) {
	GLViewport *v;
	float zsize = (1 << (ZB_Z_BITS + ZB_POINT_Z_FRAC_BITS));

	v = &c->viewport;

	v->trans.X = (float)(((v->xsize - 0.5) / 2.0) + v->xmin);
	v->trans.Y = (float)(((v->ysize - 0.5) / 2.0) + v->ymin);
	v->trans.Z = (float)(((zsize - 0.5) / 2.0) + ((1 << ZB_POINT_Z_FRAC_BITS)) / 2);

	v->scale.X = (float)((v->xsize - 0.5) / 2.0);
	v->scale.Y = (float)(-(v->ysize - 0.5) / 2.0);
	v->scale.Z = (float)(-((zsize - 0.5) / 2.0));
}

void glopBegin(GLContext *c, GLParam *p) {
	int type;

	assert(c->in_begin == 0);

	type = p[1].i;
	c->begin_type = type;
	c->in_begin = 1;
	c->vertex_n = 0;
	c->vertex_cnt = 0;

	if (c->matrix_model_projection_updated) {
		if (c->lighting_enabled) {
			// precompute inverse modelview
			c->matrix_model_view_inv = *c->matrix_stack_ptr[0];
			c->matrix_model_view_inv.invert();
			c->matrix_model_view_inv.transpose();
		} else {
			// precompute projection matrix
			c->matrix_model_projection = (*c->matrix_stack_ptr[1]) * (*c->matrix_stack_ptr[0]);
			// test to accelerate computation
			c->matrix_model_projection_no_w_transform = 0;
			if (c->matrix_model_projection._m[3][0] == 0.0 && c->matrix_model_projection._m[3][1] == 0.0 && c->matrix_model_projection._m[3][2] == 0.0)
				c->matrix_model_projection_no_w_transform = 1;
		}

		// test if the texture matrix is not Identity
		c->apply_texture_matrix = !c->matrix_stack_ptr[2]->isIdentity();

		c->matrix_model_projection_updated = 0;
	}
	// viewport
	if (c->viewport.updated) {
		gl_eval_viewport(c);
		c->viewport.updated = 0;
	}
	// triangle drawing functions
	if (c->render_mode == TGL_SELECT) {
		c->draw_triangle_front = gl_draw_triangle_select;
		c->draw_triangle_back = gl_draw_triangle_select;
	} else {
		switch (c->polygon_mode_front) {
		case TGL_POINT:
			c->draw_triangle_front = gl_draw_triangle_point;
			break;
		case TGL_LINE:
			c->draw_triangle_front = gl_draw_triangle_line;
			break;
		default:
			c->draw_triangle_front = gl_draw_triangle_fill;
			break;
		}

		switch (c->polygon_mode_back) {
		case TGL_POINT:
			c->draw_triangle_back = gl_draw_triangle_point;
			break;
		case TGL_LINE:
			c->draw_triangle_back = gl_draw_triangle_line;
			break;
		default:
			c->draw_triangle_back = gl_draw_triangle_fill;
			break;
		}
	}
}

// coords, tranformation, clip code and projection
// TODO : handle all cases
static inline void gl_vertex_transform(GLContext *c, GLVertex *v) {
	Matrix4 *m;

	if (c->lighting_enabled) {
		// eye coordinates needed for lighting

		m = c->matrix_stack_ptr[0];
		m->transform3x4(v->coord,v->ec);

		// projection coordinates
		m = c->matrix_stack_ptr[1];
		m->transform(v->ec, v->pc);

		m = &c->matrix_model_view_inv;

		m->transform3x3(c->current_normal, v->normal);

		if (c->normalize_enabled) {
			v->normal.normalize();
		}
	} else {
		// no eye coordinates needed, no normal
		// NOTE: W = 1 is assumed
		m = &c->matrix_model_projection;

		m->transform3x4(v->coord, v->pc);
		if (c->matrix_model_projection_no_w_transform) {
			v->pc.W = (m->_m[3][3]);
		}
	}

	v->clip_code = gl_clipcode(v->pc.X, v->pc.Y, v->pc.Z, v->pc.W);
}

void glopVertex(GLContext *c, GLParam *p) {
	GLVertex *v;
	int n, cnt;

	assert(c->in_begin != 0);

	n = c->vertex_n;
	cnt = c->vertex_cnt;
	cnt++;
	c->vertex_cnt = cnt;

	// quick fix to avoid crashes on large polygons
	if (n >= c->vertex_max) {
		GLVertex *newarray;
		c->vertex_max <<= 1;    // just double size
		newarray = (GLVertex *)gl_malloc(sizeof(GLVertex) * c->vertex_max);
		if (!newarray) {
			error("unable to allocate GLVertex array.");
		}
		memcpy(newarray, c->vertex, n * sizeof(GLVertex));
		gl_free(c->vertex);
		c->vertex = newarray;
	}
	// new vertex entry
	v = &c->vertex[n];
	n++;

	v->coord.X = p[1].f;
	v->coord.Y = p[2].f;
	v->coord.Z = p[3].f;
	v->coord.W = p[4].f;

	gl_vertex_transform(c, v);

	// color

	if (c->lighting_enabled) {
		gl_shade_vertex(c, v);
	} else {
		v->color = c->current_color;
	}

	// tex coords

	if (c->texture_2d_enabled) {
		if (c->apply_texture_matrix) {
			c->matrix_stack_ptr[2]->transform(c->current_tex_coord, v->tex_coord);
		} else {
			v->tex_coord = c->current_tex_coord;
		}
	}
	// precompute the mapping to the viewport
	if (v->clip_code == 0)
		gl_transform_to_viewport(c, v);

	// edge flag

	v->edge_flag = c->current_edge_flag;
	switch (c->begin_type) {
	case TGL_POINTS:
		glIssueDrawCall(new Graphics::RasterizationDrawCall());
		n = 0;
		break;
	case TGL_LINES:
		if (n == 2) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			n = 0;
		}
		break;
	case TGL_LINE_STRIP:
	case TGL_LINE_LOOP:
		if (n == 1) {
			c->vertex[2] = c->vertex[0];
		} else if (n == 2) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			c->vertex[0] = c->vertex[1];
			n = 1;
		}
		break;
	case TGL_TRIANGLES:
		if (n == 3) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			n = 0;
		}
		break;
	case TGL_TRIANGLE_STRIP:
		if (cnt >= 3) {
			if (n == 3)
				n = 0;
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
		}
		break;
	case TGL_TRIANGLE_FAN:
		if (n == 3) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			c->vertex[1] = c->vertex[2];
			n = 2;
		}
		break;
	case TGL_QUADS:
		if (n == 4) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			n = 0;
		}
		break;
	case TGL_QUAD_STRIP:
		if (n == 4) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
			for (int i = 0; i < 2; i++)
				c->vertex[i] = c->vertex[i + 2];
			n = 2;
		}
		break;
	case TGL_POLYGON:
		break;
	default:
		error("glBegin: type %x not handled", c->begin_type);
	}


	c->vertex_n = n;
}

void glopEnd(GLContext *c, GLParam *) {
	assert(c->in_begin == 1);

	if (c->begin_type == TGL_LINE_LOOP) {
		if (c->vertex_cnt >= 3) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
		}
	} else if (c->begin_type == TGL_POLYGON) {
		int i = c->vertex_cnt;
		if (i >= 3) {
			glIssueDrawCall(new Graphics::RasterizationDrawCall());
		}
	}

	c->in_begin = 0;
}

} // end of namespace TinyGL
