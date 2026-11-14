#pragma once
#include "engine_data.h"

// Synthetic channels never enter the CAN decoder or its live data store.
// Demo selection is RAM-only and starts disabled on every boot.
class DemoMode {
public:
    engine::Data live;
    bool enabled() const {return active;}
    bool announcing() const {return announcement;}
    const engine::Data &readings() const {return active?synthetic:live;}
    void toggle(uint32_t now) {
        active=!active;announcement=true;visible=false;started=now;
        if(active) {sample(now);lastSample=now;}
        else live=engine::Data{}; // Require new actual frames after leaving demo.
    }
    void service(uint32_t now) {
        if(active && uint32_t(now-lastSample)>=50) {sample(now);lastSample=now;}
    }
    // Start the one-second notice timer only once both tile queues have drained.
    bool finishAnnouncement(uint32_t now,bool displaysReady) {
        if(!announcement) return false;
        if(!visible && displaysReady) {visible=true;visibleAt=now;}
        if(visible && uint32_t(now-visibleAt)>=1000) {announcement=false;return true;}
        return false;
    }
private:
    engine::Data synthetic;
    bool active=false,announcement=false,visible=false;
    uint32_t started=0,lastSample=0,visibleAt=0;
    void sample(uint32_t now) {
        using S=engine::Signal;
        constexpr float tau=6.28318530718f;
        float phase=float(uint32_t(now-started)%60000)*tau/60000.0f;
        float load=std::sin(phase*3),temperature=85+30*std::sin(phase);
        synthetic.get(S::Rpm).set(2400+1500*load,now);
        synthetic.get(S::CoolantC).set(temperature,now);
        synthetic.get(S::OilKpa).set(300+150*load,now);
        synthetic.get(S::FuelKpa).set(350+20*std::sin(phase*4),now);
        synthetic.get(S::MapKpa).set(100+70*load,now);
        synthetic.get(S::BaroKpa).set(100.9f+0.2f*std::sin(phase),now);
        synthetic.get(S::Lambda).set(0.98f+0.12f*std::sin(phase*5),now);
        synthetic.get(S::BatteryV).set(13.7f+0.4f*std::sin(phase*7),now);
        synthetic.get(S::IatC).set(28+12*std::sin(phase*2),now);
        synthetic.get(S::Fan1).set(temperature>=95?1:0,now);
        synthetic.get(S::Fan2).set(temperature>=105?1:0,now);
    }
};
