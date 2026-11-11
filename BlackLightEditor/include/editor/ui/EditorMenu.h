#pragma once

#include "editor/core/CommandRegistry.h"
#include "editor/ui/EditorUi.h"

#include <string>
#include <string_view>
#include <vector>

namespace Rml
{
	class Element;
	class Event;
}

namespace BlackLight::Editor
{

	/// Describes one top-level menu and the commands it exposes.
	struct MenuDefinition
	{
		std::string id;
		std::string title;
		std::vector<std::string> commands;
	};

	/// Builds the menu bar from the command registry and handles dropdown clicks.
	class EditorMenu final
	{
	public:
		explicit EditorMenu(CommandRegistry &commands);

		/// Adds a top-level menu; an empty command id renders a separator.
		void AddMenu(std::string id, std::string title, std::vector<std::string> command_ids);
		/// Renders the bar and binds delegated menu listeners.
		void Initialize(EditorUi &ui);
		/// Detaches menu listeners; call before the document is closed.
		void Shutdown(EditorUi &ui);

		/// Toggles one menu open or closed.
		bool Toggle(EditorUi &ui, std::string_view id);
		/// Closes the open menu, if any.
		void CloseAll(EditorUi &ui);
		/// Returns whether a dropdown is currently open.
		bool IsOpen() const noexcept;

		/// Returns every registered menu definition.
		const std::vector<MenuDefinition> &GetMenus() const noexcept;

	private:
		void RenderBar(EditorUi &ui);
		void OpenMenu(EditorUi &ui, std::string_view id);
		void RenderDropdown(EditorUi &ui, const MenuDefinition &menu);
		std::string BuildDropdownHtml(const MenuDefinition &menu) const;
		void HandleBarClick(EditorUi &ui, Rml::Event &event);
		void HandleDocumentClick(EditorUi &ui, Rml::Event &event);

		CommandRegistry &commands_;
		std::vector<MenuDefinition> menus_;
		Rml::Element *bar_ = nullptr;
		std::string open_menu_;
	};

} // namespace BlackLight::Editor
