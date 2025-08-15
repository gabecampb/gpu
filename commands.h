#ifndef COMMANDS_H
#define COMMANDS_H

#define MAX_VA_COUNT			16
#define MAX_VA_STRIDE			2048
#define MAX_COLOR_ATTACH_COUNT	8

#define CMD_SET_REG_32		1
#define CMD_SET_REG_64		2
#define CMD_DRAW			3
#define CMD_CLEAR_ATTACHS	4

#define NUM_BYTES_CMD_REGS	1024		/* TODO: this is a placeholder value */
#define FB_CFG_REG			0x0
#define COLOR_ATTACH_0_REG	0x4			/* MAX_COLOR_ATTACH_COUNT */
#define DEPTH_ATTACH_REG	0x44
#define VIEW_ORIGIN_X_REG	0x4C
#define VIEW_ORIGIN_Y_REG	0x50
#define VIEW_SIZE_X_REG		0x54
#define VIEW_SIZE_Y_REG		0x58
#define VA0_CFG_REG			0x5C		/* MAX_VA_COUNT */
#define BASE_IDX_REG		0x9C
#define IDX_COUNT_REG		0xA4
#define VBO_ADDR_REG		0xAC
#define VBO_LEN_REG			0xB4

#define ENABLE_DEPTH_ATTACH_BIT	(1 << 31)
#define ENABLE_VA_BIT		(1 << 31)

#define CLEAR_DEPTH_ATTACH_BIT (1 << 31)
#define CLEAR_STENCIL_ATTACH_BIT (1 << 30)

#define VA_TYPE_I8			0
#define VA_TYPE_I16			1
#define VA_TYPE_I32			2
#define VA_TYPE_U8			3
#define VA_TYPE_U16			4
#define VA_TYPE_U32			5
#define VA_TYPE_F32			6

#define IS_VALID_VA_TYPE(x)	(x <= 6)

typedef struct va_type {
	uint32_t format;
	uint32_t width;
	GLenum gl_type;
} va_type;

static va_type va_type_info[] = {
	// format		width	gl_type
	{ VA_TYPE_I8,	1,		GL_BYTE },
	{ VA_TYPE_I16,	2,		GL_SHORT },
	{ VA_TYPE_I32,	4,		GL_INT },
	{ VA_TYPE_U8,	1,		GL_UNSIGNED_BYTE },
	{ VA_TYPE_U16,	2,		GL_UNSIGNED_SHORT },
	{ VA_TYPE_U32,	4,		GL_UNSIGNED_INT },
	{ VA_TYPE_F32,	4,		GL_FLOAT }
};

#define GET_VA_GL_TYPE(x)			va_type_info[x].gl_type
#define GET_VA_COMPONENT_WIDTH(x)	va_type_info[x].width

void command_decoder(uint8_t* commands, uint64_t len);

#endif
