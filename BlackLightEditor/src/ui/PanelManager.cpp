#include "editor/ui/PanelManager.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <exception>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		constexpr std::size_t RegionIndex(DockRegion region) noexcept
		{
			return static_cast<std::size_t>(region);
		}

		std::string DockId(DockRegion region)
		{
			return std::string("dock_") + ToString(region);
		}

		std::string TabsId(DockRegion region) { return DockId(region) + "_tabs"; }

		std::string ContentId(DockRegion region) { return DockId(region) + "_content"; }

	} // namespace

	const char *ToString(DockRegion region) noexcept
	{
		switch (region)
		{
		case DockRegion::Left: return "left";
		case DockRegion::Center: return "center";
		case DockRegion::Right: return "right";
		case DockRegion::Bottom: return "bottom";
		}
		return "left";
	}

	bool DockRegionFromString(std::string_view text, DockRegion &region) noexcept
	{
		if (text == "left") { region = DockRegion::Left; return true; }
		if (text == "center") { region = DockRegion::Center; return true; }
		if (text == "right") { region = DockRegion::Right; return true; }
		if (text == "bottom") { region = DockRegion::Bottom; return true; }
		return false;
	}

	PanelManager::PanelManager(EventBus &events) : events_(events) {}

	void PanelManager::RegisterPanel(PanelDescriptor panel)
	{
		if (panel.id.empty() || FindPanel(panel.id))
			return;
		defaults_.push_back({panel.region, panel.visible});
		panels_.push_back(std::move(panel));
	}

	void PanelManager::SetActionHandler(ActionHandler handler)
	{
		action_handler_ = std::move(handler);
	}

	void PanelManager::SetCloseGlyph(std::string glyph)
	{
		close_glyph_ = std::move(glyph);
	}

	void PanelManager::Initialize(EditorUi &ui) { Refresh(ui); }

	void PanelManager::Shutdown(EditorUi &ui)
	{
		for (const DockRegion region : {DockRegion::Left, DockRegion::Center, DockRegion::Right, DockRegion::Bottom})
		{
			ui.ClearListeners(TabsId(region));
			ui.ClearListeners(ContentId(region));
		}
	}

	void PanelManager::Refresh(EditorUi &ui)
	{
		for (const DockRegion region : {DockRegion::Left, DockRegion::Center, DockRegion::Right, DockRegion::Bottom})
			RenderRegion(ui, region);
		refresh_pending_ = false;
	}

	void PanelManager::RenderRegion(EditorUi &ui, DockRegion region)
	{
		const std::string dock_id = DockId(region);
		const std::string tabs_id = TabsId(region);
		const std::string content_id = ContentId(region);
		const std::string tabs_scope = tabs_id;
		const std::string content_scope = content_id;

		const std::vector<const PanelDescriptor *> visible = GetVisiblePanels(region);
		if (visible.empty())
		{
			ui.SetVisible(dock_id, false);
			ui.ClearListeners(tabs_scope);
			ui.ClearListeners(content_scope);
			ui.SetHtml(tabs_id, {});
			ui.SetHtml(content_id, {});
			return;
		}
		ui.SetVisible(dock_id, true);

		std::string &active_id = active_[RegionIndex(region)];
		const bool active_is_visible = std::any_of(visible.begin(), visible.end(),
			[&active_id](const PanelDescriptor *panel) { return panel->id == active_id; });
		if (!active_is_visible)
			active_id = visible.front()->id;

		std::string tabs_html;
		std::string content_html;
		for (const PanelDescriptor *panel : visible)
		{
			const bool is_active = panel->id == active_id;
			const std::string escaped_id = EditorUi::Escape(panel->id);

			tabs_html += "<div class=\"dock-tab";
			if (is_active)
				tabs_html += " active";
			tabs_html += "\" data-panel=\"" + escaped_id + "\">";
			tabs_html += "<span class=\"dock-tab-title\">" + EditorUi::Escape(panel->title) + "</span>";
			if (panel->closable)
				tabs_html += "<span class=\"dock-tab-close\" data-close-panel=\"" + escaped_id + "\">" +
					close_glyph_ + "</span>";
			tabs_html += "</div>";

			std::string body;
			if (panel->render)
			{
				try
				{
					body = panel->render();
				}
				catch (const std::exception &exception)
				{
					body = "<div class=\"empty\">Panel error: " +
						EditorUi::Escape(exception.what()) + "</div>";
				}
			}

			content_html += "<div class=\"panel";
			if (is_active)
				content_html += " active";
			content_html += "\" id=\"panel_" + escaped_id + "\">";
			content_html += "<div class=\"panel-body\" id=\"panel_" + escaped_id + "_body\">" +
				body + "</div></div>";
		}

		// Remove callbacks bound to the old markup before replacing it.
		ui.ClearListeners(tabs_scope);
		ui.ClearListeners(content_scope);
		ui.SetHtml(tabs_id, tabs_html);
		ui.SetHtml(content_id, content_html);

		Rml::Element *tabs_element = ui.Find(tabs_id);
		Rml::Element *content_element = ui.Find(content_id);
		ui.AddListener(tabs_scope, tabs_element, "click",
			[this, region, tabs_element](Rml::Event &event)
			{
				HandleTabClick(region, tabs_element, event);
			});
		ui.AddListener(content_scope, content_element, "click",
			[this, content_element](Rml::Event &event)
			{
				HandleContentClick(content_element, event);
			});
	}

	void PanelManager::HandleTabClick(DockRegion region, Rml::Element *container, Rml::Event &event)
	{
		Rml::Element *target = event.GetTargetElement();
		const std::string close_id = EditorUi::FindAttribute(target, container, "data-close-panel");
		if (!close_id.empty())
		{
			HidePanel(close_id);
			return;
		}

		const std::string panel_id = EditorUi::FindAttribute(target, container, "data-panel");
		if (panel_id.empty())
			return;
		SetActivePanel(region, panel_id);
	}

	void PanelManager::HandleContentClick(Rml::Element *container, Rml::Event &event)
	{
		const std::string action =
			EditorUi::FindAttribute(event.GetTargetElement(), container, "data-action");
		if (!action.empty() && action_handler_)
			action_handler_(action);
	}

	void PanelManager::RequestRefresh() noexcept { refresh_pending_ = true; }

	bool PanelManager::ConsumeRefreshRequest() noexcept
	{
		const bool pending = refresh_pending_;
		refresh_pending_ = false;
		return pending;
	}

	bool PanelManager::ShowPanel(std::string_view id)
	{
		for (auto &panel : panels_)
		{
			if (panel.id != id)
				continue;
			if (panel.visible)
				return true;
			panel.visible = true;
			RequestRefresh();
			events_.Emit({EventType::PanelVisibilityChanged, panel.id, "show"});
			return true;
		}
		return false;
	}

	bool PanelManager::HidePanel(std::string_view id)
	{
		for (auto &panel : panels_)
		{
			if (panel.id != id)
				continue;
			if (!panel.visible || !panel.closable)
				return false;
			panel.visible = false;
			RequestRefresh();
			events_.Emit({EventType::PanelVisibilityChanged, panel.id, "hide"});
			return true;
		}
		return false;
	}

	bool PanelManager::TogglePanel(std::string_view id)
	{
		const PanelDescriptor *panel = FindPanel(id);
		if (!panel)
			return false;
		return panel->visible ? HidePanel(id) : ShowPanel(id);
	}

	bool PanelManager::IsVisible(std::string_view id) const
	{
		const PanelDescriptor *panel = FindPanel(id);
		return panel && panel->visible;
	}

	bool PanelManager::SetPanelRegion(std::string_view id, DockRegion region)
	{
		for (auto &panel : panels_)
		{
			if (panel.id != id)
				continue;
			if (panel.region == region)
				return true;
			panel.region = region;
			RequestRefresh();
			events_.Emit({EventType::PanelVisibilityChanged, panel.id,
				std::string("region:") + ToString(region)});
			return true;
		}
		return false;
	}

	bool PanelManager::SetActivePanel(DockRegion region, std::string_view id)
	{
		const PanelDescriptor *panel = FindPanel(id);
		if (!panel || !panel->visible || panel->region != region)
			return false;
		std::string &active_id = active_[RegionIndex(region)];
		if (active_id == id)
			return true;
		active_id = std::string(id);
		RequestRefresh();
		events_.Emit({EventType::ActivePanelChanged, active_id, ToString(region)});
		return true;
	}

	std::string PanelManager::GetActivePanel(DockRegion region) const
	{
		return active_[RegionIndex(region)];
	}

	bool PanelManager::IsPanelActive(std::string_view id) const
	{
		const PanelDescriptor *panel = FindPanel(id);
		return panel && panel->visible &&
			GetActivePanel(panel->region) == id;
	}

	const PanelDescriptor *PanelManager::FindPanel(std::string_view id) const
	{
		for (const auto &panel : panels_)
		{
			if (panel.id == id)
				return &panel;
		}
		return nullptr;
	}

	std::vector<const PanelDescriptor *> PanelManager::GetVisiblePanels(DockRegion region) const
	{
		std::vector<const PanelDescriptor *> result;
		for (const auto &panel : panels_)
		{
			if (panel.region == region && panel.visible)
				result.push_back(&panel);
		}
		return result;
	}

	const std::vector<PanelDescriptor> &PanelManager::GetPanels() const noexcept { return panels_; }

	void PanelManager::ApplyConfig(const EditorConfig &config)
	{
		for (std::size_t index = 0; index < panels_.size(); ++index)
		{
			auto &panel = panels_[index];
			panel.visible = config.GetBool("panels", panel.id + ".visible",
				defaults_[index].visible);
			DockRegion region = defaults_[index].region;
			if (DockRegionFromString(config.Get("panels", panel.id + ".region"), region))
				panel.region = region;
		}

		for (const DockRegion region : {DockRegion::Left, DockRegion::Center, DockRegion::Right, DockRegion::Bottom})
		{
			active_[RegionIndex(region)] =
				config.Get("layout", std::string(ToString(region)) + ".active");
		}
		RequestRefresh();
	}

	void PanelManager::SaveConfig(EditorConfig &config) const
	{
		for (const auto &panel : panels_)
		{
			config.SetBool("panels", panel.id + ".visible", panel.visible);
			config.Set("panels", panel.id + ".region", ToString(panel.region));
		}
		for (const DockRegion region : {DockRegion::Left, DockRegion::Center, DockRegion::Right, DockRegion::Bottom})
		{
			config.Set("layout", std::string(ToString(region)) + ".active", GetActivePanel(region));
		}
	}

	void PanelManager::ResetLayout()
	{
		for (std::size_t index = 0; index < panels_.size(); ++index)
		{
			panels_[index].region = defaults_[index].region;
			panels_[index].visible = defaults_[index].visible;
		}
		for (auto &active : active_)
			active.clear();
		RequestRefresh();
		events_.Emit({EventType::PanelVisibilityChanged, "*", "reset"});
	}

} // namespace BlackLight::Editor
