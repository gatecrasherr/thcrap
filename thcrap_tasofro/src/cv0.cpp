/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th105 pl patcher
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "mediawiki.h"
#include "cv0.h"

TasofroCv0::LineType TasofroCv0::guessLineType(const char* file, size_t size)
{
	for (size_t i = 0; i < size && file[i] != '\n'; i++) {
		if (file[i] == '#') {
			return EMPTY;
		}
		else if (file[i] == ':') {
			return COMMAND;
		}
	}
	for (size_t i = 0; i < size && file[i] != '\n'; i++) {
		if (file[i] != '\r' && file[i] != '\t' && file[i] != ' ') {
			return TEXT;
		}
	}
	return EMPTY;
}

TasofroCv0::ALine* TasofroCv0::readLine(const char*& file, size_t& size)
{
	switch (guessLineType(file, size)) {
	case EMPTY:
		return Empty::read(file, size);
	case COMMAND:
		return Command::read(file, size);
	case TEXT:
		return Text::read(file, size);
	default:
		return nullptr; // Can't happen
	}
}



std::string TasofroCv0::ALine::readLine(const char*& file, size_t& size)
{
	std::string out;
	size_t i = 0;
	while (i < size && file[i] != '\n') {
		i++;
	}
	if (i > 0 && file[i - 1] == '\r') {
		out.assign(file, i - 1);
	}
	else {
		out.assign(file, i);
	}
	if (i < size) {
		i++;
	}

	file += i;
	size -= i;
	return out;
}



TasofroCv0::LineType TasofroCv0::Empty::getType() const
{
	return EMPTY;
}
TasofroCv0::LineType TasofroCv0::Command::getType() const
{
	return COMMAND;
}
TasofroCv0::LineType TasofroCv0::Text::getType() const
{
	return TEXT;
}


TasofroCv0::ALine::ALine()
{}

std::string TasofroCv0::ALine::unescape(const std::string& in) const
{
	std::string out;

	for (size_t pos = 0; pos < in.size(); pos++) {
		if (in[pos] == '\\' && in[pos + 1] == ',') {
			pos++;
		}

		out += in[pos];
	}
	return out;
}

std::string TasofroCv0::ALine::escape(const std::string& in) const
{
	std::string out;

	for (size_t i = 0; i < in.size(); i++) {
		if (in[i] == ',') {
			out += "\\,"sv;
		}
		else {
			out += in[i];
		}
	}

	return parse_mediawiki(out, mwdef_nsml);
}



TasofroCv0::Empty::Empty()
{}

TasofroCv0::Empty* TasofroCv0::Empty::read(const char*& file, size_t& size)
{
	Empty* line = new Empty();
	line->content = ALine::readLine(file, size);
	return line;
}

std::string TasofroCv0::Empty::toString() const
{
	return content;
}


TasofroCv0::Command::Command()
{}

TasofroCv0::Command* TasofroCv0::Command::read(const char*& file, size_t& size)
{
	Command* line = new Command();
	line->content = ALine::readLine(file, size);
	return line;
}


std::string TasofroCv0::Command::toString() const
{
	return content;
}




TasofroCv0::Text::Text()
{}

TasofroCv0::Text::Text(const std::string& content)
	: content(content)
{}

TasofroCv0::Text* TasofroCv0::Text::read(const char*& file, size_t& size)
{
	Text* line = new Text();
	line->content = ALine::readLine(file, size);
	while (guessLineType(file, size) == TEXT) {
		line->content += '\n' + ALine::readLine(file, size);
		size_t i = line->content.length() - 1;
		while (i > 0 && (line->content[i] == '\r' || line->content[i] == '\n')) {
			i--;
		}
		if (line->content[i] == '\\') {
			break;
		}
	}
	return line;
}


std::string TasofroCv0::Text::toString() const
{
	return content;
}

void TasofroCv0::Text::patch(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it, uint32_t textbox_size, json_t *patch)
{
	this->cur_line = 1;
	this->nb_lines = 0;

	this->content.clear();
	json_t *json_line;
	json_array_foreach_scoped(size_t, json_line_num, patch, json_line) {
		if (this->parseCommand(patch, json_line_num, textbox_size) == true) {
			continue;
		}

		this->beginLine(file, file_it);
		this->patchLine(arabic_convert_bidi(json_string_value(json_line)).c_str());
		this->endLine();
	}

	this->content = this->escape(this->content);
}



bool TasofroCv0::Text::parseCommand(json_t *patch, size_t json_line_num, uint32_t textbox_size)
{
	const char* line = json_array_get_string(patch, json_line_num);
	if (strcmp(line, "<balloon>") == 0) {
		this->cur_line = 1;
		this->nb_lines = 0;
		return true;
	}

	if (this->nb_lines == 0) {
		size_t max_i = json_array_size(patch);
		size_t i = json_line_num;
		while (++i < max_i) {
			line = json_array_get_string(patch, i);
			if (strcmp(line, "<balloon>") == 0) {
				break;
			}
			else if (line[0] != '\0' && (line[strlen(line) - 1] == '@' || line[strlen(line) - 1] == '\\')) {
				i++;
				break;
			}
		}
		this->nb_lines = i - json_line_num;
		if (this->nb_lines > textbox_size) {
			this->nb_lines = textbox_size;
		}
	}

	return false;
}

void TasofroCv0::Text::beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	if (this->cur_line == 1) {
		this->content = this->escape(this->content);
		file.insert(it, new Text(this->content));
		this->content.clear();
	}
}

void TasofroCv0::Text::patchLine(const char *text)
{
	std::string formattedText = text;
	if (this->cur_line != this->nb_lines) {
		formattedText += '\n';
	}
	else if (formattedText.length() == 0 || (formattedText[formattedText.length() - 1] != '@' && formattedText[formattedText.length() - 1] != '\\')) {
		formattedText += "\\\n"sv;
	}

	this->content += formattedText;
}

void TasofroCv0::Text::endLine()
{
	if (this->cur_line != this->nb_lines) {
		this->cur_line++;
	}
	else {
		this->cur_line = 1;
		this->nb_lines = 0;
	}
}



json_t *TasofroCv0::balloonNumberToLines(json_t *patch, size_t balloon_number)
{
	json_t *json_line_data;
	json_t *json_lines;

	json_line_data = json_object_numkey_get(patch, balloon_number);
	if (!json_is_object(json_line_data)) {
		return nullptr;
	}

	json_lines = json_object_get(json_line_data, "lines");
	if (!json_is_array(json_lines)) {
		return nullptr;
	}

	return json_lines;
}

int patch_cv0(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch)
{
	if (!patch) {
		return 0;
	}

	std::list<TasofroCv0::ALine*> lines;
	const char *file_in = (const char*)file_inout;
	char *file_out = (char*)file_inout;

	while (size_in > 0) {
		lines.push_back(TasofroCv0::readLine(file_in, size_in));
	}

	size_t balloon_number = 1;
	int textbox_size = 3;
	for (auto it = lines.begin(); it != lines.end(); ++it) {
		TasofroCv0::ALine *line = *it;
		if (line->getType() == TasofroCv0::COMMAND && textbox_size != 4 && line->toString().compare(0, 3, "CG:") == 0) {
			textbox_size = 4;
			continue;
		}
		else if (line->getType() != TasofroCv0::TEXT) {
			continue;
		}

		json_t *json_lines = TasofroCv0::balloonNumberToLines(patch, balloon_number);
		balloon_number++;
		if (json_lines == nullptr) {
			continue;
		}
		dynamic_cast<TasofroCv0::Text*>(line)->patch(lines, it, textbox_size, json_lines);
	}

	std::string str;
	for (TasofroCv0::ALine* line : lines) {
		str = line->toString();
		if (str.size() > size_out + 3) {
			log_print("Output file too small\n");
			break;
		}
		memcpy(file_out, str.c_str(), str.size());
		file_out += str.size();
		size_out -= str.size();
		*file_out = '\n';
		file_out++;
		size_out--;
	}
	*file_out = '\0';

	return 1;
}

/**
  * Add a way to escape commas in the th105 cv0 parser.
  *
  * Returns:
  * • 0 if the the current character is a backslash followed by a quote.
  *   We overwrite the backslash and skip the code checking for the comma.
  * • 1 otherwise, just let the game do its thing.
  *
  * Own JSON parameters
  * -------------------
  * Mandatory:
  *
  *	[string]
  *		Current position of the parser in the cv0 file.
  *		Type: immediate
  *
  *	[delim]
  *		Character the game is looking for.
  *     If the game doesn't try to kill our comma, no need to hide it.
  *		Type: immediate
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
extern "C" size_t BP_th105_cv0_escape_comma(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char delim = (char)json_object_get_immediate(bp_info, regs, "delim");
	// ----------

	if (delim != ',') {
		return 1;
	}

	char *string = (char*)json_object_get_immediate(bp_info, regs, "string");
	if (string[0] == '\\' && string[1] == ',') {
		// Erase the backslash
		size_t i = 0;
		do {
			string[i] = string[i + 1];
			++i;
		} while (string[i] && string[i] != '\n');
		// Ensure the test will fail
		regs->eax = 0;
		regs->ecx = 0;
		return 0;
	}

	return 1;
}
