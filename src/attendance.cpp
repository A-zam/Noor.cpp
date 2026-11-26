// attendance.cpp - marking attendance and charging lessons.
#include "attendance.h"
#include "students.h"
#include "storage.h"

static bool contains(const std::vector<int>& list, int value) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == value) return true;
    }
    return false;
}

std::vector<int> visibleGroupIndexes(const Database& db, const Account& who) {
    std::vector<int> result;
    for (size_t i = 0; i < db.groups.size(); i++) {
        if (who.role != "teacher" || db.groups[i].teacherId == who.id) {
            result.push_back((int)i);
        }
    }
    return result;
}

bool hasLesson(const Database& db, int groupId, const std::string& date) {
    for (size_t i = 0; i < db.lessons.size(); i++) {
        if (db.lessons[i].groupId == groupId && db.lessons[i].date == date) return true;
    }
    return false;
}

std::vector<Lesson> lessonsOfGroup(const Database& db, int groupId) {
    std::vector<Lesson> result;
    for (size_t i = 0; i < db.lessons.size(); i++) {
        if (db.lessons[i].groupId == groupId) result.push_back(db.lessons[i]);
    }
    return result;
}

// The numbers from the brief, written once so they are easy to find and change.
static const int STREAK_LENGTH = 2;            // absent from the last 2 lessons
static const int MIN_LESSONS_FOR_PERCENT = 4;  // percent rule starts at 4 lessons
static const int LOW_ATTENDANCE_PERCENT = 70;  // under 70% is low

// Count in how many lessons the student was present and absent.
static void countMarks(const std::vector<Lesson>& lessons, int studentId, int& present, int& absent) {
    present = 0;
    absent = 0;
    for (size_t i = 0; i < lessons.size(); i++) {
        if (contains(lessons[i].presentIds, studentId)) present++;
        if (contains(lessons[i].absentIds, studentId)) absent++;
    }
}

bool hasAbsenceStreak(const Database& db, int groupId, int studentId) {
    std::vector<Lesson> lessons = lessonsOfGroup(db, groupId);
    if ((int)lessons.size() < STREAK_LENGTH) return false;
    for (size_t i = lessons.size() - STREAK_LENGTH; i < lessons.size(); i++) {
        if (!contains(lessons[i].absentIds, studentId)) return false;
    }
    return true;
}

int studentAttendancePercent(const Database& db, int groupId, int studentId) {
    int present, absent;
    countMarks(lessonsOfGroup(db, groupId), studentId, present, absent);
    if (present + absent == 0) return -1;
    return present * 100 / (present + absent);
}

bool hasLowAttendance(const Database& db, int groupId, int studentId) {
    std::vector<Lesson> lessons = lessonsOfGroup(db, groupId);
    if ((int)lessons.size() < MIN_LESSONS_FOR_PERCENT) return false;
    int present, absent;
    countMarks(lessons, studentId, present, absent);
    if (present + absent == 0) return false;
    // present/total < 70/100, written with whole numbers so there is no rounding
    return present * 100 < LOW_ATTENDANCE_PERCENT * (present + absent);
}

bool markAttendance(Database& db, const Account& by, int groupId, const std::string& date,
                    const std::vector<int>& presentIds, std::string& error) {
    int gi = findGroupIndex(db, groupId);
    if (gi == -1) { error = "There is no group with that id."; return false; }
    const Group& group = db.groups[gi];

    if (by.role == "teacher" && group.teacherId != by.id) {
        error = "You can only mark attendance for your own groups.";
        return false;
    }
    if (!isValidDate(date)) { error = "The date must look like YYYY-MM-DD."; return false; }
    if (group.studentIds.empty()) { error = "This group has no students."; return false; }
    if (hasLesson(db, groupId, date)) {
        error = "Attendance for this group is already saved for " + date + ".";
        return false;
    }
    for (size_t i = 0; i < presentIds.size(); i++) {
        if (!contains(group.studentIds, presentIds[i])) {
            error = "A student marked present is not in this group.";
            return false;
        }
    }

    // Split the group into present and absent.
    Lesson lesson;
    lesson.id = nextId(db.lessons);
    lesson.groupId = groupId;
    lesson.date = date;
    for (size_t i = 0; i < group.studentIds.size(); i++) {
        int studentId = group.studentIds[i];
        if (contains(presentIds, studentId)) lesson.presentIds.push_back(studentId);
        else lesson.absentIds.push_back(studentId);
    }

    // Charge every student of the group, present or absent.
    db.lessons.push_back(lesson);
    for (size_t i = 0; i < group.studentIds.size(); i++) {
        int si = findStudentIndex(db, group.studentIds[i]);
        if (si != -1) db.students[si].balance -= group.pricePerLesson;
    }

    if (!saveLessons(db.lessons) || !saveStudents(db.students)) {
        db.lessons.pop_back();             // undo
        for (size_t i = 0; i < group.studentIds.size(); i++) {
            int si = findStudentIndex(db, group.studentIds[i]);
            if (si != -1) db.students[si].balance += group.pricePerLesson;
        }
        error = "Could not save the data files.";
        return false;
    }
    return true;
}
