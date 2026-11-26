// accounts.h - staff accounts, login and the account inbox.
// No cin/cout here: functions take values and return results.
#ifndef ACCOUNTS_H
#define ACCOUNTS_H

#include <string>
#include <vector>
#include "structs.h"

// Turn a password into a number. Same password always gives the same number.
long long hashPassword(const std::string& password);

// Check username + password. Returns the position of the account in
// db.accounts, or -1 if the username or password is wrong.
int login(const Database& db, const std::string& username, const std::string& password);

// Position of the account with this id in db.accounts, or -1 if none.
int findAccountIndex(const Database& db, int id);

// True for "director", "manager" or "teacher".
bool isValidRole(const std::string& role);

// Add a new account and save accounts.txt. Returns true on success.
// On failure returns false and puts the reason in error.
bool createAccount(Database& db, const std::string& username, const std::string& password,
                   const std::string& fullName, const std::string& role, std::string& error);

// ----- inbox (messages written by the alerts module in stage 4) -----

// How many unread messages this account has.
int countUnread(const Database& db, int accountId);

// All messages for this account, oldest first.
std::vector<Message> getInbox(const Database& db, int accountId);

// Mark all of this account's messages as read and save messages.txt.
bool markInboxRead(Database& db, int accountId);

#endif
