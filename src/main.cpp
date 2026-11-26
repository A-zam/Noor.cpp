// main.cpp - the ONLY file that uses cin and cout.
// It reads what the user types, calls the logic functions, prints results.
#include <iostream>
#include <string>
#include <ctime>
#include <cstdlib>
#include "structs.h"
#include "storage.h"
#include "accounts.h"

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

// ---------- one menu per role ----------
// "me" is a COPY of the logged-in account on purpose: adding an account
// can move the vector in memory, and a reference to it would break.

static void directorMenu(Database& db, Account me, const std::string& today) {
    (void)today;   // used from stage 3 on
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
        else if (choice == 3) notBuilt("Students", 3);
        else if (choice == 4) notBuilt("Groups", 3);
        else if (choice == 5) notBuilt("Payments", 3);
        else if (choice == 6) notBuilt("Attendance", 3);
        else if (choice == 7) notBuilt("Daily action list", 4);
        else if (choice == 8) notBuilt("Monthly report", 4);
        else if (choice == 9) showInbox(db, me);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void managerMenu(Database& db, Account me, const std::string& today) {
    (void)today;
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
        else if (choice == 1) notBuilt("Students", 3);
        else if (choice == 2) notBuilt("Groups", 3);
        else if (choice == 3) notBuilt("Payments", 3);
        else if (choice == 4) notBuilt("Attendance", 3);
        else if (choice == 5) notBuilt("Daily action list", 4);
        else if (choice == 6) notBuilt("Monthly report", 4);
        else if (choice == 7) showInbox(db, me);
        else if (choice != -1) std::cout << "Error: no such menu option.\n";
    }
}

static void teacherMenu(Database& db, Account me, const std::string& today) {
    (void)today;
    while (true) {
        std::cout << "\n=== Teacher menu ===\n"
                  << "1. Mark attendance (my groups)\n"
                  << "2. Inbox (" << countUnread(db, me.id) << " unread)\n"
                  << "0. Logout\n";
        int choice = readChoice();
        if (choice == 0) return;
        else if (choice == 1) notBuilt("Attendance", 3);
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
