#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"

#define BO_BUCKET_SIZE 4096
#define LENGTH_IN_BUFFER -1
#define ANY_LENGTH -2

#define NUM_TYPES		4
#define TYPE_CBO		1
#define TYPE_VBO		2
#define TYPE_IBO		3
#define TYPE_TBO		4
#define IS_VALID_TYPE(x) (x != 0 && x <= NUM_INTERNAL_TYPES)

// internal copy of object header info
typedef struct header_t {
	uint32_t n_cmd_bytes;

	uint16_t tex_info;
	uint8_t has_mipmaps;
	uint8_t tex_format;
	uint8_t n_dims;
	uint32_t dims[3];
} header_t;

// internal representation of an object
typedef struct object_t {
	uint64_t addr;
	uint64_t len;
	uint8_t type;
	header_t header;
	uint32_t header_len;

	uint8_t in_overlaps;
	uint8_t need_update;
	int64_t refcount;

	GLuint gl_buffer;
	GLuint gl_vao;
	uint32_t* gl_va_cfgs;
} object_t;

typedef struct bucket_t {
	uint32_t count;
	object_t** objs;
} bucket_t;

uint8_t check_overlap(uint64_t x1, uint64_t x2, uint64_t y1, uint64_t y2);
uint32_t get_region_object_count(uint64_t addr, uint64_t len);
object_t* get_region_object_list(uint32_t match_idx, uint64_t addr, uint64_t len);
uint32_t get_header_length(uint8_t type);
void object_read(object_t* obj, uint8_t* dst, uint64_t src, uint64_t n);
void object_write(object_t* obj, uint64_t dst, uint8_t* src, uint64_t n);
object_t* ref_buffer_precise(uint64_t addr, uint8_t type, int64_t len);
object_t* get_object_precise(uint64_t addr, uint8_t type, int64_t len);
void flush_object(object_t* obj);
void destroy_all_overlaps();

#endif
