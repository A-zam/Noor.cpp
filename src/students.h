// students.h - students, groups, enrolling, searching and payments.
// No cin/cout here: functions take values and return results.
#ifndef STUDENTS_H
#define STUDENTS_H

#include <string>
#include <vector>
#include "structs.h"

// ----- small helpers (also used by attendance and later modules) -----

// 1234567 -> "1 234 567", -50000 -> "-50 000".
std::string formatMoney(long long amount);

// True for a real calendar date written as YYYY-MM-DD (checks month, day, leap years).
bool isValidDate(const std::string& date);

// Position in db.students / db.groups of the record with this id, or -1.
int findStudentIndex(const Database& db, int id);
int findGroupIndex(const Database& db, int id);

// ----- students -----

// Add a student with balance 0. Returns the new id, or 0 on failure
// (the reason is put in error).
int addStudent(Database& db, const std::string& fullName, const std::string& phone,
               const std::string& parentName, const std::string& parentPhone,
               std::string& error);

// Positions (in db.students) of every student whose name contains text.
// Upper/lower case does not matter. Empty text finds nothing.
std::vector<int> searchStudentsByName(const Database& db, const std::string& text);

// ----- groups -----

// Add a group. daysText looks like "Mon,Wed,Fri". Returns the new id or 0.
int addGroup(Database& db, const std::string& name, int teacherId, long long pricePerLesson,
             const std::string& daysText, std::string& error);

// Put a student into a group (updates both records). Returns true on success.
bool enrollStudent(Database& db, int studentId, int groupId, std::string& error);

// Take a student out of a group. Past lessons stay; future lessons are not charged.
bool removeStudentFromGroup(Database& db, int studentId, int groupId, std::string& error);

// ----- payments -----

// Add a payment and increase the student's balance. amount must be above 0.
bool recordPayment(Database& db, int studentId, long long amount, const std::string& date,
                   std::string& error);

#endif
