#ifndef DEFS_H
#define DEFS_H

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

extern uint8_t ram[];
extern GLFWwindow* window;

#include "mem.h"
#include "buffer.h"
#include "texture.h"
#include "flip.h"
#include "gpu.h"

#define ERROR(...) { printf("fatal error: "); printf(__VA_ARGS__); exit(1); }
#define WARN(...) printf(__VA_ARGS__)
#define LOG(...) printf(__VA_ARGS__)

#define NS_PER_SEC 1000000000

#endif
