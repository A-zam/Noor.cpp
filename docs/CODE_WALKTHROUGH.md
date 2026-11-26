# LearNoor - Code Walkthrough (read this before the defense)

This file explains the code itself, piece by piece, in plain words.
`PROJECT_NOTES.md` explains the plan and the ideas. This file explains the lines.
We add a section here after every stage.

How to use it: open the real file next to this one and follow along.
For each function ask yourself: "What goes in? What comes out? What does it change?"

---

## Part 0 - The C++ features we use (learn these first)

Every examiner question about "why did you write it like this" comes back to these.

### struct
A struct groups related variables into one box.
```cpp
struct Payment { int id; int studentId; long long amount; std::string date; };
Payment p;          // make one box
p.amount = 50000;   // dot = open the box and reach a field
```

### std::vector
A list that grows. `v.push_back(x)` adds to the end, `v.size()` is the count, `v[i]` is item number i (counting from 0), `v.clear()` empties it, `v.pop_back()` removes the last one.
```cpp
std::vector<int> ids;  ids.push_back(7);  ids.push_back(9);   // {7, 9}
```

### The usual loop over a vector
```cpp
for (size_t i = 0; i < list.size(); i++) { ... list[i] ... }
```
`size_t` is the number type that `.size()` returns (never negative). Start at 0, stop before `size()`, because the last valid position is `size() - 1`.

### Functions, parameters, and "by value" vs "by reference"
- By value: `void f(int x)` gets a COPY. Changing x inside does not change the caller's variable.
- By reference: `void f(int& x)` has `&`. It works on the caller's ORIGINAL variable. We use this to change data, or to send an extra result back.
- `const std::string& text` = "by reference but read-only". It avoids copying a big string and promises not to change it. We use it for most inputs.
- `Database& db` = we will change the database. `const Database& db` = we only read it. You can see at a glance whether a function can modify data.

### Returning two things
A function returns only one value, so `createAccount` returns `bool` (worked or not) and puts the reason into `std::string& error`, a reference parameter the caller reads afterwards.

### static
A `static` function or variable at file level is PRIVATE to that .cpp file. Helpers like `split` are static in storage.cpp so no other file can call them. Only what is declared in the .h file is public.

### The .h / .cpp pair and #ifndef
The .h file lists what a module offers (declarations). The .cpp file contains the code (definitions). Other files do `#include "storage.h"` to use it. `#ifndef X / #define X / #endif` (include guard) stops the same header being pasted twice.

### Overloading
Several functions with the same name but different parameter types (`nextId` for each struct). The compiler picks the right one from the argument you pass.

### try / catch
```cpp
try { return std::stoll(text); } catch (...) { return 0; }
```
`stoll` throws an error if the text is not a number. `try` runs it, `catch (...)` catches ANY error and we return 0 instead of crashing.

### getline vs cin >>
`std::getline(std::cin, line)` reads a whole line as text. It always works. `cin >> number` breaks if the user types letters. So we always read text, then check it ourselves.

---

## Part 1 - structs.h

Plain data only: no functions, no logic. Six structs, one per file, plus `Database`.

| Struct | Meaning | Field worth noting |
|---|---|---|
| Account | a staff member | `passwordHash` is a number, never the real password |
| Student | a student | `balance` can be negative (debt). `groupIds` is a vector because a student can be in several groups |
| Group | a class | `teacherId` is an Account id. `studentIds` lists the members. `days` is e.g. {"Mon","Wed"} |
| Payment | one payment | `amount` is `long long` |
| Lesson | one held lesson | `presentIds` and `absentIds` split the group for that day |
| Message | one inbox alert | `toId` = who receives it, `read` = seen or not, `type` = which alert |
| Database | everything in memory | six vectors, so we pass ONE object around |

Records point to each other by id, not by copying. A Group stores `studentIds` (numbers), not whole Student objects. To get the student we look the id up in `db.students`. This is the same idea as a foreign key in a database, and it means a student's name is stored in exactly one place.

---

## Part 2 - storage.cpp

Goal: turn text lines in `data/*.txt` into structs (load) and back (save).

### Top of the file
```cpp
static std::string dataFolder = "data/";
void setDataFolder(const std::string& folder) { dataFolder = folder; }
```
The folder path is a private variable. `setDataFolder` changes it (tests use it to avoid touching real data).

### isSafeText
```cpp
bool isSafeText(const std::string& text) {
    for each character: if it is '|' or '\n' or '\r' -> return false
    return true;
}
```
Loops through every character. One bad one means false straight away. Reaching the end means true.

### split(line, sep)
```cpp
std::vector<std::string> parts;
std::string current = "";
for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == sep) { parts.push_back(current); current = ""; }
    else                { current += line[i]; }
}
parts.push_back(current);      // the last piece has no separator after it
return parts;
```
Trace `"1|Ali|5"` with `|`:
- `1` goes into current. Reach `|`: push "1", reset.
- `Ali` is built. Reach `|`: push "Ali", reset.
- `5` is built. Loop ends. The line after the loop pushes "5".
Result: {"1","Ali","5"}.

Why the last `push_back` is outside the loop: the final piece has no separator after it, so the loop never pushes it. It also means a line ending in `|` gives an EMPTY last piece, which is what we want (empty list field).

### toNumber
Wraps `std::stoll` (string to long long) in try/catch. Bad text becomes 0. This is why a damaged file does not crash the program.

### toIntList / fromIntList / fromStringList
- `toIntList("1,2,3")`: if the text is empty return an empty vector; otherwise `split` by comma and convert each piece with `toNumber`.
- `fromIntList({1,2,3})`: build the text, adding a comma BEFORE every item except the first (`if (i > 0) text += ","`). That avoids a trailing comma.
- `fromStringList` is the same for string lists like the days.

### readLines(name)
Opens `dataFolder + name` with `std::ifstream`. If it will not open, returns an empty list (a missing file is fine, it means "no records yet"). Otherwise reads line by line with `getline`. If a line ends with `\r` (a Windows line ending) it is removed, and empty lines are skipped.

### The six loaders (loadAccounts, loadStudents, ...)
All have the same shape:
```cpp
lines = readLines("students.txt");
list.clear();                              // start from empty
for each line:
    f = split(line, '|');
    if (f.size() != 7) continue;           // wrong field count: skip the broken line
    Student s;
    s.id = (int)toNumber(f[0]);            // f[0] is the 1st column ... f[6] the 7th
    ...
    s.groupIds = toIntList(f[6]);
    list.push_back(s);
```
- `continue` jumps straight to the next line of the loop.
- `(int)` converts the `long long` from `toNumber` to an `int`.
- For messages: `m.read = (f[6] == "1")`. The text "1" becomes `true`, anything else `false`.

`loadAll(db)` calls all six. It is the only thing main needs at startup.

### The six savers (saveAccounts, ...)
```cpp
std::ofstream file(dataFolder + "students.txt");   // opens AND empties the file
if (!file.is_open()) return false;
for each student: file << s.id << "|" << s.fullName << "|" ... << "\n";
return true;
```
`ofstream` opening a file erases the old content, so "rewrite the whole file" is automatic. Returning false lets the caller know the disk write failed.
`(m.read ? 1 : 0)` is the reverse of the loader: true writes `1`, false writes `0`. `a ? b : c` means "if a then b else c".

`saveAll` calls all six and remembers if any failed (`ok = false`).

### nextId
```cpp
int biggest = 0;
for each item: if (item.id > biggest) biggest = item.id;
return biggest + 1;
```
Classic "find the maximum". An empty list returns 1. It is written six times, one per struct (overloading).

---

## Part 3 - accounts.cpp

### hashPassword
```cpp
long long h = 5381;
for (each character) h = (h * 33 + (unsigned char)password[i]) % 1000000007LL;
return h;
```
- `h * 33 + character` mixes each letter in. A different password gives a very different number.
- `% 1000000007` is the remainder after dividing, so the number never overflows.
- `(unsigned char)` makes the character a positive number 0-255.
- `LL` after the number means "this literal is a long long".
Try by hand with the password "A" (code 65): 5381*33 + 65 = 177638.

### login
```cpp
long long hash = hashPassword(password);
for each account i:
    if username matches AND stored hash == hash: return i;
return -1;
```
Returns the POSITION in the vector (or -1). We compare hashes, never real passwords. Both the username and the password must match, and the error message does not tell which one was wrong (so an attacker cannot learn valid usernames).

### findAccountIndex
Same idea, searching by `id`. Returns the position or -1. Later modules use it to turn a `teacherId` into a name.

### isValidRole
`return role == "director" || role == "manager" || role == "teacher";` `||` means OR.

### createAccount
Read it as a checklist. Each check follows the same pattern:
```cpp
if (username == "") { error = "Username cannot be empty."; return false; }
```
Set the reason, leave the function with false. `find(' ') != std::string::npos` means "a space WAS found" (`npos` is the "not found" value).
After all checks pass:
```cpp
Account a;  a.id = nextId(db.accounts);  a.passwordHash = hashPassword(password); ...
db.accounts.push_back(a);
if (!saveAccounts(db.accounts)) { db.accounts.pop_back(); error = "..."; return false; }
return true;
```
If the file cannot be saved, `pop_back` removes the new account from memory so memory and file stay the same.

### Inbox helpers
- `countUnread`: loop over messages, count the ones with `toId == accountId` and `!read`.
- `getInbox`: loop and copy the account's messages into a new vector.
- `markInboxRead`: loop, set `read = true` for this account's messages, then `saveMessages`. It works on `Database& db` because it changes data.

---

## Part 4 - main.cpp

Only this file prints and reads from the keyboard.

### readLine(question)
Prints the question, then `getline`. `if (!std::getline(...))` is true when the input has ended (Ctrl+Z). Then it prints a goodbye and `std::exit(0)` stops the program. Without this, a menu loop would spin forever on a dead input.

### parseInt(text, result)
Returns true or false; the number comes out through the reference `result`.
```cpp
if (text == "" || text.size() > 9) return false;   // empty or too long for an int
size_t start = 0;  if (text[0] == '-') start = 1;  // allow a leading minus
if (start == text.size()) return false;            // text was only "-"
for (i = start; ...) if (text[i] < '0' || text[i] > '9') return false;   // not a digit
result = std::stoi(text);
return true;
```
Characters are compared as numbers: digits '0' to '9' are consecutive, so `c < '0' || c > '9'` means "not a digit". Only after every check passes is `stoi` called, so it can never throw.

### getToday
```cpp
std::time_t now = std::time(nullptr);          // seconds since 1970
std::tm* t = std::localtime(&now);             // break into year, month, day...
char buffer[11];                               // "YYYY-MM-DD" is 10 chars + 1 end marker
std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", t);   // print it in our format
return std::string(buffer);
```
`%Y-%m-%d` is the format pattern (4-digit year, 2-digit month, 2-digit day). `*` and `&` here are pointer syntax from the C time library; you do not need to go deeper than "it fills the buffer with the date text".

### notBuilt
Prints a placeholder for features coming in later stages.

### showInbox / listAccounts / createAccountScreen
These are the "screen" functions: ask, call logic, print the result. `createAccountScreen` reads four answers, calls `createAccount`, and prints "Account created." or the error text it received.

### readChoice
Reads a line, runs `parseInt`. If that fails prints an error and returns -1. The menus treat -1 as "error already shown, just show the menu again".

### The three menu functions
```cpp
while (true) {
    print the role's options
    int choice = readChoice();
    if (choice == 0) return;                // logout: leave the function
    else if (choice == 1) ...
    else if (choice != -1) print "no such menu option";
}
```
`while (true)` loops until `return`. The last `else if` catches numbers that are valid but not on the menu; -1 is skipped because its error was already printed. `(void)today;` tells the compiler "I know this parameter is unused for now" (it will be used from stage 3).
Each role has its own function, which is HOW the role-based menu works: a teacher physically cannot reach the director's options because those options are not in the teacher's function.

### main()
1. `loadAll(db)`, and stop with an error if there are no accounts.
2. `getToday()`.
3. Login loop: `for (attempt = 1; attempt <= 3 && index == -1; attempt++)` tries up to 3 times and stops early on success.
4. `Account me = db.accounts[index]` copies the logged-in account.
5. Print welcome and the unread count.
6. Choose the menu with `if / else if` on `me.role`.

---

## Part 5 - Trace a whole run (practice explaining this out loud)

User types `manager`, `manager123`, then `7`:
1. `main` calls `loadAll`: the six files become vectors.
2. `readLine` x2 gives "manager" and "manager123". `login` hashes the password to 873392302, finds account 2 with the same username and hash, returns position 1.
3. `me` = Bobur Yusupov, role "manager". `countUnread` = 0, so no unread message is printed.
4. `managerMenu` prints its 7 options. `readChoice` reads "7", `parseInt` gives 7, and the menu calls `showInbox`.
5. `showInbox` calls `getInbox`; if it is empty it prints "Your inbox is empty." The loop goes round and shows the menu again.
6. User types `0`: `return`, back in `main`, "Logged out. Goodbye."

---

## Part 6 - Stage 3: students.cpp

### Pattern used by EVERY "change data" function (learn this once)
```
1. look up the records involved   (find...Index; -1 means not found -> error)
2. validate the input             (each failure: error = "..."; return false/0)
3. change the data in memory      (push_back / += / erase)
4. save the files                 (if saving fails: UNDO the memory change, error, return false)
5. return success
```
Step 4's undo (`pop_back`, `balance -= amount`) keeps memory and files identical. Functions that CREATE something return the new id, or 0 for failure (ids start at 1, so 0 can never be real). Functions that only change things return true/false. In both cases the reason goes out through `std::string& error`.

### formatMoney (1234567 -> "1 234 567")
```cpp
bool negative = (amount < 0);  if (negative) amount = -amount;   // remember the sign, work with a positive
std::string digits = std::to_string(amount);                     // number -> text "1234567"
for (int i = digits.size() - 1; i >= 0; i--) {                   // walk from the RIGHT end
    result = digits[i] + result;                                 // put the digit at the front of the result
    count++;
    if (count % 3 == 0 && i > 0) result = " " + result;          // after every 3 digits add a space
}
if (negative) result = "-" + result;
```
`count % 3 == 0` means "count is a multiple of 3". `&& i > 0` stops a space appearing before the very first digit. Walking from the right is what makes the groups of three line up correctly.

### isValidDate
Checks the SHAPE first (length 10, `-` at positions 4 and 7, digits everywhere else), then converts with `substr` + `stoi` (safe, because we already know they are digits). Then checks real calendar rules: month 1-12, day within that month's length. February gets 29 days in a leap year:
`leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)`. (2000 was a leap year, 1900 was not.)
`daysInMonth[month - 1]`: arrays count from 0, so January is `[0]`.

### findStudentIndex / findGroupIndex
Loop and return the position of the matching id, else -1. Same shape as `findAccountIndex`.

### isValidPhone
Optional `+` at the start (`start = 1` skips it), then the rest must be 7 to 15 digits. `phone.size() - start` is the number of digits.

### toLower, contains, removeValue (private helpers, `static`)
- `toLower`: loop over characters, if it is `A`-`Z` add `('a' - 'A')` (= 32) to make it lowercase. Characters are numbers, so this arithmetic works.
- `contains(list, value)`: loop and return true if found.
- `removeValue(list, value)`: find it, `list.erase(list.begin() + i)` removes the item at position i. `begin() + i` means "the i-th position".

### addStudent
Checks name, parent name, safe text, both phones. Builds the Student with `balance = 0`, `push_back`, saves. Returns the new id.

### searchStudentsByName
Lowercases both the search text and each name, then `name.find(text) != std::string::npos` means "the text appears somewhere inside the name". So "dil" finds "Dilnoza". Returns the POSITIONS of matches so main can print them.

### parseDays (used by addGroup)
Walks over the text; at every comma (and at the very end, `i == text.size()`) the collected day is checked with `isValidDay` and checked for repeats, then added. Spaces are skipped (`else if (text[i] != ' ')`). Note the loop runs to `i <= text.size()` on purpose so the final day (which has no comma after it) is processed too.

### addGroup
Checks name, price range (1 to 100 000 000), that the teacher id belongs to an account with role "teacher" (`findAccountIndex` from accounts.h), and the days. Then builds and saves like addStudent.

### enrollStudent / removeStudentFromGroup
Both records must agree: the student's `groupIds` AND the group's `studentIds`. Enrolling does `push_back` on both; removing uses `removeValue` on both. If saving either file fails, both changes are undone. Removing keeps old lessons (history) but the student is no longer in `group.studentIds`, so future lessons do not charge them: this is the rule "a lesson is paid until the student leaves the group".

### recordPayment
Validates student exists, amount > 0, amount not absurdly large (`10000000000LL`, the `LL` marks a long long literal), date valid. Then creates a Payment and does `balance += amount`. Undo on save failure: `pop_back` and `balance -= amount`.

---

## Part 7 - Stage 3: attendance.cpp

### visibleGroupIndexes(db, who)
```cpp
if (who.role != "teacher" || db.groups[i].teacherId == who.id) result.push_back(i);
```
Read it as: "include this group if the person is NOT a teacher (manager/director see all) OR the group's teacher is this person". This ONE line is what implements "teachers only see their own groups".

### hasLesson / lessonsOfGroup
Plain loops. `hasLesson` answers "was this group already marked on this date?" so a lesson cannot be saved (and charged) twice in a day. `lessonsOfGroup` copies a group's lessons in saved order; stage 4's alerts need it.

### markAttendance - the core of the money logic
Checks in this order: group exists; if the user is a teacher the group must be theirs (a SECOND safety check, in the logic itself, so even a buggy menu cannot let a teacher into another teacher's group); date valid; group has students; not already marked; every id in `presentIds` really belongs to the group.
Then:
```cpp
for each studentId in group.studentIds:
    if (contains(presentIds, studentId)) lesson.presentIds.push_back(studentId);
    else                                  lesson.absentIds.push_back(studentId);
```
So main only tells us who is PRESENT; everyone else in the group is automatically ABSENT (nobody is forgotten). Then every student in the group gets `balance -= group.pricePerLesson`, present or absent, because the rule is "the lesson is paid no matter the attendance". Balances may go below zero: that is debt. Save lessons + students; undo everything if saving fails.
`const Group& group = db.groups[gi];` is a read-only shortcut name for that group. It stays valid because we only `push_back` into `db.lessons`, not `db.groups`.

---

## Part 8 - Stage 3: what was added in main.cpp

- `parseLong`: like `parseInt` but for money (`stoll`, up to 12 digits). It still ACCEPTS a leading `-` on purpose: the "must be more than 0" error then comes from the logic function, so a negative amount gives a clear message.
- `askInt(question, value)`: asks, parses, prints the error if it is not a number, and returns true/false. Screens use it as `if (!askInt("Student id: ", id)) return;` which means "bad input: error shown, go back to the menu".
- `printStudent`, `printGroup`: only printing. They turn ids into names with `findGroupIndex` / `findAccountIndex`.
- One `...Screen` function per action: ask -> call logic -> print result or error. They contain NO rules.
- `studentsMenu`, `groupsMenu`: small loops like the role menus; `0` goes back.
- `attendanceScreen`: lists the groups the user may see (`visibleGroupIndexes`), asks for a group id, refuses ids not in that list, refuses if already marked today, then loops over the students asking `p` or `a`. Here a wrong answer repeats the question for the SAME student (the `while (true)` with `break`), because losing the whole attendance entry over one typo would be annoying. When done it calls `markAttendance` with the list of present ids.
- The menus now call these screens. The director/manager/teacher menus changed by a few lines only: that is the benefit of keeping logic out of the menu code.
- Two comments `// stage 4: the alert check ... will be called here` mark exactly where `checkAlerts` goes after a payment and after attendance.

### Questions the examiner may ask (stage 3)
- **Where is "lesson paid no matter the attendance" in the code?** In `markAttendance`: the charge loop runs over ALL students of the group, not just the present ones.
- **How do you stop a teacher marking someone else's group?** Twice: `visibleGroupIndexes` only lists her groups and the menu refuses other ids; and `markAttendance` checks `group.teacherId != by.id` again.
- **Why check the date format?** A wrong date would be saved into the file and later break the monthly report and the "last 2 lessons" rule.
- **What if the disk write fails?** Every change is undone in memory so the program and the files never disagree.
- **Why does enroll update two records?** Student and group both store the link, so both must change together; if one failed we undo both.
- **Why can balance be negative?** A negative balance IS the debt: lessons are charged even when the student has not paid.

---

## Part 9 - Stage 4: the attendance rules (added to attendance.cpp)

The three numbers from the brief are written ONCE at the top as constants:
```cpp
static const int STREAK_LENGTH = 2;
static const int MIN_LESSONS_FOR_PERCENT = 4;
static const int LOW_ATTENDANCE_PERCENT = 70;
```
`const` = cannot change. A named constant instead of a bare `70` in the code means: one place to change, and the reader knows what the number means.

### countMarks(lessons, studentId, present, absent)
Loops over lessons and counts how many times the student is in `presentIds` and in `absentIds`. Two results come back through the reference parameters `present` and `absent`. A student who joined the group later simply has fewer marks (they are in neither list for older lessons).

### hasAbsenceStreak(db, groupId, studentId)
```cpp
lessons = lessonsOfGroup(db, groupId);              // oldest first
if (lessons.size() < 2) return false;               // not enough lessons yet
for (i = lessons.size() - 2; i < lessons.size(); i++)   // only the LAST 2 lessons
    if (!contains(lessons[i].absentIds, studentId)) return false;
return true;
```
It starts the loop at `size - 2`, so only the last two are looked at. If the student was present, or was not in the group yet, in either of them: no streak.

### studentAttendancePercent and hasLowAttendance
Percent = `present * 100 / (present + absent)`. Integer division throws the decimals away (3 of 4 = 75).
The low-attendance TEST avoids division completely:
```cpp
return present * 100 < LOW_ATTENDANCE_PERCENT * (present + absent);
```
This is "present/total < 70/100" with both sides multiplied by 100 x total. Whole numbers only, so exactly 70% is NOT low and there is no rounding problem. It also requires the group to have 4 or more lessons.

---

## Part 10 - Stage 4: alerts.cpp

### lessonsCovered(db, student)
Goes through the student's groups, finds the LOWEST price (`lowestPrice == 0 ||` handles "no price picked yet"), and returns `balance / lowestPrice`. Integer division again: 150 000 / 100 000 = 1. Returns -1 if the student is in no group (no price to divide by).

### isLowBalance
`balance >= 0` and covered is between 0 and 1. A negative balance is NOT "low balance": it is a debt, and has its own alert. This stops a debtor getting two alerts for the same problem.

### Helpers (all `static`, private)
- `accountIdsWithRole(db, "manager")`: all ids with that role. Used to find the recipients.
- `hasUnread(db, toId, type, studentId)`: is there already an UNREAD message with the same receiver, type and student? This is the whole "no duplicates" rule.
- `addMessage(...)`: builds a Message (id = `nextId`, `read = false`) and `push_back`s it. It does not save.
- `sendAlert(db, recipients, type, studentId, text, today)`: for each recipient, send only if `!hasUnread`. Returns how many were really sent. EVERY alert goes through this one function, so the duplicate rule cannot be forgotten.
- `countMessages`: how many messages of a type a person ever received (used for the director rule).

### checkAlerts(db, today) - read it as 4 blocks
It checks EVERYTHING each time (all students, all groups). That is simple and always correct; the duplicate rule stops the repeats.
1. **Per student:** `balance < 0` -> "debt" to all managers. Otherwise `isLowBalance` -> "low_balance" to all managers.
2. **Directors:** `milestones = countDebtors / 20` (integer division: 19 debtors = 0, 20 to 39 = 1, 40 = 2). Each director has already received `alreadySent` milestone messages. The loop `for (step = alreadySent + 1; step <= milestones; step++)` sends the missing ones, so a director gets exactly one message each time the count reaches 20, 40, 60... and never twice for the same milestone. `studentId` is 0 because it is about no single student.
3. **Per group, per student:** recipients = all managers + the group's teacher. `hasAbsenceStreak` -> "absence_streak". `hasLowAttendance` -> "low_attendance". Both use the rules from Part 9, so the daily list and alerts always agree.
4. If any message was created, `saveMessages` once. If saving fails, the new messages are removed again (`pop_back` loop) and 0 is returned.
It returns the number of messages created; main prints it.

### Design decisions (be ready to explain)
- Low attendance is checked PER STUDENT (so the message says who to talk to); a group-wide percentage is in the monthly report.
- Where the brief said "the debtor students grow by 20" we use "each time the count reaches a multiple of 20" (agreed earlier).
- Alerts are checked after every payment and every attendance entry, as required: main calls `checkAlerts` right after a successful `recordPayment` / `markAttendance`.

---

## Part 11 - Stage 4: daily.cpp

### weekdayName(date) - which weekday is a date?
Uses Sakamoto's method: a known short formula that gives 0 = Sunday ... 6 = Saturday.
```cpp
if (month < 3) year = year - 1;     // January and February count as part of the previous year
w = (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;
```
`t[]` is a table of small month offsets. `% 7` because weekdays repeat every 7 days. You do not need to derive it, only to say: "we need the weekday to know which groups meet today, and C++ has no easy standard function for it, so we use a known formula". Checked: 2026-11-26 gives Thu, which is correct.

### sortByBalance - the hand-written selection sort (required by the brief)
```cpp
for (i = 0; i + 1 < size; i++) {
    smallest = i;
    for (j = i + 1; j < size; j++)
        if (list[j].balance < list[smallest].balance) smallest = j;
    if (smallest != i) swap list[i] and list[smallest] using a temp variable
}
```
Idea in words: "Find the smallest balance in the unsorted part, swap it to the front of that part, then repeat for the rest." After round 1 the first place is correct, after round 2 the second place, and so on. The swap uses `Student temp = list[i]; list[i] = list[smallest]; list[smallest] = temp;` (no `std::sort`, no `std::swap`). `i + 1 < size` is written that way instead of `i < size - 1` because `size()` is unsigned and `size - 1` would wrap around for an empty list.

### buildDailyList(db, today)
Returns a `DailyList` struct holding three vectors, and prints nothing:
1. groups whose `days` contain today's weekday;
2. students with `balance < 0` or `isLowBalance`, copied into `callList` and sorted lowest first (so debtors come first);
3. a `Warning` for every student with `hasAbsenceStreak` or `hasLowAttendance`.
Returning one struct with all three results is how a function hands back several lists, and a future web page can use it directly.

---

## Part 12 - Stage 4: reports.cpp

### isValidMonth
Length 7, `-` in position 4, digits elsewhere, month 01-12.

### buildMonthlyReport
- **Revenue:** sum of payments where `date.substr(0, 7) == month`. `substr(0, 7)` takes the first 7 characters: "2026-11-03" gives "2026-11".
- **Total debt:** the sum of all negative balances turned positive (`-balance`). It is the debt RIGHT NOW (balances do not remember history), not debt at the end of that month. Say this honestly if asked.
- **Per group:** counts the group's lessons in that month, and adds up the sizes of `presentIds` and `absentIds`. `percent = present * 100 / (present + absent)`, or -1 when nothing was marked (shown as `n/a`, which avoids dividing by zero).

### monthlyReportCsv and csvField
CSV = lines of values separated by commas, which Excel opens as a table. The function builds the whole file as ONE string with `+=` and `\n`. `csvField` wraps a text in quotes if it contains a comma or a quote (and doubles inner quotes), otherwise a group called `Math, advanced` would break the columns.

### saveMonthlyReport
Builds the file name `monthly_report_YYYY-MM.csv` and gives the text to `writeReportFile` in storage.cpp: reports.cpp itself still never touches a file. If the `reports` folder is missing, the user gets a clear error instead of a crash.

---

## Part 13 - Stage 4: what main.cpp got
- `printNewAlerts(count)`: prints "N new alert message(s) were sent" when `checkAlerts` created any. It is called right after a successful payment and a successful attendance entry.
- `dailyScreen`: calls `buildDailyList`, prints the three sections as tables (Part 14), turning ids into names with the `find...Index` functions. It also uses `hasLesson` to show "saved / NOT marked yet" next to each group.
- `reportScreen`: asks for the month, builds the report, prints the totals and a table of the groups, saves the CSV and prints where it went.
- `notBuilt` was deleted: every menu option now does something.
- `buildDailyList` skips a group member whose student record is missing (damaged data) instead of crashing, the same way `checkAlerts` does.

### Questions the examiner may ask (stage 4)
- **How do you stop duplicate alerts?** All alerts go through `sendAlert`, which skips a person who already has an UNREAD message of the same type about the same student. Once they read the inbox, a new alert is allowed again.
- **Why not use std::sort?** The brief asks for a hand-written selection sort; ours is `sortByBalance`.
- **Why does the debtor get no "low balance" alert?** Debt already covers it, and two messages for one problem would be noise.
- **Why is 70% not "low"?** The rule is "under 70%", and our whole-number test `present * 100 < 70 * total` is false at exactly 70%.
- **What happens when a teacher is also needed in an alert?** The teacher id is added to the recipients list for that group only, so teachers never see other groups' alerts.
- **Why does the report not write files itself?** Only `storage.cpp` touches files; reports builds the text and asks storage to write it.

---

## Part 14 - table.cpp and how the screens use it

**Problem it solves:** the first version printed every row as `name | phone ... | balance ...`, repeating every column name in every row. A table writes each column title once and lines the columns up.

`formatTable(headers, rows, align)` returns ONE string (it prints nothing, so it follows the "only main prints" rule):
- `headers`: the column titles. `rows`: a vector of rows, each row a vector of cell texts (`std::vector<std::vector<std::string>>`).
- `align`: one letter per column, `l` left or `r` right. `"rl"` = first column right-aligned, second left-aligned. Missing letters mean left. Money and counts are right-aligned so digits line up.

How it works, in 2 steps:
1. **Widths.** Each column is as wide as its longest text, title included: start `widths[j]` at the title's length, then for every row `if (displayLength(cell) > widths[j]) widths[j] = ...`.
2. **Lines.** `separator(widths)` makes `+----+------+` (each column gets `width + 2` dashes: one space each side). `rowLine(cells, widths, align)` makes `| Ali  | ...` by padding every cell with spaces up to its column width (`padRight` for left, `padLeft` for right alignment), with `" | "` between. The table is: separator, titles, separator, rows, separator.

**displayLength (the UTF-8 detail).** `text.size()` counts BYTES, but a Cyrillic letter uses 2 bytes and takes ONE place on screen, so padding by bytes would misalign the table. A UTF-8 letter's extra bytes all start with the bits `10` (`(c & 0xC0) == 0x80`), so we count only the bytes that do NOT. `&` here is the bitwise AND: `c & 0xC0` keeps the top two bits of the byte.

Usage in main.cpp (same 3 steps every time):
```cpp
std::vector<std::vector<std::string>> rows;                       // 1. collect the rows
for (...) rows.push_back({std::to_string(s.id), s.fullName, formatMoney(s.balance)});
std::cout << formatTable({"ID", "Name", "Balance"}, rows, "rlr"); // 2. one call prints it
```
`{...}` inside `push_back` builds the row vector directly (C++11 braces).
- `studentsTable(db, positions)` / `groupsTable(db, positions)`: take POSITIONS in `db.students` / `db.groups` (so the same function shows all students, a search result or one group's students). Ids become names with `findGroupIndex` / `findAccountIndex`, lists become `"English A1, Math B1"` with a comma added before every item except the first.
- `allPositions(n)`: the list 0, 1 ... n-1, meaning "everything".
- Inbox, accounts, daily list (3 tables), monthly report and the teacher list when adding a group use the same function.

**storage.cpp additions for reports.** `writeReportFile(fileName, content)` opens `reportsFolder + fileName` and writes the text. `setReportsFolder` / `getReportsFolder` change and read the folder (tests point it to `tests/tmp/`), the same idea as `setDataFolder`.

---

## Part 15 - tests/test.cpp (stage 5)

### How to run
From the project root: `tests\run_tests.bat` (or the g++ line inside it: all `src/*.cpp` EXCEPT `main.cpp`, plus `tests/test.cpp`, with `-Isrc` so `#include "structs.h"` is found). A test file needs its own `main()`, and a program can only have one, which is why it is not inside `src/`. Success prints `ALL TESTS PASSED`. A failed check stops the program with `Assertion failed: <the condition>, file tests/test.cpp, line N`.

### What `assert` does
`assert(condition)` (from `<cassert>`) does nothing when the condition is true and aborts the program with a message showing the condition and line when it is false. So a test is just "do something, then assert what must be true". No framework needed.

### How the tests stay safe and independent
- `main` calls `setDataFolder("tests/tmp/")` and `setReportsFolder("tests/tmp/")` first, so saving never touches `data/` or `reports/`. `tests/tmp/` is ignored by git.
- Every test builds its OWN small Database in memory with the helpers below, so it does not depend on the sample files and cannot break another test.
- `baseDb()` gives accounts 1 director, 2 manager, 3 teacher, 4 another teacher. `addGroupTo(db, id, teacherId, price, day)`, `addStudentTo(db, id, balance, groupId)` (links BOTH sides, student and group) and `addLessonTo(db, groupId, date, present, absent)` build the rest. `msgCount(db, toId, type, studentId)` counts matching messages. `sameDatabase(a, b)` compares every field of every record.

### What each test proves (the lists in the brief, plus edge cases)
| Test | Proves |
|---|---|
| storage save then load | all 6 record types survive a round trip, including empty lists, negative balance and read/unread |
| storage bad files | missing file = empty list, broken lines are skipped, `\r` is removed |
| nextId / isSafeText | max+1 (not size+1), `\|` and line breaks are refused |
| accounts login / create / inbox | hash value, wrong password = -1, every createAccount rule, saved to file, read-marking only touches your own messages |
| formatMoney / isValidDate | grouping, negatives, leap years (2000 yes, 2100 no), bad shapes |
| addStudent / search / addGroup / enroll / payment | each validation, ids, both-sided enrolling, balance += amount, nothing changes when refused |
| attendance charges | EVERY student of the group is charged, present or absent; outsiders and ex-members are not; balance may go negative |
| attendance rules | other teacher refused, manager allowed, same day twice refused (no double charge), bad date / unknown id refused |
| low balance and debt | 199 999 vs 200 000 at price 100 000 (1 vs 2 lessons), balance 0 = low but not debt, -1 = debt, cheapest group price is used, no group = no alert, only managers receive these |
| absence streak | last 2 lessons only, alternating absences do not count, one lesson is not a streak, teacher + manager receive it, director and other teachers do not |
| low attendance | exactly 70% is NOT low, 60% is, the rule starts at 4 lessons |
| no duplicates | second check creates nothing, after the manager reads his inbox he gets a new one but the teacher (still unread) does not, other students are independent |
| director milestones | 19 debtors: nothing, 20: one message, not repeated, 40: second, 45: still two |
| weekday / sort / daily list | known dates, selection sort (empty, one, reversed, equal balances), call list order, warnings, damaged data skipped |
| report numbers / CSV | revenue of one month only, debt sum, attendance percent, `n/a`, exact CSV text, quoted names, file written, missing folder gives an error |
| table | exact output, right alignment, titles-only table, Cyrillic width, short rows |

### The test suite itself was tested
We broke the code on purpose in a scratch copy (a threshold off by one, no leap-year rule, charging only present students, removing the teacher check, switching the duplicate rule off, sorting descending...) and every one of 12 changes made a test fail. A test that cannot fail proves nothing; this is how we know ours can.

### Questions the examiner may ask (tests)
- **Why do the tests not use the real data folder?** So running them never damages real data, and so results never depend on what is in the sample files.
- **Why is test.cpp not in src/?** `g++ src/*.cpp` would try to link two `main` functions.
- **What is a boundary test?** Checking the exact edge of a rule (199 999 vs 200 000, 69% vs 70%, 3 vs 4 lessons), because that is where off-by-one mistakes hide.
- **What is not tested?** The menus and screens in main.cpp (they only read input and print); everything with a rule is in the tested modules.
