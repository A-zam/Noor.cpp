// students.cpp - students, groups, enrolling, searching and payments.
#include "students.h"
#include "storage.h"
#include "accounts.h"

// ---------- small helpers ----------

std::string formatMoney(long long amount) {
    bool negative = (amount < 0);
    if (negative) amount = -amount;
    std::string digits = std::to_string(amount);
    std::string result = "";
    int count = 0;
    // Walk the digits from the right, put a space after every 3 digits.
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        result = digits[i] + result;
        count++;
        if (count % 3 == 0 && i > 0) result = " " + result;
    }
    if (negative) result = "-" + result;
    return result;
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isValidDate(const std::string& date) {
    if (date.size() != 10) return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (date[i] != '-') return false;
        } else {
            if (!isDigit(date[i])) return false;
        }
    }
    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));
    if (year < 2000 || month < 1 || month > 12 || day < 1) return false;

    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int maxDay = daysInMonth[month - 1];
    if (month == 2 && leap) maxDay = 29;
    return day <= maxDay;
}

int findStudentIndex(const Database& db, int id) {
    for (size_t i = 0; i < db.students.size(); i++) {
        if (db.students[i].id == id) return (int)i;
    }
    return -1;
}

int findGroupIndex(const Database& db, int id) {
    for (size_t i = 0; i < db.groups.size(); i++) {
        if (db.groups[i].id == id) return (int)i;
    }
    return -1;
}

// Phone: optional + at the start, then 7 to 15 digits.
static bool isValidPhone(const std::string& phone) {
    size_t start = 0;
    if (phone != "" && phone[0] == '+') start = 1;
    size_t digits = phone.size() - start;
    if (digits < 7 || digits > 15) return false;
    for (size_t i = start; i < phone.size(); i++) {
        if (!isDigit(phone[i])) return false;
    }
    return true;
}

static std::string toLower(const std::string& text) {
    std::string result = text;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i] >= 'A' && result[i] <= 'Z') result[i] = result[i] + ('a' - 'A');
    }
    return result;
}

static bool contains(const std::vector<int>& list, int value) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == value) return true;
    }
    return false;
}

// Remove the first copy of value from the list. Returns true if it was there.
static bool removeValue(std::vector<int>& list, int value) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == value) {
            list.erase(list.begin() + i);
            return true;
        }
    }
    return false;
}

// ---------- students ----------

int addStudent(Database& db, const std::string& fullName, const std::string& phone,
               const std::string& parentName, const std::string& parentPhone,
               std::string& error) {
    if (fullName == "") { error = "Student name cannot be empty."; return 0; }
    if (parentName == "") { error = "Parent name cannot be empty."; return 0; }
    if (!isSafeText(fullName) || !isSafeText(parentName)) { error = "The | character is not allowed."; return 0; }
    if (!isValidPhone(phone)) { error = "Student phone must be 7-15 digits (optional + at the start)."; return 0; }
    if (!isValidPhone(parentPhone)) { error = "Parent phone must be 7-15 digits (optional + at the start)."; return 0; }

    Student s;
    s.id = nextId(db.students);
    s.fullName = fullName;
    s.phone = phone;
    s.parentName = parentName;
    s.parentPhone = parentPhone;
    s.balance = 0;
    db.students.push_back(s);

    if (!saveStudents(db.students)) {
        db.students.pop_back();
        error = "Could not write data/students.txt.";
        return 0;
    }
    return s.id;
}

std::vector<int> searchStudentsByName(const Database& db, const std::string& text) {
    std::vector<int> found;
    if (text == "") return found;
    std::string wanted = toLower(text);
    for (size_t i = 0; i < db.students.size(); i++) {
        if (toLower(db.students[i].fullName).find(wanted) != std::string::npos) {
            found.push_back((int)i);
        }
    }
    return found;
}

// ---------- groups ----------

static bool isValidDay(const std::string& day) {
    return day == "Mon" || day == "Tue" || day == "Wed" || day == "Thu" ||
           day == "Fri" || day == "Sat" || day == "Sun";
}

// "Mon, Wed,Fri" -> {"Mon","Wed","Fri"}. Returns false if a day is wrong or repeated.
static bool parseDays(const std::string& text, std::vector<std::string>& days) {
    days.clear();
    std::string current = "";
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == ',') {
            if (!isValidDay(current)) return false;
            for (size_t j = 0; j < days.size(); j++) {
                if (days[j] == current) return false;   // repeated day
            }
            days.push_back(current);
            current = "";
        } else if (text[i] != ' ') {
            current += text[i];
        }
    }
    return true;
}

int addGroup(Database& db, const std::string& name, int teacherId, long long pricePerLesson,
             const std::string& daysText, std::string& error) {
    if (name == "") { error = "Group name cannot be empty."; return 0; }
    if (!isSafeText(name)) { error = "The | character is not allowed."; return 0; }
    if (pricePerLesson <= 0 || pricePerLesson > 100000000) {
        error = "Price per lesson must be between 1 and 100 000 000.";
        return 0;
    }
    int teacherIndex = findAccountIndex(db, teacherId);
    if (teacherIndex == -1 || db.accounts[teacherIndex].role != "teacher") {
        error = "There is no teacher with that id.";
        return 0;
    }
    std::vector<std::string> days;
    if (!parseDays(daysText, days)) {
        error = "Days must look like Mon,Wed,Fri (Mon Tue Wed Thu Fri Sat Sun, no repeats).";
        return 0;
    }

    Group g;
    g.id = nextId(db.groups);
    g.name = name;
    g.teacherId = teacherId;
    g.pricePerLesson = pricePerLesson;
    g.days = days;
    db.groups.push_back(g);

    if (!saveGroups(db.groups)) {
        db.groups.pop_back();
        error = "Could not write data/groups.txt.";
        return 0;
    }
    return g.id;
}

bool enrollStudent(Database& db, int studentId, int groupId, std::string& error) {
    int si = findStudentIndex(db, studentId);
    int gi = findGroupIndex(db, groupId);
    if (si == -1) { error = "There is no student with that id."; return false; }
    if (gi == -1) { error = "There is no group with that id."; return false; }
    if (contains(db.groups[gi].studentIds, studentId)) {
        error = "That student is already in this group.";
        return false;
    }

    db.students[si].groupIds.push_back(groupId);
    db.groups[gi].studentIds.push_back(studentId);

    if (!saveStudents(db.students) || !saveGroups(db.groups)) {
        db.students[si].groupIds.pop_back();   // undo
        db.groups[gi].studentIds.pop_back();
        error = "Could not save the data files.";
        return false;
    }
    return true;
}

bool removeStudentFromGroup(Database& db, int studentId, int groupId, std::string& error) {
    int si = findStudentIndex(db, studentId);
    int gi = findGroupIndex(db, groupId);
    if (si == -1) { error = "There is no student with that id."; return false; }
    if (gi == -1) { error = "There is no group with that id."; return false; }
    if (!contains(db.groups[gi].studentIds, studentId)) {
        error = "That student is not in this group.";
        return false;
    }

    removeValue(db.students[si].groupIds, groupId);
    removeValue(db.groups[gi].studentIds, studentId);

    if (!saveStudents(db.students) || !saveGroups(db.groups)) {
        db.students[si].groupIds.push_back(groupId);   // undo
        db.groups[gi].studentIds.push_back(studentId);
        error = "Could not save the data files.";
        return false;
    }
    return true;
}

// ---------- payments ----------

bool recordPayment(Database& db, int studentId, long long amount, const std::string& date,
                   std::string& error) {
    int si = findStudentIndex(db, studentId);
    if (si == -1) { error = "There is no student with that id."; return false; }
    if (amount <= 0) { error = "The amount must be more than 0."; return false; }
    if (amount > 10000000000LL) { error = "The amount is too large."; return false; }
    if (!isValidDate(date)) { error = "The date must look like YYYY-MM-DD."; return false; }

    Payment p;
    p.id = nextId(db.payments);
    p.studentId = studentId;
    p.amount = amount;
    p.date = date;
    db.payments.push_back(p);
    db.students[si].balance += amount;

    if (!savePayments(db.payments) || !saveStudents(db.students)) {
        db.payments.pop_back();            // undo
        db.students[si].balance -= amount;
        error = "Could not save the data files.";
        return false;
    }
    return true;
}
