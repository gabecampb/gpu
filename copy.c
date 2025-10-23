#include "../../defs.h"

#define READ_FROM_DEVICE 0
#define WRITE_TO_DEVICE  1

uint8_t ongoing_read, ongoing_write;

typedef struct copy_args_t {
	uint8_t copy_type;
	uint8_t *dst, *src;
	uint64_t n;
} copy_args_t;

void copy_thread_func(copy_args_t* a) {
	memcpy(a->dst, a->src, a->n);

	if(a->copy_type == READ_FROM_DEVICE) {
		atomic_set_u8(&ongoing_read, 0);
		*(uint32_t*)(get_ram() + READ_CTRL_REG) &= ~REQUEST_READ_BIT;
	} else {
		atomic_set_u8(&ongoing_write, 0);
		*(uint32_t*)(get_ram() + WRITE_CTRL_REG) &= ~REQUEST_WRITE_BIT;
	}

	if(a->copy_type == READ_FROM_DEVICE)
		dma_read_complete_irq();
	else
		dma_write_complete_irq();
	free(a);
}

void launch_copy_thread(uint64_t dst, uint64_t src, uint64_t n, uint8_t type,
	uint8_t* ongoing_status, uint8_t* dst_ptr, uint8_t* src_ptr) {
	if(atomic_get_u8(ongoing_status))
		return;

	uint64_t end = dst + n - 1;
	if(end >= RAM_CAPACITY || end >= VRAM_CAPACITY)
		return;

	atomic_set_u8(ongoing_status, 1);

	if(type == READ_FROM_DEVICE) {
		uint32_t count = get_region_object_count(src, n);
		for(uint32_t i = 0; i < count; i++) {
			object_t* obj = get_region_object_list(i, src, n);
			flush_object(obj);
		}
	} else {
		uint32_t count = get_region_object_count(dst, n);
		for(uint32_t i = 0; i < count; i++) {
			object_t* obj = get_region_object_list(i, dst, n);
			obj->need_update = 1;
		}
	}

	copy_args_t* args = malloc(sizeof(copy_args_t));
	args->copy_type = type;
	args->dst = dst_ptr;
	args->src = src_ptr;
	args->n = n;

	pthread_t copy_thread;
	pthread_create(&copy_thread, NULL, copy_thread_func, args);
	pthread_detach(copy_thread);
}

void request_read(uint64_t dst, uint64_t src, uint64_t n) {
	launch_copy_thread(dst, src, n, READ_FROM_DEVICE, &ongoing_read, get_ram() + dst, vram + src);
}

void request_write(uint64_t dst, uint64_t src, uint64_t n) {
	launch_copy_thread(dst, src, n, WRITE_TO_DEVICE, &ongoing_write, vram + dst, get_ram() + src);
}
