// reports.h - the monthly report.
// No cin/cout here: functions build the numbers and the CSV text.
#ifndef REPORTS_H
#define REPORTS_H

#include <string>
#include <vector>
#include "structs.h"

// Attendance of one group inside the chosen month.
struct GroupAttendance {
    int groupId;
    std::string groupName;
    int lessons;      // lessons held in the month
    int present;      // total "present" marks
    int absent;       // total "absent" marks
    int percent;      // present * 100 / (present + absent), or -1 if nothing was marked
};

struct MonthlyReport {
    std::string month;                       // YYYY-MM
    long long revenue;                       // payments received in the month
    long long totalDebt;                     // sum of all negative balances, right now
    std::vector<GroupAttendance> groups;
};

// True for a real month written as YYYY-MM.
bool isValidMonth(const std::string& month);

// Fill the report for this month. Returns false (and error) if the month is invalid.
bool buildMonthlyReport(const Database& db, const std::string& month,
                        MonthlyReport& report, std::string& error);

// The report as CSV text.
std::string monthlyReportCsv(const MonthlyReport& report);

// Write reports/monthly_report_YYYY-MM.csv. On success filePath holds where it went.
bool saveMonthlyReport(const MonthlyReport& report, std::string& filePath, std::string& error);

#endif
