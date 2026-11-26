// alerts.h - alert rules and the inbox messages they create.
// No cin/cout here: functions take values and return results.
#ifndef ALERTS_H
#define ALERTS_H

#include <string>
#include "structs.h"

// Lessons the student can still pay for: balance / price of their cheapest group.
// Returns -1 if the student is in no group.
int lessonsCovered(const Database& db, const Student& student);

// Balance is not negative but covers fewer than 2 lessons.
bool isLowBalance(const Database& db, const Student& student);

// How many students have a balance below 0.
int countDebtors(const Database& db);

// Check every alert rule and write new messages into the inboxes (then save
// messages.txt). Call it after every payment and every attendance entry.
// An alert is skipped if that person already has an UNREAD message of the same
// type about the same student. Returns how many new messages were created.
int checkAlerts(Database& db, const std::string& today);

#endif
