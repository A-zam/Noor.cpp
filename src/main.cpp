// main.cpp - the ONLY file that uses cin and cout.
// It reads what the user types, calls the logic functions, prints results.
#include <iostream>
#include <string>
#include <ctime>
#include <cstdlib>
#include "structs.h"
#include "storage.h"
#include "accounts.h"
#include "students.h"
#include "attendance.h"

// ---------- input helpers ----------

// Print a question and read one whole line. If the input ends
// (Ctrl+Z / Ctrl+D) there is nothing more to read, so we stop the program.
static std::string readLine(const std::string& question) {
    std::cout << question;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\nInput ended. Goodbye.\n";
        std::exit(0);
    }
    return line;
}

// Turn text into a whole number. Only digits (and an optional leading -)
// are accepted. Returns false for letters, empty text or numbers too long.
static bool parseInt(const std::string& text, int& result) {
    if (text == "" || text.size() > 9) return false;
    size_t start = 0;
    if (text[0] == '-') start = 1;
    if (start == text.size()) return false;   // just a "-"
    for (size_t i = start; i < text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') return false;
    }
    result = std::stoi(text);
    return true;
}

// Same as parseInt but for money (up to 12 digits, so it needs long long).
static bool parseLong(const std::string& text, long long& result) {
    if (text == "" || text.size() > 13) return false;
    size_t start = 0;
    if (text[0] == '-') start = 1;
    if (start == text.size()) return false;
    for (size_t i = start; i < text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') return false;
    }
    result = std::stoll(text);
    return true;
}

// Today's date as YYYY-MM-DD. The clock is read ONLY here, once, and the
// result is passed to the logic functions as a plain string.
static std::string getToday() {
    std::time_t now = std::time(nullptr);
    std::tm* t = std::localtime(&now);
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", t);
    return std::string(buffer);
}

static void notBuilt(const std::string& name, int stage) {
    std::cout << name << " is not built yet (it comes in stage " << stage << ").\n";
}

// ---------- screens shared by the menus ----------

static void showInbox(Database& db, const Account& me) {
    std::vector<Message> inbox = getInbox(db, me.id);
    if (inbox.empty()) {
        std::cout << "Your inbox is empty.\n";
        return;
    }
    std::cout << "--- Inbox (" << inbox.size() << " messages) ---\n";
    for (size_t i = 0; i < inbox.size(); i++) {
        std::cout << (inbox[i].read ? "      " : "[NEW] ")
                  << inbox[i].date << "  " << inbox[i].text << "\n";
    }
    if (!markInboxRead(db, me.id)) {
        std::cout << "Error: could not save messages.txt.\n";
    }
}

static void listAccounts(const Database& db) {
    std::cout << "--- Accounts ---\n";
    for (size_t i = 0; i < db.accounts.size(); i++) {
        const Account& a = db.accounts[i];
        std::cout << a.id << ". " << a.fullName << " (" << a.username << ", " << a.role << ")\n";
    }
}

static void createAccountScreen(Database& db) {
    std::string username = readLine("New username: ");
    std::string password = readLine("Password (min 4 characters): ");
    std::string fullName = readLine("Full name: ");
    std::string role = readLine("Role (director / manager / teacher): ");
    std::string error;
    if (createAccount(db, username, password, fullName, role, error)) {
        std::cout << "Account created.\n";
    } else {
        std::cout << "Error: " << error << "\n";
    }
}

// Ask for a menu number. Prints an error and returns -1 if it is not a number.
static int readChoice() {
    std::string line = readLine("Choose: ");
    int choice;
    if (!parseInt(line, choice)) {
        std::cout << "Error: please type a number from the menu.\n";
        return -1;
    }
    return choice;
}

// ---------- students, groups, payments, attendance screens ----------

// Ask for a whole number. Prints an error and returns false if it is not one.
static bool askInt(const std::string& question, int& value) {
    std::string line = readLine(question);
    if (!parseInt(line, value)) {
        std::cout << "Error: please type a whole number.\n";
        return false;
    }
    return true;
}

static void printStudent(const Database& db, const Student& s) {
    std::cout << s.id << ". " << s.fullName << " | phone " << s.phone
              << " | parent " << s.parentName << " " << s.parentPhone
              << " | balance " << formatMoney(s.balance) << " UZS | groups: ";
    if (s.groupIds.empty()) std::cout << "none";
    for (size_t i = 0; i < s.groupIds.size(); i++) {
        int gi = findGroupIndex(db, s.groupIds[i]);
        if (i > 0) std::cout << ", ";
        std::cout << (gi == -1 ? "?" : db.groups[gi].name);
    }
    std::cout << "\n";
}

static void printGroup(const Database& db, const Group& g) {
    int ti = findAccountIndex(db, g.teacherId);
    std::cout << g.id << ". " << g.name << " | teacher "
              << (ti == -1 ? "?" : db.accounts[ti].fullName)
              << " | " << formatMoney(g.pricePerLesson) << " UZS/lesson | days ";
    for (size_t i = 0; i < g.days.size(); i++) {
        if (i > 0) std::cout << ",";
        std::cout << g.days[i];
    }
    std::cout << " | " << g.studentIds.size() << " students\n";
}

static void addStudentScreen(Database& db) {
    std::string name = readLine("Student full name: ");
    std::string phone = readLine("Student phone (digits): ");
    std::string parentName = readLine("Parent name: ");
    std::string parentPhone = readLine("Parent phone (digits): ");
    std::string error;
    int id = addStudent(db, name, phone, parentName, parentPhone, error);
    if (id == 0) std::cout << "Error: " << error << "\n";
    else std::cout << "Student added with id " << id << ".\n";
}

static void listStudentsScreen(const Database& db) {
    if (db.students.empty()) { std::cout << "No students yet.\n"; return; }
    for (size_t i = 0; i < db.students.size(); i++) printStudent(db, db.students[i]);
}

static void searchStudentsScreen(const Database& db) {
    std::string text = readLine("Part of the name: ");
    std::vector<int> found = searchStudentsByName(db, text);
    if (found.empty()) { std::cout << "No student found.\n"; return; }
    for (size_t i = 0; i < found.size(); i++) printStudent(db, db.students[found[i]]);
}

static void listGroupsScreen(const Database& db) {
    if (db.groups.empty()) { std::cout << "No groups yet.\n"; return; }
    for (size_t i = 0; i < db.groups.size(); i++) printGroup(db, db.groups[i]);
}

// enroll = true puts a student into a group, false takes the student out.
static void enrollScreen(Database& db, bool enroll) {
    int studentId, groupId;
    if (!askInt("Student id: ", studentId)) return;
    if (!askInt("Group id: ", groupId)) return;
    std::string error;
    bool ok = enroll ? enrollStudent(db, studentId, groupId, error)
                     : removeStudentFromGroup(db, studentId, groupId, error);
    if (ok) std::cout << (enroll ? "Student enrolled.\n" : "Student removed from the group.\n");
    else std::cout << "Error: " << error << "\n";
}

static void addGroupScreen(Database& db) {
    std::cout << "Teachers:\n";
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (db.accounts[i].role == "teacher") {
            std::cout << "  " << db.accounts[i].id << ". " << db.accounts[i].fullName << "\n";
        }
    }
    std::string name = readLine("Group name: ");
    int teacherId;
    if (!askInt("Teacher id: ", teacherId)) return;
    long long price;
    if (!parseLong(readLine("Price per lesson (UZS): "), price)) {
        std::cout << "Error: please type a whole number.\n";
        return;
    }
    std::string days = readLine("Days (for example Mon,Wed,Fri): ");
    std::string error;
    int id = addGroup(db, name, teacherId, price, days, error);
    if (id == 0) std::cout << "Error: " << error << "\n";
    else std::cout << "Group added with id " << id << ".\n";
}

static void showGroupStudentsScreen(const Database& db) {
    int groupId;
    if (!askInt("Group id: ", groupId)) return;
    int gi = findGroupIndex(db, groupId);
    if (gi == -1) { std::cout << "Error: there is no group with that id.\n"; return; }
    printGroup(db, db.groups[gi]);
    for (size_t i = 0; i < db.groups[gi].studentIds.size(); i++) {
        int si = findStudentIndex(db, db.groups[gi].studentIds[i]);
        if (si != -1) printStudent(db, db.students[si]);
    }
}

static void studentsMenu(Database& db) {
    while (true) {
        std::cout << "\n--- Students ---\n"
                  << "1. Add student\n2. List all students\n3. Search by name\n"
                  << "4. Enroll student in a group\n5. Remove student from a group\n0. Back\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) addStudentScreen(db);
        else if (choice == 2) listStudentsScreen(db);
        else if (choice == 3) searchStudentsScreen(db);
        else if (choice == 4) enrollScreen(db, true);
        else if (choice == 5) enrollScreen(db, false);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void groupsMenu(Database& db) {
    while (true) {
        std::cout << "\n--- Groups ---\n"
                  << "1. Add group\n2. List groups\n3. Show students of a group\n0. Back\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) addGroupScreen(db);
        else if (choice == 2) listGroupsScreen(db);
        else if (choice == 3) showGroupStudentsScreen(db);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void paymentScreen(Database& db, const std::string& today) {
    int studentId;
    if (!askInt("Student id: ", studentId)) return;
    int si = findStudentIndex(db, studentId);
    if (si == -1) { std::cout << "Error: there is no student with that id.\n"; return; }
    std::cout << db.students[si].fullName << ", balance "
              << formatMoney(db.students[si].balance) << " UZS\n";
    long long amount;
    if (!parseLong(readLine("Amount (UZS): "), amount)) {
        std::cout << "Error: please type a whole number.\n";
        return;
    }
    std::string error;
    if (recordPayment(db, studentId, amount, today, error)) {
        std::cout << "Payment saved. New balance: "
                  << formatMoney(db.students[si].balance) << " UZS\n";
        // stage 4: the alert check (checkAlerts) will be called here
    } else {
        std::cout << "Error: " << error << "\n";
    }
}

static void attendanceScreen(Database& db, const Account& me, const std::string& today) {
    std::vector<int> visible = visibleGroupIndexes(db, me);
    if (visible.empty()) { std::cout << "There are no groups for you.\n"; return; }
    std::cout << "--- Groups ---\n";
    for (size_t i = 0; i < visible.size(); i++) printGroup(db, db.groups[visible[i]]);

    int groupId;
    if (!askInt("Group id: ", groupId)) return;
    bool allowed = false;
    for (size_t i = 0; i < visible.size(); i++) {
        if (db.groups[visible[i]].id == groupId) allowed = true;
    }
    if (!allowed) { std::cout << "Error: that is not one of your groups.\n"; return; }
    if (hasLesson(db, groupId, today)) {
        std::cout << "Error: attendance for this group is already saved for " << today << ".\n";
        return;
    }

    const Group& group = db.groups[findGroupIndex(db, groupId)];
    std::cout << "Attendance for " << group.name << " on " << today
              << ". Type p = present, a = absent.\n";
    std::vector<int> presentIds;
    for (size_t i = 0; i < group.studentIds.size(); i++) {
        int si = findStudentIndex(db, group.studentIds[i]);
        if (si == -1) continue;
        while (true) {
            std::string answer = readLine(db.students[si].fullName + " (p/a): ");
            if (answer == "p" || answer == "P") { presentIds.push_back(group.studentIds[i]); break; }
            if (answer == "a" || answer == "A") break;
            std::cout << "Error: type p or a.\n";
        }
    }

    std::string error;
    if (markAttendance(db, me, groupId, today, presentIds, error)) {
        std::cout << "Attendance saved. Every student was charged "
                  << formatMoney(group.pricePerLesson) << " UZS.\n";
        // stage 4: the alert check (checkAlerts) will be called here
    } else {
        std::cout << "Error: " << error << "\n";
    }
}

// ---------- one menu per role ----------
// "me" is a COPY of the logged-in account on purpose: adding an account
// can move the vector in memory, and a reference to it would break.

static void directorMenu(Database& db, Account me, const std::string& today) {
    while (true) {
        std::cout << "\n=== Director menu ===\n"
                  << "1. Create staff account\n"
                  << "2. List accounts\n"
                  << "3. Students\n"
                  << "4. Groups\n"
                  << "5. Record payment\n"
                  << "6. Mark attendance\n"
                  << "7. Daily action list\n"
                  << "8. Monthly report\n"
                  << "9. Inbox (" << countUnread(db, me.id) << " unread)\n"
                  << "0. Logout\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) createAccountScreen(db);
        else if (choice == 2) listAccounts(db);
        else if (choice == 3) studentsMenu(db);
        else if (choice == 4) groupsMenu(db);
        else if (choice == 5) paymentScreen(db, today);
        else if (choice == 6) attendanceScreen(db, me, today);
        else if (choice == 7) notBuilt("Daily action list", 4);
        else if (choice == 8) notBuilt("Monthly report", 4);
        else if (choice == 9) showInbox(db, me);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void managerMenu(Database& db, Account me, const std::string& today) {
    while (true) {
        std::cout << "\n=== Manager menu ===\n"
                  << "1. Students\n"
                  << "2. Groups\n"
                  << "3. Record payment\n"
                  << "4. Mark attendance\n"
                  << "5. Daily action list\n"
                  << "6. Monthly report\n"
                  << "7. Inbox (" << countUnread(db, me.id) << " unread)\n"
                  << "0. Logout\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) studentsMenu(db);
        else if (choice == 2) groupsMenu(db);
        else if (choice == 3) paymentScreen(db, today);
        else if (choice == 4) attendanceScreen(db, me, today);
        else if (choice == 5) notBuilt("Daily action list", 4);
        else if (choice == 6) notBuilt("Monthly report", 4);
        else if (choice == 7) showInbox(db, me);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void teacherMenu(Database& db, Account me, const std::string& today) {
    while (true) {
        std::cout << "\n=== Teacher menu ===\n"
                  << "1. Mark attendance (my groups)\n"
                  << "2. Inbox (" << countUnread(db, me.id) << " unread)\n"
                  << "0. Logout\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) attendanceScreen(db, me, today);
        else if (choice == 2) showInbox(db, me);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

// ---------- program start ----------

int main() {
    Database db;
    loadAll(db);
    if (db.accounts.empty()) {
        std::cout << "Error: no accounts found in data/accounts.txt. "
                  << "Run the program from the project folder.\n";
        return 1;
    }

    std::string today = getToday();
    std::cout << "=== LearNoor - education center CRM ===\n";
    std::cout << "Today: " << today << "\n";

    // Login: 3 tries.
    int index = -1;
    for (int attempt = 1; attempt <= 3 && index == -1; attempt++) {
        std::string username = readLine("Username: ");
        std::string password = readLine("Password: ");
        index = login(db, username, password);
        if (index == -1) {
            std::cout << "Error: wrong username or password (" << attempt << "/3).\n";
        }
    }
    if (index == -1) {
        std::cout << "Too many failed attempts. Goodbye.\n";
        return 1;
    }

    Account me = db.accounts[index];   // a copy of the logged-in account
    std::cout << "\nWelcome, " << me.fullName << " (" << me.role << ").\n";
    int unread = countUnread(db, me.id);
    if (unread > 0) {
        std::cout << "You have " << unread << " unread message(s).\n";
    }

    if (me.role == "director") directorMenu(db, me, today);
    else if (me.role == "manager") managerMenu(db, me, today);
    else if (me.role == "teacher") teacherMenu(db, me, today);
    else std::cout << "Error: unknown role '" << me.role << "'.\n";

    std::cout << "Logged out. Goodbye.\n";
    return 0;
}
