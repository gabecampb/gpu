#ifndef GPU_H
#define GPU_H

#include "defs.h"

#define GPU_REGS_LOW  0x26000
#define GPU_REGS_HIGH 0x26FFF

// GPU register addresses
#define GPU_CTRL_REG			0x26000
#define RAM_ADDR_REG			0x26004
#define VRAM_ADDR_REG			0x2600C
#define QUEUE_ADDR_REG			0x26014
#define QUEUE_READ_PTR_REG		0x2601C
#define QUEUE_READ_LEN_REG		0x26024
#define SCANOUT_CTRL_REG		0x2602C
#define SCANOUT_TBO_ADDR_REG	0x26030
#define READ_CTRL_REG			0x26038
#define READ_DST_ADDR_REG		0x2603C
#define READ_SRC_ADDR_REG		0x26044
#define READ_LEN_REG			0x2604C
#define WRITE_CTRL_REG			0x26054
#define WRITE_DST_ADDR_REG		0x26058
#define WRITE_SRC_ADDR_REG		0x26060
#define WRITE_LEN_REG			0x26068

// GPU control register flags
#define DOORBELL_BIT (1 << 31)

// scanout control register flags
#define PAGE_FLIP_BIT (1 << 31)
#define VSYNC_ON_BIT (1 << 30)

// copy control register flags
#define REQUEST_READ_BIT (1 << 31)
#define REQUEST_WRITE_BIT (1 << 31)

void gpu_registers_update(uint64_t, uint64_t);

#endif
