# The web interface - how it works and how to defend it

The console program is unchanged. The `web/` folder adds a browser interface on top of the SAME C++ modules. Read this once, top to bottom, then open the files next to it.

## 1. The one-paragraph answer (say this first)
"The web page is written in HTML, CSS and JavaScript, with a small Node.js server. The server does not contain any business rule. For every action it starts a small C++ program, `engine.exe`, which loads the data files, calls the SAME functions as the console version (`markAttendance`, `recordPayment`, `checkAlerts`...), saves, and prints the answer as JSON. So charging, alerts and reports exist in exactly one place: the C++ modules."

```
browser (HTML/CSS/JS)  --->  server.js (Node)  --->  engine.exe  --->  src/*.cpp modules  --->  data/*.txt
 shows data, sends forms      logins + sessions        one command        unchanged            same files as
                              passes requests on       in, JSON out                            the console app
```

**Why not write the rules again in JavaScript?** Two copies of the rules would drift apart, and the web page would no longer be "our C++ program". This way the console and the web can even be used on the same data.

## 2. Run it
```
web\run_web.bat          builds engine.exe, then starts the server
```
Open http://localhost:3000. Logins: admin/admin123 (director), manager/manager123, teacher/teacher123. Stop with Ctrl+C. By hand: build the engine with the command at the top of `web/engine/api.cpp`, then `node web/server.js`. Rebuild the engine after any change in `src/` or `api.cpp`; the pages (`web/public`) need only a browser refresh.

## 3. The files
| File | Job | Language |
|---|---|---|
| `web/engine/api.cpp` | the bridge: one command in, JSON out, calls the existing modules | C++ |
| `web/server.js` | static files, login sessions, passes API requests to the engine one at a time | JavaScript (Node) |
| `web/public/index.html` | an empty page plus `<script src="app.js">` | HTML |
| `web/public/app.js` | all pages: draws the screens from JSON, sends forms | JavaScript |
| `web/public/style.css` | all the looks | CSS |
| `web/public/logo.png`, `favicon.svg`, `fonts/` | logo, tab icon, Golos Text font (served locally, works offline) | files |

## 4. A click, end to end (saving a lesson)
1. In the group page you choose the date and press **Save Fri 2026-11-27**. `app.js` sends `POST /api/attendance` with `{groupId, date, present:"1,2,4"}`.
2. `server.js` reads the `sid` cookie, finds the logged-in account in its `sessions` map, and ADDS `as=<account id>` itself (the browser can never choose who it is). It puts the request in the line and starts `engine.exe attendance`, sending `as=3`, `groupId=1`, `date=...`, `present=1,2,4` on stdin.
3. `api.cpp` loads the six files into a `Database`, looks up the account, checks `isAllowed(role, "attendance")`, then calls `markAttendance(db, me, groupId, date, presentIds, error)` from `attendance.cpp`. That function checks the teacher owns the group, the date is valid, the day is not already saved, charges every student, saves. Then `checkAlerts` runs, exactly like the console menu.
4. The engine prints `{"ok":true,"date":"...","newAlerts":3}` and exits. The server forwards it. `app.js` shows a toast and redraws the group page: the new lesson box appears.

## 5. api.cpp, part by part
- **Parameters.** `main` reads `key=value` lines from stdin into a `std::map` (stdin, not the command line, so any characters including Cyrillic names survive). `get(p, key)` returns text; `getNumber` accepts only whole numbers up to 13 digits (never throws); `getIdList("1,2,3")` turns text into ids.
- **JSON by hand.** The standard library has no JSON, so small helpers build text: `q(text)` puts quotes around a string and escapes `"`, `\` and line breaks; `num`, `textList`, `studentRefs` build numbers and lists. Each record has a `...Json` function (`accountJson`, `studentJson`, `groupJson`).
- **Same rules as the alerts.** `balanceStatus` returns `"debt"` if `balance < 0`, `"low"` if `isLowBalance(...)`, else `"ok"`. The coloured pips on the page are just this value plus `lessonsCovered`.
- **Permissions.** `isAllowed(role, command)` is a table of three sets: commands for everyone (`me`, `inbox`, `groups`, `attendance`), manager commands (students, payments, reports, daily list, add group...), director-only (`accounts`, `create-account`). A teacher gets only the first set. Even inside the allowed set, `markAttendance` and `visibleGroupIndexes` still restrict a teacher to their own groups.
- **`run(command, params)`** loads the data, handles `login`, then requires `as`, finds the account, checks `isAllowed`, and runs the command. Every command is: read parameters, call one module function, build JSON. Failures return `{"ok":false,"error":"..."}` with the message produced by the C++ function.
- **Why one process per request?** It is simple and always correct (fresh data each time, nothing cached). The cost is a few milliseconds, which does not matter for a staff tool.

## 6. server.js, part by part
- `callEngine(command, params)` runs `execFile(ENGINE, [command])`, writes the parameters to stdin, parses the JSON answer. `line = line.then(...)` is a **queue**: each request waits for the previous one, so two clicks can never write the same file at the same moment.
- `sessions` is a `Map` from a random token to the account id. Login creates `crypto.randomBytes(24)` and sends it as a cookie `sid` with `HttpOnly` (JavaScript on the page cannot read it) and `SameSite=Strict`.
- `handleApi` routes `/api/<command>`: `login` and `logout` are special; everything else needs a session; the session id is written into `params.as`, overriding anything the browser sent. `report-file` calls the engine's `report` and sends the CSV with `Content-Disposition: attachment` so it downloads.
- `sendFile` serves `web/public`, and refuses any path that leaves that folder (`path.normalize` + `startsWith(PUBLIC)` stops `../` tricks).
- The server listens on `127.0.0.1` only: reachable from this computer, not from the network.

## 7. app.js, part by part
- **Helpers.** `esc()` escapes text before it is put into HTML (a student named `<b>x</b>` shows as text, never as markup). `money()` writes `1 234 567`. `api()`/`post()` call the server; a 401 answer sends the user back to the login. `balanceCell()` draws the lessons-left pips. `avatar()` makes the initials circle.
- **Roles.** `PAGES` lists which pages each role sees in the left rail: director: Today, Students, Groups, Payments, Inbox, Reports, Accounts; manager: the same without Accounts; teacher: only Groups and Inbox. This is for convenience; the real protection is `isAllowed` in the engine.
- **Router.** The page is the part of the URL after `#` (`#groups/1`). `render()` draws the rail, then calls the page function. Each page function asks the API, builds HTML in a string, puts it in the page, and attaches click handlers.
- **Groups page (attendance lives here).** A lesson belongs to a group AND a date, so the form for saving it is on the group page, with the date visible. Lesson boxes show each student's status. `missedDays()` finds scheduled days since the first lesson that have no lesson saved and shows them as dashed "Not marked" boxes with a button that fills the form with that date.
- **Everything else:** Today (call list sorted by the C++ selection sort, warnings), Students (live search, side panel with payment and enrol forms), Payments, Inbox (opening it marks messages read, like the console), Reports (month picker + CSV), Accounts (director).

## 8. style.css
Colour tokens are at the top (`:root`): paper, ink (lapis), teal (actions, "present"), cobalt (focus), apricot (low balance), madder (debt). The palette comes from Samarkand tile work and the logo-star pattern is drawn as a repeating background (`--tile`). One memorable idea: the **pips** (`●●○○`) show how many lessons the balance still pays for; fewer than 2 turns apricot, debt turns red. This is the alert rule drawn on screen. The phone layout turns the left rail into a bottom bar (`@media (max-width: 800px)`).

## 9. Questions you will get
- *Is the web version a different program?* No: same C++ rules, same data files; the console version still works.
- *Where is the money logic?* `markAttendance` in `attendance.cpp`, as before. The web only calls it.
- *Who stops a teacher from opening the director pages?* The engine: `isAllowed` returns "Your role cannot do this." Hiding the menu item is only for looks.
- *Can the browser pretend to be another user?* No: the account id comes from the server-side session; the cookie is HttpOnly and the value is random.
- *What if two people click at the same moment?* The server queue runs engine calls one after another.
- *Is this secure enough for real use?* Not yet, and we say so: no HTTPS, passwords use the course's teaching hash, sessions disappear when the server restarts. It is designed for a course project on one computer.
- *Why Node and not C++ for the server?* A web server in plain C++ needs sockets and HTTP parsing; Node has it built in, and the project rule "standard library only" applies to the C++ program. The engine IS plain C++ standard library.
- *Why JSON?* A simple text format both sides understand: `{"ok":true,"balance":50000}`.
- *Why is `api.cpp` allowed to print?* It is a second "main", like `tests/test.cpp`: its only job is input/output around the modules. The modules themselves still print nothing.

## 10. Limits (be honest in the defense)
Sessions are in memory (restart = log in again). One engine process per request. No HTTPS or rate limiting.
