// storage.cpp - reading and writing the data/*.txt files.
// Format: one record per line, fields split by |, list fields use commas.
#include "storage.h"
#include <fstream>

static std::string dataFolder = "data/";

void setDataFolder(const std::string& folder) {
    dataFolder = folder;
}

static std::string reportsFolder = "reports/";

void setReportsFolder(const std::string& folder) {
    reportsFolder = folder;
}

std::string getReportsFolder() {
    return reportsFolder;
}

bool writeReportFile(const std::string& fileName, const std::string& content) {
    std::ofstream file(reportsFolder + fileName);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

bool isSafeText(const std::string& text) {
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '|' || text[i] == '\n' || text[i] == '\r') return false;
    }
    return true;
}

// ---------- small helpers ----------

// Cut a line at every separator. "a||b" gives {"a", "", "b"}.
// A trailing separator also gives an empty last piece.
static std::vector<std::string> split(const std::string& line, char sep) {
    std::vector<std::string> parts;
    std::string current = "";
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == sep) {
            parts.push_back(current);
            current = "";
        } else {
            current += line[i];
        }
    }
    parts.push_back(current);
    return parts;
}

// Text to number. Bad text gives 0 instead of crashing.
static long long toNumber(const std::string& text) {
    try {
        return std::stoll(text);
    } catch (...) {
        return 0;
    }
}

// "1,2,3" becomes {1,2,3}. Empty text becomes an empty list.
static std::vector<int> toIntList(const std::string& text) {
    std::vector<int> result;
    if (text == "") return result;
    std::vector<std::string> parts = split(text, ',');
    for (size_t i = 0; i < parts.size(); i++) {
        result.push_back((int)toNumber(parts[i]));
    }
    return result;
}

// {1,2,3} becomes "1,2,3"
static std::string fromIntList(const std::vector<int>& list) {
    std::string text = "";
    for (size_t i = 0; i < list.size(); i++) {
        if (i > 0) text += ",";
        text += std::to_string(list[i]);
    }
    return text;
}

// {"Mon","Wed"} becomes "Mon,Wed"
static std::string fromStringList(const std::vector<std::string>& list) {
    std::string text = "";
    for (size_t i = 0; i < list.size(); i++) {
        if (i > 0) text += ",";
        text += list[i];
    }
    return text;
}

// Read a file and collect its non-empty lines. A missing file gives no lines.
static std::vector<std::string> readLines(const std::string& name) {
    std::vector<std::string> lines;
    std::ifstream file(dataFolder + name);
    if (!file.is_open()) return lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);   // Windows line ending
        }
        if (line != "") lines.push_back(line);
    }
    return lines;
}

// ---------- loading ----------

static void loadAccounts(std::vector<Account>& list) {
    std::vector<std::string> lines = readLines("accounts.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 5) continue;   // broken line: skip it
        Account a;
        a.id = (int)toNumber(f[0]);
        a.username = f[1];
        a.passwordHash = toNumber(f[2]);
        a.fullName = f[3];
        a.role = f[4];
        list.push_back(a);
    }
}

static void loadStudents(std::vector<Student>& list) {
    std::vector<std::string> lines = readLines("students.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 7) continue;
        Student s;
        s.id = (int)toNumber(f[0]);
        s.fullName = f[1];
        s.phone = f[2];
        s.parentName = f[3];
        s.parentPhone = f[4];
        s.balance = toNumber(f[5]);
        s.groupIds = toIntList(f[6]);
        list.push_back(s);
    }
}

static void loadGroups(std::vector<Group>& list) {
    std::vector<std::string> lines = readLines("groups.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 6) continue;
        Group g;
        g.id = (int)toNumber(f[0]);
        g.name = f[1];
        g.teacherId = (int)toNumber(f[2]);
        g.pricePerLesson = toNumber(f[3]);
        if (f[4] != "") g.days = split(f[4], ',');
        g.studentIds = toIntList(f[5]);
        list.push_back(g);
    }
}

static void loadPayments(std::vector<Payment>& list) {
    std::vector<std::string> lines = readLines("payments.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 4) continue;
        Payment p;
        p.id = (int)toNumber(f[0]);
        p.studentId = (int)toNumber(f[1]);
        p.amount = toNumber(f[2]);
        p.date = f[3];
        list.push_back(p);
    }
}

static void loadLessons(std::vector<Lesson>& list) {
    std::vector<std::string> lines = readLines("lessons.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 5) continue;
        Lesson l;
        l.id = (int)toNumber(f[0]);
        l.groupId = (int)toNumber(f[1]);
        l.date = f[2];
        l.presentIds = toIntList(f[3]);
        l.absentIds = toIntList(f[4]);
        list.push_back(l);
    }
}

static void loadMessages(std::vector<Message>& list) {
    std::vector<std::string> lines = readLines("messages.txt");
    list.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> f = split(lines[i], '|');
        if (f.size() != 7) continue;
        Message m;
        m.id = (int)toNumber(f[0]);
        m.toId = (int)toNumber(f[1]);
        m.type = f[2];
        m.studentId = (int)toNumber(f[3]);
        m.text = f[4];
        m.date = f[5];
        m.read = (f[6] == "1");
        list.push_back(m);
    }
}

void loadAll(Database& db) {
    loadAccounts(db.accounts);
    loadStudents(db.students);
    loadGroups(db.groups);
    loadPayments(db.payments);
    loadLessons(db.lessons);
    loadMessages(db.messages);
}

// ---------- saving ----------

bool saveAccounts(const std::vector<Account>& list) {
    std::ofstream file(dataFolder + "accounts.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Account& a = list[i];
        file << a.id << "|" << a.username << "|" << a.passwordHash << "|"
             << a.fullName << "|" << a.role << "\n";
    }
    return true;
}

bool saveStudents(const std::vector<Student>& list) {
    std::ofstream file(dataFolder + "students.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Student& s = list[i];
        file << s.id << "|" << s.fullName << "|" << s.phone << "|"
             << s.parentName << "|" << s.parentPhone << "|" << s.balance << "|"
             << fromIntList(s.groupIds) << "\n";
    }
    return true;
}

bool saveGroups(const std::vector<Group>& list) {
    std::ofstream file(dataFolder + "groups.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Group& g = list[i];
        file << g.id << "|" << g.name << "|" << g.teacherId << "|"
             << g.pricePerLesson << "|" << fromStringList(g.days) << "|"
             << fromIntList(g.studentIds) << "\n";
    }
    return true;
}

bool savePayments(const std::vector<Payment>& list) {
    std::ofstream file(dataFolder + "payments.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Payment& p = list[i];
        file << p.id << "|" << p.studentId << "|" << p.amount << "|"
             << p.date << "\n";
    }
    return true;
}

bool saveLessons(const std::vector<Lesson>& list) {
    std::ofstream file(dataFolder + "lessons.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Lesson& l = list[i];
        file << l.id << "|" << l.groupId << "|" << l.date << "|"
             << fromIntList(l.presentIds) << "|" << fromIntList(l.absentIds) << "\n";
    }
    return true;
}

bool saveMessages(const std::vector<Message>& list) {
    std::ofstream file(dataFolder + "messages.txt");
    if (!file.is_open()) return false;
    for (size_t i = 0; i < list.size(); i++) {
        const Message& m = list[i];
        file << m.id << "|" << m.toId << "|" << m.type << "|" << m.studentId << "|"
             << m.text << "|" << m.date << "|" << (m.read ? 1 : 0) << "\n";
    }
    return true;
}

bool saveAll(const Database& db) {
    bool ok = true;
    if (!saveAccounts(db.accounts)) ok = false;
    if (!saveStudents(db.students)) ok = false;
    if (!saveGroups(db.groups)) ok = false;
    if (!savePayments(db.payments)) ok = false;
    if (!saveLessons(db.lessons)) ok = false;
    if (!saveMessages(db.messages)) ok = false;
    return ok;
}

// ---------- next id ----------

int nextId(const std::vector<Account>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
int nextId(const std::vector<Student>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
int nextId(const std::vector<Group>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
int nextId(const std::vector<Payment>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
int nextId(const std::vector<Lesson>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
int nextId(const std::vector<Message>& list) {
    int biggest = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id > biggest) biggest = list[i].id;
    return biggest + 1;
}
