// alerts.cpp - the four alert rules.
#include "alerts.h"
#include "attendance.h"
#include "students.h"
#include "accounts.h"
#include "storage.h"

// The numbers from the brief.
static const int LOW_BALANCE_LESSONS = 2;   // fewer than 2 lessons left
static const int DEBT_STEP = 20;            // directors hear about every 20 debtors

int lessonsCovered(const Database& db, const Student& student) {
    long long lowestPrice = 0;
    for (size_t i = 0; i < student.groupIds.size(); i++) {
        int gi = findGroupIndex(db, student.groupIds[i]);
        if (gi == -1) continue;
        long long price = db.groups[gi].pricePerLesson;
        if (price > 0 && (lowestPrice == 0 || price < lowestPrice)) lowestPrice = price;
    }
    if (lowestPrice == 0) return -1;   // no group, so no price
    return (int)(student.balance / lowestPrice);
}

bool isLowBalance(const Database& db, const Student& student) {
    if (student.balance < 0) return false;   // that is a debt, not a low balance
    int covered = lessonsCovered(db, student);
    return covered != -1 && covered < LOW_BALANCE_LESSONS;
}

int countDebtors(const Database& db) {
    int count = 0;
    for (size_t i = 0; i < db.students.size(); i++) {
        if (db.students[i].balance < 0) count++;
    }
    return count;
}

// ---------- helpers ----------

// Ids of all accounts with this role.
static std::vector<int> accountIdsWithRole(const Database& db, const std::string& role) {
    std::vector<int> ids;
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (db.accounts[i].role == role) ids.push_back(db.accounts[i].id);
    }
    return ids;
}

// Does this person already have an unread message of this type about this student?
static bool hasUnread(const Database& db, int toId, const std::string& type, int studentId) {
    for (size_t i = 0; i < db.messages.size(); i++) {
        const Message& m = db.messages[i];
        if (m.toId == toId && m.type == type && m.studentId == studentId && !m.read) return true;
    }
    return false;
}

// Add one message to the list (not saved yet).
static void addMessage(Database& db, int toId, const std::string& type, int studentId,
                       const std::string& text, const std::string& today) {
    Message m;
    m.id = nextId(db.messages);
    m.toId = toId;
    m.type = type;
    m.studentId = studentId;
    m.text = text;
    m.date = today;
    m.read = false;
    db.messages.push_back(m);
}

// Send the alert to every recipient who has no unread copy. Returns how many were sent.
static int sendAlert(Database& db, const std::vector<int>& recipients, const std::string& type,
                     int studentId, const std::string& text, const std::string& today) {
    int sent = 0;
    for (size_t i = 0; i < recipients.size(); i++) {
        if (!hasUnread(db, recipients[i], type, studentId)) {
            addMessage(db, recipients[i], type, studentId, text, today);
            sent++;
        }
    }
    return sent;
}

// How many messages of this type this person ever received (read or not).
static int countMessages(const Database& db, int toId, const std::string& type) {
    int count = 0;
    for (size_t i = 0; i < db.messages.size(); i++) {
        if (db.messages[i].toId == toId && db.messages[i].type == type) count++;
    }
    return count;
}

// ---------- the rules ----------

int checkAlerts(Database& db, const std::string& today) {
    size_t oldSize = db.messages.size();
    int created = 0;
    std::vector<int> managers = accountIdsWithRole(db, "manager");
    std::vector<int> directors = accountIdsWithRole(db, "director");

    // Rules 1 and 2: low balance and debt (managers), one student at a time.
    for (size_t i = 0; i < db.students.size(); i++) {
        const Student& s = db.students[i];
        if (s.balance < 0) {
            created += sendAlert(db, managers, "debt", s.id,
                s.fullName + " is in debt: " + formatMoney(s.balance) + " UZS", today);
        } else if (isLowBalance(db, s)) {
            created += sendAlert(db, managers, "low_balance", s.id,
                s.fullName + " has only " + std::to_string(lessonsCovered(db, s)) +
                " lesson(s) paid (balance " + formatMoney(s.balance) + " UZS)", today);
        }
    }

    // Rule 2, directors: one message each time the debtor count reaches 20, 40, 60...
    int milestones = countDebtors(db) / DEBT_STEP;
    for (size_t i = 0; i < directors.size(); i++) {
        int alreadySent = countMessages(db, directors[i], "debt_milestone");
        for (int step = alreadySent + 1; step <= milestones; step++) {
            addMessage(db, directors[i], "debt_milestone", 0,
                "Debt warning: " + std::to_string(step * DEBT_STEP) + " students or more are in debt.", today);
            created++;
        }
    }

    // Rules 3 and 4: attendance, group by group (teacher and managers).
    for (size_t g = 0; g < db.groups.size(); g++) {
        const Group& group = db.groups[g];
        std::vector<int> recipients = managers;
        if (findAccountIndex(db, group.teacherId) != -1) recipients.push_back(group.teacherId);

        for (size_t i = 0; i < group.studentIds.size(); i++) {
            int si = findStudentIndex(db, group.studentIds[i]);
            if (si == -1) continue;
            const Student& s = db.students[si];

            if (hasAbsenceStreak(db, group.id, s.id)) {
                created += sendAlert(db, recipients, "absence_streak", s.id,
                    s.fullName + " (" + group.name + ") was absent from the last 2 lessons", today);
            }
            if (hasLowAttendance(db, group.id, s.id)) {
                created += sendAlert(db, recipients, "low_attendance", s.id,
                    s.fullName + " (" + group.name + ") attendance is " +
                    std::to_string(studentAttendancePercent(db, group.id, s.id)) + "%", today);
            }
        }
    }

    if (created > 0 && !saveMessages(db.messages)) {
        while (db.messages.size() > oldSize) db.messages.pop_back();   // undo
        return 0;
    }
    return created;
}
