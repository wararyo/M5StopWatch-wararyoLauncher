#include "Renderer.h"
#include <algorithm>

namespace ui {
namespace {
constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Muted = 0xad75;
constexpr uint16_t Lime = 0xb7e0;

struct AppItem { const char* label; uint16_t color; const char* icon; };
constexpr AppItem Items[] = {
    {"STOPWATCH", 0x349f, "S"}, {"SETTINGS", 0x632c, "*"}, {"EXTERNAL 1", 0x2e17, "1"}};

constexpr int IconX = 88;
constexpr int IconRadius = 37;      // radius of the selected row, the larger one
constexpr int UnselectedRadius = 32;
constexpr int LabelX = 143;
constexpr int RowSpacing = 94;
constexpr int FirstRowY = 226;
constexpr int HintY = 454;
constexpr int CenterX = 234;
constexpr const char* HintText = "A NEXT   B OPEN   A+B HOME";
/// Slack around measured geometry, so an erase always covers what was inked.
constexpr int Margin = 8;
}  // namespace

Renderer::Renderer(M5GFX& display) : display_(display) {}

bool Renderer::begin() {
    if (display_.width() <= 0 || display_.height() <= 0) return false;

    // Widest row, so the shared box for the rows is tight enough to be worth
    // tracking rather than just using the full width.
    display_.setFont(&fonts::FreeSansBold18pt7b);
    display_.setTextSize(1.0f);
    int right = IconX + IconRadius;
    for (const auto& item : Items) {
        right = std::max<int>(right, LabelX + display_.textWidth(item.label));
    }
    itemsRight_ = right + Margin;

    return watchFace_.begin(display_);
}

void Renderer::planAppList(const app::State& state, int yOffset) {
    const int screenW = display_.width();
    const int screenH = display_.height();
    const int scroll = static_cast<int>(state.listScroll);
    listOriginY_ = yOffset;
    firstRowY_ = yOffset + FirstRowY - scroll;
    const int lastRowY = firstRowY_ + (app::LauncherApp::AppCount - 1) * RowSpacing;

    // fillCircle paints centre-radius through centre+radius inclusive, so the
    // box is 2r+1 tall, not 2r. The margin also covers the label glyphs.
    const int left = IconX - IconRadius - Margin;
    const int top = firstRowY_ - IconRadius - Margin;
    const int bottom = lastRowY + IconRadius + Margin;
    rowsHandle_ = frame_.add(
        listItems_,
        clipToScreen(Rect{static_cast<int16_t>(left), static_cast<int16_t>(top),
                          static_cast<int16_t>(itemsRight_ + Margin - left),
                          static_cast<int16_t>(bottom - top + 1)},
                     screenW, screenH),
        hashValue(static_cast<uint32_t>(state.selected)));

    display_.setFont(&fonts::FreeSans9pt7b);
    display_.setTextSize(1.0f);
    const int hintW = display_.textWidth(HintText) + Margin;
    const int hintH = display_.fontHeight() + Margin;
    hintHandle_ = frame_.add(
        listHint_,
        clipToScreen(Rect{static_cast<int16_t>(CenterX - hintW / 2),
                          static_cast<int16_t>(yOffset + HintY - hintH),
                          static_cast<int16_t>(hintW), static_cast<int16_t>(hintH)},
                     screenW, screenH),
        hashString(HintText));
}

void Renderer::paintAppList(const app::State& state) {
    const int screenH = display_.height();
    if (frame_.shouldPaint(rowsHandle_)) {
        for (int i = 0; i < app::LauncherApp::AppCount; ++i) {
            const int y = firstRowY_ + i * RowSpacing;
            if (y < -IconRadius || y > screenH + IconRadius) continue;
            const bool selected = i == state.selected;
            display_.fillCircle(IconX, y, selected ? IconRadius : UnselectedRadius,
                                Items[i].color);
            display_.setTextDatum(middle_center);
            display_.setTextColor(White, Items[i].color);
            display_.setFont(&fonts::FreeSansBold18pt7b);
            display_.drawString(Items[i].icon, IconX, y);
            display_.setTextDatum(middle_left);
            display_.setTextColor(selected ? Lime : White, Black);
            display_.setFont(selected ? &fonts::FreeSansBold18pt7b : &fonts::FreeSans18pt7b);
            display_.drawString(Items[i].label, LabelX, y);
        }
    }
    if (frame_.shouldPaint(hintHandle_)) {
        display_.setTextDatum(bottom_center);
        display_.setTextColor(Muted, Black);
        display_.setFont(&fonts::FreeSans9pt7b);
        display_.drawString(HintText, CenterX, listOriginY_ + HintY);
    }
}

void Renderer::drawToast(const char* toast) {
    display_.fillRoundRect(58, 375, 352, 54, 18, 0x2104);
    display_.drawRoundRect(58, 375, 352, 54, 18, Muted);
    display_.setTextDatum(middle_center);
    display_.setTextColor(White, 0x2104);
    display_.setFont(&fonts::FreeSans12pt7b);
    display_.drawString(toast, CenterX, 402);
}

bool Renderer::draw(const app::State& state, const WatchData& watch) {
    // The toast sits on top of the app list rather than beside it, so it is not
    // an element of its own. Showing or hiding one repaints the frame instead.
    if (state.toast != previousToast_) {
        previousToast_ = state.toast;
        repaintAll_ = true;
    }

    const int screenHeight = display_.height();
    const int progress = static_cast<int>(state.transition * screenHeight);
    const bool repaintAll = repaintAll_;
    repaintAll_ = false;

    display_.startWrite();
    if (repaintAll) display_.fillScreen(Black);

    // Lay both layers out before painting either. The watch face slides up as
    // the app list slides in, so a layer's box from the previous frame can land
    // on top of the other layer's box in this one. FramePlan resolves that by
    // erasing everything before anything is painted.
    frame_.begin(repaintAll);
    watchFace_.plan(frame_, display_, watch, -progress);
    planAppList(state, screenHeight - progress);
    frame_.resolve(display_);

    watchFace_.paint(display_, frame_);
    paintAppList(state);

    // Whatever repainted may have covered the toast, so redraw it on top.
    const bool painted = frame_.anyPaint();
    if (state.toast && painted) drawToast(state.toast);

    display_.endWrite();
    return painted || repaintAll;
}

}  // namespace ui
