
#include "graphics/tinygl/zgl.h"
#include "graphics/tinygl/zblit.h"

namespace TinyGL {

GLContext *gl_ctx;

void initSharedState(GLContext *c) {
	GLSharedState *s = &c->shared_state;
	s->lists = (GLList **)gl_zalloc(sizeof(GLList *) * MAX_DISPLAY_LISTS);
	s->texture_hash_table = (GLTexture **)gl_zalloc(sizeof(GLTexture *) * TEXTURE_HASH_TABLE_SIZE);

	alloc_texture(c, 0);
}

void endSharedState(GLContext *c) {
	GLSharedState *s = &c->shared_state;

	free_texture(c, 0);
	for (int i = 0; i < MAX_DISPLAY_LISTS; i++) {
		// TODO
	}
	gl_free(s->lists);

	gl_free(s->texture_hash_table);
}

void glInit(void *zbuffer1, int textureSize) {
	FrameBuffer *zbuffer = (FrameBuffer *)zbuffer1;
	GLContext *c;
	GLViewport *v;

	if ((textureSize & (textureSize - 1)))
		error("glInit: texture size not power of two: %d", textureSize);

	if (textureSize <= 1 || textureSize > 4096)
		error("glInit: texture size not allowed: %d", textureSize);

	c = new GLContext();
	gl_ctx = c;

	c->fb = zbuffer;

	c->fb->_textureSize = c->_textureSize = textureSize;
	c->fb->_textureSizeMask = (textureSize - 1) << ZB_POINT_ST_FRAC_BITS;

	// allocate GLVertex array
	c->vertex_max = POLYGON_MAX_VERTEX;
	c->vertex = (GLVertex *)gl_malloc(POLYGON_MAX_VERTEX * sizeof(GLVertex));

	// viewport
	v = &c->viewport;
	v->xmin = 0;
	v->ymin = 0;
	v->xsize = zbuffer->xsize;
	v->ysize = zbuffer->ysize;
	v->updated = 1;

	// shared state
	initSharedState(c);

	// lists

	c->exec_flag = 1;
	c->compile_flag = 0;
	c->print_flag = 0;

	c->in_begin = 0;

	// lights
	for (int i = 0; i < T_MAX_LIGHTS; i++) {
		GLLight *l = &c->lights[i];
		l->ambient = Vector4(0, 0, 0, 1);
		l->diffuse = Vector4(1, 1, 1, 1);
		l->specular = Vector4(1, 1, 1, 1);
		l->position = Vector4(0, 0, 1, 0);
		l->norm_position = Vector3(0, 0, 1);
		l->spot_direction = Vector3(0, 0, -1);
		l->norm_spot_direction = Vector3(0, 0, -1);
		l->spot_exponent = 0;
		l->spot_cutoff = 180;
		l->attenuation[0] = 1;
		l->attenuation[1] = 0;
		l->attenuation[2] = 0;
		l->enabled = 0;
	}
	c->first_light = NULL;
	c->ambient_light_model = Vector4(0.2f, 0.2f, 0.2f, 1);
	c->local_light_model = 0;
	c->lighting_enabled = 0;
	c->light_model_two_side = 0;

	// default materials */
	for (int i = 0; i < 2; i++) {
		GLMaterial *m = &c->materials[i];
		m->emission = Vector4(0, 0, 0, 1);
		m->ambient = Vector4(0.2f, 0.2f, 0.2f, 1);
		m->diffuse = Vector4(0.8f, 0.8f, 0.8f, 1);
		m->specular = Vector4(0, 0, 0, 1);
		m->shininess = 0;
	}
	c->current_color_material_mode = TGL_FRONT_AND_BACK;
	c->current_color_material_type = TGL_AMBIENT_AND_DIFFUSE;
	c->color_material_enabled = 0;

	// textures
	glInitTextures(c);

	// default state
	c->current_color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	c->longcurrent_color[0] = 65535;
	c->longcurrent_color[1] = 65535;
	c->longcurrent_color[2] = 65535;
	c->longcurrent_color[3] = 65535;

	c->current_normal = Vector4(1.0f, 0.0f, 0.0f, 0.0f);

	c->current_edge_flag = 1;

	c->current_tex_coord = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

	c->polygon_mode_front = TGL_FILL;
	c->polygon_mode_back = TGL_FILL;

	c->current_front_face = 0; // 0 = GL_CCW  1 = GL_CW
	c->current_cull_face = TGL_BACK;
	c->current_shade_model = TGL_SMOOTH;
	c->cull_face_enabled = 0;

	// clear
	c->clear_color = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	c->clear_depth = 0;

	// selection
	c->render_mode = TGL_RENDER;
	c->select_buffer = NULL;
	c->name_stack_size = 0;

	// blending
	c->fb->enableBlending(false);

	// alpha test
	c->fb->enableAlphaTest(false);

	// matrix
	c->matrix_mode = 0;

	c->matrix_stack_depth_max[0] = MAX_MODELVIEW_STACK_DEPTH;
	c->matrix_stack_depth_max[1] = MAX_PROJECTION_STACK_DEPTH;
	c->matrix_stack_depth_max[2] = MAX_TEXTURE_STACK_DEPTH;

	for (int i = 0; i < 3; i++) {
		c->matrix_stack[i] = (Matrix4 *)gl_zalloc(c->matrix_stack_depth_max[i] * sizeof(Matrix4));
		c->matrix_stack_ptr[i] = c->matrix_stack[i];
	}

	tglMatrixMode(TGL_PROJECTION);
	tglLoadIdentity();
	tglMatrixMode(TGL_TEXTURE);
	tglLoadIdentity();
	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();

	tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);

	tglAlphaFunc(TGL_ALWAYS, 0.f);

	tglDepthFunc(TGL_LESS);

	c->matrix_model_projection_updated = 1;

	// opengl 1.1 arrays
	c->client_states = 0;

	// opengl 1.1 polygon offset
	c->offset_states = 0;

	// shadow mode
	c->shadow_mode = 0;

	// clear the resize callback function pointer
	c->gl_resize_viewport = NULL;

	// specular buffer
	c->specbuf_first = NULL;
	c->specbuf_used_counter = 0;
	c->specbuf_num_buffers = 0;

	// depth test
	c->depth_test = 0;

	c->color_mask = (1 << 24) | (1 << 16) | (1 << 8) | (1 << 0);

	const int kDrawCallMemory = 5 * 1024 * 1024;

	c->_currentAllocatorIndex = 0;
	c->_drawCallAllocator[0].initialize(kDrawCallMemory);
	c->_drawCallAllocator[1].initialize(kDrawCallMemory);

	Graphics::Internal::tglBlitScissorRect(0, 0, c->fb->xsize, c->fb->ysize);
}

void glClose() {
	GLContext *c = gl_get_context();

	specbuf_cleanup(c);
	for (int i = 0; i < 3; i++)
		gl_free(c->matrix_stack[i]);
	endSharedState(c);
	gl_free(c->vertex);

	gl_free(c);
}

} // end of namespace TinyGL
