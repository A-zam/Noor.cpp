// daily.cpp - today's groups, students to call, attendance warnings.
#include "daily.h"
#include "alerts.h"
#include "attendance.h"
#include "students.h"

std::string weekdayName(const std::string& date) {
    if (!isValidDate(date)) return "";
    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));

    // Sakamoto's method: gives 0 for Sunday, 1 for Monday ... 6 for Saturday.
    int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year = year - 1;
    int w = (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;

    std::string names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return names[w];
}

// Selection sort: find the smallest of the rest, swap it to the front, repeat.
void sortByBalance(std::vector<Student>& list) {
    for (size_t i = 0; i + 1 < list.size(); i++) {
        size_t smallest = i;
        for (size_t j = i + 1; j < list.size(); j++) {
            if (list[j].balance < list[smallest].balance) smallest = j;
        }
        if (smallest != i) {
            Student temp = list[i];
            list[i] = list[smallest];
            list[smallest] = temp;
        }
    }
}

DailyList buildDailyList(const Database& db, const std::string& today) {
    DailyList result;

    // 1. Groups that meet today.
    std::string weekday = weekdayName(today);
    for (size_t g = 0; g < db.groups.size(); g++) {
        for (size_t d = 0; d < db.groups[g].days.size(); d++) {
            if (db.groups[g].days[d] == weekday) result.todayGroupIds.push_back(db.groups[g].id);
        }
    }

    // 2. Students to call about money: in debt, or fewer than 2 lessons left.
    for (size_t i = 0; i < db.students.size(); i++) {
        if (db.students[i].balance < 0 || isLowBalance(db, db.students[i])) {
            result.callList.push_back(db.students[i]);
        }
    }
    sortByBalance(result.callList);

    // 3. Attendance warnings.
    for (size_t g = 0; g < db.groups.size(); g++) {
        const Group& group = db.groups[g];
        for (size_t i = 0; i < group.studentIds.size(); i++) {
            int studentId = group.studentIds[i];
            if (hasAbsenceStreak(db, group.id, studentId)) {
                Warning w;
                w.studentId = studentId;
                w.groupId = group.id;
                w.text = "absent from the last 2 lessons";
                result.warnings.push_back(w);
            }
            if (hasLowAttendance(db, group.id, studentId)) {
                Warning w;
                w.studentId = studentId;
                w.groupId = group.id;
                w.text = "attendance is only " +
                         std::to_string(studentAttendancePercent(db, group.id, studentId)) + "%";
                result.warnings.push_back(w);
            }
        }
    }
    return result;
}
