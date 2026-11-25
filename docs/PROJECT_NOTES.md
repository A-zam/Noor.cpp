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
| 2 | accounts + role-based menu in main.cpp | todo |
| 3 | students and attendance | todo |
| 4 | alerts, daily list, monthly report | todo |
| 5 | test.cpp with plain asserts | todo |

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

## 4. File map (so far)
```
src/structs.h     all structs + Database
src/storage.h     public storage functions
src/storage.cpp   load, save, split helpers, nextId
src/main.cpp      TEMPORARY: just prints record counts (replaced in stage 2)
data/*.txt        sample data (6 files)
reports/          CSV output (stage 4)
docs/             this file + screenshots
```

*Update this file after every stage: tick the status table, add a "Stage N explained" section, add new exam questions.*
