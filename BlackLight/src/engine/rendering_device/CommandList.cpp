#include "engine/rendering_device/CommandList.h"
#include "CommandListInternal.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace BlackLight::Engine::Rendering
{
	namespace
	{
		/// Rejects operations on a moved-from command list.
		template <typename ImplPointer>
		auto &RequireImpl(ImplPointer &impl)
		{
			if (!impl)
				throw std::logic_error("CommandList was moved from");
			return *impl;
		}

		/// Validates a named uniform and returns an owned copy.
		std::string ValidateName(const char *name)
		{
			if (!name || !*name)
				throw std::invalid_argument("Uniform name cannot be empty");
			return name;
		}

		/// Rejects non-finite uniform values before they reach backend APIs.
		void ValidateValues(const float *values, std::size_t count)
		{
			for (std::size_t index = 0; index < count; ++index)
				if (!std::isfinite(values[index]))
					throw std::invalid_argument("Uniform values must be finite");
		}
	}

	/// Creates an empty command list.
	CommandList::CommandList() : impl_(std::make_unique<Impl>()) {}
	/// Releases command storage.
	CommandList::~CommandList() = default;
	/// Transfers command storage.
	CommandList::CommandList(CommandList &&other) noexcept = default;
	/// Transfers command storage.
	CommandList &CommandList::operator=(CommandList &&other) noexcept = default;

	/// Records the pipeline for subsequent draws.
	void CommandList::SetPipeline(const GraphicsPipeline &pipeline)
	{
		if (!pipeline.impl_)
			throw std::invalid_argument("Graphics pipeline is empty");
		RequireImpl(impl_).commands.emplace_back(Impl::PipelineCommand{pipeline.impl_});
	}

	/// Records a vertex buffer binding for subsequent draws.
	void CommandList::SetVertexBuffer(const Buffer &buffer, std::uint32_t stride, std::uint32_t offset)
	{
		if (!buffer.impl_ ||
			(buffer.impl_->usage != BufferUsage::Vertex && buffer.impl_->usage != BufferUsage::DynamicVertex) ||
			stride == 0 ||
			offset > buffer.impl_->size)
			throw std::invalid_argument("A valid vertex buffer, stride, and offset are required");
		RequireImpl(impl_).commands.emplace_back(Impl::VertexBufferCommand{buffer.impl_, stride, offset});
	}

	/// Records an index buffer binding for subsequent indexed draws.
	void CommandList::SetIndexBuffer(const Buffer &buffer, ResourceFormat index_format, std::uint32_t offset)
	{
		if (!buffer.impl_ || buffer.impl_->usage != BufferUsage::Index || offset > buffer.impl_->size ||
			(index_format != ResourceFormat::R16Uint && index_format != ResourceFormat::R32Uint))
			throw std::invalid_argument("A valid index buffer, index format, and offset are required");
		const std::size_t index_size = index_format == ResourceFormat::R16Uint ? 2 : 4;
		if (offset % index_size != 0)
			throw std::invalid_argument("Index buffer offset must be aligned to its index format");
		RequireImpl(impl_).commands.emplace_back(Impl::IndexBufferCommand{buffer.impl_, index_format, offset});
	}

	/// Records a viewport after checking finite, non-negative dimensions and depth range.
	void CommandList::SetViewport(const Viewport &viewport)
	{
		if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
			!std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
			!std::isfinite(viewport.minimum_depth) || !std::isfinite(viewport.maximum_depth) ||
			viewport.width < 0.0f || viewport.height < 0.0f ||
			viewport.minimum_depth < 0.0f || viewport.maximum_depth > 1.0f ||
			viewport.minimum_depth > viewport.maximum_depth)
			throw std::invalid_argument("Viewport values are outside the supported range");
		RequireImpl(impl_).commands.emplace_back(Impl::ViewportCommand{viewport});
	}

	/// Records a scalar uniform.
	void CommandList::SetUniform(const char *name, float value)
	{
		ValidateValues(&value, 1);
		Impl::UniformCommand command{};
		command.name = ValidateName(name);
		command.values[0] = value;
		RequireImpl(impl_).commands.emplace_back(std::move(command));
	}

	/// Records a two-component uniform.
	void CommandList::SetUniform(const char *name, const Math::Vector2 &value)
	{
		const float values[] = {value.x, value.y};
		ValidateValues(values, 2);
		Impl::UniformCommand command{};
		command.name = ValidateName(name);
		command.component_count = 2;
		command.values[0] = value.x;
		command.values[1] = value.y;
		RequireImpl(impl_).commands.emplace_back(std::move(command));
	}

	/// Records a three-component uniform.
	void CommandList::SetUniform(const char *name, const Math::Vector3 &value)
	{
		const float values[] = {value.x, value.y, value.z};
		ValidateValues(values, 3);
		Impl::UniformCommand command{};
		command.name = ValidateName(name);
		command.component_count = 3;
		command.values[0] = value.x;
		command.values[1] = value.y;
		command.values[2] = value.z;
		RequireImpl(impl_).commands.emplace_back(std::move(command));
	}

	/// Records a four-component uniform.
	void CommandList::SetUniform(const char *name, const std::array<float, 4> &value)
	{
		ValidateValues(value.data(), value.size());
		Impl::UniformCommand command{};
		command.name = ValidateName(name);
		command.component_count = 4;
		for (std::size_t index = 0; index < value.size(); ++index)
			command.values[index] = value[index];
		RequireImpl(impl_).commands.emplace_back(std::move(command));
	}

	/// Records a row-major matrix uniform.
	void CommandList::SetUniform(const char *name, const Math::Matrix4 &value)
	{
		for (const auto &row : value.m)
			ValidateValues(row, 4);
		Impl::UniformCommand command{};
		command.name = ValidateName(name);
		command.component_count = 16;
		command.matrix = true;
		for (std::size_t row = 0; row < 4; ++row)
			for (std::size_t column = 0; column < 4; ++column)
				command.values[row * 4 + column] = value.m[row][column];
		RequireImpl(impl_).commands.emplace_back(std::move(command));
	}

	/// Records a named texture binding and its texture-unit slot.
	void CommandList::SetTexture(const char *uniform_name, std::uint32_t slot, const Texture &texture)
	{
		if (!texture.impl_)
			throw std::invalid_argument("Texture is empty");
		RequireImpl(impl_).commands.emplace_back(
			Impl::TextureCommand{ValidateName(uniform_name), slot, texture.impl_});
	}

	/// Records a non-indexed draw unless its vertex count is zero.
	void CommandList::Draw(std::uint32_t vertex_count, std::uint32_t first_vertex)
	{
		if (vertex_count == 0)
			return;
		RequireImpl(impl_).commands.emplace_back(Impl::DrawCommand{vertex_count, first_vertex});
		++impl_->draw_count;
	}

	/// Records an indexed draw unless its index count is zero.
	void CommandList::DrawIndexed(std::uint32_t index_count, std::uint32_t first_index, std::int32_t vertex_offset)
	{
		if (index_count == 0)
			return;
		RequireImpl(impl_).commands.emplace_back(Impl::DrawIndexedCommand{index_count, first_index, vertex_offset});
		++impl_->draw_count;
	}

	/// Removes all commands and releases their captured resource references.
	void CommandList::Clear()
	{
		if (!impl_)
			return;
		impl_->commands.clear();
		impl_->draw_count = 0;
	}

	/// Returns the number of recorded draw commands.
	std::size_t CommandList::GetDrawCount() const noexcept { return impl_ ? impl_->draw_count : 0; }
}
