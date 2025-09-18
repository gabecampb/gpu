#ifndef KERNEL_H
#define KERNEL_H

#include "defs.h"

#define ATTR_IN  0
#define ATTR_OUT 1

#define MAX_UBO_COUNT	32
#define MAX_TBO_COUNT	16

#define N_OPS	4
#define OP_MOV	0
#define OP_LD	1
#define OP_STR	2
#define OP_TEX	3

typedef struct field_t {
	uint32_t bit_start;
	uint32_t bit_count;
} field_t;

typedef struct ins_t {
	uint16_t op;
	uint32_t field_count;
	field_t fields[16];
} ins_t;

static ins_t ins_list[] = {
	{ OP_MOV,	3, {{0,7}, {7,8}, {15,32}} },
	{ OP_LD,	5, {{0,7}, {7,8}, {15,1}, {16,2}, {18,64}} },
	{ OP_STR,	5, {{0,7}, {7,1}, {8,2}, {10,64}, {74,8}} },
	{ OP_TEX,	4, {{0,7}, {7,32}, {39,36}, {75,24}} }
};

typedef struct attrib_access_t {
	uint8_t attr_type;		// ATTR_IN, ATTR_OUT
	uint32_t stage_id;

	uint8_t interp_type;	// 0 = flat, 1 = smooth, 2 = nopersp
	uint8_t comp_type;		// 0=f, 1=i, 2=u
	uint8_t comp_count;
	uint16_t id;
} attrib_access_t;

typedef struct kernel_info_t {
	GLuint gl_program;
	uint32_t table_accesses;
	node_t* desc_accesses;

	// below are only used during build_kernel()
	uint32_t kernel_len;
	uint32_t local_mem_size;
	uint32_t n_tmus_occupied;
	uint32_t n_sampling_calls;
	node_t* attrib_accesses;
} kernel_info_t;

typedef struct code_t {
	char* str;
	uint32_t len;
} code_t;

typedef struct stage_t {
	uint32_t id;
	uint32_t len;
	code_t globals;
	code_t code;
} stage_t;

void bind_kernel();
void free_kernel(object_t* obj);

#endif
