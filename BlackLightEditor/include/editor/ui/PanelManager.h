#pragma once

#include "editor/core/EditorConfig.h"
#include "editor/core/EditorEvent.h"
#include "editor/ui/EditorUi.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Identifies one of the four editor dock regions.
	enum class DockRegion : std::uint8_t
	{
		Left,
		Center,
		Right,
		Bottom
	};

	const char *ToString(DockRegion region) noexcept;
	bool DockRegionFromString(std::string_view text, DockRegion &region) noexcept;

	/// Describes a dockable panel and how its content is generated.
	struct PanelDescriptor
	{
		std::string id;
		std::string title;
		DockRegion region = DockRegion::Left;
		bool visible = true;
		bool closable = true;
		std::function<std::string()> render;
	};

	/// Owns the tabbed dock layout and regenerates panel markup on demand.
	///
	/// Rendering is deferred: panel callbacks and event handlers only request a
	/// refresh, and the application applies it outside RmlUi event dispatch. This
	/// keeps event targets alive while clicks are processed.
	class PanelManager final
	{
	public:
		/// Invoked with an element's `data-action` value when panel content is clicked.
		using ActionHandler = std::function<void(std::string_view)>;

		explicit PanelManager(EventBus &events);

		/// Registers a panel; duplicate identifiers are ignored.
		void RegisterPanel(PanelDescriptor panel);
		/// Sets the handler for `data-action` clicks inside panel content.
		void SetActionHandler(ActionHandler handler);
		/// Sets the glyph rendered by tab close buttons.
		void SetCloseGlyph(std::string glyph);

		/// Renders every dock region and binds its delegated listeners.
		void Initialize(EditorUi &ui);
		/// Detaches dock listeners; call before the document is closed.
		void Shutdown(EditorUi &ui);
		/// Re-renders every dock region from the current panel state.
		void Refresh(EditorUi &ui);

		/// Marks the layout as needing a refresh on the next frame.
		void RequestRefresh() noexcept;
		/// Returns and clears the pending-refresh flag.
		bool ConsumeRefreshRequest() noexcept;

		/// Shows a hidden panel.
		bool ShowPanel(std::string_view id);
		/// Hides a visible panel.
		bool HidePanel(std::string_view id);
		/// Toggles a panel's visibility.
		bool TogglePanel(std::string_view id);
		/// Returns whether a panel exists and is visible.
		bool IsVisible(std::string_view id) const;
		/// Moves a panel to another dock region.
		bool SetPanelRegion(std::string_view id, DockRegion region);
		/// Activates one visible panel tab in a region.
		bool SetActivePanel(DockRegion region, std::string_view id);
		/// Returns the active tab id for a region, or an empty string.
		std::string GetActivePanel(DockRegion region) const;

		/// Returns a panel descriptor, or null when the identifier is unknown.
		const PanelDescriptor *FindPanel(std::string_view id) const;
		/// Returns the visible panels in a region, in registration order.
		std::vector<const PanelDescriptor *> GetVisiblePanels(DockRegion region) const;
		/// Returns every registered panel in registration order.
		const std::vector<PanelDescriptor> &GetPanels() const noexcept;

		/// Restores visibility and region values persisted by SaveConfig.
		void ApplyConfig(const EditorConfig &config);
		/// Persists visibility, region, and active-tab values.
		void SaveConfig(EditorConfig &config) const;
		/// Restores the layout captured when each panel was registered.
		void ResetLayout();

	private:
		struct PanelDefaults
		{
			DockRegion region = DockRegion::Left;
			bool visible = true;
		};

		void RenderRegion(EditorUi &ui, DockRegion region);
		void HandleTabClick(DockRegion region, Rml::Element *container, Rml::Event &event);
		void HandleContentClick(Rml::Element *container, Rml::Event &event);

		EventBus &events_;
		std::vector<PanelDescriptor> panels_;
		std::vector<PanelDefaults> defaults_;
		std::array<std::string, 4> active_;
		ActionHandler action_handler_;
		std::string close_glyph_ = "x";
		bool refresh_pending_ = false;
	};

} // namespace BlackLight::Editor
