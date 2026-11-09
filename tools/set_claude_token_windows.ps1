# Stores a long-lived Claude Code OAuth token for the DeskPulse daemon.
# Run via set-claude-token-windows.bat (double-click). The token never touches
# the repo: it goes to %USERPROFILE%\.config\claude-usage-monitor\claude-token,
# readable only by the current user. The daemon picks it up on its next poll.
$ErrorActionPreference = 'Stop'

$tokenDir  = Join-Path $env:USERPROFILE '.config\claude-usage-monitor'
$tokenFile = Join-Path $tokenDir 'claude-token'

# The standalone CLI; the Desktop app's `claude` shim has been broken before.
$claude = Join-Path $env:USERPROFILE '.local\bin\claude.exe'
if (-not (Test-Path $claude)) { $claude = 'claude' }

Write-Host '=== 1/2 Create a long-lived token (about one year) ==='
Write-Host 'A browser window opens. Approve, then copy the token shown at the end (sk-ant-oat01-...).'
Write-Host ''
& $claude setup-token
Write-Host ''

Write-Host '=== 2/2 Store the token for the daemon ==='
$secure = Read-Host 'Paste the token (input is hidden)' -AsSecureString
$token = ([System.Net.NetworkCredential]::new('', $secure).Password).Trim()
if (-not $token) {
    Write-Host 'No token entered - nothing changed.' -ForegroundColor Yellow
    exit 1
}
if ($token -notlike 'sk-ant-*') {
    $answer = Read-Host 'This does not look like a Claude token (expected sk-ant-...). Store anyway? (y/n)'
    if ($answer -notmatch '^(y|j)') { Write-Host 'Aborted - nothing changed.'; exit 1 }
}

New-Item -ItemType Directory -Force -Path $tokenDir | Out-Null
[System.IO.File]::WriteAllText($tokenFile, $token, [System.Text.UTF8Encoding]::new($false))
# Current user only; drop inherited ACLs.
icacls $tokenFile /inheritance:r /grant:r "$($env:USERNAME):(R,W)" | Out-Null

Write-Host ''
Write-Host "Stored: $tokenFile" -ForegroundColor Green
Write-Host 'The daemon uses it from its next poll (within 60 s); no restart needed.'
Write-Host 'Check logs\daemon.log for "Claude auth: long-lived token".'
