# A'zam's part - structs, storage, alerts, input/login/menus core

Full line-by-line text for everything is in `CODE_WALKTHROUGH.md` (parts 0-4, 10, 15). This file is what YOU must be able to explain, in the order you should learn it.

## 1. Your scope
| Where | What |
|---|---|
| `src/structs.h` | the 6 record structs + `Database` |
| `src/storage.h/.cpp` | all file reading/writing, helpers, `nextId`, `isSafeText`, report writer |
| `src/alerts.h/.cpp` | the four alert rules, `checkAlerts` |
| `src/main.cpp` (core) | `readLine`, `parseInt`, `parseLong`, `getToday`, `askInt`, `readChoice`, `directorMenu`, `managerMenu`, `teacherMenu`, `main()` |
| `tests/test.cpp` | storage (3 tests) and alerts (5 tests) |

Know but do not own: `hasAbsenceStreak` / `hasLowAttendance` (attendance.cpp, Azizbek) and `login` (accounts.cpp, Quvonchbek). Alerts and main call them.

## 2. The project in 30 seconds (your opening line)
"LearNoor is a console CRM for an education center, used only by staff. Data lives in six text files, one record per line. At startup `loadAll` reads them into vectors inside one `Database`; every change updates the vectors and rewrites the file. Logic functions only take values and return results; only `main.cpp` reads the keyboard and prints, so a web page could reuse the same functions later. Three roles (director, manager, teacher) see different menus."

## 3. structs.h
Six structs, one per file, each field = one column. A `struct` groups variables into one box (`s.balance`). Lists are `std::vector` (a student can be in several groups). Records point to each other by **id**, not by copy (a group stores student ids), so a name exists in exactly one place. `balance` is `long long` (UZS exceeds `int`), negative = debt. `Database` = six vectors so we pass ONE object. `#ifndef/#define/#endif` = include guard (header is not pasted twice).

## 4. storage.cpp
**Idea:** `data/*.txt` -> `loadAll` -> vectors -> `saveX` -> `data/*.txt`.

- `split(line, sep)`: walks the characters; at each separator pushes what it collected and starts again; after the loop pushes the last piece. Empty fields are kept: `"1|Ali||5"` gives 4 pieces `{"1","Ali","","5"}`. This matters: a student with no groups ends in `|` and must still have 7 fields.
- `toNumber(text)`: `std::stoll` inside `try/catch (...)`; bad text returns 0, never crashes.
- `toIntList("1,2,3")` -> `{1,2,3}` (empty text -> empty list); `fromIntList` does the reverse, adding `,` before every item except the first (`if (i > 0)`).
- `readLines(name)`: opens `dataFolder + name` with `ifstream`; cannot open = no lines (missing file = no records); strips Windows `\r`; skips empty lines.
- The 6 loaders all have the same shape: read lines, `clear()`, for each line `split` by `|`, **if the field count is wrong `continue` (skip the broken line)**, fill a struct (`(int)toNumber(f[0])`...), `push_back`. For messages `read = (f[6] == "1")`.
- The 6 savers: `ofstream` opening a file **erases it**, so "rewrite the whole file" is automatic; write the fields with `|` and end with `\n`; return `false` if the file cannot be opened. `(m.read ? 1 : 0)` is `if read then 1 else 0`.
- `saveAll` calls all six; `nextId(list)` = biggest id + 1 (1 for an empty list), overloaded once per struct. Max+1, not size+1, so ids stay unique after deletions.
- `isSafeText`: false if the text contains `|` or a line break (they would break the file format). Every module saving typed text calls it first.
- `setDataFolder`, `setReportsFolder`, `getReportsFolder`: the folders are `static` variables (private to storage.cpp); tests redirect them. `writeReportFile(name, text)` writes a CSV; reports.cpp never touches files itself.
- `static` functions (`split`, `toNumber`...) are private helpers; only what is in `storage.h` is public.

## 5. alerts.cpp (the biggest piece of logic you own)
Constants at the top: `LOW_BALANCE_LESSONS = 2`, `DEBT_STEP = 20` (named, not magic numbers).

- `lessonsCovered(db, student)`: lowest `pricePerLesson` among the student's groups, returns `balance / lowestPrice` (integer division: 150 000 / 100 000 = 1); `-1` if the student has no group.
- `isLowBalance`: balance >= 0 AND covered is not -1 AND covered < 2. A debtor is NOT low balance (the debt alert covers it).
- `countDebtors`: count of balances below 0.
- `accountIdsWithRole(db, "manager")`: all ids with that role = the recipients.
- `hasUnread(db, toId, type, studentId)`: is there already an UNREAD message with the same receiver, type and student? This is the whole no-duplicate rule.
- `addMessage`: builds a `Message` (id = `nextId`, `read = false`) and pushes it, no saving.
- `sendAlert(db, recipients, type, studentId, text, today)`: for each recipient, only if `!hasUnread`, add the message; returns how many were really sent. **Every alert goes through it**, so the rule cannot be forgotten.
- `checkAlerts(db, today)`, in 4 blocks:
  1. per student: `balance < 0` -> `"debt"` to managers; else `isLowBalance` -> `"low_balance"` to managers;
  2. directors: `milestones = countDebtors / 20`; each director has `alreadySent` `"debt_milestone"` messages; loop `step = alreadySent+1 .. milestones` and send one message each. 19 debtors = 0, 20-39 = 1, 40 = 2. Exactly one message per milestone, ever. (`studentId` is 0, it is about nobody in particular.);
  3. per group, per student: recipients = all managers + the group's teacher; `hasAbsenceStreak` -> `"absence_streak"`, `hasLowAttendance` -> `"low_attendance"`;
  4. `saveMessages` once if anything was created; if saving fails the new messages are removed again and 0 is returned.
  It checks everything every time (simple, always correct); the duplicate rule stops the repeats. Returns how many messages it created.
- Design choices to defend: low attendance is per student (message says who); a debtor gets no low-balance alert; teachers get nothing about money.

## 6. main.cpp core
- `readLine(question)`: prints, `getline`s a whole line; if input ended (Ctrl+Z) `std::exit(0)` so no loop spins forever.
- **Why `getline` and not `cin >>`:** `cin >> number` breaks on letters; `getline` always works and we validate ourselves.
- `parseInt(text, result)`: false if empty or longer than 9 chars; optional leading `-`; every other char must be `'0'..'9'`; only then `stoi` (so it can never throw). `parseLong` is the same for money (`stoll`, up to 13 chars). It accepts `-` on purpose, so the "must be more than 0" message comes from the logic function.
- `askInt(question, value)`: read + parse + print the error; returns false so screens do `if (!askInt(...)) return;` = "error shown, back to menu".
- `readChoice()`: returns the number or -1 (error already printed).
- `getToday()`: `time` -> `localtime` -> `strftime` with `%Y-%m-%d`; the clock is read only here and the string is passed down (tests use fixed dates).
- The three menus: `while (true)` { print the role's options, `choice = readChoice()`, `0` = `return` (logout), `else if` chain calls screens, last branch prints "no such menu option" unless `choice == -1` }. **Each role has its own function, so a teacher cannot reach a director option: it is not in the teacher's function.** `Account me` is passed **by copy** because creating an account `push_back`s and may move the vector in memory (a reference would dangle).
- `main()`: `loadAll`; stop if no accounts; `getToday`; 3 login attempts (`for (attempt...; attempt <= 3 && index == -1; ...)`); copy the account; print welcome and unread count; pick the menu by `me.role`.

## 7. Practice on paper (answers below)
1. `split("7|Ali||5", '|')` -> ?
2. Student balance 190 000, groups priced 100 000 and 80 000. `lessonsCovered`? Low balance?
3. 40 debtors; a director already has 1 `debt_milestone` message. What does `checkAlerts` send?
4. `parseInt("12a")`, `parseInt("-5")`, `parseInt("")`, `parseInt("1234567890")`?
5. A manager has an UNREAD low-balance alert about student 7. The student is still low. What happens on the next payment? What if the manager has read it?

Answers: (1) 4 pieces `{"7","Ali","","5"}`. (2) lowest price 80 000, 190 000 / 80 000 = 2, so not low. (3) milestones = 2, alreadySent = 1, one message "40 students or more are in debt". (4) false; true and -5; false; false (10 chars > 9). (5) skipped (unread duplicate); if read, a new one is created.

## 8. Questions you will get
- *Why text files?* Course scope (standard library only); readable and easy to debug.
- *What if a file is missing or damaged?* Missing = empty list; a broken line is skipped; nothing crashes.
- *Why does only storage.cpp use files?* One place to change; the logic and a future web layer do not depend on storage details.
- *How do you stop duplicate alerts?* `sendAlert` -> `hasUnread`.
- *Why integer division in `lessonsCovered`?* Lessons are whole; 150 000 / 100 000 = 1 lesson fully paid.
- *Why is `me` copied?* A reference into a vector breaks when the vector grows.
- *Why `static` helpers?* Private to the file; the .h shows only the public interface.
- *Why max+1 for ids?* Unique even after deletions.
- *Why is `test.cpp` not in `src/`?* `src/*.cpp` would link two `main`s.

## 9. Demo path you present
Login as manager -> record a small payment for student 2 -> "6 new alert messages" -> login as teacher -> unread count at login -> inbox. Then explain `checkAlerts` on screen.

## 10. Your tests and a self-check
Tests: save/load round trip, bad files, nextId/isSafeText, low balance + debt thresholds, absence streak, low attendance (70% boundary and 4-lesson start), no duplicates, director milestones. Self-check: open `alerts.cpp`, change `DEBT_STEP` to 19, run `tests\run_tests.bat`, find which assert fails and why, then change it back.
