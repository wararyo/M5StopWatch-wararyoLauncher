#pragma once
#include "features/settings/SettingsModel.h"
#include "ui/list/ListLayout.h"
#include <array>
#include <cstdio>
namespace launcher {
// The settings top menu as rows of the shared list. A row is known by what it
// opens, never by its position, so reordering the menu moves no meaning.
enum class SettingsItem : RowId { DateTime=1, Brightness, ScreenOff, Info, Back };
inline constexpr SettingsItem SettingsMenuItems[]={
    SettingsItem::DateTime,SettingsItem::Brightness,SettingsItem::ScreenOff,
    SettingsItem::Info,SettingsItem::Back};
inline constexpr int SettingsMenuCount=int(sizeof(SettingsMenuItems)/sizeof(SettingsMenuItems[0]));
inline RowId settingsRowId(SettingsItem item) { return static_cast<RowId>(item); }
// The view a row opens. Back opens none: it leaves settings.
inline bool settingsItemView(RowId id,SettingsView& view) {
    switch (static_cast<SettingsItem>(id)) {
    case SettingsItem::DateTime: view=SettingsView::DateTime; return true;
    case SettingsItem::Brightness: view=SettingsView::Brightness; return true;
    case SettingsItem::ScreenOff: view=SettingsView::ScreenOff; return true;
    case SettingsItem::Info: view=SettingsView::Info; return true;
    default: return false;
    }
}
// The menu fills the panel at rest, placed exactly like the app list. Input
// and drawing both place it through here.
inline ListPlacement settingsMenuPlacement(Viewport v,float scroll) { return fullListPlacement(v,scroll); }
// Formatted labels, owned by whoever draws: the list borrows them until paint.
struct SettingsMenuLabels { char text[SettingsMenuCount][40]{}; };
inline const char* settingsItemName(SettingsItem item) {
    switch (item) {
    case SettingsItem::DateTime: return "日時";
    case SettingsItem::Brightness: return "輝度";
    case SettingsItem::ScreenOff: return "消灯時間";
    case SettingsItem::Info: return "情報";
    default: return "戻る";
    }
}
// Rows without icons, all decidable. Input needs only the ids, so it passes
// neither a model nor labels and every label stays empty.
inline ListRows buildSettingsMenuRows(std::array<ListRow,SettingsMenuCount>& rows,
                                      const SettingsModel* model=nullptr,
                                      SettingsMenuLabels* labels=nullptr) {
    for (int i=0;i<SettingsMenuCount;++i) {
        const SettingsItem item=SettingsMenuItems[i];
        auto& row=rows[i];
        row=ListRow{};
        row.id=settingsRowId(item);
        if (!model || !labels) continue;
        char* text=labels->text[i];
        const size_t size=sizeof(labels->text[i]);
        // The stored value rides on its row, so the menu answers "what is it
        // now?" without opening the editor.
        if (item==SettingsItem::Brightness)
            std::snprintf(text,size,"%s  %d",settingsItemName(item),model->savedBrightness);
        else if (item==SettingsItem::ScreenOff)
            std::snprintf(text,size,"%s  %d秒",settingsItemName(item),model->savedScreenOffSec);
        else std::snprintf(text,size,"%s",settingsItemName(item));
        row.label=text;
    }
    return {rows.data(),SettingsMenuCount};
}
}
