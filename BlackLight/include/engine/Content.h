#pragma once

#include "../BlackLightApi.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace BlackLight::Engine
{

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Owns a rooted content directory and caches immutable binary assets.
	class BLACKLIGHT_API ContentManager final
	{
	public:
		/// Creates a content manager rooted at the supplied directory.
		explicit ContentManager(std::filesystem::path root_directory = {});

		/// Changes the root directory and clears all cached assets.
		void SetRootDirectory(std::filesystem::path root_directory);

		/// Returns the normalized root directory.
		const std::filesystem::path &GetRootDirectory() const noexcept;

		/// Loads a binary asset and returns shared immutable storage for its bytes.
		std::shared_ptr<const std::vector<std::uint8_t>> LoadBinary(const std::filesystem::path &relative_path);

		/// Loads a UTF-8 text asset and returns its contents.
		std::shared_ptr<const std::string> LoadText(const std::filesystem::path &relative_path);

		/// Removes one cached asset so the next load observes the filesystem.
		void Unload(const std::filesystem::path &relative_path);

		/// Clears every cached asset.
		void UnloadAll();

	private:
		/// Converts a relative content path into a stable cache key.
		std::string MakeKey(const std::filesystem::path &relative_path) const;

		/// Resolves and validates a relative path beneath the content root.
		std::filesystem::path ResolvePath(const std::filesystem::path &relative_path) const;

		std::filesystem::path root_directory_;
		std::unordered_map<std::string, std::shared_ptr<const std::vector<std::uint8_t>>> binary_cache_;
		std::unordered_map<std::string, std::shared_ptr<const std::string>> text_cache_;
		mutable std::mutex mutex_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
