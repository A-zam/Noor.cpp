// app.js - the whole browser side of LearNoor. Plain JavaScript, no libraries.
// It only shows data and sends forms to /api/...; all rules live in the C++ engine.

const app = document.getElementById('app');
let me = null;          // the logged-in account { id, name, role, unread }
let state = {};         // small per-page memory (selected student, attendance marks...)

// ---------- helpers ----------

// Never put text into HTML without escaping it (names could contain < or &).
function esc(text) {
  return String(text).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

// A round badge with the initials of a name; the colour depends on the id, so a person keeps theirs.
function avatar(name, id, big) {
  const initials = String(name).trim().split(/\s+/).slice(0, 2).map((w) => w[0]).join('').toUpperCase();
  return '<span class="av c' + (id % 5) + (big ? ' big' : '') + '" aria-hidden="true">' + esc(initials) + '</span>';
}
function person(name, id, big) { return '<span class="person">' + avatar(name, id, big) + '<span>' + esc(name) + '</span></span>'; }

// 1234567 -> "1 234 567"
function money(n) {
  const sign = n < 0 ? '-' : '';
  return sign + String(Math.abs(n)).replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
}

async function api(command, params, method) {
  const isPost = method === 'POST';
  const url = '/api/' + command + (!isPost && params ? '?' + new URLSearchParams(params) : '');
  const res = await fetch(url, {
    method: isPost ? 'POST' : 'GET',
    headers: { 'Content-Type': 'application/json' },
    body: isPost ? JSON.stringify(params || {}) : undefined,
  });
  const data = await res.json();
  if (res.status === 401 && command !== 'login') { me = null; showLogin(); }
  return data;
}
const post = (command, params) => api(command, params, 'POST');

let toastTimer;
function toast(text, bad) {
  const el = document.getElementById('toast');
  el.textContent = text;
  el.className = 'show' + (bad ? ' bad' : '');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className = ''; }, 3500);
}

// The lessons-left pips: how many lessons the balance still pays for (4 shown at most).
function balanceCell(s) {
  const slots = 4;
  const full = s.status === 'debt' ? 0 : Math.min(Math.max(s.lessons, 0), slots);
  let pips = '';
  for (let i = 0; i < slots; i++) pips += '<span class="pip' + (i < full ? ' full' : '') + '"></span>';
  let note = '';
  if (s.status === 'debt') note = 'in debt';
  else if (s.lessons >= 0) note = s.lessons + (s.lessons === 1 ? ' lesson left' : ' lessons left');
  else note = 'no group';
  return '<span class="' + s.status + '"><span class="pips" aria-hidden="true">' + pips + '</span>' +
         '<span class="amount">' + money(s.balance) + '</span><span class="lessons-left">' + note + '</span></span>';
}

function table(headers, rows, aligns) {
  if (rows.length === 0) return '';
  const head = headers.map((h, i) => '<th class="' + (aligns[i] === 'r' ? 'num' : '') + '">' + h + '</th>').join('');
  return '<div class="tablewrap"><table><thead><tr>' + head + '</tr></thead><tbody>' + rows.join('') + '</tbody></table></div>';
}

// ---------- pages by role ----------

const PAGES = {
  director: ['today', 'students', 'groups', 'payments', 'inbox', 'reports', 'accounts'],
  manager: ['today', 'students', 'groups', 'payments', 'inbox', 'reports'],
  teacher: ['groups', 'inbox'],
};
const TITLES = {
  today: 'Today', students: 'Students', groups: 'Groups',
  payments: 'Payments', inbox: 'Inbox', reports: 'Reports', accounts: 'Accounts',
};
const RENDER = { today, students, groups, payments, inbox, reports, accounts };

function currentPage() {
  const wanted = location.hash.replace('#', '').split('/')[0];
  const allowed = PAGES[me.role];
  return allowed.includes(wanted) ? wanted : allowed[0];
}

async function refreshMe() {
  const data = await api('me');
  if (data.ok) me = data.account;
}

async function render() {
  if (!me) return showLogin();
  const page = currentPage();
  await refreshMe();
  if (!me) return;
  const links = PAGES[me.role].map((p) => {
    const badge = p === 'inbox' && me.unread > 0 ? '<span class="badge">' + me.unread + '</span>' : '';
    return '<a class="nav' + (p === page ? ' on' : '') + '" href="#' + p + '">' + TITLES[p] + badge + '</a>';
  }).join('');
  app.innerHTML =
    '<div class="shell"><nav class="rail" aria-label="Main">' +
    '<div class="brand"><img src="logo.png" alt="LearNoor CRM"></div>' + links +
    '<a class="nav" href="#" id="logout">Log out</a>' +
    '<div class="who">' + avatar(me.name, me.id) + '<div>' + esc(me.name) + '<br><span class="role">' + esc(me.role) + '</span></div></div></nav>' +
    '<main id="page"></main></div>';
  document.getElementById('logout').onclick = async (e) => {
    e.preventDefault();
    await post('logout');
    me = null;
    showLogin();
  };
  await RENDER[page](document.getElementById('page'));
}
window.addEventListener('hashchange', () => { state = {}; if (me) render(); });

// ---------- login ----------

function showLogin() {
  app.innerHTML =
    '<div class="login"><div class="brandside"><img src="favicon.svg" alt="" width="64" height="64">' +
    '<p>Every lesson, every balance, in one place.</p></div>' +
    '<div class="formside"><form id="loginForm" class="panel"><img class="wordmark" src="logo.png" alt="LearNoor CRM"><h2>Log in</h2><div id="loginError"></div>' +
    '<div class="field"><label for="u">Username</label><input id="u" autocomplete="username" required autofocus></div>' +
    '<div class="field"><label for="p">Password</label><input id="p" type="password" autocomplete="current-password" required></div>' +
    '<button class="primary" style="width:100%">Log in</button></form></div></div>';
  document.getElementById('loginForm').onsubmit = async (e) => {
    e.preventDefault();
    const data = await post('login', { username: document.getElementById('u').value, password: document.getElementById('p').value });
    if (!data.ok) {
      document.getElementById('loginError').innerHTML = '<div class="error">' + esc(data.error) + '</div>';
      return;
    }
    me = data.account;
    location.hash = '';
    render();
  };
}

// ---------- Today ----------

function greeting() {
  const h = new Date().getHours();
  return h < 12 ? 'Good morning' : h < 18 ? 'Good afternoon' : 'Good evening';
}
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const WEEKDAY_NAMES = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
function longDate(date) {   // "2026-11-27" -> "Friday, 27 November"
  const d = new Date(date + 'T00:00:00Z');
  return WEEKDAY_NAMES[d.getUTCDay()] + ', ' + d.getUTCDate() + ' ' + MONTHS[d.getUTCMonth()];
}

async function today(el) {
  const d = await api('today');
  if (!d.ok) { el.innerHTML = '<div class="error">' + esc(d.error) + '</div>'; return; }

  const groupRows = d.groups.map((g) =>
    '<tr><td>' + esc(g.name) + '</td><td>' + esc(g.teacher) + '</td><td class="num">' + g.students + '</td><td>' +
    (g.marked ? '<span class="chip">Saved</span>' : '<a class="btn" href="#groups/' + g.id + '" style="text-decoration:none;padding:3px 10px">Mark attendance</a>') + '</td></tr>');
  const callRows = d.calls.map((s) =>
    '<tr><td>' + person(s.name, s.id) + '</td><td>' + balanceCell(s) + '</td><td>' + esc(s.parentName) + ' ' + esc(s.parentPhone) + '</td></tr>');
  const warnRows = d.warnings.map((w) =>
    '<tr><td>' + esc(w.name) + '</td><td>' + esc(w.group) + '</td><td>' + esc(w.text) + '</td></tr>');

  el.innerHTML =
    '<div class="hero"><p class="hello">' + greeting() + ', ' + esc(me.name.split(' ')[0]) + '</p><h1>' + esc(longDate(d.date)) + '</h1><p class="summary">' +
    '<span><b>' + d.groups.length + '</b>' + (d.groups.length === 1 ? 'lesson' : 'lessons') + ' today</span>' +
    '<span><b>' + d.calls.length + '</b>' + (d.calls.length === 1 ? 'call' : 'calls') + ' about money</span>' +
    '<span><b>' + d.warnings.length + '</b>attendance ' + (d.warnings.length === 1 ? 'warning' : 'warnings') + '</span></p></div>' +
    '<section class="block"><h2>Lessons today</h2>' +
    (groupRows.length ? table(['Group', 'Teacher', 'Students', 'Attendance'], groupRows, ['l', 'l', 'r', 'l']) : '<div class="panel muted">No group meets today.</div>') + '</section>' +
    '<section class="block"><h2>Call about money</h2>' +
    (callRows.length ? table(['Student', 'Balance', 'Parent'], callRows, ['l', 'l', 'l']) : '<div class="panel muted">Nobody needs a call.</div>') + '</section>' +
    '<section class="block"><h2>Attendance warnings</h2>' +
    (warnRows.length ? table(['Student', 'Group', 'Problem'], warnRows, ['l', 'l', 'l']) : '<div class="panel muted">No attendance problems.</div>') + '</section>';
}

// ---------- Students ----------

async function students(el) {
  const query = state.q || '';
  const d = await api('students', { q: query });
  if (!d.ok) { el.innerHTML = '<div class="error">' + esc(d.error) + '</div>'; return; }

  const rows = d.students.map((s) =>
    '<tr class="pick' + (state.sel === s.id ? ' on' : '') + '" data-id="' + s.id + '" tabindex="0"><td>' + person(s.name, s.id) + '</td><td>' +
    s.groups.map((g) => esc(g.name)).join(', ') + '</td><td>' + balanceCell(s) + '</td></tr>');

  el.innerHTML =
    '<div class="head"><h1>Students</h1></div>' +
    '<div class="row" style="margin-bottom:14px"><input id="q" type="search" placeholder="Search by name" value="' + esc(query) + '" aria-label="Search students">' +
    '<button id="addBtn" class="primary">Add student</button></div>' +
    '<div class="split"><div>' + (rows.length ? table(['Student', 'Groups', 'Balance'], rows, ['l', 'l', 'l']) :
      '<div class="panel muted">' + (query ? 'No student matches "' + esc(query) + '".' : 'No students yet. Add the first one.') + '</div>') +
    '</div><div id="side"></div></div>';

  const q = document.getElementById('q');
  let timer;
  q.oninput = () => {
    clearTimeout(timer);
    timer = setTimeout(async () => { state.q = q.value; const pos = q.selectionStart; await students(el); const n = document.getElementById('q'); n.focus(); n.setSelectionRange(pos, pos); }, 250);
  };
  document.getElementById('addBtn').onclick = () => { state.sel = null; addStudentForm(el); };
  el.querySelectorAll('tr.pick').forEach((tr) => {
    const open = () => { state.sel = Number(tr.dataset.id); students(el); };
    tr.onclick = open;
    tr.onkeydown = (e) => { if (e.key === 'Enter') open(); };
  });
  if (state.sel) await studentPanel(el, state.sel);
  else if (state.adding) addStudentForm(el);
}

async function studentPanel(el, id) {
  const d = await api('student', { id });
  const side = document.getElementById('side');
  if (!d.ok) { side.innerHTML = '<div class="error">' + esc(d.error) + '</div>'; return; }
  const s = d.student;
  const g = await api('groups');
  const free = g.groups.filter((x) => !s.groups.some((y) => y.id === x.id));

  side.innerHTML =
    '<div class="panel"><div style="margin-bottom:6px">' + person(s.name, s.id, true) + '</div>' +
    '<div class="bigmoney ' + s.status + '">' + balanceCell(s) + '</div>' +
    '<dl class="facts"><dt>Phone</dt><dd>' + esc(s.phone) + '</dd><dt>Parent</dt><dd>' + esc(s.parentName) + ', ' + esc(s.parentPhone) + '</dd>' +
    '<dt>Groups</dt><dd>' + (s.groups.length ? s.groups.map((x) =>
      '<span class="chip">' + esc(x.name) + '</span><button class="danger" data-leave="' + x.id + '" style="padding:0 8px;font-size:.8rem">Remove</button> ').join('') : 'None') + '</dd>' +
    d.attendance.map((a) => '<dt>' + esc(a.group) + '</dt><dd>' + (a.percent < 0 ? 'no lessons yet' : a.percent + '% attendance') + '</dd>').join('') + '</dl>' +
    '<h2>Record a payment</h2><form id="payForm"><div class="row"><input id="amt" inputmode="numeric" placeholder="Amount in UZS" aria-label="Amount in UZS" required>' +
    '<button class="primary">Save payment</button></div></form>' +
    (free.length ? '<h2 style="margin-top:20px">Add to a group</h2><div class="row"><select id="grp" aria-label="Group">' +
      free.map((x) => '<option value="' + x.id + '">' + esc(x.name) + '</option>').join('') + '</select><button id="enrollBtn">Add to group</button></div>' : '') +
    '<h2 style="margin-top:20px">Payments</h2>' +
    (d.payments.length ? d.payments.map((p) => '<p>' + esc(p.date) + ' &nbsp; <b>' + money(p.amount) + '</b> UZS</p>').join('') : '<p class="muted">No payments yet.</p>') +
    '</div>';

  document.getElementById('payForm').onsubmit = async (e) => {
    e.preventDefault();
    const r = await post('payment', { studentId: id, amount: document.getElementById('amt').value.replace(/\s/g, '') });
    if (!r.ok) return toast(r.error, true);
    toast('Payment saved. Balance is now ' + money(r.balance) + ' UZS.' + (r.newAlerts ? ' ' + r.newAlerts + ' new alerts.' : ''));
    await refreshMe(); students(el);
  };
  const enrollBtn = document.getElementById('enrollBtn');
  if (enrollBtn) enrollBtn.onclick = async () => {
    const r = await post('enroll', { studentId: id, groupId: document.getElementById('grp').value });
    if (!r.ok) return toast(r.error, true);
    toast('Added to the group.'); students(el);
  };
  side.querySelectorAll('[data-leave]').forEach((b) => b.onclick = async () => {
    const r = await post('unenroll', { studentId: id, groupId: b.dataset.leave });
    if (!r.ok) return toast(r.error, true);
    toast('Removed from the group. Old lessons stay in the history.'); students(el);
  });
}

function addStudentForm(el) {
  state.adding = true;
  document.getElementById('side').innerHTML =
    '<form class="panel" id="newStudent"><h2>New student</h2><div id="formError"></div>' +
    field('name', 'Full name') + field('phone', 'Phone', 'Digits, optional + at the start') +
    field('parentName', 'Parent name') + field('parentPhone', 'Parent phone') +
    '<button class="primary">Save student</button></form>';
  document.getElementById('newStudent').onsubmit = async (e) => {
    e.preventDefault();
    const r = await post('add-student', formValues(e.target));
    if (!r.ok) { document.getElementById('formError').innerHTML = '<div class="error">' + esc(r.error) + '</div>'; return; }
    state.adding = false; state.sel = r.id;
    toast('Student added.'); students(el);
  };
}

function field(id, label, hint, type) {
  return '<div class="field"><label for="f_' + id + '">' + label + '</label><input id="f_' + id + '" name="' + id + '" type="' + (type || 'text') + '" required>' +
         (hint ? '<div class="hint">' + hint + '</div>' : '') + '</div>';
}
function formValues(form) {
  const out = {};
  new FormData(form).forEach((v, k) => { out[k] = v; });
  return out;
}

// ---------- Groups ----------
// One page per group holds everything about it: who is in it, the form that saves a lesson
// (with a visible date), and a box for every lesson with each student's status.

const WEEKDAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
function weekdayOf(date) { return WEEKDAYS[new Date(date + 'T00:00:00Z').getUTCDay()]; }
function addDays(date, n) {
  const d = new Date(date + 'T00:00:00Z');
  d.setUTCDate(d.getUTCDate() + n);
  return d.toISOString().slice(0, 10);
}

// Scheduled days (from the group's days) since its first saved lesson that have no lesson yet.
// A new group with no lessons only offers today, when today is a scheduled day.
function missedDays(g, today) {
  const done = new Set(g.history.map((h) => h.date));
  const first = g.history.length ? g.history[g.history.length - 1].date : today;
  const out = [];
  for (let day = today; day >= first; day = addDays(day, -1)) {
    if (g.days.includes(weekdayOf(day)) && !done.has(day)) out.push(day);
  }
  return out;
}

function lessonBox(h, order) {
  const rank = (s) => { const i = order.indexOf(s.id); return i === -1 ? 999 : i; };   // same order as the roster
  const byRoster = (a, b) => rank(a) - rank(b);
  const people = h.present.slice().sort(byRoster).map((s) => '<div class="stu"><span>' + esc(s.name) + '</span><span class="in">&#10003; Present</span></div>')
    .concat(h.absent.slice().sort(byRoster).map((s) => '<div class="stu"><span>' + esc(s.name) + '</span><span class="out">&#10007; Absent</span></div>')).join('');
  return '<article class="lesson"><header><b>' + esc(h.weekday) + ' ' + esc(h.date) + '</b><span class="muted">' + h.present.length + ' of ' +
    (h.present.length + h.absent.length) + ' came</span></header>' + people + '</article>';
}

async function groups(el) {
  const d = await api('groups');
  if (!d.ok) { el.innerHTML = '<div class="error">' + esc(d.error) + '</div>'; return; }
  const canAdd = me.role !== 'teacher';
  const addBtn = canAdd ? '<button id="addBtn" class="primary">Add group</button>' : '';

  if (!d.groups.length) {
    el.innerHTML = '<div class="head"><h1>Groups</h1></div><div class="row">' + addBtn + '</div><div id="side"></div>' +
      (canAdd ? '' : '<div class="panel muted">You have no groups yet.</div>');
    if (canAdd) document.getElementById('addBtn').onclick = () => addGroupForm(el);
    return;
  }

  if (!state.sel) state.sel = Number(location.hash.split('/')[1]) || 0;
  if (!d.groups.some((x) => x.id === state.sel)) state.sel = d.groups[0].id;
  const g = d.groups.find((x) => x.id === state.sel);
  const tabs = d.groups.map((x) => '<button class="tab' + (x.id === g.id ? ' on' : '') + '" data-id="' + x.id + '">' + esc(x.name) + '</button>').join('');

  el.innerHTML = '<div class="head"><h1>Groups</h1></div><div class="row tabs" style="margin-bottom:18px">' + tabs + addBtn + '</div><div id="detail"></div>';
  el.querySelectorAll('.tab').forEach((b) => b.onclick = () => { state.sel = Number(b.dataset.id); state.date = null; state.marks = null; groups(el); });
  if (canAdd) document.getElementById('addBtn').onclick = () => { document.getElementById('detail').innerHTML = '<div id="side"></div>'; addGroupForm(el); };

  groupDetail(el, g, d.today);
}

function groupDetail(el, g, today) {
  const missed = missedDays(g, today);
  const saved = new Set(g.history.map((h) => h.date));
  if (!state.date) state.date = missed.length ? missed[0] : today;   // the newest day still waiting for attendance
  const date = state.date;
  const key = g.id + '|' + date;
  if (!state.marks || state.marksKey !== key) {                      // everybody starts as present
    state.marks = {};
    g.students.forEach((s) => { state.marks[s.id] = true; });
    state.marksKey = key;
  }
  const alreadySaved = saved.has(date);
  const scheduled = g.days.includes(weekdayOf(date));
  const came = g.students.filter((s) => state.marks[s.id]).length;

  // the form
  let form;
  if (g.students.length === 0) {
    form = '<p class="muted">This group has no students yet. Add students to it from the Students page.</p>';
  } else {
    form = '<div class="field" style="max-width:240px"><label for="date">Lesson date</label><input id="date" type="date" value="' + esc(date) + '" max="' + esc(today) + '"></div>' +
      '<p>' + esc(weekdayOf(date)) + ' ' + (scheduled ? '<span class="chip">Scheduled day</span>' : '<span class="chip warn">Not a scheduled day (' + esc(g.days.join(', ')) + ')</span>') + '</p>' +
      (alreadySaved ? '<p class="muted">A lesson for this date is already saved. Its attendance is in the box below.</p>' :
        '<div class="tablewrap" style="margin:12px 0">' + g.students.map((s) =>
          '<div class="mark"><div>' + esc(s.name) + '</div><div class="seg" role="group" aria-label="' + esc(s.name) + '">' +
          '<button type="button" class="present' + (state.marks[s.id] ? ' on' : '') + '" data-s="' + s.id + '" data-v="1">Present</button>' +
          '<button type="button" class="absent' + (!state.marks[s.id] ? ' on' : '') + '" data-s="' + s.id + '" data-v="0">Absent</button></div></div>').join('') + '</div>' +
        '<div class="savebar"><button id="saveBtn" class="primary">Save ' + esc(weekdayOf(date)) + ' ' + esc(date) + '</button><span class="muted">' + came + ' present, ' +
        (g.students.length - came) + ' absent. Charges all ' + g.students.length + ' students ' + money(g.price) + ' UZS each.</span></div>');
  }

  // the roster
  const roster = g.students.length ? g.students.map((s) =>
    '<div class="stu">' + person(s.name, s.id) + '<span class="muted">' + (s.attendance < 0 ? 'new' : s.attendance + '%') + '</span></div>').join('') :
    '<p class="muted">Nobody yet.</p>';

  // the lesson boxes: saved lessons plus dashed boxes for scheduled days nobody marked
  const items = g.history.map((h) => ({ date: h.date, html: lessonBox(h, g.students.map((s) => s.id)) }))
    .concat(missed.map((m) => ({ date: m, html: '<article class="lesson missing"><header><b>' + esc(weekdayOf(m)) + ' ' + esc(m) + '</b></header><p class="muted">Not marked</p>' +
      '<button data-mark="' + m + '">Mark this day</button></article>' })));
  items.sort((a, b) => (a.date < b.date ? 1 : -1));

  document.getElementById('detail').innerHTML =
    '<div class="ghead"><h2>' + esc(g.name) + '</h2><span class="chip">' + esc(g.days.join(', ')) + '</span><span class="muted">Teacher ' + esc(g.teacher) +
    '</span><span class="muted">' + money(g.price) + ' UZS per lesson</span></div>' +
    '<div class="split"><section class="panel"><h2>Save a lesson</h2>' + form + '</section><section class="panel"><h2>Students (' + g.students.length + ')</h2>' + roster + '</section></div>' +
    '<h2 style="margin-top:28px">Lessons</h2>' +
    (items.length ? '<div class="lessons">' + items.map((i) => i.html).join('') + '</div>' : '<div class="panel muted">No lessons yet. The first one you save will appear here.</div>');

  const dateInput = document.getElementById('date');
  if (dateInput) dateInput.onchange = () => { if (dateInput.value) { state.date = dateInput.value; groupDetail(el, g, today); } };
  el.querySelectorAll('.seg button').forEach((b) => b.onclick = () => { state.marks[b.dataset.s] = b.dataset.v === '1'; groupDetail(el, g, today); });
  el.querySelectorAll('[data-mark]').forEach((b) => b.onclick = () => {
    state.date = b.dataset.mark;
    groupDetail(el, g, today);
    document.getElementById('date').scrollIntoView({ behavior: 'smooth', block: 'center' });
  });
  const save = document.getElementById('saveBtn');
  if (save) save.onclick = async () => {
    save.disabled = true;
    const present = g.students.filter((s) => state.marks[s.id]).map((s) => s.id).join(',');
    const r = await post('attendance', { groupId: g.id, date, present });
    if (!r.ok) { save.disabled = false; return toast(r.error, true); }
    toast('Lesson saved for ' + date + '.' + (r.newAlerts ? ' ' + r.newAlerts + ' new alerts sent.' : ''));
    state.date = null; state.marks = null;
    groups(el);
  };
}

async function addGroupForm(el) {
  const a = await api('accounts');   // only a director may list accounts; a manager types the teacher id
  const teachers = a.ok ? a.accounts.filter((x) => x.role === 'teacher') : null;
  document.getElementById('side').innerHTML =
    '<form class="panel stack" id="newGroup"><h2>New group</h2><div id="formError"></div>' + field('name', 'Group name') +
    '<div class="field"><label for="f_teacherId">Teacher</label>' +
    (teachers ? '<select id="f_teacherId" name="teacherId">' + teachers.map((t) => '<option value="' + t.id + '">' + esc(t.name) + '</option>').join('') + '</select>' :
      '<input id="f_teacherId" name="teacherId" inputmode="numeric" required><div class="hint">The teacher\'s account id</div>') + '</div>' +
    field('price', 'Price per lesson (UZS)') + field('days', 'Days', 'For example Mon,Wed,Fri') +
    '<button class="primary">Save group</button></form>';
  document.getElementById('newGroup').onsubmit = async (e) => {
    e.preventDefault();
    const r = await post('add-group', formValues(e.target));
    if (!r.ok) { document.getElementById('formError').innerHTML = '<div class="error">' + esc(r.error) + '</div>'; return; }
    state.sel = r.id; state.date = null; toast('Group added.'); groups(el);
  };
}

// ---------- Payments ----------

async function payments(el) {
  const d = await api('students');
  el.innerHTML = '<div class="head"><h1>Payments</h1></div><form class="stack panel" id="payForm"><div id="formError"></div>' +
    '<div class="field"><label for="st">Student</label><select id="st">' +
    d.students.map((s) => '<option value="' + s.id + '">' + esc(s.name) + ' (balance ' + money(s.balance) + ')</option>').join('') + '</select></div>' +
    '<div class="field"><label for="amt">Amount in UZS</label><input id="amt" inputmode="numeric" required></div>' +
    '<button class="primary">Save payment</button></form>';
  document.getElementById('payForm').onsubmit = async (e) => {
    e.preventDefault();
    const r = await post('payment', { studentId: document.getElementById('st').value, amount: document.getElementById('amt').value.replace(/\s/g, '') });
    if (!r.ok) { document.getElementById('formError').innerHTML = '<div class="error">' + esc(r.error) + '</div>'; return; }
    toast('Payment saved. Balance is now ' + money(r.balance) + ' UZS.' + (r.newAlerts ? ' ' + r.newAlerts + ' new alerts.' : ''));
    payments(el);
  };
}

// ---------- Inbox ----------

const TYPE_NAMES = { low_balance: 'Low balance', debt: 'Debt', absence_streak: 'Absences', low_attendance: 'Attendance', debt_milestone: 'Debt count' };

async function inbox(el) {
  const d = await api('inbox', { markRead: 1 });   // the answer still tells which messages were new
  me.unread = 0;
  el.innerHTML = '<div class="head"><h1>Inbox</h1></div>' +
    (d.messages.length ? '<div class="tablewrap">' + d.messages.map((m) =>
      '<div class="msg' + (m.read ? '' : ' new') + '"><div style="flex:1"><span class="chip ' + (m.type === 'debt' ? 'bad' : m.type === 'low_balance' ? 'warn' : '') + '">' +
      esc(TYPE_NAMES[m.type] || m.type) + '</span> ' + esc(m.text) + '</div><time>' + esc(m.date) + '</time></div>').join('') + '</div>' :
      '<div class="panel muted">No messages. Alerts appear here after payments and lessons.</div>');
  const badge = document.querySelector('.rail .badge');
  if (badge) badge.remove();
}

// ---------- Reports ----------

async function reports(el) {
  const month = state.month || (new Date().toISOString().slice(0, 7));
  const d = await api('report', { month });
  el.innerHTML = '<div class="head"><h1>Reports</h1></div>' +
    '<div class="row" style="margin-bottom:18px"><input id="mon" type="month" value="' + esc(month) + '" style="max-width:200px" aria-label="Month">' +
    (d.ok ? '<a class="btn" href="/api/report-file?month=' + esc(month) + '" style="text-decoration:none">Download CSV</a>' : '') + '</div>' +
    (!d.ok ? '<div class="error">' + esc(d.error) + '</div>' :
      '<div class="sum"><div><b>' + money(d.revenue) + '</b>UZS received in ' + esc(d.month) + '</div><div><b>' + money(d.debt) + '</b>UZS debt right now</div></div>' +
      '<h2>Attendance by group</h2>' + table(['Group', 'Lessons', 'Present', 'Absent', 'Attendance'],
        d.groups.map((g) => '<tr><td>' + esc(g.name) + '</td><td class="num">' + g.lessons + '</td><td class="num">' + g.present + '</td><td class="num">' + g.absent +
          '</td><td class="num">' + (g.percent < 0 ? 'n/a' : g.percent + '%') + '</td></tr>'), ['l', 'r', 'r', 'r', 'r']));
  document.getElementById('mon').onchange = (e) => { if (e.target.value) { state.month = e.target.value; reports(el); } };
}

// ---------- Accounts (director) ----------

async function accounts(el) {
  const d = await api('accounts');
  el.innerHTML = '<div class="head"><h1>Accounts</h1></div><div class="split"><div>' +
    table(['Name', 'Username', 'Role'], d.accounts.map((a) => '<tr><td>' + esc(a.name) + '</td><td>' + esc(a.username) + '</td><td>' + esc(a.role) + '</td></tr>'), ['l', 'l', 'l']) +
    '</div><form class="panel" id="newAcc"><h2>New account</h2><div id="formError"></div>' +
    field('fullName', 'Full name') + field('username', 'Username') + field('password', 'Password', 'At least 4 characters', 'password') +
    '<div class="field"><label for="f_role">Role</label><select id="f_role" name="role"><option>teacher</option><option>manager</option><option>director</option></select></div>' +
    '<button class="primary">Create account</button></form></div>';
  document.getElementById('newAcc').onsubmit = async (e) => {
    e.preventDefault();
    const r = await post('create-account', formValues(e.target));
    if (!r.ok) { document.getElementById('formError').innerHTML = '<div class="error">' + esc(r.error) + '</div>'; return; }
    toast('Account created.'); accounts(el);
  };
}

// ---------- start: is there already a session? ----------

(async () => {
  const data = await api('me');
  if (data.ok) me = data.account;
  render();
})();
