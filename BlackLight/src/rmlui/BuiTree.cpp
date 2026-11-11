#include "rmlui/BuiTree.h"
#include "rmlui/BuiDocument.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <RmlUi/Core.h>
#include <RmlUi/Core/StreamMemory.h>
#include <RmlUi/Core/StyleSheetContainer.h>

namespace BlackLight::UI
{

	namespace
	{

		void SetBuildError(BuiBuildError &error, std::string message)
		{
			error.message = std::move(message);
		}

		/// Builds element ID mapping after RmlUi tree creation.
		void MapElementIds(Rml::Element &element, std::unordered_map<std::string, Rml::Element *> &elements_by_id)
		{
			const std::string &id = element.GetId();
			if (!id.empty())
			{
				elements_by_id[id] = &element;
			}

			for (int i = 0; i < element.GetNumChildren(); ++i)
			{
				Rml::Element *child = element.GetChild(i);
				if (child)
				{
					MapElementIds(*child, elements_by_id);
				}
			}
		}

	}

	BuiTree::BuiTree() = default;

	BuiTree::~BuiTree()
	{
		Close();
	}

	bool BuiTree::Build(Rml::Context &context, const BuiDocument &bui_document,
						std::unique_ptr<BuiTree> &tree, BuiBuildError &error)
	{
		auto new_tree = std::unique_ptr<BuiTree>(new BuiTree());

		new_tree->document_ = context.CreateDocument();
		if (!new_tree->document_)
		{
			SetBuildError(error, "Failed to create RmlUi document from context");
			return false;
		}

		// Load stylesheet
		const std::string &styles = bui_document.GetStyles();
		if (!styles.empty())
		{
			auto stylesheet_container = std::make_shared<Rml::StyleSheetContainer>();
			Rml::StreamMemory stream(
				reinterpret_cast<const Rml::byte *>(styles.data()),
				styles.size());

			if (!stylesheet_container->LoadStyleSheetContainer(&stream))
			{
				SetBuildError(error, "Failed to load CSS stylesheet");
				new_tree->Close();
				return false;
			}

			new_tree->document_->SetStyleSheetContainer(stylesheet_container);
		}

		// Load layout (raw RmlUi HTML). RmlUi 6.3 parses document markup through
		// Context::LoadDocumentFromMemory; ElementDocument no longer exposes the
		// old LoadChildrenFromStream/GetBody helpers.
		const std::string &layout = bui_document.GetLayout();
		if (!layout.empty())
		{
			new_tree->document_->SetInnerRML(layout);
			if (!new_tree->document_->GetNumChildren())
			{
				SetBuildError(error, "Failed to load layout HTML");
				new_tree->Close();
				return false;
			}
		}

		// Map all element IDs for later lookup
		MapElementIds(*new_tree->document_, new_tree->elements_by_id_);

		tree = std::move(new_tree);
		return true;
	}

	Rml::ElementDocument *BuiTree::GetDocument() const noexcept
	{
		return document_;
	}

	Rml::Element *BuiTree::FindById(std::string_view id) const
	{
		auto it = elements_by_id_.find(std::string(id));
		if (it != elements_by_id_.end())
		{
			return it->second;
		}
		return nullptr;
	}

	void BuiTree::Show()
	{
		if (document_)
		{
			document_->Show();
		}
	}

	void BuiTree::Close()
	{
		if (document_)
		{
			document_->Close();
			document_ = nullptr;
		}
		elements_by_id_.clear();
	}

}
