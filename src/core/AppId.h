#pragma once
namespace launcher {
// A launch target's stable identity (docs/task10/plan.md 4.1 calls it AppId).
// The values are explicit and never reused: list rows and background
// information are keyed by them, so a position in the registry is not an id.
enum class LaunchTargetId { Stopwatch=0,Settings=1,External1=2,External2=3,External3=4 };
}
