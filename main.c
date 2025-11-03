#include "../../defs.h"

uint8_t ram[RAM_CAPACITY];

GLFWwindow* window;
pthread_mutex_t atomic_rw_mx = PTHREAD_MUTEX_INITIALIZER;

void gpu_flip(uint64_t a, uint8_t v)	{ page_flip(a, v); }
void gpu_batch()						{ issue_batch(); }
void page_flip_irq()					{};
void dma_read_complete_irq()			{};
void dma_write_complete_irq()			{};

GLFWwindow* get_window() {
	return window;
}
uint8_t* get_ram() {
	return ram;
}

void atomic_move(uint8_t* dst, uint8_t* src, uint32_t n) {
	pthread_mutex_lock(&atomic_rw_mx);
	memmove(dst, src, n);
	pthread_mutex_unlock(&atomic_rw_mx);
}

void atomic_set_u8(uint8_t* var, uint8_t val) {
	atomic_move(var, &val, 1);
}

void atomic_set_u64(uint64_t* var, uint64_t val) {
	atomic_move(var, &val, 8);
}

uint8_t atomic_get_u8(uint8_t* var) {
	uint8_t val;
	atomic_move(&val, var, 1);
	return val;
}

uint64_t atomic_get_u64(uint64_t* var) {
	uint64_t val;
	atomic_move(&val, var, 8);
	return val;
}

void init_glfw() {
	if(!glfwInit())
		ERROR("failed to initialize glfw\n");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	window = glfwCreateWindow(640, 480, "GPU output", NULL, NULL);
	if(!window)
		ERROR("failed to create window\n");

	glfwMakeContextCurrent(window);
	glClearColor(0., 0., 0., 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);
}

int main() {
	init_glfw();
	LOG("initialized OpenGL\n");
	while(!glfwWindowShouldClose(window)) {
		object_t* obj = ref_buffer_precise(0, TYPE_VBO, 5);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}
