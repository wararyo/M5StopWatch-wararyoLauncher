#pragma once
#include "ui/graphics/IconBitmap.h"
#include <cstdint>
namespace launcher {
// The applications' own look, shared by every feature that shows an app: the
// launcher's rows and the background information an app offers the watch face
// (docs/task10/plan.md 4.1). Nothing here knows the launch registry.
//
// Index into the embedded mask set (tools/build_icons.py writes this order).
// Entries are appended, never reordered: the asset is indexed by this value.
enum class IconId : uint8_t { Stopwatch,Settings,App1,App2,App3,Count };
// Each app's colour (RGB565): the circle under its icon in the list, and the
// colour it suggests for its background information.
inline constexpr uint16_t StopwatchAccent=0x349f,SettingsAccent=0x632c,ExternalAccent=0x2e17;
// The embedded icon set, built by tools/build_icons.py from the PNGs in icons/.
// The masks are static and never change while the app runs, so a caller may
// keep the pointer. Returns nullptr when the asset is missing or malformed;
// whoever draws then does without the icon instead of failing.
const IconBitmap* appIcon(IconId id);
}
