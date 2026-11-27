// tests/test.cpp - plain asserts for the LearNoor logic.
// Build and run from the project root (see docs). Each test builds its own small
// Database in memory, so the tests do not depend on the sample files in data/.
// Anything that saves goes to tests/tmp/, never to data/ or reports/.
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include "structs.h"
#include "storage.h"
#include "accounts.h"
#include "students.h"
#include "attendance.h"
#include "alerts.h"
#include "daily.h"
#include "reports.h"
#include "table.h"

// ======================================================================
// Helpers that build test data
// ======================================================================

static Account makeAccount(int id, const std::string& role) {
    Account a;
    a.id = id;
    a.username = role + std::to_string(id);
    a.passwordHash = hashPassword("pass");
    a.fullName = "Person " + std::to_string(id);
    a.role = role;
    return a;
}

// Accounts: 1 = director, 2 = manager, 3 = teacher, 4 = another teacher.
static Database baseDb() {
    Database db;
    db.accounts.push_back(makeAccount(1, "director"));
    db.accounts.push_back(makeAccount(2, "manager"));
    db.accounts.push_back(makeAccount(3, "teacher"));
    db.accounts.push_back(makeAccount(4, "teacher"));
    return db;
}

static void addGroupTo(Database& db, int id, int teacherId, long long price, const std::string& day) {
    Group g;
    g.id = id;
    g.name = "Group" + std::to_string(id);
    g.teacherId = teacherId;
    g.pricePerLesson = price;
    g.days.push_back(day);
    db.groups.push_back(g);
}

// Add a student and put him into the group (groupId 0 = no group).
static void addStudentTo(Database& db, int id, long long balance, int groupId) {
    Student s;
    s.id = id;
    s.fullName = "Student" + std::to_string(id);
    s.phone = "901000000";
    s.parentName = "Parent" + std::to_string(id);
    s.parentPhone = "902000000";
    s.balance = balance;
    if (groupId != 0) {
        s.groupIds.push_back(groupId);
        db.groups[findGroupIndex(db, groupId)].studentIds.push_back(id);
    }
    db.students.push_back(s);
}

static void addLessonTo(Database& db, int groupId, const std::string& date,
                        const std::vector<int>& present, const std::vector<int>& absent) {
    Lesson l;
    l.id = nextId(db.lessons);
    l.groupId = groupId;
    l.date = date;
    l.presentIds = present;
    l.absentIds = absent;
    db.lessons.push_back(l);
}

// How many messages this person has of this type about this student.
static int msgCount(const Database& db, int toId, const std::string& type, int studentId) {
    int count = 0;
    for (size_t i = 0; i < db.messages.size(); i++) {
        const Message& m = db.messages[i];
        if (m.toId == toId && m.type == type && m.studentId == studentId) count++;
    }
    return count;
}

static bool sameInts(const std::vector<int>& a, const std::vector<int>& b) {
    return a == b;
}

// True when both databases hold exactly the same data.
static bool sameDatabase(const Database& a, const Database& b) {
    if (a.accounts.size() != b.accounts.size() || a.students.size() != b.students.size() ||
        a.groups.size() != b.groups.size() || a.payments.size() != b.payments.size() ||
        a.lessons.size() != b.lessons.size() || a.messages.size() != b.messages.size()) return false;
    for (size_t i = 0; i < a.accounts.size(); i++) {
        const Account& x = a.accounts[i]; const Account& y = b.accounts[i];
        if (x.id != y.id || x.username != y.username || x.passwordHash != y.passwordHash ||
            x.fullName != y.fullName || x.role != y.role) return false;
    }
    for (size_t i = 0; i < a.students.size(); i++) {
        const Student& x = a.students[i]; const Student& y = b.students[i];
        if (x.id != y.id || x.fullName != y.fullName || x.phone != y.phone ||
            x.parentName != y.parentName || x.parentPhone != y.parentPhone ||
            x.balance != y.balance || !sameInts(x.groupIds, y.groupIds)) return false;
    }
    for (size_t i = 0; i < a.groups.size(); i++) {
        const Group& x = a.groups[i]; const Group& y = b.groups[i];
        if (x.id != y.id || x.name != y.name || x.teacherId != y.teacherId ||
            x.pricePerLesson != y.pricePerLesson || x.days != y.days ||
            !sameInts(x.studentIds, y.studentIds)) return false;
    }
    for (size_t i = 0; i < a.payments.size(); i++) {
        const Payment& x = a.payments[i]; const Payment& y = b.payments[i];
        if (x.id != y.id || x.studentId != y.studentId || x.amount != y.amount || x.date != y.date) return false;
    }
    for (size_t i = 0; i < a.lessons.size(); i++) {
        const Lesson& x = a.lessons[i]; const Lesson& y = b.lessons[i];
        if (x.id != y.id || x.groupId != y.groupId || x.date != y.date ||
            !sameInts(x.presentIds, y.presentIds) || !sameInts(x.absentIds, y.absentIds)) return false;
    }
    for (size_t i = 0; i < a.messages.size(); i++) {
        const Message& x = a.messages[i]; const Message& y = b.messages[i];
        if (x.id != y.id || x.toId != y.toId || x.type != y.type || x.studentId != y.studentId ||
            x.text != y.text || x.date != y.date || x.read != y.read) return false;
    }
    return true;
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ======================================================================
// storage
// ======================================================================

static void testStorageSaveThenLoad() {
    std::cout << "storage: save then load gives the same data\n";
    Database db = baseDb();
    Student s1 = {1, "Ali Valiyev", "901", "Karim", "902", 400000, {1, 2}};
    Student s2 = {2, "No Group Kid", "903", "Parent", "904", -50000, {}};   // negative, no groups
    db.students.push_back(s1);
    db.students.push_back(s2);
    Group g1 = {1, "English A1", 3, 100000, {"Mon", "Wed"}, {1}};
    Group g2 = {2, "Empty", 3, 50000, {"Fri"}, {}};                          // no students
    db.groups.push_back(g1);
    db.groups.push_back(g2);
    Payment p = {1, 1, 500000, "2026-11-03"};
    db.payments.push_back(p);
    Lesson l1 = {1, 1, "2026-11-16", {1}, {}};                               // nobody absent
    Lesson l2 = {2, 1, "2026-11-18", {}, {1}};                               // nobody present
    db.lessons.push_back(l1);
    db.lessons.push_back(l2);
    Message m1 = {1, 2, "debt", 2, "some text, with comma", "2026-11-26", false};
    Message m2 = {2, 3, "low_balance", 1, "other", "2026-11-26", true};
    db.messages.push_back(m1);
    db.messages.push_back(m2);

    assert(saveAll(db));
    Database loaded;
    loadAll(loaded);
    assert(sameDatabase(db, loaded));
}

static void testStorageBadFiles() {
    std::cout << "storage: missing file, broken lines and Windows line endings\n";
    std::filesystem::remove("tests/tmp/messages.txt");
    writeFile("tests/tmp/accounts.txt",
              "garbage\n1|x|5|Name|manager\r\n1|too|few\n\n");
    Database db;
    loadAll(db);
    assert(db.messages.empty());                 // missing file = empty list
    assert(db.accounts.size() == 1);             // 2 broken lines skipped
    assert(db.accounts[0].role == "manager");    // \r removed from the last field
}

static void testStorageNextIdAndSafeText() {
    std::cout << "storage: nextId and isSafeText\n";
    std::vector<Student> none;
    assert(nextId(none) == 1);
    std::vector<Student> list(3);
    list[0].id = 3; list[1].id = 7; list[2].id = 2;
    assert(nextId(list) == 8);                   // biggest + 1, not size + 1
    assert(isSafeText("Ali Valiyev"));
    assert(!isSafeText("A|B"));
    assert(!isSafeText("A\nB"));
    assert(!isSafeText("A\rB"));
}

// ======================================================================
// accounts
// ======================================================================

static void testAccountsLogin() {
    std::cout << "accounts: hash and login\n";
    assert(hashPassword("admin123") == 251803100);
    assert(hashPassword("abc") == hashPassword("abc"));
    assert(hashPassword("abc") != hashPassword("abd"));
    Database db = baseDb();
    assert(login(db, "manager2", "pass") == 1);        // position of the account
    assert(login(db, "manager2", "wrong") == -1);
    assert(login(db, "nobody", "pass") == -1);
    Database empty;
    assert(login(empty, "x", "y") == -1);
    assert(findAccountIndex(db, 3) == 2);
    assert(findAccountIndex(db, 99) == -1);
}

static void testAccountsCreate() {
    std::cout << "accounts: createAccount rules\n";
    Database db = baseDb();
    std::string error;
    assert(createAccount(db, "newguy", "secret", "New Guy", "teacher", error));
    assert(db.accounts.size() == 5 && db.accounts[4].id == 5);   // max id + 1
    assert(db.accounts[4].passwordHash == hashPassword("secret"));

    assert(!createAccount(db, "newguy", "secret", "Other", "teacher", error));   // duplicate
    assert(!createAccount(db, "a b", "secret", "Name", "teacher", error));       // space in username
    assert(!createAccount(db, "", "secret", "Name", "teacher", error));
    assert(!createAccount(db, "short", "abc", "Name", "teacher", error));        // password < 4
    assert(!createAccount(db, "badrole", "secret", "Name", "boss", error));
    assert(!createAccount(db, "bar", "secret", "Na|me", "teacher", error));      // | not allowed
    assert(!createAccount(db, "noname", "secret", "", "teacher", error));
    assert(db.accounts.size() == 5);                                             // nothing was added

    Database loaded;
    loadAll(loaded);                                                             // it was saved to the file
    assert(loaded.accounts.size() == 5 && loaded.accounts[4].username == "newguy");
}

static void testAccountsInbox() {
    std::cout << "accounts: unread count and marking read\n";
    Database db = baseDb();
    Message a = {1, 2, "debt", 5, "x", "2026-11-26", false};
    Message b = {2, 2, "debt", 6, "y", "2026-11-26", false};
    Message c = {3, 2, "debt", 7, "z", "2026-11-26", true};
    Message d = {4, 3, "debt", 8, "w", "2026-11-26", false};   // belongs to someone else
    db.messages = {a, b, c, d};
    assert(countUnread(db, 2) == 2);
    assert(getInbox(db, 2).size() == 3);
    assert(markInboxRead(db, 2));
    assert(countUnread(db, 2) == 0);
    assert(countUnread(db, 3) == 1);                          // other inbox untouched
}

// ======================================================================
// students (helpers, students, groups, payments)
// ======================================================================

static void testFormatMoneyAndDates() {
    std::cout << "students: formatMoney and isValidDate\n";
    assert(formatMoney(0) == "0");
    assert(formatMoney(999) == "999");
    assert(formatMoney(1000) == "1 000");
    assert(formatMoney(1234567) == "1 234 567");
    assert(formatMoney(100000000) == "100 000 000");
    assert(formatMoney(-50000) == "-50 000");
    assert(formatMoney(-999) == "-999");

    assert(isValidDate("2026-11-26"));
    assert(isValidDate("2024-02-29"));      // leap year
    assert(isValidDate("2000-02-29"));      // divisible by 400
    assert(!isValidDate("2026-02-29"));
    assert(!isValidDate("2100-02-29"));     // divisible by 100 but not 400
    assert(!isValidDate("2026-04-31"));
    assert(!isValidDate("2026-13-01"));
    assert(!isValidDate("2026-00-10"));
    assert(!isValidDate("2026-11-00"));
    assert(!isValidDate("2026-4-1"));
    assert(!isValidDate("26-11-2026"));
    assert(!isValidDate("2026/11/26"));
    assert(!isValidDate("abcd-ef-gh"));
    assert(!isValidDate(""));
    assert(!isValidDate("1999-12-31"));     // before 2000 is not accepted
}

static void testAddStudentAndSearch() {
    std::cout << "students: addStudent and search\n";
    Database db = baseDb();
    std::string error;
    assert(addStudent(db, "Dilnoza Tosheva", "901110003", "Nodira", "901110004", error) == 1);
    assert(addStudent(db, "Ali Valiyev", "+998901234567", "Karim", "901110002", error) == 2);
    assert(addStudent(db, "Madina Saidova", "901110007", "Gulnora", "901110008", error) == 3);
    assert(db.students[0].balance == 0 && db.students[0].groupIds.empty());

    assert(addStudent(db, "", "901110003", "P", "901110004", error) == 0);       // empty name
    assert(addStudent(db, "X", "12", "P", "901110004", error) == 0);             // phone too short
    assert(addStudent(db, "X", "90111000a", "P", "901110004", error) == 0);      // letter in phone
    assert(addStudent(db, "X", "901110003", "P", "99", error) == 0);             // parent phone
    assert(addStudent(db, "X", "901110003", "", "901110004", error) == 0);       // empty parent
    assert(addStudent(db, "A|B", "901110003", "P", "901110004", error) == 0);
    assert(db.students.size() == 3);

    assert(searchStudentsByName(db, "ALI").size() == 1);       // upper/lower case does not matter
    assert(searchStudentsByName(db, "tosh")[0] == 0);          // position of Dilnoza
    assert(searchStudentsByName(db, "a").size() == 3);
    assert(searchStudentsByName(db, "").empty());
    assert(searchStudentsByName(db, "zzz").empty());
}

static void testAddGroup() {
    std::cout << "students: addGroup rules\n";
    Database db = baseDb();
    std::string error;
    int id = addGroup(db, "English", 3, 100000, "Mon, Wed,Fri", error);
    assert(id == 1);
    assert(db.groups[0].days.size() == 3 && db.groups[0].days[1] == "Wed");

    assert(addGroup(db, "X", 2, 100000, "Mon", error) == 0);          // teacher id is a manager
    assert(addGroup(db, "X", 99, 100000, "Mon", error) == 0);         // no such account
    assert(addGroup(db, "X", 3, 0, "Mon", error) == 0);
    assert(addGroup(db, "X", 3, -5, "Mon", error) == 0);
    assert(addGroup(db, "X", 3, 100000001, "Mon", error) == 0);       // above the limit
    assert(addGroup(db, "X", 3, 100000, "Mon,Mon", error) == 0);      // repeated day
    assert(addGroup(db, "X", 3, 100000, "Funday", error) == 0);
    assert(addGroup(db, "X", 3, 100000, "", error) == 0);
    assert(addGroup(db, "", 3, 100000, "Mon", error) == 0);
    assert(addGroup(db, "A|B", 3, 100000, "Mon", error) == 0);
    assert(addGroup(db, "Edge", 3, 100000000, "Sat", error) == 2);    // the limit itself is allowed
    assert(db.groups.size() == 2);
}

static void testEnrollAndRemove() {
    std::cout << "students: enroll and remove update both records\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addStudentTo(db, 10, 0, 0);
    std::string error;
    assert(enrollStudent(db, 10, 1, error));
    assert(db.students[0].groupIds == std::vector<int>({1}));
    assert(db.groups[0].studentIds == std::vector<int>({10}));
    assert(!enrollStudent(db, 10, 1, error));       // already in the group
    assert(!enrollStudent(db, 99, 1, error));       // unknown student
    assert(!enrollStudent(db, 10, 99, error));      // unknown group
    assert(db.groups[0].studentIds.size() == 1);

    addLessonTo(db, 1, "2026-11-20", {10}, {});
    assert(removeStudentFromGroup(db, 10, 1, error));
    assert(db.students[0].groupIds.empty() && db.groups[0].studentIds.empty());
    assert(db.lessons.size() == 1);                 // old lessons stay as history
    assert(!removeStudentFromGroup(db, 10, 1, error));   // not in the group any more
}

static void testRecordPayment() {
    std::cout << "students: recordPayment adds to the balance\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addStudentTo(db, 10, 50000, 1);
    addStudentTo(db, 11, -50000, 1);
    std::string error;
    assert(recordPayment(db, 10, 70000, "2026-11-05", error));
    assert(db.students[0].balance == 120000);
    assert(db.payments.size() == 1 && db.payments[0].id == 1);
    assert(db.payments[0].studentId == 10 && db.payments[0].amount == 70000);
    assert(db.payments[0].date == "2026-11-05");
    assert(recordPayment(db, 11, 70000, "2026-11-05", error));
    assert(db.students[1].balance == 20000);        // debt is paid off first
    assert(db.payments[1].id == 2);

    assert(!recordPayment(db, 10, 0, "2026-11-05", error));
    assert(!recordPayment(db, 10, -5, "2026-11-05", error));
    assert(!recordPayment(db, 99, 1000, "2026-11-05", error));
    assert(!recordPayment(db, 10, 1000, "2026-13-05", error));
    assert(!recordPayment(db, 10, 10000000001LL, "2026-11-05", error));
    assert(db.students[0].balance == 120000 && db.payments.size() == 2);   // nothing changed
}

// ======================================================================
// attendance
// ======================================================================

static void testMarkAttendanceCharges() {
    std::cout << "attendance: every student is charged, present or absent\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addStudentTo(db, 10, 500000, 1);
    addStudentTo(db, 11, 50000, 1);
    addStudentTo(db, 12, 0, 1);
    addStudentTo(db, 13, 777, 0);                   // not in the group
    Account teacher = db.accounts[2];
    std::string error;
    assert(markAttendance(db, teacher, 1, "2026-11-23", {10}, error));
    assert(db.students[0].balance == 400000);       // present: charged
    assert(db.students[1].balance == -50000);       // absent: charged too
    assert(db.students[2].balance == -100000);      // goes below zero = debt
    assert(db.students[3].balance == 777);          // outsider untouched
    assert(db.lessons.size() == 1);
    assert(db.lessons[0].presentIds == std::vector<int>({10}));
    assert(db.lessons[0].absentIds == std::vector<int>({11, 12}));   // everybody else is absent
    assert(hasLesson(db, 1, "2026-11-23"));
    assert(!hasLesson(db, 1, "2026-11-24"));

    // A student who left the group is not charged for the next lesson.
    std::string e2;
    assert(removeStudentFromGroup(db, 12, 1, e2));
    assert(markAttendance(db, teacher, 1, "2026-11-25", {10, 11}, error));
    assert(db.students[2].balance == -100000);
    assert(db.students[0].balance == 300000);
    assert(lessonsOfGroup(db, 1).size() == 2);
}

static void testMarkAttendanceRules() {
    std::cout << "attendance: permissions and refusals change nothing\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addGroupTo(db, 2, 4, 100000, "Tue");
    addStudentTo(db, 10, 500000, 1);
    Account teacher3 = db.accounts[2];
    Account teacher4 = db.accounts[3];
    Account manager = db.accounts[1];
    std::string error;

    assert(!markAttendance(db, teacher4, 1, "2026-11-23", {10}, error));   // not his group
    assert(!markAttendance(db, teacher3, 9, "2026-11-23", {10}, error));   // no such group
    assert(!markAttendance(db, teacher3, 1, "2026-02-30", {10}, error));   // bad date
    assert(!markAttendance(db, teacher3, 1, "2026-11-23", {99}, error));   // present id not in group
    assert(!markAttendance(db, teacher3, 2, "2026-11-23", {}, error));     // teacher 3, group of teacher 4
    assert(db.lessons.empty() && db.students[0].balance == 500000);

    assert(markAttendance(db, manager, 1, "2026-11-23", {}, error));       // a manager may mark any group
    assert(db.students[0].balance == 400000);
    assert(!markAttendance(db, teacher3, 1, "2026-11-23", {10}, error));   // same day again
    assert(db.students[0].balance == 400000 && db.lessons.size() == 1);    // not charged twice

    assert(!markAttendance(db, manager, 2, "2026-11-23", {}, error));      // group 2 has no students

    assert(visibleGroupIndexes(db, teacher3) == std::vector<int>({0}));
    assert(visibleGroupIndexes(db, teacher4) == std::vector<int>({1}));
    assert(visibleGroupIndexes(db, manager).size() == 2);
    assert(visibleGroupIndexes(db, db.accounts[0]).size() == 2);
}

// ======================================================================
// alerts
// ======================================================================

static void testLowBalanceAndDebtThresholds() {
    std::cout << "alerts: low balance and debt at their thresholds\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addGroupTo(db, 2, 3, 50000, "Tue");
    addStudentTo(db, 10, 199999, 1);   // covers 1 lesson  -> low
    addStudentTo(db, 11, 200000, 1);   // covers 2 lessons -> fine
    addStudentTo(db, 12, 0, 1);        // covers 0         -> low
    addStudentTo(db, 13, -1, 1);       // debt, not "low"
    addStudentTo(db, 14, 50, 0);       // no group: no price to compare
    addStudentTo(db, 15, 120000, 1);   // in groups 1 and 2: cheapest price 50 000 -> covers 2
    addStudentTo(db, 16, 99999, 1);    // cheapest price 50 000 -> covers 1 -> low
    for (int id = 15; id <= 16; id++) {
        db.students[findStudentIndex(db, id)].groupIds.push_back(2);
        db.groups[1].studentIds.push_back(id);
    }

    assert(lessonsCovered(db, db.students[0]) == 1);
    assert(lessonsCovered(db, db.students[1]) == 2);
    assert(lessonsCovered(db, db.students[2]) == 0);
    assert(lessonsCovered(db, db.students[4]) == -1);
    assert(lessonsCovered(db, db.students[5]) == 2);
    assert(lessonsCovered(db, db.students[6]) == 1);
    assert(isLowBalance(db, db.students[0]));
    assert(!isLowBalance(db, db.students[1]));
    assert(isLowBalance(db, db.students[2]));
    assert(!isLowBalance(db, db.students[3]));    // a debtor is not "low balance"
    assert(!isLowBalance(db, db.students[4]));
    assert(!isLowBalance(db, db.students[5]));
    assert(isLowBalance(db, db.students[6]));
    assert(countDebtors(db) == 1);

    checkAlerts(db, "2026-11-26");
    assert(msgCount(db, 2, "low_balance", 10) == 1);
    assert(msgCount(db, 2, "low_balance", 11) == 0);
    assert(msgCount(db, 2, "low_balance", 12) == 1);
    assert(msgCount(db, 2, "low_balance", 13) == 0);
    assert(msgCount(db, 2, "low_balance", 14) == 0);
    assert(msgCount(db, 2, "low_balance", 15) == 0);
    assert(msgCount(db, 2, "low_balance", 16) == 1);
    assert(msgCount(db, 2, "debt", 13) == 1);
    assert(msgCount(db, 2, "debt", 12) == 0);     // balance 0 is not a debt
    for (int id = 10; id <= 16; id++) {           // only managers get money alerts
        assert(msgCount(db, 1, "low_balance", id) == 0 && msgCount(db, 1, "debt", id) == 0);
        assert(msgCount(db, 3, "low_balance", id) == 0 && msgCount(db, 3, "debt", id) == 0);
    }
}

static void testAbsenceStreak() {
    std::cout << "alerts: absence streak = absent from the last 2 lessons\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    for (int id = 20; id <= 23; id++) addStudentTo(db, id, 1000000, 1);
    addStudentTo(db, 25, 1000000, 1);                         // joined later, in no lesson
    addLessonTo(db, 1, "2026-10-01", {20, 21}, {22, 23});
    addLessonTo(db, 1, "2026-10-02", {22}, {20, 21, 23});
    addLessonTo(db, 1, "2026-10-03", {21, 23}, {20, 22});
    assert(hasAbsenceStreak(db, 1, 20));       // absent in lessons 2 and 3
    assert(!hasAbsenceStreak(db, 1, 21));      // absent only in lesson 2
    assert(!hasAbsenceStreak(db, 1, 22));      // absent only in lesson 3
    assert(!hasAbsenceStreak(db, 1, 23));
    assert(!hasAbsenceStreak(db, 1, 25));

    checkAlerts(db, "2026-11-26");
    assert(msgCount(db, 2, "absence_streak", 20) == 1);       // manager
    assert(msgCount(db, 3, "absence_streak", 20) == 1);       // the group's teacher
    assert(msgCount(db, 1, "absence_streak", 20) == 0);       // not the director
    assert(msgCount(db, 4, "absence_streak", 20) == 0);       // not another teacher
    for (int id = 21; id <= 25; id++) assert(msgCount(db, 2, "absence_streak", id) == 0);

    Database one = baseDb();                                  // a single lesson is not a streak
    addGroupTo(one, 1, 3, 100000, "Mon");
    addStudentTo(one, 30, 1000000, 1);
    addLessonTo(one, 1, "2026-10-01", {}, {30});
    assert(!hasAbsenceStreak(one, 1, 30));

    // "The last 2 lessons" means the two newest DATES, even if an older day was saved late.
    Database late = baseDb();
    addGroupTo(late, 1, 3, 100000, "Mon");
    addStudentTo(late, 31, 1000000, 1);
    addLessonTo(late, 1, "2026-10-05", {}, {31});
    addLessonTo(late, 1, "2026-10-06", {}, {31});
    addLessonTo(late, 1, "2026-10-01", {31}, {});             // saved last, but it is the OLDEST day
    assert(hasAbsenceStreak(late, 1, 31));
    assert(lessonsOfGroup(late, 1)[0].date == "2026-10-01"); // lessons come back oldest date first
}

static std::string dayOfOctober(int i) {
    return i < 10 ? "2026-10-0" + std::to_string(i) : "2026-10-" + std::to_string(i);
}

static void testLowAttendance() {
    std::cout << "alerts: low attendance = under 70% once the group has 4+ lessons\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    for (int id = 30; id <= 32; id++) addStudentTo(db, id, 1000000, 1);
    for (int i = 1; i <= 10; i++) {                  // 10 lessons
        std::vector<int> present, absent;
        (i <= 3 ? absent : present).push_back(30);   // 30: 7 of 10 = exactly 70%
        (i <= 4 ? absent : present).push_back(31);   // 31: 6 of 10 = 60%
        (i <= 2 ? absent : present).push_back(32);   // 32: 8 of 10 = 80%
        addLessonTo(db, 1, dayOfOctober(i), present, absent);
    }
    assert(studentAttendancePercent(db, 1, 30) == 70);
    assert(studentAttendancePercent(db, 1, 31) == 60);
    assert(studentAttendancePercent(db, 1, 32) == 80);
    assert(!hasLowAttendance(db, 1, 30));            // exactly 70% is NOT under 70%
    assert(hasLowAttendance(db, 1, 31));
    assert(!hasLowAttendance(db, 1, 32));
    checkAlerts(db, "2026-11-26");
    assert(msgCount(db, 2, "low_attendance", 31) == 1);
    assert(msgCount(db, 3, "low_attendance", 31) == 1);
    assert(msgCount(db, 1, "low_attendance", 31) == 0);
    assert(msgCount(db, 2, "low_attendance", 30) == 0);
    assert(msgCount(db, 2, "low_attendance", 32) == 0);

    // The rule only starts at 4 lessons.
    Database few = baseDb();
    addGroupTo(few, 1, 3, 100000, "Mon");
    addStudentTo(few, 40, 1000000, 1);
    addStudentTo(few, 41, 1000000, 1);
    addLessonTo(few, 1, "2026-10-01", {41}, {40});
    addLessonTo(few, 1, "2026-10-02", {41}, {40});
    addLessonTo(few, 1, "2026-10-03", {41}, {40});
    assert(studentAttendancePercent(few, 1, 40) == 0);
    assert(!hasLowAttendance(few, 1, 40));           // only 3 lessons so far
    addLessonTo(few, 1, "2026-10-04", {40}, {41});   // 4th lesson: 40 has 1/4, 41 has 3/4
    assert(hasLowAttendance(few, 1, 40));            // 25% < 70%
    assert(!hasLowAttendance(few, 1, 41));           // 75% is fine
    assert(studentAttendancePercent(few, 1, 99) == -1);   // never listed: no percent
}

static void testNoDuplicateAlerts() {
    std::cout << "alerts: no duplicate while unread, new alert after reading\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addStudentTo(db, 20, 1000000, 1);
    addStudentTo(db, 21, 1000000, 1);
    addLessonTo(db, 1, "2026-10-01", {21}, {20});
    addLessonTo(db, 1, "2026-10-02", {21}, {20});

    int first = checkAlerts(db, "2026-11-26");
    assert(first == 2);                                       // streak -> manager + teacher
    size_t total = db.messages.size();
    assert(checkAlerts(db, "2026-11-26") == 0);               // same problem, still unread
    assert(db.messages.size() == total);

    assert(markInboxRead(db, 2));                             // the manager reads his inbox
    int teacherBefore = (int)getInbox(db, 3).size();
    assert(checkAlerts(db, "2026-11-27") == 1);               // only the manager gets a new one
    assert((int)getInbox(db, 3).size() == teacherBefore);     // the teacher still has it unread
    assert(countUnread(db, 2) == 1);

    addStudentTo(db, 22, 1000000, 1);                         // a different student is independent
    addLessonTo(db, 1, "2026-10-03", {21}, {20, 22});
    addLessonTo(db, 1, "2026-10-04", {21}, {20, 22});
    checkAlerts(db, "2026-11-28");
    assert(msgCount(db, 2, "absence_streak", 22) == 1);
}

static void testDirectorDebtMilestones() {
    std::cout << "alerts: directors hear about every 20 debtors\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    for (int id = 100; id < 119; id++) addStudentTo(db, id, -1000, 1);   // 19 debtors
    checkAlerts(db, "2026-11-26");
    assert(countDebtors(db) == 19);
    assert(msgCount(db, 1, "debt_milestone", 0) == 0);

    addStudentTo(db, 119, -1000, 1);                                     // 20th debtor
    checkAlerts(db, "2026-11-26");
    assert(msgCount(db, 1, "debt_milestone", 0) == 1);
    assert(checkAlerts(db, "2026-11-27") == 0);                          // not repeated
    assert(msgCount(db, 1, "debt_milestone", 0) == 1);

    for (int id = 120; id < 140; id++) addStudentTo(db, id, -1000, 1);   // 40 debtors
    checkAlerts(db, "2026-11-28");
    assert(msgCount(db, 1, "debt_milestone", 0) == 2);

    for (int id = 140; id < 145; id++) addStudentTo(db, id, -1000, 1);   // 45: still 2
    checkAlerts(db, "2026-11-29");
    assert(msgCount(db, 1, "debt_milestone", 0) == 2);

    int managerMilestones = 0;                                           // managers get the single alerts only
    for (size_t i = 0; i < db.messages.size(); i++) {
        if (db.messages[i].toId != 1 && db.messages[i].type == "debt_milestone") managerMilestones++;
    }
    assert(managerMilestones == 0);
    assert(msgCount(db, 1, "debt", 100) == 0);                           // the director gets no single debt alert
    assert(msgCount(db, 2, "debt", 100) == 1);
}

// ======================================================================
// daily list
// ======================================================================

static void testWeekdayName() {
    std::cout << "daily: weekdayName\n";
    assert(weekdayName("2026-11-26") == "Thu");
    assert(weekdayName("2026-11-23") == "Mon");
    assert(weekdayName("2024-02-29") == "Thu");
    assert(weekdayName("2000-01-01") == "Sat");
    assert(weekdayName("2026-01-04") == "Sun");
    assert(weekdayName("2026-02-30") == "");
    assert(weekdayName("nonsense") == "");
}

static void testSortByBalance() {
    std::cout << "daily: sortByBalance (selection sort)\n";
    std::vector<Student> list(4);
    long long balances[] = {5, -3, 10, 0};
    for (int i = 0; i < 4; i++) { list[i].id = i + 1; list[i].balance = balances[i]; }
    sortByBalance(list);
    assert(list[0].balance == -3 && list[1].balance == 0 && list[2].balance == 5 && list[3].balance == 10);
    assert(list[0].id == 2 && list[3].id == 3);              // the whole student moved, not just the number

    std::vector<Student> empty;
    sortByBalance(empty);
    assert(empty.empty());
    std::vector<Student> one(1);
    one[0].balance = 7;
    sortByBalance(one);
    assert(one[0].balance == 7);

    std::vector<Student> reversed(5);                        // worst case: biggest first
    for (int i = 0; i < 5; i++) reversed[i].balance = 50 - i * 10;
    sortByBalance(reversed);
    for (int i = 0; i + 1 < 5; i++) assert(reversed[i].balance <= reversed[i + 1].balance);

    std::vector<Student> same(3);                            // equal balances keep all students
    same[0].id = 1; same[1].id = 2; same[2].id = 3;
    sortByBalance(same);
    assert(same.size() == 3 && same[0].id + same[1].id + same[2].id == 6);
}

static void testBuildDailyList() {
    std::cout << "daily: buildDailyList\n";
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Thu");
    addGroupTo(db, 2, 3, 100000, "Mon");
    addStudentTo(db, 10, 1000000, 1);   // fine
    addStudentTo(db, 11, 150000, 1);    // low balance
    addStudentTo(db, 12, -20000, 1);    // debt
    addStudentTo(db, 13, -90000, 1);    // bigger debt
    addStudentTo(db, 14, 50, 0);        // no group: not on the list
    addLessonTo(db, 1, "2026-11-12", {11, 12, 13}, {10});
    addLessonTo(db, 1, "2026-11-19", {11, 12, 13}, {10});

    DailyList list = buildDailyList(db, "2026-11-26");          // a Thursday
    assert(list.todayGroupIds == std::vector<int>({1}));
    assert(list.callList.size() == 3);
    assert(list.callList[0].id == 13 && list.callList[1].id == 12 && list.callList[2].id == 11);
    assert(list.warnings.size() == 1);                          // student 10: absent twice in a row
    assert(list.warnings[0].studentId == 10 && list.warnings[0].groupId == 1);

    db.groups[0].studentIds.push_back(999);                     // a student id that does not exist
    db.lessons[0].absentIds.push_back(999);
    db.lessons[1].absentIds.push_back(999);
    assert(buildDailyList(db, "2026-11-26").warnings.size() == 1);   // skipped, no crash

    DailyList monday = buildDailyList(db, "2026-11-23");
    assert(monday.todayGroupIds == std::vector<int>({2}));
}

// ======================================================================
// reports
// ======================================================================

static Database reportDb() {
    Database db = baseDb();
    addGroupTo(db, 1, 3, 100000, "Mon");
    addGroupTo(db, 2, 3, 100000, "Tue");
    db.groups[0].name = "Math";
    db.groups[1].name = "Empty";
    addStudentTo(db, 10, -50, 1);
    addStudentTo(db, 11, -25, 1);
    addStudentTo(db, 12, 100, 1);
    Payment p1 = {1, 10, 100, "2026-10-31"};
    Payment p2 = {2, 11, 200, "2026-11-01"};
    Payment p3 = {3, 12, 300, "2026-11-30"};
    Payment p4 = {4, 12, 400, "2026-12-01"};
    db.payments = {p1, p2, p3, p4};
    addLessonTo(db, 1, "2026-10-20", {10, 11, 12}, {});          // another month
    addLessonTo(db, 1, "2026-11-03", {10, 11, 12}, {});
    addLessonTo(db, 1, "2026-11-10", {10, 11}, {12});
    return db;
}

static void testReportNumbers() {
    std::cout << "reports: revenue, debt and attendance per group\n";
    assert(isValidMonth("2026-11"));
    assert(isValidMonth("2000-01"));
    assert(!isValidMonth("2026-13"));
    assert(!isValidMonth("2026-00"));
    assert(!isValidMonth("2026-1"));
    assert(!isValidMonth("2026/11"));
    assert(!isValidMonth("1999-12"));
    assert(!isValidMonth(""));

    Database db = reportDb();
    MonthlyReport report;
    std::string error;
    assert(!buildMonthlyReport(db, "2026-13", report, error));
    assert(buildMonthlyReport(db, "2026-11", report, error));
    assert(report.revenue == 500);                 // 200 + 300, only November payments
    assert(report.totalDebt == 75);                // 50 + 25
    assert(report.groups.size() == 2);
    assert(report.groups[0].lessons == 2 && report.groups[0].present == 5 && report.groups[0].absent == 1);
    assert(report.groups[0].percent == 83);        // 5 * 100 / 6
    assert(report.groups[1].lessons == 0 && report.groups[1].percent == -1);   // nothing marked

    MonthlyReport october;
    assert(buildMonthlyReport(db, "2026-10", october, error));
    assert(october.revenue == 100 && october.groups[0].lessons == 1 && october.groups[0].percent == 100);
    MonthlyReport nothing;
    assert(buildMonthlyReport(db, "2025-01", nothing, error));
    assert(nothing.revenue == 0 && nothing.groups[0].percent == -1);
}

static void testReportCsvAndFile() {
    std::cout << "reports: CSV text and file\n";
    Database db = reportDb();
    MonthlyReport report;
    std::string error;
    assert(buildMonthlyReport(db, "2026-11", report, error));
    std::string expected =
        "Monthly report,2026-11\n"
        "Revenue (UZS),500\n"
        "Total debt (UZS),75\n"
        "\n"
        "Group,Lessons,Present,Absent,Attendance %\n"
        "Math,2,5,1,83\n"
        "Empty,0,0,0,n/a\n";
    assert(monthlyReportCsv(report) == expected);

    db.groups[0].name = "Math, \"adv\"";                          // comma and quote need quoting
    assert(buildMonthlyReport(db, "2026-11", report, error));
    assert(monthlyReportCsv(report).find("\"Math, \"\"adv\"\"\",2,5,1,83") != std::string::npos);

    assert(buildMonthlyReport(db, "2026-11", report, error));
    std::string path;
    assert(saveMonthlyReport(report, path, error));
    assert(path == "tests/tmp/monthly_report_2026-11.csv");
    assert(readFile(path) == monthlyReportCsv(report));

    setReportsFolder("tests/does_not_exist/");                    // a missing folder must give an error
    assert(!saveMonthlyReport(report, path, error));
    assert(error != "");
    setReportsFolder("tests/tmp/");
}

// ======================================================================
// table
// ======================================================================

static void testFormatTable() {
    std::cout << "table: formatTable\n";
    std::string expected =
        "+----+------+\n"
        "| ID | Name |\n"
        "+----+------+\n"
        "|  1 | Ali  |\n"
        "| 10 | Bo   |\n"
        "+----+------+\n";
    assert(formatTable({"ID", "Name"}, {{"1", "Ali"}, {"10", "Bo"}}, "rl") == expected);

    std::string noRows =                                          // titles only
        "+----+\n"
        "| ID |\n"
        "+----+\n"
        "+----+\n";
    assert(formatTable({"ID"}, {}, "") == noRows);

    std::string wide =                                            // column grows with the longest cell
        "+-------+\n"
        "| Name  |\n"
        "+-------+\n"
        "| Анвар |\n"
        "| Anvar |\n"
        "+-------+\n";
    assert(formatTable({"Name"}, {{"Анвар"}, {"Anvar"}}) == wide);   // Cyrillic counts as 1 place per letter

    std::string shortRow = formatTable({"A", "B"}, {{"x"}});       // a row with a missing cell
    assert(shortRow.find("| x | ") != std::string::npos);
}

// ======================================================================

int main() {
    std::filesystem::create_directories("tests/tmp");
    setDataFolder("tests/tmp/");
    setReportsFolder("tests/tmp/");

    testStorageSaveThenLoad();
    testStorageBadFiles();
    testStorageNextIdAndSafeText();
    testAccountsLogin();
    testAccountsCreate();
    testAccountsInbox();
    testFormatMoneyAndDates();
    testAddStudentAndSearch();
    testAddGroup();
    testEnrollAndRemove();
    testRecordPayment();
    testMarkAttendanceCharges();
    testMarkAttendanceRules();
    testLowBalanceAndDebtThresholds();
    testAbsenceStreak();
    testLowAttendance();
    testNoDuplicateAlerts();
    testDirectorDebtMilestones();
    testWeekdayName();
    testSortByBalance();
    testBuildDailyList();
    testReportNumbers();
    testReportCsvAndFile();
    testFormatTable();

    std::cout << "\nALL TESTS PASSED\n";
    return 0;
}
