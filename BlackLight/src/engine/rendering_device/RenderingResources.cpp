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
	/// Reports whether the buffer still owns a live backend resource.
	Buffer::operator bool() const noexcept
	{
		return impl_ && impl_->deletion && !impl_->deletion->IsDestroyed();
	}
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
	/// Reports whether the texture still owns a live backend resource.
	Texture::operator bool() const noexcept
	{
		return impl_ && impl_->deletion && !impl_->deletion->IsDestroyed();
	}
	/// Returns the texture width.
	std::uint32_t Texture::GetWidth() const noexcept { return impl_ ? impl_->width : 0; }
	/// Returns the texture height.
	std::uint32_t Texture::GetHeight() const noexcept { return impl_ ? impl_->height : 0; }
	/// Returns the texture sample count.
	std::uint32_t Texture::GetSampleCount() const noexcept { return impl_ ? impl_->sample_count : 0; }

	/// Creates an empty pipeline.
	GraphicsPipeline::GraphicsPipeline() = default;
	/// Releases a pipeline.
	GraphicsPipeline::~GraphicsPipeline() = default;
	/// Transfers pipeline ownership.
	GraphicsPipeline::GraphicsPipeline(GraphicsPipeline &&other) noexcept = default;
	/// Transfers pipeline ownership.
	GraphicsPipeline &GraphicsPipeline::operator=(GraphicsPipeline &&other) noexcept = default;
	/// Reports whether the pipeline still owns live backend resources.
	GraphicsPipeline::operator bool() const noexcept
	{
		return impl_ && impl_->deletion && !impl_->deletion->IsDestroyed();
	}
}
