#include "editor/core/EditorEvent.h"

#include <utility>

namespace BlackLight::Editor
{

	const char *ToString(EventType type) noexcept
	{
		switch (type)
		{
		case EventType::DocumentChanged: return "DocumentChanged";
		case EventType::DocumentModified: return "DocumentModified";
		case EventType::ObjectsChanged: return "ObjectsChanged";
		case EventType::SelectionChanged: return "SelectionChanged";
		case EventType::ModeChanged: return "ModeChanged";
		case EventType::CommandExecuted: return "CommandExecuted";
		case EventType::CommandUndone: return "CommandUndone";
		case EventType::CommandRedone: return "CommandRedone";
		case EventType::CommandHistoryChanged: return "CommandHistoryChanged";
		case EventType::PanelVisibilityChanged: return "PanelVisibilityChanged";
		case EventType::ActivePanelChanged: return "ActivePanelChanged";
		case EventType::ThemeChanged: return "ThemeChanged";
		case EventType::RecentProjectsChanged: return "RecentProjectsChanged";
		case EventType::ProjectChanged: return "ProjectChanged";
		case EventType::ProjectOpened: return "ProjectOpened";
		case EventType::ProjectClosed: return "ProjectClosed";
		case EventType::UiDocumentsChanged: return "UiDocumentsChanged";
		case EventType::UiDocumentModified: return "UiDocumentModified";
		case EventType::UiSelectionChanged: return "UiSelectionChanged";
		case EventType::StatusMessage: return "StatusMessage";
		case EventType::QuitRequested: return "QuitRequested";
		}
		return "Unknown";
	}

	EventBus::Token EventBus::Subscribe(EventType type, EventHandler handler)
	{
		if (!handler)
			return InvalidToken;
		const Token token = next_token_++;
		entries_.push_back({token, type, std::move(handler)});
		return token;
	}

	EventBus::Token EventBus::SubscribeAll(EventHandler handler)
	{
		if (!handler)
			return InvalidToken;
		const Token token = next_token_++;
		entries_.push_back({token, std::nullopt, std::move(handler)});
		return token;
	}

	void EventBus::Unsubscribe(Token token)
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it)
		{
			if (it->token == token)
			{
				entries_.erase(it);
				return;
			}
		}
	}

	void EventBus::Emit(const EditorEvent &event) const
	{
		// Copy handlers first so callbacks can safely change the subscription list.
		std::vector<EventHandler> targets;
		targets.reserve(entries_.size());
		for (const auto &entry : entries_)
		{
			if (!entry.type || *entry.type == event.type)
				targets.push_back(entry.handler);
		}
		for (const auto &handler : targets)
		{
			if (handler)
				handler(event);
		}
	}

	void EventBus::Clear() { entries_.clear(); }

	std::size_t EventBus::GetSubscriberCount() const noexcept { return entries_.size(); }

} // namespace BlackLight::Editor
