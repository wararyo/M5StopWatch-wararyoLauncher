"""Build the embedded app list icon set from the PNG masks in `icons/`.

The list draws an icon as a filled circle plus an 8 bit coverage mask pushed
with `pushGrayscaleImage`: 0 keeps the circle colour, 255 is the white glyph.
That is one byte per pixel and no runtime decoding, so scrolling rows stay cheap
compared with decoding a PNG on every repaint.

The masks are 44x44 so that the square fits inside the unselected radius 32
circle (the far corner sits 31.1px from the centre); see docs/task3/rendering.md.

Needs no third party module: the PNG reader here handles the 8 bit
non-interlaced files this repository stores. Run it when an icon changes; the
build itself uses the committed `src/ui/icons/AppIcons.bin`.

Usage: python tools/build_icons.py [--check]
"""
import argparse
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "icons"
OUTPUT = ROOT / "src/ui/icons/AppIcons.bin"
MAGIC = b"LICN"
VERSION = 1
HEADER = struct.Struct("<4s4H")
SIZE = 44

# Order and meaning of `launcher::IconId` in src/app/AppRegistry.h. The firmware
# indexes this file by that enum, so entries are appended, never reordered.
ICONS = ("ic_stopwatch.png", "ic_settings.png", "ic_app1.png", "ic_app2.png", "ic_app3.png")

CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def chunks(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a PNG file")
    offset = 8
    while offset < len(data):
        length, kind = struct.unpack_from(">I4s", data, offset)
        yield kind, data[offset + 8:offset + 8 + length]
        offset += length + 12


def unfilter(raw, width, height, channels):
    """Reverse the per scanline PNG filters; 8 bit samples only."""
    stride = width * channels
    out, previous, offset = bytearray(), bytearray(stride), 0
    for _ in range(height):
        method, line = raw[offset], bytearray(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            up = previous[x]
            upleft = previous[x - channels] if x >= channels else 0
            if method == 1:
                line[x] = (line[x] + left) & 0xFF
            elif method == 2:
                line[x] = (line[x] + up) & 0xFF
            elif method == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xFF
            elif method == 4:
                estimate = left + up - upleft
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upleft))
                nearest = (left, up, upleft)[distances.index(min(distances))]
                line[x] = (line[x] + nearest) & 0xFF
            elif method != 0:
                raise SystemExit(f"unsupported PNG filter {method}")
        out += line
        previous = line
    return bytes(out)


def mask(path):
    """The coverage mask of one icon, composited over the circle colour."""
    data = path.read_bytes()
    header, compressed = None, b""
    for kind, payload in chunks(data):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed += payload
    if header is None:
        raise SystemExit(f"{path.name}: no IHDR")
    width, height, depth, colour, _compression, _filter, interlace = header
    if (width, height) != (SIZE, SIZE):
        raise SystemExit(f"{path.name}: {width}x{height}, expected {SIZE}x{SIZE}")
    if depth != 8 or interlace or colour not in CHANNELS or colour == 3:
        raise SystemExit(f"{path.name}: unsupported PNG (depth {depth}, colour type {colour})")
    channels = CHANNELS[colour]
    pixels = unfilter(zlib.decompress(compressed), width, height, channels)
    out = bytearray(width * height)
    for i in range(width * height):
        sample = pixels[i * channels:(i + 1) * channels]
        if colour in (2, 6) and not sample[0] == sample[1] == sample[2]:
            raise SystemExit(f"{path.name}: pixel {i} is not grey; icons are coverage masks")
        # Alpha over black: transparent and black both mean "keep the circle".
        out[i] = sample[0] * sample[-1] // 255 if colour in (4, 6) else sample[0]
    return bytes(out)


def build():
    masks = [mask(SOURCE / name) for name in ICONS]
    return HEADER.pack(MAGIC, VERSION, len(masks), SIZE, SIZE) + b"".join(masks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true",
                        help="only report whether the embedded set matches the PNGs")
    arguments = parser.parse_args()

    icons = build()
    if arguments.check:
        current = arguments.output.read_bytes() if arguments.output.exists() else b""
        if current != icons:
            raise SystemExit(f"{arguments.output} is stale; run python tools/build_icons.py")
        print(f"[OK] {arguments.output.name} matches the PNGs in {SOURCE.name}/")
        return 0

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(icons)
    print(f"[Icons] {arguments.output.relative_to(ROOT).as_posix()}: "
          f"{len(ICONS)} icons at {SIZE}x{SIZE}, {len(icons)} bytes")
    for name in ICONS:
        print(f"    {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
