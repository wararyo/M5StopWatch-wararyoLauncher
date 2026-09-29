// Work 10-5: the watch face records, the face selection, Forest's rules and
// layout, and choosing a face in settings (docs/task10/plan-10-5.md 7).
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/home/FaceSelection.h"
#include "features/home/faces/AnalogLayout.h"
#include "features/home/faces/DigitalLayout.h"
#include "features/home/faces/ForestLayout.h"
#include "features/settings/SettingsMenu.h"
#include "storage/SettingsStore.h"
#include "storage/WatchPreferences.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
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
    // The built-in faces in HomeLayer's order: Analog third, stored and
    // restored like the others. Choosing writes the selection only.
    MemoryNvs three; WatchPreferences threeStore; threeStore.bind(&three);
    auto builtIn=[&](FaceSelection& f) {
        f.bind(&threeStore);
        CHECK(f.add({"digital","Digital","wf_digital"}) && f.add({"forest","Forest","wf_forest"}) &&
              f.add({"analog","Analog","wf_analog"}));
    };
    FaceSelection faces; builtIn(faces);
    CHECK(faces.count()==3 && faces.find("analog")==2 && std::strcmp(faces.at(2).name,"Analog")==0);
    CHECK(faces.startup()==0); faces.shown(0);
    CHECK(faces.choose("analog",begin)==FaceChoiceResult::Selected && faces.current()==2);
    { FaceSelection next; builtIn(next); CHECK(next.startup()==2); }
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
                    CHECK(inside(box) && box.y+box.h<l.groundTop);
                    // No tree reaches the time: its apex is below the box.
                    for (const auto& t:l.trees)
                        if (box.x<t.rx+1 && t.lx-1<box.x+box.w) CHECK(box.y+box.h<t.ay-1);
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
                    CHECK(inside(padded) && padded.y>l.groundTop);
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
                // Reference spacing at 466: centred on 367.
                if (side==466) CHECK(l.row.y==367 && l.row.iconSize==28 && l.row.groupGap==28);
            }
        // The trees keep their shape: 3.9px down for every pixel across.
        const auto l=forestLayout(v,TimeVariant::HourMinute,false);
        for (size_t i=0;i<ForestTrees.size();++i) {
            const auto& t=l.trees[i];
            CHECK(std::abs((t.by-t.ay)/((t.rx-t.lx)/2)-ForestTreeSlope)<0.01f);
        }
        if (side==466) CHECK(l.groundTop==372 && std::abs(l.trees[6].ay-272)<0.01f);
    }
}
namespace {
WatchData clockAt(int hour,int minute,int second,int day=20,TimeUs subsecond=0) {
    WatchData d; d.timeValid=true;
    d.localTime.tm_hour=hour; d.localTime.tm_min=minute; d.localTime.tm_sec=second; d.localTime.tm_mday=day;
    d.subsecondUs=subsecond;
    return d;
}
bool near(float a,float b,float tolerance=1e-3f) { return std::abs(a-b)<=tolerance; }
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
    CHECK(built.count==3 && std::strcmp(rows[0].label,"Digital  使用中")==0 && std::strcmp(rows[1].label,"Forest")==0);
    CHECK(std::strcmp(rows[2].label,"戻る")==0 && rows[2].id==SettingsFaceBack);
    // Moving the cursor chooses nothing.
    screens.handle(press(true),now); now+=200000; screens.update(now);
    CHECK(screens.model().settings.faces.selection==1 && faces.chosen==0);
    // B chooses and stays; the mark moves.
    screens.handle(press(false),now);
    m=screens.model().settings;
    CHECK(faces.chosen==1 && faces.last=="forest" && m.view==SettingsView::WatchFace && m.currentFace==1 && !screens.model().toast);
    built=buildSettingsFaceRows(rows,m.faceCount,&m,&labels);
    CHECK(std::strcmp(rows[1].label,"Forest  使用中")==0 && std::strcmp(rows[0].label,"Digital")==0);
    // Failures are said; the view stays.
    faces.next=FaceChoiceResult::SaveFailed; screens.handle(press(false),now);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,"保存に失敗しました")==0);
    faces.next=FaceChoiceResult::Failed; screens.handle(press(false),now);
    CHECK(std::strcmp(screens.model().toast,"文字盤を表示できません")==0 && screens.model().settings.view==SettingsView::WatchFace);
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
    // Three faces: Analog third, then back; choosing it moves the mark.
    {
        FakeFaces three; three.count=3;
        TestScreens s3; s3.bind(&store,&time); s3.bindHome(&three);
        TimeUs t3=0; openSettings(s3,t3);
        for (int i=0;i<3;++i) s3.handle(press(true),t3);
        s3.handle(press(false),t3);
        auto m3=s3.model().settings;
        const auto rows3=buildSettingsFaceRows(rows,m3.faceCount,&m3,&labels);
        CHECK(m3.faceCount==3 && rows3.count==4 && std::strcmp(rows[2].label,"Analog")==0);
        CHECK(std::strcmp(rows[3].label,"戻る")==0 && rows[3].id==SettingsFaceBack);
        for (int i=0;i<2;++i) { s3.handle(press(true),t3); t3+=200000; s3.update(t3); }
        s3.handle(press(false),t3);
        m3=s3.model().settings;
        CHECK(three.last=="analog" && three.current==2 && m3.currentFace==2);
        buildSettingsFaceRows(rows,m3.faceCount,&m3,&labels);
        CHECK(std::strcmp(rows[2].label,"Analog  使用中")==0 && std::strcmp(rows[0].label,"Digital")==0);
        // Back, below Analog, returns to the menu.
        s3.handle(press(true),t3); t3+=200000; s3.update(t3);
        s3.handle(press(false),t3);
        CHECK(s3.model().settings.view==SettingsView::Menu && three.chosen==1);
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
    CHECK(home2.handle(hold,0) && home2.model().toast && std::strcmp(home2.model().toast,"保存に失敗しました")==0);
}
int main() {
    records(); separation(); variants(); selection(); forestBattery(); forestLayoutRules();
    analogTimeRules(); analogLayoutRules(); analogControlRules(); settingsFaces();
    std::cout<<"PASS: watch face records, record separation, variants, selection, forest battery, "
               "forest layout, analog time, analog layout, analog control, settings face choice (three faces)\n";
}
