// server.js - the web server of LearNoor. Plain Node.js, no packages to install.
//
// Its only jobs:
//   1. serve the files in web/public (the pages the browser shows),
//   2. keep login sessions (who is logged in),
//   3. pass each request to the C++ engine and send its JSON answer back.
// The rules (charging, alerts, reports...) are NOT here: they are in the C++ modules.
//
// Run from the project root:  node web/server.js   then open http://localhost:3000

const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { execFile } = require('child_process');

const PORT = 3000;
const ROOT = path.join(__dirname, '..');                       // the project folder
const PUBLIC = path.join(__dirname, 'public');
const ENGINE = path.join(__dirname, 'engine', process.platform === 'win32' ? 'engine.exe' : 'engine');

const sessions = new Map();   // session id -> { id, role }

// ---------- talking to the C++ engine ----------

// The engine loads the data files, does one thing, saves and exits.
// Requests wait in a line so two of them never write the files at the same moment.
let line = Promise.resolve();

function callEngine(command, params) {
  const job = line.then(() => new Promise((resolve) => {
    // the parameters travel on stdin as "key=value" lines (safe for any characters, UTF-8 included)
    const input = Object.entries(params)
      .map(([k, v]) => k + '=' + String(v).replace(/[\r\n]+/g, ' '))
      .join('\n');
    const child = execFile(ENGINE, [command], { cwd: ROOT, encoding: 'utf8' }, (err, stdout) => {
      try {
        resolve(JSON.parse(stdout));
      } catch (e) {
        resolve({ ok: false, error: 'The engine did not answer. Build it first (see docs).' });
      }
    });
    child.stdin.end(input);
  }));
  line = job;
  return job;
}

// ---------- helpers ----------

function readBody(req) {
  return new Promise((resolve) => {
    let body = '';
    req.on('data', (chunk) => { if (body.length < 100000) body += chunk; });
    req.on('end', () => {
      try { resolve(JSON.parse(body || '{}')); } catch (e) { resolve({}); }
    });
  });
}

function getCookie(req, name) {
  const header = req.headers.cookie || '';
  for (const part of header.split(';')) {
    const [k, v] = part.trim().split('=');
    if (k === name) return v;
  }
  return '';
}

function sendJson(res, status, data, extraHeaders) {
  res.writeHead(status, Object.assign({ 'Content-Type': 'application/json; charset=utf-8' }, extraHeaders));
  res.end(JSON.stringify(data));
}

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8', '.svg': 'image/svg+xml', '.woff2': 'font/woff2', '.ttf': 'font/ttf',
};

function sendFile(res, urlPath) {
  const name = urlPath === '/' ? '/index.html' : urlPath;
  const file = path.normalize(path.join(PUBLIC, name));
  if (!file.startsWith(PUBLIC)) { res.writeHead(403); return res.end('Forbidden'); }   // no ../ tricks
  fs.readFile(file, (err, data) => {
    if (err) { res.writeHead(404); return res.end('Not found'); }
    res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream' });
    res.end(data);
  });
}

// ---------- the API ----------

async function handleApi(req, res, url) {
  const command = url.pathname.slice('/api/'.length);
  const sid = getCookie(req, 'sid');
  const params = req.method === 'POST' ? await readBody(req) : Object.fromEntries(url.searchParams);

  if (command === 'login') {
    const answer = await callEngine('login', { username: params.username || '', password: params.password || '' });
    if (!answer.ok) return sendJson(res, 401, answer);
    const token = crypto.randomBytes(24).toString('hex');
    sessions.set(token, { id: answer.account.id });
    return sendJson(res, 200, answer, { 'Set-Cookie': `sid=${token}; HttpOnly; SameSite=Strict; Path=/` });
  }

  if (command === 'logout') {
    sessions.delete(sid);
    return sendJson(res, 200, { ok: true }, { 'Set-Cookie': 'sid=; Max-Age=0; Path=/' });
  }

  const session = sessions.get(sid);
  if (!session) return sendJson(res, 401, { ok: false, error: 'Please log in.' });

  // the account always comes from the session, never from what the browser sends
  params.as = session.id;

  if (command === 'report-file') {   // download the CSV
    const answer = await callEngine('report', params);
    if (!answer.ok || !answer.file) return sendJson(res, 400, answer);
    const csv = fs.readFileSync(path.join(ROOT, answer.file));
    res.writeHead(200, {
      'Content-Type': 'text/csv; charset=utf-8',
      'Content-Disposition': `attachment; filename="${path.basename(answer.file)}"`,
    });
    return res.end(csv);
  }

  const answer = await callEngine(command, params);
  sendJson(res, answer.ok ? 200 : 400, answer);
}

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname.startsWith('/api/')) {
    handleApi(req, res, url).catch(() => sendJson(res, 500, { ok: false, error: 'Server error.' }));
  } else {
    sendFile(res, url.pathname);
  }
}).listen(PORT, '127.0.0.1', () => {
  console.log('LearNoor is running: http://localhost:' + PORT);
});
