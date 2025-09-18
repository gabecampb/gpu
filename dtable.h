#ifndef DTABLE_H
#define DTABLE_H

#include "defs.h"

#define MAX_UBO_SIZE	0x4000

typedef struct node_t {
	void* data;
	void* next;
} node_t;

typedef struct desc_binding_t {
	GLuint location;
	GLuint tmu;
	GLuint ubo_binding;
} desc_binding_t;

typedef struct desc_access_t {
	uint32_t type;

	uint16_t table;
	uint16_t index;

	uint32_t buffer_size;

	uint32_t n_dims;
	uint32_t sample_type;

	desc_binding_t bind_point;
} desc_access_t;

void bind_dtables();

// defined in kernel.c
node_t* get_accesses();
uint32_t get_accessed_dtables();
GLuint get_gl_program();

#endif
