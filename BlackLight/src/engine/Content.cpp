#include "engine/Content.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace BlackLight::Engine
{

	ContentManager::ContentManager(std::filesystem::path root_directory)
		: root_directory_(std::move(root_directory)) {}

	void ContentManager::SetRootDirectory(std::filesystem::path root_directory)
	{
		std::scoped_lock lock(mutex_);
		root_directory_ = std::move(root_directory);
		binary_cache_.clear();
		text_cache_.clear();
	}

	const std::filesystem::path &ContentManager::GetRootDirectory() const noexcept
	{
		return root_directory_;
	}

	std::shared_ptr<const std::vector<std::uint8_t>> ContentManager::LoadBinary(
		const std::filesystem::path &relative_path)
	{
		std::scoped_lock lock(mutex_);
		const std::string key = MakeKey(relative_path);
		if (const auto cached = binary_cache_.find(key); cached != binary_cache_.end())
		{
			return cached->second;
		}

		const std::filesystem::path path = ResolvePath(relative_path);
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("Unable to open content asset: " + path.string());
		}

		auto bytes = std::make_shared<std::vector<std::uint8_t>>(
			std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		auto result = std::shared_ptr<const std::vector<std::uint8_t>>(std::move(bytes));
		binary_cache_.emplace(key, result);
		return result;
	}

	std::shared_ptr<const std::string> ContentManager::LoadText(
		const std::filesystem::path &relative_path)
	{
		std::scoped_lock lock(mutex_);
		const std::string key = MakeKey(relative_path);
		if (const auto cached = text_cache_.find(key); cached != text_cache_.end())
		{
			return cached->second;
		}

		const std::filesystem::path path = ResolvePath(relative_path);
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("Unable to open content asset: " + path.string());
		}

		auto text = std::make_shared<std::string>(
			std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		auto result = std::shared_ptr<const std::string>(std::move(text));
		text_cache_.emplace(key, result);
		return result;
	}

	void ContentManager::Unload(const std::filesystem::path &relative_path)
	{
		std::scoped_lock lock(mutex_);
		const std::string key = MakeKey(relative_path);
		binary_cache_.erase(key);
		text_cache_.erase(key);
	}

	void ContentManager::UnloadAll()
	{
		std::scoped_lock lock(mutex_);
		binary_cache_.clear();
		text_cache_.clear();
	}

	std::string ContentManager::MakeKey(const std::filesystem::path &relative_path) const
	{
		return ResolvePath(relative_path).generic_string();
	}

	std::filesystem::path ContentManager::ResolvePath(const std::filesystem::path &relative_path) const
	{
		if (relative_path.empty() || relative_path.has_root_path())
		{
			throw std::invalid_argument("Content paths must be non-empty relative paths.");
		}
		const auto normalized = relative_path.lexically_normal();
		for (const auto &component : normalized)
		{
			if (component == "..")
			{
				throw std::invalid_argument("Content paths cannot escape the content root.");
			}
		}
		return (root_directory_ / normalized).lexically_normal();
	}

} // namespace BlackLight::Engine
