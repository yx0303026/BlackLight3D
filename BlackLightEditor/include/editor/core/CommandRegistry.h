#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one named editor action that menus, toolbars, and shortcuts share.
	struct CommandInfo
	{
		/// Creates a command descriptor with a required identifier and handler.
		CommandInfo(std::string id, std::string title, std::string category,
			std::function<bool()> execute);

		std::string id;
		std::string title;
		std::string category;
		std::string shortcut;
		std::uint32_t key = 0;
		bool ctrl = false;
		bool shift = false;
		bool alt = false;
		std::function<bool()> execute;
		std::function<bool()> can_execute;
		std::function<bool()> is_checked;

		/// Binds a virtual-key shortcut and updates the display text.
		CommandInfo &Shortcut(std::uint32_t virtual_key, bool ctrl = false,
			bool shift = false, bool alt = false);
		/// Supplies an enablement predicate evaluated by menus and shortcuts.
		CommandInfo &Enabled(std::function<bool()> predicate);
		/// Supplies a checked-state predicate rendered by menus.
		CommandInfo &Checked(std::function<bool()> predicate);
	};

	/// Registers editor commands and resolves their identifiers and shortcuts.
	class CommandRegistry final
	{
	public:
		/// Registers a command; returns false for an empty or duplicate identifier.
		bool Register(CommandInfo info);

		/// Returns a registered command, or null when the identifier is unknown.
		const CommandInfo *Find(std::string_view id) const noexcept;
		/// Invokes a command when it exists and its enablement predicate passes.
		bool Invoke(std::string_view id) const;
		/// Returns whether a command exists and is currently enabled.
		bool IsEnabled(std::string_view id) const;
		/// Returns every enabled command bound to the supplied chord.
		std::vector<const CommandInfo *> MatchShortcuts(std::uint32_t key,
			bool ctrl, bool shift, bool alt) const;
		/// Returns every registered command in registration order.
		const std::vector<CommandInfo> &GetCommands() const noexcept;

		/// Formats a chord for menus, for example `Ctrl+Shift+Z`.
		static std::string FormatShortcut(std::uint32_t key, bool ctrl, bool shift, bool alt);

	private:
		std::vector<CommandInfo> commands_;
	};

} // namespace BlackLight::Editor
