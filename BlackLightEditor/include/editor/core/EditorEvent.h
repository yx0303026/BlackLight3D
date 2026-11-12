#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace BlackLight::Editor
{

	/// Identifies a change that editor systems can observe.
	enum class EventType : std::uint8_t
	{
		DocumentChanged,
		DocumentModified,
		ObjectsChanged,
		SelectionChanged,
		ModeChanged,
		CommandExecuted,
		CommandUndone,
		CommandRedone,
		CommandHistoryChanged,
		PanelVisibilityChanged,
		ActivePanelChanged,
		ThemeChanged,
		RecentProjectsChanged,
		ProjectChanged,
		ProjectOpened,
		ProjectClosed,
		UiDocumentsChanged,
		UiDocumentModified,
		UiSelectionChanged,
		StatusMessage,
		QuitRequested
	};

	/// Returns the stable name of an event type.
	const char *ToString(EventType type) noexcept;

	/// Describes one editor notification without owning any subsystem state.
	struct EditorEvent
	{
		/// Identifies the change that occurred.
		EventType type = EventType::StatusMessage;
		/// Carries the event subject, such as a command, panel, or document identifier.
		std::string name;
		/// Carries optional human-readable context for the subject.
		std::string detail;
	};

	/// Receives editor notifications on the emitting thread.
	using EventHandler = std::function<void(const EditorEvent &)>;

	/// Dispatches editor notifications to typed or catch-all subscribers.
	///
	/// Subscribers may subscribe or unsubscribe while an event is being emitted;
	/// the currently emitted event is still delivered to the handlers that were
	/// registered when emission started.
	class EventBus final
	{
	public:
		/// Identifies a subscription so it can be removed later.
		using Token = std::uint64_t;
		/// Marks a token that never identifies a live subscription.
		static constexpr Token InvalidToken = 0;

		/// Adds a handler that receives one event type.
		Token Subscribe(EventType type, EventHandler handler);
		/// Adds a handler that receives every event type.
		Token SubscribeAll(EventHandler handler);
		/// Removes a subscription; unknown or repeated tokens are ignored.
		void Unsubscribe(Token token);
		/// Synchronously delivers an event to every matching subscriber.
		void Emit(const EditorEvent &event) const;
		/// Removes every subscription.
		void Clear();
		/// Returns the number of live subscriptions.
		std::size_t GetSubscriberCount() const noexcept;

	private:
		struct Entry
		{
			Token token = InvalidToken;
			std::optional<EventType> type;
			EventHandler handler;
		};

		std::vector<Entry> entries_;
		Token next_token_ = 1;
	};

} // namespace BlackLight::Editor
