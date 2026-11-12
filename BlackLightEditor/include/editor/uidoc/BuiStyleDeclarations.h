#pragma once

#include "editor/uidoc/BuiDocumentModel.h"

#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one `name: value` pair inside a style attribute or CSS block.
	///
	/// The ranges address the complete declaration in the owning string. `name_end`
	/// and `value_begin` delimit the spelling of the `:` separator so callers can
	/// replace a value without normalizing the original text.
	struct BuiStyleDeclaration
	{
		std::string name;
		std::string value;
		BuiRange full_range;
		BuiRange value_range;
		BuiRange name_range;
		BuiRange colon;
	};

	/// Parses a declaration list such as `left: 12dp; width: 40%; position: absolute`.
	///
	/// Honours quoting and nesting, ignores comments, and keeps every byte range
	/// usable against the original input. Malformed trailing declarations without a
	/// colon are skipped rather than failing the whole list.
	std::vector<BuiStyleDeclaration> ParseStyleDeclarations(std::string_view text);

	/// Returns the last declaration with the supplied property name, or null.
	const BuiStyleDeclaration *FindStyleDeclaration(
		const std::vector<BuiStyleDeclaration> &declarations, std::string_view name);

	/// Sets one property in a declaration list, appending it when absent.
	///
	/// Returns the rewritten list. Unrelated declarations, their order, their
	/// spelling, and every byte of whitespace and comments are preserved.
	std::string SetStyleDeclarationValue(std::string_view text, std::string_view name,
		std::string_view value);

	/// Removes one property from a declaration list. Returns the input unchanged
	/// when the property is absent.
	std::string RemoveStyleDeclaration(std::string_view text, std::string_view name);

	/// Removes every occurrence of a property, used before appending a fresh value.
	std::string RemoveAllStyleDeclarations(std::string_view text, std::string_view name);

	/// Joins a `name: value` pair onto a declaration list with a safe separator.
	std::string AppendStyleDeclaration(std::string_view text, std::string_view name,
		std::string_view value);

	/// Builds the selector the editor associates with one element node.
	///
	/// Returns `#id` when the element has an id, otherwise `.first-class`, and an
	/// empty string when neither attribute exists.
	std::string SelectorOfNode(const BuiNode &node);

	/// Splits a selector list on commas, trimming whitespace around each item.
	std::vector<std::string> SplitSelectorList(std::string_view selectors);

	/// Returns whether a comma-separated selector list can address one node.
	///
	/// Supports tag, `.class`, `#id`, and compound selectors built from those three
	/// forms. Descendant and pseudo selectors are reported as unsupported so the
	/// style panel can disable editing rather than miswrite them.
	bool SelectorTargetsNode(std::string_view selector_list, const BuiNode &node);

} // namespace BlackLight::Editor
