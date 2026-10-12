"""Grab the device framebuffer over USB serial and save it as a PNG.

Windows counterpart to screenshot.sh, without the ffmpeg dependency.
Usage: python tools/screenshot_windows.py [output.png] [COMx]
Runs with PlatformIO's bundled Python (it ships pyserial).
"""
import struct
import sys
import zlib

import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A


def find_port() -> str:
    for p in list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    sys.exit("No ESP32-S3 found on USB (VID 0x303A)")


def capture(port_name: str) -> tuple[int, int, bytes]:
    port = serial.Serial(port_name, 115200, timeout=10)
    port.reset_input_buffer()
    port.write(b"screenshot\n")
    port.flush()
    while True:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if not line:
            sys.exit("Timeout waiting for SCREENSHOT_START")
        if line.startswith("SCREENSHOT_START"):
            _, w, h, size = line.split()
            w, h, size = int(w), int(h), int(size)
            break
        if line == "SCREENSHOT_ERR":
            sys.exit("Device reported screenshot error")
    data = bytearray()
    while len(data) < size:
        chunk = port.read(min(4096, size - len(data)))
        if not chunk:
            sys.exit(f"Timeout: got {len(data)} of {size} bytes")
        data += chunk
    port.close()
    return w, h, bytes(data)


def rgb565le_to_png(w: int, h: int, raw: bytes) -> bytes:
    rows = bytearray()
    for y in range(h):
        rows.append(0)  # filter: none
        for x in range(w):
            v = raw[2 * (y * w + x)] | (raw[2 * (y * w + x) + 1] << 8)
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            rows += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(tag: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 9))
            + chunk(b"IEND", b""))


def main() -> None:
    out = sys.argv[1] if len(sys.argv) > 1 else "screenshot.png"
    port = sys.argv[2] if len(sys.argv) > 2 else find_port()
    print(f"Taking screenshot from {port}...")
    w, h, raw = capture(port)
    with open(out, "wb") as f:
        f.write(rgb565le_to_png(w, h, raw))
    print(f"Saved: {out} ({w}x{h})")


if __name__ == "__main__":
    main()
