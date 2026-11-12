#include "editor/uidoc/BuiStyleSheet.h"

#include <algorithm>
#include <cctype>

namespace BlackLight::Editor
{

	namespace
	{

		/// Tests ASCII whitespace without changing signed character values.
		bool IsSpace(char value) noexcept
		{
			return std::isspace(static_cast<unsigned char>(value)) != 0;
		}

		/// Trims ASCII whitespace from both ends of a view.
		std::string_view Trim(std::string_view text) noexcept
		{
			while (!text.empty() && IsSpace(text.front()))
				text.remove_prefix(1);
			while (!text.empty() && IsSpace(text.back()))
				text.remove_suffix(1);
			return text;
		}

		/// Skips whitespace and comments starting at one offset.
		std::size_t SkipTrivia(std::string_view text, std::size_t position) noexcept
		{
			while (position < text.size())
			{
				if (IsSpace(text[position]))
				{
					++position;
					continue;
				}
				if (text.compare(position, 2, "/*") == 0)
				{
					const std::size_t close = text.find("*/", position + 2);
					position = close == std::string_view::npos ? text.size() : close + 2;
					continue;
				}
				break;
			}
			return position;
		}

		/// Finds a structural character, skipping quoted strings and comments.
		///
		/// Returns the offset, or `npos` when the stylesheet ends first. `depth`
		/// receives the brace nesting level at the found offset for callers that
		/// need to distinguish top-level rules from nested blocks.
		std::size_t FindStructural(std::string_view text, std::size_t position,
			char wanted, int &depth) noexcept
		{
			char quote = '\0';
			while (position < text.size())
			{
				const char character = text[position];
				if (quote != '\0')
				{
					if (character == '\\' && position + 1 < text.size())
					{
						position += 2;
						continue;
					}
					if (character == quote)
						quote = '\0';
					++position;
					continue;
				}
				if (text.compare(position, 2, "/*") == 0)
				{
					const std::size_t close = text.find("*/", position + 2);
					position = close == std::string_view::npos ? text.size() : close + 2;
					continue;
				}
				if (character == '"' || character == '\'')
				{
					quote = character;
					++position;
					continue;
				}
				if (character == wanted && depth == 0)
					return position;
				if (character == '{')
				{
					if (wanted == '}')
						++depth;
					else if (depth == 0)
						return position;
				}
				else if (character == '}' && wanted == '}')
				{
					if (depth == 0)
						return position;
					--depth;
				}
				++position;
			}
			return std::string_view::npos;
		}

		/// Returns the leading whitespace of the line that starts at one offset.
		std::string LineIndent(std::string_view text, std::size_t offset)
		{
			std::size_t begin = (std::min)(offset, text.size());
			while (begin > 0 && text[begin - 1] != '\n')
				--begin;
			std::size_t end = begin;
			while (end < text.size() && (text[end] == ' ' || text[end] == '\t'))
				++end;
			return std::string(text.substr(begin, end - begin));
		}

	} // namespace

	const BuiStyleDeclaration *BuiStyleRule::FindDeclaration(std::string_view name) const
	{
		return FindStyleDeclaration(declarations, name);
	}

	std::vector<BuiStyleRule> ParseStyleSheet(std::string_view styles)
	{
		std::vector<BuiStyleRule> rules;
		std::size_t position = 0;
		while (true)
		{
			position = SkipTrivia(styles, position);
			if (position >= styles.size())
				break;

			// A stray closing brace at top level is skipped instead of aborting.
			if (styles[position] == '}')
			{
				++position;
				continue;
			}

			const std::size_t selector_begin = position;
			int depth = 0;
			const std::size_t open = FindStructural(styles, position, '{', depth);
			if (open == std::string_view::npos)
				break;

			const std::string_view selector =
				Trim(styles.substr(selector_begin, open - selector_begin));
			int close_depth = 0;
			const std::size_t close = FindStructural(styles, open + 1, '}', close_depth);

			BuiStyleRule rule;
			rule.selector = std::string(selector);
			rule.selector_range = {selector_begin,
				selector_begin + selector.size()};
			rule.open_brace = {open, open + 1};
			rule.at_rule = !selector.empty() && selector.front() == '@';
			if (close == std::string_view::npos)
			{
				// Keep the tolerant body range so callers can still address the rule.
				rule.body_range = {open + 1, styles.size()};
				rule.close_brace = {styles.size(), styles.size()};
				rule.full_range = {selector_begin, styles.size()};
				rule.declarations = ParseStyleDeclarations(rule.body_range.Size() == 0
					? std::string_view()
					: styles.substr(rule.body_range.begin, rule.body_range.Size()));
				rules.push_back(std::move(rule));
				break;
			}

			rule.body_range = {open + 1, close};
			rule.close_brace = {close, close + 1};
			rule.full_range = {selector_begin, close + 1};
			rule.declarations = ParseStyleDeclarations(
				styles.substr(rule.body_range.begin, rule.body_range.Size()));
			rules.push_back(std::move(rule));
			position = close + 1;
		}
		return rules;
	}

	std::string SetRuleDeclarations(std::string_view styles, const BuiStyleRule &rule,
		std::string_view declarations)
	{
		if (rule.body_range.begin > rule.body_range.end ||
			rule.body_range.end > styles.size())
		{
			return std::string(styles);
		}

		std::string result(styles.substr(0, rule.body_range.begin));
		const bool multiline = styles.substr(rule.body_range.begin,
			rule.body_range.Size()).find('\n') != std::string_view::npos ||
			rule.declarations.size() > 1;
		if (multiline)
		{
			const std::string indent = LineIndent(styles, rule.selector_range.begin) + "  ";
			result += '\n';
			std::string_view remaining = declarations;
			while (!remaining.empty())
			{
				const std::size_t newline = remaining.find('\n');
				std::string_view line = Trim(remaining.substr(0, newline));
				if (!line.empty())
				{
					result += indent;
					result.append(line);
				}
				result += '\n';
				if (newline == std::string_view::npos)
					break;
				remaining.remove_prefix(newline + 1);
			}
			result += LineIndent(styles, rule.selector_range.begin);
		}
		else
		{
			result.append(Trim(declarations));
		}
		result.append(styles.substr(rule.body_range.end));
		return result;
	}

	std::string SetRuleDeclarationValue(std::string_view styles, const BuiStyleRule &rule,
		std::string_view name, std::string_view value)
	{
		std::string body(styles.substr(rule.body_range.begin, rule.body_range.Size()));
		const std::vector<BuiStyleDeclaration> declarations = ParseStyleDeclarations(body);
		const BuiStyleDeclaration *existing = FindStyleDeclaration(declarations, name);
		if (existing)
		{
			const std::size_t begin = existing->value_range.begin;
			const std::size_t end = existing->value_range.end;
			body.replace(begin, end - begin, std::string(Trim(value)));
			return SetRuleDeclarations(styles, rule, body);
		}
		body = AppendStyleDeclaration(body, name, value);
		return SetRuleDeclarations(styles, rule, body);
	}

	std::string AppendStyleRule(std::string_view styles, std::string_view selector,
		std::string_view declarations)
	{
		const std::string trimmed_selector(Trim(selector));
		if (trimmed_selector.empty())
			return std::string(styles);

		const std::vector<BuiStyleRule> rules = ParseStyleSheet(styles);
		std::string indent = "  ";
		for (const BuiStyleRule &rule : rules)
		{
			if (!rule.at_rule)
			{
				indent = LineIndent(styles, rule.selector_range.begin);
				break;
			}
		}

		std::string block;
		std::string_view remaining = declarations;
		bool wrote_any = false;
		while (!remaining.empty())
		{
			const std::size_t semicolon = remaining.find(';');
			const std::string_view item = Trim(remaining.substr(0, semicolon));
			if (!item.empty())
			{
				block += indent + "  ";
				block.append(item);
				block += ";\n";
				wrote_any = true;
			}
			if (semicolon == std::string_view::npos)
				break;
			remaining.remove_prefix(semicolon + 1);
		}
		if (!wrote_any)
			block.clear();

		std::string result(styles);
		std::size_t tail = result.size();
		while (tail > 0 && IsSpace(result[tail - 1]))
			--tail;
		if (tail < result.size())
			result.resize(tail);
		if (!result.empty())
			result += "\n\n";
		result += indent + trimmed_selector + " {\n" + block + indent + "}";
		result += styles.substr(tail);
		return result;
	}

	const BuiStyleRule *FindRuleForNode(const std::vector<BuiStyleRule> &rules,
		const BuiNode &node)
	{
		for (const BuiStyleRule &rule : rules)
		{
			if (rule.at_rule)
				continue;
			if (SelectorTargetsNode(rule.selector, node))
				return &rule;
		}
		return nullptr;
	}

} // namespace BlackLight::Editor
