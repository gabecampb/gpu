#include "defs.h"

bucket_t bo_bucket[VRAM_CAPACITY / BO_BUCKET_SIZE];

object_t** obj_overlaps_list;
uint32_t obj_overlaps_count;

int64_t ref_counter = 0;

uint8_t check_overlap(uint64_t x1, uint64_t x2, uint64_t y1, uint64_t y2) {
	return x2 >= y1 && y2 >= x1;
}

// get number of objects whose buffers overlap with the region
uint32_t get_region_object_count(uint64_t addr, uint64_t len) {
	object_t** seen_list = 0;
	uint32_t count = 0;

	uint32_t start_bucket = addr / BO_BUCKET_SIZE;
	uint32_t end_bucket = (addr + len - 1) / BO_BUCKET_SIZE;
	uint32_t n_buckets = end_bucket - start_bucket + 1;
	for(uint32_t b = addr / BO_BUCKET_SIZE; n_buckets--; b++) {
		bucket_t* bucket = &bo_bucket[b];
		for(uint32_t i = 0; i < bucket->count; i++) {
			object_t* obj = bucket->objs[i];

			uint8_t seen = 0;
			for(uint32_t j = 0; j < count && !seen; j++)
				if(seen_list[j] == obj)
					seen = 1;

			uint64_t x1 = obj->addr, x2 = obj->addr + obj->len - 1;
			uint64_t y1 = addr, y2 = addr + len - 1;

			if(!seen && check_overlap(x1, x2, y1, y2)) {
				seen_list = realloc(seen_list, sizeof(object_t*) * (count + 1));
				seen_list[count] = obj;
				count++;
			}
		}
	}

	if(seen_list)
		free(seen_list);
	return count;
}

// get objects whose buffers overlap with the region
object_t* get_region_object_list(uint32_t match_idx, uint64_t addr, uint64_t len) {
	object_t** seen_list = 0;
	uint32_t count = 0;
	uint32_t obj_idx = 0;

	uint32_t start_bucket = addr / BO_BUCKET_SIZE;
	uint32_t end_bucket = (addr + len - 1) / BO_BUCKET_SIZE;
	uint32_t n_buckets = end_bucket - start_bucket + 1;
	for(uint32_t b = addr / BO_BUCKET_SIZE; n_buckets--; b++) {
		bucket_t* bucket = &bo_bucket[b];
		for(uint32_t i = 0; i < bucket->count; i++) {
			object_t* obj = bucket->objs[i];

			uint8_t seen = 0;
			for(uint32_t j = 0; j < count && !seen; j++)
				if(seen_list[j] == obj)
					seen = 1;

			uint64_t x1 = obj->addr, x2 = obj->addr + obj->len - 1;
			uint64_t y1 = addr, y2 = addr + len - 1;

			if(!seen && check_overlap(x1, x2, y1, y2)) {
				seen_list = realloc(seen_list, sizeof(object_t*) * (count + 1));
				seen_list[count] = obj;
				if(obj_idx == match_idx) {
					if(seen_list)
						free(seen_list);
					return bucket->objs[i];
				}
				count++;
				obj_idx++;
			}
		}
	}

	if(seen_list)
		free(seen_list);
	return 0;
}

void add_to_bucket(object_t* obj) {
	uint32_t start_bucket = obj->addr / BO_BUCKET_SIZE;
	uint32_t end_bucket = (obj->addr + obj->len - 1) / BO_BUCKET_SIZE;
	uint32_t n_buckets = end_bucket - start_bucket + 1;
	for(uint32_t b = obj->addr / BO_BUCKET_SIZE; n_buckets--; b++) {
		bucket_t* bucket = &bo_bucket[b];

		bucket->objs = realloc(bucket->objs,
			sizeof(object_t*) * (bucket->count + 1));
		bucket->objs[bucket->count] = obj;
		bucket->count++;
	}
}

void remove_from_bucket(object_t* obj) {
	uint32_t start_bucket = obj->addr / BO_BUCKET_SIZE;
	uint32_t end_bucket = (obj->addr + obj->len - 1) / BO_BUCKET_SIZE;
	uint32_t n_buckets = end_bucket - start_bucket + 1;
	for(uint32_t b = obj->addr / BO_BUCKET_SIZE; n_buckets--; b++) {
		bucket_t* bucket = &bo_bucket[b];

		if(bucket->count == 1) {
			free(bucket->objs);
			bucket->objs = (void*)0;
			bucket->count = 0;
			continue;
		}

		for(uint32_t i = 0; i < bucket->count; i++)
			if(bucket->objs[i] == obj) {
				if(i < bucket->count - 1)
					bucket->objs[i] = bucket->objs[bucket->count - 1];

				bucket->objs = realloc(bucket->objs,
					sizeof(object_t*) * (bucket->count - 1));
				bucket->count--;
				break;
			}
	}
}

void add_to_overlaps(object_t* obj) {
	object_t** list = obj_overlaps_list;
	list = realloc(list, sizeof(object_t*) * (obj_overlaps_count + 1));
	list[obj_overlaps_count] = obj;
	obj_overlaps_list = list;
	obj_overlaps_count++;
}

void remove_from_overlaps(object_t* obj) {
	if(!obj->in_overlaps)
		return;

	if(obj_overlaps_count == 1) {
		free(obj_overlaps_list);
		obj_overlaps_list = (void*)0;
		obj_overlaps_count = 0;
		obj->in_overlaps = 0;
		return;
	}

	for(uint32_t i = 0; i < obj_overlaps_count; i++)
		if(obj_overlaps_list[i] == obj) {
			object_t** list = obj_overlaps_list;

			if(i < obj_overlaps_count - 1)
				list[i] = list[obj_overlaps_count - 1];

			list = realloc(list, sizeof(object_t*) * (obj_overlaps_count - 1));

			obj_overlaps_list = list;
			obj_overlaps_count--;
			obj->in_overlaps = 0;
			return;
		}
}

void mark_all_overlaps(uint64_t addr, uint64_t len) {
	uint32_t count = get_region_object_count(addr, len);
	if(count == 1)
		return;		// no overlaps

	for(uint32_t i = 0; i < count; i++) {
		object_t* obj = get_region_object_list(i, addr, len);
		if(obj->in_overlaps)
			continue;

		add_to_overlaps(obj);

		obj->in_overlaps = 1;
	}
}

uint32_t get_header_length(uint8_t type) {
	switch(type) {
		case TYPE_CBO:	return 4;	break;
		case TYPE_TBO:	return 14;	break;
		default:		return 0;
	}
}

uint64_t get_header_info(header_t* header, uint64_t addr, uint8_t type) {
	uint32_t header_len = get_header_length(type);
	if(!header_len)
		ERROR("bad type specified to get_header_info\n");

	uint8_t* data = malloc(header_len);
	data = gpu_read(data, addr, header_len);

	if(!data)
		ERROR("something went wrong reading header of object %llx\n", addr);

	uint64_t len = 0;
	switch(type) {
		case TYPE_CBO:
			header->n_cmd_bytes = *(uint32_t*)data;
			if(header->n_cmd_bytes)
				len = header_len + header->n_cmd_bytes;
			break;
		case TYPE_TBO:
			header->tex_info = *(uint16_t*)data;
			header->has_mipmaps = header->tex_info >> 15;
			header->n_dims = (header->tex_info >> 13) & 0x3;
			if(header->n_dims == 0 || header->n_dims > 3)
				break;
			memcpy(header->dims, data + 2, 12);
			for(uint32_t i = 0; i < header->n_dims; i++) {
				if(header->dims[i] == 0
				|| (header->n_dims == 1 && header->dims[i] > MAX_1D_TEXTURE_DIM)
				|| (header->n_dims == 2 && header->dims[i] > MAX_2D_TEXTURE_DIM)
				|| (header->n_dims == 3 && header->dims[i] > MAX_3D_TEXTURE_DIM))
					break;
			}
			header->tex_format = header->tex_info & 0xFF;
			if(!IS_VALID_FORMAT(header->tex_format))
				break;
			if((header->has_mipmaps || header->n_dims != 2)
			&& (header->tex_format == FORMAT_DEPTH_16
				|| header->tex_format == FORMAT_DEPTH_32F
				|| header->tex_format == FORMAT_DEPTH_24_STENCIL_8))
				break;
			len = header_len + get_tex_data_size(header);
			break;
	}

	free(data);
	return len;
}

object_t* get_object_precise(uint64_t addr, uint8_t type, int64_t len) {
	if(len <= 0 && len != ANY_LENGTH) {
		WARN("bad length %lld passed to get_object_precise, must be a "
			"positive value or ANY_LENGTH\n", len);
		return 0;
	}

	if(len > 0 && addr + len >= VRAM_CAPACITY)
		return 0;

	bucket_t* bucket = &bo_bucket[addr / BO_BUCKET_SIZE];
	for(uint32_t i = 0; i < bucket->count; i++) {
		object_t* obj = bucket->objs[i];

		if(obj->addr == addr && obj->type == type
		&& (len == ANY_LENGTH ? 1 : obj->len == len))
			return obj;
	}
	return 0;
}

object_t* create_object(header_t* header, uint64_t addr, uint8_t type, uint64_t len) {
	uint8_t* data = malloc(len);
	data = gpu_read(data, addr, len);

	if(!data) {
		WARN("error occurred during read for create_object %llx, length %llx\n",
			addr, len);
		return 0;
	}

	object_t* obj = calloc(1, sizeof(object_t));
	obj->addr = addr;
	obj->len = len;
	obj->type = type;
	obj->header_len = get_header_length(type);
	obj->header = *header;

	add_to_bucket(obj);

	mark_all_overlaps(addr, len);

	if(type == TYPE_VBO) {
		glGenBuffers(1, &obj->gl_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, obj->gl_buffer);
		glBufferData(GL_ARRAY_BUFFER, obj->len, data, GL_STATIC_DRAW);
	}
	if(type == TYPE_IBO) {
		glGenBuffers(1, &obj->gl_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->gl_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj->len, data, GL_STATIC_DRAW);
	}
	if(type == TYPE_TBO) {
		glGenBuffers(1, &obj->gl_buffer);
		glBindTexture(get_tex_gl_target(obj->header.n_dims), obj->gl_buffer);
		upload_texture(obj, data);
	}

	free(data);
	return obj;
}

void object_read(object_t* obj, uint8_t* dst, uint64_t src, uint64_t n) {
	if(src < obj->addr || src + n > obj->addr + obj->len)
		ERROR("object read [%d,%d] out of bounds\n", src, src + n - 1);

	if(src < obj->addr + get_header_length(obj->type)) {
		uint32_t count = obj->addr + get_header_length(obj->type) - src;
		count = count > n ? n : count;
		memmove(dst, vram + src, count);
		dst += count;
		src += count;
		n -= count;
		if(!n)
			return;
	}

	if(obj->type == TYPE_VBO) {
		glBindBuffer(GL_ARRAY_BUFFER, obj->gl_buffer);
		glGetBufferSubData(GL_ARRAY_BUFFER, src - obj->addr, n, dst);
	} else if(obj->type == TYPE_IBO) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->gl_buffer);
		glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, src - obj->addr, n, dst);
	} else if(obj->type == TYPE_TBO)
		read_texture(obj, dst, src, n);
	else
		memmove(dst, vram + src, n);
}

void object_write(object_t* obj, uint64_t dst, uint8_t* src, uint64_t n) {
	if(dst < obj->addr || dst + n > obj->addr + obj->len)
		ERROR("object write [%d,%d] out of bounds\n", dst, dst + n - 1);

	if(dst < obj->addr + get_header_length(obj->type)) {
		obj->need_update = 1;	// mark object for recreation if header changed

		uint32_t count = obj->addr + get_header_length(obj->type) - dst;
		count = count > n ? n : count;
		dst += count;
		src += count;
		n -= count;
		if(!n)
			return;
	}

	if(obj->type == TYPE_VBO) {
		glBindBuffer(GL_ARRAY_BUFFER, obj->gl_buffer);
		glBufferSubData(GL_ARRAY_BUFFER, dst - obj->addr, n, src);
	} else if(obj->type == TYPE_IBO) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->gl_buffer);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, dst - obj->addr, n, src);
	} else if(obj->type == TYPE_TBO)
		write_texture(obj, dst, src, n);
}

// flush object's data to VRAM
void flush_object(object_t* obj) {
	uint8_t* data = malloc(obj->len);

	if(!obj->in_overlaps) {		// optimal case: no overlaps
		object_read(obj, data, obj->addr, obj->len);
		gpu_write(obj->addr, data, obj->len);
	} else {
		// the preferred refcount age for overlapping objects is swapped here.
		// we read newest (latest writes - prioritize objects referenced this
		// cmd buffer) and write to oldest (since that's what gpu_read sees).
		gpu_read_newest(data, obj->addr, obj->len);
		gpu_write_oldest(obj->addr, data, obj->len);
	}

	free(data);
}

void flush_all_overlaps() {
	// writes directly to VRAM, skips object updating - they'll be freed anyway
	for(uint32_t i = 0; i < obj_overlaps_count; i++) {
		object_t* obj = obj_overlaps_list[i];
		uint8_t* data = malloc(obj->len);

		gpu_read_newest(data, obj->addr, obj->len);
		memmove(vram + obj->addr, data, obj->len);
		free(data);
	}
}

void free_object(object_t* obj) {
	remove_from_bucket(obj);
	remove_from_overlaps(obj);

	if(obj->in_overlaps) {
		uint32_t count = get_region_object_count(obj->addr, obj->len);

		// recalc overlaps list for this object's range
		for(uint32_t i = 0; i < count; i++) {
			object_t* object = get_region_object_list(i, obj->addr, obj->len);
			remove_from_overlaps(object);
		}
		mark_all_overlaps(obj->addr, obj->len);
	}

	if(obj->type == TYPE_VBO || obj->type == TYPE_IBO || obj->type == TYPE_TBO)
		glDeleteBuffers(1, &obj->gl_buffer);
	free(obj);
}

void destroy_all_overlaps() {
	flush_all_overlaps();
	for(uint32_t i = 0; i < obj_overlaps_count; i++)
		free_object(obj_overlaps_list[i]);
}

object_t* ref_buffer_precise(uint64_t addr, uint8_t type, int64_t len) {
	if(addr >= VRAM_CAPACITY) {
		WARN("referenced buffer starting address %llx past end of VRAM\n", addr);
		return 0;
	}
	if(addr % 256) {
		WARN("referenced buffer address %llx is not 256-byte aligned\n", addr);
		return 0;
	}

	header_t header;
	if(len == LENGTH_IN_BUFFER)
		len = get_header_info(&header, addr, type);

	if(!len) {
		WARN("referenced zero-length buffer %llx\n", addr);
		return 0;
	}

	uint64_t end = addr + len - 1;
	if(addr + len - 1 >= VRAM_CAPACITY) {
		WARN("referenced buffer ending address %llx past end of VRAM\n", end);
		return 0;
	}

	object_t* obj = get_object_precise(addr, type, len);
	if(!obj || obj->need_update) {
		obj = get_object_precise(addr, type, ANY_LENGTH);

		if(obj) {	// object must be resized or updated
			flush_object(obj);
			free_object(obj);
		}

		obj = create_object(&header, addr, type, len);
	}

	obj->refcount = ref_counter++;
	return obj;
}
