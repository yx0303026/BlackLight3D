#pragma once

#include "engine/rendering_device/RenderingDevice.h"

#include <RmlUi_Renderer_DX11.h>
#include <RmlUi/Core.h>
#include <d3d11_1.h>
#include <wrl/client.h>

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BlackLight::Engine::Rendering::Detail
{
	/// Keeps RmlUi's render-interface address and effect handles stable across device loss.
	class D3D11UiRenderer final : public Rml::RenderInterface
	{
	public:
		/// Creates the official renderer after validating its D3D11.1 runtime dependency.
		void Initialize(ID3D11Device *device)
		{
			Microsoft::WRL::ComPtr<ID3D11Device1> device1;
			UINT quality = 0;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(device1.GetAddressOf()))) ||
				FAILED(device->CheckMultisampleQualityLevels(
					DXGI_FORMAT_R8G8B8A8_UNORM, NUM_MSAA_SAMPLES, &quality)) ||
				!quality)
				throw RenderingException(RenderingError::DeviceCreationFailed, "Direct3D11",
										 "RmlUi requires the D3D11.1 runtime and UI MSAA support");
			renderer_ = std::make_unique<RenderInterface_DX11>(device);
			try
			{
				for (auto &entry : filters_)
				{
					auto &effect = entry.second;
					// Saved mask images are local to a rendered frame and cannot survive a reset.
					if (effect.name.empty())
						continue;
					effect.native = renderer_->CompileFilter(effect.name, effect.parameters);
					if (!effect.native)
						throw RenderingException(RenderingError::DeviceRecoveryFailed, "Direct3D11",
												 "failed to restore an RmlUi filter");
				}
				for (auto &entry : shaders_)
				{
					auto &effect = entry.second;
					effect.native = renderer_->CompileShader(effect.name, effect.parameters);
					if (!effect.native)
						throw RenderingException(RenderingError::DeviceRecoveryFailed, "Direct3D11",
												 "failed to restore an RmlUi shader");
				}
			}
			catch (...)
			{
				ReleaseDeviceResources();
				throw;
			}
		}

		/// Releases cached GPU handles while retaining documents and effect descriptions.
		void ReleaseDeviceResources()
		{
			Rml::ReleaseCompiledGeometry(this);
			Rml::ReleaseTextures(this);
			if (renderer_)
			{
				for (auto &entry : filters_)
				{
					if (entry.second.native)
						renderer_->ReleaseFilter(entry.second.native);
					entry.second.native = 0;
				}
				for (auto &entry : shaders_)
				{
					if (entry.second.native)
						renderer_->ReleaseShader(entry.second.native);
					entry.second.native = 0;
				}
			}
			renderer_.reset();
		}

		/// Returns the official adapter used for frame setup and composition.
		RenderInterface_DX11 &GetRenderer() const { return *renderer_; }

		/// Compiles UI geometry using the official adapter.
		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
													Rml::Span<const int> indices) override
		{
			return renderer_->CompileGeometry(vertices, indices);
		}
		/// Releases geometry before the old native renderer is destroyed.
		void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override
		{
			if (renderer_)
				renderer_->ReleaseGeometry(geometry);
		}
		/// Draws compiled geometry with RmlUi's premultiplied vertex colors.
		void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
							Rml::TextureHandle texture) override
		{
			renderer_->RenderGeometry(geometry, translation, texture);
		}
		/// Loads textures with the official adapter's supported image formats.
		Rml::TextureHandle LoadTexture(Rml::Vector2i &dimensions, const Rml::String &source) override
		{
			return renderer_->LoadTexture(dimensions, source);
		}
		/// Uploads generated images and font atlases.
		Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
										   Rml::Vector2i dimensions) override
		{
			return renderer_->GenerateTexture(source, dimensions);
		}
		/// Releases textures before resetting their native renderer.
		void ReleaseTexture(Rml::TextureHandle texture) override
		{
			if (renderer_)
				renderer_->ReleaseTexture(texture);
		}
		/// Enables the UI's scissor rectangle.
		void EnableScissorRegion(bool enable) override { renderer_->EnableScissorRegion(enable); }
		/// Updates clipping in surface pixel coordinates.
		void SetScissorRegion(Rml::Rectanglei region) override { renderer_->SetScissorRegion(region); }
		/// Enables stencil-based UI clip masks.
		void EnableClipMask(bool enable) override { renderer_->EnableClipMask(enable); }
		/// Renders UI geometry into a clip mask.
		void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
							  Rml::Vector2f translation) override
		{
			renderer_->RenderToClipMask(operation, geometry, translation);
		}
		/// Applies an element transform.
		void SetTransform(const Rml::Matrix4f *transform) override { renderer_->SetTransform(transform); }
		/// Allocates a UI composition layer.
		Rml::LayerHandle PushLayer() override { return renderer_->PushLayer(); }
		/// Translates stable filter handles before composing native layers.
		void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
							 Rml::BlendMode mode, Rml::Span<const Rml::CompiledFilterHandle> filters) override
		{
			std::vector<Rml::CompiledFilterHandle> native;
			native.reserve(filters.size());
			for (const auto handle : filters)
			{
				const auto found = filters_.find(handle);
				if (found != filters_.end() && found->second.native)
					native.push_back(found->second.native);
			}
			renderer_->CompositeLayers(source, destination, mode, {native.data(), native.size()});
		}
		/// Restores the previous composition layer.
		void PopLayer() override { renderer_->PopLayer(); }
		/// Captures a layer as a texture managed by RmlUi's texture database.
		Rml::TextureHandle SaveLayerAsTexture() override { return renderer_->SaveLayerAsTexture(); }
		/// Retains a frame-local mask using the same stable filter-handle namespace.
		Rml::CompiledFilterHandle SaveLayerAsMaskImage() override
		{
			const auto native = renderer_->SaveLayerAsMaskImage();
			return StoreFilter({}, {}, native);
		}
		/// Retains filter parameters so a replacement device can recompile them.
		Rml::CompiledFilterHandle CompileFilter(const Rml::String &name,
												const Rml::Dictionary &parameters) override
		{
			return StoreFilter(name, parameters, renderer_->CompileFilter(name, parameters));
		}
		/// Releases both a stable filter handle and its current native implementation.
		void ReleaseFilter(Rml::CompiledFilterHandle filter) override
		{
			const auto found = filters_.find(filter);
			if (found == filters_.end())
				return;
			if (renderer_ && found->second.native)
				renderer_->ReleaseFilter(found->second.native);
			filters_.erase(found);
		}
		/// Retains shader parameters so a replacement device can recompile them.
		Rml::CompiledShaderHandle CompileShader(const Rml::String &name,
												const Rml::Dictionary &parameters) override
		{
			const auto native = renderer_->CompileShader(name, parameters);
			if (!native)
				return 0;
			const auto handle = next_effect_++;
			try
			{
				shaders_.emplace(handle, Effect{name, parameters, native});
			}
			catch (...)
			{
				renderer_->ReleaseShader(native);
				throw;
			}
			return handle;
		}
		/// Draws an element with its current native shader implementation.
		void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry,
						  Rml::Vector2f translation, Rml::TextureHandle texture) override
		{
			const auto found = shaders_.find(shader);
			if (found != shaders_.end() && found->second.native)
				renderer_->RenderShader(found->second.native, geometry, translation, texture);
		}
		/// Releases both a stable shader handle and its native implementation.
		void ReleaseShader(Rml::CompiledShaderHandle shader) override
		{
			const auto found = shaders_.find(shader);
			if (found == shaders_.end())
				return;
			if (renderer_ && found->second.native)
				renderer_->ReleaseShader(found->second.native);
			shaders_.erase(found);
		}

	private:
		/// Stores the recipe and current native handle of one compiled UI effect.
		struct Effect
		{
			Rml::String name;
			Rml::Dictionary parameters;
			Rml::CompiledFilterHandle native = 0;
		};

		/// Registers a filter without leaking its native handle on allocation failure.
		Rml::CompiledFilterHandle StoreFilter(const Rml::String &name,
											  const Rml::Dictionary &parameters,
											  Rml::CompiledFilterHandle native)
		{
			if (!native)
				return 0;
			const auto handle = next_effect_++;
			try
			{
				filters_.emplace(handle, Effect{name, parameters, native});
			}
			catch (...)
			{
				renderer_->ReleaseFilter(native);
				throw;
			}
			return handle;
		}

		std::unique_ptr<RenderInterface_DX11> renderer_;
		std::unordered_map<Rml::CompiledFilterHandle, Effect> filters_;
		std::unordered_map<Rml::CompiledShaderHandle, Effect> shaders_;
		std::uintptr_t next_effect_ = 1;
	};

	/// Restores the application bindings not preserved by the official DX11 adapter.
	class D3D11UiBindings final
	{
	public:
		/// Retains render targets, shader constants, and the second texture slot.
		explicit D3D11UiBindings(ID3D11DeviceContext *context) : context_(context)
		{
			context_->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets_, &depth_);
			context_->PSGetConstantBuffers(0, 2, pixel_constants_);
			context_->VSGetConstantBuffers(1, 1, &vertex_constant_);
			context_->PSGetShaderResources(1, 1, &texture_);
		}
		/// Restores bindings and releases every reference acquired during capture.
		~D3D11UiBindings()
		{
			context_->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets_, depth_);
			context_->PSSetConstantBuffers(0, 2, pixel_constants_);
			context_->VSSetConstantBuffers(1, 1, &vertex_constant_);
			context_->PSSetShaderResources(1, 1, &texture_);
			for (auto *target : targets_)
				if (target)
					target->Release();
			for (auto *buffer : pixel_constants_)
				if (buffer)
					buffer->Release();
			if (depth_)
				depth_->Release();
			if (vertex_constant_)
				vertex_constant_->Release();
			if (texture_)
				texture_->Release();
		}
		/// Prevents copying retained COM references.
		D3D11UiBindings(const D3D11UiBindings &) = delete;
		/// Prevents assigning retained COM references.
		D3D11UiBindings &operator=(const D3D11UiBindings &) = delete;

	private:
		ID3D11DeviceContext *context_;
		ID3D11RenderTargetView *targets_[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
		ID3D11DepthStencilView *depth_ = nullptr;
		ID3D11Buffer *pixel_constants_[2]{};
		ID3D11Buffer *vertex_constant_ = nullptr;
		ID3D11ShaderResourceView *texture_ = nullptr;
	};
}
