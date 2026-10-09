// Work 10-5: the watch face records, the face selection, Forest's rules and
// layout, and choosing a face in settings (docs/task10/plan-10-5.md 7).
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/home/FaceSelection.h"
#include "features/home/faces/AnalogLayout.h"
#include "features/home/faces/DigitalLayout.h"
#include "features/home/faces/ForestLayout.h"
#include "features/home/faces/NoonishBackground.h"
#include "features/settings/SettingsMenu.h"
#include "i18n/Strings.h"
#include "storage/SettingsStore.h"
#include "storage/WatchPreferences.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
// A face row marked as shown, as this build's language spells it.
std::string inUse(const char* name) {
    char buffer[48]; std::snprintf(buffer,sizeof(buffer),text::InUseFormat,name);
    return buffer;
}
// One NVS namespace in memory: the settings blob under "config" and the
// records by key, as storage/NvsBackend.cpp keeps them.
struct MemoryNvs : SettingsBackend, RecordBackend {
    std::map<std::string,std::vector<uint8_t>> records;
    bool readable=true,writable=true;
    int writes=0,reads=0;
    bool load(void* data,size_t& size) override {
        size_t n=size; const auto r=read("config",static_cast<uint8_t*>(data),n);
        size=n; return r==PrefResult::Ok;
    }
    bool save(const void* data,size_t size) override {
        return write("config",static_cast<const uint8_t*>(data),size)==PrefResult::Ok;
    }
    PrefResult read(const char* key,uint8_t* data,size_t& size) override {
        ++reads;
        if (!readable) return PrefResult::Unavailable;
        const auto it=records.find(key);
        if (it==records.end()) return PrefResult::Missing;
        if (it->second.size()>size) return PrefResult::Invalid;
        std::memcpy(data,it->second.data(),it->second.size()); size=it->second.size();
        return PrefResult::Ok;
    }
    PrefResult write(const char* key,const uint8_t* data,size_t size) override {
        if (!writable) return PrefResult::WriteFailed;
        ++writes; records[key].assign(data,data+size);
        return PrefResult::Ok;
    }
};
Events press(bool next) { Events e{}; e.next=next; e.decide=!next; return e; }
}
void records() {
    MemoryNvs nvs; WatchPreferences store;
    char id[WatchFaceIdMax+1];
    // Nothing bound, nothing stored, then a round trip.
    CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Unavailable && id[0]==0);
    store.bind(&nvs);
    CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Missing);
    CHECK(store.saveSelection("forest")==PrefResult::Ok);
    CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Ok && std::strcmp(id,"forest")==0);
    // The layout is explicit: format, length, the bytes.
    const auto& raw=nvs.records["watch_sel"];
    CHECK(raw.size()==8 && raw[0]==1 && raw[1]==6 && std::memcmp(raw.data()+2,"forest",6)==0);
    // The longest id fits; one more is refused before anything is written.
    const std::string longest(WatchFaceIdMax,'a');
    CHECK(store.saveSelection(longest.c_str())==PrefResult::Ok);
    CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Ok && longest==id);
    const int writes=nvs.writes;
    CHECK(store.saveSelection((longest+"a").c_str())==PrefResult::TooLarge && nvs.writes==writes);
    CHECK(store.saveSelection("")==PrefResult::TooLarge && store.saveSelection("a b")==PrefResult::TooLarge);
    // Unknown format, bad lengths, a record too big to read: invalid, and
    // left exactly as it was.
    for (const std::vector<uint8_t>& bad:{std::vector<uint8_t>{2,1,'x'},std::vector<uint8_t>{1,3,'a','b'},
                                           std::vector<uint8_t>{1,0},std::vector<uint8_t>{1},
                                           std::vector<uint8_t>(80,1)}) {
        nvs.records["watch_sel"]=bad;
        CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Invalid && id[0]==0);
        CHECK(nvs.records["watch_sel"]==bad);
    }
    nvs.readable=false;
    CHECK(store.loadSelection(id,sizeof(id))==PrefResult::Unavailable);
    nvs.readable=true;
    nvs.writable=false;
    CHECK(store.saveSelection("digital")==PrefResult::WriteFailed);
    nvs.writable=true;
    // Face records: up to 64 bytes of payload under the face's key.
    uint8_t payload[FacePayloadMax+1]{},back[FacePayloadMax]{};
    size_t size=0;
    for (size_t i=0;i<sizeof(payload);++i) payload[i]=uint8_t(i*7);
    CHECK(store.saveFace("wf_forest",payload,FacePayloadMax)==PrefResult::Ok);
    CHECK(store.loadFace("wf_forest",back,sizeof(back),size)==PrefResult::Ok && size==FacePayloadMax);
    CHECK(std::memcmp(back,payload,size)==0);
    CHECK(store.saveFace("wf_forest",payload,FacePayloadMax+1)==PrefResult::TooLarge);
    CHECK(store.saveFace("wf_forest",payload,0)==PrefResult::Ok);
    CHECK(store.loadFace("wf_forest",back,sizeof(back),size)==PrefResult::Ok && size==0);
    // A payload longer than the caller can take is not cut short.
    store.saveFace("wf_forest",payload,8);
    CHECK(store.loadFace("wf_forest",back,4,size)==PrefResult::Invalid && size==0);
    // Keys: at most 15 of [a-z0-9_], never the host's own.
    CHECK(validRecordKey("wf_digital") && validRecordKey("abcdefghijklmno") && !validRecordKey("abcdefghijklmnop"));
    CHECK(!validRecordKey("") && !validRecordKey("WF") && !validRecordKey("wf-x") && !validRecordKey(nullptr));
    CHECK(store.saveFace("config",payload,1)==PrefResult::Invalid && store.saveFace("watch_sel",payload,1)==PrefResult::Invalid);
    CHECK(store.loadFace("config",back,sizeof(back),size)==PrefResult::Invalid);
}
// Each face reaches its own record and nothing else; the settings record
// survives every face write (docs/task10/plan-10-5.md 4).
void separation() {
    MemoryNvs nvs; SettingsStore settings; settings.begin(nvs);
    CHECK(settings.save({150,60}));
    const auto settingsRecord=nvs.records["config"];
    WatchPreferences store; store.bind(&nvs);
    FaceSelection selection; selection.bind(&store);
    auto* digital=selection.add({"digital","Digital","wf_digital"});
    auto* forest=selection.add({"forest","Forest","wf_forest"});
    auto* plain=selection.add({"plain","Plain",nullptr});
    CHECK(digital && forest && plain && digital->bound() && forest->bound() && !plain->bound());
    const uint8_t a[2]={1,1},b[2]={1,0};
    CHECK(digital->save(a,2)==PrefResult::Ok && forest->save(b,2)==PrefResult::Ok);
    uint8_t read[FacePayloadMax]; size_t size=0;
    CHECK(digital->load(read,sizeof(read),size)==PrefResult::Ok && size==2 && read[1]==1);
    CHECK(forest->load(read,sizeof(read),size)==PrefResult::Ok && size==2 && read[1]==0);
    CHECK(plain->save(a,2)==PrefResult::Unavailable && plain->load(read,sizeof(read),size)==PrefResult::Unavailable);
    CHECK(nvs.records.count("wf_digital") && nvs.records.count("wf_forest") && nvs.records.size()==3);
    CHECK(nvs.records["config"]==settingsRecord);
    SettingsStore reopened; CHECK(reopened.begin(nvs) && reopened.get().brightness==150 && reopened.get().screenOffSec==60);
    // Registration refuses what could reach another record.
    FaceSelection refused; refused.bind(&store);
    CHECK(refused.add({"digital","Digital","wf_digital"}));
    CHECK(!refused.add({"digital","Again","wf_other"}));        // id taken
    CHECK(!refused.add({"other","Other","wf_digital"}));        // key taken
    CHECK(!refused.add({"other","Other","config"}));            // the settings
    CHECK(!refused.add({"other","Other","watch_sel"}));         // the selection
    CHECK(!refused.add({"other","Other","Bad-Key"}));
    CHECK(!refused.add({"","Empty",nullptr}) && !refused.add({nullptr,"Null",nullptr}));
    CHECK(refused.add({"b",nullptr,nullptr}) && refused.add({"c",nullptr,nullptr}) && refused.add({"d",nullptr,nullptr}));
    CHECK(!refused.add({"e",nullptr,nullptr}) && refused.count()==FaceSelection::Capacity);
}
// The variant record, through Digital's and Forest's controls.
void variants() {
    MemoryNvs nvs; WatchPreferences store; store.bind(&nvs);
    FacePreferences digitalPrefs(&store,"wf_digital"),forestPrefs(&store,"wf_forest");
    DigitalControl digital; digital.bindPreferences(&digitalPrefs);
    CHECK(digital.variant()==TimeVariant::HourMinute && nvs.writes==0);  // missing: default, no write
    HomeEvent hold; hold.kind=HomeEventKind::LongPress;
    auto out=digital.handle(hold,{468,468});
    CHECK(out.changed && !out.saveFailed && digital.variant()==TimeVariant::HourMinuteSecond && nvs.writes==1);
    CHECK((nvs.records["wf_digital"]==std::vector<uint8_t>{1,2,1,1}));
    // Restored on the next start; the other face's record is untouched.
    DigitalControl restarted; restarted.bindPreferences(&digitalPrefs);
    CHECK(restarted.variant()==TimeVariant::HourMinuteSecond && !nvs.records.count("wf_forest"));
    // A failed save keeps the change on screen, says so, and is retried by
    // the next change even when the value returns to the stored one... which
    // then needs no write at all.
    nvs.writable=false;
    out=digital.handle(hold,{468,468});
    CHECK(out.changed && out.saveFailed && digital.variant()==TimeVariant::HourMinute);
    nvs.writable=true;
    const int before=nvs.writes;
    out=digital.handle(hold,{468,468});
    CHECK(!out.saveFailed && digital.variant()==TimeVariant::HourMinuteSecond && nvs.writes==before);
    out=digital.handle(hold,{468,468});
    CHECK(!out.saveFailed && nvs.writes==before+1 && (nvs.records["wf_digital"]==std::vector<uint8_t>{1,2,1,0}));
    // Unknown version, bad values, bad lengths: the default, nothing written.
    for (const std::vector<uint8_t>& bad:{std::vector<uint8_t>{1,2,2,1},std::vector<uint8_t>{1,2,1,7},
                                           std::vector<uint8_t>{1,1,1},std::vector<uint8_t>{1,3,1,1,0}}) {
        nvs.records["wf_forest"]=bad;
        const int writes=nvs.writes;
        ForestControl forest; forest.bindPreferences(&forestPrefs);
        CHECK(forest.variant()==TimeVariant::HourMinute && nvs.writes==writes && nvs.records["wf_forest"]==bad);
    }
    // Only a choice writes, and then in the current version.
    ForestControl forest; forest.bindPreferences(&forestPrefs);
    CHECK(!forest.handle(hold).saveFailed && (nvs.records["wf_forest"]==std::vector<uint8_t>{1,2,1,1}));
    // A face without storage keeps its variant in RAM and never fails.
    ForestControl loose; loose.bindPreferences(nullptr);
    CHECK(!loose.handle(hold).saveFailed && loose.variant()==TimeVariant::HourMinuteSecond);
    // Forest's taps mean nothing; its deadlines follow the variant.
    HomeEvent tap; tap.x=234; tap.y=400;
    const auto none=forest.handle(tap);
    CHECK(!none.changed && none.request==HomeRequest::None);
    WatchData d; d.timeValid=true; d.localTime.tm_sec=10; d.subsecondUs=250000;
    CHECK(forest.nextUpdate(0,d)==750000);
    forest.handle(hold);
    CHECK(forest.nextUpdate(0,d)==49750000);
}
// Which face starts, and what a choice does (docs/task10/plan-10-5.md 5).
void selection() {
    MemoryNvs nvs; WatchPreferences store; store.bind(&nvs);
    auto registry=[&](FaceSelection& s) {
        s.bind(&store);
        s.add({"digital","Digital","wf_digital"}); s.add({"forest","Forest","wf_forest"});
    };
    { FaceSelection s; registry(s); CHECK(s.startup()==0 && nvs.writes==0); }
    // Unknown and broken records show the first face and stay as they are.
    nvs.records["watch_sel"]={1,7,'s','u','n','d','i','a','l'};
    { FaceSelection s; registry(s); CHECK(s.startup()==0 && nvs.writes==0 && nvs.records["watch_sel"].size()==9); }
    nvs.records["watch_sel"]={9,1,'x'};
    { FaceSelection s; registry(s); CHECK(s.startup()==0 && nvs.writes==0); }
    nvs.readable=false;
    { FaceSelection s; registry(s); CHECK(s.startup()==0); }
    nvs.readable=true;
    FaceSelection s; registry(s);
    CHECK(s.startup()==0); s.shown(0);
    int begins=0; bool ok=true;
    auto begin=[&](int) { ++begins; return ok; };
    // Moving to Forest begins it and stores its id.
    CHECK(s.choose("forest",begin)==FaceChoiceResult::Selected && begins==1 && s.current()==1);
    char id[WatchFaceIdMax+1]; store.loadSelection(id,sizeof(id));
    CHECK(std::strcmp(id,"forest")==0);
    // The same face again: nothing begun, nothing written.
    const int writes=nvs.writes;
    CHECK(s.choose("forest",begin)==FaceChoiceResult::Unchanged && begins==1 && nvs.writes==writes);
    CHECK(s.choose("sundial",begin)==FaceChoiceResult::Unknown && begins==1);
    // A face that cannot begin: the previous one stays, nothing is stored.
    ok=false;
    CHECK(s.choose("digital",begin)==FaceChoiceResult::Failed && s.current()==1 && nvs.writes==writes);
    ok=true;
    // Shown but not stored: the next start shows the last stored face; the
    // same choice again retries only the storing.
    nvs.writable=false;
    CHECK(s.choose("digital",begin)==FaceChoiceResult::SaveFailed && s.current()==0 && begins==3);
    { FaceSelection next; registry(next); CHECK(next.startup()==1); }
    nvs.writable=true;
    CHECK(s.choose("digital",begin)==FaceChoiceResult::Selected && begins==3);
    { FaceSelection next; registry(next); CHECK(next.startup()==0); }
    // The stored face is restored at start.
    s.choose("forest",begin);
    { FaceSelection next; registry(next); CHECK(next.startup()==1); }
    // Without storage every choice shows but none is kept.
    FaceSelection loose; loose.add({"digital","Digital","wf_digital"}); loose.add({"forest","Forest","wf_forest"});
    CHECK(loose.startup()==0); loose.shown(0);
    CHECK(loose.choose("forest",begin)==FaceChoiceResult::SaveFailed && loose.current()==1);
    // The built-in faces in HomeLayer's order: Analog third, Noonish fourth,
    // stored and restored like the others. Choosing writes the selection only.
    MemoryNvs three; WatchPreferences threeStore; threeStore.bind(&three);
    auto builtIn=[&](FaceSelection& f) {
        f.bind(&threeStore);
        CHECK(f.add({"digital","Digital","wf_digital"}) && f.add({"forest","Forest","wf_forest"}) &&
              f.add({"analog","Analog","wf_analog"}) && f.add({"noonish","Noonish","wf_noonish"}));
    };
    FaceSelection faces; builtIn(faces);
    CHECK(faces.count()==4 && faces.find("analog")==2 && std::strcmp(faces.at(2).name,"Analog")==0);
    CHECK(faces.find("noonish")==3 && std::strcmp(faces.at(3).name,"Noonish")==0);
    CHECK(faces.startup()==0); faces.shown(0);
    CHECK(faces.choose("analog",begin)==FaceChoiceResult::Selected && faces.current()==2);
    { FaceSelection next; builtIn(next); CHECK(next.startup()==2); }
    CHECK(faces.choose("noonish",begin)==FaceChoiceResult::Selected && faces.current()==3);
    { FaceSelection next; builtIn(next); CHECK(next.startup()==3); }
    CHECK(faces.choose("digital",begin)==FaceChoiceResult::Selected);
    { FaceSelection next; builtIn(next); CHECK(next.startup()==0); }
    CHECK(three.records.size()==1 && three.records.count("watch_sel"));
}
// Below 30% or charging, no hysteresis, failures never read as low.
void forestBattery() {
    for (bool was:{false,true}) {
        CHECK(infoBatteryShown(29,false,true,was) && !infoBatteryShown(30,false,true,was) &&
              !infoBatteryShown(31,false,true,was));
        CHECK(infoBatteryShown(30,true,true,was) && infoBatteryShown(100,true,true,was) && infoBatteryShown(0,false,true,was));
        CHECK(infoBatteryShown(-1,true,true,was));              // charging, level unknown
        CHECK(infoBatteryShown(-1,false,true,was)==was);        // unknown level: kept
        CHECK(infoBatteryShown(50,false,false,was)==was);       // charging unreadable: kept
        CHECK(infoBatteryShown(12,false,false,was));            // the level still decides
        CHECK(infoBatteryShown(101,false,true,was)==was);       // out of range is unknown
    }
    // Through the control: the layout follows the battery and the items, and
    // the same frame twice changes nothing.
    ForestControl c; WatchData d; d.batteryPercent=80; d.charging=false;
    c.update(d); CHECK(!c.batteryShown() && !c.info());
    d.charging=true; c.update(d); CHECK(c.batteryShown() && c.info());
    d.charging=false; d.batteryPercent=29; c.update(d); c.update(d); CHECK(c.batteryShown());
    d.batteryPercent=-1; c.update(d); CHECK(c.batteryShown());       // unknown after showing: kept
    d.batteryPercent=30; c.update(d); CHECK(!c.batteryShown() && !c.info());
    d.batteryPercent=-1; c.update(d); CHECK(!c.batteryShown());      // unknown, not showing: hidden
    d.background.count=3; c.update(d); CHECK(c.items()==2 && c.info() && !c.batteryShown());
    BackgroundSnapshot s; s.count=3;
    for (int i=0;i<3;++i) { s.items[i].appId=static_cast<LaunchTargetId>(40+i); s.items[i].nextChangeAt=1000*(i+1); }
    CHECK(c.backgroundInterest(s).count==2);
}
// Every part inside the round panel, the time on plain sky and the row on
// plain ground in both layouts and variants, the scenery moved as a whole.
namespace { bool near(float a,float b,float tolerance=1e-3f) { return std::abs(a-b)<=tolerance; } }
void forestLayoutRules() {
    for (int side:{466,468}) {
        const Viewport v{side,side};
        const float r=side/2.0f;
        auto inside=[&](Rect b) {
            for (int x:{b.x,b.x+b.w}) for (int y:{b.y,b.y+b.h}) {
                const float dx=x-r,dy=y-r; if (dx*dx+dy*dy>r*r) return false;
            }
            return true;
        };
        for (const auto variant:{TimeVariant::HourMinute,TimeVariant::HourMinuteSecond})
            for (bool info:{false,true}) {
                const auto l=forestLayout(v,variant,info);
                const auto plain=forestLayout(v,variant,false);
                if (info) {
                    CHECK(std::abs((plain.groundTop-l.groundTop)-80*l.scale)<1.5f);
                    CHECK(plain.timeBaseline-l.timeBaseline==int(177.5f*l.scale)-int(145.5f*l.scale));
                }
                const bool seconds=variant==TimeVariant::HourMinuteSecond;
                for (const Rect& box:{l.time.hour,l.time.minute,l.time.second}) {
                    if (box.empty()) continue;
                    // On plain sky: every row's centre above the sky's gradient.
                    CHECK(inside(box) && box.y+box.h<=l.skyFadeTop);
                    // No tree reaches the time: the highest it comes under the
                    // box, its apex or its side where the box ends short of
                    // the apex, is below the box.
                    for (const auto& t:l.trees) {
                        if (!(box.x<t.rx+1 && t.lx-1<box.x+box.w)) continue;
                        const float dx=std::max({0.0f,float(box.x)-t.ax,t.ax-float(box.x+box.w)});
                        CHECK(box.y+box.h<t.ay+dx*(t.by-t.ay)/((t.rx-t.lx)/2)-1);
                    }
                }
                CHECK(l.time.hour.x+l.time.hour.w==l.time.minute.x);
                if (seconds) CHECK(l.time.minute.x+l.time.minute.w==l.time.second.x);
                if (!info) continue;
                // The widest the row can be, with the battery and two items.
                const int limit=infoGroupWidthLimit(l.row,3);
                const int widths[3]={limit,limit,limit};
                Rect groups[3];
                CHECK(placeInfoRow(l.row,widths,3,groups)==3);
                for (const auto& g:groups) {
                    const Rect padded{g.x-2,g.y-2,g.w+4,g.h+4};
                    // On plain ground: below the ground's gradient.
                    CHECK(inside(padded) && padded.y>=l.groundFadeBottom);
                    for (const auto& t:l.trees) if (padded.x<t.rx+1 && t.lx-1<padded.x+padded.w) CHECK(padded.y>t.by+1);
                }
                CHECK(groups[0].x+3*limit+2*l.row.groupGap==groups[2].x+groups[2].w);
                // Shares: a narrow battery leaves its room to the items, so the
                // reference row (18% and two 02:40) is not shortened; labels
                // too long for the row share it equally, and the row fits.
                {
                    const int wanted[3]={70,97,97};
                    int limits[3]{};
                    infoGroupLimits(l.row,wanted,3,limits);
                    CHECK(limits[0]>=70 && limits[1]>=97 && limits[2]>=97);
                    const int wide[3]={70,400,300};
                    infoGroupLimits(l.row,wide,3,limits);
                    const int room=l.row.width-2*l.row.groupGap;
                    CHECK(limits[0]>=70 && limits[1]==limits[2] && 70+limits[1]+limits[2]<=room);
                    CHECK(limits[1]>=infoGroupWidthLimit(l.row,3));
                    const int one[1]={900};
                    infoGroupLimits(l.row,one,1,limits);
                    CHECK(limits[0]==l.row.width);
                }
                // Reference spacing at 466: centred on 377.
                if (side==466) CHECK(l.row.y==377 && l.row.iconSize==28 && l.row.groupGap==28);
            }
        // The trees keep their shape, and the higher a base the further back.
        const auto l=forestLayout(v,TimeVariant::HourMinute,false);
        const auto withInfo=forestLayout(v,TimeVariant::HourMinute,true);
        for (size_t i=0;i<ForestTrees.size();++i) {
            const auto& t=l.trees[i];
            const auto& r=ForestTrees[i];
            CHECK(near(t.by-t.ay,(r.baseY-r.apexY)*l.scale,1e-3f) && near(t.rx-t.lx,2*r.half*l.scale,1e-3f));
            CHECK(near(t.ay-withInfo.trees[i].ay,ForestInfoShift*l.scale,1e-3f) && t.depth==withInfo.trees[i].depth);
            for (const auto& u:l.trees) if (u.by<t.by) CHECK(u.depth>t.depth);
        }
        CHECK(near(l.skyFadeTop-withInfo.skyFadeTop,ForestInfoShift*l.scale,1e-3f));
        CHECK(near(l.groundFadeBottom-withInfo.groundFadeBottom,ForestInfoShift*l.scale,1e-3f));
        // Reference rows at 466: the far trees take the most haze, the nearest none.
        if (side==466) {
            CHECK(l.groundTop==372 && withInfo.groundTop==292 && near(withInfo.trees[5].ay,186));
            CHECK(near(withInfo.skyFadeTop,192.5f) && near(withInfo.groundFadeBottom,341));
            CHECK(near(forest::haze(withInfo.trees[0],forest::Day),0.7f*42/40) && forest::haze(withInfo.trees[17],forest::Day)==0);
        }
    }
}
// Forest's colours (docs/forest-gradient/plan.md 4): the hourly palette and
// the gradients drawn from it.
void forestPalettes() {
    using forest::hex;
    auto same=[](ForestColor a,ForestColor b) { return near(a.r,b.r) && near(a.g,b.g) && near(a.b,b.b); };
    // The keys themselves, the day held between its two keys, noon while unknown.
    CHECK(forestPalette(4)==forest::Night && forestPalette(6)==forest::Dawn && forestPalette(18)==forest::Dusk);
    for (int h=9;h<=16;++h) CHECK(forestPalette(h)==forest::Day);
    CHECK(forestPalette(-1)==forest::Day && forestPalette(24)==forest::Day);
    // From dawn to the day a third per hour, from the day to dusk half...
    CHECK(same(forestPalette(7).skyTop,forest::mix(forest::Dawn.skyTop,forest::Day.skyTop,1.0f/3)));
    CHECK(near(forestPalette(17).haze,(forest::Day.haze+forest::Dusk.haze)/2));
    // ...and across midnight: two keys, 18 and 6, twelve hours apart either way.
    ForestPalette a{},b{};
    a.skyTop=hex(0x000000); b.skyTop=hex(0xffffff);
    const std::array<ForestPaletteKey,2> keys{{{6,a},{18,b}}};
    CHECK(same(forestPaletteAt(keys,0).skyTop,{127.5f,127.5f,127.5f}));
    CHECK(same(forestPaletteAt(keys,21).skyTop,{191.25f,191.25f,191.25f}));
    CHECK(forestPaletteAt(keys,6)==a && forestPaletteAt(keys,18)==b);
    const std::array<ForestPaletteKey,1> one{{{3,b}}};
    CHECK(forestPaletteAt(one,0)==b && forestPaletteAt(one,23)==b);
    // RGB565 rounds to the nearest step.
    CHECK(forest::rgb565(hex(0xffffff))==0xffff && forest::rgb565(hex(0x000000))==0);
    CHECK(forest::rgb565(hex(0x46c9e6))==0x4e5c);
    // The gradients end in their plain colours exactly, so the time and the
    // row drawn over those colours meet the scenery without a seam.
    const auto l=forestLayout({466,466},TimeVariant::HourMinute,true);
    const auto& p=forest::Day;
    CHECK(same(forest::sky(l,p,l.skyFadeTop-0.5f),p.skyTop) && same(forest::sky(l,p,l.skyFadeBottom+0.5f),p.skyBottom));
    CHECK(same(forest::ground(l,p,l.groundFadeBottom+0.5f),p.groundBottom) && same(forest::ground(l,p,l.groundFadeTop),p.groundTop));
    // A near tree is its own colour; a far one leans to the sky of its rows.
    const auto& nearTree=l.trees[17];
    CHECK(same(forest::tree(l,p,nearTree,nearTree.by),p.treeBottom));
    const auto& farTree=l.trees[0];
    const float y=(farTree.ay+farTree.by)/2;
    const ForestColor own=forest::mix(p.treeTop,p.treeBottom,0.5f);
    CHECK(same(forest::tree(l,p,farTree,y),forest::mix(own,forest::sky(l,p,y),forest::haze(farTree,p))));
    // The control's hour: the time's, or none while it is unknown.
    ForestControl c; WatchData d; d.timeValid=true; d.localTime.tm_hour=19;
    c.update(d); CHECK(c.hour()==19);
    d.timeValid=false; c.update(d); CHECK(c.hour()==-1);
    d.timeValid=true; d.localTime.tm_hour=24; c.update(d); CHECK(c.hour()==-1);
    // The list lies on the hour's plain ground under the row, noon's while
    // the time is unknown, and blends between the keys like the scenery.
    CHECK(forestListBackground(12)==forest::rgb565(forest::Day.groundBottom));
    CHECK(forestListBackground(2)==forest::rgb565(forest::Night.groundBottom));
    CHECK(forestListBackground(-1)==forestListBackground(12));
    CHECK(forestListBackground(17)==forest::rgb565(forest::mix(forest::Day.groundBottom,forest::Dusk.groundBottom,0.5f)));
    CHECK(forestListBackground(forestHour(d))==forestListBackground(-1));
    d.localTime.tm_hour=19; CHECK(forestListBackground(forestHour(d))==forestListBackground(19));
    // The home gesture's band is the hour's sky at the top of the panel, where
    // it gives way to the scenery (docs/task14/plan.md 2.3).
    for (int hour:{-1,2,6,12,17,19,22}) {
        CHECK(forestHomeGestureBackground(hour)==forest::rgb565(forestPalette(hour).skyTop));
        CHECK(forestHomeGestureBackground(hour)==forest::rgb565(forest::sky(l,forestPalette(hour),0.5f)));
    }
    CHECK(forestHomeGestureBackground(12)!=forestHomeGestureBackground(22));
}
namespace {
WatchData clockAt(int hour,int minute,int second,int day=20,TimeUs subsecond=0) {
    WatchData d; d.timeValid=true;
    d.localTime.tm_hour=hour; d.localTime.tm_min=minute; d.localTime.tm_sec=second; d.localTime.tm_mday=day;
    d.subsecondUs=subsecond;
    return d;
}
bool near(AnalogPoint a,AnalogPoint b) { return near(a.x,b.x) && near(a.y,b.y); }
bool sameStroke(const AnalogStroke& a,const AnalogStroke& b) { return near(a.a,b.a) && near(a.b,b.b) && a.r==b.r; }
std::string dateText(const AnalogTime& t) { char text[3]; formatAnalogDate(t,text); return text; }
// drawWideLineClipped's coverage of pixel (x,y): whether it draws there.
bool strokeReaches(const AnalogStroke& s,int x,int y) {
    const float dx=s.b.x-s.a.x,dy=s.b.y-s.a.y,length=dx*dx+dy*dy;
    const float px=x-s.a.x,py=y-s.a.y;
    const float t=length>0 ? std::clamp((px*dx+py*dy)/length,0.0f,1.0f) : 0.0f;
    const float ex=px-dx*t,ey=py-dy*t;
    return s.r+0.5f-std::sqrt(ex*ex+ey*ey)>1.0f/32;
}
// Every pixel the stroke draws lies in its bounds, and the bounds are no
// wider than the reach they allow for.
void checkBounds(const AnalogStroke& s) {
    const Rect b=strokeBounds(s);
    for (int y=b.y-4;y<b.y+b.h+4;++y) for (int x=b.x-4;x<b.x+b.w+4;++x)
        if (strokeReaches(s,x,y)) CHECK(b.contains(x,y));
    CHECK(b.w<=int(std::abs(s.a.x-s.b.x)+2*(s.r+analog::Edge))+3);
    CHECK(b.h<=int(std::abs(s.a.y-s.b.y)+2*(s.r+analog::Edge))+3);
}
}
// The reading the hands show: steps of ten seconds over twelve hours, the dot
// by the second, the day (docs/task11/plan-11-1.md 3).
void analogTimeRules() {
    const auto l=analogLayout({466,466},AnalogDateX);
    // Twelve, three, six and nine o'clock.
    const AnalogPoint c=l.centre;
    CHECK(near(c,{232.5f,232.5f}));
    const float hourAt[4]={0,90,180,270};
    for (int i=0;i<4;++i) {
        const auto t=analogTime(clockAt(3*i,0,0));
        CHECK(t.valid && analogHourDegrees(t)==hourAt[i] && analogMinuteDegrees(t)==0);
    }
    CHECK(near(analogHour(l,analogTime(clockAt(3,0,0))).b,{c.x+122,c.y}));
    CHECK(near(analogHour(l,analogTime(clockAt(6,0,0))).b,{c.x,354.5f}));     // the reference's six
    CHECK(near(analogHour(l,analogTime(clockAt(9,0,0))).b,{c.x-122,c.y}));
    CHECK(near(analogMinute(l,analogTime(clockAt(9,0,0))).b,{c.x,63.5f}));    // the reference's twelve
    CHECK(near(analogMinute(l,analogTime(clockAt(9,15,0))).b,{c.x+169,c.y}));
    CHECK(near(analogMinute(l,analogTime(clockAt(9,30,0))).b,{c.x,c.y+169}));
    CHECK(near(analogMinute(l,analogTime(clockAt(9,45,0))).b,{c.x-169,c.y}));
    CHECK(near(analogSecond(l,analogTime(clockAt(9,0,0))).a,{c.x,33}));      // the reference's dot, centred
    CHECK(near(analogSecond(l,analogTime(clockAt(9,0,15))).a,{c.x+199.5f,c.y}));
    // Halfway: 1:30 puts the hour hand at 45 degrees; 10:07 as the Noonish
    // reference reads.
    CHECK(analogHourDegrees(analogTime(clockAt(1,30,0)))==45);
    const auto ten=analogTime(clockAt(10,7,0));
    CHECK(ten.step==3642 && analogHourDegrees(ten)==303.5f && analogMinuteDegrees(ten)==42);
    // Twelve hours apart: the same hands, the same keys.
    for (int h=0;h<12;++h) {
        const auto a=analogTime(clockAt(h,34,56)),b=analogTime(clockAt(h+12,34,56));
        CHECK(a.step==b.step && a.hourKey()==b.hourKey() && a.minuteKey()==b.minuteKey() && a.secondKey()==b.secondKey());
    }
    // Within ten seconds only the dot moves: same strokes, same keys.
    const auto first=analogTime(clockAt(12,34,10));
    for (int s=10;s<20;++s) {
        const auto t=analogTime(clockAt(12,34,s));
        CHECK(t.step==first.step && t.hourKey()==first.hourKey() && t.minuteKey()==first.minuteKey());
        CHECK(sameStroke(analogHour(l,t),analogHour(l,first)) && sameStroke(analogMinute(l,t),analogMinute(l,first)));
        CHECK(t.second==s && t.secondKey()==s+1 && analogSecondDegrees(t)==6.0f*s);
    }
    // :09 to :10 and :59 to :00: the minute hand one degree, the hour hand a
    // twelfth of one.
    auto next=[&](WatchData a,WatchData b) {
        const auto x=analogTime(a),y=analogTime(b);
        CHECK(analogMinuteDegrees(y)==analogMinuteDegrees(x)+1 && near(analogHourDegrees(y)-analogHourDegrees(x),1/12.0f,1e-4f));
        CHECK(x.hourKey()!=y.hourKey() && x.minuteKey()!=y.minuteKey());
    };
    next(clockAt(12,34,9),clockAt(12,34,10));
    next(clockAt(12,34,59),clockAt(12,35,0));
    CHECK(analogMinuteDegrees(analogTime(clockAt(12,35,0)))==210);
    // 11:59:5x to 12:00, and 23:59:5x to the next day: round to zero, and the
    // date moves on.
    for (int h:{11,23}) {
        const auto before=analogTime(clockAt(h,59,59,20)),after=analogTime(clockAt((h+1)%24,0,0,h==23 ? 21 : 20));
        CHECK(before.step==4319 && analogMinuteDegrees(before)==359 && near(analogHourDegrees(before),359+11/12.0f,1e-3f));
        CHECK(after.step==0 && analogMinuteDegrees(after)==0 && analogHourDegrees(after)==0);
        CHECK(dateText(after)==(h==23 ? "21" : "20"));
    }
    // One turn: every step in range, the hour hand always forwards.
    for (int step=0;step<4320;++step) {
        const auto t=analogTime(clockAt(step/360,(step%360)/6,(step%6)*10));
        CHECK(t.step==step && analogMinuteDegrees(t)>=0 && analogMinuteDegrees(t)<360);
        CHECK(near(analogHourDegrees(t),step/12.0f,1e-4f) && analogHourDegrees(t)<360);
    }
    // A leap second shows as :59.
    const auto leap=analogTime(clockAt(12,34,60));
    CHECK(leap.valid && leap.second==59 && leap.step==analogTime(clockAt(12,34,59)).step);
    // The day without a leading zero.
    CHECK(dateText(analogTime(clockAt(1,0,0,1)))=="1" && dateText(analogTime(clockAt(1,0,0,9)))=="9");
    CHECK(dateText(analogTime(clockAt(1,0,0,10)))=="10" && dateText(analogTime(clockAt(1,0,0,31)))=="31");
    // Invalid is not 0:00: no keys, the date as --.
    WatchData unset=clockAt(0,0,0); unset.timeValid=false;
    for (const auto& d:{unset,clockAt(24,0,0),clockAt(-1,0,0),clockAt(1,60,0),clockAt(1,0,61),clockAt(1,0,-1),
                        clockAt(1,0,0,0),clockAt(1,0,0,32)}) {
        const auto t=analogTime(d);
        CHECK(!t.valid && t.hourKey()==0 && t.minuteKey()==0 && t.secondKey()==0 && dateText(t)=="--");
    }
    const auto midnight=analogTime(clockAt(0,0,0));
    CHECK(midnight.hourKey()!=0 && midnight.minuteKey()!=0 && midnight.secondKey()!=0);
}
// Fixed parts, strokes inside the round panel and inside their bounds, the
// date and the row where the references have them.
void analogLayoutRules() {
    for (const Viewport v:{Viewport{466,466},Viewport{468,468},Viewport{480,466}}) {
        const int side=std::min(v.width,v.height);
        for (float dateX:{AnalogDateX,NoonishDateX}) {
            const auto l=analogLayout(v,dateX);
            // Centred in the viewport; the date on the centre's horizontal.
            CHECK(near(l.centre,{(v.width-1)/2.0f,(v.height-1)/2.0f}) && l.date.y==l.centre.y);
            const float radius=side/2.0f;
            auto inside=[&](Rect b) {
                for (float x:{b.x-0.5f,b.x+b.w-0.5f}) for (float y:{b.y-0.5f,b.y+b.h-0.5f}) {
                    const float dx=x-l.centre.x,dy=y-l.centre.y; if (dx*dx+dy*dy>radius*radius) return false;
                }
                return true;
            };
            // Whatever the angle, the hands and the dot stay on the panel.
            for (const auto& s:{analogHour(l,{}),analogMinute(l,{}),analogSecond(l,{})}) {
                const float reach=std::hypot(s.b.x-l.centre.x,s.b.y-l.centre.y)+s.r+analog::Edge;
                CHECK(reach<radius);
            }
            CHECK(inside(strokeBounds(analogSecond(l,analogTime(clockAt(0,0,0))))));
            // The date wider than the reference's "20" (44px), and the row
            // with the battery and two items at their limit.
            const auto date=placeAnalogDate(l,int(48*l.scale),int(26*l.scale),0);
            CHECK(inside(date.box));
            const int limit=infoGroupWidthLimit(l.row,3);
            const int widths[3]={limit,limit,limit};
            Rect groups[3];
            CHECK(placeInfoRow(l.row,widths,3,groups)==3);
            for (const auto& g:groups) CHECK(inside({g.x-2,g.y-2,g.w+4,g.h+4}));
            if (side!=466 || v.width!=466) continue;
            // The references' measures.
            CHECK(l.hourLength==122 && l.hourRadius==5 && l.minuteLength==169 && l.minuteRadius==2.5f);
            CHECK(l.hubRadius==12 && l.secondOrbit==199.5f && l.secondRadius==11.5f);
            CHECK(l.row.y==357 && l.row.iconSize==28 && l.row.groupGap==28 && l.row.batteryWidth==18);
            CHECK(strokeBounds(analogHub(l))==(Rect{219,219,28,28}));
            // "20" is 44px wide with 26px of ink above the baseline: Analog's
            // at 340..383 and Noonish's (43px) at 348..390, rows 220..245.
            if (dateX==AnalogDateX) {
                const auto p=placeAnalogDate(l,44,26,0);
                CHECK(p.x==340 && p.baseline==246 && p.box==(Rect{338,218,48,30}));
            } else {
                const auto p=placeAnalogDate(l,43,26,0);
                CHECK(p.x==348 && p.baseline==246);
            }
        }
        // Every pixel a stroke draws is inside its bounds: the axes, the
        // diagonals and odd angles, and the dot at every second.
        const auto l=analogLayout(v,AnalogDateX);
        for (float degrees:{0.0f,1/12.0f,1.0f,30.0f,42.0f,45.0f,89.0f,90.0f,135.0f,179.0f,180.0f,225.0f,270.0f,303.5f,359.0f}) {
            checkBounds({l.centre,analogPoint(l,l.hourLength,degrees),l.hourRadius});
            checkBounds({l.centre,analogPoint(l,l.minuteLength,degrees),l.minuteRadius});
        }
        for (int s=0;s<60;++s) checkBounds(analogSecond(l,analogTime(clockAt(0,0,s))));
        checkBounds(analogHub(l));
        // A hand can turn inside the same bounds: why the keys, and not the
        // rectangles, say that it moved.
        const auto a=analogTime(clockAt(0,0,0)),b=analogTime(clockAt(0,0,10));
        CHECK(strokeBounds(analogHour(l,a))==strokeBounds(analogHour(l,b)) && a.hourKey()!=b.hourKey());
    }
}
// Analog's and Noonish's controls: each its own record, the dot by a long
// press only, the battery and items as Forest's, deadlines every ten seconds
// or every second (docs/task11/plan-11-1.md 4).
void analogControlRules() {
    MemoryNvs nvs; WatchPreferences store; store.bind(&nvs);
    FacePreferences analogPrefs(&store,"wf_analog"),noonishPrefs(&store,"wf_noonish");
    AnalogControl analog,noonish;
    analog.bindPreferences(&analogPrefs); noonish.bindPreferences(&noonishPrefs);
    CHECK(!analog.seconds() && !noonish.seconds() && nvs.writes==0);    // missing: no dot, no write
    HomeEvent hold; hold.kind=HomeEventKind::LongPress;
    HomeEvent tap; tap.x=234; tap.y=234;
    // A tap means nothing, and stores nothing.
    auto out=analog.handle(tap);
    CHECK(!out.changed && !out.saveFailed && out.request==HomeRequest::None && !analog.seconds() && nvs.writes==0);
    out=analog.handle(hold);
    CHECK(out.changed && !out.saveFailed && analog.seconds() && !noonish.seconds() && nvs.writes==1);
    CHECK((nvs.records["wf_analog"]==std::vector<uint8_t>{1,2,1,1}) && !nvs.records.count("wf_noonish"));
    // The other face's choice goes to its own record and leaves this one.
    noonish.handle(hold); noonish.handle(hold);
    CHECK(!noonish.seconds() && (nvs.records["wf_noonish"]==std::vector<uint8_t>{1,2,1,0}));
    CHECK((nvs.records["wf_analog"]==std::vector<uint8_t>{1,2,1,1}));
    // Restored on the next start, each from its own record.
    {
        AnalogControl a,n; a.bindPreferences(&analogPrefs); n.bindPreferences(&noonishPrefs);
        CHECK(a.seconds() && !n.seconds());
    }
    // Unknown version, bad values, bad lengths: no dot, nothing written.
    for (const std::vector<uint8_t>& bad:{std::vector<uint8_t>{1,2,2,1},std::vector<uint8_t>{1,2,1,7},
                                           std::vector<uint8_t>{1,1,1},std::vector<uint8_t>{1,3,1,1,0}}) {
        nvs.records["wf_noonish"]=bad;
        const int writes=nvs.writes;
        AnalogControl n; n.bindPreferences(&noonishPrefs);
        CHECK(!n.seconds() && nvs.writes==writes && nvs.records["wf_noonish"]==bad);
    }
    // A failed save keeps the change and says so; the next change retries,
    // and needs no write when it returns to what is stored.
    nvs.writable=false;
    out=analog.handle(hold);
    CHECK(out.changed && out.saveFailed && !analog.seconds());
    nvs.writable=true;
    int writes=nvs.writes;
    out=analog.handle(hold);
    CHECK(!out.saveFailed && analog.seconds() && nvs.writes==writes);
    out=analog.handle(hold);
    CHECK(!out.saveFailed && nvs.writes==writes+1 && (nvs.records["wf_analog"]==std::vector<uint8_t>{1,2,1,0}));
    // Without storage the dot is kept in RAM and nothing fails.
    AnalogControl loose; loose.bindPreferences(nullptr);
    CHECK(!loose.handle(hold).saveFailed && loose.seconds());
    // Deadlines without the dot: the next ten-second boundary, never now.
    CHECK(!analog.seconds());
    CHECK(analog.nextUpdate(0,clockAt(12,34,9,20,750000))==250000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,10,20,0))==10000000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,19,20,999999))==1);
    CHECK(analog.nextUpdate(1000,clockAt(12,34,15,20,500000))==1000+4500000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,59,20,500000))==500000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,60,20,250000))==750000);     // leap second: the same :00
    // The latest reading decides, forwards or back: nothing is caught up.
    CHECK(analog.nextUpdate(5000000,clockAt(12,0,3))==12000000);
    CHECK(analog.nextUpdate(6000000,clockAt(11,59,55))==11000000);
    // Items with earlier label deadlines do not change the clock's own.
    WatchData busy=clockAt(12,34,9,20,750000);
    busy.background.count=2; busy.background.items[0].nextChangeAt=1; busy.background.items[1].nextChangeAt=2;
    CHECK(analog.nextUpdate(0,busy)==250000);
    // With the dot: every second.
    analog.handle(hold);
    CHECK(analog.nextUpdate(0,clockAt(12,34,9,20,750000))==250000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,10,20,0))==1000000);
    CHECK(analog.nextUpdate(0,clockAt(12,34,12,20,400000))==600000);
    // Invalid time or reading: no deadline of the clock's own.
    for (bool seconds:{true,false}) {
        if (analog.seconds()!=seconds) analog.handle(hold);
        WatchData unset=clockAt(12,34,9); unset.timeValid=false;
        for (const auto& d:{unset,clockAt(12,34,9,20,-1),clockAt(12,34,9,20,1000000),clockAt(12,34,61),clockAt(24,0,0)})
            CHECK(analog.nextUpdate(0,d)==INT64_MAX);
    }
    writes=nvs.writes;
    // Battery and items as Forest's; the same frame twice changes nothing, and
    // neither frames nor deadline queries store anything.
    AnalogControl c; c.bindPreferences(&analogPrefs);
    const int reads=nvs.reads;
    WatchData d=clockAt(12,0,0); d.batteryPercent=31;
    c.update(d); CHECK(!c.batteryShown());
    d.batteryPercent=30; c.update(d); CHECK(!c.batteryShown());
    d.batteryPercent=29; c.update(d); c.update(d); CHECK(c.batteryShown());
    d.batteryPercent=-1; c.update(d); CHECK(c.batteryShown());          // unknown after showing: kept
    d.batteryPercent=80; c.update(d); CHECK(!c.batteryShown());
    d.batteryPercent=-1; c.update(d); CHECK(!c.batteryShown());         // unknown, not showing: hidden
    d.charging=true; c.update(d); CHECK(c.batteryShown());              // charging, level unknown
    d.batteryPercent=80; d.chargingKnown=false; c.update(d); CHECK(c.batteryShown());  // unreadable: kept
    d.charging=false; d.chargingKnown=true; c.update(d); CHECK(!c.batteryShown());
    for (int n=0;n<=3;++n) { d.background.count=n; c.update(d); c.update(d); CHECK(c.items()==std::min(n,2)); }
    BackgroundSnapshot s; s.count=3;
    for (int i=0;i<3;++i) { s.items[i].appId=static_cast<LaunchTargetId>(40+i); s.items[i].nextChangeAt=1000*(i+1); }
    const auto interest=c.backgroundInterest(s);
    CHECK(interest.count==2 && interest.ids[0]==s.items[0].appId && interest.ids[1]==s.items[1].appId);
    const bool seconds=c.seconds();
    for (int i=0;i<3;++i) c.nextUpdate(0,d);
    CHECK(c.seconds()==seconds && nvs.writes==writes && nvs.reads==reads);
}
namespace {
struct Rgb { int r,g,b; };
Rgb rgbOf(uint16_t c) { return {(c>>11)<<3,((c>>5)&0x3f)<<2,(c&0x1f)<<3}; }
uint16_t rgb565(int r,int g,int b) { return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3)); }
AnalogTime timeAt(int hour,int minute,int second=0) { return analogTime(clockAt(hour,minute,second)); }
// Pixel centres at `radius` from the centre towards `degrees`.
AnalogPoint towards(const AnalogLayout& l,float radius,float degrees) { return analogPoint(l,radius,degrees); }
}
// Noonish's regions, their blended edges and the lightened dot
// (docs/task11/plan-11-3.md 7).
void noonishBackgroundRules() {
    // The palette is the references' colours.
    CHECK(NoonishPalette[NoonishTop]==rgb565(96,196,218) && NoonishPalette[NoonishRight]==rgb565(144,214,229));
    CHECK(NoonishPalette[NoonishBottom]==rgb565(131,167,209) && NoonishPalette[NoonishLeft]==rgb565(78,130,189));
    CHECK(NoonishInk==rgb565(222,243,247));
    const auto l=analogLayout({466,466},NoonishDateX);
    // At 10:07, as the reference sits: the points measured there take its
    // colours (docs/task11/11-1-validation.md 2).
    const auto rest=noonishSplit(l,timeAt(10,7));
    CHECK(noonishColour(rest,233,80)==NoonishPalette[NoonishTop] && noonishColour(rest,420,200)==NoonishPalette[NoonishRight]);
    CHECK(noonishColour(rest,233,420)==NoonishPalette[NoonishBottom] && noonishColour(rest,40,250)==NoonishPalette[NoonishLeft]);
    // The home gesture's band is the region at the middle of the top edge,
    // the colour that shows there once the band goes (docs/task14/plan.md 2.3).
    {
        CHECK(noonishTopColour(l,timeAt(10,7))==NoonishPalette[NoonishTop]);
        int seen=0;
        for (int step=0;step<4320;step+=7) {
            const auto t=timeAt(step/360,(step%360)/6,(step%6)*10);
            const auto s=noonishSplit(l,t);
            const uint16_t top=noonishTopColour(l,t);
            CHECK(top==noonishColour(s,int(l.centre.x),0));
            for (int i=0;i<4;++i) if (top==NoonishPalette[i]) seen|=1<<i;
        }
        CHECK(seen==0xf); // Every region reaches the top some time of the day.
    }
    // The split is the hands' step: the seconds do not move it, and an unknown
    // time rests at 10:07.
    const auto a=noonishSplit(l,timeAt(12,34,10)),b=noonishSplit(l,timeAt(12,34,19));
    CHECK(a.step==b.step && a.hx==b.hx && a.my==b.my);
    CHECK(noonishSplit(l,AnalogTime{}).step==NoonishRestStep && noonishSplit(l,AnalogTime{}).hx==rest.hx);
    // Unit vectors along the hands, all the way round.
    for (int step=0;step<4320;step+=7) {
        const auto s=noonishSplit(l,timeAt(step/360,(step%360)/6,(step%6)*10));
        CHECK(near(std::hypot(s.hx,s.hy),1.0f,1e-3f) && near(std::hypot(s.mx,s.my),1.0f,1e-3f));
    }
    // Every pixel of a frame is one of the four colours or, next to a line,
    // a blend of them; away from the lines exactly its region's colour.
    auto regionsSeen=[&](const NoonishSplit& s) {
        bool seen[4]{};
        for (int y=0;y<466;y+=3) for (int x=0;x<466;x+=3) {
            const float px=x-s.centre.x,py=y-s.centre.y;
            const float dh=s.hx*py-s.hy*px,dm=s.mx*py-s.my*px;
            if (std::fabs(dh)<noonish::Edge || std::fabs(dm)<noonish::Edge) continue;
            const auto r=noonishRegion(s,float(x),float(y));
            CHECK(noonishColour(s,x,y)==NoonishPalette[r]);
            seen[r]=true;
        }
        return int(seen[0])|int(seen[1])<<1|int(seen[2])<<2|int(seen[3])<<3;
    };
    CHECK(regionsSeen(rest)==0xf);
    // The hands together (12:00): the two regions between them are gone.
    // Opposite (6:00): the other two are.
    CHECK(regionsSeen(noonishSplit(l,timeAt(12,0)))==(1<<NoonishRight|1<<NoonishLeft));
    CHECK(regionsSeen(noonishSplit(l,timeAt(6,0)))==(1<<NoonishTop|1<<NoonishBottom));
    // Overtaking: from 3:00 to 3:59, a point at 150 degrees changes region
    // only when the minute line passes it, at :25 and again at :55 (its other
    // half). The hour line never reaches it. No reordering by angle.
    {
        const auto p=towards(l,150,150);
        auto regionAt=[&](int step) { return noonishRegion(noonishSplit(l,timeAt(3,step/6,(step%6)*10)),p.x,p.y); };
        NoonishRegion last=regionAt(0);
        int changes=0,first=-1,second=-1;
        for (int step=1;step<360;++step) {
            const auto r=regionAt(step);
            if (r==last) continue;
            ++changes; (first<0 ? first : second)=step; last=r;
        }
        CHECK(changes==2 && first/6==25 && second/6==55);
    }
    // Blended edges: next to the minute line at 10:07 (42 degrees), between
    // the two regions' colours channel by channel; the same every time.
    {
        int blended=0;
        for (int i=40;i<160;++i) {
            const auto p=towards(l,float(i),42);
            const int x=int(std::lround(p.x)),y=int(std::lround(p.y));
            const uint16_t c=noonishColour(rest,x,y);
            CHECK(c==noonishColour(rest,x,y));
            if (c==NoonishPalette[NoonishTop] || c==NoonishPalette[NoonishRight]) continue;
            ++blended;
            const Rgb v=rgbOf(c),t=rgbOf(NoonishPalette[NoonishTop]),r=rgbOf(NoonishPalette[NoonishRight]);
            CHECK(v.r>=std::min(t.r,r.r) && v.r<=std::max(t.r,r.r) && v.g>=std::min(t.g,r.g) && v.g<=std::max(t.g,r.g));
            CHECK(v.b>=std::min(t.b,r.b) && v.b<=std::max(t.b,r.b));
        }
        CHECK(blended>20);
        // A line exactly between pixel centres (12:00 at x=232.5) needs no
        // blending: 232 and 233 are half a pixel from it, on either side.
        const auto noon=noonishSplit(l,timeAt(12,0));
        CHECK(noonishColour(noon,232,100)==NoonishPalette[NoonishLeft] && noonishColour(noon,233,100)==NoonishPalette[NoonishRight]);
        CHECK(noonishColour(noon,232,232)==noonishColour(noon,232,232));    // the centre: defined
    }
    // Painting by runs gives exactly the pixels noonishColour gives, whole
    // rows and partial ones, at every angle, including the hands flat along
    // a row (3:00, 9:00, 3:15) and on top of each other.
    {
        auto check=[&](const NoonishSplit& s,int y,int x0,int x1) {
            int next=x0;
            noonishRow(s,y,x0,x1,[&](int x,int length,uint16_t colour) {
                CHECK(x==next && length>0);
                for (int i=x;i<x+length;++i) CHECK(noonishColour(s,i,y)==colour);
                if (x>x0) CHECK(noonishColour(s,x-1,y)!=colour);    // runs are maximal
                next=x+length;
            });
            CHECK(next==x1);
        };
        for (int step=0;step<4320;step+=37) {
            const auto s=noonishSplit(l,timeAt(step/360,(step%360)/6,(step%6)*10));
            for (int y=0;y<466;y+=5) { check(s,y,0,466); check(s,y,(y*7)%200,233+(y*3)%233); }
        }
        for (const auto& t:{timeAt(3,0),timeAt(9,0),timeAt(3,15),timeAt(12,0),timeAt(6,0),timeAt(9,45)}) {
            const auto s=noonishSplit(l,t);
            for (int y=225;y<241;++y) check(s,y,0,466);
        }
    }
    // The dot: lighter than the region under it by the same rule everywhere;
    // across a boundary each side from its own region. The reference's dot
    // on the top region reads (207,237,244).
    for (uint16_t c:NoonishPalette) {
        const Rgb light=rgbOf(noonishLight(c)),base=rgbOf(c);
        CHECK(light.r>base.r && light.g>=base.g && light.b>=base.b);
    }
    {
        const Rgb light=rgbOf(noonishLight(NoonishPalette[NoonishTop]));
        CHECK(std::abs(light.r-207)<=8 && std::abs(light.g-237)<=8 && std::abs(light.b-244)<=8);
    }
    const auto noon=noonishSplit(l,timeAt(12,0));
    const auto dot=analogSecond(l,timeAt(12,0,0));        // on the line, at the top
    uint16_t left=0,right=0,edge=0,again=0;
    CHECK(noonishDot(noon,dot,228,33,left) && noonishDot(noon,dot,237,33,right));
    CHECK(left==noonishLight(NoonishPalette[NoonishLeft]) && right==noonishLight(NoonishPalette[NoonishRight]));
    // Its rim blends from the light to the region under that very pixel;
    // drawn again from the same input it is the same, never whiter.
    CHECK(noonishDot(noon,dot,224,25,edge) && noonishDot(noon,dot,224,25,again) && edge==again);
    CHECK(edge!=noonishLight(NoonishPalette[NoonishLeft]) && edge!=NoonishPalette[NoonishLeft]);
    uint16_t none;
    CHECK(!noonishDot(noon,dot,232,20,none) && !noonishDot(noon,dot,245,33,none));
}
namespace {
// The clock layer as settings sees it: the first `count` faces, the one
// shown, and what a choice returns.
struct FakeFaces : HomeControlPort {
    const char* ids[4]={"digital","forest","analog","noonish"};
    const char* names[4]={"Digital","Forest","Analog","Noonish"};
    int count=2,current=0,chosen=0;
    FaceChoiceResult next=FaceChoiceResult::Selected;
    std::string last;
    HomeOutcome outcome{};
    HomeOutcome handle(const HomeEvent&) override { return outcome; }
    int faceCount() const override { return count; }
    WatchFaceChoice faceAt(int i) const override { return {ids[i],names[i]}; }
    int currentFace() const override { return current; }
    FaceChoiceResult chooseFace(const char* id) override {
        ++chosen; last=id;
        if (next==FaceChoiceResult::Selected || next==FaceChoiceResult::SaveFailed)
            for (int i=0;i<count;++i) if (std::strcmp(id,ids[i])==0) current=i;
        return next;
    }
};
void openSettings(ScreenManager& s,TimeUs& now) {
    Events home{}; home.home=true; s.handle(home,now); now+=1000;
    Events e{}; e.next=true;
    s.handle(e,now); now+=200000; s.update(now);
    while (LaunchRegistry[s.model().launcher.list.selection].id!=LaunchTargetId::Settings) { s.handle(e,now); now+=200000; s.update(now); }
    Events decide{}; decide.decide=true;
    s.handle(decide,now); now+=1000;
    CHECK(s.model().screen==ScreenId::Settings);
}
struct StubHal : Hal {
    TimeUs now() override { return 0; }
    InputSnapshot sampleInput() override { return {}; }
    UsbState sampleUsb() override { return {}; }
    void setScreenOff(bool) override {}
    void waitUs(TimeUs) override {}
    bool inputPending() override { return false; }
    bool readRtc(CivilTime& utc) override { utc={2026,9,20,15,0,0}; return true; }
    bool writeRtc(const CivilTime&) override { return true; }
    void setUtcClock(int64_t) override {}
    int64_t utcClockUs() override { return 0; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
};
}
void settingsFaces() {
    MemoryNvs nvs; SettingsStore store; store.begin(nvs);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    FakeFaces faces; screens.bindHome(&faces);
    TimeUs now=0;
    openSettings(screens,now);
    // Row 3 opens the choice; its label names the face shown.
    for (int i=0;i<3;++i) screens.handle(press(true),now);
    auto m=screens.model().settings;
    CHECK(m.menu.selection==3 && m.faceCount==2 && m.currentFace==0 && std::strcmp(m.faceNames[1],"Forest")==0);
    screens.handle(press(false),now);
    m=screens.model().settings;
    CHECK(m.view==SettingsView::WatchFace && m.faces.selection==0 && faces.chosen==0);
    std::array<ListRow,SettingsFaceRows> rows{}; SettingsFaceLabels labels;
    auto built=buildSettingsFaceRows(rows,m.faceCount,&m,&labels);
    CHECK(built.count==3 && rows[0].label==inUse("Digital") && std::strcmp(rows[1].label,"Forest")==0);
    CHECK(std::strcmp(rows[2].label,text::Back)==0 && rows[2].id==SettingsFaceBack);
    // Moving the cursor chooses nothing.
    screens.handle(press(true),now); now+=200000; screens.update(now);
    CHECK(screens.model().settings.faces.selection==1 && faces.chosen==0);
    // B chooses and stays; the mark moves.
    screens.handle(press(false),now);
    m=screens.model().settings;
    CHECK(faces.chosen==1 && faces.last=="forest" && m.view==SettingsView::WatchFace && m.currentFace==1 && !screens.model().toast);
    built=buildSettingsFaceRows(rows,m.faceCount,&m,&labels);
    CHECK(rows[1].label==inUse("Forest") && std::strcmp(rows[0].label,"Digital")==0);
    // Failures are said; the view stays.
    faces.next=FaceChoiceResult::SaveFailed; screens.handle(press(false),now);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,text::SaveFailed)==0);
    faces.next=FaceChoiceResult::Failed; screens.handle(press(false),now);
    CHECK(std::strcmp(screens.model().toast,text::FaceUnavailable)==0 && screens.model().settings.view==SettingsView::WatchFace);
    faces.next=FaceChoiceResult::Selected;
    // Back returns to the menu on the same row; nothing more is chosen.
    const int chosen=faces.chosen;
    screens.handle(press(true),now); now+=200000; screens.update(now);
    screens.handle(press(false),now);
    m=screens.model().settings;
    CHECK(m.view==SettingsView::Menu && m.menu.selection==3 && faces.chosen==chosen);
    // A tap on a face row chooses it too, and home leaves from the choice.
    screens.handle(press(false),now);
    const auto row=layoutListRow(settingsMenuPlacement({468,468},screens.model().settings.faces.scroll),0);
    Events tap{}; tap.gesture=Gesture::Tap; tap.x=row.labelX+10; tap.y=row.centerY;
    screens.handle(tap,now);
    CHECK(faces.last=="digital" && faces.chosen==chosen+1 && faces.current==0);
    Events home{}; home.home=true; screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home);
    // A new visit starts at the top of the menu and of the choice.
    openSettings(screens,now);
    for (int i=0;i<3;++i) screens.handle(press(true),now);
    screens.handle(press(false),now);
    CHECK(screens.model().settings.faces.selection==0);
    // Four faces: Analog third, Noonish fourth, then back; choosing moves the
    // mark.
    {
        FakeFaces three; three.count=4;
        TestScreens s3; s3.bind(&store,&time); s3.bindHome(&three);
        TimeUs t3=0; openSettings(s3,t3);
        for (int i=0;i<3;++i) s3.handle(press(true),t3);
        s3.handle(press(false),t3);
        auto m3=s3.model().settings;
        const auto rows3=buildSettingsFaceRows(rows,m3.faceCount,&m3,&labels);
        CHECK(m3.faceCount==4 && rows3.count==5 && std::strcmp(rows[2].label,"Analog")==0);
        CHECK(std::strcmp(rows[3].label,"Noonish")==0);
        CHECK(std::strcmp(rows[4].label,text::Back)==0 && rows[4].id==SettingsFaceBack);
        for (int i=0;i<2;++i) { s3.handle(press(true),t3); t3+=200000; s3.update(t3); }
        s3.handle(press(false),t3);
        m3=s3.model().settings;
        CHECK(three.last=="analog" && three.current==2 && m3.currentFace==2);
        buildSettingsFaceRows(rows,m3.faceCount,&m3,&labels);
        CHECK(rows[2].label==inUse("Analog") && std::strcmp(rows[0].label,"Digital")==0);
        // Noonish, below it, then back at the bottom.
        s3.handle(press(true),t3); t3+=200000; s3.update(t3);
        s3.handle(press(false),t3);
        m3=s3.model().settings;
        CHECK(three.last=="noonish" && three.current==3 && m3.currentFace==3 && m3.faces.selection==3);
        buildSettingsFaceRows(rows,m3.faceCount,&m3,&labels);
        CHECK(rows[3].label==inUse("Noonish") && std::strcmp(rows[2].label,"Analog")==0);
        s3.handle(press(true),t3); t3+=200000; s3.update(t3);
        CHECK(s3.model().settings.faces.selection==4);
        s3.handle(press(false),t3);
        CHECK(s3.model().settings.view==SettingsView::Menu && three.chosen==2);
    }
    // Without a clock layer the choice offers back only.
    TestScreens bare; bare.bind(&store,&time); bare.bindHome(nullptr);
    TimeUs t=0; openSettings(bare,t);
    for (int i=0;i<3;++i) bare.handle(press(true),t);
    bare.handle(press(false),t);
    CHECK(bare.model().settings.view==SettingsView::WatchFace && bare.model().settings.faceCount==0);
    bare.handle(press(false),t);
    CHECK(bare.model().settings.view==SettingsView::Menu);
    // A face that kept its change but could not store it: the clock says so.
    FakeFaces clock; clock.outcome={true,HomeRequest::None,true};
    TestScreens home2; home2.bindHome(&clock);
    Events hold{}; hold.gesture=Gesture::LongPress; hold.x=234; hold.y=234;
    CHECK(home2.handle(hold,0) && home2.model().toast && std::strcmp(home2.model().toast,text::SaveFailed)==0);
}
int main() {
    records(); separation(); variants(); selection(); forestBattery(); forestLayoutRules(); forestPalettes();
    analogTimeRules(); analogLayoutRules(); analogControlRules(); noonishBackgroundRules(); settingsFaces();
    std::cout<<"PASS: watch face records, record separation, variants, selection, forest battery, forest palettes, "
               "forest layout, analog time, analog layout, analog control, noonish background, settings face choice (four faces)\n";
}
