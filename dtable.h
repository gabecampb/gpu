#ifndef DTABLE_H
#define DTABLE_H

#include "defs.h"

typedef struct desc_binding_t {
	GLuint location;
	GLuint tmu;
} desc_binding_t;

typedef struct desc_access_t {
	uint32_t type;

	uint16_t table;
	uint16_t index;

	desc_binding_t bind_point;

	void* next;
} desc_access_t;

void bind_dtables();

// defined in shader.c
desc_access_t* get_accesses();
uint32_t get_accessed_dtables();

#endif
