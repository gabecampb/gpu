#include "defs.h"

GLuint page_flip_fbo;

pthread_t vblank_wait_thread;
uint64_t curr_wait_ns, suppress_vsync_irq;

void issue_page_flip_irq() {
	// stub for page flip completion IRQs
}

void* vblank_wait_func(void* args) {
	uint64_t wait_to = a_read_u64(&curr_wait_ns);

	struct timespec tm;
	tm.tv_sec	= wait_to / NS_PER_SEC;
	tm.tv_nsec	= wait_to % NS_PER_SEC;

	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tm, NULL);

	if(!a_read_u64(&suppress_vsync_irq) && a_read_u64(&curr_wait_ns) == wait_to)
		issue_page_flip_irq();
}

void page_flip(uint64_t addr, uint8_t vsync_on) {
	object_t* obj = ref_buffer_precise(addr, TYPE_TBO, LENGTH_IN_BUFFER);
	if(!obj) {
		WARN("page_flip: failed to get texture object at %llx\n", addr);
		return;
	}
	if(obj->header.n_dims != 2) {
		WARN("page_flip: texture object %llx was not 2D\n", obj->addr);
		return;
	}
	if(!IS_COLOR_FORMAT(obj->header.tex_format)) {
		WARN("page_flip: texture object %llx was not of color format\n", obj->addr);
		return;
	}

	if(!page_flip_fbo)
		glGenFramebuffers(1, &page_flip_fbo);

	// blit texture to default framebuffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, page_flip_fbo);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, obj->gl_buffer, 0);
	GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE) {
		WARN("page_flip: read framebuffer was incomplete\n");
		return;
	}

	// determine next vblank start time
	GLFWmonitor* mon = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(mon);
	int ref_rate = mode->refreshRate;
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	uint64_t interval_ns = NS_PER_SEC / ref_rate;
	uint64_t now_ns = tm.tv_sec * NS_PER_SEC + tm.tv_nsec;
	uint64_t next_vblank_ns = (now_ns / interval_ns + 1) * interval_ns;

	// perform blit to display
	glBindTexture(GL_TEXTURE_2D, obj->gl_buffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, page_flip_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	int w, h;
	glfwGetWindowSize(window, &w, &h);
	glBlitFramebuffer(0, obj->header.dims[1], obj->header.dims[0], 0, 0, 0,
		w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	// send page flip completion IRQ
	if(vsync_on) {
		a_write_u64(&suppress_vsync_irq, 0);
		if(a_read_u64(&curr_wait_ns) < next_vblank_ns) {
			a_write_u64(&curr_wait_ns, next_vblank_ns);

			pthread_create(&vblank_wait_thread, NULL, vblank_wait_func, NULL);
			pthread_detach(vblank_wait_thread);
		}
	} else if(!vsync_on) {		// if vsync is off, page flip is immediate
		a_write_u64(&suppress_vsync_irq, 1);
		issue_page_flip_irq();
	}

	glfwSwapInterval(vsync_on > 0);
	glfwSwapBuffers(window);
	glfwPollEvents();
}
