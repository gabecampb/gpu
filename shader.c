#include "defs.h"

GLuint program;

desc_access_t* get_accesses() {
	return NULL;	// placeholder
}

uint32_t get_accessed_dtables() {
	return 0;		// placeholder
}

void bind_program() {
	if(program) {
		glUseProgram(program);
		return;
	}

	const char* vsh =
		"#version 330\n"
		"layout(location = 0) in vec3 pos;\n"
		"void main() {\n"
		"	gl_Position = vec4(pos.x, -pos.y, pos.z, 1);\n"
		"}";

	const char* fsh =
		"#version 330\n"
		"layout(location = 0) out vec4 final;\n"
		"void main() {\n"
		"	final = vec4(1,1,1,1);\n"
		"}";

	GLuint gl_vsh = glCreateShader(GL_VERTEX_SHADER);
	GLuint gl_fsh = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(gl_vsh, 1, &vsh, 0);
	glShaderSource(gl_fsh, 1, &fsh, 0);
	glCompileShader(gl_vsh);
	GLint status = 0;
	glGetShaderiv(gl_vsh, GL_COMPILE_STATUS, &status);
	if(!status) {
		LOG("failed to compile vertex shader\n");
		exit(1);
	}
	glCompileShader(gl_fsh);
	glGetShaderiv(gl_fsh, GL_COMPILE_STATUS, &status);
	if(!status) {
		LOG("failed to compile fragment shader\n");
		exit(1);
	}
	program = glCreateProgram();
	glAttachShader(program, gl_vsh);
	glAttachShader(program, gl_fsh);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if(!status) {
		LOG("failed to link program\n");
		exit(1);
	}
	glDetachShader(program, gl_vsh);
	glDetachShader(program, gl_fsh);

	glUseProgram(program);
}
