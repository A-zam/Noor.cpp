// api.cpp - the bridge between the web server and the LearNoor C++ modules.
//
// It is NOT a second copy of the rules. Every rule (charging, alerts, reports...)
// is still in src/*.cpp. This file only:
//   1. reads one command and its parameters,
//   2. calls the existing functions,
//   3. prints the result as JSON.
//
// Usage:   engine <command>          (parameters arrive on stdin, one "key=value" per line)
// Output:  one line of JSON, always {"ok":true,...} or {"ok":false,"error":"..."}
//
// Build (from the project root):
//   g++ -std=c++17 -Isrc web/engine/api.cpp src/accounts.cpp src/alerts.cpp src/attendance.cpp
//       src/daily.cpp src/reports.cpp src/storage.cpp src/students.cpp -o web/engine/engine

#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "accounts.h"
#include "alerts.h"
#include "attendance.h"
#include "daily.h"
#include "reports.h"
#include "storage.h"
#include "structs.h"
#include "students.h"

typedef std::map<std::string, std::string> Params;

// ---------- small JSON helpers (the standard library has none) ----------

// "Ali "the" B" -> "Ali \"the\" B" with quotes around it.
static std::string q(const std::string& text) {
    std::string out = "\"";
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out + "\"";
}

static std::string num(long long n) { return std::to_string(n); }

static std::string fail(const std::string& error) {
    return "{\"ok\":false,\"error\":" + q(error) + "}";
}

static std::string textList(const std::vector<std::string>& list) {
    std::string out = "[";
    for (size_t i = 0; i < list.size(); i++) {
        if (i > 0) out += ",";
        out += q(list[i]);
    }
    return out + "]";
}

// ---------- reading parameters ----------

static std::string get(const Params& p, const std::string& key) {
    Params::const_iterator it = p.find(key);
    return it == p.end() ? "" : it->second;
}

// A whole number, up to 13 digits. Returns false for anything else.
static bool getNumber(const Params& p, const std::string& key, long long& value) {
    std::string text = get(p, key);
    if (text.empty() || text.size() > 13) return false;
    size_t start = (text[0] == '-') ? 1 : 0;
    if (start == text.size()) return false;
    for (size_t i = start; i < text.size(); i++)
        if (text[i] < '0' || text[i] > '9') return false;
    value = std::stoll(text);
    return true;
}

// "1,2,3" -> {1,2,3}; anything that is not a number is ignored.
static std::vector<int> getIdList(const Params& p, const std::string& key) {
    std::vector<int> result;
    std::string text = get(p, key) + ",";
    std::string piece;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == ',') {
            if (!piece.empty() && piece.size() < 10 && piece.find_first_not_of("0123456789") == std::string::npos)
                result.push_back(std::stoi(piece));
            piece = "";
        } else {
            piece += text[i];
        }
    }
    return result;
}

static std::string getToday() {
    time_t now = time(nullptr);
    char buffer[16];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&now));
    return buffer;
}

// ---------- who may do what (the role rules of the console menus) ----------

static bool isAllowed(const std::string& role, const std::string& command) {
    static const std::set<std::string> everyone = {"me", "inbox", "groups", "group-sheet", "attendance"};
    static const std::set<std::string> manager = {
        "today", "students", "student", "add-student", "add-group", "enroll", "unenroll",
        "payment", "report"};
    static const std::set<std::string> director = {"accounts", "create-account"};

    if (everyone.count(command)) return true;
    if (role == "manager") return manager.count(command) > 0;
    if (role == "director") return manager.count(command) > 0 || director.count(command) > 0;
    return false;   // a teacher gets only the "everyone" commands
}

// ---------- JSON for one record ----------

static std::string accountJson(const Database& db, const Account& a) {
    return "{\"id\":" + num(a.id) + ",\"username\":" + q(a.username) + ",\"name\":" + q(a.fullName) +
           ",\"role\":" + q(a.role) + ",\"unread\":" + num(countUnread(db, a.id)) + "}";
}

// "ok", "low" (fewer than 2 lessons paid) or "debt": the same rules as the alerts.
static std::string balanceStatus(const Database& db, const Student& s) {
    if (s.balance < 0) return "debt";
    if (isLowBalance(db, s)) return "low";
    return "ok";
}

static std::string groupRefs(const Database& db, const std::vector<int>& groupIds) {
    std::string out = "[";
    bool first = true;
    for (size_t i = 0; i < groupIds.size(); i++) {
        int g = findGroupIndex(db, groupIds[i]);
        if (g == -1) continue;
        if (!first) out += ",";
        first = false;
        out += "{\"id\":" + num(db.groups[g].id) + ",\"name\":" + q(db.groups[g].name) + "}";
    }
    return out + "]";
}

// [{"id":1,"name":"Ali"}, ...] for a list of student ids (a missing student is skipped)
static std::string studentRefs(const Database& db, const std::vector<int>& ids) {
    std::string out = "[";
    for (size_t i = 0; i < ids.size(); i++) {
        int s = findStudentIndex(db, ids[i]);
        if (s == -1) continue;
        if (out.size() > 1) out += ",";
        out += "{\"id\":" + num(db.students[s].id) + ",\"name\":" + q(db.students[s].fullName) + "}";
    }
    return out + "]";
}

static std::string studentJson(const Database& db, const Student& s) {
    return "{\"id\":" + num(s.id) + ",\"name\":" + q(s.fullName) + ",\"phone\":" + q(s.phone) +
           ",\"parentName\":" + q(s.parentName) + ",\"parentPhone\":" + q(s.parentPhone) +
           ",\"balance\":" + num(s.balance) + ",\"lessons\":" + num(lessonsCovered(db, s)) +
           ",\"status\":" + q(balanceStatus(db, s)) + ",\"groups\":" + groupRefs(db, s.groupIds) + "}";
}

static std::string teacherName(const Database& db, int teacherId) {
    int t = findAccountIndex(db, teacherId);
    return t == -1 ? "(unknown)" : db.accounts[t].fullName;
}

static std::string groupJson(const Database& db, const Group& g, const std::string& today) {
    std::string students = "[";
    for (size_t i = 0; i < g.studentIds.size(); i++) {
        int s = findStudentIndex(db, g.studentIds[i]);
        if (s == -1) continue;
        if (students.size() > 1) students += ",";
        students += "{\"id\":" + num(db.students[s].id) + ",\"name\":" + q(db.students[s].fullName) +
                    ",\"balance\":" + num(db.students[s].balance) +
                    ",\"status\":" + q(balanceStatus(db, db.students[s])) +
                    ",\"attendance\":" + num(studentAttendancePercent(db, g.id, db.students[s].id)) + "}";
    }
    students += "]";

    std::vector<Lesson> lessons = lessonsOfGroup(db, g.id);
    std::string history = "[";
    for (size_t i = lessons.size(); i > 0; i--) {   // newest first
        if (history.size() > 1) history += ",";
        history += "{\"date\":" + q(lessons[i - 1].date) + ",\"weekday\":" + q(weekdayName(lessons[i - 1].date)) +
                   ",\"present\":" + studentRefs(db, lessons[i - 1].presentIds) +
                   ",\"absent\":" + studentRefs(db, lessons[i - 1].absentIds) + "}";
    }
    history += "]";

    return "{\"id\":" + num(g.id) + ",\"name\":" + q(g.name) + ",\"teacherId\":" + num(g.teacherId) +
           ",\"teacher\":" + q(teacherName(db, g.teacherId)) + ",\"price\":" + num(g.pricePerLesson) +
           ",\"days\":" + textList(g.days) + ",\"students\":" + students + ",\"history\":" + history +
           ",\"markedToday\":" + (hasLesson(db, g.id, today) ? "true" : "false") + "}";
}

// ---------- commands ----------

static std::string cmdLogin(const Database& db, const Params& p) {
    int index = login(db, get(p, "username"), get(p, "password"));
    if (index == -1) return fail("Wrong username or password.");
    return "{\"ok\":true,\"account\":" + accountJson(db, db.accounts[index]) + "}";
}

static std::string cmdToday(const Database& db, const std::string& today) {
    DailyList list = buildDailyList(db, today);

    std::string groups = "[";
    for (size_t i = 0; i < list.todayGroupIds.size(); i++) {
        int g = findGroupIndex(db, list.todayGroupIds[i]);
        if (g == -1) continue;
        if (groups.size() > 1) groups += ",";
        groups += "{\"id\":" + num(db.groups[g].id) + ",\"name\":" + q(db.groups[g].name) +
                  ",\"teacher\":" + q(teacherName(db, db.groups[g].teacherId)) +
                  ",\"students\":" + num(db.groups[g].studentIds.size()) +
                  ",\"marked\":" + (hasLesson(db, db.groups[g].id, today) ? "true" : "false") + "}";
    }
    groups += "]";

    std::string calls = "[";
    for (size_t i = 0; i < list.callList.size(); i++) {
        if (i > 0) calls += ",";
        calls += studentJson(db, list.callList[i]);
    }
    calls += "]";

    std::string warnings = "[";
    for (size_t i = 0; i < list.warnings.size(); i++) {
        int s = findStudentIndex(db, list.warnings[i].studentId);
        int g = findGroupIndex(db, list.warnings[i].groupId);
        if (s == -1 || g == -1) continue;
        if (warnings.size() > 1) warnings += ",";
        warnings += "{\"studentId\":" + num(db.students[s].id) + ",\"name\":" + q(db.students[s].fullName) +
                    ",\"group\":" + q(db.groups[g].name) + ",\"text\":" + q(list.warnings[i].text) + "}";
    }
    warnings += "]";

    return "{\"ok\":true,\"date\":" + q(today) + ",\"weekday\":" + q(weekdayName(today)) +
           ",\"debtors\":" + num(countDebtors(db)) + ",\"groups\":" + groups + ",\"calls\":" + calls +
           ",\"warnings\":" + warnings + "}";
}

static std::string cmdStudents(const Database& db, const Params& p) {
    std::string text = get(p, "q");
    std::vector<int> positions;
    if (text.empty()) {
        for (size_t i = 0; i < db.students.size(); i++) positions.push_back((int)i);
    } else {
        positions = searchStudentsByName(db, text);
    }
    std::string out = "{\"ok\":true,\"students\":[";
    for (size_t i = 0; i < positions.size(); i++) {
        if (i > 0) out += ",";
        out += studentJson(db, db.students[positions[i]]);
    }
    return out + "]}";
}

static std::string cmdStudent(const Database& db, const Params& p) {
    long long id;
    if (!getNumber(p, "id", id)) return fail("Missing student id.");
    int s = findStudentIndex(db, (int)id);
    if (s == -1) return fail("No student with that id.");

    std::string payments = "[";
    for (size_t i = db.payments.size(); i > 0; i--) {   // newest first
        if (db.payments[i - 1].studentId != (int)id) continue;
        if (payments.size() > 1) payments += ",";
        payments += "{\"date\":" + q(db.payments[i - 1].date) + ",\"amount\":" + num(db.payments[i - 1].amount) + "}";
    }
    payments += "]";

    std::string attendance = "[";
    for (size_t i = 0; i < db.students[s].groupIds.size(); i++) {
        int gid = db.students[s].groupIds[i];
        int g = findGroupIndex(db, gid);
        if (g == -1) continue;
        if (attendance.size() > 1) attendance += ",";
        attendance += "{\"group\":" + q(db.groups[g].name) + ",\"percent\":" + num(studentAttendancePercent(db, gid, (int)id)) + "}";
    }
    attendance += "]";

    return "{\"ok\":true,\"student\":" + studentJson(db, db.students[s]) + ",\"payments\":" + payments +
           ",\"attendance\":" + attendance + "}";
}

static std::string cmdGroups(const Database& db, const Account& me, const std::string& today) {
    std::vector<int> positions = visibleGroupIndexes(db, me);   // a teacher sees only their groups
    std::string out = "{\"ok\":true,\"today\":" + q(today) + ",\"groups\":[";
    for (size_t i = 0; i < positions.size(); i++) {
        if (i > 0) out += ",";
        out += groupJson(db, db.groups[positions[i]], today);
    }
    return out + "]}";
}

static std::string cmdInbox(Database& db, const Account& me, const Params& p) {
    std::vector<Message> inbox = getInbox(db, me.id);
    std::string out = "{\"ok\":true,\"messages\":[";
    for (size_t i = inbox.size(); i > 0; i--) {   // newest first
        const Message& m = inbox[i - 1];
        if (i != inbox.size()) out += ",";
        out += "{\"id\":" + num(m.id) + ",\"type\":" + q(m.type) + ",\"studentId\":" + num(m.studentId) +
               ",\"text\":" + q(m.text) + ",\"date\":" + q(m.date) + ",\"read\":" + (m.read ? "true" : "false") + "}";
    }
    out += "]}";
    if (get(p, "markRead") == "1") markInboxRead(db, me.id);   // the output above still shows what was new
    return out;
}

static std::string cmdAccounts(const Database& db) {
    std::string out = "{\"ok\":true,\"accounts\":[";
    for (size_t i = 0; i < db.accounts.size(); i++) {
        if (i > 0) out += ",";
        out += accountJson(db, db.accounts[i]);
    }
    return out + "]}";
}

static std::string cmdReport(const Database& db, const Params& p) {
    MonthlyReport report;
    std::string error;
    if (!buildMonthlyReport(db, get(p, "month"), report, error)) return fail(error);

    std::string groups = "[";
    for (size_t i = 0; i < report.groups.size(); i++) {
        const GroupAttendance& g = report.groups[i];
        if (i > 0) groups += ",";
        groups += "{\"name\":" + q(g.groupName) + ",\"lessons\":" + num(g.lessons) + ",\"present\":" + num(g.present) +
                  ",\"absent\":" + num(g.absent) + ",\"percent\":" + num(g.percent) + "}";
    }
    groups += "]";

    std::string path;
    bool saved = saveMonthlyReport(report, path, error);
    return "{\"ok\":true,\"month\":" + q(report.month) + ",\"revenue\":" + num(report.revenue) +
           ",\"debt\":" + num(report.totalDebt) + ",\"groups\":" + groups +
           ",\"file\":" + (saved ? q(path) : "null") + "}";
}

// After a payment or a lesson the alerts are checked, exactly like in the console program.
static std::string withAlerts(Database& db, const std::string& today, const std::string& extra) {
    int created = checkAlerts(db, today);
    return "{\"ok\":true," + extra + ",\"newAlerts\":" + num(created) + "}";
}

static std::string run(const std::string& command, const Params& p) {
    Database db;
    loadAll(db);
    std::string today = getToday();

    if (command == "login") return cmdLogin(db, p);

    // every other command needs a logged-in account, which the server puts in "as"
    long long asId;
    if (!getNumber(p, "as", asId)) return fail("Not logged in.");
    int meIndex = findAccountIndex(db, (int)asId);
    if (meIndex == -1) return fail("Not logged in.");
    Account me = db.accounts[meIndex];   // a copy, like in main.cpp
    if (!isAllowed(me.role, command)) return fail("Your role cannot do this.");

    std::string error;
    long long a, b;

    if (command == "me") return "{\"ok\":true,\"account\":" + accountJson(db, me) + ",\"today\":" + q(today) + "}";
    if (command == "today") return cmdToday(db, today);
    if (command == "students") return cmdStudents(db, p);
    if (command == "student") return cmdStudent(db, p);
    if (command == "groups") return cmdGroups(db, me, today);
    if (command == "inbox") return cmdInbox(db, me, p);
    if (command == "accounts") return cmdAccounts(db);
    if (command == "report") return cmdReport(db, p);

    if (command == "add-student") {
        int id = addStudent(db, get(p, "name"), get(p, "phone"), get(p, "parentName"), get(p, "parentPhone"), error);
        if (id == 0) return fail(error);
        return "{\"ok\":true,\"id\":" + num(id) + "}";
    }
    if (command == "add-group") {
        if (!getNumber(p, "teacherId", a)) return fail("Choose a teacher.");
        if (!getNumber(p, "price", b)) return fail("Price must be a whole number.");
        int id = addGroup(db, get(p, "name"), (int)a, b, get(p, "days"), error);
        if (id == 0) return fail(error);
        return "{\"ok\":true,\"id\":" + num(id) + "}";
    }
    if (command == "enroll" || command == "unenroll") {
        if (!getNumber(p, "studentId", a) || !getNumber(p, "groupId", b)) return fail("Choose a student and a group.");
        bool done = (command == "enroll") ? enrollStudent(db, (int)a, (int)b, error)
                                          : removeStudentFromGroup(db, (int)a, (int)b, error);
        if (!done) return fail(error);
        return "{\"ok\":true}";
    }
    if (command == "payment") {
        if (!getNumber(p, "studentId", a)) return fail("Choose a student.");
        if (!getNumber(p, "amount", b)) return fail("Amount must be a whole number.");
        std::string date = get(p, "date").empty() ? today : get(p, "date");
        if (!recordPayment(db, (int)a, b, date, error)) return fail(error);
        int s = findStudentIndex(db, (int)a);
        return withAlerts(db, today, "\"balance\":" + num(db.students[s].balance));
    }
    if (command == "attendance") {
        if (!getNumber(p, "groupId", a)) return fail("Choose a group.");
        std::string date = get(p, "date").empty() ? today : get(p, "date");
        if (!markAttendance(db, me, (int)a, date, getIdList(p, "present"), error)) return fail(error);
        return withAlerts(db, today, "\"date\":" + q(date));
    }
    if (command == "create-account") {
        if (!createAccount(db, get(p, "username"), get(p, "password"), get(p, "fullName"), get(p, "role"), error))
            return fail(error);
        return "{\"ok\":true}";
    }
    return fail("Unknown command.");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << fail("No command.") << "\n";
        return 1;
    }
    Params params;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        params[line.substr(0, eq)] = line.substr(eq + 1);
    }
    std::cout << run(argv[1], params) << "\n";
    return 0;
}
