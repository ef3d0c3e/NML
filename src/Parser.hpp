#ifndef NML_PARSER_HPP
#define NML_PARSER_HPP

#include <string>
#include <string_view>
#include <functional>
#include <regex>

#include "Syntax.hpp"

class Document;
class Compiler;

/**
 * @brief Represents a file to be parsed
 */
struct File
{
friend class Parser;

	std::string name; ///< File's name (for displaying)
	std::string_view content; ///< File's content

	std::size_t line; ///< Line offset (for displaying)
	std::size_t pos; ///< Character offset on first line (for displaying)

	std::deque<const File*> stack; ///< Call stack

	/**
	 * @brief Constructor
	 *
	 * @param _name File's name (for displaying)
	 * @param _content File's content
	 * @param _line Line offset (for displaying)
	 * @param _pos Character offset on first line (from offset, for displaying)
	 * @param prev Previous file
	 */
	[[nodiscard]] File(const std::string& _name, const std::string_view& _content, std::size_t _line, std::size_t _pos, const File& prev):
		name(_name), content(_content), line(_line), pos(_pos)
	{
		stack = prev.stack;
		stack.push_back(&prev);
	}

	/**
	 * @brief Constructor
	 *
	 * @param _name File's name (for displaying)
	 * @param _content File's content
	 * @param _line Line offset (for displaying)
	 * @param _off Character offset on first line (from offset, for displaying)
	 */
	[[nodiscard]] File(const std::string& _name, const std::string_view& _content, std::size_t _line, std::size_t _pos) noexcept:
		name(_name), content(_content), line(_line), pos(_pos) {}

	/**
	 * @brief Gets line starting from `start`
	 *
	 * @param start Start position of line
	 * @returns The line starting at `start`
	 */
	[[nodiscard]] std::string_view get_line(std::size_t start) const noexcept;

	/**
	 * @brief Gets a token starting from start and ending with a separator
	 *
	 * @param separator Separator that should end the token
	 * @param escape Escape character
	 * @param start Start position
	 * @param end End position
	 * @returns Token found
	 */
	[[nodiscard]] std::pair<std::string, std::size_t> get_token(const std::string_view& separator, const std::string_view& escape,
			std::size_t start, std::size_t end = std::string::npos) const noexcept;
};

/**
 * @brief Additional data for parser
 */
struct ParserData
{
	/**
	 * @brief Represents a match
	 */
	struct Match
	{
		std::string original; ///< Original string used to compile regex
		std::regex regex; ///< Compiled regex
		std::function<std::size_t(Document&, const File& f, ParserData&, std::size_t, std::size_t)> callback; ///< Called when match

		/**
		 * @brief Constructor
		 *
		 * @param match String match to compile
		 * @param callback Callback on match
		 */
		[[nodiscard]] Match(const std::string& match, decltype(callback)&& callback);
	};

	/**
	 * @brief Initializes default matches
	 */
	[[nodiscard]] ParserData(Parser& parser);

	Parser& parser;

	std::vector<Match> matches; ///< List of possible matches
	std::vector<std::size_t> match_point; ///< Last point at which a regex matched
	std::vector<std::size_t> match_length; ///< Length of last regex match
	std::vector<std::string> match_str; ///< Matched strings
	std::size_t current_match_length; ///< Length of current match
	std::string current_match_str; ///< Current matched string

	Syntax::Style style = Syntax::Style::NONE; ///< Current style (used to keep track of what styles are currently enabled)
	std::vector<bool> customStyle; ///< Keeps track of custom styles
	std::vector<std::size_t> customPres; ///< Keeps track of custom presentations
	std::size_t last_match_index = 0; ///< Index of previous match

	struct ListEntry
	{
		bool ordered; ///< Whether list is ordered
		std::size_t counter; ///< List current counter (Modified with "#+CounterN" or simply incrementing
	};
	std::deque<ListEntry> list; ///< Current open lists

	/**
	 * @brief Adds match after an other one
	 *
	 * @param regex Regex of other match
	 * @param match Match to add
	 */
	void emplace_after(const std::string_view& regex, ParserData::Match&& m);
};

/**
 * @brief Parser
 */
class Parser
{
	const Compiler* const compiler;
public:
	/**
	 * @brief Constructor
	 * Initializes matches
	 * 
	 * @param _compiler Compiler to use
	 */
	[[nodiscard]] Parser(const Compiler* const _compiler);

	/**
	 * @brief Parse file
	 *
	 * @param f File to parse
	 * @param inheritDoc Document to inherit from
	 * @param inheritData ParserData to inherit from
	 * @returns Document containing parsed file
	 */
	[[nodiscard]] std::pair<Document, ParserData> parse(const File& f, const Document* inheritDoc = nullptr, ParserData* inheritData = nullptr);
};

#endif // NML_PARSER_HPP
