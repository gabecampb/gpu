#include "defs.h"

uint8_t ram[RAM_CAPACITY];

GLFWwindow* window;
uint16_t window_width, window_height;

void init_glfw() {
	if(!glfwInit())
		ERROR("failed to initialize glfw\n");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	window = glfwCreateWindow(640, 480, "GPU output", NULL, NULL);
	if(!window)
		ERROR("failed to create window\n");

	int w, h;
	glfwGetWindowSize(window, &w, &h);
	window_width  = w;
	window_height = h;

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
