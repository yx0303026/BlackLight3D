#include "engine/rendering_device/CommandList.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace BlackLight::Engine::Rendering
{
	/// Stores normalized draw metadata until device submission.
	class CommandList::Impl
	{
	public:
		std::size_t draw_count = 0;
	};

	/// Creates an empty command list.
	CommandList::CommandList() : impl_(std::make_unique<Impl>()) {}
	/// Releases command storage.
	CommandList::~CommandList() = default;
	/// Transfers command storage.
	CommandList::CommandList(CommandList &&other) noexcept = default;
	/// Transfers command storage.
	CommandList &CommandList::operator=(CommandList &&other) noexcept = default;
	/// Selects the pipeline for subsequent draws.
	void CommandList::SetPipeline(const GraphicsPipeline &pipeline)
	{
		if (!pipeline)
			throw std::invalid_argument("Graphics pipeline is empty");
	}
	/// Selects a vertex buffer for subsequent draws.
	void CommandList::SetVertexBuffer(const Buffer &buffer, std::uint32_t stride, std::uint32_t)
	{
		if (!buffer || stride == 0)
			throw std::invalid_argument("Vertex buffer and non-zero stride are required");
	}
	/// Selects an index buffer for subsequent indexed draws.
	void CommandList::SetIndexBuffer(const Buffer &buffer, ResourceFormat, std::uint32_t)
	{
		if (!buffer)
			throw std::invalid_argument("Index buffer is empty");
	}
	/// Selects the viewport.
	void CommandList::SetViewport(const Viewport &viewport)
	{
		if (viewport.width < 0.0f || viewport.height < 0.0f)
			throw std::invalid_argument("Viewport dimensions cannot be negative");
	}
	/// Records a non-indexed draw.
	void CommandList::Draw(std::uint32_t vertex_count, std::uint32_t)
	{
		if (vertex_count == 0)
			return;
		++impl_->draw_count;
	}
	/// Records an indexed draw.
	void CommandList::DrawIndexed(std::uint32_t index_count, std::uint32_t, std::int32_t)
	{
		if (index_count == 0)
			return;
		++impl_->draw_count;
	}
	/// Removes all recorded commands.
	void CommandList::Clear() { impl_->draw_count = 0; }
	/// Returns the number of recorded draws.
	std::size_t CommandList::GetDrawCount() const noexcept { return impl_ ? impl_->draw_count : 0; }
}
