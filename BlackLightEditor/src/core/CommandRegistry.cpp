#include "editor/core/CommandRegistry.h"

#include <utility>

namespace BlackLight::Editor
{

	CommandInfo::CommandInfo(std::string id_value, std::string title_value,
		std::string category_value, std::function<bool()> execute_value)
		: id(std::move(id_value)),
		  title(std::move(title_value)),
		  category(std::move(category_value)),
		  execute(std::move(execute_value))
	{
	}

	CommandInfo &CommandInfo::Shortcut(std::uint32_t virtual_key, bool ctrl_value,
		bool shift_value, bool alt_value)
	{
		key = virtual_key;
		ctrl = ctrl_value;
		shift = shift_value;
		alt = alt_value;
		shortcut = CommandRegistry::FormatShortcut(key, ctrl, shift, alt);
		return *this;
	}

	CommandInfo &CommandInfo::Enabled(std::function<bool()> predicate)
	{
		can_execute = std::move(predicate);
		return *this;
	}

	CommandInfo &CommandInfo::Checked(std::function<bool()> predicate)
	{
		is_checked = std::move(predicate);
		return *this;
	}

	bool CommandRegistry::Register(CommandInfo info)
	{
		if (info.id.empty() || !info.execute)
			return false;
		if (Find(info.id))
			return false;
		commands_.push_back(std::move(info));
		return true;
	}

	const CommandInfo *CommandRegistry::Find(std::string_view id) const noexcept
	{
		for (const auto &command : commands_)
		{
			if (command.id == id)
				return &command;
		}
		return nullptr;
	}

	bool CommandRegistry::Invoke(std::string_view id) const
	{
		const CommandInfo *command = Find(id);
		if (!command || !command->execute)
			return false;
		if (command->can_execute && !command->can_execute())
			return false;
		return command->execute();
	}

	bool CommandRegistry::IsEnabled(std::string_view id) const
	{
		const CommandInfo *command = Find(id);
		if (!command || !command->execute)
			return false;
		return !command->can_execute || command->can_execute();
	}

	std::vector<const CommandInfo *> CommandRegistry::MatchShortcuts(std::uint32_t key,
		bool ctrl, bool shift, bool alt) const
	{
		std::vector<const CommandInfo *> matches;
		if (key == 0)
			return matches;
		for (const auto &command : commands_)
		{
			if (command.key != key || command.ctrl != ctrl || command.shift != shift ||
				command.alt != alt)
			{
				continue;
			}
			if (command.can_execute && !command.can_execute())
				continue;
			matches.push_back(&command);
		}
		return matches;
	}

	const std::vector<CommandInfo> &CommandRegistry::GetCommands() const noexcept { return commands_; }

	std::string CommandRegistry::FormatShortcut(std::uint32_t key, bool ctrl, bool shift, bool alt)
	{
		if (key == 0)
			return {};

		std::string result;
		if (ctrl)
			result += "Ctrl+";
		if (shift)
			result += "Shift+";
		if (alt)
			result += "Alt+";

		if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
		{
			result.push_back(static_cast<char>(key));
			return result;
		}
		if (key >= 0x70 && key <= 0x7B) // VK_F1..VK_F24
		{
			result += "F" + std::to_string(key - 0x6F);
			return result;
		}

		switch (key)
		{
		case 0x08: result += "Backspace"; break;
		case 0x09: result += "Tab"; break;
		case 0x0D: result += "Enter"; break;
		case 0x1B: result += "Esc"; break;
		case 0x20: result += "Space"; break;
		case 0x21: result += "Page Up"; break;
		case 0x22: result += "Page Down"; break;
		case 0x23: result += "End"; break;
		case 0x24: result += "Home"; break;
		case 0x25: result += "Left"; break;
		case 0x26: result += "Up"; break;
		case 0x27: result += "Right"; break;
		case 0x28: result += "Down"; break;
		case 0x2D: result += "Insert"; break;
		case 0x2E: result += "Delete"; break;
		default: result += "Key " + std::to_string(key); break;
		}
		return result;
	}

} // namespace BlackLight::Editor
