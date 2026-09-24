#pragma once
#include "ui/graphics/IconBitmap.h"
#include <cstdint>
namespace launcher {
// Identity of a row inside one list, chosen by the list's owner. It only has
// to be unique within that list; what it opens is the owner's business.
using RowId=uint16_t;
// One row as the list sees it. Everything is borrowed: `label` and `mask` must
// stay unchanged at least until the frame that planned them has painted, and
// must not point into a copy that goes away first.
struct ListRow {
    RowId id=0;
    const char* label="";
    // A row with an icon gets the filled circle. The mask on top is optional,
    // so a missing asset still leaves the circle and the rows scan the same.
    bool icon=false;
    uint16_t iconColor=0;
    const IconBitmap* mask=nullptr;
    // Dimmed is only a look. Whether B or a tap decides the row is `enabled`:
    // a dimmed external slot still opens, to explain why it cannot launch.
    bool dimmed=false,enabled=true;
};
// Read-only rows with their count. The count is input, never a constant.
struct ListRows {
    const ListRow* rows=nullptr;
    int count=0;
    const ListRow& operator[](int index) const { return rows[index]; }
};
// Snapshot of a ListController for the frame, as the view reads it.
struct ListState {
    int selection=-1; // Index into the rows; -1 when there are none.
    float scroll=0;   // selection*rowSpacing centres that row.
    bool dragging=false,animating=false;
};
// What B or a tap produced. The list only names the row; the screen acts.
struct ListDecision {
    bool changed=false,decided=false;
    RowId id=0;
    int index=-1;
};
}
