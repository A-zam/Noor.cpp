// attendance.h - marking attendance and charging lessons.
// No cin/cout here: functions take values and return results.
#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include <string>
#include <vector>
#include "structs.h"

// Positions (in db.groups) of the groups this account may mark attendance for:
// a teacher gets only their own groups, a manager or director gets all.
std::vector<int> visibleGroupIndexes(const Database& db, const Account& who);

// True if attendance for this group is already saved for this date.
bool hasLesson(const Database& db, int groupId, const std::string& date);

// All saved lessons of a group, in the order they were saved (oldest first).
std::vector<Lesson> lessonsOfGroup(const Database& db, int groupId);

// ----- attendance rules (the thresholds live in attendance.cpp) -----

// True if the student was absent from the group's last 2 lessons.
bool hasAbsenceStreak(const Database& db, int groupId, int studentId);

// The student's attendance in this group as a whole percent (present * 100 / lessons
// the student was listed in), or -1 if the student appears in no lesson yet.
int studentAttendancePercent(const Database& db, int groupId, int studentId);

// True if the group has 4 or more lessons AND the student's attendance is under 70%.
bool hasLowAttendance(const Database& db, int groupId, int studentId);

// Save one lesson. presentIds are the students who came; every other student
// of the group is absent. EVERY student of the group is charged the group's
// price (present or absent). Returns true on success, otherwise false + error.
bool markAttendance(Database& db, const Account& by, int groupId, const std::string& date,
                    const std::vector<int>& presentIds, std::string& error);

#endif
