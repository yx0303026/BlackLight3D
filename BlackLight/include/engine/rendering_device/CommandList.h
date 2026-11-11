#pragma once

#include "../../BlackLightApi.h"
#include "RenderingResources.h"

#include <cstdint>
#include <memory>

namespace BlackLight::Engine::Rendering
{
	/// Defines a viewport in drawable pixel coordinates.
	struct Viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minimum_depth = 0.0f;
		float maximum_depth = 1.0f;
	};

	/// Records normalized resource bindings and draw commands for one frame.
	class BLACKLIGHT_API CommandList final
	{
	public:
		/// Creates an empty command list.
		CommandList();
		/// Releases recorded command storage.
		~CommandList();
		/// Transfers recorded command ownership.
		CommandList(CommandList &&other) noexcept;
		/// Transfers recorded command ownership.
		CommandList &operator=(CommandList &&other) noexcept;
		/// Prevents copying recorded commands.
		CommandList(const CommandList &) = delete;
		/// Prevents copying recorded commands.
		CommandList &operator=(const CommandList &) = delete;
		/// Selects the pipeline used by subsequent draw calls.
		void SetPipeline(const GraphicsPipeline &pipeline);
		/// Selects the vertex buffer used by subsequent draw calls.
		void SetVertexBuffer(const Buffer &buffer, std::uint32_t stride, std::uint32_t offset = 0);
		/// Selects the index buffer used by subsequent indexed draw calls.
		void SetIndexBuffer(const Buffer &buffer, ResourceFormat index_format, std::uint32_t offset = 0);
		/// Selects the viewport for subsequent draw calls.
		void SetViewport(const Viewport &viewport);
		/// Records a non-indexed draw.
		void Draw(std::uint32_t vertex_count, std::uint32_t first_vertex = 0);
		/// Records an indexed draw.
		void DrawIndexed(std::uint32_t index_count, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0);
		/// Removes all recorded commands.
		void Clear();
		/// Returns the number of recorded draw commands.
		std::size_t GetDrawCount() const noexcept;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
		friend class RenderingDevice;
	};
}
