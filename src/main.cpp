// main.cpp - TEMPORARY stage 1 version: only checks that the data loads.
// Stage 2 replaces this with login and the role menu.
#include <iostream>
#include "storage.h"

int main() {
    Database db;
    loadAll(db);
    std::cout << "accounts: " << db.accounts.size() << "\n";
    std::cout << "students: " << db.students.size() << "\n";
    std::cout << "groups:   " << db.groups.size() << "\n";
    std::cout << "payments: " << db.payments.size() << "\n";
    std::cout << "lessons:  " << db.lessons.size() << "\n";
    std::cout << "messages: " << db.messages.size() << "\n";
    return 0;
}
