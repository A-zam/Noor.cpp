@echo off
rem Build and run the tests. Run from the project root:  tests\run_tests.bat
g++ -std=c++17 -Wall -Wextra -Isrc tests/test.cpp src/storage.cpp src/accounts.cpp src/students.cpp src/attendance.cpp src/alerts.cpp src/daily.cpp src/reports.cpp src/table.cpp -o test.exe
if errorlevel 1 exit /b 1
.\test.exe
