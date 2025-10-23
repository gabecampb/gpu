#ifndef DEFS_H
#define DEFS_H

#define RAM_CAPACITY 0x4000000
#define NS_PER_SEC 1000000000

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

GLFWwindow* get_window();
uint8_t* get_ram();
uint8_t atomic_get_u8(uint8_t*);
uint64_t atomic_get_u64(uint64_t*);
void atomic_set_u8(uint8_t*, uint8_t);
void atomic_set_u64(uint64_t*, uint64_t);

#include "gpu.h"

#endif
