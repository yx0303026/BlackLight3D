#pragma once

#include "editor/uidoc/BuiDocumentModel.h"
#include "editor/uidoc/BuiStyleDeclarations.h"

#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one CSS rule inside the `<styles>` section.
	///
	/// Ranges address the complete stylesheet text. `body_range` covers the bytes
	/// between the braces, so a rule can be rewritten without touching its selector
	/// or the surrounding formatting.
	struct BuiStyleRule
	{
		std::string selector;
		BuiRange selector_range;
		BuiRange open_brace;
		BuiRange body_range;
		BuiRange close_brace;
		BuiRange full_range;
		bool at_rule = false;
		std::vector<BuiStyleDeclaration> declarations;

		/// Returns the last declaration with the supplied property name, or null.
		const BuiStyleDeclaration *FindDeclaration(std::string_view name) const;
	};

	/// Splits a `<styles>` section into rules while preserving every source byte.
	///
	/// At-rules such as `@media` are reported but never edited. Comments, nested
	/// blocks, and quoted strings are skipped so braces inside them do not split a
	/// rule. The parser is tolerant: unclosed rules are reported with empty ranges
	/// rather than failing the whole sheet.
	std::vector<BuiStyleRule> ParseStyleSheet(std::string_view styles);

	/// Replaces the declaration block of one rule.
	///
	/// `declarations` is the raw text to place between the braces. Returns the input
	/// unchanged when the rule range is invalid.
	std::string SetRuleDeclarations(std::string_view styles, const BuiStyleRule &rule,
		std::string_view declarations);

	/// Sets one property inside one rule, creating the rule body when empty.
	std::string SetRuleDeclarationValue(std::string_view styles, const BuiStyleRule &rule,
		std::string_view name, std::string_view value);

	/// Appends `selector { declarations }` to a stylesheet with tidy indentation.
	///
	/// The new rule mirrors the indentation of the last top-level rule when one
	/// exists, otherwise it uses a two-space indent relative to the section.
	std::string AppendStyleRule(std::string_view styles, std::string_view selector,
		std::string_view declarations);

	/// Returns the first rule whose complete selector list can address one node.
	const BuiStyleRule *FindRuleForNode(const std::vector<BuiStyleRule> &rules,
		const BuiNode &node);

} // namespace BlackLight::Editor
