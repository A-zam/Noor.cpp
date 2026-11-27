@echo off
rem Builds the C++ engine, then starts the web server. Run from anywhere: it moves to the project root first.
cd /d "%~dp0.."
g++ -std=c++17 -Isrc web/engine/api.cpp src/accounts.cpp src/alerts.cpp src/attendance.cpp src/daily.cpp src/reports.cpp src/storage.cpp src/students.cpp -o web/engine/engine.exe
if errorlevel 1 (
  echo Engine build failed.
  exit /b 1
)
echo Open http://localhost:3000   (Ctrl+C to stop)
node web/server.js
