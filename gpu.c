#include "../../defs.h"

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

	memcpy(batch, &get_ram()[read_ptr], read_len - overflow);
	memcpy(batch + overflow, &get_ram()[ring_addr], overflow);

	for(uint32_t i = 0; i < read_len / 8; i++)
		dispatch_cmd_buffer(batch[i]);

	free(batch);

	glFinish();
}

void issue_batch() {
	uint32_t* ctrl 		= (uint32_t*)&get_ram()[GPU_CTRL_REG];
	uint64_t* ring_addr	= (uint64_t*)&get_ram()[QUEUE_ADDR_REG];
	uint64_t* read_ptr	= (uint64_t*)&get_ram()[QUEUE_READ_PTR_REG];
	uint64_t* read_len	= (uint64_t*)&get_ram()[QUEUE_READ_LEN_REG];

	if(!(*ctrl & DOORBELL_BIT))
		return;

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
}

void gpu_registers_update(void* cpu, uint64_t start, uint64_t length) {
	uint32_t* ctrl 		= (uint32_t*)&get_ram()[GPU_CTRL_REG];
	uint32_t* scan_ctrl	= (uint32_t*)&get_ram()[SCANOUT_CTRL_REG];
	uint64_t* scan_tbo	= (uint64_t*)&get_ram()[SCANOUT_TBO_ADDR_REG];
	uint32_t* copy_r	= (uint32_t*)&get_ram()[READ_CTRL_REG];
	uint32_t* copy_w	= (uint32_t*)&get_ram()[WRITE_CTRL_REG];

	if(*copy_r & REQUEST_READ_BIT) {
		uint64_t dst = *(uint64_t*)(get_ram() + READ_DST_ADDR_REG);
		uint64_t src = *(uint64_t*)(get_ram() + READ_SRC_ADDR_REG);
		uint64_t n   = *(uint64_t*)(get_ram() + READ_LEN_REG);
		request_read(dst, src, n);
	}

	if(*copy_w & REQUEST_WRITE_BIT) {
		uint64_t dst = *(uint64_t*)(get_ram() + WRITE_DST_ADDR_REG);
		uint64_t src = *(uint64_t*)(get_ram() + WRITE_SRC_ADDR_REG);
		uint64_t n   = *(uint64_t*)(get_ram() + WRITE_LEN_REG);
		request_write(dst, src, n);
	}

	if(*ctrl & DOORBELL_BIT)
		gpu_batch();

	if(*scan_ctrl & PAGE_FLIP_BIT) {
		*scan_ctrl &= ~PAGE_FLIP_BIT;
		gpu_flip(*scan_tbo, (*scan_ctrl & VSYNC_ON_BIT) > 0);
	}
}
