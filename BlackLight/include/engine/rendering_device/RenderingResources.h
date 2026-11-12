#pragma once

#include "../../BlackLightApi.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace BlackLight::Engine::Rendering
{
	enum class RenderingBackend;
	namespace Detail
	{
		class OpenGLDevice;
		class Direct3D11Device;
		class DeviceLifetime;
	}

	/// Describes formats supported by the normalized resource API.
	enum class ResourceFormat
	{
		R8Unorm,
		RGBA8Unorm,
		RGBA8Srgb,
		R32Float,
		RG32Float,
		RGB32Float,
		RGBA32Float,
		D24S8,
		R16Uint,
		R32Uint
	};

	/// Selects how texture samples are filtered and addressed.
	enum class TextureFilter
	{
		Nearest,
		Linear
	};

	/// Selects how coordinates outside the normalized texture range are handled.
	enum class TextureAddressMode
	{
		Clamp,
		Repeat,
		Mirror
	};

	/// Describes a single vertex attribute in the interleaved vertex buffer.
	struct VertexAttribute
	{
		std::uint32_t location = 0;
		std::uint32_t component_count = 0;
		ResourceFormat format = ResourceFormat::R32Float;
		std::uint32_t offset = 0;
		const char *semantic_name = nullptr;
		std::uint32_t semantic_index = 0;
	};

	/// Selects the intended usage of a GPU buffer.
	enum class BufferUsage
	{
		Vertex,
		Index,
		Constant,
		Storage,
		DynamicVertex
	};

	/// Describes a buffer allocation and its initial bytes.
	struct BufferDesc
	{
		BufferUsage usage = BufferUsage::Vertex;
		std::size_t size = 0;
		const void *initial_data = nullptr;
	};

	/// Describes a two-dimensional texture allocation.
	struct TextureDesc
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t mip_levels = 1;
		ResourceFormat format = ResourceFormat::RGBA8Unorm;
		/// Supplies every mip level in tightly packed order when non-null.
		const void *initial_data = nullptr;
		/// Must cover all mip levels when initial_data is non-null.
		std::size_t initial_data_size = 0;
		TextureFilter minification_filter = TextureFilter::Linear;
		TextureFilter magnification_filter = TextureFilter::Linear;
		TextureAddressMode address_u = TextureAddressMode::Clamp;
		TextureAddressMode address_v = TextureAddressMode::Clamp;
		bool render_target = false;
		/// Number of samples used by a render target. 1 creates a normal texture;
		/// a power-of-two value above 1 creates a multisampled render target that
		/// must be resolved before it can be sampled.
		std::uint32_t sample_count = 1;
	};

	/// Owns one backend buffer and releases it through its device implementation.
	class BLACKLIGHT_API Buffer final
	{
	public:
		/// Creates an empty buffer handle.
		Buffer();
		/// Releases the owned GPU resource.
		~Buffer();
		/// Transfers resource ownership.
		Buffer(Buffer &&other) noexcept;
		/// Releases the current resource and transfers ownership.
		Buffer &operator=(Buffer &&other) noexcept;
		/// Prevents copying a unique GPU resource.
		Buffer(const Buffer &) = delete;
		/// Prevents copying a unique GPU resource.
		Buffer &operator=(const Buffer &) = delete;
		/// Reports whether the handle owns a resource.
		explicit operator bool() const noexcept;
		/// Returns the resource byte size.
		std::size_t GetSize() const noexcept;

	private:
		class Impl;
		std::shared_ptr<Impl> impl_;
		friend class RenderingDevice;
		friend class CommandList;
		friend class Detail::OpenGLDevice;
		friend class Detail::Direct3D11Device;
	};

	/// Owns one backend texture and releases it through its device implementation.
	class BLACKLIGHT_API Texture final
	{
	public:
		/// Creates an empty texture handle.
		Texture();
		/// Releases the owned GPU resource.
		~Texture();
		/// Transfers resource ownership.
		Texture(Texture &&other) noexcept;
		/// Releases the current resource and transfers ownership.
		Texture &operator=(Texture &&other) noexcept;
		/// Prevents copying a unique GPU resource.
		Texture(const Texture &) = delete;
		/// Prevents copying a unique GPU resource.
		Texture &operator=(const Texture &) = delete;
		/// Reports whether the handle owns a resource.
		explicit operator bool() const noexcept;
		/// Returns the texture width.
		std::uint32_t GetWidth() const noexcept;
		/// Returns the texture height.
		std::uint32_t GetHeight() const noexcept;
		/// Returns the number of samples, or 1 for a single-sample texture.
		std::uint32_t GetSampleCount() const noexcept;

	private:
		class Impl;
		std::shared_ptr<Impl> impl_;
		friend class RenderingDevice;
		friend class CommandList;
		friend class Detail::OpenGLDevice;
		friend class Detail::Direct3D11Device;
	};

	/// Describes a shader source variant for a specific normalized backend.
	struct ShaderSource
	{
		const char *open_gl = nullptr;
		const char *direct3d11 = nullptr;
		const char *entry_point = "main";
	};

	/// Describes the vertex and fragment shader pair used by a graphics pipeline.
	struct GraphicsPipelineDesc
	{
		ShaderSource vertex_shader{};
		ShaderSource fragment_shader{};
		const VertexAttribute *vertex_attributes = nullptr;
		std::uint32_t vertex_attribute_count = 0;
		std::uint32_t vertex_stride = 0;
		ResourceFormat color_format = ResourceFormat::RGBA8Unorm;
		ResourceFormat depth_format = ResourceFormat::D24S8;
		bool depth_test = true;
		bool depth_write = true;
		bool alpha_blending = false;
		enum class CullMode
		{
			None,
			Front,
			Back
		} cull_mode = CullMode::None;
		enum class PrimitiveTopology
		{
			TriangleList,
			LineList,
			PointList
		} topology = PrimitiveTopology::TriangleList;
		/// Sample count of the render targets this pipeline draws into. Must match
		/// the active color/depth target; 1 for single-sample targets.
		std::uint32_t sample_count = 1;
		bool srgb = false;
	};

	/// Owns a backend graphics pipeline.
	class BLACKLIGHT_API GraphicsPipeline final
	{
	public:
		/// Creates an empty pipeline handle.
		GraphicsPipeline();
		/// Releases the owned pipeline.
		~GraphicsPipeline();
		/// Transfers pipeline ownership.
		GraphicsPipeline(GraphicsPipeline &&other) noexcept;
		/// Releases the current pipeline and transfers ownership.
		GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept;
		/// Prevents copying a unique pipeline.
		GraphicsPipeline(const GraphicsPipeline &) = delete;
		/// Prevents copying a unique pipeline.
		GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
		/// Reports whether the handle owns a pipeline.
		explicit operator bool() const noexcept;

	private:
		class Impl;
		std::shared_ptr<Impl> impl_;
		friend class RenderingDevice;
		friend class CommandList;
		friend class Detail::OpenGLDevice;
		friend class Detail::Direct3D11Device;
		friend class Detail::DeviceLifetime;
	};
}
