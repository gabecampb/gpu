#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"

#define BO_BUCKET_SIZE 4096
#define LENGTH_IN_BUFFER -1
#define ANY_LENGTH -2

#define NUM_TYPES		3
#define TYPE_CBO		1
#define TYPE_VBO		2
#define TYPE_IBO		3
#define IS_VALID_TYPE(x) (x != 0 && x <= NUM_INTERNAL_TYPES)

// internal copy of object header info
typedef struct header_t {
	uint32_t n_cmd_bytes;
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
} object_t;

typedef struct bucket_t {
	uint32_t count;
	object_t** objs;
} bucket_t;

uint32_t get_region_object_count(uint64_t addr, uint64_t len);
object_t* get_region_object_list(uint32_t match_idx, uint64_t addr, uint64_t len);
void object_read(object_t* obj, uint8_t* dst, uint64_t src, uint64_t n);
void object_write(object_t* obj, uint64_t dst, uint8_t* src, uint64_t n);
object_t* ref_buffer_precise(uint64_t addr, uint8_t type, int64_t len);
object_t* get_object_precise(uint64_t addr, uint8_t type, int64_t len);
void destroy_all_overlaps();

#endif
