"""Turn the watch face pictures in a render-check log into PNG files.

Build the render check with the pictures switched on, record its serial log,
then decode it here:

    PLATFORMIO_BUILD_FLAGS=-DLAUNCHER_RENDER_SHOTS pio run -e m5stopwatch-render-check
    (install it as usual, then)
    python tools/capture_serial.py COM11 120 <log> reset
    python tools/render_shots.py <log> <output directory>

The firmware prints each frame as run-length pairs of RGB565 pixels as read
back from the panel ("[Shot]", "[ShotData]", "[ShotEnd]", see
src/host/RenderDiagnostics.cpp). "[ShotProbe]" says how the readback orders the
bytes. Needs no third party module.

Usage: python tools/render_shots.py <log> <output directory>
"""
import base64
from pathlib import Path
import struct
import sys
import zlib


def png(path, width, height, rgb):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    rows = b"".join(b"\x00" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def decode(width, height, data, swapped):
    rgb = bytearray()
    for i in range(0, len(data) - 2, 3):
        run = data[i] + 1
        value = data[i + 1] << 8 | data[i + 2]
        if swapped:
            value = (value >> 8) | ((value & 0xFF) << 8)
        r, g, b = value >> 11, (value >> 5) & 63, value & 31
        rgb += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31)) * run
    if len(rgb) != width * height * 3:
        raise SystemExit(f"picture decodes to {len(rgb) // 3} pixels, expected {width * height}")
    return bytes(rgb)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    log, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    swapped, current, lines = False, None, []
    for raw in log.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("[ShotProbe]"):
            swapped = line.split("red=")[1].strip().lower() != "f800"
        elif line.startswith("[Shot] "):
            name, width, height = line.split()[1:4]
            current, lines = (name, int(width), int(height)), []
        elif line.startswith("[ShotData] ") and current:
            lines.append(line.split(" ", 1)[1])
        elif line.startswith("[ShotEnd]") and current:
            name, width, height = current
            path = out / f"{name}.png"
            png(path, width, height, decode(width, height, base64.b64decode("".join(lines)), swapped))
            print(f"[Shots] {path}")
            current = None
    return 0


if __name__ == "__main__":
    sys.exit(main())
