// reports.cpp - monthly revenue, debt and attendance, as a CSV file.
#include "reports.h"
#include "storage.h"

bool isValidMonth(const std::string& month) {
    if (month.size() != 7 || month[4] != '-') return false;
    for (int i = 0; i < 7; i++) {
        if (i != 4 && (month[i] < '0' || month[i] > '9')) return false;
    }
    int year = std::stoi(month.substr(0, 4));
    int m = std::stoi(month.substr(5, 2));
    return year >= 2000 && m >= 1 && m <= 12;
}

bool buildMonthlyReport(const Database& db, const std::string& month,
                        MonthlyReport& report, std::string& error) {
    if (!isValidMonth(month)) {
        error = "The month must look like YYYY-MM (for example 2026-11).";
        return false;
    }
    report.month = month;
    report.revenue = 0;
    report.totalDebt = 0;
    report.groups.clear();

    // Revenue: payments whose date starts with the month ("2026-11-03" starts with "2026-11").
    for (size_t i = 0; i < db.payments.size(); i++) {
        if (db.payments[i].date.substr(0, 7) == month) report.revenue += db.payments[i].amount;
    }

    // Total debt: add up every negative balance (as a positive number).
    for (size_t i = 0; i < db.students.size(); i++) {
        if (db.students[i].balance < 0) report.totalDebt += -db.students[i].balance;
    }

    // Attendance per group, using only the lessons of this month.
    for (size_t g = 0; g < db.groups.size(); g++) {
        GroupAttendance row;
        row.groupId = db.groups[g].id;
        row.groupName = db.groups[g].name;
        row.lessons = 0;
        row.present = 0;
        row.absent = 0;
        for (size_t i = 0; i < db.lessons.size(); i++) {
            const Lesson& lesson = db.lessons[i];
            if (lesson.groupId == row.groupId && lesson.date.substr(0, 7) == month) {
                row.lessons++;
                row.present += (int)lesson.presentIds.size();
                row.absent += (int)lesson.absentIds.size();
            }
        }
        if (row.present + row.absent == 0) row.percent = -1;
        else row.percent = row.present * 100 / (row.present + row.absent);
        report.groups.push_back(row);
    }
    return true;
}

// A name with a comma or quote would break the CSV columns, so wrap it in quotes.
static std::string csvField(const std::string& text) {
    if (text.find(',') == std::string::npos && text.find('"') == std::string::npos) return text;
    std::string result = "\"";
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '"') result += "\"\"";   // a quote inside quotes is doubled
        else result += text[i];
    }
    result += "\"";
    return result;
}

std::string monthlyReportCsv(const MonthlyReport& report) {
    std::string csv = "Monthly report," + report.month + "\n";
    csv += "Revenue (UZS)," + std::to_string(report.revenue) + "\n";
    csv += "Total debt (UZS)," + std::to_string(report.totalDebt) + "\n";
    csv += "\n";
    csv += "Group,Lessons,Present,Absent,Attendance %\n";
    for (size_t i = 0; i < report.groups.size(); i++) {
        const GroupAttendance& row = report.groups[i];
        csv += csvField(row.groupName) + "," + std::to_string(row.lessons) + "," +
               std::to_string(row.present) + "," + std::to_string(row.absent) + ",";
        csv += (row.percent == -1 ? "n/a" : std::to_string(row.percent)) + "\n";
    }
    return csv;
}

bool saveMonthlyReport(const MonthlyReport& report, std::string& filePath, std::string& error) {
    std::string fileName = "monthly_report_" + report.month + ".csv";
    if (!writeReportFile(fileName, monthlyReportCsv(report))) {
        error = "Could not write " + getReportsFolder() + fileName + ". Does the folder exist?";
        return false;
    }
    filePath = getReportsFolder() + fileName;
    return true;
}
