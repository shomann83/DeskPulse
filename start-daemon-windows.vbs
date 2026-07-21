' DeskPulse daemon launcher for Windows.
' Starts the BLE usage daemon hidden (no console window) and appends all
' output to %LOCALAPPDATA%\DeskPulse\daemon.log. Registered as a logon task.
Option Explicit

Dim sh, fso, q, daemonDir, py, logDir, logFile, cmd
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
q = Chr(34)

' Resolve the daemon dir relative to this script (repo root), so the file is
' portable and carries no hard-coded user path.
daemonDir = fso.GetParentFolderName(WScript.ScriptFullName) & "\daemon"
py = daemonDir & "\.venv\Scripts\python.exe"

logDir = sh.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\DeskPulse"
If Not fso.FolderExists(logDir) Then fso.CreateFolder(logDir)
logFile = logDir & "\daemon.log"

sh.CurrentDirectory = daemonDir
cmd = "cmd /c " & q & q & py & q & " claude_usage_daemon.py --provider both >> " & q & logFile & q & " 2>&1" & q

' 0 = hidden window, False = do not wait (daemon runs forever)
sh.Run cmd, 0, False
