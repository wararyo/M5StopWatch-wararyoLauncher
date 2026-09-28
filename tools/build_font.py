"""Build an embedded VLW font subset from a TrueType font.

LovyanGFX reads VLW, so the embedded asset stays VLW; this tool rasterises it
directly instead of subsetting a file made by an external web service. FreeType
with light hinting and `ceil` on the unhinted advance reproduces
vlw-font-creator.m5stack.com exactly: for the 307 glyph subset every metric
matches and 50 of about 146,000 bitmap pixels differ by 1/255.

Embedding the whole font is not an option (about 2.3MiB in a 4MiB host image),
so the default subset (the UI's Japanese font) keeps ASCII, kana and the
punctuation the UI uses, plus every non-ASCII character that appears in `src/`.
Characters outside the subset are drawn as `?` by `fitText`, per docs/plan.md 5.2.

The watch face fonts hold far less, so they name their characters in a file
under tools/fonts/ (`--chars-file`) and are written elsewhere (`--output`).
Without either option the tool builds the default subset exactly as before.
The commands for every committed asset are in docs/task10/fonts.md.

Needs `pip install freetype-py`, and only when the font or the character set
changes; the build itself uses the committed subset.

Usage: python tools/build_font.py --source <font.ttf> [--size 28] [--check]
       python tools/build_font.py --source <font.ttf> --size <px> --chars-file <chars.txt>
                                  --output <asset.vlw> [--check]
       python tools/build_font.py --source <font.ttf> --compare <other.vlw>
"""
import argparse
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "src/ui/graphics/fonts/GenShinGothicMedium28.vlw"
HEADER = struct.Struct(">6I")
GLYPH = struct.Struct(">4I2iI")

# Fixed ranges: printable ASCII, hiragana (with the combining marks), katakana
# up to the iteration marks (the digraph U+30FF is not used), and the
# punctuation Japanese UI text uses.
RANGES = ((0x20, 0x7E), (0x3041, 0x3096), (0x3099, 0x309E), (0x30A0, 0x30FE))
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


def listed_characters(path):
    """Every character in a UTF-8 file, line breaks aside. All are required."""
    text = path.read_text(encoding="utf-8").replace("\r", "").replace("\n", "")
    codes = {ord(character) for character in text}
    if not codes:
        raise SystemExit(f"{path}: no characters")
    return codes, set(codes)


def character_set():
    """(every code point to embed, the ones the UI names explicitly)."""
    required = {ord(character) for character in PUNCTUATION}
    required.update(ord(character) for character in harvest(ROOT / "src"))
    wanted = {code for first, last in RANGES for code in range(first, last + 1)}
    wanted.update(required)
    return wanted, required


def render(font, size, codes):
    """Rasterise the code points the font covers, in code point order."""
    import freetype

    face = freetype.Face(str(font))
    face.set_pixel_sizes(0, size)
    flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_LIGHT
    glyphs, missing = [], []
    for code in sorted(codes):
        if face.get_char_index(code) == 0:
            missing.append(code)
            continue
        face.load_char(chr(code), flags)
        slot = face.glyph
        bitmap = slot.bitmap
        rows = [bytes(bitmap.buffer[y * bitmap.pitch:y * bitmap.pitch + bitmap.width])
                for y in range(bitmap.rows)]
        glyphs.append(dict(code=code, height=bitmap.rows, width=bitmap.width,
                           # The creator advances by the unhinted advance, rounded up.
                           advance=math.ceil(slot.linearHoriAdvance / 65536),
                           dy=slot.bitmap_top, dx=slot.bitmap_left,
                           bitmap=b"".join(rows)))
    return glyphs, missing


def build(glyphs, size):
    # LovyanGFX reads these as the initial max ascent/descent, so they follow the
    # glyphs that are actually embedded, exactly as the web creator wrote them.
    ascent = max((glyph["dy"] for glyph in glyphs), default=0)
    descent = max((glyph["height"] - glyph["dy"] for glyph in glyphs), default=0)
    parts = [HEADER.pack(len(glyphs), 11, size, 0, ascent, -descent & 0xFFFFFFFF)]
    parts.extend(GLYPH.pack(glyph["code"], glyph["height"], glyph["width"], glyph["advance"],
                            glyph["dy"], glyph["dx"], 0) for glyph in glyphs)
    parts.extend(glyph["bitmap"] for glyph in glyphs)
    return b"".join(parts)


def read_vlw(data):
    count = HEADER.unpack_from(data)[0]
    glyphs, offset, bitmap = {}, HEADER.size, HEADER.size + count * GLYPH.size
    for _ in range(count):
        code, height, width, advance, dy, dx, _reserved = GLYPH.unpack_from(data, offset)
        offset += GLYPH.size
        glyphs[code] = dict(code=code, height=height, width=width, advance=advance,
                            dy=dy, dx=dx, bitmap=data[bitmap:bitmap + width * height])
        bitmap += width * height
    return glyphs


def compare(glyphs, reference):
    """Report how far this rendering is from another VLW, glyph by glyph."""
    produced = {glyph["code"]: glyph for glyph in glyphs}
    shared = sorted(set(produced) & set(reference))
    fields = ("height", "width", "advance", "dy", "dx")
    mismatched, differing, largest, total = [], 0, 0, 0
    for code in shared:
        made, want = produced[code], reference[code]
        if any(made[key] != want[key] for key in fields):
            mismatched.append((code, {key: (want[key], made[key]) for key in fields
                                      if made[key] != want[key]}))
            continue
        for made_pixel, want_pixel in zip(made["bitmap"], want["bitmap"]):
            if made_pixel != want_pixel:
                differing += 1
                largest = max(largest, abs(made_pixel - want_pixel))
        total += len(want["bitmap"])
    print(f"[Compare] shared={len(shared)} only_in_output={len(set(produced) - set(reference))} "
          f"only_in_reference={len(set(reference) - set(produced))}")
    print(f"[Compare] metric mismatches={len(mismatched)}")
    for code, difference in mismatched[:10]:
        print(f"    U+{code:04X} {chr(code)} {difference}")
    print(f"[Compare] differing pixels={differing} of {total} "
          f"({100.0 * differing / max(1, total):.3f}%) max difference={largest}/255")
    return not mismatched and largest <= 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="the TrueType font to rasterise; it is not kept in this repository")
    parser.add_argument("--size", type=int, default=28, help="pixel size (default 28)")
    parser.add_argument("--output", type=Path, default=OUTPUT,
                        help="the asset to write or check (default: the UI's Japanese subset)")
    parser.add_argument("--chars-file", type=Path,
                        help="embed exactly the characters in this UTF-8 file instead of "
                             "the default Japanese UI set")
    parser.add_argument("--check", action="store_true",
                        help="only report whether the existing subset is current")
    parser.add_argument("--compare", type=Path,
                        help="report the difference against another VLW instead of writing")
    arguments = parser.parse_args()

    wanted, required = (listed_characters(arguments.chars_file) if arguments.chars_file
                        else character_set())
    # The VLW index is a 16 bit binary search; anything outside the BMP cannot
    # be stored and is rendered as the fallback character instead.
    outside = sorted(code for code in required if code > 0xFFFF)
    glyphs, missing = render(arguments.source, arguments.size,
                             {code for code in wanted if code <= 0xFFFF})
    subset = build(glyphs, arguments.size)

    if arguments.compare:
        print(f"[Font] {arguments.source.name} at {arguments.size}px: {len(glyphs)} glyphs")
        return 0 if compare(glyphs, read_vlw(arguments.compare.read_bytes())) else 1

    if arguments.check:
        current = arguments.output.read_bytes() if arguments.output.exists() else b""
        if current != subset:
            raise SystemExit(f"{arguments.output} is stale; run python tools/build_font.py "
                             "(docs/task10/fonts.md has the command)")
        print(f"[OK] {arguments.output.name} matches the current font and character set")
        return 0

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(subset)
    print(f"[Font] {arguments.source.name} at {arguments.size}px")
    try:
        shown = arguments.output.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        shown = arguments.output.as_posix()
    print(f"[Font] {shown}: {len(glyphs)} glyphs, {len(subset)} bytes")
    # Range gaps are expected; only the characters the UI names must exist.
    absent = sorted(code for code in missing if code in required)
    if absent:
        print("[Font] not in the source font: " + " ".join(f"U+{code:04X}" for code in absent))
    if outside:
        print("[Font] outside the BMP, drawn as the fallback character: " +
              " ".join(f"U+{code:04X}" for code in outside))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ImportError:
        sys.exit("freetype-py is required: pip install freetype-py")
