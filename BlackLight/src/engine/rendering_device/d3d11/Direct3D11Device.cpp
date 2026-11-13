#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../DeviceImplementation.h"
#include "../CommandListInternal.h"
#include "../RenderingResourcesInternal.h"
#include "../Win32Support.h"
#include "D3D11UiRenderer.h"
#include "engine/Logger.h"
#include "engine/rendering_device/Shader.h"

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace BlackLight::Engine::Rendering::Detail
{
	namespace
	{
		using Microsoft::WRL::ComPtr;

		constexpr wchar_t kWindowClass[] = L"BlackLightRenderingDeviceD3D11";
		constexpr std::uint8_t kVertexStage = 1;
		constexpr std::uint8_t kPixelStage = 2;
		std::atomic<std::uint64_t> g_next_device_id{1};

		/// Distinguishes recoverable device removal from other HRESULT failures.
		void CheckD3DResult(ID3D11Device *device, HRESULT result, const char *operation,
							RenderingError code = RenderingError::ResourceCreationFailed)
		{
			if (SUCCEEDED(result))
				return;
			const HRESULT reason = device ? device->GetDeviceRemovedReason() : S_OK;
			if (FAILED(reason) || result == DXGI_ERROR_DEVICE_REMOVED ||
				result == DXGI_ERROR_DEVICE_RESET || result == DXGI_ERROR_DEVICE_HUNG ||
				result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
				code = RenderingError::DeviceLost;
			std::ostringstream message;
			message << operation << " (HRESULT 0x" << std::hex
					<< static_cast<unsigned long>(result);
			if (FAILED(reason) && reason != result)
				message << ", device reason 0x" << static_cast<unsigned long>(reason);
			message << ')';
			throw RenderingException(code, "Direct3D11", message.str());
		}

		/// Maps a public format to a DXGI format and its packed texel size.
		bool MapFormat(ResourceFormat format, DXGI_FORMAT &native_format, std::uint32_t &byte_size)
		{
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				native_format = DXGI_FORMAT_R8_UNORM;
				byte_size = 1;
				return true;
			case ResourceFormat::RGBA8Unorm:
				native_format = DXGI_FORMAT_R8G8B8A8_UNORM;
				byte_size = 4;
				return true;
			case ResourceFormat::RGBA8Srgb:
				native_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				byte_size = 4;
				return true;
			case ResourceFormat::R32Float:
				native_format = DXGI_FORMAT_R32_FLOAT;
				byte_size = 4;
				return true;
			case ResourceFormat::RG32Float:
				native_format = DXGI_FORMAT_R32G32_FLOAT;
				byte_size = 8;
				return true;
			case ResourceFormat::RGB32Float:
				native_format = DXGI_FORMAT_R32G32B32_FLOAT;
				byte_size = 12;
				return true;
			case ResourceFormat::RGBA32Float:
				native_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				byte_size = 16;
				return true;
			case ResourceFormat::D24S8:
				native_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				byte_size = 4;
				return true;
			case ResourceFormat::R16Uint:
				native_format = DXGI_FORMAT_R16_UINT;
				byte_size = 2;
				return true;
			case ResourceFormat::R32Uint:
				native_format = DXGI_FORMAT_R32_UINT;
				byte_size = 4;
				return true;
			}
			return false;
		}

		/// Maps a vertex attribute's scalar format and component count.
		DXGI_FORMAT MapVertexFormat(ResourceFormat format, std::uint32_t components)
		{
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				return components == 1 ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::R16Uint:
				return components == 1 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::R32Uint:
				return components == 1 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::R32Float:
				return components == 1 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::RG32Float:
				return components == 2 ? DXGI_FORMAT_R32G32_FLOAT : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::RGB32Float:
				return components == 3 ? DXGI_FORMAT_R32G32B32_FLOAT : DXGI_FORMAT_UNKNOWN;
			case ResourceFormat::RGBA32Float:
				return components == 4 ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_UNKNOWN;
			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}

		/// Returns the byte width of a vertex attribute format.
		std::uint32_t VertexFormatSize(ResourceFormat format, std::uint32_t components)
		{
			if (MapVertexFormat(format, components) == DXGI_FORMAT_UNKNOWN)
				return 0;
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				return 1;
			case ResourceFormat::R16Uint:
				return 2;
			case ResourceFormat::R32Uint:
			case ResourceFormat::R32Float:
				return 4;
			case ResourceFormat::RG32Float:
				return 8;
			case ResourceFormat::RGB32Float:
				return 12;
			case ResourceFormat::RGBA32Float:
				return 16;
			default:
				return 0;
			}
		}

		/// Maps normalized topology to the D3D11 input-assembler topology.
		D3D11_PRIMITIVE_TOPOLOGY MapTopology(GraphicsPipelineDesc::PrimitiveTopology topology)
		{
			switch (topology)
			{
			case GraphicsPipelineDesc::PrimitiveTopology::TriangleList:
				return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			case GraphicsPipelineDesc::PrimitiveTopology::LineList:
				return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			case GraphicsPipelineDesc::PrimitiveTopology::PointList:
				return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			}
			return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}

		/// Compiles one HLSL stage and logs complete compiler diagnostics.
		ComPtr<ID3DBlob> CompileShader(const char *source, const char *entry, const char *target,
									   const char *stage)
		{
			ComPtr<ID3DBlob> bytecode;
			ComPtr<ID3DBlob> diagnostics;
			const HRESULT result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
											  entry && *entry ? entry : "main", target,
											  D3DCOMPILE_ENABLE_STRICTNESS, 0,
											  bytecode.GetAddressOf(), diagnostics.GetAddressOf());
			if (diagnostics)
			{
				const std::string message(static_cast<const char *>(diagnostics->GetBufferPointer()),
										  diagnostics->GetBufferSize());
				if (FAILED(result))
					BL_LOG(LogLevel::Error, "Direct3D11",
						   std::string(stage) + " shader compilation failed: " + message);
				else if (!message.empty())
					BL_LOG(LogLevel::Warning, "Direct3D11", message);
			}
			if (FAILED(result) || !bytecode)
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 std::string(stage) + " shader compilation failed");
			return bytecode;
		}

		/// Reflects shader constants and texture registers into a pipeline payload.
		template <typename Pipeline>
		void ReflectShader(ID3D11Device *device, ID3DBlob *bytecode, std::uint8_t stage,
						   std::vector<ComPtr<ID3D11Buffer>> &constant_buffers,
						   std::unordered_map<std::string, std::uint32_t> &constant_buffer_indices,
						   Pipeline &pipeline)
		{
			ComPtr<ID3D11ShaderReflection> reflection;
			if (FAILED(D3DReflect(bytecode->GetBufferPointer(), bytecode->GetBufferSize(),
								  __uuidof(ID3D11ShaderReflection),
								  reinterpret_cast<void **>(reflection.GetAddressOf()))))
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "shader reflection failed");
			D3D11_SHADER_DESC shader_description{};
			if (FAILED(reflection->GetDesc(&shader_description)))
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "shader reflection metadata query failed");

			for (UINT index = 0; index < shader_description.ConstantBuffers; ++index)
			{
				auto *constant_description = reflection->GetConstantBufferByIndex(index);
				D3D11_SHADER_BUFFER_DESC buffer_description{};
				if (!constant_description ||
					FAILED(constant_description->GetDesc(&buffer_description)) ||
					buffer_description.Type != D3D_CT_CBUFFER || !buffer_description.Size)
					continue;

				D3D11_SHADER_INPUT_BIND_DESC binding_description{};
				if (FAILED(reflection->GetResourceBindingDescByName(
						buffer_description.Name, &binding_description)))
					throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
											 "constant-buffer register reflection failed");

				const std::string key = std::string(buffer_description.Name) + "#" +
										std::to_string(binding_description.BindPoint);
				auto found = constant_buffer_indices.find(key);
				std::uint32_t buffer_index = 0;
				if (found == constant_buffer_indices.end())
				{
					const UINT byte_width = (buffer_description.Size + 15u) & ~15u;
					D3D11_BUFFER_DESC buffer_desc{};
					buffer_desc.ByteWidth = byte_width;
					buffer_desc.Usage = D3D11_USAGE_DEFAULT;
					buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
					ComPtr<ID3D11Buffer> buffer;
					CheckD3DResult(device, device->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf()), "constant-buffer allocation failed");
					buffer_index = static_cast<std::uint32_t>(constant_buffers.size());
					constant_buffer_indices.emplace(key, buffer_index);
					constant_buffers.push_back(std::move(buffer));
					pipeline.constant_data.emplace_back(byte_width, std::uint8_t{0});
					pipeline.constant_buffer_bindings.push_back(
						{buffer_index, binding_description.BindPoint, stage});
				}
				else
				{
					buffer_index = found->second;
					if (pipeline.constant_data[buffer_index].size() <
						static_cast<std::size_t>((buffer_description.Size + 15u) & ~15u))
						throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
												 "same constant buffer has inconsistent reflected sizes");
					auto binding = std::find_if(
						pipeline.constant_buffer_bindings.begin(), pipeline.constant_buffer_bindings.end(),
						[&](const ConstantBufferBinding &candidate)
						{ return candidate.buffer_index == buffer_index; });
					if (binding != pipeline.constant_buffer_bindings.end())
						binding->stage_mask |= stage;
				}

				for (UINT variable_index = 0; variable_index < buffer_description.Variables; ++variable_index)
				{
					auto *variable = constant_description->GetVariableByIndex(variable_index);
					D3D11_SHADER_VARIABLE_DESC variable_description{};
					if (!variable || FAILED(variable->GetDesc(&variable_description)) ||
						!variable_description.Name)
						continue;
					D3D11_SHADER_TYPE_DESC type_description{};
					auto *type = variable->GetType();
					if (type && FAILED(type->GetDesc(&type_description)))
						continue;

					const bool matrix = type_description.Class == D3D_SVC_MATRIX_ROWS ||
										type_description.Class == D3D_SVC_MATRIX_COLUMNS;
					const std::uint32_t component_count =
						matrix ? type_description.Rows * type_description.Columns
							   : type_description.Rows * type_description.Columns;
					auto existing = std::find_if(
						pipeline.uniform_bindings.begin(), pipeline.uniform_bindings.end(),
						[&](const UniformBinding &candidate)
						{
							return candidate.name == variable_description.Name &&
								   candidate.buffer_index == buffer_index;
						});
					if (existing != pipeline.uniform_bindings.end())
					{
						existing->stage_mask |= stage;
						continue;
					}
					pipeline.uniform_bindings.push_back({variable_description.Name,
														 buffer_index,
														 variable_description.StartOffset,
														 variable_description.Size,
														 binding_description.BindPoint,
														 stage,
														 component_count,
														 type_description.Type == D3D_SVT_FLOAT && type_description.Elements == 0,
														 matrix,
														 type_description.Class == D3D_SVC_MATRIX_ROWS});
				}
			}

			for (UINT index = 0; index < shader_description.BoundResources; ++index)
			{
				D3D11_SHADER_INPUT_BIND_DESC resource{};
				if (FAILED(reflection->GetResourceBindingDesc(index, &resource)) ||
					!resource.Name || resource.Type != D3D_SIT_TEXTURE)
					continue;
				auto existing = pipeline.texture_bindings.find(resource.Name);
				if (existing != pipeline.texture_bindings.end() &&
					existing->second != resource.BindPoint)
					throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
											 "same texture name uses different registers across shader stages");
				pipeline.texture_bindings[resource.Name] = resource.BindPoint;
				pipeline.texture_stage_bindings[resource.Name] |= stage;
			}
		}

		/// Resolves a texture address mode to D3D11 sampler state.
		D3D11_TEXTURE_ADDRESS_MODE MapAddressMode(TextureAddressMode mode)
		{
			switch (mode)
			{
			case TextureAddressMode::Clamp:
				return D3D11_TEXTURE_ADDRESS_CLAMP;
			case TextureAddressMode::Repeat:
				return D3D11_TEXTURE_ADDRESS_WRAP;
			case TextureAddressMode::Mirror:
				return D3D11_TEXTURE_ADDRESS_MIRROR;
			}
			return D3D11_TEXTURE_ADDRESS_CLAMP;
		}

		/// Maps independent minification and magnification filters to D3D11.
		D3D11_FILTER MapSamplerFilter(TextureFilter minification, TextureFilter magnification,
									  bool has_mipmaps)
		{
			const bool min_linear = minification == TextureFilter::Linear;
			const bool mag_linear = magnification == TextureFilter::Linear;
			if (!has_mipmaps)
			{
				if (min_linear)
					return mag_linear ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
									  : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				return mag_linear ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
								  : D3D11_FILTER_MIN_MAG_MIP_POINT;
			}
			if (min_linear)
				return mag_linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
								  : D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			return mag_linear ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
							  : D3D11_FILTER_MIN_MAG_MIP_POINT;
		}

		/// Returns a checked byte size for a texture mip level.
		std::size_t MipByteSize(std::uint32_t width, std::uint32_t height, std::uint32_t pixel_size)
		{
			if (width && static_cast<std::size_t>(height) >
							 std::numeric_limits<std::size_t>::max() / width)
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "texture dimensions overflow size_t");
			const std::size_t pixels = static_cast<std::size_t>(width) * height;
			if (pixel_size && pixels > std::numeric_limits<std::size_t>::max() / pixel_size)
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "texture byte size overflows size_t");
			return pixels * pixel_size;
		}
	}

	/// Owns D3D11 device, context, swap chain, surface views, and deferred resources.
	class Direct3D11Device::Impl
	{
	public:
		/// Routes owned and attached window messages without replacing host window data.
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
												LPARAM lparam, UINT_PTR, DWORD_PTR reference)
		{
			auto &self = *reinterpret_cast<Impl *>(reference);
			if (message == WM_CLOSE)
			{
				self.close = true;
				return self.owns_window ? 0 : DefSubclassProc(window, message, wparam, lparam);
			}
			if (message == WM_NCDESTROY)
			{
				self.close = true;
				self.width = self.height = 0;
				if (self.ui_runtime)
					self.ui_runtime->system.SetWindow(nullptr);
				ReleaseWindowInput(self.input);
			}
			if (message == WM_SIZE)
				self.resize_pending = true;
			if (message == WM_DPICHANGED && self.owns_window)
			{
				const auto &bounds = *reinterpret_cast<const RECT *>(lparam);
				SetWindowPos(window, nullptr, bounds.left, bounds.top,
							 bounds.right - bounds.left, bounds.bottom - bounds.top,
							 SWP_NOZORDER | SWP_NOACTIVATE);
				self.resize_pending = true;
				return 0;
			}
			if (self.ui_runtime)
				self.ui_runtime->system.SetWindow(window);
			if (!ForwardWindowInput(self.input, self.ui_context, self.ui_input.get(),
									window, &self.ui_keys, message, wparam, lparam))
				return 0;
			return DefSubclassProc(window, message, wparam, lparam);
		}

		/// Detects asynchronous removal before calling the immediate context.
		void CheckDevice() const
		{
			if (device)
				CheckD3DResult(device.Get(), device->GetDeviceRemovedReason(),
							   "D3D11 device is unavailable", RenderingError::DeviceLost);
		}

		/// Updates client pixels and UI density independently of swap-chain allocation.
		void SynchronizeSurface()
		{
			RECT bounds{};
			if (window && GetClientRect(window, &bounds))
			{
				const auto new_width = IsIconic(window) ? 0u : static_cast<std::uint32_t>((std::max)(0L, bounds.right - bounds.left));
				const auto new_height = IsIconic(window) ? 0u : static_cast<std::uint32_t>((std::max)(0L, bounds.bottom - bounds.top));
				resize_pending = resize_pending || width != new_width || height != new_height;
				width = new_width;
				height = new_height;
			}
			else
			{
				width = height = 0;
			}
			if (ui_context)
			{
				ui_context->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
				ui_context->SetDensityIndependentPixelRatio(
					static_cast<float>(GetWindowDpi(window)) / USER_DEFAULT_SCREEN_DPI);
			}
		}

		/// Builds matching swap-chain color and D24S8 depth views transactionally.
		void CreateSurfaceViews()
		{
			ComPtr<ID3D11Texture2D> back_buffer;
			CheckD3DResult(device.Get(), swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf())), "swap-chain GetBuffer failed");
			D3D11_TEXTURE2D_DESC depth_desc{};
			back_buffer->GetDesc(&depth_desc);
			const UINT buffer_width = depth_desc.Width;
			const UINT buffer_height = depth_desc.Height;
			ComPtr<ID3D11RenderTargetView> color;
			CheckD3DResult(device.Get(), device->CreateRenderTargetView(back_buffer.Get(), nullptr, color.GetAddressOf()), "swap-chain RTV creation failed");
			depth_desc.MipLevels = 1;
			depth_desc.ArraySize = 1;
			depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depth_desc.Usage = D3D11_USAGE_DEFAULT;
			depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depth_desc.CPUAccessFlags = 0;
			depth_desc.MiscFlags = 0;
			ComPtr<ID3D11Texture2D> depth;
			ComPtr<ID3D11DepthStencilView> view;
			CheckD3DResult(device.Get(), device->CreateTexture2D(&depth_desc, nullptr, depth.GetAddressOf()), "default depth texture creation failed");
			CheckD3DResult(device.Get(), device->CreateDepthStencilView(depth.Get(), nullptr, view.GetAddressOf()), "default depth view creation failed");
			render_target = std::move(color);
			depth_texture = std::move(depth);
			depth_view = std::move(view);
			buffer_width_ = buffer_width;
			buffer_height_ = buffer_height;
		}

		/// Unbinds swap-chain references before deferred resizing outside the WndProc.
		void ResizeSurface()
		{
			if (!device || !resize_pending || !width || !height)
				return;
			CheckDevice();
			if (width != buffer_width_ || height != buffer_height_)
			{
				context->OMSetRenderTargets(0, nullptr, nullptr);
				render_target.Reset();
				depth_view.Reset();
				depth_texture.Reset();
				CheckD3DResult(device.Get(), swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
							   "swap-chain ResizeBuffers failed", RenderingError::PresentationFailed);
			}
			if (!render_target || !depth_view)
				CreateSurfaceViews();
			resize_pending = false;
		}

		/// Creates device resources for initialization and recovery on the same HWND.
		void CreateDevice()
		{
			DXGI_SWAP_CHAIN_DESC swap_desc{};
			swap_desc.BufferCount = 2;
			swap_desc.BufferDesc.Width = (std::max)(1u, width);
			swap_desc.BufferDesc.Height = (std::max)(1u, height);
			swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swap_desc.OutputWindow = window;
			swap_desc.SampleDesc.Count = 1;
			swap_desc.Windowed = TRUE;
			swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
			CheckD3DResult(nullptr, D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requested, static_cast<UINT>(std::size(requested)), D3D11_SDK_VERSION, &swap_desc, swap_chain.GetAddressOf(), device.GetAddressOf(), &feature_level, context.GetAddressOf()), "D3D11CreateDeviceAndSwapChain failed",
						   RenderingError::DeviceCreationFailed);
			CreateSurfaceViews();
			capabilities = {};
			capabilities.max_texture_width = feature_level >= D3D_FEATURE_LEVEL_11_0 ? 16384u : 8192u;
			capabilities.max_texture_height = capabilities.max_texture_width;
			capabilities.max_render_targets = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
			capabilities.compute_shaders = feature_level >= D3D_FEATURE_LEVEL_11_0;
			capabilities.storage_buffers = feature_level >= D3D_FEATURE_LEVEL_11_0;
			for (UINT samples = 1; samples <= 32; samples *= 2)
			{
				UINT quality = 0;
				if (SUCCEEDED(device->CheckMultisampleQualityLevels(
						DXGI_FORMAT_R8G8B8A8_UNORM, samples, &quality)) &&
					quality)
					capabilities.max_samples = samples;
			}
			device_id = g_next_device_id.fetch_add(1, std::memory_order_relaxed);
			lifetime->Activate(true);
			resize_pending = false;
		}

		/// Releases every old-device reference before creating another swap chain.
		void ReleaseDevice() noexcept
		{
			if (context)
				context->ClearState();
			active_color_target.reset();
			active_depth_target.reset();
			lifetime->Shutdown();
			render_target.Reset();
			depth_view.Reset();
			depth_texture.Reset();
			swap_chain.Reset();
			for (auto &query : timer_starts)
				query.Reset();
			for (auto &query : timer_ends)
				query.Reset();
			for (auto &query : timer_disjoint)
				query.Reset();
			timer_query_pending = {};
			timer_query_active = false;
			if (context)
				context->Flush();
			context.Reset();
			device.Reset();
			buffer_width_ = buffer_height_ = 0;
		}

		DeviceConfig config{};
		InputState *input = nullptr;
		HWND window = nullptr;
		bool owns_window = false;
		bool subclassed = false;
		bool close = false;
		bool resize_pending = false;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t buffer_width_ = 0;
		std::uint32_t buffer_height_ = 0;
		D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_10_1;
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		ComPtr<IDXGISwapChain> swap_chain;
		ComPtr<ID3D11RenderTargetView> render_target;
		ComPtr<ID3D11Texture2D> depth_texture;
		ComPtr<ID3D11DepthStencilView> depth_view;
		std::shared_ptr<DeviceLifetime> lifetime = std::make_shared<DeviceLifetime>();
		std::uint64_t device_id = 0;
		::BlackLight::Engine::Rendering::DeviceCapabilities capabilities{};
		std::shared_ptr<Texture::Impl> active_color_target;
		std::shared_ptr<Texture::Impl> active_depth_target;
		std::shared_ptr<UiRuntime> ui_runtime;
		std::unique_ptr<D3D11UiRenderer> ui_renderer;
		std::unique_ptr<TextInputMethodEditor_Win32> ui_input;
		std::array<bool, 256> ui_keys{};
		Rml::Context *ui_context = nullptr;
		std::array<ComPtr<ID3D11Query>, 2> timer_starts;
		std::array<ComPtr<ID3D11Query>, 2> timer_ends;
		std::array<ComPtr<ID3D11Query>, 2> timer_disjoint;
		std::array<bool, 2> timer_query_pending{};
		std::size_t timer_query_write_index = 0;
		bool timer_query_active = false;
		bool timer_query_skipped = false;
	};

	/// Creates an uninitialized D3D11 device.
	Direct3D11Device::Direct3D11Device() : impl_(std::make_unique<Impl>())
	{
	}

	/// Releases device, deferred resources, and an owned native window.
	Direct3D11Device::~Direct3D11Device() { Shutdown(); }

	/// Initializes the D3D11 device, swap chain, default RTV, and depth target.
	void Direct3D11Device::Initialize(const DeviceConfig &config, InputState *input)
	{
		if (impl_->window)
			throw RenderingException(RenderingError::InvalidConfiguration, "Direct3D11",
									 "device is already initialized");
		if (!config.surface.width || !config.surface.height)
			throw RenderingException(RenderingError::InvalidConfiguration, "Direct3D11",
									 "surface dimensions must be non-zero");
		impl_->config = config;
		impl_->input = input;
		impl_->close = false;
		impl_->width = config.surface.width;
		impl_->height = config.surface.height;

		try
		{
			if (config.surface.mode == SurfaceMode::AttachWindow)
			{
				impl_->window = reinterpret_cast<HWND>(config.surface.native_window);
				if (!IsWindow(impl_->window))
					throw RenderingException(RenderingError::InvalidConfiguration, "Direct3D11",
											 "external HWND is invalid");
			}
			else
			{
				WNDCLASSW window_class{};
				window_class.lpfnWndProc = DefWindowProcW;
				window_class.hInstance = GetModuleHandleW(nullptr);
				window_class.lpszClassName = kWindowClass;
				window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
				if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
					throw RenderingException(RenderingError::WindowCreationFailed, "Direct3D11",
											 "RegisterClassW failed");
				const std::wstring title = RmlWin32::ConvertToUTF16(config.surface.title);
				const DWORD style = config.surface.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX);
				impl_->window = CreateWindowExW(0, kWindowClass, title.c_str(), style,
												CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
				if (!impl_->window)
					throw RenderingException(RenderingError::WindowCreationFailed, "Direct3D11",
											 "CreateWindowExW failed");
				impl_->owns_window = true;
				RECT bounds{0, 0, static_cast<LONG>(config.surface.width),
							static_cast<LONG>(config.surface.height)};
				AdjustClientBounds(bounds, style, GetWindowDpi(impl_->window));
				SetWindowPos(impl_->window, nullptr, 0, 0, bounds.right - bounds.left,
							 bounds.bottom - bounds.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
			if (!SetWindowSubclass(impl_->window, Impl::WindowProcedure,
								   reinterpret_cast<UINT_PTR>(impl_.get()), reinterpret_cast<DWORD_PTR>(impl_.get())))
				throw RenderingException(RenderingError::WindowCreationFailed, "Direct3D11",
										 "window input handler setup failed");
			impl_->subclassed = true;
			impl_->SynchronizeSurface();
			impl_->CreateDevice();
			if (config.enable_ui)
			{
				impl_->ui_runtime = UiRuntime::Acquire("Direct3D11");
				impl_->ui_runtime->system.SetWindow(impl_->window);
				impl_->ui_renderer = std::make_unique<D3D11UiRenderer>();
				impl_->ui_renderer->Initialize(impl_->device.Get());
				impl_->ui_input = std::make_unique<TextInputMethodEditor_Win32>();
				const std::string name = "blacklight-d3d11-" +
										 std::to_string(reinterpret_cast<std::uintptr_t>(impl_.get()));
				impl_->ui_context = Rml::CreateContext(name,
													   {static_cast<int>(impl_->width), static_cast<int>(impl_->height)},
													   impl_->ui_renderer.get(), impl_->ui_input.get());
				if (!impl_->ui_context)
					throw RenderingException(RenderingError::DeviceCreationFailed, "Direct3D11",
											 "RmlUi context creation failed");
				impl_->SynchronizeSurface();
			}
			if (impl_->owns_window)
				ShowWindow(impl_->window, SW_SHOW);
		}
		catch (...)
		{
			Shutdown();
			throw;
		}
	}

	/// Pumps Win32 messages and refreshes the current client dimensions.
	bool Direct3D11Device::ProcessEvents()
	{
		if (impl_->ui_runtime)
		{
			if (impl_->close)
				return false;
			impl_->ui_runtime->system.SetWindow(impl_->window);
		}
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				impl_->close = true;
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (impl_->close)
			return false;
		impl_->CheckDevice();
		impl_->SynchronizeSurface();
		impl_->ResizeSurface();
		impl_->lifetime->Flush();
		if (!impl_->width || !impl_->height)
			MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		return !impl_->close;
	}

	/// Clears and binds the swap-chain color and default depth targets.
	void Direct3D11Device::BeginFrame()
	{
		impl_->CheckDevice();
		impl_->SynchronizeSurface();
		impl_->ResizeSurface();
		if (!impl_->context || !impl_->render_target || !impl_->depth_view)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "device is not initialized");
		impl_->active_color_target.reset();
		impl_->active_depth_target.reset();
		if (!impl_->width || !impl_->height)
			return;
		ID3D11RenderTargetView *targets[] = {impl_->render_target.Get()};
		impl_->context->OMSetRenderTargets(1, targets, impl_->depth_view.Get());
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(impl_->width);
		viewport.Height = static_cast<float>(impl_->height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		impl_->context->RSSetViewports(1, &viewport);
		const float color[] = {0.2f, 0.2f, 0.2f, 1.0f};
		impl_->context->ClearRenderTargetView(impl_->render_target.Get(), color);
		impl_->context->ClearDepthStencilView(
			impl_->depth_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		impl_->CheckDevice();
	}

	/// Clears and binds an optional off-screen color and depth target pair.
	void Direct3D11Device::BeginFrame(const Texture *color_target, const Texture *depth_target)
	{
		impl_->CheckDevice();
		if (!impl_->context)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "device is not initialized");
		if (!color_target && !depth_target)
		{
			BeginFrame();
			return;
		}
		const Texture::Impl *color = color_target ? color_target->impl_.get() : nullptr;
		const Texture::Impl *depth = depth_target ? depth_target->impl_.get() : nullptr;
		auto validate = [&](const Texture::Impl *texture)
		{
			if (!texture || texture->device_id != impl_->device_id ||
				texture->backend != RenderingBackend::Direct3D11 || !texture->render_target)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "invalid or foreign render-target texture");
		};
		if (color)
		{
			validate(color);
			if (!color->tertiary)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "color texture has no render-target view");
		}
		if (depth)
		{
			if (!depth || depth->device_id != impl_->device_id ||
				depth->backend != RenderingBackend::Direct3D11 ||
				depth->format != ResourceFormat::D24S8 || !depth->render_target ||
				!depth->quaternary)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "depth texture has no depth-stencil view");
		}
		if (color && depth && (color->width != depth->width || color->height != depth->height || color->sample_count != depth->sample_count))
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "color and depth target dimensions or sample counts do not match");

		ID3D11RenderTargetView *render_target_view =
			color ? reinterpret_cast<ID3D11RenderTargetView *>(color->tertiary) : nullptr;
		auto *depth_view = depth
							   ? reinterpret_cast<ID3D11DepthStencilView *>(depth->quaternary)
							   : nullptr;
		impl_->context->OMSetRenderTargets(color ? 1u : 0u,
										   color ? &render_target_view : nullptr, depth_view);
		impl_->active_color_target = color_target ? color_target->impl_ : nullptr;
		impl_->active_depth_target = depth_target ? depth_target->impl_ : nullptr;
		const std::uint32_t width = color ? color->width : depth->width;
		const std::uint32_t height = color ? color->height : depth->height;
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		impl_->context->RSSetViewports(1, &viewport);
		const float clear_color[] = {0.2f, 0.2f, 0.2f, 1.0f};
		if (color)
			impl_->context->ClearRenderTargetView(render_target_view, clear_color);
		if (depth_view)
			impl_->context->ClearDepthStencilView(
				depth_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		impl_->CheckDevice();
	}

	/// Composes RmlUi onto the window while preserving application D3D11 bindings.
	void Direct3D11Device::RenderUi()
	{
		if (!impl_->ui_context || impl_->close)
			return;
		impl_->CheckDevice();
		impl_->SynchronizeSurface();
		impl_->ResizeSurface();
		if (!impl_->width || !impl_->height)
			return;
		impl_->ui_runtime->system.SetWindow(impl_->window);
		impl_->ui_context->Update();
		D3D11UiBindings bindings(impl_->context.Get());
		auto &renderer = impl_->ui_renderer->GetRenderer();
		renderer.SetViewport(static_cast<int>(impl_->width), static_cast<int>(impl_->height));
		renderer.BeginFrame();
		try
		{
			impl_->ui_context->Render();
		}
		catch (...)
		{
			renderer.EndFrame(impl_->render_target.Get());
			throw;
		}
		renderer.EndFrame(impl_->render_target.Get());
		impl_->CheckDevice();
	}

	/// Presents the current swap-chain buffer.
	void Direct3D11Device::Present()
	{
		if (!impl_->swap_chain)
			throw RenderingException(RenderingError::PresentationFailed, "Direct3D11",
									 "device is not initialized");
		if (impl_->close || !impl_->width || !impl_->height)
			return;
		CheckD3DResult(impl_->device.Get(),
					   impl_->swap_chain->Present(impl_->config.surface.vertical_sync ? 1 : 0, 0),
					   "swap-chain Present failed", RenderingError::PresentationFailed);
	}

	/// Recreates the native device while retaining the HWND, UI documents, and input sink.
	void Direct3D11Device::Recover()
	{
		if (!impl_->window || !IsWindow(impl_->window) || impl_->close)
			throw RenderingException(RenderingError::DeviceRecoveryFailed, "Direct3D11",
									 "recovery requires a live surface");
		BL_LOG(LogLevel::Warning, "Direct3D11", "Recreating the rendering device");
		try
		{
			ReleaseWindowInput(impl_->input);
			if (impl_->ui_renderer)
				impl_->ui_renderer->ReleaseDeviceResources();
			impl_->ReleaseDevice();
			impl_->SynchronizeSurface();
			impl_->CreateDevice();
			if (impl_->ui_renderer)
			{
				impl_->ui_renderer->Initialize(impl_->device.Get());
				Rml::ReleaseFontResources();
			}
			impl_->SynchronizeSurface();
			BL_LOG(LogLevel::Info, "Direct3D11", "Rendering device restored");
		}
		catch (const std::exception &error)
		{
			if (impl_->ui_renderer)
				impl_->ui_renderer->ReleaseDeviceResources();
			impl_->ReleaseDevice();
			throw RenderingException(RenderingError::DeviceRecoveryFailed, "Direct3D11", error.what());
		}
	}

	/// Destroys resource callbacks before releasing the D3D11 context and device.
	void Direct3D11Device::Shutdown() noexcept
	{
		if (!impl_)
			return;
		if (impl_->ui_context)
		{
			Rml::RemoveContext(impl_->ui_context->GetName());
			impl_->ui_context = nullptr;
		}
		if (impl_->ui_runtime)
			Rml::ReleaseRenderManagers();
		impl_->ui_input.reset();
		impl_->ui_renderer.reset();
		if (impl_->ui_runtime)
		{
			impl_->ui_runtime->system.SetWindow(nullptr);
			impl_->ui_runtime.reset();
		}
		impl_->ReleaseDevice();
		if (impl_->subclassed && IsWindow(impl_->window))
			RemoveWindowSubclass(impl_->window, Impl::WindowProcedure,
								 reinterpret_cast<UINT_PTR>(impl_.get()));
		impl_->subclassed = false;
		if (impl_->owns_window && impl_->window && IsWindow(impl_->window))
			DestroyWindow(impl_->window);
		impl_->window = nullptr;
		impl_->owns_window = false;
		impl_->close = true;
		impl_->width = impl_->height = 0;
		impl_->input = nullptr;
	}

	/// Returns the current client dimensions and close state.
	SurfaceState Direct3D11Device::GetSurfaceState() const noexcept
	{
		return {impl_->width, impl_->height, impl_->close};
	}

	/// Sets the host input sink.
	void Direct3D11Device::SetInputSink(InputState *input) noexcept { impl_->input = input; }

	/// Returns the D3D11 backend identity.
	RenderingBackend Direct3D11Device::GetBackend() const noexcept { return RenderingBackend::Direct3D11; }

	/// Returns the limits and optional features detected at initialization.
	DeviceCapabilities Direct3D11Device::GetCapabilities() const noexcept { return impl_->capabilities; }

	/// Returns the native window handle.
	NativeWindowHandle Direct3D11Device::GetNativeWindow() const noexcept
	{
		return reinterpret_cast<NativeWindowHandle>(impl_->window);
	}

	/// Returns the UI context when UI was enabled during initialization.
	Rml::Context *Direct3D11Device::GetUiContext() const noexcept { return impl_->ui_context; }

	/// Returns the shared Win32 platform services while UI is initialized.
	Rml::SystemInterface *Direct3D11Device::GetUiSystemInterface() const noexcept
	{
		return impl_->ui_runtime ? &impl_->ui_runtime->system : nullptr;
	}

	/// Creates a normalized command list.
	CommandList Direct3D11Device::CreateCommandList() { return {}; }

	/// Executes the ordered command queue using D3D11 resources and immediate context.
	void Direct3D11Device::Submit(CommandList &commands)
	{
		impl_->CheckDevice();
		if (!impl_->device || !impl_->context || !commands.impl_)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "device is not initialized or command list was moved from");

		auto &context = *impl_->context.Get();
		std::shared_ptr<GraphicsPipeline::Impl> pipeline;
		std::shared_ptr<Buffer::Impl> vertex_buffer;
		std::shared_ptr<Buffer::Impl> index_buffer;
		std::uint32_t vertex_stride = 0;
		std::uint32_t vertex_offset = 0;
		std::uint32_t index_offset = 0;
		ResourceFormat index_format = ResourceFormat::R16Uint;

		auto validate_owner = [&](std::uint64_t device_id, RenderingBackend backend)
		{
			if (device_id != impl_->device_id || backend != RenderingBackend::Direct3D11)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "command list contains a resource from another device");
		};
		auto bind_vertex_buffer = [&]()
		{
			if (!vertex_buffer)
				return;
			if (!vertex_stride || vertex_stride > std::numeric_limits<UINT>::max() ||
				vertex_offset > vertex_buffer->size)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "vertex-buffer binding is invalid");
			ID3D11Buffer *buffer = reinterpret_cast<ID3D11Buffer *>(vertex_buffer->native);
			const UINT stride = vertex_stride;
			const UINT offset = vertex_offset;
			context.IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
		};
		auto bind_index_buffer = [&]()
		{
			if (!index_buffer)
				return;
			ID3D11Buffer *buffer = reinterpret_cast<ID3D11Buffer *>(index_buffer->native);
			context.IASetIndexBuffer(buffer,
									 index_format == ResourceFormat::R16Uint
										 ? DXGI_FORMAT_R16_UINT
										 : DXGI_FORMAT_R32_UINT,
									 index_offset);
		};
		auto bind_pipeline = [&]()
		{
			if (!pipeline)
				return;
			context.VSSetShader(reinterpret_cast<ID3D11VertexShader *>(pipeline->native), nullptr, 0);
			context.PSSetShader(reinterpret_cast<ID3D11PixelShader *>(pipeline->auxiliary), nullptr, 0);
			context.IASetInputLayout(pipeline->backend_objects.empty()
										 ? nullptr
										 : reinterpret_cast<ID3D11InputLayout *>(pipeline->backend_objects[0]));
			context.RSSetState(reinterpret_cast<ID3D11RasterizerState *>(pipeline->rasterizer_state));
			context.OMSetBlendState(reinterpret_cast<ID3D11BlendState *>(pipeline->blend_state),
									nullptr, 0xffffffffu);
			context.OMSetDepthStencilState(
				reinterpret_cast<ID3D11DepthStencilState *>(pipeline->depth_state), 0);
			context.IASetPrimitiveTopology(MapTopology(pipeline->topology));
			for (const auto &binding : pipeline->constant_buffer_bindings)
			{
				if (binding.buffer_index >= pipeline->constant_buffer_objects.size())
					throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
											 "pipeline constant-buffer metadata is invalid");
				ID3D11Buffer *constant_buffer = reinterpret_cast<ID3D11Buffer *>(
					pipeline->constant_buffer_objects[binding.buffer_index]);
				if (binding.stage_mask & kVertexStage)
					context.VSSetConstantBuffers(binding.bind_slot, 1, &constant_buffer);
				if (binding.stage_mask & kPixelStage)
					context.PSSetConstantBuffers(binding.bind_slot, 1, &constant_buffer);
			}
			bind_vertex_buffer();
			bind_index_buffer();
		};
		auto apply_uniform = [&](const CommandList::Impl::UniformCommand &command)
		{
			if (!pipeline)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "uniform command requires a pipeline");
			bool matched = false;
			for (const auto &binding : pipeline->uniform_bindings)
			{
				if (binding.name != command.name)
					continue;
				matched = true;
				if (!binding.floating_point || binding.component_count != command.component_count ||
					binding.matrix != command.matrix)
					throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
											 "uniform type does not match the supplied value");
				if (binding.buffer_index >= pipeline->constant_data.size() ||
					binding.buffer_index >= pipeline->constant_buffer_objects.size() ||
					binding.byte_offset > pipeline->constant_data[binding.buffer_index].size() ||
					binding.byte_size > pipeline->constant_data[binding.buffer_index].size() -
											binding.byte_offset)
					throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
											 "uniform reflection range is invalid");
				auto &data = pipeline->constant_data[binding.buffer_index];
				auto *destination = data.data() + binding.byte_offset;
				if (command.matrix)
				{
					if (binding.byte_size < sizeof(float) * 16)
						throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
												 "uniform type does not match a 4x4 matrix");
					std::array<float, 16> packed{};
					for (std::size_t row = 0; row < 4; ++row)
						for (std::size_t column = 0; column < 4; ++column)
						{
							const std::size_t target = binding.row_major
														   ? row * 4 + column
														   : column * 4 + row;
							packed[target] = command.values[row * 4 + column];
						}
					std::memcpy(destination, packed.data(), sizeof(packed));
				}
				else
				{
					const std::size_t byte_count =
						static_cast<std::size_t>(command.component_count) * sizeof(float);
					if (byte_count > binding.byte_size)
						throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
												 "uniform type does not match the supplied scalar or vector");
					std::memcpy(destination, command.values.data(), byte_count);
				}

				ID3D11Buffer *constant_buffer = reinterpret_cast<ID3D11Buffer *>(
					pipeline->constant_buffer_objects[binding.buffer_index]);
				context.UpdateSubresource(constant_buffer, 0, nullptr, data.data(), 0, 0);
				for (const auto &slot : pipeline->constant_buffer_bindings)
					if (slot.buffer_index == binding.buffer_index)
					{
						if (slot.stage_mask & kVertexStage)
							context.VSSetConstantBuffers(slot.bind_slot, 1, &constant_buffer);
						if (slot.stage_mask & kPixelStage)
							context.PSSetConstantBuffers(slot.bind_slot, 1, &constant_buffer);
					}
			}
			if (!matched)
				throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
										 "uniform not found in current pipeline: " + command.name);
		};

		try
		{
			for (const auto &entry : commands.impl_->commands)
			{
				std::visit([&](const auto &command)
						   {
					using T = std::decay_t<decltype(command)>;
					if constexpr (std::is_same_v<T, CommandList::Impl::PipelineCommand>)
					{
						pipeline = command.pipeline;
						validate_owner(pipeline->device_id, pipeline->backend);
						const std::uint32_t target_samples =
							impl_->active_color_target
								? impl_->active_color_target->sample_count
								: (impl_->active_depth_target ? impl_->active_depth_target->sample_count : 1);
						if (pipeline->sample_count != target_samples)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "pipeline sample count does not match the active render target");
						bind_pipeline();
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::VertexBufferCommand>)
					{
						vertex_buffer = command.buffer;
						validate_owner(vertex_buffer->device_id, vertex_buffer->backend);
						if (vertex_buffer->usage != BufferUsage::Vertex)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "bound buffer is not a vertex buffer");
						vertex_stride = command.stride;
						vertex_offset = command.offset;
						bind_vertex_buffer();
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::IndexBufferCommand>)
					{
						index_buffer = command.buffer;
						validate_owner(index_buffer->device_id, index_buffer->backend);
						if (index_buffer->usage != BufferUsage::Index)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "bound buffer is not an index buffer");
						index_format = command.format;
						index_offset = command.offset;
						bind_index_buffer();
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::ViewportCommand>)
					{
						const auto &value = command.viewport;
						if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
							!std::isfinite(value.width) || !std::isfinite(value.height) ||
							!std::isfinite(value.minimum_depth) || !std::isfinite(value.maximum_depth) ||
							value.width < 0.0f || value.height < 0.0f ||
							value.minimum_depth < 0.0f || value.maximum_depth > 1.0f ||
							value.minimum_depth > value.maximum_depth ||
							value.x < D3D11_VIEWPORT_BOUNDS_MIN || value.y < D3D11_VIEWPORT_BOUNDS_MIN ||
							value.x + value.width > D3D11_VIEWPORT_BOUNDS_MAX ||
							value.y + value.height > D3D11_VIEWPORT_BOUNDS_MAX)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "viewport is outside D3D11 bounds");
						D3D11_VIEWPORT viewport{};
						viewport.TopLeftX = value.x;
						viewport.TopLeftY = value.y;
						viewport.Width = value.width;
						viewport.Height = value.height;
						viewport.MinDepth = value.minimum_depth;
						viewport.MaxDepth = value.maximum_depth;
						context.RSSetViewports(1, &viewport);
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::UniformCommand>)
					{
						apply_uniform(command);
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::TextureCommand>)
					{
						if (!pipeline)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "texture command requires a pipeline");
						validate_owner(command.texture->device_id, command.texture->backend);
						if (command.texture->sample_count != 1)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "multisampled textures must be resolved before sampling");
						if (command.texture == impl_->active_color_target ||
							command.texture == impl_->active_depth_target)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "an active render target cannot be sampled in the same pass");
						if (command.slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT ||
							command.slot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "texture slot exceeds D3D11 limits");
						auto binding = pipeline->texture_bindings.find(command.uniform_name);
						auto stages = pipeline->texture_stage_bindings.find(command.uniform_name);
						if (binding == pipeline->texture_bindings.end() ||
							stages == pipeline->texture_stage_bindings.end() ||
							binding->second != command.slot)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "texture uniform/register not found in current pipeline: " +
														 command.uniform_name);
						if (!command.texture->native)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "depth-only textures cannot be sampled through this API");
						ID3D11ShaderResourceView *view =
							reinterpret_cast<ID3D11ShaderResourceView *>(command.texture->native);
						ID3D11SamplerState *sampler =
							reinterpret_cast<ID3D11SamplerState *>(command.texture->auxiliary);
						if (stages->second & kVertexStage)
						{
							context.VSSetShaderResources(command.slot, 1, &view);
							context.VSSetSamplers(command.slot, 1, &sampler);
						}
						if (stages->second & kPixelStage)
						{
							context.PSSetShaderResources(command.slot, 1, &view);
							context.PSSetSamplers(command.slot, 1, &sampler);
						}
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::DrawCommand>)
					{
						if (!pipeline || !vertex_buffer || !vertex_stride ||
							vertex_offset > vertex_buffer->size)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "draw requires a pipeline and valid vertex buffer");
						const std::size_t available =
							(vertex_buffer->size - vertex_offset) / vertex_stride;
						if (command.first > available || command.count > available - command.first)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "draw range exceeds the vertex buffer");
						context.Draw(command.count, command.first);
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::DrawIndexedCommand>)
					{
						if (!pipeline || !vertex_buffer || !index_buffer ||
							vertex_offset > vertex_buffer->size || index_offset > index_buffer->size)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "indexed draw requires a pipeline and both buffers");
						const std::size_t index_size = index_format == ResourceFormat::R16Uint ? 2u : 4u;
						const std::size_t available =
							(index_buffer->size - index_offset) / index_size;
						if (command.first > available || command.count > available - command.first)
							throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
													 "indexed draw range exceeds the index buffer");
						context.DrawIndexed(command.count, command.first, command.vertex_offset);
					} }, entry);
			}
		}
		catch (const RenderingException &)
		{
			impl_->lifetime->Flush();
			throw;
		}
		catch (const std::exception &error)
		{
			impl_->lifetime->Flush();
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11", error.what());
		}

		pipeline.reset();
		vertex_buffer.reset();
		index_buffer.reset();
		impl_->CheckDevice();
		commands.Clear();
		impl_->lifetime->Flush();
	}

	/// Creates a checked D3D11 vertex, index, constant, or raw storage buffer.
	Buffer Direct3D11Device::CreateBuffer(const BufferDesc &description)
	{
		impl_->CheckDevice();
		if (!impl_->device || !impl_->context || !description.size ||
			description.size > std::numeric_limits<UINT>::max())
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "buffer size is invalid or device is not initialized");
		if (description.usage == BufferUsage::Constant &&
			(description.size % 16 != 0 || description.size > D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u))
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "constant buffer size must be 16-byte aligned and at most 64 KiB");
		if (description.usage == BufferUsage::Storage && description.size % 4 != 0)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "raw storage buffers must be a multiple of four bytes");

		D3D11_BUFFER_DESC native_desc{};
		native_desc.ByteWidth = static_cast<UINT>(description.size);
		native_desc.Usage = description.usage == BufferUsage::DynamicVertex
								? D3D11_USAGE_DYNAMIC
								: D3D11_USAGE_DEFAULT;
		if (description.usage == BufferUsage::DynamicVertex)
			native_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		switch (description.usage)
		{
		case BufferUsage::Vertex:
		case BufferUsage::DynamicVertex:
			native_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			break;
		case BufferUsage::Index:
			native_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			break;
		case BufferUsage::Constant:
			native_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			break;
		case BufferUsage::Storage:
			if (impl_->feature_level < D3D_FEATURE_LEVEL_11_0)
				throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
										 "raw storage buffers require feature level 11_0");
			native_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			native_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			break;
		}
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = description.initial_data;
		ComPtr<ID3D11Buffer> buffer;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateBuffer(&native_desc, description.initial_data ? &initial : nullptr, buffer.GetAddressOf()),
					   "ID3D11Device::CreateBuffer failed");

		ComPtr<ID3D11ShaderResourceView> storage_view;
		ComPtr<ID3D11UnorderedAccessView> storage_unordered_view;
		if (description.usage == BufferUsage::Storage)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
			view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			view_desc.BufferEx.FirstElement = 0;
			view_desc.BufferEx.NumElements = static_cast<UINT>(description.size / 4);
			view_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateShaderResourceView(buffer.Get(), &view_desc, storage_view.GetAddressOf()),
						   "raw storage-buffer view creation failed");
			D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_view_desc{};
			unordered_view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			unordered_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			unordered_view_desc.Buffer.FirstElement = 0;
			unordered_view_desc.Buffer.NumElements = static_cast<UINT>(description.size / 4);
			unordered_view_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateUnorderedAccessView(buffer.Get(), &unordered_view_desc, storage_unordered_view.GetAddressOf()),
						   "raw storage-buffer UAV creation failed");
		}

		auto resource = std::make_shared<Buffer::Impl>();
		resource->size = description.size;
		resource->usage = description.usage;
		resource->device_id = impl_->device_id;
		resource->backend = RenderingBackend::Direct3D11;
		resource->lifetime = impl_->lifetime;
		resource->native = reinterpret_cast<std::uintptr_t>(buffer.Get());
		resource->auxiliary = reinterpret_cast<std::uintptr_t>(storage_view.Get());
		resource->quaternary = reinterpret_cast<std::uintptr_t>(storage_unordered_view.Get());
		resource->deletion = impl_->lifetime->Register(
			[buffer, storage_view, storage_unordered_view]() mutable
			{
				storage_unordered_view.Reset();
				storage_view.Reset();
				buffer.Reset();
			});
		if (description.usage == BufferUsage::DynamicVertex)
		{
			resource->shadow.resize(description.size, 0);
			if (description.initial_data)
				std::memcpy(resource->shadow.data(), description.initial_data, description.size);
		}
		Buffer result;
		result.impl_ = std::move(resource);
		return result;
	}

	/// Updates a checked dynamic vertex-buffer range using WRITE_DISCARD streaming.
	void Direct3D11Device::UpdateBuffer(Buffer &buffer, std::size_t offset,
										const void *data, std::size_t size)
	{
		impl_->CheckDevice();
		if (!buffer.impl_ || !impl_->context ||
			buffer.impl_->usage != BufferUsage::DynamicVertex ||
			buffer.impl_->device_id != impl_->device_id ||
			buffer.impl_->backend != RenderingBackend::Direct3D11 ||
			offset > buffer.impl_->size || size > buffer.impl_->size - offset ||
			(size && !data))
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "dynamic vertex-buffer update range or owner is invalid");
		if (!size)
			return;
		if (buffer.impl_->shadow.size() != buffer.impl_->size)
			buffer.impl_->shadow.resize(buffer.impl_->size, 0);
		std::memcpy(buffer.impl_->shadow.data() + offset, data, size);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		auto *native = reinterpret_cast<ID3D11Buffer *>(buffer.impl_->native);
		CheckD3DResult(impl_->device.Get(), impl_->context->Map(native, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
					   "dynamic vertex-buffer mapping failed");
		std::memcpy(mapped.pData, buffer.impl_->shadow.data(), buffer.impl_->size);
		impl_->context->Unmap(native, 0);
		impl_->CheckDevice();
	}

	/// Starts a timestamp/disjoint query pair when one of two slots is available.
	void Direct3D11Device::BeginGpuTimerQuery()
	{
		impl_->CheckDevice();
		if (!impl_->context || impl_->timer_query_active)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "device is unavailable or a GPU timer query is already active");
		for (std::size_t index = 0; index < impl_->timer_query_pending.size(); ++index)
		{
			if (impl_->timer_starts[index] && impl_->timer_ends[index] && impl_->timer_disjoint[index])
				continue;
			D3D11_QUERY_DESC description{};
			description.Query = D3D11_QUERY_TIMESTAMP;
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateQuery(&description, impl_->timer_starts[index].GetAddressOf()), "GPU start-timestamp query creation failed");
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateQuery(&description, impl_->timer_ends[index].GetAddressOf()), "GPU end-timestamp query creation failed");
			description.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateQuery(&description, impl_->timer_disjoint[index].GetAddressOf()), "GPU disjoint query creation failed");
		}
		impl_->timer_query_skipped = false;
		for (std::size_t attempt = 0; attempt < impl_->timer_query_pending.size(); ++attempt)
		{
			const std::size_t index =
				(impl_->timer_query_write_index + attempt) % impl_->timer_query_pending.size();
			if (impl_->timer_query_pending[index])
				continue;
			impl_->timer_query_write_index = index;
			impl_->context->Begin(impl_->timer_disjoint[index].Get());
			impl_->context->End(impl_->timer_starts[index].Get());
			impl_->timer_query_active = true;
			return;
		}
		impl_->timer_query_skipped = true;
	}

	/// Ends the active GPU query pair and queues it for non-blocking polling.
	void Direct3D11Device::EndGpuTimerQuery()
	{
		if (!impl_->timer_query_active || !impl_->context)
			return;
		const std::size_t index = impl_->timer_query_write_index;
		impl_->context->End(impl_->timer_ends[index].Get());
		impl_->context->End(impl_->timer_disjoint[index].Get());
		impl_->timer_query_pending[index] = true;
		impl_->timer_query_write_index = (index + 1) % impl_->timer_query_pending.size();
		impl_->timer_query_active = false;
	}

	/// Returns any completed, non-disjoint GPU interval without blocking.
	bool Direct3D11Device::TryGetGpuTimeMilliseconds(double &milliseconds)
	{
		impl_->CheckDevice();
		if (!impl_->context)
			return false;
		for (std::size_t index = 0; index < impl_->timer_query_pending.size(); ++index)
		{
			if (!impl_->timer_query_pending[index])
				continue;
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
			const HRESULT disjoint_result = impl_->context->GetData(
				impl_->timer_disjoint[index].Get(), &disjoint, sizeof(disjoint),
				D3D11_ASYNC_GETDATA_DONOTFLUSH);
			CheckD3DResult(impl_->device.Get(), disjoint_result, "GPU disjoint query polling failed");
			if (disjoint_result == S_FALSE)
				continue;
			UINT64 start = 0;
			UINT64 end = 0;
			const HRESULT start_result = impl_->context->GetData(
				impl_->timer_starts[index].Get(), &start, sizeof(start),
				D3D11_ASYNC_GETDATA_DONOTFLUSH);
			const HRESULT end_result = impl_->context->GetData(
				impl_->timer_ends[index].Get(), &end, sizeof(end),
				D3D11_ASYNC_GETDATA_DONOTFLUSH);
			CheckD3DResult(impl_->device.Get(), start_result, "GPU start timestamp polling failed");
			CheckD3DResult(impl_->device.Get(), end_result, "GPU end timestamp polling failed");
			if (start_result == S_FALSE || end_result == S_FALSE)
				continue;
			impl_->timer_query_pending[index] = false;
			if (disjoint.Disjoint || !disjoint.Frequency || end < start)
				continue;
			milliseconds = static_cast<double>(end - start) * 1000.0 /
						   static_cast<double>(disjoint.Frequency);
			return true;
		}
		return false;
	}

	/// Creates a checked D3D11 2D texture, sampler, and optional RTV or DSV.
	Texture Direct3D11Device::CreateTexture(const TextureDesc &description)
	{
		impl_->CheckDevice();
		if (!impl_->device || !impl_->context)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "device is not initialized");
		if (!description.width || !description.height || !description.mip_levels ||
			description.width > impl_->capabilities.max_texture_width ||
			description.height > impl_->capabilities.max_texture_height)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "texture dimensions or mip count are invalid");

		std::uint32_t maximum_mip_levels = 1;
		for (std::uint32_t dimension = (std::max)(description.width, description.height);
			 dimension > 1; dimension >>= 1)
			++maximum_mip_levels;
		if (description.mip_levels > maximum_mip_levels)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "texture mip count exceeds its dimensions");
		if (!IsValidSampleCount(description.sample_count) ||
			description.sample_count > impl_->capabilities.max_samples)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "texture sample count is not supported");
		const bool multisampled = description.sample_count > 1;
		if (multisampled && (description.mip_levels != 1 || description.initial_data ||
							 description.initial_data_size || !description.render_target))
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "multisampled textures require one mip level, no initial data, and render_target");

		DXGI_FORMAT format{};
		std::uint32_t pixel_size = 0;
		if (!MapFormat(description.format, format, pixel_size))
			throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
									 "texture format is not supported");
		if (multisampled)
		{
			UINT quality_levels = 0;
			const HRESULT quality_result = impl_->device->CheckMultisampleQualityLevels(
				format, description.sample_count, &quality_levels);
			CheckD3DResult(impl_->device.Get(), quality_result,
						   "texture multisample query failed");
			if (!quality_levels)
				throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
										 "texture format does not support the requested sample count");
		}
		std::vector<D3D11_SUBRESOURCE_DATA> initial_subresources;
		std::size_t required_data_size = 0;
		std::uint32_t mip_width = description.width;
		std::uint32_t mip_height = description.height;
		for (std::uint32_t level = 0; level < description.mip_levels; ++level)
		{
			const std::size_t level_size = MipByteSize(mip_width, mip_height, pixel_size);
			if (required_data_size > std::numeric_limits<std::size_t>::max() - level_size)
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "texture mip data size overflows size_t");
			required_data_size += level_size;
			mip_width = (std::max)(1u, mip_width / 2);
			mip_height = (std::max)(1u, mip_height / 2);
		}
		if (description.initial_data && description.initial_data_size < required_data_size)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "initial texture data does not contain every mip level");
		if (!description.initial_data && description.initial_data_size)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "initial data size is non-zero for a null pointer");
		if (description.initial_data)
		{
			initial_subresources.resize(description.mip_levels);
			const auto *bytes = static_cast<const std::uint8_t *>(description.initial_data);
			std::size_t offset = 0;
			mip_width = description.width;
			mip_height = description.height;
			for (std::uint32_t level = 0; level < description.mip_levels; ++level)
			{
				const std::size_t level_size = MipByteSize(mip_width, mip_height, pixel_size);
				initial_subresources[level].pSysMem = bytes + offset;
				initial_subresources[level].SysMemPitch = mip_width * pixel_size;
				initial_subresources[level].SysMemSlicePitch = 0;
				offset += level_size;
				mip_width = (std::max)(1u, mip_width / 2);
				mip_height = (std::max)(1u, mip_height / 2);
			}
		}

		const bool depth_format = description.format == ResourceFormat::D24S8;
		D3D11_TEXTURE2D_DESC native_desc{};
		native_desc.Width = description.width;
		native_desc.Height = description.height;
		native_desc.MipLevels = description.mip_levels;
		native_desc.ArraySize = 1;
		native_desc.Format = format;
		native_desc.SampleDesc.Count = description.sample_count;
		native_desc.SampleDesc.Quality = 0;
		native_desc.Usage = D3D11_USAGE_DEFAULT;
		if (depth_format)
			native_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		else if (multisampled)
			native_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		else
			native_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE |
									(description.render_target ? D3D11_BIND_RENDER_TARGET : 0);
		ComPtr<ID3D11Texture2D> texture;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateTexture2D(&native_desc, initial_subresources.empty() ? nullptr : initial_subresources.data(), texture.GetAddressOf()), "ID3D11Device::CreateTexture2D failed");

		ComPtr<ID3D11ShaderResourceView> shader_view;
		ComPtr<ID3D11SamplerState> sampler;
		ComPtr<ID3D11RenderTargetView> render_target_view;
		ComPtr<ID3D11DepthStencilView> depth_view;
		if (!depth_format)
		{
			if (!multisampled)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
				view_desc.Format = format;
				view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MipLevels = description.mip_levels;
				CheckD3DResult(impl_->device.Get(), impl_->device->CreateShaderResourceView(texture.Get(), &view_desc, shader_view.GetAddressOf()),
							   "texture shader-resource view creation failed");

				D3D11_SAMPLER_DESC sampler_desc{};
				sampler_desc.Filter = MapSamplerFilter(
					description.minification_filter, description.magnification_filter,
					description.mip_levels > 1);
				sampler_desc.AddressU = MapAddressMode(description.address_u);
				sampler_desc.AddressV = MapAddressMode(description.address_v);
				sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
				sampler_desc.MaxAnisotropy = 1;
				sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
				sampler_desc.MinLOD = 0.0f;
				sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
				CheckD3DResult(impl_->device.Get(), impl_->device->CreateSamplerState(&sampler_desc, sampler.GetAddressOf()), "texture sampler creation failed");
			}
			if (description.render_target)
			{
				D3D11_RENDER_TARGET_VIEW_DESC target_desc{};
				target_desc.Format = format;
				if (multisampled)
				{
					target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					target_desc.Texture2D.MipSlice = 0;
				}
				CheckD3DResult(impl_->device.Get(), impl_->device->CreateRenderTargetView(texture.Get(), &target_desc, render_target_view.GetAddressOf()),
							   "texture render-target view creation failed");
			}
		}
		else
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC view_desc{};
			view_desc.Format = format;
			if (multisampled)
			{
				view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MipSlice = 0;
			}
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateDepthStencilView(texture.Get(), &view_desc, depth_view.GetAddressOf()),
						   "texture depth-stencil view creation failed");
		}

		auto resource = std::make_shared<Texture::Impl>();
		resource->width = description.width;
		resource->height = description.height;
		resource->mip_levels = description.mip_levels;
		resource->format = description.format;
		resource->render_target = description.render_target || depth_format;
		resource->sample_count = description.sample_count;
		resource->device_id = impl_->device_id;
		resource->backend = RenderingBackend::Direct3D11;
		resource->lifetime = impl_->lifetime;
		resource->native = reinterpret_cast<std::uintptr_t>(shader_view.Get());
		resource->auxiliary = reinterpret_cast<std::uintptr_t>(sampler.Get());
		resource->tertiary = reinterpret_cast<std::uintptr_t>(render_target_view.Get());
		resource->quaternary = reinterpret_cast<std::uintptr_t>(depth_view.Get());
		resource->resource = reinterpret_cast<std::uintptr_t>(texture.Get());
		resource->deletion = impl_->lifetime->Register(
			[texture, shader_view, sampler, render_target_view, depth_view]() mutable
			{
				depth_view.Reset();
				render_target_view.Reset();
				sampler.Reset();
				shader_view.Reset();
				texture.Reset();
			});
		Texture result;
		result.impl_ = std::move(resource);
		return result;
	}

	/// Resolves a multisampled color target into a single-sample render target.
	void Direct3D11Device::ResolveTexture(const Texture &source, const Texture &destination)
	{
		impl_->CheckDevice();
		if (!impl_->device || !impl_->context)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "device is not initialized");
		const auto *source_impl = source.impl_.get();
		const auto *destination_impl = destination.impl_.get();
		if (!source_impl || !destination_impl)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "resolve texture is empty");
		if (source_impl->device_id != impl_->device_id ||
			source_impl->backend != RenderingBackend::Direct3D11 ||
			destination_impl->device_id != impl_->device_id ||
			destination_impl->backend != RenderingBackend::Direct3D11)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "resolve textures must belong to this device");
		if (source_impl->sample_count <= 1 || !source_impl->render_target || !source_impl->resource)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "resolve source must be a multisampled render target");
		if (destination_impl->sample_count != 1 || !destination_impl->render_target ||
			!destination_impl->resource)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "resolve destination must be a single-sample render target");
		if (source_impl->format != destination_impl->format ||
			source_impl->width != destination_impl->width ||
			source_impl->height != destination_impl->height)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11",
									 "resolve textures must share a format and dimensions");
		if (source_impl->format == ResourceFormat::D24S8)
			throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
									 "depth-stencil targets cannot be resolved");
		DXGI_FORMAT format{};
		std::uint32_t pixel_size = 0;
		if (!MapFormat(source_impl->format, format, pixel_size) || !pixel_size)
			throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
									 "texture format is not supported");

		auto *context = impl_->context.Get();
		// ResolveSubresource requires that neither resource is bound for writing.
		context->OMSetRenderTargets(0, nullptr, nullptr);
		context->ResolveSubresource(
			reinterpret_cast<ID3D11Texture2D *>(destination_impl->resource), 0,
			reinterpret_cast<ID3D11Texture2D *>(source_impl->resource), 0, format);
		// Restore the render targets that were bound before the resolve.
		if (!impl_->active_color_target && !impl_->active_depth_target)
		{
			ID3D11RenderTargetView *render_target = impl_->render_target.Get();
			context->OMSetRenderTargets(1, &render_target, impl_->depth_view.Get());
		}
		else
		{
			ID3D11RenderTargetView *render_target =
				impl_->active_color_target
					? reinterpret_cast<ID3D11RenderTargetView *>(impl_->active_color_target->tertiary)
					: nullptr;
			ID3D11DepthStencilView *depth_view =
				impl_->active_depth_target
					? reinterpret_cast<ID3D11DepthStencilView *>(impl_->active_depth_target->quaternary)
					: nullptr;
			context->OMSetRenderTargets(render_target ? 1u : 0u,
										render_target ? &render_target : nullptr, depth_view);
		}
		impl_->CheckDevice();
	}

	/// Compiles HLSL stages and creates D3D11 input layout and fixed-function states.
	GraphicsPipeline Direct3D11Device::CreateGraphicsPipeline(
		const GraphicsPipelineDesc &description)
	{
		impl_->CheckDevice();
		if (!impl_->device || !impl_->context)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "device is not initialized");
		if ((description.vertex_attribute_count && !description.vertex_attributes) ||
			(description.vertex_attribute_count && !description.vertex_stride))
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "vertex attributes require a pointer and non-zero stride");
		if (!IsValidSampleCount(description.sample_count) ||
			description.sample_count > impl_->capabilities.max_samples)
			throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11",
									 "pipeline sample count is not supported");
		if (MapTopology(description.topology) == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
									 "primitive topology is invalid");

		const char *vertex_source = nullptr;
		const char *pixel_source = nullptr;
		try
		{
			vertex_source = ShaderContract::GetSource(description.vertex_shader, RenderingBackend::Direct3D11);
			pixel_source = ShaderContract::GetSource(description.fragment_shader, RenderingBackend::Direct3D11);
		}
		catch (const std::exception &error)
		{
			throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11", error.what());
		}
		const char *vertex_entry = description.vertex_shader.entry_point;
		const char *pixel_entry = description.fragment_shader.entry_point;
		ComPtr<ID3DBlob> vertex_bytecode = CompileShader(
			vertex_source, vertex_entry, "vs_4_0", "vertex");
		ComPtr<ID3DBlob> pixel_bytecode = CompileShader(
			pixel_source, pixel_entry, "ps_4_0", "pixel");

		ComPtr<ID3D11VertexShader> vertex_shader;
		ComPtr<ID3D11PixelShader> pixel_shader;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateVertexShader(vertex_bytecode->GetBufferPointer(), vertex_bytecode->GetBufferSize(), nullptr, vertex_shader.GetAddressOf()), "vertex shader object creation failed");
		CheckD3DResult(impl_->device.Get(), impl_->device->CreatePixelShader(pixel_bytecode->GetBufferPointer(), pixel_bytecode->GetBufferSize(), nullptr, pixel_shader.GetAddressOf()), "pixel shader object creation failed");

		std::vector<D3D11_INPUT_ELEMENT_DESC> input_elements;
		std::vector<std::string> semantic_names;
		input_elements.reserve(description.vertex_attribute_count);
		semantic_names.reserve(description.vertex_attribute_count);
		for (std::uint32_t index = 0; index < description.vertex_attribute_count; ++index)
		{
			const auto &attribute = description.vertex_attributes[index];
			const DXGI_FORMAT format = MapVertexFormat(attribute.format, attribute.component_count);
			const std::uint32_t byte_size = VertexFormatSize(attribute.format, attribute.component_count);
			if (format == DXGI_FORMAT_UNKNOWN || !byte_size ||
				attribute.offset > description.vertex_stride ||
				byte_size > description.vertex_stride - attribute.offset)
				throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
										 "vertex attribute layout is invalid");
			for (std::uint32_t previous = 0; previous < index; ++previous)
				if (description.vertex_attributes[previous].semantic_index == attribute.semantic_index &&
					description.vertex_attributes[previous].semantic_name &&
					attribute.semantic_name &&
					std::strcmp(description.vertex_attributes[previous].semantic_name,
								attribute.semantic_name) == 0)
					throw RenderingException(RenderingError::ResourceCreationFailed, "Direct3D11",
											 "vertex semantics must be unique");
			semantic_names.push_back(
				attribute.semantic_name && *attribute.semantic_name
					? attribute.semantic_name
					: "ATTRIBUTE" + std::to_string(attribute.location));
			D3D11_INPUT_ELEMENT_DESC element{};
			element.SemanticName = semantic_names.back().c_str();
			element.SemanticIndex = attribute.semantic_index;
			element.Format = format;
			element.InputSlot = 0;
			element.AlignedByteOffset = attribute.offset;
			element.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			element.InstanceDataStepRate = 0;
			input_elements.push_back(element);
		}

		ComPtr<ID3D11InputLayout> input_layout;
		if (!input_elements.empty())
			CheckD3DResult(impl_->device.Get(), impl_->device->CreateInputLayout(input_elements.data(), static_cast<UINT>(input_elements.size()), vertex_bytecode->GetBufferPointer(), vertex_bytecode->GetBufferSize(), input_layout.GetAddressOf()), "input layout does not match the vertex shader signature");

		D3D11_RASTERIZER_DESC raster_desc{};
		raster_desc.FillMode = D3D11_FILL_SOLID;
		raster_desc.CullMode = description.cull_mode == GraphicsPipelineDesc::CullMode::None
								   ? D3D11_CULL_NONE
							   : description.cull_mode == GraphicsPipelineDesc::CullMode::Front
								   ? D3D11_CULL_FRONT
								   : D3D11_CULL_BACK;
		raster_desc.FrontCounterClockwise = FALSE;
		raster_desc.DepthClipEnable = TRUE;
		raster_desc.MultisampleEnable = description.sample_count > 1 ? TRUE : FALSE;
		ComPtr<ID3D11RasterizerState> raster_state;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateRasterizerState(&raster_desc, raster_state.GetAddressOf()), "rasterizer-state creation failed");

		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		auto &render_target_blend = blend_desc.RenderTarget[0];
		render_target_blend.BlendEnable = description.alpha_blending;
		render_target_blend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		render_target_blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		render_target_blend.BlendOp = D3D11_BLEND_OP_ADD;
		render_target_blend.SrcBlendAlpha = D3D11_BLEND_ONE;
		render_target_blend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		render_target_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		render_target_blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		ComPtr<ID3D11BlendState> blend_state;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateBlendState(&blend_desc, blend_state.GetAddressOf()), "blend-state creation failed");

		D3D11_DEPTH_STENCIL_DESC depth_desc{};
		depth_desc.DepthEnable = description.depth_test;
		depth_desc.DepthWriteMask = description.depth_write
										? D3D11_DEPTH_WRITE_MASK_ALL
										: D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depth_desc.StencilEnable = description.depth_format == ResourceFormat::D24S8;
		depth_desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		depth_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		depth_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		depth_desc.BackFace = depth_desc.FrontFace;
		ComPtr<ID3D11DepthStencilState> depth_state;
		CheckD3DResult(impl_->device.Get(), impl_->device->CreateDepthStencilState(&depth_desc, depth_state.GetAddressOf()), "depth-stencil-state creation failed");

		std::vector<ComPtr<ID3D11Buffer>> constant_buffers;
		std::unordered_map<std::string, std::uint32_t> constant_buffer_indices;
		auto resource = std::make_shared<GraphicsPipeline::Impl>();
		ReflectShader(impl_->device.Get(), vertex_bytecode.Get(), kVertexStage,
					  constant_buffers, constant_buffer_indices, *resource);
		ReflectShader(impl_->device.Get(), pixel_bytecode.Get(), kPixelStage,
					  constant_buffers, constant_buffer_indices, *resource);

		resource->vertex_stride = description.vertex_stride;
		resource->color_format = description.color_format;
		resource->depth_format = description.depth_format;
		resource->depth_test = description.depth_test;
		resource->depth_write = description.depth_write;
		resource->alpha_blending = description.alpha_blending;
		resource->cull_mode = description.cull_mode;
		resource->topology = description.topology;
		resource->sample_count = description.sample_count;
		resource->srgb = description.srgb || description.color_format == ResourceFormat::RGBA8Srgb;
		resource->vertex_source = vertex_source;
		resource->fragment_source = pixel_source;
		resource->entry_point = vertex_entry ? vertex_entry : "main";
		resource->device_id = impl_->device_id;
		resource->backend = RenderingBackend::Direct3D11;
		resource->lifetime = impl_->lifetime;
		resource->native = reinterpret_cast<std::uintptr_t>(vertex_shader.Get());
		resource->auxiliary = reinterpret_cast<std::uintptr_t>(pixel_shader.Get());
		resource->backend_objects.push_back(reinterpret_cast<std::uintptr_t>(input_layout.Get()));
		resource->rasterizer_state = reinterpret_cast<std::uintptr_t>(raster_state.Get());
		resource->blend_state = reinterpret_cast<std::uintptr_t>(blend_state.Get());
		resource->depth_state = reinterpret_cast<std::uintptr_t>(depth_state.Get());
		resource->constant_buffer_objects.reserve(constant_buffers.size());
		for (const auto &buffer : constant_buffers)
			resource->constant_buffer_objects.push_back(reinterpret_cast<std::uintptr_t>(buffer.Get()));

		resource->deletion = impl_->lifetime->Register(
			[vertex_shader, pixel_shader, input_layout, raster_state, blend_state,
			 depth_state, constant_buffers]() mutable
			{
				constant_buffers.clear();
				depth_state.Reset();
				blend_state.Reset();
				raster_state.Reset();
				input_layout.Reset();
				pixel_shader.Reset();
				vertex_shader.Reset();
			});
		GraphicsPipeline result;
		result.impl_ = std::move(resource);
		return result;
	}
}
