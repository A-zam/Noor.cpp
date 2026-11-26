# Azizbek's part - students, groups, payments, attendance

Full line-by-line text is in `CODE_WALKTHROUGH.md` (parts 0, 6, 7, 8, 14, 15). This file is what YOU must be able to explain, in the order to learn it.

## 1. Your scope
| Where | What |
|---|---|
| `src/students.h/.cpp` | `formatMoney`, `isValidDate`, `findStudentIndex`, `findGroupIndex`, `addStudent`, `searchStudentsByName`, `addGroup`, `enrollStudent`, `removeStudentFromGroup`, `recordPayment` |
| `src/attendance.h/.cpp` | `visibleGroupIndexes`, `hasLesson`, `lessonsOfGroup`, `markAttendance`, the rules `hasAbsenceStreak`, `studentAttendancePercent`, `hasLowAttendance` |
| `src/main.cpp` (screens) | `studentsTable`, `groupsTable`, `allPositions`, `addStudentScreen`, `listStudentsScreen`, `searchStudentsScreen`, `listGroupsScreen`, `enrollScreen`, `addGroupScreen`, `showGroupStudentsScreen`, `studentsMenu`, `groupsMenu`, `paymentScreen`, `attendanceScreen`, `printNewAlerts` |
| `tests/test.cpp` | students (5 tests), attendance (2 tests) |

Others call your code: `alerts.cpp` uses your attendance rules and `formatMoney`; `daily.cpp` uses `isValidDate` and your rules.

## 2. The one pattern behind every "change data" function
```
1. look up the records involved   (find...Index; -1 = not found -> error)
2. validate the input             (each failure: error = "..."; return false / 0)
3. change the data in memory      (push_back / += / erase)
4. save the files                 (if saving fails: UNDO the memory change, set error, return)
5. return success
```
Functions that CREATE something return the new id, or **0 for failure** (real ids start at 1). The others return `true/false`. The reason travels out through the reference parameter `std::string& error`: a function cannot return two things, so one goes through the parameter. The undo in step 4 (`pop_back`, `balance -= amount`) keeps memory and files identical. The functions print nothing: `main.cpp` prints.

## 3. students.cpp, function by function
- `formatMoney(1234567)` -> `"1 234 567"`: remember the sign, work with the positive number, `to_string`, walk the digits **from the right**, put each digit at the front of `result`, and after every 3rd digit (`count % 3 == 0 && i > 0`) a space.
- `isValidDate`: first the SHAPE (length 10, `-` at positions 4 and 7, digits elsewhere), then `substr` + `stoi` (safe because the shape is known), then the calendar: year >= 2000, month 1-12, `daysInMonth[month - 1]` (arrays start at 0), February has 29 in a leap year: `(year % 4 == 0 && year % 100 != 0) || year % 400 == 0` (2000 yes, 1900/2100 no).
- `findStudentIndex` / `findGroupIndex`: loop, return the position or -1. Everything else uses them to turn ids into records.
- `isValidPhone` (private): optional leading `+`, then 7-15 digits.
- Private helpers: `toLower` (adds `'a' - 'A'` = 32 to capital letters), `contains(list, value)`, `removeValue(list, value)` (`erase(begin() + i)`).
- `addStudent`: checks name, parent name, safe text (`isSafeText`), both phones; builds the Student with `balance = 0`, saves; returns the new id (`nextId`).
- `searchStudentsByName`: lowercase both sides, `name.find(text) != std::string::npos` = "appears inside"; returns POSITIONS so main can print them; empty text finds nothing.
- `addGroup`: name, price 1..100 000 000, teacher must be an account whose role is "teacher", days parsed by `parseDays`. `parseDays` walks the text, at each comma (and at the very end: loop runs to `i <= size`) validates the collected day against `Mon..Sun`, rejects repeats, skips spaces.
- `enrollStudent`: student and group must exist, not already enrolled; adds to BOTH `student.groupIds` and `group.studentIds` (both records store the link, so both must change; if saving fails both are undone).
- `removeStudentFromGroup`: `removeValue` on both lists. Old lessons stay (history); the student is no longer in `group.studentIds`, so future lessons do not charge them. This is the rule "a lesson is paid until the student leaves the group".
- `recordPayment`: student exists, `0 < amount <= 10 000 000 000`, valid date; creates a Payment, `balance += amount`; undo on save failure.

## 4. attendance.cpp, function by function
- `visibleGroupIndexes(db, who)`: `if (who.role != "teacher" || group.teacherId == who.id)` include the group. That one line is "teachers see only their own groups".
- `hasLesson(db, groupId, date)`: was this group already marked that day (prevents double charging).
- `lessonsOfGroup`: that group's lessons in saved order (oldest first).
- `markAttendance(db, by, groupId, date, presentIds, error)` - **the money logic**. Checks in order: group exists; **if `by` is a teacher the group must be his** (a second safety check inside the logic, so even a buggy menu cannot break the rule); valid date; group has students; not already marked; every present id belongs to the group. Then every student of the group goes into `presentIds` or `absentIds` (so the caller only says who is PRESENT; nobody is forgotten), the lesson is added, and **every student of the group is charged `pricePerLesson` whether present or absent** (rule: "paid no matter the attendance"). Balance may become negative = debt. Save lessons + students; undo everything if saving fails.
- Constants at the top: `STREAK_LENGTH = 2`, `MIN_LESSONS_FOR_PERCENT = 4`, `LOW_ATTENDANCE_PERCENT = 70`.
- `countMarks(lessons, studentId, present, absent)` (private): counts presents/absents of one student, results through reference parameters.
- `hasAbsenceStreak`: needs at least 2 lessons; loop from `size - 2` to the end = the LAST 2 lessons; if the student is not in `absentIds` of either, false. A student who joined later is in neither list, so no streak.
- `studentAttendancePercent`: `present * 100 / (present + absent)` (whole percent), `-1` if the student was never listed.
- `hasLowAttendance`: group must have 4+ lessons, then `present * 100 < 70 * (present + absent)`. Whole numbers only, no rounding; **exactly 70% is NOT low**.

## 5. Your screens in main.cpp
Every screen = ask -> call logic -> print the result or `Error: ...`. No rules in screens.
- `studentsTable(db, positions)` / `groupsTable(db, positions)` build the rows (ids become names via `findGroupIndex` / `findAccountIndex`; group lists joined with `", "`) and call `formatTable`. They take POSITIONS so the same function shows everything, a search result or one group's students. `allPositions(n)` = 0..n-1.
- `enrollScreen(db, true/false)` is used for both enroll and remove.
- `attendanceScreen`: lists the groups the user may see (`visibleGroupIndexes`), asks a group id, refuses ids not in that list, refuses if already marked today, then loops over the group's students asking `p` or `a` (a wrong answer repeats the question for the same student, so one typo does not cost the whole entry), then calls `markAttendance`.
- `paymentScreen`: id -> shows name and balance -> amount (`parseLong`) -> `recordPayment` -> new balance -> `printNewAlerts(checkAlerts(db, today))`. `attendanceScreen` also calls it after a successful save.

## 6. Practice on paper (answers below)
1. Walk `formatMoney(1234567)`.
2. Group price 100 000, students A (balance 500 000) and B (50 000), only A present. Balances after? Lesson lists?
3. `isValidDate("2026-02-29")`, `("2024-02-29")`, `("2026-4-1")`?
4. A teacher of group 2 calls `markAttendance` for group 1. Result? Balances?
5. Student 12 was removed from the group, then a lesson is saved. Is 12 charged?
6. Why does `recordPayment` do `pop_back()` and `balance -= amount` when saving fails?

Answers: (1) digits "1234567", from the right: 7,6,5 -> space, 4,3,2 -> space, 1 = `1 234 567`. (2) A 400 000, B -50 000; present {A}, absent {B}. (3) false (2026 is not a leap year); true; false (wrong shape). (4) false with "own groups" error, nothing changes. (5) No: charging loops over `group.studentIds`, which no longer contains 12. (6) So memory and files never disagree: the data is only "really" changed when it was saved.

## 7. Questions you will get
- *Where is "paid no matter attendance" in the code?* The charge loop in `markAttendance` runs over ALL students of the group.
- *How do you stop a teacher marking someone else's group?* Twice: the menu only lists his groups, and `markAttendance` checks `group.teacherId != by.id` again.
- *Why check the date format?* A wrong date in a file would break the report and the "last 2 lessons" rule.
- *What if the disk write fails?* Everything is undone in memory.
- *Why does enrolling update two records?* Both store the link; both change together or neither.
- *Why can balance be negative?* Negative = debt; lessons are charged even if not paid.
- *Why whole-number math in `hasLowAttendance`?* No rounding, and exactly 70% behaves correctly.
- *Why return 0 on failure from `addStudent`?* Ids start at 1, so 0 can never be a real id.

## 8. Demo path you present
Manager: Students -> add student -> enroll in a group -> search by name -> record a payment (balance changes) -> Mark attendance (p/a) -> show the lesson charged everyone. Then log in as the teacher and show she only sees her own groups.

## 9. Your tests and a self-check
Tests: formatMoney/isValidDate, addStudent + search, addGroup rules, enroll/remove (both sides, history kept), recordPayment; attendance charges (present and absent, outsiders and ex-members untouched) and attendance rules (permissions, same-day, bad input, nothing changed when refused). Self-check: in `attendance.cpp` delete the teacher check, run `tests\run_tests.bat`, read which assert fails, then restore it.
