#pragma once
#include "features/launcher/AppListModel.h"
#include "multifirm/SlotCatalog.h"
#include <array>
namespace launcher {
// Row ids are LaunchTargetId values, so a row keeps its meaning whatever position the
// registry gives it.
inline RowId appRowId(LaunchTargetId id) { return static_cast<RowId>(id); }
inline const LaunchEntry* launchEntry(RowId id) {
    for (const auto& entry:LaunchRegistry) if (appRowId(entry.id)==id) return &entry;
    return nullptr;
}
// A launchable slot lends the row its own name; everything else keeps the
// registry name and only dims, so the list stays icon plus name (plan.md 5.2)
// and the reason lives on the detail screen. The names point into `slots`,
// which therefore has to outlive the frame.
inline void applySlots(const SlotCatalog& slots,AppListModel& m) {
    for (int i=0;i<AppListCount;++i) {
        const auto& entry=LaunchRegistry[i];
        if (entry.kind!=TargetKind::External) continue;
        const auto& slot=slots.slots[entry.slot-1];
        const bool ready=slots.layoutSupported && slot.status==SlotStatus::Ready;
        m.names[i]=ready && slot.name[0] ? slot.name : nullptr;
        m.rowDimmed[i]=!ready;
    }
}
inline uint16_t appIconColor(const LaunchEntry& entry) {
    switch (entry.id) {
    case LaunchTargetId::Stopwatch: return StopwatchAccent;
    case LaunchTargetId::Settings: return SettingsAccent;
    default: return ExternalAccent;
    }
}
using IconLookup=const IconBitmap* (*)(IconId);
// Every row is decidable: a dimmed external slot still opens, to explain why it
// cannot launch. Input needs only the ids, so it passes no icon lookup.
inline ListRows buildAppListRows(const AppListModel& m,std::array<ListRow,AppListCount>& rows,
                                 IconLookup icons=nullptr) {
    for (int i=0;i<AppListCount;++i) {
        const auto& entry=LaunchRegistry[i];
        auto& row=rows[i];
        row.id=appRowId(entry.id);
        row.label=m.names[i] ? m.names[i] : entry.name;
        row.icon=true; row.iconColor=appIconColor(entry);
        row.mask=icons ? icons(entry.icon) : nullptr;
        row.dimmed=m.rowDimmed[i]; row.enabled=true;
    }
    return {rows.data(),AppListCount};
}
}
