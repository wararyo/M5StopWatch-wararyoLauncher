#pragma once
#include "ui/rendering/Geometry.h"
#include <cstdint>
namespace launcher {
// Hours and minutes, or with seconds after a long press. Both faces show the
// time this way (docs/task10/plan.md 3.1).
enum class TimeVariant : uint8_t { HourMinute,HourMinuteSecond };
// What the painter measured of its time font. Widths are the widest advance
// any value of the group can take: HH over 00..23 and --, mm and ss over
// 00..59 and --. The colon has one width.
struct TimeMetrics {
    int ascent=0,descent=0;
    int hourWidth=0,minuteWidth=0,colonWidth=0;
};
// The time in groups that tile its row without overlapping: HH with the colon
// after it, mm (with the second colon when seconds show), ss. A new second
// repaints ss alone, a new minute mm. Digits are proportional: HH is right
// aligned, mm left aligned (centred between the colons with seconds), ss
// left aligned, so the colons stay put (docs/task10/fonts.md 5).
struct TimeGroups {
    Rect hour{},minute{},second{};
    int hourRight=0,colon1X=0,colon2X=0;
    int minuteX=0;         // left edge (HH:mm) or centre (HH:mm:ss) of mm
    int secondX=0;         // left edge of ss
};
inline TimeGroups placeTimeGroups(int cx,int baseline,TimeVariant variant,const TimeMetrics& m) {
    // A little room above and below the ink for the edges' coverage.
    const int margin=2,top=baseline-m.ascent-margin,height=m.ascent+m.descent+2*margin;
    const bool seconds=variant==TimeVariant::HourMinuteSecond;
    const int width=m.hourWidth+m.colonWidth+m.minuteWidth+(seconds ? m.colonWidth+m.minuteWidth : 0);
    const int left=cx-width/2;
    TimeGroups g;
    g.hourRight=left+m.hourWidth;
    g.colon1X=g.hourRight;
    const int minuteLeft=g.colon1X+m.colonWidth;
    g.hour={left-margin,top,g.colon1X+m.colonWidth-(left-margin),height};
    if (seconds) {
        g.minuteX=minuteLeft+m.minuteWidth/2;
        g.colon2X=minuteLeft+m.minuteWidth;
        g.secondX=g.colon2X+m.colonWidth;
        g.minute={minuteLeft,top,m.minuteWidth+m.colonWidth,height};
        g.second={g.secondX,top,m.minuteWidth+margin,height};
    } else {
        g.minuteX=minuteLeft;
        g.minute={minuteLeft,top,m.minuteWidth+margin,height};
    }
    return g;
}
}
