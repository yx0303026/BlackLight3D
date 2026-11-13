#pragma once

#include <windows.h>
#include <gl/GL.h>

#include <cstddef>
#include <cstdint>

namespace BlackLight::Engine::Rendering::Detail
{
	using GLChar = char;
	using GLSizePtr = std::ptrdiff_t;

	inline constexpr GLenum GL_ARRAY_BUFFER_VALUE = 0x8892;
	inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_VALUE = 0x8893;
	inline constexpr GLenum GL_STATIC_DRAW_VALUE = 0x88E4;
	inline constexpr GLenum GL_DYNAMIC_DRAW_VALUE = 0x88E8;
	inline constexpr GLenum GL_TEXTURE0_VALUE = 0x84C0;
	inline constexpr GLenum GL_TEXTURE_2D_VALUE = 0x0DE1;
	inline constexpr GLenum GL_TEXTURE_2D_MULTISAMPLE_VALUE = 0x9100;
	inline constexpr GLenum GL_READ_FRAMEBUFFER_VALUE = 0x8CA8;
	inline constexpr GLenum GL_DRAW_FRAMEBUFFER_VALUE = 0x8CA9;
	inline constexpr GLenum GL_COLOR_BUFFER_BIT_VALUE = 0x00004000;
	inline constexpr GLenum GL_NEAREST_FILTER_VALUE = 0x2600;
	inline constexpr GLenum GL_TEXTURE_MIN_FILTER_VALUE = 0x2801;
	inline constexpr GLenum GL_TEXTURE_MAG_FILTER_VALUE = 0x2800;
	inline constexpr GLenum GL_TEXTURE_WRAP_S_VALUE = 0x2802;
	inline constexpr GLenum GL_TEXTURE_WRAP_T_VALUE = 0x2803;
	inline constexpr GLenum GL_CLAMP_TO_EDGE_VALUE = 0x812F;
	inline constexpr GLenum GL_MIRRORED_REPEAT_VALUE = 0x8370;
	inline constexpr GLenum GL_SRGB8_ALPHA8_VALUE = 0x8C43;
	inline constexpr GLenum GL_R8_VALUE = 0x8229;
	inline constexpr GLenum GL_RED_VALUE = 0x1903;
	inline constexpr GLenum GL_RG_VALUE = 0x8227;
	inline constexpr GLenum GL_RG32F_VALUE = 0x8230;
	inline constexpr GLenum GL_RGB32F_VALUE = 0x8815;
	inline constexpr GLenum GL_RGBA32F_VALUE = 0x8814;
	inline constexpr GLenum GL_DEPTH24_STENCIL8_VALUE = 0x88F0;
	inline constexpr GLenum GL_DEPTH_STENCIL_VALUE = 0x84F9;
	inline constexpr GLenum GL_UNSIGNED_INT_24_8_VALUE = 0x84FA;
	inline constexpr GLenum GL_R16UI_VALUE = 0x8234;
	inline constexpr GLenum GL_R32UI_VALUE = 0x8236;
	inline constexpr GLenum GL_RED_INTEGER_VALUE = 0x8D94;
	inline constexpr GLenum GL_UNSIGNED_SHORT_VALUE = 0x1403;
	inline constexpr GLenum GL_UNSIGNED_INT_VALUE = 0x1405;
	inline constexpr GLenum GL_VERTEX_SHADER_VALUE = 0x8B31;
	inline constexpr GLenum GL_FRAGMENT_SHADER_VALUE = 0x8B30;
	inline constexpr GLenum GL_COMPILE_STATUS_VALUE = 0x8B81;
	inline constexpr GLenum GL_LINK_STATUS_VALUE = 0x8B82;
	inline constexpr GLenum GL_INFO_LOG_LENGTH_VALUE = 0x8B84;
	inline constexpr GLenum GL_FRAMEBUFFER_VALUE = 0x8D40;
	inline constexpr GLenum GL_COLOR_ATTACHMENT0_VALUE = 0x8CE0;
	inline constexpr GLenum GL_DEPTH_STENCIL_ATTACHMENT_VALUE = 0x821A;
	inline constexpr GLenum GL_FRAMEBUFFER_COMPLETE_VALUE = 0x8CD5;
	inline constexpr GLenum GL_MAX_TEXTURE_SIZE_VALUE = 0x0D33;
	inline constexpr GLenum GL_MAX_COLOR_ATTACHMENTS_VALUE = 0x8CDF;
	inline constexpr GLenum GL_MAX_SAMPLES_VALUE = 0x8D57;
	inline constexpr GLenum GL_UNPACK_ALIGNMENT_VALUE = 0x0CF5;
	inline constexpr GLenum GL_BLEND_VALUE = 0x0BE2;
	inline constexpr GLenum GL_DEPTH_TEST_VALUE = 0x0B71;
	inline constexpr GLenum GL_CULL_FACE_VALUE = 0x0B44;
	inline constexpr GLenum GL_FRONT_VALUE = 0x0404;
	inline constexpr GLenum GL_BACK_VALUE = 0x0405;
	inline constexpr GLenum GL_SRC_ALPHA_VALUE = 0x0302;
	inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA_VALUE = 0x0303;
	inline constexpr GLenum GL_TRIANGLES_VALUE = 0x0004;
	inline constexpr GLenum GL_LINES_VALUE = 0x0001;
	inline constexpr GLenum GL_POINTS_VALUE = 0x0000;
	inline constexpr GLenum GL_UNIFORM_BUFFER_VALUE = 0x8A11;
	inline constexpr GLenum GL_TEXTURE_MAX_LEVEL_VALUE = 0x813D;
	inline constexpr GLenum GL_TEXTURE_WRAP_R_VALUE = 0x8072;
	inline constexpr GLenum GL_CLAMP_VALUE = 0x2900;
	inline constexpr GLenum GL_REPEAT_VALUE = 0x2901;
	inline constexpr GLenum GL_TEXTURE_COMPARE_MODE_VALUE = 0x884C;
	inline constexpr GLenum GL_BUFFER_SUB_DATA_VALUE = 0x88E8;
	inline constexpr GLenum GL_TIME_ELAPSED_VALUE = 0x88BF;
	inline constexpr GLenum GL_QUERY_RESULT_VALUE = 0x8866;
	inline constexpr GLenum GL_QUERY_RESULT_AVAILABLE_VALUE = 0x8867;

	/// Loads the OpenGL 3.3 entry points used by normalized resources and draws.
	struct OpenGLFunctions
	{
		using GenBuffersProc = void(APIENTRY *)(GLsizei, GLuint *);
		using BindBufferProc = void(APIENTRY *)(GLenum, GLuint);
		using BufferDataProc = void(APIENTRY *)(GLenum, GLSizePtr, const void *, GLenum);
		using DeleteBuffersProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using GenTexturesProc = void(APIENTRY *)(GLsizei, GLuint *);
		using DeleteTexturesProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using BindTextureProc = void(APIENTRY *)(GLenum, GLuint);
		using TexParameteriProc = void(APIENTRY *)(GLenum, GLenum, GLint);
		using TexImage2DProc = void(APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
		using TexImage2DMultisampleProc = void(APIENTRY *)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
		using BlitFramebufferProc = void(APIENTRY *)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
		using PixelStoreiProc = void(APIENTRY *)(GLenum, GLint);
		using GenerateMipmapProc = void(APIENTRY *)(GLenum);
		using GenVertexArraysProc = void(APIENTRY *)(GLsizei, GLuint *);
		using BindVertexArrayProc = void(APIENTRY *)(GLuint);
		using DeleteVertexArraysProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using CreateShaderProc = GLuint(APIENTRY *)(GLenum);
		using ShaderSourceProc = void(APIENTRY *)(GLuint, GLsizei, const GLChar *const *, const GLint *);
		using CompileShaderProc = void(APIENTRY *)(GLuint);
		using GetShaderivProc = void(APIENTRY *)(GLuint, GLenum, GLint *);
		using GetShaderInfoLogProc = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, GLChar *);
		using DeleteShaderProc = void(APIENTRY *)(GLuint);
		using CreateProgramProc = GLuint(APIENTRY *)();
		using AttachShaderProc = void(APIENTRY *)(GLuint, GLuint);
		using LinkProgramProc = void(APIENTRY *)(GLuint);
		using GetProgramivProc = void(APIENTRY *)(GLuint, GLenum, GLint *);
		using GetActiveUniformProc = void(APIENTRY *)(GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLChar *);
		using GetProgramInfoLogProc = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, GLChar *);
		using DeleteProgramProc = void(APIENTRY *)(GLuint);
		using UseProgramProc = void(APIENTRY *)(GLuint);
		using BindAttribLocationProc = void(APIENTRY *)(GLuint, GLuint, const GLChar *);
		using BlendFuncSeparateProc = void(APIENTRY *)(GLenum, GLenum, GLenum, GLenum);
		using GetUniformLocationProc = GLint(APIENTRY *)(GLuint, const GLChar *);
		using Uniform1fProc = void(APIENTRY *)(GLint, GLfloat);
		using Uniform2fvProc = void(APIENTRY *)(GLint, GLsizei, const GLfloat *);
		using Uniform3fvProc = void(APIENTRY *)(GLint, GLsizei, const GLfloat *);
		using Uniform4fvProc = void(APIENTRY *)(GLint, GLsizei, const GLfloat *);
		using UniformMatrix4fvProc = void(APIENTRY *)(GLint, GLsizei, GLboolean, const GLfloat *);
		using Uniform1iProc = void(APIENTRY *)(GLint, GLint);
		using EnableVertexAttribArrayProc = void(APIENTRY *)(GLuint);
		using VertexAttribPointerProc = void(APIENTRY *)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
		using VertexAttribIPointerProc = void(APIENTRY *)(GLuint, GLint, GLenum, GLsizei, const void *);
		using DrawArraysProc = void(APIENTRY *)(GLenum, GLint, GLsizei);
		using DrawElementsProc = void(APIENTRY *)(GLenum, GLsizei, GLenum, const void *);
		using DrawElementsBaseVertexProc = void(APIENTRY *)(GLenum, GLsizei, GLenum, const void *, GLint);
		using ActiveTextureProc = void(APIENTRY *)(GLenum);
		using ClientActiveTextureProc = void(APIENTRY *)(GLenum);
		using GenSamplersProc = void(APIENTRY *)(GLsizei, GLuint *);
		using SamplerParameteriProc = void(APIENTRY *)(GLuint, GLenum, GLint);
		using BindSamplerProc = void(APIENTRY *)(GLuint, GLuint);
		using DeleteSamplersProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using GenFramebuffersProc = void(APIENTRY *)(GLsizei, GLuint *);
		using BindFramebufferProc = void(APIENTRY *)(GLenum, GLuint);
		using FramebufferTexture2DProc = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
		using CheckFramebufferStatusProc = GLenum(APIENTRY *)(GLenum);
		using DeleteFramebuffersProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using BufferSubDataProc = void(APIENTRY *)(GLenum, std::ptrdiff_t, GLSizePtr, const void *);
		using GenQueriesProc = void(APIENTRY *)(GLsizei, GLuint *);
		using DeleteQueriesProc = void(APIENTRY *)(GLsizei, const GLuint *);
		using BeginQueryProc = void(APIENTRY *)(GLenum, GLuint);
		using EndQueryProc = void(APIENTRY *)(GLenum);
		using GetQueryObjectivProc = void(APIENTRY *)(GLuint, GLenum, GLint *);
		using GetQueryObjectui64vProc = void(APIENTRY *)(GLuint, GLenum, std::uint64_t *);

#define BL_GL_FUNCTIONS(X) \
	X(GenBuffers, glGenBuffers) \
	X(BindBuffer, glBindBuffer) \
	X(BufferData, glBufferData) \
	X(DeleteBuffers, glDeleteBuffers) \
	X(GenTextures, glGenTextures) \
	X(DeleteTextures, glDeleteTextures) \
	X(BindTexture, glBindTexture) \
	X(TexParameteri, glTexParameteri) \
	X(TexImage2D, glTexImage2D) \
	X(TexImage2DMultisample, glTexImage2DMultisample) \
	X(BlitFramebuffer, glBlitFramebuffer) \
	X(PixelStorei, glPixelStorei) \
	X(GenerateMipmap, glGenerateMipmap) \
	X(GenVertexArrays, glGenVertexArrays) \
	X(BindVertexArray, glBindVertexArray) \
	X(DeleteVertexArrays, glDeleteVertexArrays) \
	X(CreateShader, glCreateShader) \
	X(ShaderSource, glShaderSource) \
	X(CompileShader, glCompileShader) \
	X(GetShaderiv, glGetShaderiv) \
	X(GetShaderInfoLog, glGetShaderInfoLog) \
	X(DeleteShader, glDeleteShader) \
	X(CreateProgram, glCreateProgram) \
	X(AttachShader, glAttachShader) \
	X(LinkProgram, glLinkProgram) \
	X(GetProgramiv, glGetProgramiv) \
	X(GetActiveUniform, glGetActiveUniform) \
	X(GetProgramInfoLog, glGetProgramInfoLog) \
	X(DeleteProgram, glDeleteProgram) \
	X(UseProgram, glUseProgram) \
	X(BindAttribLocation, glBindAttribLocation) \
	X(BlendFuncSeparate, glBlendFuncSeparate) \
	X(GetUniformLocation, glGetUniformLocation) \
	X(Uniform1f, glUniform1f) \
	X(Uniform2fv, glUniform2fv) \
	X(Uniform3fv, glUniform3fv) \
	X(Uniform4fv, glUniform4fv) \
	X(UniformMatrix4fv, glUniformMatrix4fv) \
	X(Uniform1i, glUniform1i) \
	X(EnableVertexAttribArray, glEnableVertexAttribArray) \
	X(VertexAttribPointer, glVertexAttribPointer) \
	X(VertexAttribIPointer, glVertexAttribIPointer) \
	X(DrawArrays, glDrawArrays) \
	X(DrawElements, glDrawElements) \
	X(DrawElementsBaseVertex, glDrawElementsBaseVertex) \
	X(ActiveTexture, glActiveTexture) \
	X(ClientActiveTexture, glClientActiveTexture) \
	X(GenSamplers, glGenSamplers) \
	X(SamplerParameteri, glSamplerParameteri) \
	X(BindSampler, glBindSampler) \
	X(DeleteSamplers, glDeleteSamplers) \
	X(GenFramebuffers, glGenFramebuffers) \
	X(BindFramebuffer, glBindFramebuffer) \
	X(FramebufferTexture2D, glFramebufferTexture2D) \
	X(CheckFramebufferStatus, glCheckFramebufferStatus) \
	X(DeleteFramebuffers, glDeleteFramebuffers) \
	X(BufferSubData, glBufferSubData) \
	X(GenQueries, glGenQueries) \
	X(DeleteQueries, glDeleteQueries) \
	X(BeginQuery, glBeginQuery) \
	X(EndQuery, glEndQuery) \
	X(GetQueryObjectiv, glGetQueryObjectiv) \
	X(GetQueryObjectui64v, glGetQueryObjectui64v)

#define BL_DECLARE_GL_FUNCTION(name, symbol) name##Proc name = nullptr;
		BL_GL_FUNCTIONS(BL_DECLARE_GL_FUNCTION)
#undef BL_DECLARE_GL_FUNCTION

		/// Resolves required functions from WGL or the system OpenGL library.
		bool Load() noexcept
		{
			const HMODULE library = GetModuleHandleW(L"opengl32.dll");
			bool loaded = true;
#define BL_LOAD_GL_FUNCTION(name, symbol)                                      \
			name = reinterpret_cast<name##Proc>(wglGetProcAddress(#symbol));     \
			if (reinterpret_cast<std::uintptr_t>(name) <= 3 ||                  \
				reinterpret_cast<std::uintptr_t>(name) == static_cast<std::uintptr_t>(-1)) \
				name = reinterpret_cast<name##Proc>(GetProcAddress(library, #symbol)); \
			loaded = loaded && name != nullptr;
			BL_GL_FUNCTIONS(BL_LOAD_GL_FUNCTION)
#undef BL_LOAD_GL_FUNCTION
			return loaded;
		}
#undef BL_GL_FUNCTIONS
	};
}
