#!/usr/bin/env python3
"""Send a theme exported by the web Theme Studio to the board over USB.

Usage:
    python3 tools/send_theme.py <serial-port> <theme.json>

Example (macOS):
    python3 tools/send_theme.py /dev/cu.usbmodem1101 ~/Downloads/mfd_theme.json

The board answers "theme: loaded and saved" and keeps the theme across restarts.
Requires pyserial:  python3 -m pip install pyserial
"""
import json
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial is missing: python3 -m pip install pyserial")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    port, path = sys.argv[1], sys.argv[2]
    with open(path, encoding="utf-8") as f:
        theme = json.load(f)                     # validate before sending
    payload = "theme " + json.dumps(theme, separators=(",", ":")) + "\n"
    with serial.Serial(port, 115200, timeout=0.2) as s:
        time.sleep(0.3)
        s.reset_input_buffer()
        for i in range(0, len(payload), 256):    # small chunks: the USB CDC buffer is finite
            s.write(payload[i:i + 256].encode("utf-8"))
            s.flush()
            time.sleep(0.02)
        deadline = time.time() + 3
        reply = b""
        while time.time() < deadline:
            reply += s.read(256)
            if b"theme:" in reply and reply.endswith(b"\n"):
                break
    text = reply.decode("utf-8", "replace").strip()
    print(text or "no answer from the board (wrong port, or the firmware is not running?)")
    return 0 if "loaded" in text else 1


if __name__ == "__main__":
    sys.exit(main())
