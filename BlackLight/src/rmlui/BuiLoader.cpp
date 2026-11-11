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

		/// Checks whitespace without passing signed characters to the C locale API.
		bool IsSpace(char value)
		{
			return std::isspace(static_cast<unsigned char>(value)) != 0;
		}

		/// Normalizes container tag names for case-insensitive comparisons.
		std::string ToLower(std::string_view value)
		{
			std::string result(value);
			for (char &character : result)
			{
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}
			return result;
		}

		/// Converts a byte offset into a one-based diagnostic line.
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

		/// Tracks forward progress while scanning the raw-content container.
		struct ParseState
		{
			std::string_view content;
			std::size_t position = 0;
			std::string error_message;
		};

		/// Advances past whitespace and reports whether input remains.
		bool SkipWhitespace(ParseState &state)
		{
			while (state.position < state.content.size() &&
				   IsSpace(state.content[state.position]))
			{
				++state.position;
			}
			return state.position < state.content.size();
		}

		/// Reads a container or layout tag without consuming its inner content.
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

		/// Extracts raw CSS or RML while ensuring every scan iteration advances.
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
						const std::size_t tag_start = state.content.find('<', state.position);
						if (tag_start == std::string_view::npos)
						{
							state.position = state.content.size();
							return false;
						}
						state.position = tag_start;
						if (!ParseTag(state, tag_name, is_closing))
						{
							if (state.position < state.content.size())
								++state.position;
							continue;
						}

						if (ToLower(tag_name) == ToLower(section_name) && is_closing)
						{
							// Found closing tag
							content = std::string(state.content.substr(content_start, tag_start - content_start));
							// Trim trailing whitespace
							while (!content.empty() && IsSpace(content.back()))
							{
								content.pop_back();
							}
							return true;
						}
					}
					return false;
				}
			}

			return false;
		}

		/// Requires both sections and preserves their raw CSS and RML text.
		bool ParseBuiXml(std::string_view bui_content, std::string &styles, std::string &layout,
						 BuiLoadError &error)
		{
			ParseState state{bui_content, 0, {}};

			std::string styles_content;
			std::string layout_content;

			// Parse <styles> section
			if (!ExtractSection(state, "styles", styles_content))
			{
				error.line = GetLineNumber(bui_content, state.position);
				error.message = "Missing or unclosed <styles> section";
				return false;
			}

			// Parse <layout> section
			state.position = 0; // Reset to find layout
			if (!ExtractSection(state, "layout", layout_content))
			{
				error.line = GetLineNumber(bui_content, state.position);
				error.message = "Missing or unclosed <layout> section";
				return false;
			}

			styles = std::move(styles_content);
			layout = std::move(layout_content);
			return true;
		}

	} // namespace

	/// Reads the complete file before parsing its raw-content container.
	bool BuiLoader::LoadFromFile(std::string_view file_path,
								 std::unique_ptr<BuiDocument> &document, BuiLoadError &error)
	{

		std::ifstream file(std::string(file_path), std::ios::binary);
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

	/// Produces a document only after both required sections have loaded.
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
