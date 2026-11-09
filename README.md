# DeskPulse

ESP32 desk-side usage monitor for Claude Code and ChatGPT / Codex. A small
Waveshare AMOLED board sits on your desk and shows how much of your rate limits
you have left, updated live over Bluetooth from a background daemon on your
computer.

This is a **personal fork** focused on **Windows support**. It is maintained as
a separate multi-provider fork of
[HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter),
by way of [JeongJaeSoon/DeskPulse](https://github.com/JeongJaeSoon/DeskPulse).
The GitHub fork relationship is intentionally retained. See
[UPSTREAM.md](UPSTREAM.md) for origin history and [NOTICE.md](NOTICE.md) for
license and third-party asset notes.

## What this fork adds

- **Windows support** — a hidden background daemon plus a logon autostart task,
  so the device stays live without keeping a terminal open.
- **Length-based rate-limit windows** — usage windows are matched by their
  advertised `limit_window_seconds` instead of the position they arrive in, so
  a provider adding or dropping a window relabels the device instead of
  mislabelling it. OpenAI removed the 5h window in July 2026; ChatGPT now
  reports a weekly limit only, and the freed slot shows the credit balance.
- **Screen auto-cycle** — screens rotate on a timer so the device is glanceable
  without touching it.
- **Provider-accurate icons** — the ChatGPT screen uses the OpenAI mark and is
  labelled "ChatGPT"; the Claude screens use the Claude logo.

> Some runtime names still use the original `Claude Controller` and
> `claude-usage-daemon` identifiers for compatibility with existing BLE pairing
> and service setup.

## Supported hardware

- [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=149786)
- [Waveshare ESP32-C6-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm?&aff_id=149786)
- [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm?&aff_id=149786)

Board env names:

- `waveshare_amoled_216`
- `waveshare_amoled_18`

Use a real USB **data** cable — a charge-only cable powers the board but does
not expose a serial port.

## Requirements

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html)
- Python 3 (for the daemon and, on Windows/macOS, for screenshots)
- Claude Code login for Claude usage
- ChatGPT / Codex login for ChatGPT usage
- Linux only: `curl`, `bluetoothctl`, `busctl`

If `pio` is not on your PATH, use the bundled copy:

- Windows: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`
- macOS / Linux: `~/.platformio/penv/bin/pio`

## Windows setup

Run PlatformIO commands from **PowerShell**, not Git Bash / MSYS — the toolchain
install can break under MSYS.

### 1. Flash firmware

Find the board's COM port (Device Manager, or `Get-CimInstance Win32_SerialPort`),
then flash:

```powershell
pio run -d firmware -e waveshare_amoled_216 -t upload --upload-port COM4
```

If the board is not detected, hold **BOOT** and tap **RESET** once to force
download mode, then retry.

### 2. Set up the daemon

The device connects over BLE automatically — no manual Windows pairing needed.

```powershell
python -m venv daemon\.venv
daemon\.venv\Scripts\python.exe -m pip install bleak httpx
daemon\.venv\Scripts\python.exe daemon\claude_usage_daemon.py --provider both
```

Leave it running and the device starts updating. See
[Provider selection](#provider-selection) for `claude` / `codex` / `both`.

### 3. Autostart on logon

`start-daemon-windows.vbs` launches the daemon hidden and appends output to
`logs\daemon.log` in the repository (gitignored). Register it as a logon task:

1. Open **Task Scheduler** and create a task that runs at logon.
2. Action: start a program → `wscript.exe`, argument: the full path to
   `start-daemon-windows.vbs`.

The script resolves the daemon directory relative to its own location, so keep
`start-daemon-windows.vbs` in the repository root.

### 4. Long-lived Claude token (recommended)

By default the daemon reads the Claude Code login from
`%USERPROFILE%\.claude\.credentials.json` and refreshes it when it expires.
That refresh can be rejected (HTTP 400) — the Claude panel then stays empty
until you run `/login` again. A token from `claude setup-token` lasts about a
year and needs no refresh:

1. Double-click `set-claude-token-windows.bat`. It runs `claude setup-token`
   (browser approval) and asks you to paste the token.
2. The token is stored outside the repository in
   `%USERPROFILE%\.config\claude-usage-monitor\claude-token` (current user only).
3. The daemon picks it up on its next poll; `logs\daemon.log` shows
   `Claude auth: long-lived token (...)`.

Alternatively set the environment variable `DESKPULSE_CLAUDE_TOKEN`; it takes
precedence over the file. If the API rejects the token, the daemon logs it and
falls back to the Claude Code login. After about 330 days it logs a renewal
reminder — run the `.bat` again.

### Update in one step

Double-click `flash-windows.bat`: it builds and flashes the firmware
(`waveshare_amoled_216`), restarts the daemon and saves a device screenshot to
`logs\screenshot.png` (via `tools\screenshot_windows.py`).

## macOS setup

### 1. Flash firmware

```bash
./flash-mac.sh waveshare_amoled_216
./flash-mac.sh waveshare_amoled_18 /dev/cu.usbmodem1101
```

The first command auto-detects `/dev/cu.usbmodem*`. Pass the serial port
explicitly if more than one device is connected.

### 2. Pair Bluetooth

Open **System Settings → Bluetooth** and connect to `Claude Controller`. The
daemon discovers the device by that BLE name.

### 3. Install daemon

```bash
./install-mac.sh
```

The installer creates `daemon/.venv/`, installs `bleak` and `httpx`, creates
`daemon/config.toml` if missing, installs
`~/Library/LaunchAgents/com.user.claude-usage-daemon.plist`, and starts the
LaunchAgent. On first run, allow the Bluetooth permission prompt.

### 4. Check logs

```bash
tail -F ~/Library/Logs/claude-usage-daemon.out.log
```

Useful service commands:

```bash
launchctl list | grep claude-usage
launchctl unload ~/Library/LaunchAgents/com.user.claude-usage-daemon.plist
launchctl load -w ~/Library/LaunchAgents/com.user.claude-usage-daemon.plist
```

## Linux setup

The Linux installer currently uses the legacy Claude-only shell daemon.

### 1. Flash firmware

```bash
./flash.sh waveshare_amoled_216
./flash.sh waveshare_amoled_18 /dev/ttyACM1
```

### 2. Pair Bluetooth

```bash
bluetoothctl scan le
bluetoothctl pair <MAC>
bluetoothctl trust <MAC>
```

Pair the device named `Claude Controller`. The MAC address is also shown on the
Bluetooth screen.

### 3. Install daemon

```bash
./install.sh
systemctl --user start claude-usage-daemon
```

Useful service commands:

```bash
systemctl --user status claude-usage-daemon
journalctl --user -u claude-usage-daemon -f
systemctl --user restart claude-usage-daemon
systemctl --user stop claude-usage-daemon
```

## Provider selection

Choose which usage data the device shows: `claude`, `codex`, or `both`.

On macOS a helper script writes the setting to `daemon/config.toml`:

```bash
./switch-provider.sh both
```

On any platform you can set it directly. Priority, highest first:

1. CLI flag: `--provider both`
2. Environment variable: `CLAWDMETER_PROVIDER=both`
3. Config file: `daemon/config.toml`
4. Built-in fallback: `claude`

In `both` mode a healthy daemon payload looks like this:

```json
{"p":"both",
 "c":{"s":58,"sr":244,"w":19,"wr":7764,"st":"allowed","ok":true,"sw":18000,"ww":604800},
 "x":{"s":0,"sr":0,"w":4,"wr":9923,"st":"allowed","ok":true,"sw":0,"ww":604800,
      "ch":false,"cb":"0","cm":0}}
```

Per provider: `s`/`sr` short window percentage and minutes to reset, `w`/`wr` the
same for the weekly window, `st` status, `ok` parse success. `sw`/`ww` give each
window's length in seconds — **`0` means the provider has no such window**, which
is what tells the firmware to show credits instead of a phantom `0%`. ChatGPT
additionally reports `ch` (has credits), `cb` (balance) and `cm` (approximate
messages left).

Payloads go out as BLE writes without response, so they must stay under the
negotiated ATT limit (252 bytes at NimBLE's default MTU of 255). The daemon logs
a warning if a payload would exceed it rather than letting it be cut silently.

## Screen controls

Main screen cycle:

```text
Usage → Claude → ChatGPT → Bluetooth → Usage
```

Screens also rotate automatically on a timer (auto-cycle); a button press
pauses the rotation for one round.

Buttons:

| Button | Function |
| ------ | -------- |
| Left | Hold to send Space |
| Middle / PWR | Cycle screens; on splash, cycle animations |
| Right | Press to send Shift+Tab |

Tap the screen to toggle between splash and the last non-splash screen.

## Development references

- Board porting: [docs/porting/adding-a-board.md](docs/porting/adding-a-board.md)
- HAL contract: [docs/porting/hal-contract.md](docs/porting/hal-contract.md)
- Upstream origin: [UPSTREAM.md](UPSTREAM.md)
- License and third-party notices: [NOTICE.md](NOTICE.md)
