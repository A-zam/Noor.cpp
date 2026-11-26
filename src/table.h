// table.h - turns rows of text into an aligned text table.
// It only builds a string. main.cpp prints it.
#ifndef TABLE_H
#define TABLE_H

#include <string>
#include <vector>

// Build a table like this:
//   +----+--------+---------+
//   | ID | Name   | Balance |
//   +----+--------+---------+
//   | 1  | Ali    |  50 000 |
//   +----+--------+---------+
// headers = column titles. rows = one vector of cell texts per row.
// align = one letter per column: 'l' = left (default), 'r' = right.
// Example: "lr" means column 1 is left-aligned and column 2 right-aligned.
std::string formatTable(const std::vector<std::string>& headers,
                        const std::vector<std::vector<std::string>>& rows,
                        const std::string& align = "");

#endif
