#pragma once
#include "features/settings/SettingsModel.h"
#include "i18n/Strings.h"
#include "ui/list/ListLayout.h"
#include <algorithm>
#include <array>
#include <cstdio>
namespace launcher {
// The settings top menu as rows of the shared list. A row is known by what it
// opens, never by its position, so reordering the menu moves no meaning.
enum class SettingsItem : RowId { DateTime=1, Brightness, ScreenOff, Info, Back, WatchFace };
inline constexpr SettingsItem SettingsMenuItems[]={
    SettingsItem::DateTime,SettingsItem::Brightness,SettingsItem::ScreenOff,
    SettingsItem::WatchFace,SettingsItem::Info,SettingsItem::Back};
inline constexpr int SettingsMenuCount=int(sizeof(SettingsMenuItems)/sizeof(SettingsMenuItems[0]));
inline RowId settingsRowId(SettingsItem item) { return static_cast<RowId>(item); }
// The view a row opens. Back opens none: it leaves settings.
inline bool settingsItemView(RowId id,SettingsView& view) {
    switch (static_cast<SettingsItem>(id)) {
    case SettingsItem::DateTime: view=SettingsView::DateTime; return true;
    case SettingsItem::Brightness: view=SettingsView::Brightness; return true;
    case SettingsItem::ScreenOff: view=SettingsView::ScreenOff; return true;
    case SettingsItem::Info: view=SettingsView::Info; return true;
    case SettingsItem::WatchFace: view=SettingsView::WatchFace; return true;
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
    case SettingsItem::DateTime: return text::DateTime;
    case SettingsItem::Brightness: return text::Brightness;
    case SettingsItem::ScreenOff: return text::ScreenOff;
    case SettingsItem::Info: return text::Info;
    case SettingsItem::WatchFace: return text::WatchFace;
    default: return text::Back;
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
        else if (item==SettingsItem::ScreenOff) {
            char seconds[16];
            std::snprintf(seconds,sizeof(seconds),text::SecondsFormat,model->savedScreenOffSec);
            std::snprintf(text,size,"%s  %s",settingsItemName(item),seconds);
        }
        else if (item==SettingsItem::WatchFace && model->currentFace>=0 && model->currentFace<model->faceCount &&
                 model->faceNames[model->currentFace])
            std::snprintf(text,size,"%s  %s",settingsItemName(item),model->faceNames[model->currentFace]);
        else std::snprintf(text,size,"%s",settingsItemName(item));
        row.label=text;
    }
    return {rows.data(),SettingsMenuCount};
}
// The watch face list: one row per face, in registration order, then back.
// A face row is known by its position among the faces plus one; back by its
// own id, so it never collides with a face.
inline constexpr RowId SettingsFaceBack=100;
inline constexpr int SettingsFaceRows=SettingsFaceCapacity+1;
struct SettingsFaceLabels { char text[SettingsFaceRows][48]{}; };
inline int settingsFaceIndex(RowId id) { return id>=1 && id<=SettingsFaceCapacity ? int(id)-1 : -1; }
// Input passes neither labels nor names: it needs the ids and the count only.
inline ListRows buildSettingsFaceRows(std::array<ListRow,SettingsFaceRows>& rows,int faceCount,
                                      const SettingsModel* model=nullptr,SettingsFaceLabels* labels=nullptr) {
    faceCount=std::clamp(faceCount,0,SettingsFaceCapacity);
    for (int i=0;i<=faceCount;++i) {
        auto& row=rows[i];
        row=ListRow{};
        row.id=i<faceCount ? RowId(i+1) : SettingsFaceBack;
        if (!model || !labels) continue;
        char* text=labels->text[i];
        const size_t size=sizeof(labels->text[i]);
        // The face that shows says so; choosing is what moves the mark.
        if (i==faceCount) std::snprintf(text,size,"%s",text::Back);
        else if (i==model->currentFace) std::snprintf(text,size,text::InUseFormat,model->faceNames[i] ? model->faceNames[i] : "");
        else std::snprintf(text,size,"%s",model->faceNames[i] ? model->faceNames[i] : "");
        row.label=text;
    }
    return {rows.data(),faceCount+1};
}
}
