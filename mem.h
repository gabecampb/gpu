#ifndef MEM_H
#define MEM_H

#include "defs.h"

#define RAM_CAPACITY	0x4000000	/* default: 64 MB */
#define VRAM_CAPACITY	0x8000000	/* default: 128 MB */

extern uint8_t ram[RAM_CAPACITY];
extern uint8_t vram[VRAM_CAPACITY];
uint8_t* gpu_read(uint8_t* dst, uint64_t src, uint64_t n);
uint8_t* gpu_read_newest(uint8_t* dst, uint64_t src, uint64_t n);
void gpu_write(uint64_t dst, uint8_t* src, uint64_t n);
void gpu_write_oldest(uint64_t dst, uint8_t* src, uint64_t n);

uint64_t a_read_u64(uint64_t* value);
void a_write_u64(uint64_t* value, uint64_t new_value);

#endif
