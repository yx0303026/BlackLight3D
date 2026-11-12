#pragma once

#include "engine/rendering_device/RenderingResources.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BlackLight::Engine::Rendering::Detail
{
	/// Makes a backend destruction callback idempotent across resource and device teardown.
	class DeviceDeletion final
	{
	public:
		/// Creates a token for one backend object's destruction.
		explicit DeviceDeletion(std::function<void()> callback) : callback_(std::move(callback)) {}
		/// Executes any backend callback not already run or disarmed.
		~DeviceDeletion() { Destroy(); }

		/// Executes the backend callback once.
		void Destroy() noexcept
		{
			std::function<void()> callback;
			{
				std::scoped_lock lock(mutex_);
				callback = std::move(callback_);
			}
			if (callback)
			{
				try
				{
					callback();
				}
				catch (...)
				{
				}
			}
		}

		/// Marks the resource for device-thread destruction.
		void Retire() noexcept { retired_.store(true, std::memory_order_release); }

		/// Reports whether the resource has been released by its owner.
		bool IsRetired() const noexcept { return retired_.load(std::memory_order_acquire); }

		/// Reports whether the destruction callback has already run.
		bool IsDestroyed() const noexcept
		{
			std::scoped_lock lock(mutex_);
			return !callback_;
		}

	private:
		/// Disarms a callback when registry allocation fails and the caller owns cleanup.
		void Abandon() noexcept
		{
			std::scoped_lock lock(mutex_);
			callback_ = {};
		}

		friend class DeviceLifetime;
		mutable std::mutex mutex_;
		std::function<void()> callback_;
		std::atomic<bool> retired_{false};
	};

	/// Defers backend destruction until the owning device has a valid destruction context.
	class DeviceLifetime final
	{
	public:
		/// Marks the device alive and allows immediate destruction callbacks.
		void Activate(bool defer_destruction) noexcept
		{
			std::scoped_lock lock(mutex_);
			alive_ = true;
			defer_destruction_ = defer_destruction;
		}

		/// Registers an object so device shutdown can destroy it before native teardown.
		std::shared_ptr<DeviceDeletion> Register(std::function<void()> callback)
		{
			auto deletion = std::make_shared<DeviceDeletion>(std::move(callback));
			std::scoped_lock lock(mutex_);
			if (alive_)
			{
				live_.erase(std::remove_if(live_.begin(), live_.end(),
										   [](const auto &entry)
										   { return !entry || entry->IsDestroyed(); }),
							live_.end());
				try
				{
					live_.push_back(deletion);
				}
				catch (...)
				{
					deletion->Abandon();
					throw;
				}
			}
			return deletion;
		}

		/// Defers a registered resource's destruction to the owning device thread.
		void Retire(const std::shared_ptr<DeviceDeletion> &deletion) noexcept
		{
			if (!deletion)
				return;
			deletion->Retire();
			bool execute_now = false;
			{
				std::scoped_lock lock(mutex_);
				if (!alive_)
					return;
				execute_now = !defer_destruction_;
			}
			if (execute_now)
				deletion->Destroy();
		}

		/// Executes all queued destruction callbacks on the device thread.
		void Flush() noexcept
		{
			std::scoped_lock lock(mutex_);
			for (const auto &callback : live_)
				if (callback && callback->IsRetired())
					callback->Destroy();
			live_.erase(std::remove_if(live_.begin(), live_.end(),
									   [](const auto &callback)
									   { return !callback || callback->IsDestroyed(); }),
						live_.end());
		}

		/// Destroys all registered resources before disabling the native device.
		void Shutdown() noexcept
		{
			std::vector<std::shared_ptr<DeviceDeletion>> callbacks;
			{
				std::scoped_lock lock(mutex_);
				alive_ = false;
				callbacks.swap(live_);
			}
			for (const auto &callback : callbacks)
				if (callback)
					callback->Destroy();
		}

	private:
		std::mutex mutex_;
		bool alive_ = false;
		bool defer_destruction_ = true;
		std::vector<std::shared_ptr<DeviceDeletion>> live_;
	};

	/// Maps a reflected uniform to a D3D11 constant-buffer byte range.
	struct UniformBinding
	{
		std::string name;
		std::uint32_t buffer_index = 0;
		std::uint32_t byte_offset = 0;
		std::uint32_t byte_size = 0;
		std::uint32_t bind_slot = 0;
		std::uint8_t stage_mask = 0;
		std::uint32_t component_count = 0;
		bool floating_point = false;
		bool matrix = false;
		bool row_major = false;
	};

	/// Describes a reflected constant buffer slot for a shader stage.
	struct ConstantBufferBinding
	{
		std::uint32_t buffer_index = 0;
		std::uint32_t bind_slot = 0;
		std::uint8_t stage_mask = 0;
	};

	/// Stores backend-neutral metadata and a deferred destruction callback.
	struct BufferPayload
	{
		std::size_t size = 0;
		BufferUsage usage = BufferUsage::Vertex;
		std::uint64_t device_id = 0;
		RenderingBackend backend{};
		std::weak_ptr<DeviceLifetime> lifetime;
		std::shared_ptr<DeviceDeletion> deletion;
		std::uintptr_t native = 0;
		std::uintptr_t auxiliary = 0;
		std::uintptr_t quaternary = 0;
		std::uintptr_t tertiary = 0;
		std::vector<std::uint8_t> shadow;

		/// Defers or performs backend destruction exactly once.
		~BufferPayload()
		{
			if (auto owner = lifetime.lock())
				owner->Retire(deletion);
			else if (deletion)
				deletion->Destroy();
		}
	};

	/// Stores backend-neutral texture metadata and a deferred destruction callback.
	struct TexturePayload
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t mip_levels = 1;
		ResourceFormat format = ResourceFormat::RGBA8Unorm;
		bool render_target = false;
		std::uint64_t device_id = 0;
		RenderingBackend backend{};
		std::weak_ptr<DeviceLifetime> lifetime;
		std::shared_ptr<DeviceDeletion> deletion;
		std::uintptr_t native = 0;
		std::uintptr_t auxiliary = 0;
		std::uintptr_t tertiary = 0;
		std::uintptr_t quaternary = 0;

		/// Defers or performs backend destruction exactly once.
		~TexturePayload()
		{
			if (auto owner = lifetime.lock())
				owner->Retire(deletion);
			else if (deletion)
				deletion->Destroy();
		}
	};

	/// Stores backend-neutral pipeline metadata and a deferred destruction callback.
	struct PipelinePayload
	{
		std::uint32_t vertex_stride = 0;
		ResourceFormat color_format = ResourceFormat::RGBA8Unorm;
		ResourceFormat depth_format = ResourceFormat::D24S8;
		bool depth_test = true;
		bool depth_write = true;
		bool alpha_blending = false;
		GraphicsPipelineDesc::CullMode cull_mode = GraphicsPipelineDesc::CullMode::None;
		GraphicsPipelineDesc::PrimitiveTopology topology = GraphicsPipelineDesc::PrimitiveTopology::TriangleList;
		std::uint32_t sample_count = 1;
		bool srgb = false;
		std::string vertex_source;
		std::string fragment_source;
		std::string entry_point;
		std::vector<VertexAttribute> attributes;
		std::vector<std::string> attribute_semantics;
		std::unordered_map<std::string, std::int32_t> uniform_locations;
		std::unordered_map<std::string, std::uint32_t> uniform_types;
		std::vector<std::uintptr_t> backend_objects;
		std::vector<std::vector<std::uint8_t>> constant_data;
		std::vector<UniformBinding> uniform_bindings;
		std::unordered_map<std::string, std::uint32_t> texture_bindings;
		std::unordered_map<std::string, std::uint8_t> texture_stage_bindings;
		std::vector<ConstantBufferBinding> constant_buffer_bindings;
		std::vector<std::uintptr_t> constant_buffer_objects;
		std::uintptr_t blend_state = 0;
		std::uintptr_t rasterizer_state = 0;
		std::uintptr_t depth_state = 0;
		std::uint64_t device_id = 0;
		RenderingBackend backend{};
		std::weak_ptr<DeviceLifetime> lifetime;
		std::shared_ptr<DeviceDeletion> deletion;
		std::uintptr_t native = 0;
		std::uintptr_t auxiliary = 0;

		/// Defers or performs backend destruction exactly once.
		~PipelinePayload()
		{
			if (auto owner = lifetime.lock())
				owner->Retire(deletion);
			else if (deletion)
				deletion->Destroy();
		}
	};
}

namespace BlackLight::Engine::Rendering
{
	/// Defines the private buffer implementation shared by device backends.
	class Buffer::Impl final : public Detail::BufferPayload {};
	/// Defines the private texture implementation shared by device backends.
	class Texture::Impl final : public Detail::TexturePayload {};
	/// Defines the private pipeline implementation shared by device backends.
	class GraphicsPipeline::Impl final : public Detail::PipelinePayload {};
}
