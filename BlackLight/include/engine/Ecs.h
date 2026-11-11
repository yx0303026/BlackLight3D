#pragma once

#include "../BlackLightApi.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BlackLight::Engine
{

	/// Identifies an entity with an index and a generation counter.
	struct Entity
	{
		std::uint32_t index = 0;
		std::uint32_t generation = 0;

		/// Returns whether two entity handles identify the same lifetime.
		friend constexpr bool operator==(Entity left, Entity right) noexcept
		{
			return left.index == right.index && left.generation == right.generation;
		}

		/// Returns whether two entity handles identify different lifetimes.
		friend constexpr bool operator!=(Entity left, Entity right) noexcept
		{
			return !(left == right);
		}
	};

	/// Hashes an entity handle for use in unordered containers.
	struct EntityHash
	{
		/// Computes a stable hash from the index and generation.
		std::size_t operator()(Entity entity) const noexcept
		{
			return (static_cast<std::size_t>(entity.index) << 32u) ^
				   static_cast<std::size_t>(entity.generation);
		}
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Owns entities and type-indexed component pools.
	class BLACKLIGHT_API World final
	{
	public:
		/// Creates an empty world with no live entities.
		World() = default;

		/// Destroys all entities and their component storage.
		~World() = default;

		World(const World &) = delete;
		World &operator=(const World &) = delete;
		World(World &&) = delete;
		World &operator=(World &&) = delete;

		/// Creates a new entity, reusing a released slot when possible.
		Entity CreateEntity();

		/// Destroys an entity and removes every component attached to it.
		void DestroyEntity(Entity entity);

		/// Returns whether an entity is alive in this world.
		bool IsAlive(Entity entity) const noexcept;

		/// Removes all entities and releases their component data.
		void Clear();

		/// Returns the number of currently live entities.
		std::size_t GetEntityCount() const noexcept;

		/// Adds or replaces a component and returns the stored value.
		template <typename Component, typename... Arguments>
		Component &Add(Entity entity, Arguments &&...arguments);

		/// Removes a component if one is attached to the entity.
		template <typename Component>
		void Remove(Entity entity);

		/// Returns a mutable component pointer, or nullptr when absent.
		template <typename Component>
		Component *Get(Entity entity);

		/// Returns a read-only component pointer, or nullptr when absent.
		template <typename Component>
		const Component *Get(Entity entity) const;

		/// Returns whether an entity has a component of the requested type.
		template <typename Component>
		bool Has(Entity entity) const;

		/// Iterates all live entities containing every requested component type.
		template <typename... Components, typename Function>
		void Each(Function &&function);

	private:
		using ComponentType = std::type_index;

		struct ComponentPoolBase
		{
			/// Removes one entity from a type-erased component pool.
			virtual ~ComponentPoolBase() = default;
			virtual void Remove(Entity entity) = 0;
		};

		template <typename Component>
		struct ComponentPool final : ComponentPoolBase
		{
			std::unordered_map<Entity, Component, EntityHash> values;

			/// Removes one entity from this concrete component pool.
			void Remove(Entity entity) override
			{
				values.erase(entity);
			}
		};

		/// Returns or creates the concrete component pool for a type.
		template <typename Component>
		ComponentPool<Component> &GetPool();

		/// Returns an existing concrete component pool for a type, if present.
		template <typename Component>
		const ComponentPool<Component> *FindPool() const;

		std::vector<std::uint32_t> generations_;
		std::vector<std::uint8_t> alive_;
		std::vector<std::uint32_t> free_indices_;
		std::vector<Entity> live_entities_;
		std::unordered_map<ComponentType, std::unique_ptr<ComponentPoolBase>> component_pools_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

	template <typename Component, typename... Arguments>
	Component &World::Add(Entity entity, Arguments &&...arguments)
	{
		if (!IsAlive(entity))
		{
			throw std::invalid_argument("Cannot add a component to a dead entity.");
		}
		auto &pool = GetPool<Component>();
		auto it = pool.values.find(entity);
		if (it == pool.values.end())
		{
			auto inserted = pool.values.emplace(
				entity, Component{std::forward<Arguments>(arguments)...});
			return inserted.first->second;
		}
		it->second = Component{std::forward<Arguments>(arguments)...};
		return it->second;
	}

	template <typename Component>
	void World::Remove(Entity entity)
	{
		auto *pool = const_cast<ComponentPool<Component> *>(FindPool<Component>());
		if (pool != nullptr)
		{
			pool->Remove(entity);
		}
	}

	template <typename Component>
	Component *World::Get(Entity entity)
	{
		auto *pool = const_cast<ComponentPool<Component> *>(FindPool<Component>());
		if (pool == nullptr)
		{
			return nullptr;
		}
		auto it = pool->values.find(entity);
		return it == pool->values.end() ? nullptr : &it->second;
	}

	template <typename Component>
	const Component *World::Get(Entity entity) const
	{
		const auto *pool = FindPool<Component>();
		if (pool == nullptr)
		{
			return nullptr;
		}
		auto it = pool->values.find(entity);
		return it == pool->values.end() ? nullptr : &it->second;
	}

	template <typename Component>
	bool World::Has(Entity entity) const
	{
		return Get<Component>(entity) != nullptr;
	}

	template <typename... Components, typename Function>
	void World::Each(Function &&function)
	{
		for (const Entity entity : live_entities_)
		{
			if ((Has<Components>(entity) && ...))
			{
				std::invoke(std::forward<Function>(function), entity, *Get<Components>(entity)...);
			}
		}
	}

	template <typename Component>
	World::ComponentPool<Component> &World::GetPool()
	{
		const ComponentType type = std::type_index(typeid(Component));
		auto it = component_pools_.find(type);
		if (it == component_pools_.end())
		{
			auto pool = std::make_unique<ComponentPool<Component>>();
			auto *result = pool.get();
			component_pools_.emplace(type, std::move(pool));
			return *result;
		}
		return *static_cast<ComponentPool<Component> *>(it->second.get());
	}

	template <typename Component>
	const World::ComponentPool<Component> *World::FindPool() const
	{
		const auto it = component_pools_.find(std::type_index(typeid(Component)));
		if (it == component_pools_.end())
		{
			return nullptr;
		}
		return static_cast<const ComponentPool<Component> *>(it->second.get());
	}

} // namespace BlackLight::Engine
