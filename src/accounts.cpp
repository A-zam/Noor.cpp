// accounts.cpp - login, account creation and the inbox.
#include "accounts.h"
#include "storage.h"

long long hashPassword(const std::string& password) {
    long long h = 5381;
    for (size_t i = 0; i < password.size(); i++) {
        h = (h * 33 + (unsigned char)password[i]) % 1000000007LL;
    }
    return h;
}

int login(const Database& db, const std::string& username, const std::string& password) {
    long long hash = hashPassword(password);
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (db.accounts[i].username == username && db.accounts[i].passwordHash == hash) {
            return (int)i;
        }
    }
    return -1;
}

int findAccountIndex(const Database& db, int id) {
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (db.accounts[i].id == id) return (int)i;
    }
    return -1;
}

bool isValidRole(const std::string& role) {
    return role == "director" || role == "manager" || role == "teacher";
}

bool createAccount(Database& db, const std::string& username, const std::string& password,
                   const std::string& fullName, const std::string& role, std::string& error) {
    if (username == "") { error = "Username cannot be empty."; return false; }
    if (username.find(' ') != std::string::npos) { error = "Username cannot contain spaces."; return false; }
    if (!isSafeText(username) || !isSafeText(fullName)) { error = "The | character is not allowed."; return false; }
    if (fullName == "") { error = "Full name cannot be empty."; return false; }
    if (password.size() < 4) { error = "Password must have at least 4 characters."; return false; }
    if (!isValidRole(role)) { error = "Role must be director, manager or teacher."; return false; }
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (db.accounts[i].username == username) { error = "That username is already taken."; return false; }
    }

    Account a;
    a.id = nextId(db.accounts);
    a.username = username;
    a.passwordHash = hashPassword(password);
    a.fullName = fullName;
    a.role = role;
    db.accounts.push_back(a);

    if (!saveAccounts(db.accounts)) {
        db.accounts.pop_back();   // undo, so memory and file stay the same
        error = "Could not write data/accounts.txt.";
        return false;
    }
    return true;
}

int countUnread(const Database& db, int accountId) {
    int count = 0;
    for (size_t i = 0; i < db.messages.size(); i++) {
        if (db.messages[i].toId == accountId && !db.messages[i].read) count++;
    }
    return count;
}

std::vector<Message> getInbox(const Database& db, int accountId) {
    std::vector<Message> inbox;
    for (size_t i = 0; i < db.messages.size(); i++) {
        if (db.messages[i].toId == accountId) inbox.push_back(db.messages[i]);
    }
    return inbox;
}

bool markInboxRead(Database& db, int accountId) {
    for (size_t i = 0; i < db.messages.size(); i++) {
        if (db.messages[i].toId == accountId) db.messages[i].read = true;
    }
    return saveMessages(db.messages);
}
