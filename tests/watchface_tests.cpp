// Work 10-5: the watch face records, the face selection, Forest's rules and
// layout, and choosing a face in settings (docs/task10/plan-10-5.md 7).
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/home/FaceSelection.h"
#include "features/home/faces/DigitalLayout.h"
#include "features/home/faces/ForestLayout.h"
#include "features/settings/SettingsMenu.h"
#include "storage/SettingsStore.h"
#include "storage/WatchPreferences.h"
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
    nvs.records["watch_sel"]={1,6,'a','n','a','l','o','g'};
    { FaceSelection s; registry(s); CHECK(s.startup()==0 && nvs.writes==0 && nvs.records["watch_sel"].size()==8); }
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
    CHECK(s.choose("analog",begin)==FaceChoiceResult::Unknown && begins==1);
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
}
// Below 30% or charging, no hysteresis, failures never read as low.
void forestBattery() {
    for (bool was:{false,true}) {
        CHECK(forestBatteryShown(29,false,true,was) && !forestBatteryShown(30,false,true,was) &&
              !forestBatteryShown(31,false,true,was));
        CHECK(forestBatteryShown(30,true,true,was) && forestBatteryShown(100,true,true,was) && forestBatteryShown(0,false,true,was));
        CHECK(forestBatteryShown(-1,true,true,was));              // charging, level unknown
        CHECK(forestBatteryShown(-1,false,true,was)==was);        // unknown level: kept
        CHECK(forestBatteryShown(50,false,false,was)==was);       // charging unreadable: kept
        CHECK(forestBatteryShown(12,false,false,was));            // the level still decides
        CHECK(forestBatteryShown(101,false,true,was)==was);       // out of range is unknown
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
                const int limit=forestGroupWidthLimit(l,3);
                const int widths[3]={limit,limit,limit};
                Rect groups[3];
                CHECK(placeForestInfo(l,widths,3,groups)==3);
                for (const auto& g:groups) {
                    const Rect padded{g.x-2,g.y-2,g.w+4,g.h+4};
                    CHECK(inside(padded) && padded.y>l.groundTop);
                    for (const auto& t:l.trees) if (padded.x<t.rx+1 && t.lx-1<padded.x+padded.w) CHECK(padded.y>t.by+1);
                }
                CHECK(groups[0].x+3*limit+2*l.groupGap==groups[2].x+groups[2].w);
                // Shares: a narrow battery leaves its room to the items, so the
                // reference row (18% and two 02:40) is not shortened; labels
                // too long for the row share it equally, and the row fits.
                {
                    const int wanted[3]={70,97,97};
                    int limits[3]{};
                    forestGroupLimits(l,wanted,3,limits);
                    CHECK(limits[0]>=70 && limits[1]>=97 && limits[2]>=97);
                    const int wide[3]={70,400,300};
                    forestGroupLimits(l,wide,3,limits);
                    const int room=l.infoWidth-2*l.groupGap;
                    CHECK(limits[0]>=70 && limits[1]==limits[2] && 70+limits[1]+limits[2]<=room);
                    CHECK(limits[1]>=forestGroupWidthLimit(l,3));
                    const int one[1]={900};
                    forestGroupLimits(l,one,1,limits);
                    CHECK(limits[0]==l.infoWidth);
                }
                // Reference spacing at 466: centred on 367.
                if (side==466) CHECK(l.infoY==367 && l.iconSize==28 && l.groupGap==28);
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
// The clock layer as settings sees it: two faces, the one shown, and what a
// choice returns.
struct FakeFaces : HomeControlPort {
    const char* ids[2]={"digital","forest"};
    const char* names[2]={"Digital","Forest"};
    int current=0,chosen=0;
    FaceChoiceResult next=FaceChoiceResult::Selected;
    std::string last;
    HomeOutcome outcome{};
    HomeOutcome handle(const HomeEvent&) override { return outcome; }
    int faceCount() const override { return 2; }
    WatchFaceChoice faceAt(int i) const override { return {ids[i],names[i]}; }
    int currentFace() const override { return current; }
    FaceChoiceResult chooseFace(const char* id) override {
        ++chosen; last=id;
        if (next==FaceChoiceResult::Selected || next==FaceChoiceResult::SaveFailed) current=std::strcmp(id,"forest")==0;
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
    records(); separation(); variants(); selection(); forestBattery(); forestLayoutRules(); settingsFaces();
    std::cout<<"PASS: watch face records, record separation, variants, selection, forest battery, "
               "forest layout, settings face choice\n";
}
