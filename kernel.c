#include "../../defs.h"

kernel_info_t* bound_kernel;

node_t* get_accesses() {
	return bound_kernel ? bound_kernel->desc_accesses : 0;
}

uint32_t get_accessed_dtables() {
	return bound_kernel ? bound_kernel->table_accesses : 0;
}

GLuint get_gl_program() {
	return bound_kernel ? bound_kernel->gl_program : 0;
}

void add_to_list(node_t** list, void* data) {
	if(!list)
		ERROR("add_to_list(): invalid pointer\n");
	node_t* node = malloc(sizeof(node_t));
	node->data = data;
	node->next = *list;
	*list = node;
}

void remove_from_list(node_t** list, node_t* to_remove) {
	if(!list)
		ERROR("remove_from_list(): invalid pointer\n");
	node_t* prev = 0;
	for(node_t* node = *list; node; node = node->next) {
		if(node != to_remove) {
			prev = node;
			continue;
		}

		if(!prev)
			*list = node->next;
		else
			prev->next = node->next;

		free(node->data);
		free(node);
		return;
	}
}

void free_list(node_t* node) {
	if(!node)
		return;
	node_t* next = node->next;
	free(node->data);
	free(node);
	free_list(next);
}

void free_kernel(object_t* obj) {
	kernel_info_t* info = obj->kernel_info;
	glDeleteProgram(info->gl_program);
	glDeleteBuffers(1, &info->gl_uregs_ubo);
	free_list(info->desc_accesses);
	free(info);
}

desc_access_t* get_desc_in_list(node_t* desc_list, uint16_t table, uint16_t index) {
	for(node_t* node = desc_list; node; node = node->next) {
		desc_access_t* d = node->data;
		if(d->table == table && d->index == index)
			return d;
	}
	return 0;
}

uint8_t ref_attrib(node_t** attrib_list, uint32_t stage_id, uint8_t attr_type,
	uint16_t id, uint8_t interp_type, uint8_t comp_type, uint8_t comp_count,
	uint8_t comp) {
	for(node_t* node = *attrib_list; node; node = node->next) {
		attrib_access_t* a = node->data;

		if(stage_id == 1 && attr_type == ATTR_IN && a->id == id)
			interp_type = a->interp_type;	// inherit vtx shader's interp_type

		if(a->id != id || a->stage_id != stage_id)
			continue;

		if(a->attr_type != attr_type || a->comp_type != comp_type
		|| comp >= a->comp_count || a->interp_type != interp_type)
			return 0;

		return 1;
	}

	if(comp_type > 2 || interp_type > 2 || (comp_type != 0 && interp_type != 0))
		return 0;

	attrib_access_t* a = malloc(sizeof(attrib_access_t));
	a->attr_type = attr_type;
	a->stage_id = stage_id;
	a->interp_type = interp_type;
	a->comp_type = comp_type;
	a->comp_count = comp_count;
	a->id = id;
	add_to_list(attrib_list, a);
	return 1;
}

void add_code(code_t* code, char* str) {
	if(!str)
		return;
	uint32_t extend_len = strlen(str) + 1;
	code->str = realloc(code->str, code->len + extend_len);
	memset(code->str + code->len, 0, extend_len);
	strcat(code->str, str);
	code->len += extend_len;
}

void add_code_int(code_t* code, int64_t value) {
	char str[32];
	snprintf(str, 32, "%lld", value);
	add_code(code, str);
}

void add_code_uint(code_t* code, uint64_t value) {
	add_code_int(code, value);
	add_code(code, "u");
}

void add_code_reg(code_t* code, uint8_t reg) {
	add_code(code, "regs[");
	add_code_int(code, reg);
	add_code(code, "]");
}

void add_code_ureg(code_t* code, uint8_t reg) {
	add_code(code, "u_regs[");
	add_code_int(code, reg / 4);
	add_code(code, "][");
	add_code_int(code, reg % 4);
	add_code(code, "]");
}

void add_code_buffer_ref(code_t* code, uint16_t table, uint16_t index) {
	add_code(code, "buffer");
	add_code_int(code, table);
	add_code_int(code, index);
	add_code(code, "_data[");
}

void add_code_buffer_imm_idx(code_t* code, uint32_t imm) {
	add_code_int(code, imm / 16);
	add_code(code, "][");
	add_code_int(code, imm / 4 % 4);
}

void add_code_buffer_reg_idx(code_t* code, uint8_t reg) {
	add_code(code, "floatBitsToUint(");
	add_code_reg(code, reg);
	add_code(code, ") / 16");
	add_code(code, "][floatBitsToUint(");
	add_code_reg(code, reg);
	add_code(code, ") / 4 % 4");
}

uint8_t add_code_array_imm_idx(code_t* code, uint32_t imm) {
	add_code_int(code, imm / 4);
}

void add_code_array_reg_idx(code_t* code, uint8_t reg) {
	add_code(code, "floatBitsToUint(");
	add_code_reg(code, reg);
	add_code(code, ") / 4");
}

void add_code_tex_ref(code_t* code, uint16_t table, uint16_t index) {
	add_code(code, "tex");
	add_code_int(code, table);
	add_code_int(code, index);
}

void add_code_attrib_ref(code_t* code, uint16_t id, uint8_t comp) {
	add_code(code, "attrib");
	add_code_int(code, id);
	char str[3] = { '.', "xyzw"[comp], '\0' };
	add_code(code, str);
}

uint64_t read_field(uint8_t* ptr, field_t field) {
	if(field.bit_count == 0 || field.bit_count > 64)
		ERROR("read_field(): field width is invalid\n");

	uint64_t value = 0;
	uint8_t tmp[9] = {0};
	uint32_t bit_end = field.bit_start + field.bit_count - 1;
	uint32_t n_bytes = (bit_end / 8) - (field.bit_start / 8) + 1;

	ptr += field.bit_start / 8;

	for(uint32_t i = 0; i < n_bytes; i++) {
		uint32_t s = field.bit_start % 8;

		tmp[i] |= ptr[i] >> s;
		if(i + 1 < n_bytes)
			tmp[i] |= ptr[i+1] << (8-s);
	}

	memcpy(&value, tmp, 8);

	value &= field.bit_count == 64 ? -1 : (1ull << field.bit_count) - 1;
	return value;
}

uint8_t decode_ins(kernel_info_t* info, stage_t* stage, uint32_t offset, uint8_t* ptr) {
	code_t* code = &stage->code;

	uint8_t op = (*ptr) & 0x7F; // op = low 7 bits of instruction
	if(op >= N_OPS) {
		WARN("invalid opcode\n");
		return 0;
	}

	ins_t* ins = &ins_list[op];
	field_t last_field = ins->fields[ins->field_count - 1];

	uint32_t ins_width =
		(last_field.bit_start + last_field.bit_count + 7) / 8;
	ins_width += ins_width % 2;	// round up to multiple of two

	if(offset + ins_width - 1 >= stage->len) {
		WARN("instruction width is out of bounds\n");
		return 0;
	}

#define F(x) read_field(ptr, ins->fields[x])

	if(op == OP_MOV) {
		uint64_t imm = F(2), dst = F(1);

		add_code_reg(code, dst);
		add_code(code, " = uintBitsToFloat(");
		add_code_uint(code, imm);
		add_code(code, ");\n");
	}

	if(op == OP_ULD) {
		uint64_t src = F(2), dst = F(1);

		add_code_reg(code, dst);
		add_code(code, " = ");
		add_code_ureg(code, src);
		add_code(code, ";\n");
	}

	if(op == OP_LD) {
		uint64_t src = F(4), st = F(3), si = F(2), dst = F(1);

		add_code_reg(code, dst);
		add_code(code, " = ");

		if(st == 0) {
			uint16_t table = src >> 48;
			uint16_t index = (src >> 32) & 0xFFFF;

			if(!get_desc_in_list(info->desc_accesses, table, index)) {
				WARN("buffer referenced not in kernel binary's list of buffers\n");
				return 0;
			}
			add_code_buffer_ref(code, table, index);
			if(si)	add_code_buffer_imm_idx(code, src & 0xFFFFFFFF);
			else	add_code_buffer_reg_idx(code, src & 0xFF);
			add_code(code, "];\n");
		} else if(st == 1) {
			uint8_t comp_type	= src & 0x3;
			uint8_t comp_count	= ((src >> 2) & 0x3) + 1;
			uint8_t comp		= (src >> 4) & 0x3;
			uint16_t id			= (src >> 6) & 0xFFFF;

			if(!ref_attrib(&info->attrib_accesses, stage->id, ATTR_IN, id,
				0, comp_type, comp_count, comp)) {
				WARN("attribute access invalid\n");
				return 0;
			}
			if(comp_type == 1)	add_code(code, "intBitsToFloat(");
			if(comp_type == 2)	add_code(code, "uintBitsToFloat(");
			add_code_attrib_ref(code, id, comp);
			if(comp_type != 0)
				add_code(code, ")");
			add_code(code, ";\n");
		} else if(st == 2) {
			add_code(code, "local_mem[");
			if(si)	add_code_array_imm_idx(code, src & 0xFFFFFFFF);
			else	add_code_array_reg_idx(code, src & 0xFF);
			add_code(code, "];\n");
		} else {
			WARN("invalid source type field\n");
			return 0;
		}
	}

	if(op == OP_STR) {
		uint64_t src = F(4), dst = F(3), dt = F(2), di = F(1);

		if(dt == 0) {
			uint16_t table = dst >> 48;
			uint16_t index = (dst >> 32) & 0xFFFF;

			if(!get_desc_in_list(info->desc_accesses, table, index)) {
				WARN("buffer referenced not in kernel binary's list of buffers\n");
				return 0;
			}
			add_code_buffer_ref(code, table, index);
			if(di)	add_code_buffer_imm_idx(code, dst & 0xFFFFFFFF);
			else	add_code_buffer_reg_idx(code, dst & 0xFF);
			add_code(code, "] = ");
			add_code_reg(code, src & 0xFF);
		} else if(dt == 1) {
			uint8_t interp_type = stage->id == 0 ? dst & 0x3 : 0;
			uint8_t comp_type	= (dst >> 2) & 0x3;
			uint8_t comp_count	= ((dst >> 4) & 0x3) + 1;
			uint8_t comp		= (dst >> 6) & 0x3;
			uint16_t id			= (dst >> 8) & 0xFFFF;

			if(!ref_attrib(&info->attrib_accesses, stage->id, ATTR_OUT, id,
				interp_type, comp_type, comp_count, comp)) {
				WARN("attribute access invalid\n");
				return 0;
			}
			add_code_attrib_ref(code, id, comp);
			add_code(code, " = ");
			if(comp_type == 1)	add_code(code, "floatBitsToInt(");
			if(comp_type == 2)	add_code(code, "floatBitsToUint(");
			add_code_reg(code, src & 0xFF);
			if(comp_type != 0)
				add_code(code, ")");
		} else if(dt == 2) {
			add_code(code, "local_mem[");
			if(di)	add_code_array_imm_idx(code, dst & 0xFFFFFFFF);
			else	add_code_array_reg_idx(code, dst & 0xFF);
			add_code(code, "] = ");
			add_code_reg(code, src & 0xFF);
		} else {
			WARN("invalid destination type field\n");
			return 0;
		}
		add_code(code, ";\n");
	}

	if(op == OP_TEX) {
		uint64_t tc = F(3), src = F(2), dst = F(1);

		uint16_t table = (src >> 20) & 0xFFFF;
		uint16_t index = (src >> 4) & 0xFFFF;
		uint32_t sample_type = (src >> 2) & 0x3;
		uint32_t n_dims = src & 0x3;

		if(n_dims == 0) {
			WARN("invalid dimension count field\n");
			return 0;
		}

		if(sample_type > 2) {
			WARN("invalid sample type field\n");
			return 0;
		}

		desc_access_t* d = get_desc_in_list(info->desc_accesses, table, index);
		if(!d) {
			if(info->n_tmus_occupied >= MAX_TBO_COUNT) {
				WARN("too many textures accessed in kernel\n");
				return 0;
			}

			d = malloc(sizeof(desc_access_t));
			d->type = TYPE_TBO;
			d->table = table;
			d->index = index;
			d->n_dims = n_dims;
			d->sample_type = sample_type;

			add_to_list(&info->desc_accesses, d);
			info->table_accesses |= 1 << table;
			d->bind_point.tmu = ++info->n_tmus_occupied;
		} else if(d->type != TYPE_TBO || d->n_dims != n_dims
			|| d->sample_type != sample_type) {
			WARN("sampled incompatible descriptor\n");
			return 0;
		}

		switch(d->sample_type) {
			case 0: add_code(code, "vec4 t");  break;
			case 1: add_code(code, "ivec4 t"); break;
			case 2: add_code(code, "uvec4 t"); break;
		}
		add_code_int(code, info->n_sampling_calls);
		add_code(code, " = texture(");
		add_code_tex_ref(code, table, index);
		add_code(code, ", ");
		switch(d->n_dims) {
			case 1: add_code(code, "float("); break;
			case 2: add_code(code, "vec2("); break;
			case 3: add_code(code, "vec3("); break;
		}
		for(uint32_t i = 0; i < d->n_dims; i++) {
			add_code_reg(code, (tc >> (i*8)) & 0xFF);
			if(i + 1 < d->n_dims)
				add_code(code, ", ");
		}
		add_code(code, "));\n");

		for(uint32_t i = 0; i < 4; i++) {
			add_code_reg(code, (dst >> (i*8)) & 0xFF);
			add_code(code, " = ");
			if(d->sample_type == 1)	add_code(code, "intBitsToFloat(");
			if(d->sample_type == 2)	add_code(code, "uintBitsToFloat(");
			add_code(code, "t");
			add_code_int(code, info->n_sampling_calls);
			char component[] = { '.', "rgba"[i], '\0' };
			add_code(code, component);
			if(d->sample_type != 0)
				add_code(code, ")");
			add_code(code, ";\n");
		}

		info->n_sampling_calls++;
	}

	if(op == OP_VOUT) {
		uint64_t src = F(1);

		add_code(code, "gl_Position = vec4(");
		for(uint32_t i = 0; i < 4; i++) {
			add_code_reg(code, (src >> (i*8)) & 0xFF);
			if(i + 1 < 4)
				add_code(code, ", ");
		}
		add_code(code, ");\n");
	}

	return ins_width;
}

void define_globals(kernel_info_t* info, stage_t* stage) {
	code_t* globals = &stage->globals;

	for(node_t* node = info->attrib_accesses; node; node = node->next) {
		attrib_access_t* a = node->data;

		if(a->stage_id != stage->id)
			continue;

		if((stage->id == 0 && a->attr_type == ATTR_IN)
		|| (stage->id == 1 && a->attr_type == ATTR_OUT)) {
			add_code(globals, "layout(location = ");
			add_code_int(globals, a->id);
			add_code(globals, ") ");
		}

		if((stage->id == 0 && a->attr_type == ATTR_OUT)
		|| (stage->id == 1 && a->attr_type == ATTR_IN))
			switch(a->interp_type) {
				case 0: add_code(globals, "flat ");   break;
				case 1: add_code(globals, "smooth "); break;
				case 2: add_code(globals, "noperspective "); break;
			}

		if(a->attr_type == ATTR_IN)
			add_code(globals, "in ");
		if(a->attr_type == ATTR_OUT)
			add_code(globals, "out ");

		if(a->comp_count > 1 && a->comp_type != 0)
			add_code(globals, a->comp_type == 1 ? "i" : "u");
		switch(a->comp_count) {
			case 1:
				switch(a->comp_type) {
					case 0: add_code(globals, "float "); break;
					case 1: add_code(globals, "int ");   break;
					case 2: add_code(globals, "uint ");  break;
				}
				break;
			case 2: add_code(globals, "vec2 "); break;
			case 3: add_code(globals, "vec3 "); break;
			case 4: add_code(globals, "vec4 "); break;
		}

		add_code(globals, "attrib");
		add_code_int(globals, a->id);
		add_code(globals, ";\n");
	}

	for(node_t* node = info->desc_accesses; node; node = node->next) {
		desc_access_t* d = node->data;
		add_code(globals, "uniform ");
		if(d->type == TYPE_TBO) {
			if(d->sample_type == 1)	add_code(globals, "i");
			if(d->sample_type == 2)	add_code(globals, "u");
			switch(d->n_dims) {
				case 1: add_code(globals, "sampler1D "); break;
				case 2: add_code(globals, "sampler2D "); break;
				case 3: add_code(globals, "sampler3D "); break;
			}
			add_code_tex_ref(globals, d->table, d->index);
			add_code(globals, ";\n");
		} else if(d->type == TYPE_UBO) {
			add_code(globals, "buffer");
			add_code_int(globals, d->table);
			add_code_int(globals, d->index);
			add_code(globals, " {");

			add_code(globals, "vec4 buffer");
			add_code_int(globals, d->table);
			add_code_int(globals, d->index);
			add_code(globals, "_data[");
			add_code_int(globals, d->buffer_size / 16);
			add_code(globals, "];\n};\n");
		}
	}
}

uint8_t build_stage(kernel_info_t* info, stage_t* stage, uint8_t* src) {
	add_code(&stage->globals,
		"#version 330 core\n"
	);
	if(info->local_mem_size) {
		add_code(&stage->globals, "float local_mem[");
		add_code_int(&stage->globals, info->local_mem_size / 4);
		add_code(&stage->globals, "];\n");
	}
	add_code(&stage->globals,
		"uniform ureg_buffer { vec4 u_regs[8]; };\n"
	);

	add_code(&stage->code,
		"void main() {\n"
		"float regs[256];\n"
	);

	uint32_t offs = 0;
	while(offs < stage->len) {
		uint32_t ins_len = decode_ins(info, stage, offs, src + offs);
		if(!ins_len) {
			WARN("decoding instruction failed\n");
			return 1;
		}
		offs += ins_len;
	}

	add_code(&stage->code, "}\n");

	define_globals(info, stage);
	return 0;
}

void free_stage(stage_t* stage) {
	free(stage->code.str);
	free(stage->globals.str);
}

GLuint build_program(stage_t* v_stage, stage_t* f_stage) {
	code_t vsh_code;
	memset(&vsh_code, 0, sizeof(code_t));
	add_code(&vsh_code, v_stage->globals.str);
	add_code(&vsh_code, v_stage->code.str);

	GLuint gl_vsh = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(gl_vsh, 1, &vsh_code.str, 0);
	glCompileShader(gl_vsh);
	GLint status = 0;
	glGetShaderiv(gl_vsh, GL_COMPILE_STATUS, &status);
	if(!status) {
		WARN("failed to compile vertex shader\n");
		glDeleteShader(gl_vsh);
		free(vsh_code.str);
		return 0;
	}

	code_t fsh_code;
	memset(&fsh_code, 0, sizeof(code_t));
	add_code(&fsh_code, f_stage->globals.str);
	add_code(&fsh_code, f_stage->code.str);

	GLuint gl_fsh = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(gl_fsh, 1, &fsh_code.str, 0);
	glCompileShader(gl_fsh);
	glGetShaderiv(gl_fsh, GL_COMPILE_STATUS, &status);
	if(!status) {
		WARN("failed to compile fragment shader\n");
		glDeleteShader(gl_vsh);
		glDeleteShader(gl_fsh);
		free(vsh_code.str);
		free(fsh_code.str);
		return 0;
	}

	GLuint gl_program = glCreateProgram();
	glAttachShader(gl_program, gl_vsh);
	glAttachShader(gl_program, gl_fsh);
	glLinkProgram(gl_program);
	glGetProgramiv(gl_program, GL_LINK_STATUS, &status);

	glDetachShader(gl_program, gl_vsh);
	glDetachShader(gl_program, gl_fsh);
	glDeleteShader(gl_vsh);
	glDeleteShader(gl_fsh);

	free(vsh_code.str);
	free(fsh_code.str);

	if(!status) {
		WARN("failed to link program\n");
		glDeleteProgram(gl_program);
		return 0;
	}

	return gl_program;
}

void build_kernel(object_t* obj) {
	kernel_info_t info;
	memset(&info, 0, sizeof(kernel_info_t));
	info.kernel_len = obj->header.kernel_len;

	uint8_t* data = malloc(info.kernel_len);
	data = gpu_read(data, obj->addr + obj->header_len, info.kernel_len);

	if(info.kernel_len < 20) {
		WARN("kernel length is too small\n");
		return;
	}

	uint32_t n_stages		= *(uint32_t*)data;
	uint32_t* stage_len		= data + 4;
	info.local_mem_size		= *(uint32_t*)(data + 12);
	uint32_t n_buffers		= *(uint32_t*)(data + 16);

	if(info.local_mem_size % 4) {
		WARN("kernel local memory size must be a multiple of 4\n");
		free(data);
		return;
	}

	if(n_stages != 2) {
		WARN("kernel stage count must be 2\n");
		free(data);
		return;
	}

	if(n_buffers > MAX_UBO_COUNT) {
		WARN("too many read-only buffers in kernel\n");
		free(data);
		return;
	}

	// process read-only (uniform) buffer descriptions
	for(uint32_t i = 0; i < n_buffers; i++) {
		desc_access_t* d = malloc(sizeof(desc_access_t));

		uint64_t buffer_info = *(uint64_t*)(data + 20 + i*8);
		d->type  = TYPE_UBO;
		d->table = buffer_info >> 48;
		d->index = (buffer_info >> 32) & 0xFFFF;
		d->buffer_size = buffer_info & 0xFFFFFFFF;

		d->bind_point.ubo_binding = i + 1;	// 0 is reserved for uregs_ubo

		if(get_desc_in_list(info.desc_accesses, d->table, d->index)) {
			WARN("duplicate read-only buffer description in kernel binary\n");
			free_list(info.desc_accesses);
			free(data);
			free(d);
			return;
		}

		if(d->buffer_size == 0 || d->buffer_size % 16
		|| d->buffer_size > MAX_UBO_SIZE) {
			WARN("invalid read-only buffer size in kernel binary\n");
			free_list(info.desc_accesses);
			free(data);
			free(d);
			return;
		}

		add_to_list(&info.desc_accesses, d);
		info.table_accesses |= 1 << d->table;
	}

	// process kernel binary and generate GLSL
	stage_t stages[2];
	memset(stages, 0, sizeof(stage_t) * 2);
	uint64_t offset = 20 + n_buffers*8;
	for(uint32_t i = 0; i < n_stages; i++) {
		stage_t* stage = &stages[i];
		stage->id = i;
		stage->len = stage_len[i];

		uint8_t error = offset + stage->len - 1 >= info.kernel_len;
		if(error)
			WARN("stage %d range out of kernel bounds\n", i);
		else if(error = build_stage(&info, stage, data + offset))
			WARN("stage %d failed to build\n", i);

		if(error) {
			free_stage(&stages[0]);
			free_stage(&stages[1]);
			free_list(info.desc_accesses);
			free_list(info.attrib_accesses);
			free(data);
			return;
		}

		offset += stage->len;
	}

	info.gl_program = build_program(&stages[0], &stages[1]);

	free_stage(&stages[0]);
	free_stage(&stages[1]);
	free_list(info.attrib_accesses);
	info.attrib_accesses = 0;
	free(data);

	if(!info.gl_program) {
		WARN("failed to build program\n");
		free_list(info.desc_accesses);
		return;
	}

	// set bind_point of each descriptor to GL-assigned location
	for(node_t* node = info.desc_accesses, tmp; node; node = node->next) {
		desc_access_t* d = node->data;

		code_t name;
		memset(&name, 0, sizeof(code_t));
		if(d->type == TYPE_TBO) {
			add_code_tex_ref(&name, d->table, d->index);

			GLint loc = glGetUniformLocation(info.gl_program, name.str);
			if(loc == -1) {	// shouldn't happen, but play it safe
				tmp.next = node->next;
				remove_from_list(&info.desc_accesses, node);
				node = &tmp;
			}

			d->bind_point.location = loc;
		} else if(d->type == TYPE_UBO) {
			add_code(&name, "buffer");
			add_code_int(&name, d->table);
			add_code_int(&name, d->index);

			GLuint idx = glGetUniformBlockIndex(info.gl_program, name.str);
			if(idx == GL_INVALID_INDEX) { // may happen if declared but not used
				tmp.next = node->next;
				remove_from_list(&info.desc_accesses, node);
				node = &tmp;
			}

			d->bind_point.location = idx;
		}
		free(name.str);
	}

	glGenBuffers(1, &info.gl_uregs_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, info.gl_uregs_ubo);
	glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);

	obj->kernel_info = malloc(sizeof(kernel_info_t));
	memcpy(obj->kernel_info, &info, sizeof(kernel_info_t));
}

void bind_kernel() {
	uint64_t kernel_addr = *(uint64_t*)(cmd_regs + KERNEL_ADDR_REG);

	object_t* obj = ref_buffer_precise(kernel_addr, TYPE_KERNEL, LENGTH_IN_BUFFER);
	if(!obj) {
		WARN("failed to get kernel object at %llx\n", kernel_addr);
		return;
	}

	if(obj->kernel_info == 0)
		build_kernel(obj);

	bound_kernel = obj->kernel_info;

	if(bound_kernel) {
		glUseProgram(bound_kernel->gl_program);

		load_uregs();

		GLuint idx = glGetUniformBlockIndex(bound_kernel->gl_program, "uregs_ubo");
		if(idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(bound_kernel->gl_program, idx, 0);
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, bound_kernel->gl_uregs_ubo);
		}
	}
}

void load_uregs() {
	if(!bound_kernel)
		return;
	uint8_t* data = malloc(128);
	memcpy(data, cmd_regs + UNIFORM_0_REG, 128);
	glBindBuffer(GL_UNIFORM_BUFFER, bound_kernel->gl_uregs_ubo);
	glBufferData(GL_UNIFORM_BUFFER, 128, data, GL_STATIC_DRAW);
	free(data);
}
