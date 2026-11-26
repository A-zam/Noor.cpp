// daily.h - the daily action list.
// No cin/cout here: buildDailyList returns the lists, main prints them.
#ifndef DAILY_H
#define DAILY_H

#include <string>
#include <vector>
#include "structs.h"

// One attendance warning about a student in a group.
struct Warning {
    int studentId;
    int groupId;
    std::string text;
};

struct DailyList {
    std::vector<int> todayGroupIds;     // groups that have a lesson today
    std::vector<Student> callList;      // students to call about money, lowest balance first
    std::vector<Warning> warnings;      // students with attendance problems
};

// "Mon", "Tue" ... for a YYYY-MM-DD date, or "" if the date is not valid.
std::string weekdayName(const std::string& date);

// Sort students by balance, smallest first. Hand-written selection sort.
void sortByBalance(std::vector<Student>& list);

// Build the three lists for this day.
DailyList buildDailyList(const Database& db, const std::string& today);

#endif
