@echo off
rem Build + flash the firmware, restart the daemon, then grab a screenshot.
rem Double-click from Explorer. Output and screenshot land in logs\.
setlocal
rem pio writes Unicode progress bars; without UTF-8 the redirect to a
rem log file crashes with UnicodeEncodeError (cp1252) and the upload hangs.
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
cd /d "%~dp0"
if not exist logs mkdir logs

set "PIO=pio"
set "PIOPY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
where pio >nul 2>nul || set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"

echo === 1/3 Firmware bauen und flashen ===
set "PORT="
for /f "delims=" %%P in ('call "%PIOPY%" tools\find_port.py') do set "PORT=%%P"
if not defined PORT (
    "%PIOPY%" tools\find_port.py > logs\port.log 2>&1
    type logs\port.log
    echo.
    echo Board nicht gefunden - USB-Kabel pruefen ^(Datenkabel, nicht nur Ladekabel^).
    pause
    exit /b 1
)
echo Board an %PORT% - bauen und flashen dauert ca. 1-2 Minuten, bitte warten...
"%PIO%" run -d firmware -e waveshare_amoled_216 -t upload --upload-port %PORT% > logs\flash.log 2>&1
if errorlevel 1 (
    type logs\flash.log
    echo.
    echo FEHLER beim Bauen/Flashen - Details in logs\flash.log
    pause
    exit /b 1
)
echo OK

echo === 2/3 Daemon neu starten ===
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | Where-Object CommandLine -like '*claude_usage_daemon*' | ForEach-Object { Stop-Process -Id $_.ProcessId }"
wscript "%~dp0start-daemon-windows.vbs"
echo OK - warte 90 s auf die ersten Daten...
timeout /t 90 /nobreak >nul

echo === 3/3 Screenshot ===
"%PIOPY%" tools\screenshot_windows.py logs\screenshot.png %PORT% > logs\screenshot.log 2>&1
type logs\screenshot.log
echo.
echo Fertig. Fenster schliesst in 10 s.
timeout /t 10 >nul
