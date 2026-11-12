#include "editor/ui/BuiGizmo.h"

#include "editor/uidoc/BuiStyleDeclarations.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Formats one density-independent value with at most one decimal place.
		std::string Dp(float value)
		{
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(value));
			std::string text(buffer);
			if (!text.empty() && text.back() == '0' && text.find('.') != std::string::npos)
			{
				text.pop_back();
				if (!text.empty() && text.back() == '.')
					text.pop_back();
			}
			return text + "dp";
		}

		/// Returns the axis-aligned overlap of one value with a border band.
		bool InBand(float position, float start, float end, float grab) noexcept
		{
			return position >= start - grab && position <= end + grab;
		}

		/// Configures one square resize handle with a fixed visual style.
		void ConfigureHandle(Rml::Element *handle)
		{
			if (!handle)
				return;
			handle->SetProperty("position", "absolute");
			handle->SetProperty("width", "7px");
			handle->SetProperty("height", "7px");
			handle->SetProperty("margin-left", "-4px");
			handle->SetProperty("margin-top", "-4px");
			handle->SetProperty("background-color", "#4aa3ff");
			handle->SetProperty("border", "1px solid #d8ecff");
			handle->SetProperty("box-sizing", "border-box");
			handle->SetProperty("pointer-events", "none");
		}

		/// Returns the density ratio used to convert preview pixels to document units.
		float DensityOf(Rml::ElementDocument *document) noexcept
		{
			if (!document)
				return 1.0f;
			const Rml::Context *context = document->GetContext();
			if (!context)
				return 1.0f;
			const float ratio = context->GetDensityIndependentPixelRatio();
			return ratio > 0.0f ? ratio : 1.0f;
		}

	} // namespace

	/// Creates an unattached gizmo with no runtime elements.
	BuiGizmo::BuiGizmo() = default;

	/// Detaches the runtime overlay before destruction.
	BuiGizmo::~BuiGizmo()
	{
		Detach();
	}

	/// Attaches an eight-handle overlay to a preview document.
	void BuiGizmo::Attach(Rml::ElementDocument *document)
	{
		Detach();
		document_ = document;
		if (!document_)
			return;
		density_ = DensityOf(document_);

		Rml::ElementPtr overlay = document_->CreateElement("div");
		overlay_ = document_->AppendChild(std::move(overlay));
		if (!overlay_)
		{
			document_ = nullptr;
			return;
		}
		overlay_->SetId("__bl_gizmo");
		overlay_->SetProperty("position", "absolute");
		overlay_->SetProperty("border", "1px solid #4aa3ff");
		overlay_->SetProperty("box-sizing", "border-box");
		overlay_->SetProperty("pointer-events", "none");
		overlay_->SetProperty("z-index", "1000");
		overlay_->SetProperty("display", "none");

		for (Rml::Element *&handle : handles_)
		{
			Rml::ElementPtr child = document_->CreateElement("div");
			handle = overlay_->AppendChild(std::move(child));
			ConfigureHandle(handle);
		}
		Update();
	}

	/// Removes the overlay from its owner document and resets drag state.
	void BuiGizmo::Detach()
	{
		drag_handle_ = BuiGizmoHandle::None;
		suppress_click_ = false;
		if (document_ && overlay_)
			document_->RemoveChild(overlay_);
		document_ = nullptr;
		overlay_ = nullptr;
		handles_.fill(nullptr);
	}

	/// Enables or disables pointer input and overlay display.
	void BuiGizmo::SetEnabled(bool enabled) noexcept
	{
		enabled_ = enabled;
		if (!enabled_)
		{
			drag_handle_ = BuiGizmoHandle::None;
			SetOverlayVisible(false);
		}
		else
		{
			Update();
		}
	}

	/// Returns the current enabled state.
	bool BuiGizmo::IsEnabled() const noexcept { return enabled_; }

	/// Stores a structural selection path and refreshes the overlay.
	void BuiGizmo::SetSelectionPath(std::string_view path)
	{
		if (selection_path_ == path)
			return;
		selection_path_ = std::string(path);
		drag_handle_ = BuiGizmoHandle::None;
		suppress_click_ = false;
		Update();
	}

	/// Returns the selected structural path.
	const std::string &BuiGizmo::GetSelectionPath() const noexcept
	{
		return selection_path_;
	}

	/// Sets the snap grid used for drags and keyboard nudges.
	void BuiGizmo::SetGridStep(float step) noexcept
	{
		grid_step_ = (std::max)(0.0f, step);
	}

	/// Returns the current snap grid.
	float BuiGizmo::GetGridStep() const noexcept { return grid_step_; }

	/// Finds the selected element and updates the visible rectangle.
	void BuiGizmo::Update()
	{
		if (!enabled_ || !document_ || selection_path_.empty())
		{
			SetOverlayVisible(false);
			return;
		}
		Rml::Element *element = FindSelectedElement();
		if (!element || !element->IsVisible(true))
		{
			SetOverlayVisible(false);
			return;
		}
		ApplyOverlayGeometry(element, drag_handle_ == BuiGizmoHandle::None
			? ReadGeometry(element)
			: transient_geometry_);
		SetOverlayVisible(true);
	}

	/// Starts a move or resize gesture when the selected rectangle is hit.
	void BuiGizmo::HandleMouseDown(Rml::Event &event)
	{
		if (!enabled_ || !document_ || selection_path_.empty() ||
			drag_handle_ != BuiGizmoHandle::None)
		{
			return;
		}

		Rml::Element *element = FindSelectedElement();
		if (!element)
			return;
		float x = 0.0f;
		float y = 0.0f;
		if (!MousePosition(event, x, y))
			return;

		const BuiGizmoGeometry geometry = ReadGeometry(element);
		const BuiGizmoHandle handle = HitTestGizmo(geometry, x, y, 6.0f);
		if (handle == BuiGizmoHandle::None)
			return;

		drag_handle_ = handle;
		drag_start_geometry_ = geometry;
		transient_geometry_ = geometry;
		drag_start_x_ = x;
		drag_start_y_ = y;
		drag_had_style_ = false;
		drag_original_style_.clear();
		if (const Rml::Variant *style = element->GetAttribute("style"))
		{
			drag_original_style_ = style->Get<std::string>();
			drag_had_style_ = true;
		}
		event.StopPropagation();
	}

	/// Applies the current pointer delta to the transient rectangle.
	void BuiGizmo::HandleMouseMove(Rml::Event &event)
	{
		if (!enabled_ || drag_handle_ == BuiGizmoHandle::None)
			return;

		Rml::Element *element = FindSelectedElement();
		float x = 0.0f;
		float y = 0.0f;
		if (!element || !MousePosition(event, x, y))
		{
			Cancel();
			return;
		}

		BuiGizmoGeometry target = ApplyGizmoDrag(drag_start_geometry_, drag_handle_,
			x - drag_start_x_, y - drag_start_y_);
		if (grid_step_ > 0.0f)
		{
			const float step = grid_step_;
			const auto snap = [step](float value)
			{
				return std::round(value / step) * step;
			};
			if (drag_handle_ == BuiGizmoHandle::Move)
			{
				target.x = snap(target.x);
				target.y = snap(target.y);
			}
			else
			{
				target.x = snap(target.x);
				target.y = snap(target.y);
				target.width = (std::max)(1.0f, snap(target.width));
				target.height = (std::max)(1.0f, snap(target.height));
			}
		}
		transient_geometry_ = target;
		ApplyTransientGeometry(element, transient_geometry_);
		ApplyOverlayGeometry(element, transient_geometry_);
		event.StopPropagation();
	}

	/// Commits the transient rectangle through the host edit callback.
	void BuiGizmo::HandleMouseUp(Rml::Event &event)
	{
		if (!enabled_ || drag_handle_ == BuiGizmoHandle::None)
			return;

		const BuiGizmoGeometry geometry = transient_geometry_;
		const bool moved = !geometry.NearlyEquals(drag_start_geometry_, 0.25f);
		drag_handle_ = BuiGizmoHandle::None;
		// A press-and-release without movement must not swallow the click that
		// performs preview selection.
		suppress_click_ = moved;
		if (moved)
			Commit(geometry);
		else
			Update();
		event.StopPropagation();
	}

	/// Offsets the selection by a keyboard step and commits the result.
	bool BuiGizmo::Nudge(float delta_x, float delta_y)
	{
		if (!enabled_ || !document_ || selection_path_.empty() ||
			drag_handle_ != BuiGizmoHandle::None)
		{
			return false;
		}
		Rml::Element *element = FindSelectedElement();
		if (!element)
			return false;

		BuiGizmoGeometry geometry = ReadGeometry(element);
		geometry.x += delta_x;
		geometry.y += delta_y;
		Commit(geometry);
		return true;
	}

	/// Abandons the active drag and restores the authored inline style.
	void BuiGizmo::Cancel()
	{
		if (drag_handle_ == BuiGizmoHandle::None)
			return;
		if (Rml::Element *element = FindSelectedElement())
			RestoreAuthoredGeometry(element);
		drag_handle_ = BuiGizmoHandle::None;
		transient_geometry_ = {};
		Update();
	}

	/// Returns whether a gesture is in progress.
	bool BuiGizmo::IsDragging() const noexcept
	{
		return drag_handle_ != BuiGizmoHandle::None;
	}

	/// Consumes the click suppression flag set by a moved gesture.
	bool BuiGizmo::ConsumeClickSuppression() noexcept
	{
		const bool suppressed = suppress_click_;
		suppress_click_ = false;
		return suppressed;
	}

	/// Installs the callback invoked after one completed gesture.
	void BuiGizmo::SetEditHandler(EditHandler handler)
	{
		edit_handler_ = std::move(handler);
	}

	/// Finds the element tagged with the selected structural path.
	Rml::Element *BuiGizmo::FindSelectedElement() const
	{
		return document_ && !selection_path_.empty()
			? FindElementByPath(document_)
			: nullptr;
	}

	/// Recursively searches runtime preview elements for a path attribute.
	Rml::Element *BuiGizmo::FindElementByPath(Rml::Element *element) const
	{
		if (!element || element == overlay_)
			return nullptr;
		if (const Rml::Variant *attribute = element->GetAttribute("data-bl-node"))
		{
			if (attribute->Get<std::string>() == selection_path_)
				return element;
		}
		for (int index = 0; index < element->GetNumChildren(); ++index)
		{
			if (Rml::Element *result = FindElementByPath(element->GetChild(index)))
				return result;
		}
		return nullptr;
	}

	/// Reads a selected element rectangle relative to its offset parent.
	BuiGizmoGeometry BuiGizmo::ReadGeometry(Rml::Element *element) const
	{
		BuiGizmoGeometry geometry;
		if (!element || !document_)
			return geometry;

		const Rml::Vector2f element_offset =
			element->GetAbsoluteOffset(Rml::BoxArea::Border);
		Rml::Element *parent = element->GetOffsetParent();
		const Rml::Vector2f parent_offset =
			parent && parent != element
				? parent->GetAbsoluteOffset(Rml::BoxArea::Border)
				: Rml::Vector2f(0.0f, 0.0f);
		const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);

		const float scale = density_ > 0.0f ? 1.0f / density_ : 1.0f;
		geometry.x = (element_offset.x - parent_offset.x) * scale;
		geometry.y = (element_offset.y - parent_offset.y) * scale;
		geometry.width = (std::max)(1.0f, size.x * scale);
		geometry.height = (std::max)(1.0f, size.y * scale);
		return geometry;
	}

	/// Applies a temporary absolute rectangle to the live preview element.
	void BuiGizmo::ApplyTransientGeometry(Rml::Element *element,
		const BuiGizmoGeometry &geometry)
	{
		if (!element)
			return;
		element->SetProperty("position", "absolute");
		element->SetProperty("left", Dp(geometry.x));
		element->SetProperty("top", Dp(geometry.y));
		element->SetProperty("width", Dp(geometry.width));
		element->SetProperty("height", Dp(geometry.height));
	}

	/// Restores the authored inline geometry after a cancelled gesture.
	void BuiGizmo::RestoreAuthoredGeometry(Rml::Element *element)
	{
		if (!element)
			return;
		if (!drag_had_style_)
		{
			for (const char *property : {"position", "left", "top", "width", "height"})
				element->RemoveProperty(property);
			return;
		}
		// Rebuild the transient properties from the authored declaration list so
		// values the gesture overwrote come back verbatim.
		for (const char *property : {"position", "left", "top", "width", "height"})
			element->RemoveProperty(property);
		const std::vector<BuiStyleDeclaration> declarations =
			ParseStyleDeclarations(drag_original_style_);
		for (const BuiStyleDeclaration &declaration : declarations)
			element->SetProperty(declaration.name, declaration.value);
	}

	/// Positions the selection border and its eight resize handles.
	void BuiGizmo::ApplyOverlayGeometry(Rml::Element *element,
		const BuiGizmoGeometry &geometry)
	{
		if (!overlay_ || !document_)
			return;

		// Geometry is expressed against the selected element's offset parent while
		// the overlay lives on the document, so translate between the two origins.
		float origin_x = 0.0f;
		float origin_y = 0.0f;
		if (element)
		{
			Rml::Element *parent = element->GetOffsetParent();
			if (parent && parent != element)
			{
				const Rml::Vector2f parent_offset =
					parent->GetAbsoluteOffset(Rml::BoxArea::Border);
				const Rml::Vector2f document_offset =
					document_->GetAbsoluteOffset(Rml::BoxArea::Border);
				const float scale = density_ > 0.0f ? 1.0f / density_ : 1.0f;
				origin_x = (parent_offset.x - document_offset.x) * scale;
				origin_y = (parent_offset.y - document_offset.y) * scale;
			}
		}

		overlay_->SetProperty("left", Dp(origin_x + geometry.x));
		overlay_->SetProperty("top", Dp(origin_y + geometry.y));
		overlay_->SetProperty("width", Dp(geometry.width));
		overlay_->SetProperty("height", Dp(geometry.height));

		// Percentage handles resolve against the overlay box itself.
		struct HandlePlacement
		{
			Rml::Element *handle = nullptr;
			const char *left = "0%";
			const char *top = "0%";
		};
		const std::array<HandlePlacement, 8> placements = {{
			{handles_[0], "0%", "0%"},
			{handles_[1], "50%", "0%"},
			{handles_[2], "100%", "0%"},
			{handles_[3], "100%", "50%"},
			{handles_[4], "100%", "100%"},
			{handles_[5], "50%", "100%"},
			{handles_[6], "0%", "100%"},
			{handles_[7], "0%", "50%"},
		}};
		for (const HandlePlacement &placement : placements)
		{
			if (!placement.handle)
				continue;
			placement.handle->SetProperty("left", placement.left);
			placement.handle->SetProperty("top", placement.top);
		}
	}

	/// Shows or hides the runtime overlay.
	void BuiGizmo::SetOverlayVisible(bool visible)
	{
		if (overlay_)
			overlay_->SetProperty("display", visible ? "block" : "none");
	}

	/// Converts a mouse event's screen position into offset-parent coordinates.
	bool BuiGizmo::MousePosition(Rml::Event &event, float &x, float &y) const
	{
		if (!document_)
			return false;
		Rml::Element *element = FindSelectedElement();
		if (!element)
			return false;
		Rml::Element *parent = element->GetOffsetParent();
		const Rml::Vector2f origin =
			parent && parent != element
				? parent->GetAbsoluteOffset(Rml::BoxArea::Border)
				: document_->GetAbsoluteOffset(Rml::BoxArea::Border);
		const Rml::Vector2f mouse = event.GetUnprojectedMouseScreenPos();
		const float scale = density_ > 0.0f ? 1.0f / density_ : 1.0f;
		x = (mouse.x - origin.x) * scale;
		y = (mouse.y - origin.y) * scale;
		return true;
	}

	/// Reports one rectangle through the host callback and resynchronizes.
	void BuiGizmo::Commit(const BuiGizmoGeometry &geometry)
	{
		const auto round_to = [](float value)
		{
			return std::round(value * 10.0f) / 10.0f;
		};
		BuiGizmoGeometry rounded = geometry;
		rounded.x = round_to(rounded.x);
		rounded.y = round_to(rounded.y);
		rounded.width = (std::max)(1.0f, round_to(rounded.width));
		rounded.height = (std::max)(1.0f, round_to(rounded.height));
		if (edit_handler_)
			edit_handler_(selection_path_, rounded);
		transient_geometry_ = {};
		Update();
	}

} // namespace BlackLight::Editor
