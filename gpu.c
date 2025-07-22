#include "defs.h"

// GPU control register flags
#define DOORBELL_BIT (1 << 31)
#define RW_REQ_BIT (1 << 30)
#define RW_OP_BIT (1 << 29)

#define GPU_CTRL_REG			0x26000
#define RAM_ADDR_REG			0x26004
#define VRAM_ADDR_REG			0x2600C
#define QUEUE_ADDR_REG			0x26014
#define QUEUE_READ_PTR_REG		0x2601C
#define QUEUE_READ_LEN_REG		0x26024

void command_decoder(uint8_t* commands, uint64_t len) {

}

void dispatch_cmd_buffer(uint64_t addr) {
	if(addr % 256) {
		WARN("command buffer address %llx is not 256-byte aligned, skipping\n", addr);
		return;
	}
	ref_buffer_precise(addr, TYPE_CBO, LENGTH_IN_BUFFER);
	object_t* obj = get_object_precise(addr, TYPE_CBO, ANY_LENGTH);

	if(!obj) {
		WARN("failed to get command buffer object %llx, skipping\n", addr);
		return;
	}

	uint8_t* cmds = calloc(1, obj->len);
	gpu_read(cmds, addr, obj->len);

	command_decoder(cmds + obj->header_len, obj->header.n_cmd_bytes);

	destroy_all_overlaps();
	free(cmds);
}

void process_batch(uint64_t ring_addr, uint64_t read_ptr, uint64_t read_len) {
	if(read_len == 0) {
		WARN("read length for batch is 0, skipping\n");
		return;
	}
	if(read_len % 8) {
		WARN("read length %llx for batch is not a multiple of 8, skipping\n", read_len);
		return;
	}

	uint64_t ring_end = ring_addr + 16384 - 1;
	if(read_ptr < ring_addr || read_ptr > ring_end) {
		WARN("read pointer %llx for batch is not within DMA ring bounds [%llx, %llx], skipping\n",
				read_ptr, ring_addr, ring_end);
		return;
	}

	uint64_t* batch = calloc(1, read_len);
	uint64_t overflow = (read_ptr + read_len > ring_end) ?
		read_ptr + read_len - ring_end : 0;

	memcpy(batch, &ram[read_ptr], read_len - overflow);
	memcpy(batch + overflow, &ram[ring_addr], overflow);

	for(uint32_t i = 0; i < read_len / 8; i++)
		dispatch_cmd_buffer(batch[i]);

	free(batch);

	glFinish();
}

void gpu_registers_update(uint64_t start, uint64_t length) {
	uint32_t* ctrl 		= (uint32_t*)&ram[GPU_CTRL_REG];
	uint64_t* ring_addr	= (uint64_t*)&ram[QUEUE_ADDR_REG];
	uint64_t* read_ptr	= (uint64_t*)&ram[QUEUE_READ_PTR_REG];
	uint64_t* read_len	= (uint64_t*)&ram[QUEUE_READ_LEN_REG];

	if((*ctrl) & DOORBELL_BIT) {
		uint64_t old_read_ptr = *read_ptr;
		*read_ptr += *read_len;
		*ctrl &= ~DOORBELL_BIT;
		if(*ring_addr % 16384) {
			WARN("DMA ring address %llx misaligned, skip doorbell ring\n", ring_addr);
			return;
		}
		if(*ring_addr + 16384 - 1 >= RAM_CAPACITY) {
			WARN("DMA ring address %llx out of bounds, skip doorbell ring\n", ring_addr);
			return;
		}
		process_batch(*ring_addr, old_read_ptr, *read_len);
		// TODO: check for new doorbell rings issued since the last
	}
}
