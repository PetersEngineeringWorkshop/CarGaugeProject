#include "demo_mode.h"
#include "dual_button.h"
#include <cassert>
#include <cstdio>
#include <limits>

using E=DebouncedButton::Event;
void quiet(DualButton::Events e,bool toggle=false) {
    assert(e.first==E::None && e.second==E::None && e.toggle==toggle);
}
void chord(uint32_t base) {
    DualButton b;b.begin(true,true,base);
    quiet(b.update(false,false,base+100));
    quiet(b.update(false,false,base+140));
    quiet(b.update(false,false,base+2139));
    quiet(b.update(false,false,base+2140),true);
    quiet(b.update(false,false,base+10000)); // Held for longer: one event only.
    quiet(b.update(true,false,base+10001));
    quiet(b.update(true,false,base+10041));
    quiet(b.update(true,false,base+13000));
    quiet(b.update(true,true,base+13001));
    quiet(b.update(true,true,base+13041)); // Neither release switches faces.
    quiet(b.update(false,false,base+14000));
    quiet(b.update(false,false,base+14040));
    quiet(b.update(false,false,base+16040),true); // Rearmed after both releases.
}
int main() {
    chord(0);chord(UINT32_MAX-1000);
    DualButton b;b.begin(true,true,0);
    b.update(false,true,100);assert(b.update(false,true,140).first==E::Pressed);
    b.update(false,false,500);quiet(b.update(false,false,540));
    quiet(b.update(false,false,2100)); // First button's long event is suppressed.
    quiet(b.update(false,false,2539));
    quiet(b.update(false,false,2540),true); // Hold starts at overlapping press.
    b.begin(true,true,0);
    b.update(false,false,100);b.update(false,false,140);
    quiet(b.update(true,false,2139)); // Pending release before deadline must not fire.
    quiet(b.update(true,false,2140));quiet(b.update(true,false,2179));
    quiet(b.update(false,false,2200));quiet(b.update(false,false,2240));
    quiet(b.update(false,false,6000)); // Aborted chord stays consumed until both release.
    b.update(true,true,6100);quiet(b.update(true,true,6140));
    b.update(false,true,6200);assert(b.update(false,true,6240).first==E::Pressed);
    b.update(true,true,6300);assert(b.update(true,true,6340).first==E::ShortPress);
    b.begin(false,false,0);quiet(b.update(false,false,5000));
    b.update(true,true,5100);quiet(b.update(true,true,5140));
    b.update(true,false,5200);assert(b.update(true,false,5240).second==E::Pressed);
    assert(b.update(true,false,7200).second==E::LongPress); // Single holds unchanged.
    b.begin(true,true,0);
    b.update(false,false,100);b.update(true,true,110);quiet(b.update(true,true,200));
    b.update(false,true,300);b.update(false,true,340);
    b.update(false,false,2290);quiet(b.update(false,false,2300)); // Near-threshold stagger.
    quiet(b.update(false,false,2330));quiet(b.update(false,false,4330),true);

    DemoMode mode;using S=engine::Signal;
    assert(!mode.enabled() && !mode.announcing());
    mode.live.get(S::Rpm).set(123,0);
    mode.toggle(100);assert(mode.enabled() && mode.announcing());
    assert(mode.readings().get(S::Rpm).value!=123);
    assert(!mode.finishAnnouncement(2000,false)); // Transfer time isn't visible time.
    assert(!mode.finishAnnouncement(2100,true));
    assert(!mode.finishAnnouncement(3099,true));assert(mode.finishAnnouncement(3100,true));
    assert(!mode.finishAnnouncement(4100,true));
    float low[unsigned(S::Count)],high[unsigned(S::Count)];
    for(unsigned i=0;i<unsigned(S::Count);++i) {low[i]=1e9f;high[i]=-1e9f;}
    for(uint32_t t=100;t<=60100;t+=50) {
        mode.service(t);
        for(unsigned i=0;i<unsigned(S::Count);++i) {
            const auto &c=mode.readings().get(S(i));
            assert(c.state(t)==engine::State::Valid);
            if(c.value<low[i]) low[i]=c.value;if(c.value>high[i]) high[i]=c.value;
        }
    }
    for(unsigned i=0;i<unsigned(S::Count);++i) assert(high[i]>low[i]);
    assert(low[unsigned(S::BatteryV)]>13 && high[unsigned(S::BatteryV)]<15);
    assert(low[unsigned(S::IatC)]>=15 && high[unsigned(S::IatC)]<=41);
    mode.live.get(S::Rpm).set(999,60100); // Real traffic cannot overwrite synthetic data.
    assert(mode.readings().get(S::Rpm).value!=999);
    mode.toggle(60200);assert(!mode.enabled());
    for(const auto &c:mode.readings().channels) assert(c.state(60200)==engine::State::Never);
    mode.live.get(S::Rpm).set(456,60300);assert(mode.readings().get(S::Rpm).value==456);
    mode.service(61000);assert(mode.readings().get(S::Rpm).value==456);
    assert(mode.readings().get(S::Rpm).state(62000)==engine::State::Stale);
    DemoMode rollover;rollover.toggle(UINT32_MAX-500);rollover.service(100);
    assert(rollover.readings().get(S::BatteryV).state(100)==engine::State::Valid);
    assert(!rollover.finishAnnouncement(UINT32_MAX-100,true));
    assert(rollover.finishAnnouncement(899,true));
    DemoMode reboot;assert(!reboot.enabled());
    puts("PASS: debounced two-button hold, stagger/abort/release suppression, boot-held and rollover; dynamic isolated demo data, full notice interval, clean live return and reboot default");
}
