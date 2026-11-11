#include "engine/Ecs.h"

#include <stdexcept>

namespace BlackLight::Engine
{

	Entity World::CreateEntity()
	{
		std::uint32_t index = 0;
		if (!free_indices_.empty())
		{
			index = free_indices_.back();
			free_indices_.pop_back();
		}
		else
		{
			index = static_cast<std::uint32_t>(generations_.size());
			generations_.push_back(0);
		alive_.push_back(0);
		}

		const Entity entity{index, generations_[index]};
	alive_[index] = 1;
		live_entities_.push_back(entity);
		return entity;
	}

	void World::DestroyEntity(Entity entity)
	{
		if (!IsAlive(entity))
		{
			return;
		}

		for (auto &pool : component_pools_)
		{
			pool.second->Remove(entity);
		}

		const auto it = std::find(live_entities_.begin(), live_entities_.end(), entity);
		if (it != live_entities_.end())
		{
			*it = live_entities_.back();
			live_entities_.pop_back();
		}

		++generations_[entity.index];
	alive_[entity.index] = 0;
		free_indices_.push_back(entity.index);
	}

	bool World::IsAlive(Entity entity) const noexcept
	{
	return entity.index < generations_.size() &&
		alive_[entity.index] != 0 &&
		generations_[entity.index] == entity.generation;
	}

	void World::Clear()
	{
		component_pools_.clear();
		free_indices_.clear();
		for (std::uint32_t index = 0; index < generations_.size(); ++index)
		{
			if (alive_[index] != 0)
			{
				++generations_[index];
			}
			alive_[index] = 0;
			free_indices_.push_back(index);
		}
		live_entities_.clear();
	}

	std::size_t World::GetEntityCount() const noexcept
	{
		return live_entities_.size();
	}

} // namespace BlackLight::Engine
