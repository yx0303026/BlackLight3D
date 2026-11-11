#include "editor/project/RecentProjects.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		constexpr const char *Section = "recent_projects";

		/// Lowercases ASCII text for case-insensitive path comparison.
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return text;
		}

		/// Returns whether two paths refer to the same entry on Windows.
		bool SamePath(std::string_view left, std::string_view right)
		{
			if (left.size() != right.size())
				return false;
			return std::equal(left.begin(), left.end(), right.begin(),
				[](char first, char second)
				{
					return std::tolower(static_cast<unsigned char>(first)) ==
						std::tolower(static_cast<unsigned char>(second));
				});
		}

		/// Builds a configuration key for one entry field.
		std::string Key(std::size_t index, const char *field)
		{
			return "item." + std::to_string(index) + "." + field;
		}

	} // namespace

	RecentProjects::RecentProjects(std::size_t limit, EventBus *events)
		: limit_(limit), events_(events)
	{
	}

	void RecentProjects::Load(const EditorConfig &config)
	{
		entries_.clear();
		const int count = config.GetInt(Section, "count", 0);
		for (int index = 0; index < count; ++index)
		{
			if (entries_.size() >= limit_)
				break;
			const std::size_t position = static_cast<std::size_t>(index);
			RecentProject entry;
			entry.path = config.Get(Section, Key(position, "path"));
			if (entry.path.empty())
				continue;
			entry.name = config.Get(Section, Key(position, "name"));
			if (entry.name.empty())
				entry.name = std::filesystem::u8path(entry.path).stem().u8string();
			entry.kind = config.Get(Section, Key(position, "kind"));
			entry.opened = config.Get(Section, Key(position, "opened"));
			entries_.push_back(std::move(entry));
		}
	}

	void RecentProjects::Save(EditorConfig &config) const
	{
		const int stored = config.GetInt(Section, "count", 0);
		for (int index = 0; index < stored; ++index)
		{
			const std::size_t position = static_cast<std::size_t>(index);
			config.Remove(Section, Key(position, "name"));
			config.Remove(Section, Key(position, "path"));
			config.Remove(Section, Key(position, "kind"));
			config.Remove(Section, Key(position, "opened"));
		}

		config.SetInt(Section, "count", static_cast<int>(entries_.size()));
		for (std::size_t index = 0; index < entries_.size(); ++index)
		{
			const RecentProject &entry = entries_[index];
			config.Set(Section, Key(index, "name"), entry.name);
			config.Set(Section, Key(index, "path"), entry.path);
			config.Set(Section, Key(index, "kind"), entry.kind);
			config.Set(Section, Key(index, "opened"), entry.opened);
		}
	}

	bool RecentProjects::Add(RecentProject entry)
	{
		if (entry.path.empty() || limit_ == 0)
			return false;

		entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
			[&entry](const RecentProject &existing)
			{
				return SamePath(existing.path, entry.path);
			}),
			entries_.end());

		entries_.insert(entries_.begin(), std::move(entry));
		if (entries_.size() > limit_)
			entries_.resize(limit_);
		NotifyChanged();
		return true;
	}

	bool RecentProjects::Remove(std::string_view path)
	{
		const auto it = std::find_if(entries_.begin(), entries_.end(),
			[path](const RecentProject &entry) { return SamePath(entry.path, path); });
		if (it == entries_.end())
			return false;
		entries_.erase(it);
		NotifyChanged();
		return true;
	}

	void RecentProjects::Clear()
	{
		if (entries_.empty())
			return;
		entries_.clear();
		NotifyChanged();
	}

	const std::vector<RecentProject> &RecentProjects::GetEntries() const noexcept
	{
		return entries_;
	}

	std::size_t RecentProjects::GetLimit() const noexcept { return limit_; }

	void RecentProjects::SetLimit(std::size_t limit)
	{
		limit_ = limit;
		if (entries_.size() > limit_)
		{
			entries_.resize(limit_);
			NotifyChanged();
		}
	}

	void RecentProjects::NotifyChanged() const
	{
		if (events_)
			events_->Emit({EventType::RecentProjectsChanged, {}, {}});
	}

} // namespace BlackLight::Editor
