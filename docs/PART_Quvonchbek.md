# Quvonchbek's part - accounts and login, daily list, monthly report, tables

Full line-by-line text is in `CODE_WALKTHROUGH.md` (parts 0, 3, 11, 12, 14, 15). This file is what YOU must be able to explain, in the order to learn it.

## 1. Your scope
| Where | What |
|---|---|
| `src/accounts.h/.cpp` | `hashPassword`, `login`, `findAccountIndex`, `isValidRole`, `createAccount`, `countUnread`, `getInbox`, `markInboxRead` |
| `src/daily.h/.cpp` | `weekdayName`, `sortByBalance`, `buildDailyList` |
| `src/reports.h/.cpp` | `isValidMonth`, `buildMonthlyReport`, `monthlyReportCsv`, `saveMonthlyReport` |
| `src/table.h/.cpp` | `formatTable` |
| `src/main.cpp` (screens) | `showInbox`, `listAccounts`, `createAccountScreen`, `dailyScreen`, `reportScreen` |
| `tests/test.cpp` | accounts (3), daily (3), reports (2), table (1) |

You use code from others: `isValidDate`, `formatMoney` (students.cpp), `isLowBalance` (alerts.cpp), `hasAbsenceStreak` / `hasLowAttendance` (attendance.cpp), `writeReportFile` (storage.cpp).

## 2. accounts.cpp
- `hashPassword(password)`: `h = 5381`; for each character `h = (h * 33 + c) % 1000000007`. Same password = same number, so we store and compare numbers, never the real password. `% 1000000007` keeps it inside `long long`; `(unsigned char)` makes the character positive. By hand for "A" (code 65): 5381*33 + 65 = 177 638. It is a teaching hash (djb2) and NOT secure for real systems (real ones use bcrypt with a random salt): say so honestly.
- `login(db, username, password)`: hash the typed password, loop over accounts, both username AND hash must match, return the **position** in the vector or **-1** (a real position is never negative, so -1 safely means "not found"). It never prints. The error message does not say which part was wrong (so nobody can discover valid usernames).
- `findAccountIndex(db, id)`: position or -1 (used to turn a teacher id into a name).
- `isValidRole`: `role == "director" || role == "manager" || role == "teacher"`.
- `createAccount(db, username, password, fullName, role, error)`: a checklist, each `if (bad) { error = "..."; return false; }`: empty username, space in username (`find(' ') != std::string::npos`), `|` not allowed (`isSafeText`), empty name, password under 4 characters, bad role, duplicate username. Then the Account is built (`id = nextId`, hashed password), pushed, saved; if saving fails `pop_back()` undoes it. The reason comes out through `std::string& error`.
- Inbox: `countUnread` (messages with this `toId` and not read), `getInbox` (copies this account's messages), `markInboxRead` (sets `read = true` on this account's messages only, then saves). Reading the inbox matters: alerts are only re-sent when the old one has been read.

## 3. daily.cpp
- `weekdayName("2026-11-26")` -> `"Thu"`. Sakamoto's formula: `if (month < 3) year--; w = (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;` 0 = Sunday. Say: "we need the weekday to know which groups meet today and C++ has no simple standard function for it, so we use a known formula." Invalid date gives `""`.
- `sortByBalance(list)` - the hand-written **selection sort** required by the brief:
```cpp
for (i = 0; i + 1 < size; i++) {
    smallest = i;
    for (j = i + 1; j < size; j++)
        if (list[j].balance < list[smallest].balance) smallest = j;
    if (smallest != i) swap list[i] and list[smallest] through a temp variable
}
```
  In words: find the smallest balance in the unsorted part, swap it to the front of that part, repeat. After round 1 the first place is final, after round 2 the second... `i + 1 < size` (not `i < size - 1`) because `size()` is unsigned and `size - 1` would wrap around for an empty list. No `std::sort`, no `std::swap`.
- `buildDailyList(db, today)` returns a `DailyList` struct (3 vectors) and prints nothing:
  1. `todayGroupIds`: groups whose `days` contain today's weekday;
  2. `callList`: students with `balance < 0` or `isLowBalance`, copied then sorted (debtors first);
  3. `warnings`: a `Warning` for every student with `hasAbsenceStreak` (text "absent from the last 2 lessons") or `hasLowAttendance` ("attendance is only N%"). A group member whose student record is missing is skipped (damaged data never crashes).
  Returning one struct is how a function hands back several lists.

## 4. reports.cpp
- `isValidMonth`: length 7, `-` at position 4, digits, month 01-12, year >= 2000.
- `buildMonthlyReport(db, month, report, error)`: **revenue** = sum of payments with `date.substr(0, 7) == month` (first 7 characters of "2026-11-03" are "2026-11"); **total debt** = sum of every negative balance as a positive number (it is the debt RIGHT NOW, not at the end of that month: balances keep no history, say this honestly); **per group**: its lessons in that month, total present and absent marks, `percent = present * 100 / (present + absent)`, or `-1` when nothing was marked (shown as `n/a`, avoids dividing by zero).
- `csvField(text)` (private): a name containing a comma or quote is wrapped in `"..."` with inner quotes doubled, otherwise the columns would break.
- `monthlyReportCsv(report)`: builds the whole file as ONE string with `+=` and `\n` (CSV = values separated by commas; Excel opens it as a table).
- `saveMonthlyReport(report, filePath, error)`: file name `monthly_report_YYYY-MM.csv`, text handed to `writeReportFile` in storage.cpp (reports.cpp itself never touches a file); a missing `reports` folder gives a clear error.

## 5. table.cpp (used by every list in the program)
- Why: the old output repeated every column name in every row. A table writes each title once.
- `formatTable(headers, rows, align)` returns ONE string. `align` = one letter per column, `l` or `r` (`"rl"`); missing = left. Money and counts are right-aligned.
- Step 1, widths: each column = its longest text (title included). Step 2, lines: `separator` makes `+----+----+` (width + 2 dashes per column), `rowLine` pads every cell with spaces (`padRight` / `padLeft`) with `" | "` between; the table = separator, titles, separator, rows, separator.
- `displayLength(text)`: counts characters, not bytes. A Cyrillic letter is 2 bytes but 1 place on screen, so padding by `size()` would misalign. UTF-8 extra bytes all start with bits `10`, so we count only bytes where `(c & 0xC0) != 0x80` (`&` = bitwise AND).

## 6. Your screens in main.cpp
- `showInbox`: gets the inbox, builds rows (`NEW` marker, date, text), prints the table, then `markInboxRead`.
- `listAccounts`, `createAccountScreen` (director only: four `readLine`s then `createAccount`, prints "Account created." or the error).
- `dailyScreen`: calls `buildDailyList`, prints 3 tables (groups today with "saved / NOT marked yet" via `hasLesson`, call list, warnings), ids turned into names.
- `reportScreen`: asks month, `buildMonthlyReport`, prints revenue, debt and a group table, `saveMonthlyReport`, prints where the file went.

## 7. Practice on paper (answers below)
1. Weekday of 2026-11-26 with the formula (t = {0,3,2,5,0,3,5,1,4,6,2,4}).
2. Selection-sort `{5, -3, 10, 0}` round by round.
3. Report for November: payments 100 (Oct 31), 200 (Nov 1), 300 (Nov 30), 400 (Dec 1). Revenue? Balances -50, -25, 100: total debt?
4. Table widths for headers `{"ID","Name"}` and rows `{"1","Ali"},{"10","Bo"}`.
5. Why does `login` return -1 and not 0 for "not found"?
6. Why does the report print `n/a`?

Answers: (1) month >= 3 so year stays 2026; 2026 + 506 - 20 + 5 + t[10] (= 2) + 26 = 2545; 2545 % 7 = 4 = Thu. (2) round 1: smallest -3 swaps with 5 -> {-3,5,10,0}; round 2: smallest of the rest is 0 -> {-3,0,10,5}; round 3: smallest of {10,5} is 5 -> {-3,0,5,10}. (3) revenue 200 + 300 = 500 (`substr(0,7) == "2026-11"`); debt 50 + 25 = 75. (4) ID column width 2, Name width 4: `+----+------+`, `|  1 | Ali  |`, `| 10 | Bo   |` with `align = "rl"`. (5) 0 is a valid position (the first account), so it cannot mean "not found". (6) 0 present + 0 absent would divide by zero; -1 marks "nothing to compute".

## 8. Questions you will get
- *Why hash passwords?* The file never holds the real password; ours is a simple teaching hash, real systems use salted bcrypt.
- *Why return a position instead of the Account?* Then the caller can tell "not found" (-1) and still access the record.
- *Why not `std::sort`?* The brief asks for a hand-written sort; ours is `sortByBalance`.
- *Why does the report not write the file itself?* Only `storage.cpp` touches files; reports builds the text and asks storage to write it.
- *Why `substr(0, 7)` for the month?* Dates are `YYYY-MM-DD`; the first 7 characters are the month.
- *Why is total debt "right now"?* Balances keep no history.
- *What does `formatTable` return and why not print?* A string; only main prints, so a web page could reuse the same function.
- *Why count UTF-8 bytes specially?* To keep columns aligned for non-ASCII names.

## 9. Demo path you present
Director: create a staff account -> log in with it -> manager: daily action list (three tables) -> monthly report for 2026-11 -> open `reports/monthly_report_2026-11.csv` in Excel. Show the sorted call list (debtors first) and explain the selection sort.

## 10. Your tests and a self-check
Tests: hash value + login rules, every `createAccount` rule + file saved, inbox counting and reading; weekday of known dates (leap day included), sort (empty, single, reversed, equal balances), daily list order and warnings; report numbers, exact CSV text, quoting, file written, missing folder error; exact table text, right alignment, Cyrillic width. Self-check: in `daily.cpp` flip `<` to `>` in the sort, run `tests\run_tests.bat`, find the failing assert, then restore it.
