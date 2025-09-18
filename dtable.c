#include "defs.h"

void load_dtable(uint32_t dtbl_slot, node_t* accesses) {
	uint64_t dtbl_addr = *(uint64_t*)(cmd_regs + DTBL_0_ADDR_REG + (dtbl_slot * 8));

	object_t* dtbl = ref_buffer_precise(dtbl_addr, TYPE_DTBL, LENGTH_IN_BUFFER);
	if(!dtbl) {
		WARN("failed to get descriptor table object at %llx\n", dtbl_addr);
		return;
	}

	uint8_t* data = malloc(dtbl->len);
	data = gpu_read(data, dtbl->addr, dtbl->len);

	// validate + load all descriptors accessed from the table
	for(node_t* node = accesses; node; node = node->next) {
		desc_access_t* d = node->data;
		if(d->table != dtbl_slot)
			continue;

		if(d->index >= dtbl->header.n_descriptors) {
			WARN("shader accessed descriptor non-existent in table #%d\n", dtbl_slot);
			return;
		}

		uint64_t mdata	= *(uint64_t*)(data + 2 + (d->index * 16));
		uint64_t addr	= *(uint64_t*)(data + 2 + (d->index * 16) + 8);

		// reference the object as type d->type
		object_t* obj = 0;
		if(d->type == TYPE_TBO)
			obj = ref_buffer_precise(addr, TYPE_TBO, LENGTH_IN_BUFFER);
		if(d->type == TYPE_UBO) {
			if(mdata == 0 || mdata % 16 || mdata > MAX_UBO_SIZE) {
				WARN("invalid read-only buffer size for descriptor in table #%d\n", dtbl_slot);
				return;
			}
			obj = ref_buffer_precise(addr, TYPE_UBO, mdata);
		}

		if(!obj) {
			WARN("failed to reference object for descriptor access in table #%d\n", dtbl_slot);
			return;
		}

		if(d->type != obj->type) {
			WARN("descriptor access type did not match referenced object in table #%d\n", dtbl_slot);
			return;
		}

		// bind obj->gl_buffer using info recorded in d->bind_point
		if(obj->type == TYPE_TBO) {
			GLenum target = get_tex_gl_target(obj->header.n_dims);

			glActiveTexture(GL_TEXTURE0 + d->bind_point.tmu);
			glBindTexture(target, obj->gl_buffer);

			GLenum min_filter;
			GLenum mag_filter = mdata & 0x8 ? GL_LINEAR : GL_NEAREST;
			switch(mdata & 0x7) {
				case 0: min_filter = GL_NEAREST;	break;
				case 1: min_filter = GL_LINEAR;		break;
				case 2: min_filter = GL_NEAREST_MIPMAP_NEAREST;	break;
				case 3: min_filter = GL_LINEAR_MIPMAP_NEAREST;	break;
				case 4: min_filter = GL_NEAREST_MIPMAP_LINEAR;	break;
				case 5: min_filter = GL_LINEAR_MIPMAP_LINEAR;	break;
				default:
					WARN("minify filter was invalid\n");
					return;
			}
			if((mdata & 0x7) >= 2 && !obj->header.has_mipmaps) {
				WARN("mipmapped minify filter used with non-mipmapped texture\n");
				return;
			}
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
			GLenum params[] = {
				GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R
			};
			for(uint32_t i = 0; i < obj->header.n_dims; i++) {
				GLenum mode;
				switch((mdata >> (4 + i*2)) & 0x3) {
					case 0: mode = GL_REPEAT;			break;
					case 1: mode = GL_MIRRORED_REPEAT;	break;
					case 2: mode = GL_CLAMP_TO_EDGE;	break;
					default:
						WARN("wrap mode was invalid\n");
						return;
				}
				glTexParameteri(target, params[i], mode);
			}

			uint32_t max_aniso = 1 << ((mdata >> 10) & 0x7);
			if(max_aniso > 16) {
				WARN("anisotropic filtering was set higher than x16\n");
				return;
			}
			if(max_aniso > 1)
				WARN("anisotropic filtering is not yet implemented!\n");

			glUniform1i(d->bind_point.location, d->bind_point.tmu);
		}
		if(obj->type == TYPE_UBO) {
			glUniformBlockBinding(get_gl_program(), d->bind_point.location,
				d->bind_point.ubo_binding);
			glBindBufferBase(GL_UNIFORM_BUFFER, d->bind_point.ubo_binding,
				obj->gl_buffer);
		}
	}

	free(data);
}

void bind_dtables() {
	uint32_t accessed_dtables = get_accessed_dtables();
	for(uint32_t i = 0; i < MAX_DTABLE_COUNT; i++) {
		if(accessed_dtables & (1 << i))
			load_dtable(i, get_accesses());
	}
}
