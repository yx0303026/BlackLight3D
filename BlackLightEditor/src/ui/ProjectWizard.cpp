#include "editor/ui/ProjectWizard.h"

#include "editor/ui/EditorUi.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cctype>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Removes leading and trailing ASCII whitespace.
		std::string Trim(std::string_view text)
		{
			std::size_t begin = 0;
			std::size_t end = text.size();
			while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
				++begin;
			while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
				--end;
			return std::string(text.substr(begin, end - begin));
		}

		/// Returns the element id used by one project type option.
		const char *KindOptionId(ProjectKind kind) noexcept
		{
			switch (kind)
			{
			case ProjectKind::WindowsApp: return "wizard_kind_windows";
			case ProjectKind::BlackLightGame: return "wizard_kind_game";
			case ProjectKind::BlockLightUIInject: return "wizard_kind_inject";
			}
			return "";
		}

		/// Returns the element id used by one graphics API option.
		const char *ApiOptionId(GraphicsApi api) noexcept
		{
			switch (api)
			{
			case GraphicsApi::OpenGL: return "wizard_api_opengl";
			case GraphicsApi::DirectX11: return "wizard_api_dx11";
			case GraphicsApi::DirectX12: return "wizard_api_dx12";
			}
			return "";
		}

		/// Returns the description shown under the project type options.
		const char *KindDescription(ProjectKind kind) noexcept
		{
			switch (kind)
			{
			case ProjectKind::WindowsApp:
				return "A desktop window with a selectable graphics API.";
			case ProjectKind::BlackLightGame:
				return "A game loop with fixed and variable updates plus an assets content root.";
			case ProjectKind::BlockLightUIInject:
				return "A DLL shell with a stable export table for injecting BlackLightUI.";
			}
			return "";
		}

	} // namespace

	void ProjectWizard::Initialize(EditorUi &ui)
	{
		Shutdown(ui);
		ui_ = &ui;

		Rml::Element *overlay = ui.Find("project_wizard");
		if (!overlay)
			return;

		ui.AddListener("project_wizard", overlay, "click", [this](Rml::Event &event)
		{
			const std::string action =
				EditorUi::FindAttribute(event.GetTargetElement(), nullptr, "data-action");
			if (!action.empty())
				HandleAction(action);
		});
		ui.AddListener("project_wizard_inputs", "wizard_name", "change",
			[this](Rml::Event &) { HandleNameChanged(); });

		Hide();
		RefreshSelection();
	}

	void ProjectWizard::Shutdown(EditorUi &ui)
	{
		ui.ClearListeners("project_wizard");
		ui.ClearListeners("project_wizard_inputs");
		ui_ = nullptr;
		visible_ = false;
	}

	void ProjectWizard::SetCreateHandler(CreateHandler handler)
	{
		create_handler_ = std::move(handler);
	}

	void ProjectWizard::SetBrowseHandler(BrowseHandler handler)
	{
		browse_handler_ = std::move(handler);
	}

	void ProjectWizard::Show(std::string default_name,
		const std::filesystem::path &default_location,
		const std::filesystem::path &engine_root)
	{
		if (!ui_)
			return;

		if (default_name.empty())
			default_name = "MyProject";
		SetInput("wizard_name", default_name);
		SetInput("wizard_location", default_location.u8string());
		SetInput("wizard_engine", engine_root.u8string());

		last_derived_namespace_ = DeriveNamespace(default_name);
		SetInput("wizard_namespace", last_derived_namespace_);

		kind_ = ProjectKind::WindowsApp;
		graphics_api_ = GraphicsApi::OpenGL;
		RefreshSelection();
		ClearError();
		ui_->SetVisible("project_wizard", true);
		visible_ = true;
	}

	void ProjectWizard::Hide()
	{
		if (ui_)
		{
			ui_->SetVisible("project_wizard", false);
			ClearError();
		}
		visible_ = false;
	}

	bool ProjectWizard::IsVisible() const noexcept { return visible_; }

	void ProjectWizard::SelectKind(ProjectKind kind)
	{
		kind_ = kind;
		RefreshSelection();
	}

	void ProjectWizard::SelectGraphicsApi(GraphicsApi api)
	{
		graphics_api_ = api;
		RefreshSelection();
	}

	void ProjectWizard::HandleAction(std::string_view action)
	{
		if (action == "wizard:cancel")
		{
			Hide();
			return;
		}
		if (action == "wizard:create")
		{
			CreateProject();
			return;
		}
		if (action == "wizard:browse")
		{
			BrowseLocation();
			return;
		}

		constexpr std::string_view kind_prefix = "wizard:kind:";
		if (action.substr(0, kind_prefix.size()) == kind_prefix)
		{
			ProjectKind kind = ProjectKind::WindowsApp;
			if (ProjectKindFromString(action.substr(kind_prefix.size()), kind))
				SelectKind(kind);
			return;
		}

		constexpr std::string_view api_prefix = "wizard:graphics:";
		if (action.substr(0, api_prefix.size()) == api_prefix)
		{
			GraphicsApi api = GraphicsApi::OpenGL;
			if (GraphicsApiFromString(action.substr(api_prefix.size()), api))
				SelectGraphicsApi(api);
		}
	}

	void ProjectWizard::CreateProject()
	{
		if (!ui_)
			return;

		const std::string name = Trim(GetInput("wizard_name"));
		if (name.empty())
		{
			ShowError("Enter a project name.");
			return;
		}
		if (!IsValidProjectName(name))
		{
			ShowError("The project name may only contain letters, digits, spaces, '-' and '_', "
				"and must start with a letter.");
			return;
		}

		const std::string location = Trim(GetInput("wizard_location"));
		if (location.empty())
		{
			ShowError("Choose a project location.");
			return;
		}
		const std::string engine_root = Trim(GetInput("wizard_engine"));
		if (engine_root.empty())
		{
			ShowError("Choose the BlackLight engine root.");
			return;
		}

		ProjectOptions options;
		options.name = name;
		options.directory = std::filesystem::u8path(location);
		options.name_space = Trim(GetInput("wizard_namespace"));
		if (options.name_space.empty())
			options.name_space = DeriveNamespace(name);
		if (!IsValidNamespace(options.name_space))
		{
			ShowError("The namespace must be a valid C++ identifier.");
			return;
		}
		options.kind = kind_;
		options.graphics_api = graphics_api_;
		options.engine_root = std::filesystem::u8path(engine_root);

		if (!create_handler_)
		{
			ShowError("Project creation is not configured.");
			return;
		}

		std::string error;
		if (!create_handler_(options, error))
		{
			ShowError(error.empty() ? "Failed to create the project." : error);
			return;
		}
		Hide();
	}

	void ProjectWizard::BrowseLocation()
	{
		if (!browse_handler_)
			return;
		const std::filesystem::path initial =
			std::filesystem::u8path(Trim(GetInput("wizard_location")));
		const std::filesystem::path chosen = browse_handler_(initial);
		if (!chosen.empty())
			SetInput("wizard_location", chosen.u8string());
	}

	void ProjectWizard::RefreshSelection()
	{
		if (!ui_)
			return;

		for (const ProjectKind kind : {ProjectKind::WindowsApp, ProjectKind::BlackLightGame,
			ProjectKind::BlockLightUIInject})
		{
			ui_->SetClass(KindOptionId(kind), "selected", kind == kind_);
		}
		for (const GraphicsApi api : {GraphicsApi::OpenGL, GraphicsApi::DirectX11,
			GraphicsApi::DirectX12})
		{
			ui_->SetClass(ApiOptionId(api), "selected", api == graphics_api_);
		}
		ui_->SetClass("wizard_graphics_section", "hidden",
			kind_ == ProjectKind::BlockLightUIInject);
		RefreshKindHint();
	}

	void ProjectWizard::RefreshKindHint()
	{
		if (ui_)
			ui_->SetText("wizard_kind_hint", KindDescription(kind_));
	}

	void ProjectWizard::ShowError(std::string_view message)
	{
		if (!ui_)
			return;
		ui_->SetText("wizard_error", message);
		ui_->SetVisible("wizard_error", true);
	}

	void ProjectWizard::ClearError()
	{
		if (!ui_)
			return;
		ui_->SetText("wizard_error", {});
		ui_->SetVisible("wizard_error", false);
	}

	void ProjectWizard::HandleNameChanged()
	{
		if (!ui_)
			return;
		const std::string current = Trim(GetInput("wizard_namespace"));
		if (!current.empty() && current != last_derived_namespace_)
			return;
		last_derived_namespace_ = DeriveNamespace(Trim(GetInput("wizard_name")));
		SetInput("wizard_namespace", last_derived_namespace_);
	}

	std::string ProjectWizard::GetInput(std::string_view id) const
	{
		if (!ui_)
			return {};
		Rml::ElementFormControl *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(
			ui_->Find(id));
		return control ? control->GetValue() : std::string();
	}

	void ProjectWizard::SetInput(std::string_view id, std::string_view value)
	{
		if (!ui_)
			return;
		Rml::ElementFormControl *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(
			ui_->Find(id));
		if (control)
			control->SetValue(std::string(value));
	}

} // namespace BlackLight::Editor
