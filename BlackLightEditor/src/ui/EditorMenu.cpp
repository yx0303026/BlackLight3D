#include "editor/ui/EditorMenu.h"

#include <RmlUi/Core.h>

#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Returns whether an element is the supplied ancestor or one of its descendants.
		bool Contains(Rml::Element *element, Rml::Element *ancestor)
		{
			for (Rml::Element *current = element; current; current = current->GetParentNode())
			{
				if (current == ancestor)
					return true;
			}
			return false;
		}

	} // namespace

	EditorMenu::EditorMenu(CommandRegistry &commands) : commands_(commands) {}

	void EditorMenu::AddMenu(std::string id, std::string title,
		std::vector<std::string> command_ids)
	{
		if (id.empty())
			return;
		menus_.push_back({std::move(id), std::move(title), std::move(command_ids)});
	}

	void EditorMenu::Initialize(EditorUi &ui)
	{
		Shutdown(ui);
		RenderBar(ui);

		EditorUi *layer = &ui;
		bar_ = ui.Find("menu_bar_items");
		ui.AddListener("menu_bar", bar_, "click",
			[this, layer](Rml::Event &event) { HandleBarClick(*layer, event); });
		ui.AddListener("menu_document", ui.GetDocument(), "click",
			[this, layer](Rml::Event &event) { HandleDocumentClick(*layer, event); });
	}

	void EditorMenu::Shutdown(EditorUi &ui)
	{
		ui.ClearListeners("menu_bar");
		ui.ClearListeners("menu_document");
		bar_ = nullptr;
		open_menu_.clear();
	}

	void EditorMenu::RenderBar(EditorUi &ui)
	{
		std::string html;
		for (const auto &menu : menus_)
		{
			html += "<div class=\"menu-root\">";
			html += "<div class=\"menu-button\" data-menu=\"" + EditorUi::Escape(menu.id) + "\">" +
				EditorUi::Escape(menu.title) + "</div>";
			html += "<div class=\"menu-dropdown\" id=\"menu_dropdown_" +
				EditorUi::Escape(menu.id) + "\"></div>";
			html += "</div>";
		}
		ui.SetHtml("menu_bar_items", html);
	}

	bool EditorMenu::Toggle(EditorUi &ui, std::string_view id)
	{
		if (open_menu_ == id)
		{
			CloseAll(ui);
			return false;
		}
		OpenMenu(ui, id);
		return !open_menu_.empty();
	}

	void EditorMenu::OpenMenu(EditorUi &ui, std::string_view id)
	{
		const MenuDefinition *menu = nullptr;
		for (const auto &candidate : menus_)
		{
			if (candidate.id == id)
			{
				menu = &candidate;
				break;
			}
		}
		if (!menu)
			return;

		CloseAll(ui);
		if (!ui.Find("menu_dropdown_" + std::string(id)))
			return;
		RenderDropdown(ui, *menu);
		ui.SetClass("menu_dropdown_" + std::string(id), "open", true);
		open_menu_ = menu->id;
	}

	void EditorMenu::RenderDropdown(EditorUi &ui, const MenuDefinition &menu)
	{
		ui.SetHtml("menu_dropdown_" + menu.id, BuildDropdownHtml(menu));
	}

	std::string EditorMenu::BuildDropdownHtml(const MenuDefinition &menu) const
	{
		std::string html;
		bool last_was_separator = true;
		for (const auto &command_id : menu.commands)
		{
			if (command_id.empty())
			{
				if (!last_was_separator)
				{
					html += "<div class=\"menu-separator\"></div>";
					last_was_separator = true;
				}
				continue;
			}

			const CommandInfo *info = commands_.Find(command_id);
			if (!info)
				continue;

			const bool enabled = commands_.IsEnabled(command_id);
			const bool checked = info->is_checked && info->is_checked();
			html += "<div class=\"menu-item";
			if (!enabled)
				html += " disabled";
			html += "\" data-command=\"" + EditorUi::Escape(info->id) + "\">";
			html += "<span class=\"menu-check\">";
			if (checked)
				html += "*";
			html += "</span>";
			html += "<span class=\"menu-label\">" + EditorUi::Escape(info->title) + "</span>";
			html += "<span class=\"menu-shortcut\">" + EditorUi::Escape(info->shortcut) + "</span>";
			html += "</div>";
			last_was_separator = false;
		}
		return html;
	}

	void EditorMenu::HandleBarClick(EditorUi &ui, Rml::Event &event)
	{
		Rml::Element *target = event.GetTargetElement();

		const std::string command =
			EditorUi::FindAttribute(target, bar_, "data-command");
		if (!command.empty())
		{
			if (!commands_.IsEnabled(command))
				return;
			commands_.Invoke(command);
			CloseAll(ui);
			return;
		}

		const std::string menu = EditorUi::FindAttribute(target, bar_, "data-menu");
		if (!menu.empty())
			Toggle(ui, menu);
	}

	void EditorMenu::HandleDocumentClick(EditorUi &ui, Rml::Event &event)
	{
		if (open_menu_.empty())
			return;
		if (bar_ && Contains(event.GetTargetElement(), bar_))
			return;
		CloseAll(ui);
	}

	void EditorMenu::CloseAll(EditorUi &ui)
	{
		for (const auto &menu : menus_)
			ui.SetClass("menu_dropdown_" + menu.id, "open", false);
		open_menu_.clear();
	}

	bool EditorMenu::IsOpen() const noexcept { return !open_menu_.empty(); }

	const std::vector<MenuDefinition> &EditorMenu::GetMenus() const noexcept { return menus_; }

} // namespace BlackLight::Editor
