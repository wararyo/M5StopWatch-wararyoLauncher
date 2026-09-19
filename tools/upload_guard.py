"""Host guard adapted from MultiFirm f5a8fd8 examples/upload_guard.py.

Self-contained PRE script: it does not load anything from lib_deps. PlatformIO
may resolve packages before running PRE scripts. There is no upload override.
"""
from pathlib import Path

HOST_MAX_SIZE = 0x400000
BLOCKED_TARGETS = frozenset(("upload", "uploadfs", "uploadfsota", "erase"))


def check_targets(targets):
    blocked = sorted(BLOCKED_TARGETS.intersection(targets))
    if blocked:
        raise RuntimeError(
            "MultiFirm host protection: blocked " + ", ".join(blocked) +
            ". Use multifirm.py install-host .pio/build/m5stopwatch/firmware.bin "
            "(preview first; --port COMxx --execute to install). "
            "See docs/product-build.md. Full-device upload is not supported.")


def check_size(path):
    size = Path(path).stat().st_size
    if size > HOST_MAX_SIZE:
        raise RuntimeError(f"MultiFirm host image is {size} bytes; "
                           f"limit is {HOST_MAX_SIZE} (0x400000)")
    print(f"[MultiFirm] host size: {size}/{HOST_MAX_SIZE} bytes")


def configure(env, targets):
    check_targets(targets)
    if env.get("PROGNAME", "program") == "program":
        env.Replace(PROGNAME="firmware")
    image = env.subst("$BUILD_DIR/${PROGNAME}.bin")

    def verify_size(source, target, env):
        check_size(image)

    env.AddPostAction(image, verify_size)
    check = env.Alias("multifirm_check_size", image, verify_size)
    env.AlwaysBuild(check)
    env.Depends(env.Alias("buildprog"), check)


if "Import" in globals():
    Import("env")
    from SCons.Script import COMMAND_LINE_TARGETS
    configure(env, COMMAND_LINE_TARGETS)
elif __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Check the MultiFirm host binary size")
    parser.add_argument("image")
    check_size(parser.parse_args().image)
