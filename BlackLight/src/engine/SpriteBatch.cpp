#include "engine/SpriteBatch.h"
#include "engine/Camera2D.h"
#include "engine/FrameStats.h"
#include "engine/rendering_device/CommandList.h"
#include "engine/rendering_device/RenderingResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace BlackLight::Engine
{

	/// Replaces the destination receiving submitted sprite commands.
	void SpriteBatch::SetSubmitCallback(SubmitCallback callback)
	{
		submit_callback_ = std::move(callback);
	}

	/// Finishes the previous batch and starts collecting a new ordered batch.
	void SpriteBatch::Begin(SpriteSortMode sort_mode)
	{
		if (begun_)
		{
			End();
		}
		sort_mode_ = sort_mode;
		begun_ = true;
		commands_.clear();
	}

	/// Collects a sprite or submits it immediately when requested.
	void SpriteBatch::Draw(const SpriteCommand &command)
	{
		if (!begun_)
		{
			return;
		}
		if (sort_mode_ == SpriteSortMode::Immediate)
		{
			if (submit_callback_)
			{
				submit_callback_(std::vector<SpriteCommand>{command});
			}
			return;
		}
		commands_.push_back(command);
	}

	/// Sorts collected sprites and invokes the host's submission callback.
	void SpriteBatch::End()
	{
		if (!begun_)
		{
			return;
		}
		SortCommands();
		if (submit_callback_ && !commands_.empty())
		{
			submit_callback_(commands_);
		}
		commands_.clear();
		begun_ = false;
	}

	/// Discards queued sprites without issuing a GPU submission.
	void SpriteBatch::Clear()
	{
		commands_.clear();
		begun_ = false;
	}

	/// Returns the number of deferred sprites awaiting submission.
	std::size_t SpriteBatch::GetCommandCount() const noexcept
	{
		return commands_.size();
	}

	/// Reports whether Draw can currently accept sprites.
	bool SpriteBatch::IsBegun() const noexcept
	{
		return begun_;
	}

	/// Preserves stable ordering within each requested sort key.
	void SpriteBatch::SortCommands()
	{
		switch (sort_mode_)
		{
		case SpriteSortMode::Texture:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.texture < right.texture;
							 });
			break;
		case SpriteSortMode::BackToFront:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.depth > right.depth;
							 });
			break;
		case SpriteSortMode::FrontToBack:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.depth < right.depth;
							 });
			break;
		case SpriteSortMode::Deferred:
		case SpriteSortMode::Immediate:
			break;
		}
	}

	namespace
	{
		/// Stores one interleaved position, UV, and tint for the sprite shaders.
		struct SpriteVertex
		{
			float x;
			float y;
			float u;
			float v;
			float r;
			float g;
			float b;
			float a;
		};

		constexpr const char *kSpriteVertexGl = R"glsl(#version 330 compatibility
in vec2 POSITION;
in vec2 TEXCOORD;
in vec4 COLOR;
uniform mat4 uProjection;
out vec2 vTexCoord;
out vec4 vColor;
void main()
{
    gl_Position = vec4(POSITION, 0.0, 1.0) * uProjection;
    vTexCoord = TEXCOORD;
    vColor = COLOR;
})glsl";

		constexpr const char *kSpritePixelGl = R"glsl(#version 330 compatibility
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragmentColor;
void main()
{
    fragmentColor = texture(uTexture, vTexCoord) * vColor;
})glsl";

		constexpr const char *kSpriteVertexHlsl = R"hlsl(
cbuffer SpriteConstants : register(b0) { row_major float4x4 uProjection; };
struct Input { float2 position : POSITION; float2 texCoord : TEXCOORD; float4 color : COLOR; };
struct Output { float4 position : SV_POSITION; float2 texCoord : TEXCOORD; float4 color : COLOR; };
Output main(Input input)
{
    Output output;
    output.position = mul(float4(input.position, 0.0, 1.0), uProjection);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
})hlsl";

		constexpr const char *kSpritePixelHlsl = R"hlsl(
Texture2D uTexture : register(t0);
SamplerState uTextureSampler : register(s0);
struct Input { float4 position : SV_POSITION; float2 texCoord : TEXCOORD; float4 color : COLOR; };
float4 main(Input input) : SV_TARGET
{
    return uTexture.Sample(uTextureSampler, input.texCoord) * input.color;
})hlsl";

		/// Creates six transformed vertices for a rotated and tinted quad.
		void AppendSpriteVertices(std::vector<SpriteVertex> &vertices, const SpriteCommand &command)
		{
			const float cosine = std::cos(command.rotation);
			const float sine = std::sin(command.rotation);
			const float left = -command.origin.x;
			const float top = -command.origin.y;
			const float right = command.size.x - command.origin.x;
			const float bottom = command.size.y - command.origin.y;
			const std::array<Math::Vector2, 4> corners{{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
			const std::array<Math::Vector2, 4> uvs{{command.texture_origin,
													{command.texture_origin.x + command.texture_size.x, command.texture_origin.y},
													{command.texture_origin.x + command.texture_size.x,
													 command.texture_origin.y + command.texture_size.y},
													{command.texture_origin.x, command.texture_origin.y + command.texture_size.y}}};
			const std::array<std::size_t, 6> indices{{0, 1, 2, 0, 2, 3}};
			for (const auto index : indices)
			{
				const float x = corners[index].x * command.scale.x;
				const float y = corners[index].y * command.scale.y;
				vertices.push_back({command.position.x + x * cosine - y * sine,
									command.position.y + x * sine + y * cosine,
									uvs[index].x,
									uvs[index].y,
									command.color.r,
									command.color.g,
									command.color.b,
									command.color.a});
			}
		}
	}

	/// Owns the dynamic sprite resources and texture-handle table.
	class SpriteBatchRenderer::Impl
	{
	public:
		Rendering::RenderingDevice &device;
		FrameStats *stats;
		Rendering::Texture white_texture;
		Rendering::GraphicsPipeline pipeline;
		Rendering::Buffer vertices;
		std::size_t vertex_capacity = 0;
		TextureHandle next_texture = 1;
		const Rendering::Texture *last_texture = nullptr;
		const Camera2D *camera = nullptr;
		std::unordered_map<TextureHandle, const Rendering::Texture *> textures;

		/// Creates the pipeline and white fallback texture used by colored primitives.
		Impl(Rendering::RenderingDevice &owner, FrameStats *frame_stats, std::size_t maximum_sprites)
			: device(owner), stats(frame_stats)
		{
			if (!maximum_sprites || maximum_sprites >
										std::numeric_limits<std::size_t>::max() / (6u * sizeof(SpriteVertex)))
				throw std::invalid_argument("Maximum sprite capacity is invalid");
			vertex_capacity = maximum_sprites * 6u;
			const std::array<std::uint8_t, 4> white{{255, 255, 255, 255}};
			Rendering::TextureDesc texture_description;
			texture_description.width = texture_description.height = 1;
			texture_description.format = Rendering::ResourceFormat::RGBA8Unorm;
			texture_description.initial_data = white.data();
			texture_description.initial_data_size = white.size();
			texture_description.minification_filter = Rendering::TextureFilter::Nearest;
			texture_description.magnification_filter = Rendering::TextureFilter::Nearest;
			white_texture = device.CreateTexture(texture_description);

			const std::array<Rendering::VertexAttribute, 3> attributes{{{0, 2, Rendering::ResourceFormat::RG32Float,
																		 static_cast<std::uint32_t>(offsetof(SpriteVertex, x)), "POSITION", 0},
																		{1, 2, Rendering::ResourceFormat::RG32Float,
																		 static_cast<std::uint32_t>(offsetof(SpriteVertex, u)), "TEXCOORD", 0},
																		{2, 4, Rendering::ResourceFormat::RGBA32Float,
																		 static_cast<std::uint32_t>(offsetof(SpriteVertex, r)), "COLOR", 0}}};
			Rendering::GraphicsPipelineDesc pipeline_description;
			pipeline_description.vertex_shader = {kSpriteVertexGl, kSpriteVertexHlsl, "main"};
			pipeline_description.fragment_shader = {kSpritePixelGl, kSpritePixelHlsl, "main"};
			pipeline_description.vertex_attributes = attributes.data();
			pipeline_description.vertex_attribute_count = static_cast<std::uint32_t>(attributes.size());
			pipeline_description.vertex_stride = sizeof(SpriteVertex);
			pipeline_description.depth_test = false;
			pipeline_description.depth_write = false;
			pipeline_description.alpha_blending = true;
			pipeline_description.cull_mode = Rendering::GraphicsPipelineDesc::CullMode::None;
			pipeline = device.CreateGraphicsPipeline(pipeline_description);

			Rendering::BufferDesc buffer_description;
			buffer_description.usage = Rendering::BufferUsage::DynamicVertex;
			buffer_description.size = vertex_capacity * sizeof(SpriteVertex);
			vertices = device.CreateBuffer(buffer_description);
		}
	};

	/// Creates GPU resources for a textured 2D pipeline.
	SpriteBatchRenderer::SpriteBatchRenderer(Rendering::RenderingDevice &device,
											 FrameStats *stats,
											 std::size_t maximum_sprites)
		: impl_(std::make_unique<Impl>(device, stats, maximum_sprites)) {}
	/// Releases resources while the owning device is still available.
	SpriteBatchRenderer::~SpriteBatchRenderer() = default;
	/// Allocates a stable handle for a borrowed texture.
	TextureHandle SpriteBatchRenderer::RegisterTexture(const Rendering::Texture &texture)
	{
		if (!texture)
			throw std::invalid_argument("Cannot register an empty sprite texture");
		const TextureHandle handle = impl_->next_texture++;
		impl_->textures.emplace(handle, &texture);
		return handle;
	}
	/// Reassociates a known nonzero handle with another texture.
	void SpriteBatchRenderer::SetTexture(TextureHandle handle, const Rendering::Texture &texture)
	{
		if (!handle || !texture || impl_->textures.find(handle) == impl_->textures.end())
			throw std::invalid_argument("Sprite texture handle or resource is invalid");
		impl_->textures[handle] = &texture;
	}
	/// Forgets a borrowed texture handle.
	void SpriteBatchRenderer::UnregisterTexture(TextureHandle handle) noexcept
	{
		const auto found = impl_->textures.find(handle);
		if (found != impl_->textures.end() && impl_->last_texture == found->second)
			impl_->last_texture = nullptr;
		impl_->textures.erase(handle);
	}
	/// Sets the projection source for subsequent sprite submissions.
	void SpriteBatchRenderer::SetCamera(const Camera2D &camera) noexcept { impl_->camera = &camera; }
	/// Returns the renderer-owned white fallback texture.
	const Rendering::Texture &SpriteBatchRenderer::GetWhiteTexture() const noexcept
	{
		return impl_->white_texture;
	}
	/// Generates streaming vertices and submits one command list per texture group.
	void SpriteBatchRenderer::Submit(const std::vector<SpriteCommand> &commands)
	{
		if (commands.empty())
			return;
		if (!impl_->camera)
			throw std::logic_error("Sprite renderer requires a camera before submission");
		const auto viewport = impl_->camera->GetViewport();
		const auto projection = impl_->camera->GetProjection();
		std::size_t first = 0;
		while (first < commands.size())
		{
			const TextureHandle handle = commands[first].texture;
			const Rendering::Texture *texture = &impl_->white_texture;
			if (handle)
			{
				const auto found = impl_->textures.find(handle);
				if (found == impl_->textures.end() || !found->second || !*found->second)
					throw std::out_of_range("Sprite command references an unregistered texture");
				texture = found->second;
			}
			std::size_t end = first + 1;
			while (end < commands.size() && commands[end].texture == handle)
				++end;
			const std::size_t sprite_count = end - first;
			const std::size_t vertex_count = sprite_count * 6u;
			if (vertex_count > std::numeric_limits<std::uint32_t>::max())
				throw std::length_error("Sprite batch exceeds the draw vertex limit");
			if (vertex_count > impl_->vertex_capacity)
			{
				impl_->vertex_capacity = vertex_count;
				Rendering::BufferDesc description;
				description.usage = Rendering::BufferUsage::DynamicVertex;
				description.size = vertex_count * sizeof(SpriteVertex);
				impl_->vertices = impl_->device.CreateBuffer(description);
			}
			std::vector<SpriteVertex> upload;
			upload.reserve(vertex_count);
			for (std::size_t index = first; index < end; ++index)
				AppendSpriteVertices(upload, commands[index]);
			impl_->device.UpdateBuffer(impl_->vertices, 0, upload.data(),
									   upload.size() * sizeof(SpriteVertex));
			auto list = impl_->device.CreateCommandList();
			list.SetPipeline(impl_->pipeline);
			list.SetVertexBuffer(impl_->vertices, sizeof(SpriteVertex));
			list.SetViewport(viewport);
			list.SetUniform("uProjection", projection);
			list.SetTexture("uTexture", 0, *texture);
			list.Draw(static_cast<std::uint32_t>(vertex_count));
			impl_->device.Submit(list);
			if (impl_->stats)
				impl_->stats->RecordDraw(vertex_count,
										 impl_->last_texture && impl_->last_texture != texture);
			impl_->last_texture = texture;
			first = end;
		}
	}

} // namespace BlackLight::Engine
