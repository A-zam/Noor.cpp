// structs.h - all the data types of LearNoor in one place.
// Every struct is one line in one data file.
#ifndef STRUCTS_H
#define STRUCTS_H

#include <string>
#include <vector>

// accounts.txt: id|username|passwordHash|fullName|role
struct Account {
    int id;
    std::string username;
    long long passwordHash;
    std::string fullName;
    std::string role;       // "director", "manager" or "teacher"
};

// students.txt: id|fullName|phone|parentName|parentPhone|balance|groupIds
struct Student {
    int id;
    std::string fullName;
    std::string phone;
    std::string parentName;
    std::string parentPhone;
    long long balance;      // UZS, can be negative (debt)
    std::vector<int> groupIds;
};

// groups.txt: id|name|teacherId|pricePerLesson|days|studentIds
struct Group {
    int id;
    std::string name;
    int teacherId;          // an Account id
    long long pricePerLesson;
    std::vector<std::string> days;   // e.g. "Mon", "Wed"
    std::vector<int> studentIds;
};

// payments.txt: id|studentId|amount|date
struct Payment {
    int id;
    int studentId;
    long long amount;
    std::string date;       // YYYY-MM-DD
};

// lessons.txt: id|groupId|date|presentIds|absentIds
struct Lesson {
    int id;
    int groupId;
    std::string date;       // YYYY-MM-DD
    std::vector<int> presentIds;
    std::vector<int> absentIds;
};

// messages.txt: id|toId|type|studentId|text|date|read
struct Message {
    int id;
    int toId;               // an Account id (the inbox owner)
    std::string type;       // "low_balance", "debt", "absence_streak", "low_attendance"
    int studentId;
    std::string text;       // must not contain the | character
    std::string date;
    bool read;
};

// Everything the program keeps in memory. Loaded once at startup.
struct Database {
    std::vector<Account> accounts;
    std::vector<Student> students;
    std::vector<Group> groups;
    std::vector<Payment> payments;
    std::vector<Lesson> lessons;
    std::vector<Message> messages;
};

#endif
