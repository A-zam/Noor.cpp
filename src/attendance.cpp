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
