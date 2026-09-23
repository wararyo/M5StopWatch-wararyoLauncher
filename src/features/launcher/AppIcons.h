#pragma once
#include "app/AppRegistry.h"
#include "ui/graphics/IconBitmap.h"
namespace launcher {
// The launcher's embedded icon set, built by tools/build_icons.py from the
// PNGs in icons/. Resolving an IconId happens here, on the launcher side; the
// shared list only ever receives the resulting IconBitmap.
// Returns nullptr when the asset is missing or malformed; the list then draws
// the circle alone instead of failing to show the row.
const IconBitmap* appIcon(IconId id);
}
