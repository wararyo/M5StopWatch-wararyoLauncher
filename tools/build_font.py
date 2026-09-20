"""Subset a VLW font down to the glyphs the fixed UI needs and embed the result.

The full GenShinGothic 28px VLW is about 2.3MB, which does not belong in a 4MiB
host image. This tool keeps ASCII, kana and the punctuation the UI uses, plus
every non-ASCII character that appears in `src/`, and writes a smaller VLW that
`src/CMakeLists.txt` embeds. Characters outside the subset are drawn as `?` by
`fitText`, which matches the design decision in docs/plan.md 5.2.

Usage: python tools/build_font.py --source <path to the full .vlw> [--check]
"""
import argparse
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "src/ui/fonts/GenShinGothic28.vlw"
HEADER = struct.Struct(">6I")
GLYPH = struct.Struct(">7I")

# Fixed ranges: printable ASCII, hiragana (with the combining marks), katakana
# including the prolonged sound mark, and the punctuation Japanese UI text uses.
RANGES = ((0x20, 0x7E), (0x3041, 0x3096), (0x3099, 0x309E), (0x30A0, 0x30FF))
PUNCTUATION = "、。・「」『』〜ー々〇"


def harvest(directory):
    """Non-ASCII characters that appear in the product sources."""
    found = set()
    for path in sorted(directory.rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        for character in path.read_text(encoding="utf-8"):
            if ord(character) > 0x7F:
                found.add(character)
    return found


def read_font(path):
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise SystemExit(f"{path}: not a VLW file")
    count, version, size, unused, ascent, descent = HEADER.unpack_from(data)
    glyphs, offset, bitmap = [], HEADER.size, HEADER.size + count * GLYPH.size
    for _ in range(count):
        entry = GLYPH.unpack_from(data, offset)
        offset += GLYPH.size
        length = entry[2] * entry[1]  # width * height
        glyphs.append((entry, data[bitmap:bitmap + length]))
        bitmap += length
    if bitmap != len(data):
        raise SystemExit(f"{path}: glyph bitmaps do not fill the file")
    return (count, version, size, unused, ascent, descent), glyphs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="the full VLW font to subset; it is not kept in this repository")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true",
                        help="only report whether the existing subset is current")
    arguments = parser.parse_args()

    required = {ord(character) for character in PUNCTUATION}
    required.update(ord(character) for character in harvest(ROOT / "src"))
    wanted = {code for first, last in RANGES for code in range(first, last + 1)}
    wanted.update(required)
    # The VLW index is a 16 bit binary search; anything outside the BMP cannot
    # be stored and is rendered as the fallback character instead.
    outside = sorted(code for code in required if code > 0xFFFF)
    wanted = {code for code in wanted if code <= 0xFFFF}

    header, glyphs = read_font(arguments.source)
    available = {entry[0]: (entry, bitmap) for entry, bitmap in glyphs}
    # Range gaps are expected; only the characters the UI names must exist.
    missing = sorted(code for code in required if 0x20 <= code <= 0xFFFF and code not in available)
    kept = sorted(code for code in wanted if code in available)

    parts = [HEADER.pack(len(kept), header[1], header[2], header[3], header[4], header[5])]
    parts.extend(GLYPH.pack(*available[code][0]) for code in kept)
    parts.extend(available[code][1] for code in kept)
    subset = b"".join(parts)

    if arguments.check:
        current = arguments.output.read_bytes() if arguments.output.exists() else b""
        if current != subset:
            raise SystemExit(f"{arguments.output} is stale; run python tools/build_font.py")
        print(f"[OK] {arguments.output.name} matches the current character set")
        return

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(subset)
    print(f"[Font] {arguments.source.name}: {header[0]} glyphs, {arguments.source.stat().st_size} bytes")
    print(f"[Font] {arguments.output.relative_to(ROOT).as_posix()}: {len(kept)} glyphs, {len(subset)} bytes")
    if missing:
        print("[Font] not in the source font: " +
              " ".join(f"U+{code:04X}" for code in missing))
    if outside:
        print("[Font] outside the BMP, drawn as the fallback character: " +
              " ".join(f"U+{code:04X}" for code in outside))


if __name__ == "__main__":
    sys.exit(main())
