@echo off
rem Create a long-lived Claude token (claude setup-token) and store it for the
rem daemon, so the display no longer depends on the Claude Code login refresh.
rem Double-click from Explorer. Renew about once a year (the daemon log warns).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\set_claude_token_windows.ps1"
echo.
pause
