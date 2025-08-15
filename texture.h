#ifndef TEXTURE_H
#define TEXTURE_H

#include "defs.h"

#define MAX_1D_TEXTURE_DIM	8192
#define MAX_2D_TEXTURE_DIM	8192
#define MAX_3D_TEXTURE_DIM	2048

// 1 bpp
#define FORMAT_R_8			0
#define FORMAT_R_U8			1
#define FORMAT_R_I8			2
// 2 bpp
#define FORMAT_RG_8			3
#define FORMAT_RG_U8		4
#define FORMAT_RG_I8		5
#define FORMAT_DEPTH_16		6
// 4 bpp
#define FORMAT_RGBA_8		7
#define FORMAT_RGBA_U8		8
#define FORMAT_RGBA_I8		9
#define FORMAT_R_32F		10
#define FORMAT_DEPTH_32F	11
#define FORMAT_DEPTH_24_STENCIL_8	12
// 8 bpp
#define FORMAT_RG_32F		13
// 16 bpp
#define FORMAT_RGBA_32F		14

#define IS_VALID_FORMAT(x) (x <= 14)

#define IS_COLOR_FORMAT(x) (!(IS_DEPTH_FORMAT(x) || IS_DEPTH_STENCIL_FORMAT(x)))
#define IS_DEPTH_FORMAT(x) (x == FORMAT_DEPTH_16 || x == FORMAT_DEPTH_32F)
#define IS_DEPTH_STENCIL_FORMAT(x) (x == FORMAT_DEPTH_24_STENCIL_8)

typedef struct tex_fmt {
	uint32_t format;
	uint32_t bpp;
	GLenum gl_internal_format;
	GLenum gl_format;
	GLenum gl_type;
} tex_fmt;

static tex_fmt tex_fmt_info[] = {
	// format						bpp			gl_internal_format		gl_format				gl_type
	{ FORMAT_R_8, 					1,			GL_R8,					GL_RED,					GL_UNSIGNED_BYTE },
	{ FORMAT_R_U8, 					1,			GL_R8UI,				GL_RED_INTEGER,			GL_UNSIGNED_BYTE },
	{ FORMAT_R_I8, 					1,			GL_R8I,					GL_RED_INTEGER,			GL_BYTE },

	{ FORMAT_RG_8,					2,			GL_RG8,					GL_RG,					GL_UNSIGNED_BYTE },
	{ FORMAT_RG_U8,					2,			GL_RG8UI,				GL_RG_INTEGER,			GL_UNSIGNED_BYTE },
	{ FORMAT_RG_I8,					2,			GL_RG8I,				GL_RG_INTEGER,			GL_BYTE },
	{ FORMAT_DEPTH_16,				2,			GL_DEPTH_COMPONENT16,	GL_DEPTH_COMPONENT,		GL_UNSIGNED_SHORT },

	{ FORMAT_RGBA_8,				4,			GL_RGBA8,				GL_RGBA,				GL_UNSIGNED_BYTE },
	{ FORMAT_RGBA_U8,				4,			GL_RGBA8UI,				GL_RGBA_INTEGER,		GL_UNSIGNED_BYTE },
	{ FORMAT_RGBA_I8,				4,			GL_RGBA8I,				GL_RGBA_INTEGER,		GL_BYTE },
	{ FORMAT_R_32F,					4,			GL_R32F,				GL_RED,					GL_FLOAT },
	{ FORMAT_DEPTH_32F,				4,			GL_DEPTH_COMPONENT32F,	GL_DEPTH_COMPONENT,		GL_FLOAT },
	{ FORMAT_DEPTH_24_STENCIL_8,	4,			GL_DEPTH24_STENCIL8,	GL_DEPTH_STENCIL,		GL_UNSIGNED_INT_24_8 },

	{ FORMAT_RG_32F,				8,			GL_RG32F,				GL_RG,					GL_FLOAT },

	{ FORMAT_RGBA_32F,				16,			GL_RGBA32F,				GL_RGBA,				GL_FLOAT }
};

#define GET_FORMAT_BPP(x)					tex_fmt_info[x].bpp
#define GET_FORMAT_GL_INTERNAL_FORMAT(x)	tex_fmt_info[x].gl_internal_format
#define GET_FORMAT_GL_FORMAT(x)				tex_fmt_info[x].gl_format
#define GET_FORMAT_GL_TYPE(x)				tex_fmt_info[x].gl_type

GLenum get_tex_gl_target(uint8_t n_dims);
uint64_t get_tex_data_size(header_t* hdr);
void upload_texture(object_t* obj, uint8_t* data);
void read_texture(object_t* obj, uint8_t* dst, uint64_t src, uint64_t n);
void write_texture(object_t* obj, uint64_t dst, uint8_t* src, uint64_t n);

#endif
