#pragma once

#include "Renderer.h"

namespace ui {

/// Self-check for the partial repaint path, compiled in only with
/// -DLAUNCHER_BENCH.
///
/// The renderer never clears the screen between frames, so a frame's result
/// depends on every frame before it. A box that is a pixel too small, or two
/// elements whose boxes overlap once one of them moves, leaves damage that is
/// invisible in the code and easy to miss on the device.
///
/// This drives the renderer through scripted scrolls and transitions, reads the
/// panel's framebuffer back, and compares it against a full repaint of the very
/// same state. The two must be identical. It also reports the cost of each
/// phase, so the effect of a change on the frame budget is visible.
///
/// Prints to the console and returns. Call once, after Renderer::begin().
void runRepaintCheck(Renderer& renderer, M5GFX& display);

}  // namespace ui
