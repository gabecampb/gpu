#ifndef GPU_H
#define GPU_H

#include "defs.h"

#define GPU_REGS_LOW  0x26000
#define GPU_REGS_HIGH 0x26FFF

void gpu_registers_update(uint64_t, uint64_t);

#endif
