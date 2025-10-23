#include "../../defs.h"

GLenum get_tex_gl_target(uint8_t n_dims) {
	switch(n_dims) {
		case 1: return GL_TEXTURE_1D;
		case 2: return GL_TEXTURE_2D;
		case 3: return GL_TEXTURE_3D;
	}
}

uint32_t calc_level_size(uint8_t format, uint8_t n_dims, uint32_t dims[3]) {
	uint32_t n_bytes = GET_FORMAT_BPP(format);
	for(uint32_t i = 0; i < n_dims; i++)
		n_bytes *= dims[i];
	return n_bytes;
}

uint32_t get_tex_level_count(header_t* hdr) {
	if(!hdr->has_mipmaps)
		return 1;

	uint32_t max_dim = hdr->dims[0];
	if(hdr->n_dims >= 2)	max_dim = fmax(max_dim, hdr->dims[1]);
	if(hdr->n_dims == 3)	max_dim = fmax(max_dim, hdr->dims[2]);

	return 1 + log2(max_dim);
}

void get_tex_level_dims(header_t* hdr, uint32_t level, uint32_t dims[3]) {
	dims[0] = hdr->dims[0];
	dims[1] = hdr->dims[1];
	dims[2] = hdr->dims[2];
	for(uint32_t i = 0; i < hdr->n_dims; i++) {
		dims[i] >>= level;
		if(!dims[i])
			dims[i] = 1;
	}
}

uint64_t get_tex_level_offset(header_t* hdr, uint32_t level) {
	uint64_t total_bytes = 0;
	for(uint32_t i = 0; i < level; i++) {
		uint32_t dims[3];
		get_tex_level_dims(hdr, i, dims);
		total_bytes += calc_level_size(hdr->tex_format, hdr->n_dims, dims);
	}
	return total_bytes;
}

uint64_t get_tex_data_size(header_t* hdr) {
	if(!hdr->has_mipmaps)
		return calc_level_size(hdr->tex_format, hdr->n_dims, hdr->dims);

	uint64_t total_bytes = 0;
	uint32_t n_levels = get_tex_level_count(hdr);
	for(uint32_t i = 0; i < n_levels; i++) {
		uint32_t dims[3];
		get_tex_level_dims(hdr, i, dims);
		total_bytes += calc_level_size(hdr->tex_format, hdr->n_dims, dims);
	}
	return total_bytes;
}

void upload_level(object_t* obj, uint32_t level, uint8_t* src) {
	header_t* hdr = &obj->header;

	uint32_t bpp = GET_FORMAT_BPP(hdr->tex_format);
	uint32_t align = bpp < 4 ? bpp : 4;
	glPixelStorei(GL_UNPACK_ALIGNMENT, align);

	GLenum gl_intl_fmt	= GET_FORMAT_GL_INTERNAL_FORMAT(hdr->tex_format);
	GLenum gl_fmt		= GET_FORMAT_GL_FORMAT(hdr->tex_format);
	GLenum gl_type		= GET_FORMAT_GL_TYPE(hdr->tex_format);

	GLenum target = get_tex_gl_target(hdr->n_dims);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, obj->gl_buffer);

	uint32_t dims[3];
	get_tex_level_dims(hdr, level, dims);
	switch(target) {
		case GL_TEXTURE_1D:
			glTexImage1D(target, level, gl_intl_fmt, dims[0], 0, gl_fmt, gl_type, src);
			break;
		case GL_TEXTURE_2D:
			glTexImage2D(target, level, gl_intl_fmt, dims[0], dims[1], 0, gl_fmt, gl_type, src);
			break;
		case GL_TEXTURE_3D:
			glTexImage3D(target, level, gl_intl_fmt, dims[0], dims[1], dims[2], 0, gl_fmt, gl_type, src);
			break;
	}
}

void download_level(object_t* obj, uint32_t level, uint8_t* dst) {
	header_t* hdr = &obj->header;

	uint32_t bpp = GET_FORMAT_BPP(hdr->tex_format);
	uint32_t align = bpp < 4 ? bpp : 4;
	glPixelStorei(GL_PACK_ALIGNMENT, align);

	GLenum gl_fmt	= GET_FORMAT_GL_FORMAT(hdr->tex_format);
	GLenum gl_type	= GET_FORMAT_GL_TYPE(hdr->tex_format);

	GLenum target = get_tex_gl_target(hdr->n_dims);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, obj->gl_buffer);
	glGetTexImage(target, level, gl_fmt, gl_type, dst);
}

void upload_texture(object_t* obj, uint8_t* data) {
	GLenum target = get_tex_gl_target(obj->header.n_dims);

	uint32_t level_count = get_tex_level_count(&obj->header);
	for(uint32_t i = 0; i < level_count; i++) {
		uint64_t offset = get_tex_level_offset(&obj->header, i);
		upload_level(obj, i, data + get_header_length(TYPE_TBO) + offset);
	}
}

void rw_texture(uint8_t is_read, object_t* obj, uint8_t* data, uint64_t addr, uint64_t n) {
	header_t* hdr = &obj->header;
	uint8_t fmt = hdr->tex_format;
	uint32_t bpp = GET_FORMAT_BPP(fmt);
	uint64_t offset = addr - obj->addr - get_header_length(TYPE_TBO);

	uint32_t level_count = get_tex_level_count(hdr);
	uint32_t first_level;
	for(int32_t i = level_count - 1; i >= 0; i--) {
		if(get_tex_level_offset(hdr, i) > offset)
			break;
		first_level = i;
	}

	uint8_t* tmp_data = malloc(calc_level_size(hdr->tex_format, hdr->n_dims, hdr->dims));

	for(uint64_t curr_level = first_level; n; curr_level++) {
		uint32_t level_dims[3];
		get_tex_level_dims(hdr, curr_level, level_dims);

		uint64_t level_size = calc_level_size(hdr->tex_format, hdr->n_dims, level_dims);
		uint64_t level_start = get_tex_level_offset(hdr, curr_level);
		uint64_t level_end = level_start + level_size - 1;

		uint64_t bytes_to_access = level_end - offset + 1;
		bytes_to_access = bytes_to_access < n ? bytes_to_access : n;

		if(is_read) {
			download_level(obj, curr_level, tmp_data);
			memcpy(data, tmp_data + (offset - level_start), bytes_to_access);
		} else {
			download_level(obj, curr_level, tmp_data);
			memcpy(tmp_data + (offset - level_start), data, bytes_to_access);
			upload_level(obj, curr_level, tmp_data);
		}

		n -= bytes_to_access;
		data += bytes_to_access;
		offset += bytes_to_access;
	}

	free(tmp_data);
}

void read_texture(object_t* obj, uint8_t* dst, uint64_t src, uint64_t n) {
	rw_texture(1, obj, dst, src, n);
}

void write_texture(object_t* obj, uint64_t dst, uint8_t* src, uint64_t n) {
	rw_texture(0, obj, src, dst, n);
}
