#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BlackLight::Editor
{

	/// Identifies a node inside one parsed `.bui` document.
	using BuiNodeId = std::uint32_t;

	/// Marks a missing or invalid node identifier.
	constexpr BuiNodeId InvalidBuiNode = 0;

	/// Identifies the two kinds of nodes represented by the layout tree.
	enum class BuiNodeType : std::uint8_t
	{
		Element,
		Text
	};

	/// Describes one half-open byte range in the complete source document.
	struct BuiRange
	{
		std::size_t begin = 0;
		std::size_t end = 0;

		/// Returns the number of bytes in the range.
		std::size_t Size() const noexcept { return end >= begin ? end - begin : 0; }
		/// Returns whether the range is empty.
		bool Empty() const noexcept { return begin == end; }
	};

	/// Describes one attribute and its original source span.
	struct BuiAttribute
	{
		std::string name;
		std::string value;
		char quote = '"';
		BuiRange name_range;
		BuiRange value_range;
		BuiRange full_range;
	};

	/// Describes one element or text node in the layout tree.
	struct BuiNode
	{
		BuiNodeId id = InvalidBuiNode;
		BuiNodeType type = BuiNodeType::Element;
		BuiNodeId parent = InvalidBuiNode;
		std::vector<BuiNodeId> children;

		std::string tag;
		std::string text;
		bool self_closing = false;

		BuiRange open_tag;
		BuiRange tag_name;
		BuiRange content;
		BuiRange close_tag;
		std::vector<BuiAttribute> attributes;

		/// Returns the last attribute with the supplied name, or null.
		const BuiAttribute *FindAttribute(std::string_view attribute_name) const noexcept;
	};

	/// Carries a non-fatal parser warning or a fatal parse result.
	struct BuiParseError
	{
		std::string message;
		std::size_t line = 0;
	};

	/// Describes a new element or text node inserted through the model API.
	struct BuiNodeTemplate
	{
		BuiNodeType type = BuiNodeType::Element;
		std::string tag = "div";
		std::string text;
		std::vector<std::pair<std::string, std::string>> attributes;
	};

	/// Identifies the text segment replaced by one edit.
	struct BuiEditResult
	{
		std::string old_text;
		std::string new_text;
		std::string old_selection_path;
		std::string new_selection_path;
	};

	/// Owns the lossless source text and the parsed layout tree of one `.bui` document.
	///
	/// The model never normalizes or reformats untouched source. Every edit replaces
	/// one source range and then reparses the layout section. Node identifiers are
	/// session-local; callers persist selection through structural paths.
	class BuiDocumentModel final
	{
	public:
		BuiDocumentModel() = default;

		/// Parses a complete `.bui` file. A missing layout is fatal; malformed layout
		/// markup is recorded as a warning and parsed as far as possible.
		static bool Parse(std::string text, BuiDocumentModel &model,
			BuiParseError &error);

		/// Returns the complete current file contents.
		const std::string &GetText() const noexcept;
		/// Returns the raw styles section, excluding the `<styles>` tags.
		const std::string &GetStyles() const noexcept;
		/// Returns the raw layout section, excluding the `<layout>` tags.
		const std::string &GetLayout() const noexcept;
		/// Returns whether the source used CRLF line endings.
		bool UsesCrlf() const noexcept;
		/// Returns the currently stored warning, or an empty message.
		const BuiParseError &GetWarning() const noexcept;

		/// Returns a node by identifier, or null when it does not exist.
		const BuiNode *Find(BuiNodeId id) const noexcept;
		/// Returns the synthetic document root, or invalid when parsing failed.
		BuiNodeId GetRoot() const noexcept;
		/// Returns the model revision, incremented by every successful edit.
		std::uint64_t GetRevision() const noexcept;

		/// Returns the structural child-index path of a node, for example `0/2/1`.
		std::string PathOf(BuiNodeId id) const;
		/// Resolves a structural path, returning invalid for malformed or stale paths.
		BuiNodeId NodeAtPath(std::string_view path) const noexcept;
		/// Returns a path suitable for a newly inserted child of a parent.
		std::string ChildPath(BuiNodeId parent, std::size_t index) const;

		/// Sets or inserts one attribute. Returns false without changing text on failure.
		bool SetAttribute(BuiNodeId id, std::string_view name, std::string_view value,
			BuiEditResult *result = nullptr);
		/// Removes one attribute. Returns false without changing text on failure.
		bool RemoveAttribute(BuiNodeId id, std::string_view name,
			BuiEditResult *result = nullptr);
		/// Sets a text node's content, escaping markup characters.
		bool SetText(BuiNodeId id, std::string_view text, BuiEditResult *result = nullptr);
		/// Replaces the textual content of one element while preserving its child elements.
		bool SetElementText(BuiNodeId id, std::string_view text,
			BuiEditResult *result = nullptr);
		/// Inserts a node at the supplied child index.
		bool InsertNode(BuiNodeId parent, std::size_t index,
			const BuiNodeTemplate &node, BuiEditResult *result = nullptr);
		/// Removes a node and its complete source lines.
		bool RemoveNode(BuiNodeId id, BuiEditResult *result = nullptr);
		/// Moves a node under another parent and child index.
		bool MoveNode(BuiNodeId id, BuiNodeId new_parent, std::size_t index,
			BuiEditResult *result = nullptr);

		/// Returns whether `ancestor` is `node` or one of its ancestors.
		bool IsAncestorOrSelf(BuiNodeId ancestor, BuiNodeId node) const noexcept;

		/// Serializes the layout with a structural path attribute on every element.
		/// The returned markup is for preview only and never replaces model source.
		std::string BuildPreviewLayout() const;

		/// Parses an escaped XML entity value used by attributes and text nodes.
		static std::string DecodeEntities(std::string_view text);

	private:
		struct Section
		{
			BuiRange tag;
			BuiRange content;
			bool valid = false;
		};

		/// Rebuilds the node table from the current source text.
		void Rebuild();
		/// Rescans the container sections and refreshes style/layout slices.
		void RefreshSections();
		/// Locates the outer `<styles>` and `<layout>` sections.
		void ScanSections();

		std::string text_;
		std::string styles_;
		std::string layout_;
		Section styles_section_;
		Section layout_section_;
		std::vector<BuiNode> nodes_;
		BuiNodeId root_ = InvalidBuiNode;
		BuiParseError warning_;
		bool crlf_ = false;
		std::uint64_t revision_ = 0;
	};

} // namespace BlackLight::Editor
