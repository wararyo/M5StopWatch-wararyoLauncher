#pragma once
#include "ListLayout.h"
#include "features/settings/SettingsModel.h"
namespace launcher {
struct SettingsGeometry : Viewport { SettingsView view=SettingsView::Menu; int cursor=0; };
constexpr int SettingsMenuRows=5;
inline int settingsFieldCount(SettingsView view) {
    return view==SettingsView::DateTime ? 5 :
        (view==SettingsView::Brightness || view==SettingsView::ScreenOff) ? 1 : 0;
}
// Info only confirms; the others save or cancel.
inline int settingsButtonCount(SettingsView view) { return view==SettingsView::Info ? 1 : 2; }
// An action takes effect where it stands: it neither edits a value nor leaves
// the view, so it sits between the fields and the buttons in the cursor order.
// A view that has none keeps exactly the slots it had before.
inline int settingsActionCount(SettingsView view) { return view==SettingsView::Info ? 1 : 0; }
inline int settingsSlotCount(SettingsView view) {
    return view==SettingsView::Menu ? SettingsMenuRows
        : settingsFieldCount(view)+settingsActionCount(view)+settingsButtonCount(view);
}
// Five rows centred on the panel. They all fit, so the menu never scrolls and
// the cursor is the only thing that moves; the arc placement is still the app
// list's, so the two screens read as the same kind of list.
inline RowLayout settingsMenuRow(const SettingsGeometry& m,int index) {
    ListGeometry laid; laid.width=m.width; laid.height=m.height;
    laid.transition=1; laid.scroll=float(2*rowSpacing(m));
    return layoutRow(laid,index);
}
// Menu text starts where an app list row's circle would, so both line up.
inline int settingsMenuLabelX(const SettingsGeometry& m,const RowLayout& row) {
    return row.iconX-iconRadius(m);
}
inline void settingsFieldCentre(const SettingsGeometry& m,int index,int& cx,int& cy) {
    if (m.view==SettingsView::DateTime) {
        constexpr int dx[]={-96,0,96,-54,54};
        cx=m.width/2+offsetPx(m,dx[index]);
        cy=offsetPx(m,index<3 ? 186 : 300);
    } else { cx=m.width/2; cy=offsetPx(m,232); }
}
inline int settingsFieldHalfWidth(const SettingsGeometry& m) {
    return offsetPx(m,m.view==SettingsView::DateTime ? 42 : 78);
}
// A field is one element: the up arrow, the value and the down arrow together,
// so a value change repaints the column as a unit.
inline Rect settingsFieldBox(const SettingsGeometry& m,int index) {
    int cx=0,cy=0; settingsFieldCentre(m,index,cx,cy);
    const int hw=settingsFieldHalfWidth(m),hh=offsetPx(m,56);
    return {cx-hw,cy-hh,2*hw,2*hh};
}
inline int settingsArrowHeight(const SettingsGeometry& m) { return offsetPx(m,30); }
inline Rect settingsArrowBox(const SettingsGeometry& m,int index,bool up) {
    const Rect box=settingsFieldBox(m,index); const int h=settingsArrowHeight(m);
    return {box.x,up ? box.y : box.y+box.h-h,box.w,h};
}
inline Rect settingsValueBox(const SettingsGeometry& m,int index) {
    const Rect box=settingsFieldBox(m,index); const int h=settingsArrowHeight(m);
    return {box.x,box.y+h,box.w,box.h-2*h};
}
// Only the date and time lines have separators: 0 and 1 sit between the date
// fields, 2 between hour and minute.
inline int settingsSeparatorCount(SettingsView view) { return view==SettingsView::DateTime ? 3 : 0; }
inline Rect settingsSeparatorBox(const SettingsGeometry& m,int index) {
    const int left=index==2 ? 3 : index,right=index==2 ? 4 : index+1;
    int lx=0,ly=0,rx=0,ry=0;
    settingsFieldCentre(m,left,lx,ly); settingsFieldCentre(m,right,rx,ry);
    const int w=offsetPx(m,14),h=offsetPx(m,40);
    return {(lx+rx)/2-w/2,ly-h/2,w,h};
}
inline Rect settingsButtonBox(const SettingsGeometry& m,int index) {
    const int count=settingsButtonCount(m.view);
    const int w=offsetPx(m,count==1 ? 150 : 138),h=offsetPx(m,52);
    const int cx=count==1 ? m.width/2 : m.width/2+offsetPx(m,index==0 ? -74 : 74);
    return {cx-w/2,offsetPx(m,400)-h/2,w,h};
}
inline Rect settingsTitleBox(const SettingsGeometry& m) {
    const int h=offsetPx(m,48);
    return {m.width/4,offsetPx(m,76)-h/2,m.width/2,h};
}
inline Rect settingsInfoBox(const SettingsGeometry& m,int line) {
    const int h=offsetPx(m,46),margin=offsetPx(m,60);
    return {margin,offsetPx(m,170)+line*offsetPx(m,54)-h/2,m.width-2*margin,h};
}
constexpr int SettingsInfoLines=3;
// Actions continue the information lines, so they share their height, margins
// and spacing instead of inventing a second row geometry.
inline Rect settingsActionBox(const SettingsGeometry& m,int index) {
    return settingsInfoBox(m,SettingsInfoLines+index);
}
// Drawing and hit testing share these rectangles, so nothing outside a painted
// target can be tapped.
struct SettingsHit { enum Kind { None,MenuRow,Field,Up,Down,Action,Button } kind=None; int index=0; };
inline SettingsHit hitSettings(const SettingsGeometry& m,int x,int y) {
    const auto view=m.view;
    if (view==SettingsView::Menu) {
        for (int i=0;i<SettingsMenuRows;++i)
            if (settingsMenuRow(m,i).box.contains(x,y)) return {SettingsHit::MenuRow,i};
        return {};
    }
    for (int i=0;i<settingsFieldCount(view);++i) {
        if (settingsArrowBox(m,i,true).contains(x,y)) return {SettingsHit::Up,i};
        if (settingsArrowBox(m,i,false).contains(x,y)) return {SettingsHit::Down,i};
        if (settingsValueBox(m,i).contains(x,y)) return {SettingsHit::Field,i};
    }
    for (int i=0;i<settingsActionCount(view);++i)
        if (settingsActionBox(m,i).contains(x,y)) return {SettingsHit::Action,i};
    for (int i=0;i<settingsButtonCount(view);++i)
        if (settingsButtonBox(m,i).contains(x,y)) return {SettingsHit::Button,i};
    return {};
}
}
