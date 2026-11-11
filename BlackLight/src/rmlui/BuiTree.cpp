#include "rmlui/BuiTree.h"
#include "rmlui/BuiDocument.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <RmlUi/Core.h>

namespace BlackLight::UI
{

	namespace
	{

		/// Records the first document construction failure.
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

	/// Creates a wrapper without a document.
	BuiTree::BuiTree() = default;

	/// Closes the document while its context is still alive.
	BuiTree::~BuiTree()
	{
		Close();
	}

	/// Loads a full RML document so the native parser processes its head and styles.
	bool BuiTree::Build(Rml::Context &context, const BuiDocument &bui_document,
						std::unique_ptr<BuiTree> &tree, BuiBuildError &error)
	{
		auto new_tree = std::unique_ptr<BuiTree>(new BuiTree());

		const std::string &styles = bui_document.GetStyles();
		const std::string &layout = bui_document.GetLayout();
		if (layout.empty())
		{
			SetBuildError(error, ".bui layout is empty");
			return false;
		}

		const std::string full_rml = "<rml>\n<head>\n<style>\n" + styles +
			"\n</style>\n</head>\n<body>\n" + layout + "\n</body>\n</rml>\n";
		new_tree->document_ = context.LoadDocumentFromMemory(full_rml);
		if (!new_tree->document_)
		{
			SetBuildError(error, "Failed to load RmlUi document from .bui content");
			return false;
		}

		if (!new_tree->document_->GetNumChildren())
		{
			SetBuildError(error, "Failed to load layout HTML - document has no children");
			new_tree->Close();
			return false;
		}

		// Map all element IDs for later lookup
		MapElementIds(*new_tree->document_, new_tree->elements_by_id_);

		tree = std::move(new_tree);
		error = {};
		return true;
	}

	/// Returns the document until Close invalidates it.
	Rml::ElementDocument *BuiTree::GetDocument() const noexcept
	{
		return document_;
	}

	/// Resolves an element from the document's initial ID index.
	Rml::Element *BuiTree::FindById(std::string_view id) const
	{
		auto it = elements_by_id_.find(std::string(id));
		if (it != elements_by_id_.end())
		{
			return it->second;
		}
		return nullptr;
	}

	/// Makes the loaded document visible.
	void BuiTree::Show()
	{
		if (document_)
		{
			document_->Show();
		}
	}

	/// Unloads the document and clears the borrowed element pointers.
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
