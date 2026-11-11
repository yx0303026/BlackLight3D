#include "rmlui/BuiLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace BlackLight::UI
{

	namespace
	{

		bool IsSpace(char value)
		{
			return std::isspace(static_cast<unsigned char>(value)) != 0;
		}

		std::string_view Trim(std::string_view value)
		{
			while (!value.empty() && IsSpace(value.front()))
			{
				value.remove_prefix(1);
			}
			while (!value.empty() && IsSpace(value.back()))
			{
				value.remove_suffix(1);
			}
			return value;
		}

		std::string ToLower(std::string_view value)
		{
			std::string result(value);
			for (char &character : result)
			{
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}
			return result;
		}

		std::size_t GetLineNumber(std::string_view content, std::size_t position)
		{
			std::size_t line = 1;
			for (std::size_t index = 0; index < position && index < content.size(); ++index)
			{
				if (content[index] == '\n')
				{
					++line;
				}
			}
			return line;
		}

		struct ParseState
		{
			std::string_view content;
			std::size_t position = 0;
			std::string error_message;
		};

		bool SkipWhitespace(ParseState &state)
		{
			while (state.position < state.content.size() &&
				   IsSpace(state.content[state.position]))
			{
				++state.position;
			}
			return state.position < state.content.size();
		}

		bool ExpectChar(ParseState &state, char ch)
		{
			if (!SkipWhitespace(state))
			{
				return false;
			}
			if (state.content[state.position] != ch)
			{
				return false;
			}
			++state.position;
			return true;
		}

		std::string_view ExtractTagName(std::string_view tag_content)
		{
			std::size_t end = 0;
			while (end < tag_content.size() &&
				   (std::isalnum(static_cast<unsigned char>(tag_content[end])) ||
					tag_content[end] == '_' || tag_content[end] == '-'))
			{
				++end;
			}
			return tag_content.substr(0, end);
		}

		std::string_view ExtractText(ParseState &state)
		{
			std::size_t start = state.position;
			std::size_t end = start;

			while (end < state.content.size() && state.content[end] != '<')
			{
				++end;
			}

			state.position = end;
			return state.content.substr(start, end - start);
		}

		bool ParseTag(ParseState &state, std::string &tag_name, bool &is_closing)
		{
			if (!SkipWhitespace(state) || state.content[state.position] != '<')
			{
				return false;
			}

			++state.position;

			// Check for closing tag
			is_closing = false;
			if (state.position < state.content.size() && state.content[state.position] == '/')
			{
				is_closing = true;
				++state.position;
			}

			if (!SkipWhitespace(state))
			{
				state.error_message = "Unexpected end of file in tag";
				return false;
			}

			std::size_t tag_start = state.position;
			while (state.position < state.content.size() &&
				   (std::isalnum(static_cast<unsigned char>(state.content[state.position])) ||
					state.content[state.position] == '_' ||
					state.content[state.position] == '-'))
			{
				++state.position;
			}

			if (state.position == tag_start)
			{
				state.error_message = "Invalid tag name";
				return false;
			}

			tag_name = std::string(state.content.substr(tag_start, state.position - tag_start));

			// Skip attributes and find closing >
			while (state.position < state.content.size() && state.content[state.position] != '>')
			{
				++state.position;
			}

			if (state.position >= state.content.size())
			{
				state.error_message = "Unterminated tag";
				return false;
			}

			++state.position;
			return true;
		}

		bool ExtractSection(ParseState &state, const std::string &section_name, std::string &content)
		{
			// Find opening tag
			std::string tag_name;
			bool is_closing = false;

			while (state.position < state.content.size())
			{
				if (!ParseTag(state, tag_name, is_closing))
				{
					// If we can't parse a tag, try to find the next one
					if (state.position < state.content.size())
					{
						++state.position;
						continue;
					}
					return false;
				}

				if (ToLower(tag_name) == ToLower(section_name) && !is_closing)
				{
					// Found opening tag, extract content until closing tag
					std::size_t content_start = state.position;

					while (state.position < state.content.size())
					{
						if (!ParseTag(state, tag_name, is_closing))
						{
							// Treat remaining as content if no more tags
							if (state.position >= state.content.size())
							{
								content = std::string(state.content.substr(content_start));
								return true;
							}
							continue;
						}

						if (ToLower(tag_name) == ToLower(section_name) && is_closing)
						{
							// Found closing tag
							content = std::string(
								state.content.substr(content_start,
													 state.position - content_start - (tag_name.size() + 3)) // < / tag >
							);
							// Trim trailing whitespace
							while (!content.empty() && IsSpace(content.back()))
							{
								content.pop_back();
							}
							return true;
						}
					}
					return true;
				}
			}

			return false;
		}

		bool ParseBuiXml(std::string_view bui_content, std::string &styles, std::string &layout,
						 BuiLoadError &error)
		{
			ParseState state{bui_content};

			std::string styles_content;
			std::string layout_content;

			// Parse <styles> section
			if (!ExtractSection(state, "styles", styles_content))
			{
				error.line = GetLineNumber(bui_content, state.position);
				error.message = "Missing <styles> section";
				return false;
			}

			// Parse <layout> section
			state.position = 0; // Reset to find layout
			if (!ExtractSection(state, "layout", layout_content))
			{
				error.line = GetLineNumber(bui_content, state.position);
				error.message = "Missing <layout> section";
				return false;
			}

			styles = std::move(styles_content);
			layout = std::move(layout_content);
			return true;
		}

	} // namespace

	bool BuiLoader::LoadFromFile(std::string_view file_path,
								 std::unique_ptr<BuiDocument> &document, BuiLoadError &error)
	{

		std::ifstream file(file_path.data(), std::ios::binary);
		if (!file)
		{
			error.message = "Failed to open file: ";
			error.message += file_path;
			error.line = 0;
			return false;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		if (!file)
		{
			error.message = "Failed to read file";
			error.line = 0;
			return false;
		}

		std::string content = buffer.str();
		return Parse(content, document, error);
	}

	bool BuiLoader::Parse(std::string_view bui_content,
						  std::unique_ptr<BuiDocument> &document, BuiLoadError &error)
	{

		if (bui_content.empty())
		{
			error.message = ".bui content is empty";
			error.line = 1;
			return false;
		}

		std::string styles;
		std::string layout;

		if (!ParseBuiXml(bui_content, styles, layout, error))
		{
			return false;
		}

		if (styles.empty())
		{
			error.message = "<styles> section is empty";
			error.line = GetLineNumber(bui_content, 0);
			return false;
		}

		if (layout.empty())
		{
			error.message = "<layout> section is empty";
			error.line = GetLineNumber(bui_content, 0);
			return false;
		}

		auto new_document = std::make_unique<BuiDocument>();
		new_document->SetStyles(std::move(styles));
		new_document->SetLayout(std::move(layout));

		document = std::move(new_document);
		return true;
	}

} // namespace BlackLight::UI
