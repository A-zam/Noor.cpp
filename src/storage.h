// storage.h - the ONLY module that reads or writes files.
#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>
#include "structs.h"

// Load all 6 files into db. A missing file means an empty list.
void loadAll(Database& db);

// Rewrite one file from a vector. Return false if the file cannot be written.
bool saveAccounts(const std::vector<Account>& list);
bool saveStudents(const std::vector<Student>& list);
bool saveGroups(const std::vector<Group>& list);
bool savePayments(const std::vector<Payment>& list);
bool saveLessons(const std::vector<Lesson>& list);
bool saveMessages(const std::vector<Message>& list);

// Save everything (used by tests and after big changes).
bool saveAll(const Database& db);

// New id = biggest id in the list + 1 (1 if the list is empty).
int nextId(const std::vector<Account>& list);
int nextId(const std::vector<Student>& list);
int nextId(const std::vector<Group>& list);
int nextId(const std::vector<Payment>& list);
int nextId(const std::vector<Lesson>& list);
int nextId(const std::vector<Message>& list);

// Folder that holds the .txt files (default "data/"). Tests change it so
// they do not touch the real data. Must end with "/".
void setDataFolder(const std::string& folder);

#endif
