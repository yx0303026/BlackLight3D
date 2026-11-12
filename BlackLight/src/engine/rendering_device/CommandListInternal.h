#pragma once

#include "engine/rendering_device/CommandList.h"
#include "RenderingResourcesInternal.h"

#include <array>
#include <string>
#include <variant>
#include <vector>

namespace BlackLight::Engine::Rendering
{
	/// Stores the strongly owned resources and ordered commands submitted by a device.
	class CommandList::Impl final
	{
	public:
		/// Identifies a pipeline binding.
		struct PipelineCommand
		{
			std::shared_ptr<GraphicsPipeline::Impl> pipeline;
		};

		/// Identifies a vertex-buffer binding.
		struct VertexBufferCommand
		{
			std::shared_ptr<Buffer::Impl> buffer;
			std::uint32_t stride = 0;
			std::uint32_t offset = 0;
		};

		/// Identifies an index-buffer binding.
		struct IndexBufferCommand
		{
			std::shared_ptr<Buffer::Impl> buffer;
			ResourceFormat format = ResourceFormat::R16Uint;
			std::uint32_t offset = 0;
		};

		/// Stores a viewport update.
		struct ViewportCommand
		{
			Viewport viewport{};
		};

		/// Stores a scalar/vector/matrix uniform update.
		struct UniformCommand
		{
			std::string name;
			std::array<float, 16> values{};
			std::uint32_t component_count = 1;
			bool matrix = false;
		};

		/// Stores a texture and sampler-slot binding.
		struct TextureCommand
		{
			std::string uniform_name;
			std::uint32_t slot = 0;
			std::shared_ptr<Texture::Impl> texture;
		};

		/// Stores a non-indexed draw.
		struct DrawCommand
		{
			std::uint32_t count = 0;
			std::uint32_t first = 0;
		};

		/// Stores an indexed draw.
		struct DrawIndexedCommand
		{
			std::uint32_t count = 0;
			std::uint32_t first = 0;
			std::int32_t vertex_offset = 0;
		};

		using Command = std::variant<PipelineCommand, VertexBufferCommand, IndexBufferCommand,
									 ViewportCommand, UniformCommand, TextureCommand,
									 DrawCommand, DrawIndexedCommand>;

		std::vector<Command> commands;
		std::size_t draw_count = 0;
	};
}
