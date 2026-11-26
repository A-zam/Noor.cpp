// table.cpp - builds the aligned text tables used by every list on screen.
#include "table.h"

// How many characters the text shows on screen. A UTF-8 letter such as a Cyrillic
// one uses 2 bytes but takes ONE place, so we skip the "continuation" bytes
// (they all start with the bits 10).
static size_t displayLength(const std::string& text) {
    size_t length = 0;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = (unsigned char)text[i];
        if ((c & 0xC0) != 0x80) length++;
    }
    return length;
}

// Text plus spaces until it is "width" characters wide.
static std::string padRight(const std::string& text, size_t width) {
    return text + std::string(width - displayLength(text), ' ');
}

static std::string padLeft(const std::string& text, size_t width) {
    return std::string(width - displayLength(text), ' ') + text;
}

// A separator line such as +----+--------+
static std::string separator(const std::vector<size_t>& widths) {
    std::string line = "+";
    for (size_t i = 0; i < widths.size(); i++) {
        line += std::string(widths[i] + 2, '-') + "+";
    }
    return line + "\n";
}

// One table line such as | 1  | Ali    |
static std::string rowLine(const std::vector<std::string>& cells, const std::vector<size_t>& widths,
                           const std::string& align) {
    std::string line = "|";
    for (size_t i = 0; i < widths.size(); i++) {
        std::string cell = (i < cells.size()) ? cells[i] : "";
        bool right = (i < align.size() && align[i] == 'r');
        line += " " + (right ? padLeft(cell, widths[i]) : padRight(cell, widths[i])) + " |";
    }
    return line + "\n";
}

std::string formatTable(const std::vector<std::string>& headers,
                        const std::vector<std::vector<std::string>>& rows,
                        const std::string& align) {
    // 1. Each column is as wide as its longest text (title included).
    std::vector<size_t> widths;
    for (size_t j = 0; j < headers.size(); j++) widths.push_back(displayLength(headers[j]));
    for (size_t i = 0; i < rows.size(); i++) {
        for (size_t j = 0; j < headers.size() && j < rows[i].size(); j++) {
            if (displayLength(rows[i][j]) > widths[j]) widths[j] = displayLength(rows[i][j]);
        }
    }

    // 2. Put the lines together: top, titles, line, rows, bottom.
    std::string table = separator(widths);
    table += rowLine(headers, widths, "");
    table += separator(widths);
    for (size_t i = 0; i < rows.size(); i++) table += rowLine(rows[i], widths, align);
    table += separator(widths);
    return table;
}
