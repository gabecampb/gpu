#include "defs.h"

uint8_t vram[VRAM_CAPACITY];
pthread_mutex_t atomic_rw_mx = PTHREAD_MUTEX_INITIALIZER;

#define MATCH_LOWEST 0
#define MATCH_HIGHEST 1

object_t* next_object(uint64_t addr, uint64_t search_len, object_t* exclude_obj, uint8_t match_refcount) {
	object_t* next_obj = 0;

	uint64_t min_addr = UINT64_MAX;	// want first object
	int64_t best_refcount = match_refcount == MATCH_LOWEST ? INT64_MAX : INT64_MIN;
	uint32_t count = get_region_object_count(addr, search_len);
	for(uint32_t i = 0; i < count; i++) {
		object_t* obj = get_region_object_list(i, addr, search_len);

		if(obj == exclude_obj)
			continue;

		// require better refcount than excluded for portion cut-off
		if(exclude_obj) {
			if((match_refcount == MATCH_LOWEST  && obj->refcount > exclude_obj->refcount)
			|| (match_refcount == MATCH_HIGHEST && obj->refcount < exclude_obj->refcount))
				continue;
		}

		uint8_t better_refcount = 0;
		if((match_refcount == MATCH_LOWEST  && obj->refcount < best_refcount)
		|| (match_refcount == MATCH_HIGHEST && obj->refcount > best_refcount))
			better_refcount = 1;

		// for same-address objects, require better refcount
		if(obj->addr == min_addr) {
			if(!better_refcount)
				continue;
		}

		if(obj->addr < addr) {	// inside object
			if(!better_refcount)
				continue;
			next_obj = obj;
			min_addr = addr;
			best_refcount = obj->refcount;
		}

		// even if worse refcount, prioritize lowest address
		if(obj->addr >= addr && obj->addr <= min_addr) {
			next_obj = obj;
			min_addr = obj->addr;
			if(better_refcount)
				best_refcount = obj->refcount;
		}
	}

	return next_obj;
}

object_t* get_portion(uint64_t addr, uint64_t* len, uint64_t max_len, uint8_t match_refcount) {
	uint32_t count = get_region_object_count(addr, max_len);

	if(count == 1) {	// optimal case: single object
		object_t* obj = get_region_object_list(0, addr, max_len);
		if(obj->addr <= addr) {
			uint64_t offset = addr - obj->addr;
			uint64_t remaining = obj->len - offset;
			uint64_t end = obj->addr + obj->len - 1;
			if(addr + max_len - 1 <= end) {
				*len = remaining > max_len ? max_len : remaining;
				return obj;
			}
		}
	}

	if(!count) {		// optimal case: no objects
		*len = max_len;
		return 0;
	}

	// in maximum span, choose first object with highest or lowest refcount
	object_t* chosen_obj = next_object(addr, max_len, 0, match_refcount);
	if(!chosen_obj)
		ERROR("get_portion failed to get object at %llx\n", addr);

	if(chosen_obj->addr > addr) {
		*len = chosen_obj->addr - addr;
		*len = *len > max_len ? max_len : *len;
		return 0;
	}

	uint64_t offset = addr - chosen_obj->addr;
	uint64_t remaining = chosen_obj->len - offset;
	remaining = remaining > max_len ? max_len : remaining;
	count = get_region_object_count(addr, remaining);

	// no other object before the end of this object or single byte portion
	if(count == 1 || remaining == 1) {
		*len = remaining > max_len ? max_len : remaining;
		return chosen_obj;
	}

	// cut portion if there's another object before the end of this object
	object_t* obj = next_object(addr + 1, remaining - 1, chosen_obj, match_refcount);
	if(obj)
		remaining = obj->addr - addr;

	*len = remaining > max_len ? max_len : remaining;
	return chosen_obj;
}

uint8_t* __gpu_read(uint8_t* dst, uint64_t src, uint64_t n, uint8_t match_refcount) {
	if(!n || src + n >= VRAM_CAPACITY) {
		WARN("gpu_read [%llx, %llx] out of VRAM bounds\n", src, src + n - 1);
		return 0;
	}

	uint64_t addr = src, total_bytes_read = 0;
	while(total_bytes_read < n) {
		uint64_t max_len = n - total_bytes_read;
		uint64_t read_len = 0;
		object_t* obj = get_portion(addr, &read_len, max_len, match_refcount);

		if(obj)
			object_read(obj, dst + total_bytes_read, addr, read_len);
		else
			memmove(dst + total_bytes_read, vram + addr, read_len);

		total_bytes_read += read_len;
		addr += read_len;
	}

	return dst;
}

uint8_t* gpu_read(uint8_t* dst, uint64_t src, uint64_t n) {
	return __gpu_read(dst, src, n, MATCH_LOWEST);
}

uint8_t* gpu_read_newest(uint8_t* dst, uint64_t src, uint64_t n) {
	return __gpu_read(dst, src, n, MATCH_HIGHEST);
}

void __gpu_write(uint64_t dst, uint8_t* src, uint64_t n, uint8_t match_refcount) {
	if(!n || dst + n >= VRAM_CAPACITY) {
		WARN("gpu_write [%llx, %llx] out of VRAM bounds\n", dst, dst + n - 1);
		return;
	}

	memmove(vram + dst, src, n);

	uint64_t addr = dst, total_bytes_written = 0;
	while(total_bytes_written < n) {
		uint64_t max_len = n - total_bytes_written;
		uint64_t write_len = 0;
		object_t* obj = get_portion(addr, &write_len, max_len, match_refcount);

		// to keep gpu_read coherent, we update GPU-side data immediately
		if(obj)
			object_write(obj, addr, src + total_bytes_written, write_len);

		total_bytes_written += write_len;
		addr += write_len;
	}
}

void gpu_write(uint64_t dst, uint8_t* src, uint64_t n) {
	__gpu_write(dst, src, n, MATCH_HIGHEST);
}

void gpu_write_oldest(uint64_t dst, uint8_t* src, uint64_t n) {
	__gpu_write(dst, src, n, MATCH_LOWEST);
}

uint64_t a_read_u64(uint64_t* value) {
	pthread_mutex_lock(&atomic_rw_mx);
	uint64_t x = *value;
	pthread_mutex_unlock(&atomic_rw_mx);
	return x;
}

void a_write_u64(uint64_t* value, uint64_t new_value) {
	pthread_mutex_lock(&atomic_rw_mx);
	*value = new_value;
	pthread_mutex_unlock(&atomic_rw_mx);
}
