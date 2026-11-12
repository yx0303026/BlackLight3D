#include "editor/uidoc/BuiDocumentModel.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Tests ASCII whitespace without changing signed character values.
		bool IsSpace(char value) noexcept
		{
			return std::isspace(static_cast<unsigned char>(value)) != 0;
		}

		/// Compares tag names case-insensitively.
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

		/// Returns the lower-case spelling of a tag name.
		std::string Lower(std::string_view text)
		{
			std::string result(text);
			for (char &character : result)
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			return result;
		}

		/// Returns whether a tag is an RmlUi-style void element.
		bool IsVoidElement(std::string_view tag) noexcept
		{
			static constexpr const char *void_elements[] = {
				"area", "base", "br", "col", "embed", "hr", "img", "input",
				"link", "meta", "param", "source", "track", "wbr"};
			for (const char *element : void_elements)
			{
				if (EqualsIgnoreCase(tag, element))
					return true;
			}
			return false;
		}

		/// Returns whether a tag keeps its content as raw text.
		bool IsRawTextElement(std::string_view tag) noexcept
		{
			return EqualsIgnoreCase(tag, "style") || EqualsIgnoreCase(tag, "script");
		}

		/// Converts a byte offset to a one-based line number.
		std::size_t LineAt(std::string_view text, std::size_t offset) noexcept
		{
			std::size_t line = 1;
			const std::size_t end = (std::min)(offset, text.size());
			for (std::size_t index = 0; index < end; ++index)
			{
				if (text[index] == '\n')
					++line;
			}
			return line;
		}

		/// Escapes text for HTML character data.
		std::string EscapeText(std::string_view text)
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
				default: result.push_back(character); break;
				}
			}
			return result;
		}

		/// Escapes text for a double-quoted attribute value.
		std::string EscapeAttribute(std::string_view text)
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

		/// Returns the leading whitespace of the line containing a zero-based offset.
		std::string LineIndent(std::string_view text, std::size_t offset)
		{
			std::size_t line_begin = (std::min)(offset, text.size());
			while (line_begin > 0 && text[line_begin - 1] != '\n')
				--line_begin;
			std::size_t line_end = line_begin;
			while (line_end < text.size() && (text[line_end] == ' ' || text[line_end] == '\t'))
				++line_end;
			return std::string(text.substr(line_begin, line_end - line_begin));
		}

		/// Returns the beginning of the line containing an offset.
		std::size_t LineBegin(std::string_view text, std::size_t offset) noexcept
		{
			std::size_t position = (std::min)(offset, text.size());
			while (position > 0 && text[position - 1] != '\n')
				--position;
			return position;
		}

		/// Returns the offset just after the next newline, or the end of text.
		std::size_t LineEnd(std::string_view text, std::size_t offset) noexcept
		{
			const std::size_t newline = text.find('\n', (std::min)(offset, text.size()));
			return newline == std::string_view::npos ? text.size() : newline + 1;
		}

		/// Detects whether the first newline is CRLF.
		bool DetectCrlf(std::string_view text) noexcept
		{
			const std::size_t newline = text.find('\n');
			return newline != std::string_view::npos && newline > 0 && text[newline - 1] == '\r';
		}

		/// Appends the newline spelling already used by the document.
		void AppendNewline(std::string &text, bool crlf)
		{
			text += crlf ? "\r\n" : "\n";
		}

		/// Parses one attribute and records whether scanning succeeded.
		bool ParseAttribute(std::string_view source, std::size_t &position,
			std::size_t open_tag_end, BuiAttribute &attribute)
		{
			while (position < open_tag_end && IsSpace(source[position]))
				++position;
			if (position >= open_tag_end || source[position] == '>' ||
				source[position] == '/')
			{
				return false;
			}

			const std::size_t name_begin = position;
			while (position < open_tag_end && !IsSpace(source[position]) &&
				source[position] != '=' && source[position] != '>' && source[position] != '/')
			{
				++position;
			}
			if (position == name_begin)
				return false;
			attribute.name = std::string(source.substr(name_begin, position - name_begin));
			attribute.name_range = {name_begin, position};

			while (position < open_tag_end && IsSpace(source[position]))
				++position;
			if (position >= open_tag_end || source[position] != '=')
			{
				attribute.value.clear();
				attribute.quote = '\0';
				attribute.value_range = {position, position};
				attribute.full_range = {name_begin, position};
				return true;
			}

			++position;
			while (position < open_tag_end && IsSpace(source[position]))
				++position;
			if (position >= open_tag_end)
				return false;

			if (source[position] == '"' || source[position] == '\'')
			{
				attribute.quote = source[position];
				++position;
				const std::size_t value_begin = position;
				while (position < open_tag_end && source[position] != attribute.quote)
					++position;
				attribute.value_range = {value_begin, position};
				attribute.value = BuiDocumentModel::DecodeEntities(
					source.substr(value_begin, position - value_begin));
				if (position < open_tag_end)
					++position;
			}
			else
			{
				const std::size_t value_begin = position;
				while (position < open_tag_end && !IsSpace(source[position]) &&
					source[position] != '>')
				{
					++position;
				}
				attribute.quote = '\0';
				attribute.value_range = {value_begin, position};
				attribute.value = BuiDocumentModel::DecodeEntities(
					source.substr(value_begin, position - value_begin));
			}

			attribute.full_range = {name_begin, position};
			return true;
		}

	} // namespace

	const BuiAttribute *BuiNode::FindAttribute(std::string_view attribute_name) const noexcept
	{
		for (auto it = attributes.rbegin(); it != attributes.rend(); ++it)
		{
			if (EqualsIgnoreCase(it->name, attribute_name))
				return &*it;
		}
		return nullptr;
	}

	std::string BuiDocumentModel::DecodeEntities(std::string_view text)
	{
		std::string result;
		result.reserve(text.size());
		for (std::size_t index = 0; index < text.size();)
		{
			if (text[index] != '&')
			{
				result.push_back(text[index++]);
				continue;
			}

			const std::size_t semicolon = text.find(';', index + 1);
			if (semicolon == std::string_view::npos || semicolon - index > 12)
			{
				result.push_back(text[index++]);
				continue;
			}

			const std::string_view entity = text.substr(index + 1, semicolon - index - 1);
			if (entity == "amp") result.push_back('&');
			else if (entity == "lt") result.push_back('<');
			else if (entity == "gt") result.push_back('>');
			else if (entity == "quot") result.push_back('"');
			else if (entity == "apos") result.push_back('\'');
			else if (entity == "nbsp") result.push_back(' ');
			else if (!entity.empty() && entity[0] == '#')
			{
				unsigned long code = 0;
				bool valid = false;
				try
				{
					if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X'))
					{
						code = std::stoul(std::string(entity.substr(2)), nullptr, 16);
						valid = true;
					}
					else if (entity.size() > 1)
					{
						code = std::stoul(std::string(entity.substr(1)), nullptr, 10);
						valid = true;
					}
				}
				catch (...)
				{
					valid = false;
				}

				if (valid && code <= 0x7F)
					result.push_back(static_cast<char>(code));
				else
				{
					result.append(text.substr(index, semicolon - index + 1));
				}
			}
			else
			{
				result.append(text.substr(index, semicolon - index + 1));
			}
			index = semicolon + 1;
		}
		return result;
	}

	bool BuiDocumentModel::Parse(std::string text, BuiDocumentModel &model,
		BuiParseError &error)
	{
		error = {};
		BuiDocumentModel parsed;
		parsed.text_ = std::move(text);
		parsed.crlf_ = DetectCrlf(parsed.text_);
		parsed.ScanSections();
		if (!parsed.styles_section_.valid)
		{
			error = {"Missing or unclosed <styles> section",
				LineAt(parsed.text_, parsed.styles_section_.tag.begin)};
			return false;
		}
		if (!parsed.layout_section_.valid)
		{
			error = {"Missing or unclosed <layout> section",
				LineAt(parsed.text_, parsed.layout_section_.tag.begin)};
			return false;
		}

		parsed.styles_ = parsed.text_.substr(parsed.styles_section_.content.begin,
			parsed.styles_section_.content.Size());
		parsed.layout_ = parsed.text_.substr(parsed.layout_section_.content.begin,
			parsed.layout_section_.content.Size());
		parsed.Rebuild();
		model = std::move(parsed);
		error = model.warning_;
		return true;
	}

	void BuiDocumentModel::ScanSections()
	{
		styles_section_ = {};
		layout_section_ = {};
		std::size_t position = 0;
		while (position < text_.size())
		{
			const std::size_t open = text_.find('<', position);
			if (open == std::string::npos)
				break;
			if (text_.compare(open, 4, "<!--") == 0)
			{
				const std::size_t close = text_.find("-->", open + 4);
				position = close == std::string::npos ? text_.size() : close + 3;
				continue;
			}
			if (open + 1 >= text_.size())
				break;
			if (text_[open + 1] == '!' || text_[open + 1] == '?')
			{
				const std::size_t close = text_.find('>', open + 2);
				position = close == std::string::npos ? text_.size() : close + 1;
				continue;
			}

			std::size_t name_begin = open + 1;
			const bool closing = name_begin < text_.size() && text_[name_begin] == '/';
			if (closing)
				++name_begin;
			std::size_t name_end = name_begin;
			while (name_end < text_.size() &&
				(std::isalnum(static_cast<unsigned char>(text_[name_end])) ||
					text_[name_end] == '_' || text_[name_end] == '-'))
			{
				++name_end;
			}
			if (name_end == name_begin)
			{
				position = open + 1;
				continue;
			}

			const std::string_view tag(text_.data() + name_begin, name_end - name_begin);
			const std::size_t tag_end = text_.find('>', name_end);
			if (tag_end == std::string::npos)
				break;

			Section *section = EqualsIgnoreCase(tag, "styles")
				? &styles_section_
				: EqualsIgnoreCase(tag, "layout") ? &layout_section_ : nullptr;
			if (section && !closing)
			{
				section->tag = {open, tag_end + 1};
				const std::string closing_tag = "</" + std::string(tag) + ">";
				const std::size_t close_begin = text_.find(closing_tag, tag_end + 1);
				if (close_begin == std::string::npos)
				{
					section->valid = false;
					position = tag_end + 1;
					continue;
				}
				section->content = {tag_end + 1, close_begin};
				section->valid = true;
				position = close_begin + closing_tag.size();
				continue;
			}
			position = tag_end + 1;
		}
	}

	void BuiDocumentModel::RefreshSections()
	{
		ScanSections();
		if (styles_section_.valid)
		{
			styles_ = text_.substr(styles_section_.content.begin,
				styles_section_.content.Size());
		}
		if (layout_section_.valid)
		{
			layout_ = text_.substr(layout_section_.content.begin,
				layout_section_.content.Size());
		}
	}

	void BuiDocumentModel::Rebuild()
	{
		nodes_.clear();
		warning_ = {};
		root_ = InvalidBuiNode;
		if (!layout_section_.valid)
			return;

		root_ = 1;
		nodes_.push_back({});
		nodes_[0].id = root_;
		nodes_[0].type = BuiNodeType::Element;
		nodes_[0].tag = "#document";
		nodes_[0].open_tag = layout_section_.content;
		nodes_[0].content = layout_section_.content;
		nodes_[0].close_tag = {layout_section_.content.end,
			layout_section_.content.end};

		struct OpenElement
		{
			BuiNodeId id = InvalidBuiNode;
			std::string tag;
		};
		std::vector<OpenElement> stack;
		stack.push_back({root_, "#document"});
		std::string current_text;
		std::size_t text_begin = 0;

		const auto append_node = [this](BuiNode node) -> BuiNodeId
		{
			if (!nodes_.empty() && node.id == InvalidBuiNode)
				node.id = static_cast<BuiNodeId>(nodes_.size() + 1);
			nodes_.push_back(std::move(node));
			return nodes_.back().id;
		};

		const auto flush_text = [&]()
		{
			if (current_text.empty())
				return;
			const bool meaningful = std::any_of(current_text.begin(), current_text.end(),
				[](char character) { return !IsSpace(character); });
			if (!meaningful)
			{
				// Keep the raw empty/whitespace span addressable while excluding it
				// from the editable child list.
				current_text.clear();
				return;
			}
			BuiNode node;
			node.type = BuiNodeType::Text;
			const std::size_t raw_length = current_text.size();
			node.text = BuiDocumentModel::DecodeEntities(current_text);
			node.open_tag = {text_begin, text_begin + raw_length};
			node.content = node.open_tag;
			node.parent = stack.back().id;
			const BuiNodeId id = append_node(std::move(node));
			const BuiNodeId parent_id = stack.back().id;
			nodes_[parent_id - 1].children.push_back(id);
			current_text.clear();
		};

		std::size_t position = layout_section_.content.begin;
		const std::size_t layout_end = layout_section_.content.end;
		while (position < layout_end)
		{
			if (text_.compare(position, 4, "<!--") == 0)
			{
				flush_text();
				const std::size_t close = text_.find("-->", position + 4);
				position = close == std::string::npos || close >= layout_end
					? layout_end
					: close + 3;
				continue;
			}
			if (text_[position] != '<')
			{
				const std::size_t next = text_.find('<', position);
				const std::size_t end = next == std::string::npos || next > layout_end
					? layout_end
					: next;
				if (current_text.empty())
					text_begin = position;
				current_text.append(text_, position, end - position);
				position = end;
				continue;
			}
			if (position + 1 >= layout_end)
				break;
			if (text_[position + 1] == '!' || text_[position + 1] == '?')
			{
				flush_text();
				const std::size_t close = text_.find('>', position + 2);
				position = close == std::string::npos || close >= layout_end
					? layout_end
					: close + 1;
				continue;
			}

			std::size_t cursor = position + 1;
			const bool closing = cursor < layout_end && text_[cursor] == '/';
			if (closing)
				++cursor;
			const std::size_t name_begin = cursor;
			while (cursor < layout_end &&
				(std::isalnum(static_cast<unsigned char>(text_[cursor])) ||
					text_[cursor] == '_' || text_[cursor] == '-' || text_[cursor] == ':'))
			{
				++cursor;
			}
			if (cursor == name_begin)
			{
				if (current_text.empty())
					text_begin = position;
				current_text.push_back(text_[position]);
				++position;
				continue;
			}

			const std::size_t name_end = cursor;
			std::size_t tag_end = text_.find('>', cursor);
			if (tag_end == std::string::npos || tag_end >= layout_end)
			{
				if (warning_.message.empty())
				{
					warning_ = {"Unterminated tag", LineAt(text_, position)};
				}
				if (current_text.empty())
					text_begin = position;
				current_text.append(text_, position, layout_end - position);
				position = layout_end;
				break;
			}

			const std::string_view tag_name(text_.data() + name_begin, name_end - name_begin);
			if (closing)
			{
				flush_text();
				bool matched = false;
				for (std::size_t index = stack.size(); index > 1; --index)
				{
					if (!EqualsIgnoreCase(stack[index - 1].tag, tag_name))
						continue;
					while (stack.size() > index)
					{
						if (warning_.message.empty())
						{
							warning_ = {"Unclosed <" + stack.back().tag + "> element",
								LineAt(text_, position)};
						}
						stack.pop_back();
					}
					const BuiNodeId matched_id = stack.back().id;
					BuiNode &node = nodes_[matched_id - 1];
					node.close_tag = {position, tag_end + 1};
					stack.pop_back();
					matched = true;
					break;
				}
				if (!matched && warning_.message.empty())
				{
					warning_ = {"Unexpected closing tag </" + std::string(tag_name) + ">",
						LineAt(text_, position)};
				}
				position = tag_end + 1;
				continue;
			}

			flush_text();
			BuiNode node;
			node.type = BuiNodeType::Element;
			node.tag = std::string(tag_name);
			node.open_tag = {position, tag_end + 1};
			node.tag_name = {name_begin, name_end};
			node.self_closing = tag_end > position && text_[tag_end - 1] == '/';
			node.content = {tag_end + 1, tag_end + 1};
			node.parent = stack.back().id;

			std::size_t attribute_position = name_end;
			while (attribute_position < tag_end)
			{
				BuiAttribute attribute;
				const std::size_t before_attribute = attribute_position;
				if (!ParseAttribute(text_, attribute_position, tag_end, attribute))
					break;
				if (attribute_position == before_attribute)
					break;
				node.attributes.push_back(std::move(attribute));
			}

			const BuiNodeId parent = stack.back().id;
			const BuiNodeId id = append_node(std::move(node));
			nodes_[parent - 1].children.push_back(id);

			if (!nodes_[id - 1].self_closing && !IsVoidElement(tag_name))
			{
				stack.push_back({id, std::string(tag_name)});
				if (IsRawTextElement(tag_name))
				{
					const std::string closing_tag = "</" + std::string(tag_name) + ">";
					const std::size_t close_begin = text_.find(closing_tag, tag_end + 1);
					if (close_begin != std::string::npos && close_begin < layout_end)
					{
						current_text.clear();
						text_begin = tag_end + 1;
						if (close_begin > tag_end + 1)
						{
				text_begin = tag_end + 1;
				current_text.assign(text_, text_begin, close_begin - text_begin);
							flush_text();
						}
						nodes_[id - 1].close_tag = {close_begin,
							close_begin + closing_tag.size()};
						nodes_[id - 1].content = {tag_end + 1, close_begin};
						stack.pop_back();
						position = close_begin + closing_tag.size();
						continue;
					}
				}
			}
			position = tag_end + 1;
		}

		flush_text();
		if (stack.size() > 1 && warning_.message.empty())
		{
			warning_ = {"Unclosed <" + stack.back().tag + "> element",
				LineAt(text_, text_.size())};
		}
		if (stack.size() == 1 && warning_.message.empty() && warning_.line == 0)
		{
			// A clean layout leaves the synthetic root as the only open frame.
			warning_ = {};
		}
		while (stack.size() > 1)
		{
			nodes_[stack.back().id - 1].content.end = layout_section_.content.end;
			nodes_[stack.back().id - 1].close_tag = {
				layout_section_.content.end, layout_section_.content.end};
			stack.pop_back();
		}
	}

	const std::string &BuiDocumentModel::GetText() const noexcept { return text_; }

	const std::string &BuiDocumentModel::GetStyles() const noexcept { return styles_; }

	const std::string &BuiDocumentModel::GetLayout() const noexcept { return layout_; }

	bool BuiDocumentModel::UsesCrlf() const noexcept { return crlf_; }

	const BuiParseError &BuiDocumentModel::GetWarning() const noexcept { return warning_; }

	const BuiNode *BuiDocumentModel::Find(BuiNodeId id) const noexcept
	{
		if (id == InvalidBuiNode || id > nodes_.size())
			return nullptr;
		return &nodes_[id - 1];
	}

	BuiNodeId BuiDocumentModel::GetRoot() const noexcept { return root_; }

	std::uint64_t BuiDocumentModel::GetRevision() const noexcept { return revision_; }

	std::string BuiDocumentModel::PathOf(BuiNodeId id) const
	{
		const BuiNode *node = Find(id);
		if (!node || id == root_)
			return {};

		std::vector<std::size_t> indices;
		while (node && node->id != root_)
		{
			const BuiNode *parent = Find(node->parent);
			if (!parent)
				return {};
			const auto it = std::find(parent->children.begin(), parent->children.end(), id);
			if (it == parent->children.end())
				return {};
			indices.push_back(static_cast<std::size_t>(
				std::distance(parent->children.begin(), it)));
			id = parent->id;
			node = parent;
		}

		std::string path;
		for (auto it = indices.rbegin(); it != indices.rend(); ++it)
		{
			if (!path.empty())
				path.push_back('/');
			path += std::to_string(*it);
		}
		return path;
	}

	BuiNodeId BuiDocumentModel::NodeAtPath(std::string_view path) const noexcept
	{
		if (!path.empty() && path.front() == '/')
			path.remove_prefix(1);
		BuiNodeId current = root_;
		if (path.empty())
			return current;

		std::size_t position = 0;
		while (position <= path.size())
		{
			const std::size_t slash = path.find('/', position);
			const std::string_view segment = path.substr(position,
				slash == std::string_view::npos ? path.size() - position : slash - position);
			if (segment.empty())
				return InvalidBuiNode;
			std::size_t index = 0;
			for (const char character : segment)
			{
				if (!std::isdigit(static_cast<unsigned char>(character)))
					return InvalidBuiNode;
				index = index * 10 + static_cast<std::size_t>(character - '0');
				if (index > 1000000)
					return InvalidBuiNode;
			}
			const BuiNode *node = Find(current);
			if (!node || index >= node->children.size())
				return InvalidBuiNode;
			current = node->children[index];
			if (slash == std::string_view::npos)
				break;
			position = slash + 1;
		}
		return current;
	}

	std::string BuiDocumentModel::ChildPath(BuiNodeId parent, std::size_t index) const
	{
		std::string path = PathOf(parent);
		if (!path.empty())
			path.push_back('/');
		path += std::to_string(index);
		return path;
	}

	bool BuiDocumentModel::IsAncestorOrSelf(BuiNodeId ancestor, BuiNodeId node) const noexcept
	{
		while (node != InvalidBuiNode)
		{
			if (node == ancestor)
				return true;
			const BuiNode *entry = Find(node);
			node = entry ? entry->parent : InvalidBuiNode;
		}
		return false;
	}

	bool BuiDocumentModel::SetAttribute(BuiNodeId id, std::string_view name,
		std::string_view value, BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		if (!node || node->type != BuiNodeType::Element || node->self_closing ||
			node->open_tag.Empty() || name.empty())
		{
			return false;
		}

		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		const BuiAttribute *attribute = node->FindAttribute(name);
		std::string replacement;
		std::size_t begin = 0;
		std::size_t end = 0;
		std::string effective_name(name);
		if (attribute)
		{
			effective_name = attribute->name;
			const std::string escaped = EscapeAttribute(value);
			if (attribute->quote == '\'' || attribute->quote == '\0')
				replacement = effective_name + "=\"" + escaped + "\"";
			else
			{
				const std::size_t prefix_size =
					attribute->value_range.begin - attribute->full_range.begin;
				// Keep the original name and equals/whitespace spelling, replacing
				// only the value bytes.
				replacement = text_.substr(attribute->full_range.begin, prefix_size);
				replacement += escaped;
				if (attribute->quote == '"')
					replacement += "\"";
			}
			begin = attribute->full_range.begin;
			end = attribute->full_range.end;
		}
		else
		{
			std::size_t insert = node->open_tag.end;
			if (insert > node->open_tag.begin && text_[insert - 1] == '>')
				--insert;
			while (insert > node->open_tag.begin && IsSpace(text_[insert - 1]))
				--insert;
			if (insert > node->open_tag.begin && text_[insert - 1] == '/')
				--insert;
			begin = end = insert;
			replacement = " " + effective_name + "=\"" + EscapeAttribute(value) + "\"";
		}

		text_.replace(begin, end - begin, replacement);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path = old_path;
		}
		return true;
	}

	bool BuiDocumentModel::SetStyles(std::string_view styles, BuiEditResult *result)
	{
		if (!styles_section_.valid)
			return false;

		const std::string old_text = text_;
		text_.replace(styles_section_.content.begin, styles_section_.content.Size(),
			std::string(styles));
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
		}
		return true;
	}

	bool BuiDocumentModel::RemoveAttribute(BuiNodeId id, std::string_view name,
		BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		if (!node || node->type != BuiNodeType::Element)
			return false;
		const BuiAttribute *attribute = node->FindAttribute(name);
		if (!attribute)
			return false;

		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		std::size_t begin = attribute->full_range.begin;
		std::size_t end = attribute->full_range.end;
		while (end < text_.size() && IsSpace(text_[end]) && text_[end] != '\r' &&
			text_[end] != '\n')
		{
			++end;
		}
		while (begin > node->open_tag.begin && IsSpace(text_[begin - 1]))
			--begin;
		text_.erase(begin, end - begin);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path = old_path;
		}
		return true;
	}

	bool BuiDocumentModel::SetText(BuiNodeId id, std::string_view text,
		BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		if (!node || node->type != BuiNodeType::Text)
			return false;
		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		const std::string escaped = EscapeText(text);
		text_.replace(node->open_tag.begin, node->open_tag.Size(), escaped);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path = old_path;
		}
		return true;
	}

	bool BuiDocumentModel::SetElementText(BuiNodeId id, std::string_view text,
		BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		if (!node || node->type != BuiNodeType::Element || node->self_closing)
			return false;
		std::size_t begin = node->content.begin;
		std::size_t end = node->content.end;
		if (!node->children.empty())
		{
			const BuiNode *first = Find(node->children.front());
			const BuiNode *last = Find(node->children.back());
			if (first && last)
			{
				begin = first->open_tag.begin;
				end = last->close_tag.Empty() ? last->open_tag.end : last->close_tag.end;
			}
		}
		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		text_.replace(begin, end - begin, EscapeText(text));
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path = old_path;
		}
		return true;
	}

	bool BuiDocumentModel::InsertNode(BuiNodeId parent, std::size_t index,
		const BuiNodeTemplate &node, BuiEditResult *result)
	{
		const BuiNode *parent_node = Find(parent);
		if (!parent_node || parent_node->type != BuiNodeType::Element ||
			parent_node->self_closing || index > parent_node->children.size() ||
			(node.type == BuiNodeType::Element && node.tag.empty()))
		{
			return false;
		}

		const std::string old_text = text_;
		const std::string parent_path = PathOf(parent);
		const std::string newline = crlf_ ? "\r\n" : "\n";
		std::string indent;
		if (!parent_node->children.empty())
		{
			const BuiNode *sibling = Find(parent_node->children.front());
			if (sibling)
				indent = LineIndent(text_, sibling->open_tag.begin);
		}
		if (indent.empty())
		{
			const std::string parent_indent = LineIndent(text_, parent_node->open_tag.begin);
			indent = parent_indent + "  ";
		}

		std::string block = indent;
		if (node.type == BuiNodeType::Text)
			block += EscapeText(node.text);
		else
		{
			block += "<" + node.tag;
			for (const auto &attribute : node.attributes)
			{
				block += " " + attribute.first + "=\"" +
					EscapeAttribute(attribute.second) + "\"";
			}
			if (EqualsIgnoreCase(node.tag, "input") || EqualsIgnoreCase(node.tag, "img") ||
				EqualsIgnoreCase(node.tag, "br") || EqualsIgnoreCase(node.tag, "hr"))
			{
				block += "/>";
			}
			else
			{
				block += "></" + node.tag + ">";
				if (!node.text.empty())
				{
					const std::string close = "</" + node.tag + ">";
					block.insert(block.size() - close.size(), EscapeText(node.text));
				}
			}
			block += newline;
		}

		std::size_t insert_position = parent_node->content.end;
		std::size_t new_index = index;
		if (!parent_node->children.empty())
		{
			if (index < parent_node->children.size())
			{
				const BuiNode *target = Find(parent_node->children[index]);
				insert_position = LineBegin(text_, target->open_tag.begin);
			}
			else
			{
				const BuiNode *target = Find(parent_node->children.back());
				insert_position = target->close_tag.Empty()
					? target->open_tag.end
					: target->close_tag.end;
				insert_position = LineEnd(text_, insert_position);
			}
		}
		else
		{
			// Keep the new child inside the parent even when the element was
			// authored as `<div></div>` on one line.
			insert_position = parent_node->open_tag.end;
			const std::string parent_indent = LineIndent(text_, parent_node->open_tag.begin);
			if (block.size() >= newline.size() &&
				block.compare(block.size() - newline.size(), newline.size(), newline) == 0)
			{
				block.resize(block.size() - newline.size());
			}
			block += newline + parent_indent;
			block = newline + block;
		}

		if (node.type == BuiNodeType::Text)
			block += newline;

		text_.insert(insert_position, block);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = parent_path;
			result->new_selection_path = ChildPath(parent, new_index);
		}
		return true;
	}

	bool BuiDocumentModel::RemoveNode(BuiNodeId id, BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		if (!node || id == root_)
			return false;
		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		std::size_t begin = LineBegin(text_, node->open_tag.begin);
		std::size_t end = node->close_tag.Empty() ? node->open_tag.end : node->close_tag.end;
		if (text_.compare(begin, 4, "<!--") == 0)
			begin = node->open_tag.begin;
		end = LineEnd(text_, end);
		if (begin > 0 && text_[begin - 1] != '\n')
			begin = node->open_tag.begin;
		text_.erase(begin, end - begin);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path.clear();
		}
		return true;
	}

	bool BuiDocumentModel::MoveNode(BuiNodeId id, BuiNodeId new_parent,
		std::size_t index, BuiEditResult *result)
	{
		const BuiNode *node = Find(id);
		const BuiNode *parent_node = Find(new_parent);
		if (!node || !parent_node || id == root_ || node->type != BuiNodeType::Element ||
			parent_node->type != BuiNodeType::Element || parent_node->self_closing ||
			IsAncestorOrSelf(id, new_parent))
		{
			return false;
		}

		const std::string old_text = text_;
		const std::string old_path = PathOf(id);
		const std::string newline = crlf_ ? "\r\n" : "\n";
		const std::size_t source_begin = node->open_tag.begin;
		const std::size_t source_end = node->close_tag.Empty()
			? node->open_tag.end
			: node->close_tag.end;
		std::string block = text_.substr(source_begin, source_end - source_begin);
		const std::string source_indent = LineIndent(text_, source_begin);

		std::size_t target_parent_children = parent_node->children.size();
		if (node->parent == new_parent)
			--target_parent_children;
		if (index > target_parent_children)
			index = target_parent_children;

		std::size_t insert_position = parent_node->content.end;
		if (!parent_node->children.empty())
		{
			std::size_t remaining = 0;
			for (const BuiNodeId child : parent_node->children)
			{
				if (child == id)
					continue;
				if (remaining == index)
				{
					const BuiNode *target = Find(child);
					insert_position = LineBegin(text_, target->open_tag.begin);
					break;
				}
				++remaining;
				const BuiNode *target = Find(child);
				insert_position = target->close_tag.Empty()
					? target->open_tag.end
					: target->close_tag.end;
			}
			if (index >= remaining)
				insert_position = LineEnd(text_, insert_position);
		}

		std::string dest_indent;
		if (!parent_node->children.empty())
		{
			for (const BuiNodeId child : parent_node->children)
			{
				if (child == id)
					continue;
				const BuiNode *sibling = Find(child);
				if (sibling)
					dest_indent = LineIndent(text_, sibling->open_tag.begin);
				break;
			}
		}
		if (dest_indent.empty())
			dest_indent = LineIndent(text_, parent_node->open_tag.begin) + "  ";

		// Remove the complete source lines first, then index the insertion point
		// against the shortened text.
		std::size_t erase_begin = LineBegin(text_, source_begin);
		std::size_t erase_end = LineEnd(text_, source_end);
		if (erase_begin > 0 && text_[erase_begin - 1] != '\n')
			erase_begin = source_begin;
		text_.erase(erase_begin, erase_end - erase_begin);

		if (erase_begin < insert_position)
			insert_position = insert_position > erase_end
				? insert_position - (erase_end - erase_begin)
				: erase_begin;

		// Reindent the moved block only when the destination indentation differs.
		if (source_indent != dest_indent)
		{
			std::string reindented;
			bool line_start = true;
			for (std::size_t index_in_block = 0; index_in_block < block.size();
				++index_in_block)
			{
				if (line_start && block.compare(index_in_block, source_indent.size(),
						source_indent) == 0)
				{
					reindented += dest_indent;
					index_in_block += source_indent.size() - 1;
				}
				else
				{
					reindented.push_back(block[index_in_block]);
				}
				line_start = block[index_in_block] == '\n';
			}
			block = std::move(reindented);
		}

		std::string insertion = dest_indent + block + newline;
		if (insert_position > 0 && text_[insert_position - 1] != '\n')
			insertion = newline + insertion;
		text_.insert(insert_position, insertion);
		RefreshSections();
		Rebuild();
		++revision_;
		if (result)
		{
			result->old_text = old_text;
			result->new_text = text_;
			result->old_selection_path = old_path;
			result->new_selection_path = ChildPath(new_parent, index);
		}
		return true;
	}

	std::string BuiDocumentModel::BuildPreviewLayout() const
	{
		std::string preview = layout_;
		std::vector<const BuiNode *> elements;
		elements.reserve(nodes_.size());
		for (const BuiNode &node : nodes_)
		{
			if (node.type == BuiNodeType::Element && node.id != root_ &&
				!node.open_tag.Empty())
			{
				elements.push_back(&node);
			}
		}
		std::sort(elements.begin(), elements.end(),
			[](const BuiNode *left, const BuiNode *right)
			{
				return left->open_tag.begin < right->open_tag.begin;
			});

		const std::size_t layout_begin = layout_section_.content.begin;
		for (auto it = elements.rbegin(); it != elements.rend(); ++it)
		{
			const BuiNode &node = **it;
			if (!node.FindAttribute("data-bl-node"))
			{
				std::size_t insert = node.open_tag.end;
				if (insert > node.open_tag.begin && text_[insert - 1] == '>')
					--insert;
				while (insert > node.open_tag.begin && IsSpace(text_[insert - 1]))
					--insert;
				if (insert > node.open_tag.begin && text_[insert - 1] == '/')
					--insert;
				const std::string attribute = " data-bl-node=\"" + PathOf(node.id) + "\"";
				preview.insert(insert - layout_begin, attribute);
			}
			else
			{
				const BuiAttribute *existing = node.FindAttribute("data-bl-node");
				const std::string attribute = " data-bl-node=\"" + PathOf(node.id) + "\"";
				preview.replace(existing->full_range.begin - layout_begin,
					existing->full_range.Size(), attribute);
			}
		}
		return preview;
	}

} // namespace BlackLight::Editor
