#include "engine/rendering_device/RenderingResources.h"
#include "RenderingResourcesInternal.h"

#include <utility>

namespace BlackLight::Engine::Rendering
{
	/// Creates an empty buffer.
	Buffer::Buffer() = default;
	/// Releases a buffer.
	Buffer::~Buffer() = default;
	/// Transfers buffer ownership.
	Buffer::Buffer(Buffer &&other) noexcept = default;
	/// Transfers buffer ownership.
	Buffer &Buffer::operator=(Buffer &&other) noexcept = default;
	/// Reports whether the buffer has an implementation.
	Buffer::operator bool() const noexcept { return static_cast<bool>(impl_); }
	/// Returns the buffer size.
	std::size_t Buffer::GetSize() const noexcept { return impl_ ? impl_->size : 0; }

	/// Creates an empty texture.
	Texture::Texture() = default;
	/// Releases a texture.
	Texture::~Texture() = default;
	/// Transfers texture ownership.
	Texture::Texture(Texture &&other) noexcept = default;
	/// Transfers texture ownership.
	Texture &Texture::operator=(Texture &&other) noexcept = default;
	/// Reports whether the texture has an implementation.
	Texture::operator bool() const noexcept { return static_cast<bool>(impl_); }
	/// Returns the texture width.
	std::uint32_t Texture::GetWidth() const noexcept { return impl_ ? impl_->width : 0; }
	/// Returns the texture height.
	std::uint32_t Texture::GetHeight() const noexcept { return impl_ ? impl_->height : 0; }

	/// Creates an empty pipeline.
	GraphicsPipeline::GraphicsPipeline() = default;
	/// Releases a pipeline.
	GraphicsPipeline::~GraphicsPipeline() = default;
	/// Transfers pipeline ownership.
	GraphicsPipeline::GraphicsPipeline(GraphicsPipeline &&other) noexcept = default;
	/// Transfers pipeline ownership.
	GraphicsPipeline &GraphicsPipeline::operator=(GraphicsPipeline &&other) noexcept = default;
	/// Reports whether the pipeline has an implementation.
	GraphicsPipeline::operator bool() const noexcept { return static_cast<bool>(impl_); }
}
