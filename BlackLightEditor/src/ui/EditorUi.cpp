#include "editor/ui/EditorUi.h"

#include "rmlui/BuiDocument.h"
#include "rmlui/BuiLoader.h"

#include <RmlUi/Core.h>

#include <utility>

namespace BlackLight::Editor
{

	/// Bridges an RmlUi listener to a stored editor callback.
	class EditorUi::CallbackListener final : public Rml::EventListener
	{
	public:
		explicit CallbackListener(EventHandler handler) : handler_(std::move(handler)) {}

		void ProcessEvent(Rml::Event &event) override
		{
			if (handler_)
				handler_(event);
		}

	private:
		EventHandler handler_;
	};

	EditorUi::EditorUi() = default;

	EditorUi::~EditorUi() { Shutdown(); }

	bool EditorUi::Initialize(Rml::Context &context, std::string_view bui_content,
		const EditorTheme &theme, std::string &error)
	{
		Shutdown();

		std::unique_ptr<BlackLight::UI::BuiDocument> document;
		BlackLight::UI::BuiLoadError load_error;
		if (!BlackLight::UI::BuiLoader::Parse(bui_content, document, load_error))
		{
			error = load_error.message;
			if (load_error.line > 0)
				error += " (line " + std::to_string(load_error.line) + ")";
			return false;
		}

		base_styles_ = document->GetStyles();

		BlackLight::UI::BuiDocument themed;
		themed.SetStyles(ThemeRegistry::Apply(theme, base_styles_));
		themed.SetLayout(document->GetLayout());

		BlackLight::UI::BuiBuildError build_error;
		if (!BlackLight::UI::BuiTree::Build(context, themed, tree_, build_error))
		{
			error = build_error.message;
			tree_.reset();
			base_styles_.clear();
			return false;
		}

		tree_->Show();
		return true;
	}

	void EditorUi::Shutdown()
	{
		ClearListeners();
		tree_.reset();
		base_styles_.clear();
	}

	void EditorUi::SetTheme(const EditorTheme &theme)
	{
		Rml::ElementDocument *document = GetDocument();
		if (!document)
			return;
		auto sheet =
			Rml::Factory::InstanceStyleSheetString(ThemeRegistry::Apply(theme, base_styles_));
		if (sheet)
			document->SetStyleSheetContainer(std::move(sheet));
	}

	bool EditorUi::IsReady() const noexcept { return tree_ && tree_->GetDocument(); }

	Rml::ElementDocument *EditorUi::GetDocument() const noexcept
	{
		return tree_ ? tree_->GetDocument() : nullptr;
	}

	Rml::Element *EditorUi::Find(std::string_view id) const
	{
		Rml::ElementDocument *document = GetDocument();
		return document ? document->GetElementById(std::string(id)) : nullptr;
	}

	void EditorUi::SetText(std::string_view id, std::string_view text)
	{
		if (Rml::Element *element = Find(id))
			element->SetInnerRML(Escape(text));
	}

	void EditorUi::SetHtml(std::string_view id, std::string_view html)
	{
		if (Rml::Element *element = Find(id))
			element->SetInnerRML(std::string(html));
	}

	void EditorUi::SetClass(std::string_view id, std::string_view class_name, bool active)
	{
		if (Rml::Element *element = Find(id))
			element->SetClass(std::string(class_name), active);
	}

	void EditorUi::SetVisible(std::string_view id, bool visible)
	{
		SetClass(id, "hidden", !visible);
	}

	void EditorUi::SetProperty(std::string_view id, std::string_view property,
		std::string_view value)
	{
		if (Rml::Element *element = Find(id))
			element->SetProperty(std::string(property), std::string(value));
	}

	bool EditorUi::AddListener(std::string_view scope, std::string_view element_id,
		std::string_view event, EventHandler handler)
	{
		Rml::Element *element = Find(element_id);
		if (!element)
			return false;
		AddListener(scope, element, event, std::move(handler));
		return true;
	}

	void EditorUi::AddListener(std::string_view scope, Rml::Element *element,
		std::string_view event, EventHandler handler)
	{
		if (!element || !handler)
			return;
		auto listener = std::make_unique<CallbackListener>(std::move(handler));
		element->AddEventListener(std::string(event), listener.get());
		listeners_.push_back({std::string(scope), element, std::string(event), std::move(listener)});
	}

	void EditorUi::ClearListeners(std::string_view scope)
	{
		for (auto it = listeners_.begin(); it != listeners_.end();)
		{
			if (it->scope != scope)
			{
				++it;
				continue;
			}
			if (it->element)
				it->element->RemoveEventListener(it->event, it->listener.get());
			it = listeners_.erase(it);
		}
	}

	void EditorUi::ClearListeners()
	{
		for (auto &entry : listeners_)
		{
			if (entry.element)
				entry.element->RemoveEventListener(entry.event, entry.listener.get());
		}
		listeners_.clear();
	}

	std::string EditorUi::Escape(std::string_view text)
	{
		std::string result;
		result.reserve(text.size());
		for (const char character : text)
		{
			switch (character)
			{
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '"': result += "&quot;"; break;
			default: result.push_back(character); break;
			}
		}
		return result;
	}

	std::string EditorUi::FindAttribute(Rml::Element *element, Rml::Element *stop,
		std::string_view name)
	{
		for (Rml::Element *current = element; current && current != stop;
			current = current->GetParentNode())
		{
			const std::string value = current->GetAttribute(std::string(name), std::string());
			if (!value.empty())
				return value;
		}
		return {};
	}

} // namespace BlackLight::Editor
