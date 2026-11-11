#pragma once

#include "editor/core/EditorConfig.h"
#include "editor/core/EditorEvent.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one project shown by the recent-projects list.
	struct RecentProject
	{
		std::string name;
		/// Path to the `.blacklight` project file.
		std::string path;
		std::string kind;
		/// ISO timestamp of the last successful open or creation.
		std::string opened;
	};

	/// Maintains the most-recently-used project list and persists it through EditorConfig.
	class RecentProjects final
	{
	public:
		/// Creates a list capped at `limit` entries; `events` receives change notifications.
		explicit RecentProjects(std::size_t limit = 10, EventBus *events = nullptr);

		/// Replaces the list from the `[recent_projects]` configuration section.
		void Load(const EditorConfig &config);
		/// Writes the list to the `[recent_projects]` configuration section.
		void Save(EditorConfig &config) const;

		/// Inserts an entry at the front, removes duplicates, and trims to the limit.
		/// Returns false for an empty path or a zero limit.
		bool Add(RecentProject entry);
		/// Removes one path; returns whether it was present.
		bool Remove(std::string_view path);
		/// Removes every entry.
		void Clear();

		/// Returns entries newest first.
		const std::vector<RecentProject> &GetEntries() const noexcept;
		/// Returns the maximum number of retained entries.
		std::size_t GetLimit() const noexcept;
		/// Changes the limit and trims the list.
		void SetLimit(std::size_t limit);

	private:
		void NotifyChanged() const;

		std::vector<RecentProject> entries_;
		std::size_t limit_ = 10;
		EventBus *events_ = nullptr;
	};

} // namespace BlackLight::Editor
