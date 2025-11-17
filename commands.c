#include "../../defs.h"

GLuint gl_fbo;
uint32_t fbo_dims[2];
uint32_t fbo_n_color_attachs;
uint8_t cmd_regs[NUM_BYTES_CMD_REGS];

void set_raster_states();
void bind_vao(object_t* vbo);
void gl_set_draw_buffers(uint8_t bmp);
void bind_fbo();

uint32_t exec_cmd(uint16_t op, uint8_t* cmd, uint8_t* end) {
	// before any command that might access descriptors, need to run
	// bind_dtables() because it's assumed the dtables that will be accessed are
	// configured by that point. done again after kernel/dtable address changes.
	uint8_t need_dtable_bind = 1;

	switch(op) {
		case CMD_SET_REG_32: {
			if(cmd + 14 > end) {
				WARN("set 32-bit register command out of bounds\n");
				return 14;
			}

			uint64_t reg_addr = *(uint64_t*)(cmd + 2);
			if(reg_addr + 3 >= NUM_BYTES_CMD_REGS) {
				WARN("set 32-bit register failed - out of register profile bounds\n");
				return 14;
			}

			uint32_t value = *(uint32_t*)(cmd + 10);
			*(uint32_t*)(cmd_regs + reg_addr) = value;

			uint64_t x1 = reg_addr, x2 = reg_addr + 3;
			uint64_t y1 = FB_CFG_REG, y2 = DEPTH_ATTACH_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				bind_fbo();

			y1 = DTBL_0_ADDR_REG, y2 = KERNEL_ADDR_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				need_dtable_bind = 1;

			y1 = KERNEL_ADDR_REG, y2 = KERNEL_ADDR_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				bind_kernel();

			y1 = UNIFORM_0_REG, y2 = UNIFORM_0_REG + 127;
			if(check_overlap(x1, x2, y1, y2))
				load_uregs();

			y1 = RASTER_CFG_REG, y2 = RASTER_CFG_REG + 16 +
				(MAX_COLOR_ATTACH_COUNT * 8) - 1;
			if(check_overlap(x1, x2, y1, y2))
				set_raster_states();

			return 14;
		} case CMD_SET_REG_64: {
			if(cmd + 18 > end) {
				WARN("set 64-bit register command out of bounds\n");
				return 18;
			}

			uint64_t reg_addr = *(uint64_t*)(cmd + 2);
			if(reg_addr + 7 >= NUM_BYTES_CMD_REGS) {
				WARN("set 64-bit register failed - out of register profile bounds\n");
				return 18;

			}

			uint64_t value = *(uint64_t*)(cmd + 10);
			*(uint64_t*)(cmd_regs + reg_addr) = value;

			uint64_t x1 = reg_addr, x2 = reg_addr + 7;
			uint64_t y1 = FB_CFG_REG, y2 = DEPTH_ATTACH_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				bind_fbo();

			y1 = DTBL_0_ADDR_REG, y2 = KERNEL_ADDR_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				need_dtable_bind = 1;

			y1 = KERNEL_ADDR_REG, y2 = KERNEL_ADDR_REG + 7;
			if(check_overlap(x1, x2, y1, y2))
				bind_kernel();

			y1 = UNIFORM_0_REG, y2 = UNIFORM_0_REG + 127;
			if(check_overlap(x1, x2, y1, y2))
				load_uregs();

			y1 = RASTER_CFG_REG, y2 = RASTER_CFG_REG + 16 +
				(MAX_COLOR_ATTACH_COUNT * 8) - 1;
			if(check_overlap(x1, x2, y1, y2))
				set_raster_states();

			return 18;
		} case CMD_DRAW: {
			if(need_dtable_bind) {
				bind_dtables();
				need_dtable_bind = 0;
			}

			uint64_t vbo_addr	= *(uint64_t*)(cmd_regs + VBO_ADDR_REG);
			uint64_t vbo_len	= *(uint64_t*)(cmd_regs + VBO_LEN_REG);
			uint64_t base_idx	= *(uint64_t*)(cmd_regs + BASE_IDX_REG);
			uint64_t idx_count	= *(uint64_t*)(cmd_regs + IDX_COUNT_REG);

			if(vbo_len > VRAM_CAPACITY) {
				WARN("length for vbo %llx too large, skipping command\n", vbo_addr);
				return 2;
			}

			object_t* vbo = ref_buffer_precise(vbo_addr, TYPE_VBO, vbo_len);
			if(!vbo) {
				WARN("failed to get vbo %llx for draw, skipping command\n", vbo_addr);
				return 2;
			}

			bind_vao(vbo);
			glDrawArrays(GL_TRIANGLES, base_idx, idx_count);
			return 2;
		} case CMD_CLEAR_ATTACHS: {
			if(cmd + 27 > end) {
				WARN("clear attachments command out of bounds\n");
				return 27;
			}

			uint32_t bmp = *(uint32_t*)(cmd + 2);
			float rgba[4];
			memmove(rgba, cmd + 6, 16);
			float depth = *(uint32_t*)(cmd + 22);
			uint8_t stencil = *(uint8_t*)(cmd + 26);

			glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
			glClearDepth(depth);
			glClearStencil(stencil);

			uint32_t fbo_color_attachs_bmp = (1 << fbo_n_color_attachs) - 1;
			uint32_t clr_color_bmp = bmp & fbo_color_attachs_bmp;
			gl_set_draw_buffers(clr_color_bmp);

			GLbitfield mask = clr_color_bmp ? GL_COLOR_BUFFER_BIT : 0;
			if((bmp & CLEAR_DEPTH_ATTACH_BIT) > 0) mask |= GL_DEPTH_BUFFER_BIT;
			if((bmp & CLEAR_STENCIL_ATTACH_BIT) > 0) mask |= GL_STENCIL_BUFFER_BIT;
			glClear(mask);

			gl_set_draw_buffers(fbo_color_attachs_bmp);
			return 27;
		} default:
			return 0;
	}
}

// process all in 'commands', up to 'len' bytes
void command_decoder(uint8_t* commands, uint64_t len) {
	uint8_t* end = commands + len - 1;

	memset(cmd_regs, 0, NUM_BYTES_CMD_REGS);
	bind_fbo();
	set_raster_states();

	while(commands < end) {
		if(commands + 1 > end) {
			WARN("command op is past end of command buffer\n");
			break;
		}
		uint16_t op = *(uint16_t*)commands;
		uint32_t read_amt = exec_cmd(op, commands, end);
		if(!read_amt) {
			WARN("command decoding stopped due to malformed command (opcode: %x)\n", op);
			return;
		}
		commands += read_amt;
	}
}

void gl_set_stencil_cfg(GLenum face, uint64_t cfg) {
	uint8_t func_idx = cfg & 0x7;
	uint8_t func_ref = (cfg >> 3) & 0xFF;
	uint8_t func_mask = (cfg >> 11) & 0xFF;

	GLenum func[] = { GL_ALWAYS, GL_NEVER, GL_LESS, GL_LEQUAL, GL_GREATER,
		GL_GEQUAL, GL_EQUAL, GL_NOTEQUAL };
	glStencilFuncSeparate(face, func[func_idx], func_ref, func_mask);

	uint8_t write_mask = (cfg >> 19) & 0xFF;
	glStencilMaskSeparate(face, write_mask);

	GLenum op[] = { GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_INCR_WRAP,
		GL_DECR, GL_DECR_WRAP, GL_INVERT };
	uint8_t spdp = (cfg >> 27) & 0x7;
	uint8_t spdf = (cfg >> 30) & 0x7;
	uint8_t sf = (cfg >> 33) & 0x7;
	glStencilOpSeparate(face, op[sf], op[spdf], op[spdp]);
}

void check_blend_param_range(uint8_t* op, uint32_t maximum) {
	if(*op > maximum) {
		WARN("blending parameter %llu was invalid, defaulting to 0\n", *op);
		*op = 0;
	}
}

void gl_set_blend_cfg(uint32_t idx, uint64_t cfg) {
	uint8_t a_dfac		= cfg & 0xF;
	uint8_t a_sfac		= (cfg >> 4) & 0xF;
	uint8_t a_op		= (cfg >> 8) & 0x7;
	uint8_t rgb_dfac	= (cfg >> 11) & 0xF;
	uint8_t rgb_sfac	= (cfg >> 15) & 0xF;
	uint8_t rgb_op		= (cfg >> 18) & 0x7;

	GLenum fac[] = { GL_ONE, GL_ZERO, GL_SRC_COLOR, GL_DST_COLOR, GL_SRC_ALPHA,
		GL_DST_ALPHA, GL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_DST_COLOR,
		GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA };

	check_blend_param_range(&a_dfac, 9);
	check_blend_param_range(&a_sfac, 9);
	check_blend_param_range(&rgb_dfac, 9);
	check_blend_param_range(&rgb_sfac, 9);

	glBlendFuncSeparatei(idx, fac[rgb_sfac], fac[rgb_dfac], fac[a_sfac], fac[a_dfac]);

	GLenum op[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT,
		GL_MIN, GL_MAX };

	check_blend_param_range(&a_op, 4);
	check_blend_param_range(&rgb_op, 4);

	glBlendEquationSeparatei(idx, op[rgb_op], op[a_op]);
}

void set_raster_states() {
	uint64_t cfg  = *(uint64_t*)(cmd_regs + RASTER_CFG_REG);
	uint64_t cfg2 = *(uint64_t*)(cmd_regs + RASTER_CFG_REG + 8);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_BLEND);

	glEnable(GL_CULL_FACE);
	switch(cfg & 0x3) {
		case 0: glDisable(GL_CULL_FACE);		break;
		case 1: glCullFace(GL_BACK);			break;
		case 2: glCullFace(GL_FRONT);			break;
		case 3: glCullFace(GL_FRONT_AND_BACK);	break;
	}
	((cfg >> 2) & 0x1) ? glFrontFace(GL_CW) : glFrontFace(GL_CCW);

	uint8_t depth_func = (cfg >> 3) & 0x7;
	uint8_t write_depth = (cfg >> 6) & 0x1;

	glDepthMask(write_depth ? GL_TRUE : GL_FALSE);

	GLenum func[] = { GL_ALWAYS, GL_NEVER, GL_LESS, GL_LEQUAL, GL_GREATER,
		GL_GEQUAL, GL_EQUAL, GL_NOTEQUAL };
	glDepthFunc(func[depth_func]);

	gl_set_stencil_cfg(GL_BACK, cfg >> 9);
	gl_set_stencil_cfg(GL_FRONT, (cfg2 << 19) | (cfg >> 45));

	uint32_t* blend_cfg = (uint32_t*)(cmd_regs + BLEND_0_CFG_REG);
	for(uint32_t i = 0; i < MAX_COLOR_ATTACH_COUNT; i++)
		gl_set_blend_cfg(i, blend_cfg[i]);
}

void set_va(uint32_t index, uint32_t va_cfg) {
	if(!(va_cfg & ENABLE_VA_BIT))
		return;

	uint8_t normalize = (va_cfg >> 30) & 0x1;
	uint8_t convert_to_float = (va_cfg >> 29) & 0x1;
	uint8_t type	= (va_cfg >> 24) & 0x1F;
	uint8_t count	= ((va_cfg >> 22) & 0x3) + 1;
	uint32_t stride	= ((va_cfg >> 11) & 0x7FF) + 1;
	size_t offset	= va_cfg & 0x7FF;

	if(!IS_VALID_VA_TYPE(type)) {
		WARN("invalid type, skipping attribute\n");
		return;
	}

	GLenum gl_type		= GET_VA_GL_TYPE(type);
	uint32_t comp_size	= GET_VA_COMPONENT_WIDTH(type);
	uint32_t el_size	= comp_size * count;

	if(type == VA_TYPE_F32 && normalize) {
		WARN("cannot normalize floating-point, skipping attribute\n");
		return;
	}

	if(type == VA_TYPE_F32 && convert_to_float) {
		WARN("convert to float is for integers only, skipping attribute\n");
		return;
	}

	if(stride > MAX_VA_STRIDE) {
		WARN("stride is greater than maximum allowed, skipping attribute\n");
		return;
	}

	if(el_size > stride) {
		WARN("stride is less than element size, skipping attribute\n");
		return;
	}

	if(offset % comp_size) {
		WARN("offset not aligned to component size, skipping attribute\n");
		return;
	}

	if(offset + el_size - 1 >= stride) {
		WARN("attribute end is beyond vertex stride, skipping attribute\n");
		return;
	}

	if(type == VA_TYPE_F32 || convert_to_float)
		glVertexAttribPointer(index, count, gl_type, normalize ? GL_TRUE : GL_FALSE, stride, (void*)offset);
	else
		glVertexAttribIPointer(index, count, gl_type, stride, (void*)offset);

	glEnableVertexAttribArray(index);
}

// construct + bind VAO for this VBO currently described by command registers
void bind_vao(object_t* vbo) {
	uint32_t* va_cfg = (uint32_t*)(cmd_regs + VA0_CFG_REG);

	if(vbo->gl_vao) {
		if(memcmp(va_cfg, vbo->gl_va_cfgs, MAX_VA_COUNT * 4) == 0) {
			glBindVertexArray(vbo->gl_vao);		// optimal case: no changes
			return;
		}
		glDeleteVertexArrays(1, &vbo->gl_vao);
	} else
		vbo->gl_va_cfgs = malloc(MAX_VA_COUNT * 4);

	memmove(vbo->gl_va_cfgs, va_cfg, MAX_VA_COUNT * 4);

	glGenVertexArrays(1, &vbo->gl_vao);
	glBindVertexArray(vbo->gl_vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo->gl_buffer);

	for(uint32_t i = 0; i < MAX_VA_COUNT; i++, va_cfg++)
		set_va(i, *va_cfg);
}

void gl_bind_attachment(GLenum target, object_t* tbo) {
	if(tbo->header.n_dims != 2) {
		WARN("tbo %llx for attachment %d is not 2D\n", tbo->addr);
		return;
	}

	if(fbo_dims[0] != 0) {
		if(tbo->header.dims[0] != fbo_dims[0] || tbo->header.dims[1] != fbo_dims[1]) {
			WARN("tbo %llx does not match existing attachment dimensions\n", tbo->addr);
			return;
		}
	} else {
		fbo_dims[0] = tbo->header.dims[0];
		fbo_dims[1] = tbo->header.dims[1];
	}

	glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_2D, tbo->gl_buffer, 0);
}

void gl_set_draw_buffers(uint8_t bmp) {
	GLenum buffs[MAX_COLOR_ATTACH_COUNT];
	for(uint32_t i = 0; i < MAX_COLOR_ATTACH_COUNT; i++) {
		buffs[i] = GL_NONE;
		if(bmp & (1 << i))
			buffs[i] = GL_COLOR_ATTACHMENT0 + i;
	}
	glDrawBuffers(MAX_COLOR_ATTACH_COUNT, buffs);
}

// construct + bind FBO currently described by command registers
void bind_fbo() {
	if(gl_fbo)
		glDeleteFramebuffers(1, &gl_fbo);

	glGenFramebuffers(1, &gl_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl_fbo);
	fbo_dims[0] = fbo_dims[1] = 0;
	fbo_n_color_attachs = 0;

	uint32_t fb_cfg = *(uint32_t*)(cmd_regs + FB_CFG_REG);
	uint32_t n_color_attachs = fb_cfg & 0xFF;
	uint8_t has_depth_attach = (fb_cfg & ENABLE_DEPTH_ATTACH_BIT) > 0;

	if(n_color_attachs == 0 && !has_depth_attach) {
		glDeleteFramebuffers(1, &gl_fbo);
		gl_fbo = 0;
		return;
	}

	for(uint32_t i = 0; i < n_color_attachs; i++) {
		uint64_t tbo_addr = *(uint64_t*)(cmd_regs + COLOR_ATTACH_0_REG + (i*8));
		object_t* tbo = ref_buffer_precise(tbo_addr, TYPE_TBO, LENGTH_IN_BUFFER);
		if(!tbo) {
			WARN("failed to get tbo %llx for color attachment %d\n", tbo_addr, i);
			glDeleteFramebuffers(1, &gl_fbo);
			gl_fbo = 0;
			return;
		}
		if(!IS_COLOR_FORMAT(tbo->header.tex_format)) {
			WARN("tbo %llx for color attachment is not of color format\n", tbo->addr);
			glDeleteFramebuffers(1, &gl_fbo);
			gl_fbo = 0;
			return;
		}
		gl_bind_attachment(GL_COLOR_ATTACHMENT0 + i, tbo);
	}

	if(has_depth_attach) {
		uint64_t tbo_addr = *(uint64_t*)(cmd_regs + DEPTH_ATTACH_REG);
		object_t* tbo = ref_buffer_precise(tbo_addr, TYPE_TBO, LENGTH_IN_BUFFER);
		if(!tbo) {
			WARN("failed to get tbo %llx for depth attachment\n", tbo_addr);
			glDeleteFramebuffers(1, &gl_fbo);
			gl_fbo = 0;
			return;
		}
		if(IS_COLOR_FORMAT(tbo->header.tex_format)) {
			WARN("tbo %llx for depth attachment is not of depth format\n", tbo->addr);
			glDeleteFramebuffers(1, &gl_fbo);
			gl_fbo = 0;
			return;
		}

		if(IS_DEPTH_FORMAT(tbo->header.tex_format))
			gl_bind_attachment(GL_DEPTH_ATTACHMENT, tbo);
		else if(IS_DEPTH_STENCIL_FORMAT(tbo->header.tex_format))
			gl_bind_attachment(GL_DEPTH_STENCIL_ATTACHMENT, tbo);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE) {
		WARN("framebuffer was incomplete\n");
		glDeleteFramebuffers(1, &gl_fbo);
		gl_fbo = 0;
		return;
	}

	gl_set_draw_buffers((1 << n_color_attachs) - 1);
	fbo_n_color_attachs = n_color_attachs;

	uint32_t x = *(uint32_t*)(cmd_regs + VIEW_ORIGIN_X_REG);
	uint32_t y = *(uint32_t*)(cmd_regs + VIEW_ORIGIN_Y_REG);
	uint32_t w = *(uint32_t*)(cmd_regs + VIEW_SIZE_X_REG);
	uint32_t h = *(uint32_t*)(cmd_regs + VIEW_SIZE_Y_REG);

	glViewport(0, 0, 1, 1);

	if(!w || !h) {
		WARN("viewport dimension was 0, not setting viewport\n");
		glDeleteFramebuffers(1, &gl_fbo);
		gl_fbo = 0;
		return;
	}

	if(x + w > fbo_dims[0] || y + h > fbo_dims[1]) {
		WARN("viewport range exceeds attachment(s), not setting viewport\n");
		glDeleteFramebuffers(1, &gl_fbo);
		gl_fbo = 0;
		return;
	}

	glViewport(x, y, w, h);
}
