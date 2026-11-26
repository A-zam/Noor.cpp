# LearNoor - Project Notes (living document)

Console CRM for an education center, used only by staff. C++17, standard library only.
Build: `g++ -std=c++17 src/*.cpp -o learnoor` (run from the project root, because the program reads `data/`).

---

## 1. The plan (original brief)

**Rules:** plain structs, vectors, strings, functions. No templates, no smart pointers, no tricks. One `.h/.cpp` pair per module, plus `main.cpp` and `structs.h`.

**Key design rule:** ALL logic lives in functions that take values and return results. Only `main.cpp` uses `cin`/`cout`. Only `storage.cpp` touches files. (Reason: a web layer can call the same functions later without a rewrite. Do not build web parts until asked.)

**Roles**
| Role | Can do | Alerts |
|---|---|---|
| director | everything, creates accounts | debt alerts |
| manager | students, groups, payments, attendance, reports | balance and attendance alerts |
| teacher | marks attendance for OWN groups only | attendance alerts for own groups |

**Data files** (one record per line, fields split by `|`, list fields use commas)
- accounts.txt: `id|username|passwordHash|fullName|role`
- students.txt: `id|fullName|phone|parentName|parentPhone|balance|groupIds`
- groups.txt: `id|name|teacherId|pricePerLesson|days|studentIds`
- payments.txt: `id|studentId|amount|date`
- lessons.txt: `id|groupId|date|presentIds|absentIds`
- messages.txt: `id|toId|type|studentId|text|date|read`

Money = `long long` UZS. Dates = `YYYY-MM-DD`. New id = max id + 1. Load everything at startup, rewrite the file after each change.

**Alerts** (checked after every payment and attendance entry, written to inboxes; skip if that person already has an UNREAD alert of the same type about the same student)
- Low balance: balance / lowest group price < 2 lessons -> managers
- Debt: balance < 0 -> managers; to directors each time the debtor count passes a multiple of 20
- Absence streak: absent from the group's last 2 lessons -> group's teacher + managers
- Low attendance: < 70% once the group has 4+ lessons -> group's teacher + managers

**Other features:** login with simple hash; students/groups (add, list, enroll, search); payments add to balance; attendance per group for today; daily action list (today's groups, students to call lowest balance first using a hand-written selection sort, attendance warnings); monthly report CSV `reports/monthly_report_YYYY-MM.csv` (revenue, total debt, attendance per group).

**Input handling:** bad input prints an error and returns to the menu. Never crash.

### Decisions we made together
- **Balance deduction:** when a lesson (attendance entry) is saved, EVERY enrolled student is charged the group's price, present or absent.
- **Debt alert to directors:** fires when the debtor count passes a multiple of 20 (20, 40, 60...).
- **"Today":** `main.cpp` reads the system date once and passes it as a string into the logic functions (so tests can use fixed dates).
- **Password hash:** `h = 5381; for each char: h = (h*33 + char) % 1000000007`.
- **Demo logins:** `admin/admin123` (director), `manager/manager123` (manager), `teacher/teacher123` (teacher).

---

## 2. Stages and status

| # | Stage | Status |
|---|---|---|
| 1 | structs.h, storage.h/.cpp, sample data | DONE |
| 2 | accounts + role-based menu in main.cpp | DONE |
| 3 | students, groups, payments and attendance | DONE (see CODE_WALKTHROUGH.md parts 6-8) |
| 4 | alerts, daily list, monthly report | DONE (see CODE_WALKTHROUGH.md parts 9-13) |
| 5 | tests/test.cpp with plain asserts (24 tests) | DONE (see CODE_WALKTHROUGH.md part 15) |

Stage 5 tests: balance deduction, each alert at its threshold, no duplicate alerts, report totals, save-then-load returns the same data.

---

## 3. Stage 1 explained (for the presentation)

### The big picture
The program is like a notebook. The six `.txt` files are the notebook pages on disk. When the program starts it **reads every page into memory** (vectors inside one `Database` object). While running it only changes the vectors. After each change it **writes the page back** to disk. That is the whole storage idea.

```
data/*.txt  --loadAll-->  Database (vectors in memory)  --saveX-->  data/*.txt
```

### structs.h - "what does a record look like?"
A `struct` is a box that groups related variables. `struct Student` holds a student's id, name, phone, balance and so on. Each struct matches ONE file and each field matches ONE column in that file.

Things worth knowing:
- Lists are `std::vector`. A student's `groupIds` is a vector of ints, because a student can be in several groups. A group's `days` is a vector of strings like `"Mon"`.
- `balance` is `long long` because UZS amounts get large (an `int` maxes out around 2.1 billion). It can be negative: negative means the student owes money (debt).
- `Message.read` is a `bool`. In the file it is stored as `1` or `0`.
- `Database` is just six vectors in one struct, so we can pass ONE thing around instead of six.
- `#ifndef STRUCTS_H ... #endif` is an include guard: it stops the file being pasted in twice by accident.

### How a line becomes a struct (the core of storage.cpp)
Take this line from students.txt:
```
2|Dilnoza Tosheva|901110003|Nodira Tosheva|901110004|150000|1
```
1. `readLines` reads the file and returns the lines as a vector of strings (it skips blank lines and removes the Windows `\r`).
2. `split(line, '|')` walks through the characters one by one, building up `current`. At each `|` it pushes `current` into the result and starts a new one. At the end it pushes the last piece. Result: 7 strings.
3. The loader checks `f.size() == 7`. If not, the line is broken and is skipped (the program never crashes on bad files).
4. It fills a `Student`: text fields are copied directly; numbers go through `toNumber`, which wraps `std::stoll` in try/catch so bad text becomes 0 instead of crashing.
5. List fields like `1,2,3` go through `toIntList` (split by comma, convert each piece). An empty field means an empty list.
6. `push_back` adds the finished struct to the vector.

Saving does the reverse: loop over the vector, write each field with `|` between, `fromIntList` turns `{1,2}` back into `1,2`, and end the line with `\n`. `std::ofstream` opens the file for writing and **overwrites** the old content, which is exactly "rewrite the file after each change".

**Why split keeps empty fields:** `1|1|2026-11-16|1,2,3,4|` ends with `|` so the absent list is empty. `split` pushes the last piece even when it is empty, so we still get 5 fields. If it dropped empty pieces, a lesson with nobody absent would look broken.

### Other functions
- `nextId(list)`: loops to find the biggest id, returns +1. It is written once per struct (function overloading: same name, different parameter type, the compiler picks the right one). Using max+1 instead of `size()+1` means ids stay unique even if records get deleted.
- `setDataFolder`: the folder is a `static` variable inside storage.cpp (`static` = private to that file). Tests will point it at a test folder so they never damage real data.
- `saveAll`: calls all six savers and returns false if any failed.
- The functions marked `static` (`split`, `toNumber`, `readLines`...) are helpers that only storage.cpp can see. That keeps the module's public face small: only what is in `storage.h`.

### The sample data and what it demos
- 3 accounts (director/manager/teacher), 2 groups, 8 students, 6 payments, 6 lessons.
- **Absence streak:** Dilnoza (student 2) was absent from English's last two lessons (ids 3 and 4).
- **Low balance:** Dilnoza has 150,000 and English costs 100,000/lesson, so she covers 1 lesson (< 2).
- **Debt:** Nigora (student 8) has -50,000.
- English attendance is 13/16 = 81%, so low-attendance does NOT trigger (tests will use changed data for it).
- Balances in the sample file are set directly; they are not recomputed from payments.

### Likely questions from the examiner (and answers)
- **Why one file per type, not a database?** Course scope: only the standard library. Text files are readable and easy to debug.
- **What if the file is missing or damaged?** Missing = empty list. A broken line is skipped. The program does not crash.
- **Why `long long`?** Money in UZS exceeds the `int` range easily.
- **Why does only storage.cpp use files?** One place to change if storage ever changes (for example to a database), and the web layer can reuse everything else.
- **Why is the password stored as a hash?** So the file never contains the real password. Our hash is simple (a teaching hash, not production security) and we say so honestly.
- **Why max+1 for ids?** Unique even after deletions.

---

## 4. Stage 2 explained (for the presentation)

### What was added
- `accounts.h/.cpp`: password hash, login, creating accounts, and the inbox helpers (count unread, list, mark read).
- `main.cpp` (real version): login, three role menus, input helpers.
- `storage.cpp` got one small function, `isSafeText`.

### The flow of the program
```
start -> loadAll -> show today's date -> login (3 tries) -> role menu loop -> logout -> end
```
1. `main` loads all data, then asks for username and password.
2. `login(db, username, password)` hashes the typed password and compares it with every account's stored hash. It returns the account's position in the vector, or -1 if nothing matches. It never prints anything: main decides what to show.
3. On success main copies the account (`Account me = db.accounts[index]`), greets the user and prints the unread count (`countUnread`).
4. Depending on `me.role`, main calls `directorMenu`, `managerMenu` or `teacherMenu`. Each is a `while (true)` loop that prints ONLY that role's options, reads a number and runs the matching action. Option 0 does `return`, which ends the loop and logs out.

### The password hash (hashPassword)
```
h = 5381
for each character c in the password:
    h = (h * 33 + c) % 1000000007
```
It turns the password into one number. The same password always gives the same number, so we can compare numbers without storing the real password. `% 1000000007` keeps the number from growing too big for `long long`. This is the "djb2" teaching hash: fine for a course project, NOT secure for real systems (real systems use bcrypt or similar with a random salt). Say this honestly in the defense.

### Input handling (the "never crash" rule)
- `readLine` reads a WHOLE line with `getline`. We never use `cin >> number`, because typing letters there breaks `cin` and can loop forever.
- `parseInt` checks character by character that the text is only digits (with an optional leading minus), so "abc", "" or "12x" are rejected before any conversion. It also rejects numbers longer than 9 digits so `stoi` cannot overflow.
- `readChoice` uses both. If it returns -1 the menu prints an error and shows the menu again. The menu code treats -1 as "error already shown, just loop".
- If the input stream ends (Ctrl+Z), `readLine` exits cleanly instead of looping forever.

### Why `Account me` is passed BY COPY to the menus
`createAccount` does `push_back` on `db.accounts`. When a vector grows it may move to new memory, and any reference or pointer to its old elements would then point at garbage. So the menus get a copy of the logged-in account that stays valid.

### createAccount (validation = a list of early returns)
Each rule is an `if (...) { error = "..."; return false; }`: empty username, spaces, `|` character, empty name, password shorter than 4, bad role, duplicate username. Only if every check passes does it build the Account (id = `nextId`), add it and save. If saving fails it does `pop_back()` so memory and file never disagree. The error text goes out through the `error` parameter (a reference), which is how a function returns TWO things (success/fail and a reason) without printing.

### isSafeText (why it exists)
Our file format uses `|` as the field separator. If someone typed `A|B` as a name, the line would split into the wrong number of fields and the record would be skipped on the next load. So every module that saves typed text calls `isSafeText` first. It lives in storage because it protects the file format.

### Why logic functions don't print
`login`, `createAccount`, `countUnread` return values only. Only main prints. A later web page can call the same functions and show the result as HTML.

### The date
`getToday()` in main reads the clock ONCE (`time`, `localtime`, `strftime` format `%Y-%m-%d`) and the string is passed down. Tests can pass fixed dates like "2026-11-25" instead.

### What the menus show (role-based)
| Role | Options |
|---|---|
| director | create account, list accounts, students, groups, payment, attendance, daily list, monthly report, inbox |
| manager | students, groups, payment, attendance, daily list, monthly report, inbox |
| teacher | mark attendance (own groups), inbox |

Options for stages 3 and 4 print "not built yet" for now. Viewing the inbox marks everything read, which matters for the "skip duplicate unread alert" rule in stage 4.

### Questions the examiner may ask
- **Why hash passwords?** So accounts.txt never holds the real password. Our hash is weak on purpose (simple); real systems use salted bcrypt.
- **Why getline and not cin >>?** `cin >>` breaks on letters when it expects a number. getline always succeeds, and we validate the text ourselves.
- **How does a teacher not see the director menu?** The menu function is chosen by `me.role`, and each menu only prints its own options.
- **What stops a wrong account being created?** `createAccount` validation list; nothing is saved unless all checks pass.
- **Why return -1 from login?** Array positions are 0 or more, so -1 can never be a real position: it safely means "not found".

---

## 5. File map
```
src/structs.h        all structs + Database
src/storage.*        load, save, split helpers, nextId, isSafeText, report file writer
src/accounts.*       password hash, login, createAccount, inbox helpers
src/students.*       students, groups, enroll, search, payments, formatMoney, isValidDate
src/attendance.*     markAttendance (charges), streak and percentage rules
src/alerts.*         the four alert rules, checkAlerts
src/daily.*          weekday, selection sort, daily action list
src/reports.*        monthly report numbers and CSV
src/table.*          formatTable (aligned text tables)
src/main.cpp         input helpers, screens, role menus, main() (only file with cin/cout)
tests/test.cpp       plain-assert tests;  tests/run_tests.bat builds and runs them
data/*.txt           sample data (6 files)
reports/             CSV output
docs/                notes + screenshots
```

## 6. Who explains what (defense)
Each of us explains our own files and the screens in main.cpp that call them, plus the tests for them. Everyone should still be able to follow the whole program (read CODE_WALKTHROUGH.md).

| Person | Files | Screens / functions in main.cpp | Tests |
|---|---|---|---|
| A'zam | structs.h, storage.*, alerts.* | input helpers (`readLine`, `parseInt`, `parseLong`, `getToday`, `askInt`, `readChoice`), the 3 role menus, `main()` | storage (3), alerts (5) |
| Azizbek | students.*, attendance.* | students and groups menus and screens, payment screen, attendance screen, `studentsTable`, `groupsTable`, `printNewAlerts` | students (5), attendance (2) |
| Quvonchbek | accounts.*, daily.*, reports.*, table.* | inbox, accounts list, create account, daily list, report screens | accounts (3), daily (3), reports (2), table (1) |

Study guides: `PART_Azam.md`, `PART_Azizbek.md`, `PART_Quvonchbek.md`. The full line-by-line explanation of everything is `CODE_WALKTHROUGH.md`.

## 7. Run and test
```
g++ -std=c++17 src/*.cpp -o learnoor        (program, run from the project root)
.\learnoor.exe
tests\run_tests.bat                          (all tests)
```
Logins: admin/admin123 (director), manager/manager123, teacher/teacher123. The program edits data/*.txt; `git restore data/` brings the sample data back.

*Update this file after every stage: tick the status table, add a "Stage N explained" section, add new exam questions.*
