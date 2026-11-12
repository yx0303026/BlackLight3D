#include "editor/uidoc/BuiStyleDeclarations.h"

#include <algorithm>
#include <cstdio>
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

		/// Compares two property or tag names case-insensitively.
		bool EqualsIgnoreCase(std::string_view left, std::string_view right) noexcept
		{
			if (left.size() != right.size())
				return false;
			for (std::size_t index = 0; index < left.size(); ++index)
			{
				if (std::tolower(static_cast<unsigned char>(left[index])) !=
					std::tolower(static_cast<unsigned char>(right[index])))
				{
					return false;
				}
			}
			return true;
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

		/// Skips whitespace and CSS comments starting at one offset.
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
					position = close == std::string_view::npos
						? text.size()
						: close + 2;
					continue;
				}
				break;
			}
			return position;
		}

		/// Returns whether a declaration name matches the requested property.
		bool Matches(const BuiStyleDeclaration &declaration,
			std::string_view name) noexcept
		{
			return EqualsIgnoreCase(declaration.name, name);
		}

		/// Appends one declaration, inserting before any trailing whitespace.
		std::string AppendBeforeTrailingSpace(std::string_view text,
			std::string_view name, std::string_view value)
		{
			std::size_t tail = text.size();
			while (tail > 0 && IsSpace(text[tail - 1]))
				--tail;

			const bool needs_separator =
				tail > 0 && text[tail - 1] != ';' && text[tail - 1] != '{';

			std::string result;
			result.reserve(text.size() + name.size() + value.size() + 4);
			result.append(text.substr(0, tail));
			if (needs_separator)
				result += ';';
			if (tail > 0 || needs_separator)
				result += ' ';
			result.append(name);
			result.append(": ");
			result.append(value);
			result.append(text.substr(tail));
			return result;
		}

		/// Parses one simple compound selector into its tag, id, and classes.
		struct CompoundSelector
		{
			std::string tag;
			std::string id;
			std::vector<std::string> classes;
		};

		/// Returns whether one selector character may appear inside a name.
		bool IsSelectorNameChar(char character) noexcept
		{
			const unsigned char value = static_cast<unsigned char>(character);
			return std::isalnum(value) != 0 || character == '_' || character == '-' ||
				value >= 0x80;
		}

		/// Parses `tag#id.class1.class2`; returns false for combinators or pseudo state.
		bool ParseCompoundSelector(std::string_view selector, CompoundSelector &result)
		{
			result = {};
			if (selector.empty())
				return false;
			if (selector.find_first_of(" \t\r\n>+~:[(") != std::string_view::npos)
				return false;

			std::size_t position = 0;
			if (selector[0] != '.' && selector[0] != '#')
			{
				std::size_t end = 0;
				while (end < selector.size() && IsSelectorNameChar(selector[end]))
					++end;
				if (end == 0)
					return false;
				result.tag = std::string(selector.substr(0, end));
				position = end;
			}

			while (position < selector.size())
			{
				const char marker = selector[position];
				if (marker != '.' && marker != '#')
					return false;
				std::size_t end = position + 1;
				while (end < selector.size() && IsSelectorNameChar(selector[end]))
					++end;
				if (end == position + 1)
					return false;
				std::string identifier(selector.substr(position + 1, end - position - 1));
				if (marker == '.')
					result.classes.push_back(std::move(identifier));
				else
					result.id = std::move(identifier);
				position = end;
			}
			return true;
		}

		/// Returns whether the class attribute value contains a class token.
		bool HasClassToken(std::string_view classes, std::string_view token)
		{
			std::size_t begin = 0;
			while (begin < classes.size())
			{
				while (begin < classes.size() && IsSpace(classes[begin]))
					++begin;
				std::size_t end = begin;
				while (end < classes.size() && !IsSpace(classes[end]))
					++end;
				if (classes.substr(begin, end - begin) == token)
					return true;
				begin = end;
			}
			return false;
		}

	} // namespace

	std::vector<BuiStyleDeclaration> ParseStyleDeclarations(std::string_view text)
	{
		std::vector<BuiStyleDeclaration> declarations;
		std::size_t position = 0;
		while (true)
		{
			position = SkipTrivia(text, position);
			if (position >= text.size())
				break;

			const std::size_t declaration_begin = position;
			char quote = '\0';
			int parentheses = 0;
			std::size_t colon = std::string_view::npos;
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
				if (character == '"' || character == '\'')
				{
					quote = character;
					++position;
					continue;
				}
				if (character == '(')
				{
					++parentheses;
					++position;
					continue;
				}
				if (character == ')')
				{
					parentheses = (std::max)(0, parentheses - 1);
					++position;
					continue;
				}
				if (character == ':' && parentheses == 0 &&
					colon == std::string_view::npos)
				{
					colon = position;
					++position;
					continue;
				}
				if ((character == ';' || character == '}') && parentheses == 0)
					break;
				++position;
			}

			const std::size_t declaration_end = position;
			if (colon == std::string_view::npos)
			{
				// Not a declaration; skip the fragment rather than failing the list.
				if (position < text.size())
					++position;
				continue;
			}

			const std::string_view name_view =
				Trim(text.substr(declaration_begin, colon - declaration_begin));
			if (name_view.empty())
			{
				if (position < text.size() && text[position] == ';')
					++position;
				continue;
			}

			std::size_t value_begin = colon + 1;
			while (value_begin < declaration_end && IsSpace(text[value_begin]))
				++value_begin;
			std::size_t value_end = declaration_end;
			while (value_end > value_begin && IsSpace(text[value_end - 1]))
				--value_end;

			// The full range starts at the raw declaration start (after any comment
			// trivia) so removals take the leading whitespace with them.
			const std::size_t raw_begin = declaration_begin;
			BuiStyleDeclaration declaration;
			std::size_t name_begin = raw_begin;
			while (name_begin < colon && IsSpace(text[name_begin]))
				++name_begin;
			declaration.name = std::string(name_view);
			declaration.value = std::string(text.substr(value_begin, value_end - value_begin));
			declaration.name_range = {name_begin, colon};
			declaration.colon = {colon, colon + 1};
			declaration.value_range = {value_begin, value_end};
			declaration.full_range = {raw_begin,
				value_end > value_begin ? value_end : colon + 1};
			declarations.push_back(std::move(declaration));

			if (position < text.size() && text[position] == ';')
				++position;
		}
		return declarations;
	}

	const BuiStyleDeclaration *FindStyleDeclaration(
		const std::vector<BuiStyleDeclaration> &declarations, std::string_view name)
	{
		for (auto it = declarations.rbegin(); it != declarations.rend(); ++it)
		{
			if (Matches(*it, name))
				return &*it;
		}
		return nullptr;
	}

	std::string RemoveStyleDeclaration(std::string_view text, std::string_view name)
	{
		const std::vector<BuiStyleDeclaration> declarations = ParseStyleDeclarations(text);
		const BuiStyleDeclaration *declaration = FindStyleDeclaration(declarations, name);
		if (!declaration)
			return std::string(text);

		std::size_t begin = declaration->full_range.begin;
		std::size_t end = declaration->full_range.end;
		while (end < text.size() && IsSpace(text[end]))
			++end;
		if (end < text.size() && text[end] == ';')
		{
			++end;
			// Take the separator's trailing space so the next declaration keeps its
			// own indentation instead of inheriting a stray leading blank.
			while (end < text.size() && IsSpace(text[end]) && text[end] != '\n')
				++end;
		}
		// Absorb the indentation only up to the start of the line, so removing the
		// first declaration does not drag the next one's indentation with it.
		std::size_t line_begin = begin;
		while (line_begin > 0 && text[line_begin - 1] != '\n')
			--line_begin;
		while (begin > line_begin && IsSpace(text[begin - 1]))
			--begin;

		std::string result(text.substr(0, begin));
		result.append(text.substr(end));
		return result;
	}

	std::string RemoveAllStyleDeclarations(std::string_view text, std::string_view name)
	{
		std::string result(text);
		while (FindStyleDeclaration(ParseStyleDeclarations(result), name))
		{
			std::string next = RemoveStyleDeclaration(result, name);
			if (next == result)
				break;
			result = std::move(next);
		}
		return result;
	}

	std::string SetStyleDeclarationValue(std::string_view text, std::string_view name,
		std::string_view value)
	{
		const std::string trimmed = std::string(Trim(value));
		if (trimmed.empty())
			return RemoveStyleDeclaration(text, name);

		const std::vector<BuiStyleDeclaration> declarations = ParseStyleDeclarations(text);
		const BuiStyleDeclaration *declaration = FindStyleDeclaration(declarations, name);
		if (!declaration)
			return AppendBeforeTrailingSpace(text, name, trimmed);

		// The last declaration wins in CSS, so only its value bytes need rewriting.
		std::string result(text.substr(0, declaration->value_range.begin));
		result.append(trimmed);
		result.append(text.substr(declaration->value_range.end));
		return result;
	}

	std::string AppendStyleDeclaration(std::string_view text, std::string_view name,
		std::string_view value)
	{
		const std::string trimmed = std::string(Trim(value));
		if (trimmed.empty())
			return std::string(text);
		return AppendBeforeTrailingSpace(text, name, trimmed);
	}

	std::string SelectorOfNode(const BuiNode &node)
	{
		if (const BuiAttribute *id = node.FindAttribute("id"); id && !id->value.empty())
			return "#" + id->value;
		if (const BuiAttribute *classes = node.FindAttribute("class"))
		{
			std::size_t begin = 0;
			while (begin < classes->value.size() && IsSpace(classes->value[begin]))
				++begin;
			std::size_t end = begin;
			while (end < classes->value.size() && !IsSpace(classes->value[end]))
				++end;
			if (end > begin)
				return "." + classes->value.substr(begin, end - begin);
		}
		return {};
	}

	std::vector<std::string> SplitSelectorList(std::string_view selectors)
	{
		std::vector<std::string> result;
		std::size_t begin = 0;
		while (begin <= selectors.size())
		{
			const std::size_t comma = selectors.find(',', begin);
			const std::string_view item = Trim(selectors.substr(begin,
				comma == std::string_view::npos ? selectors.size() - begin : comma - begin));
			if (!item.empty())
				result.emplace_back(item);
			if (comma == std::string_view::npos)
				break;
			begin = comma + 1;
		}
		return result;
	}

	bool SelectorTargetsNode(std::string_view selector_list, const BuiNode &node)
	{
		if (node.type != BuiNodeType::Element)
			return false;

		std::string_view id;
		std::string_view classes;
		if (const BuiAttribute *attribute = node.FindAttribute("id"))
			id = attribute->value;
		if (const BuiAttribute *attribute = node.FindAttribute("class"))
			classes = attribute->value;

		for (const std::string &selector : SplitSelectorList(selector_list))
		{
			CompoundSelector compound;
			if (!ParseCompoundSelector(selector, compound))
				continue;
			if (!compound.tag.empty() && !EqualsIgnoreCase(compound.tag, node.tag))
				continue;
			if (!compound.id.empty() && compound.id != id)
				continue;
			bool classes_match = true;
			for (const std::string &class_name : compound.classes)
			{
				if (!HasClassToken(classes, class_name))
				{
					classes_match = false;
					break;
				}
			}
			if (classes_match)
				return true;
		}
		return false;
	}

} // namespace BlackLight::Editor
