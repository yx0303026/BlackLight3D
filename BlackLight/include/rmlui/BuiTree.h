#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

// Transparent hash and equality for string_view lookup in unordered_map
namespace BlackLight::UI::detail
{
	struct StringHash
	{
		using is_transparent = void;
		[[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
		[[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
		[[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
	};

	struct StringEqual
	{
		using is_transparent = void;
		[[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
	};
}

namespace Rml
{
	class Context;
	class Element;
	class ElementDocument;
}

namespace BlackLight::UI
{

	class BuiDocument;

	/// Carries a runtime error raised while creating native RmlUi elements from .bui files.
	struct BuiBuildError
	{
		/// Describes the first document or element creation failure.
		std::string message;
	};

	/// Builds and owns the RmlUi document from a loaded .bui file.
	/// This is the direct replacement for the HTML parser pipeline.
	class BuiTree final
	{
	public:
		/// Closes the owned document; its RmlUi context must still be alive.
		~BuiTree();

		/// Builds a document directly from BuiDocument (raw RmlUi HTML+CSS).
		static bool Build(Rml::Context &context, const BuiDocument &bui_document,
						  std::unique_ptr<BuiTree> &tree, BuiBuildError &error);

		/// Returns the root RmlUi document, or nullptr after Close.
		Rml::ElementDocument *GetDocument() const noexcept;

		/// Finds a native RmlUi element by its id attribute.
		Rml::Element *FindById(std::string_view id) const;

		/// Shows the document if it has not been closed.
		void Show();

		/// Closes the document and invalidates all native element lookups.
		void Close();

		/// Prevents copying because a wrapper uniquely owns a context-bound document.
		BuiTree(const BuiTree &) = delete;
		/// Prevents copying because a wrapper uniquely owns a context-bound document.
		BuiTree &operator=(const BuiTree &) = delete;
		/// Prevents moving a wrapper while native document pointers are indexed.
		BuiTree(BuiTree &&) = delete;
		/// Prevents moving a wrapper while native document pointers are indexed.
		BuiTree &operator=(BuiTree &&) = delete;

	private:
		/// Creates an empty wrapper before the document is populated.
		BuiTree();

		Rml::ElementDocument *document_ = nullptr;
		std::unordered_map<std::string, Rml::Element *, detail::StringHash, detail::StringEqual> elements_by_id_;
	};

} // namespace BlackLight::UI
