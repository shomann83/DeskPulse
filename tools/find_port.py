"""Print the COM port of the attached ESP32-S3 (Espressif USB VID 0x303A).

Exit code 1 and a list of all serial ports on stderr if none is found.
"""
import sys

from serial.tools import list_ports

ports = list(list_ports.comports())
for p in ports:
    if p.vid == 0x303A:
        print(p.device)
        sys.exit(0)

print("Kein ESP32-S3 gefunden. Vorhandene serielle Ports:", file=sys.stderr)
for p in ports:
    print(f"  {p.device}: {p.description} (VID {p.vid})", file=sys.stderr)
if not ports:
    print("  (keine)", file=sys.stderr)
sys.exit(1)
