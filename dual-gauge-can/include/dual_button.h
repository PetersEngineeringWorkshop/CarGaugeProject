#pragma once
#include "button.h"

// Both-button gestures consume individual actions until both buttons are released.
// Qualification starts when both presses have passed debounce. No blocking waits.
class DualButton {
public:
    struct Events {
        DebouncedButton::Event first=DebouncedButton::Event::None;
        DebouncedButton::Event second=DebouncedButton::Event::None;
        bool toggle=false;
    };
    void begin(bool firstHigh,bool secondHigh,uint32_t now) {
        first.begin(firstHigh,now);second.begin(secondHigh,now);
        blocked=!(firstHigh && secondHigh);chord=false;cancelled=false;sent=false;
    }
    Events update(bool firstHigh,bool secondHigh,uint32_t now) {
        Events out{first.update(firstHigh,now),second.update(secondHigh,now),false};
        if(blocked) {
            if(first.released() && second.released() && firstHigh && secondHigh) blocked=false;
            return {};
        }
        if(!chord && !first.released() && !second.released()) {
            chord=true;cancelled=false;sent=false;started=now;
        }
        if(chord) {
            if(first.released() || second.released()) cancelled=true;
            bool fire=!sent && !cancelled && !firstHigh && !secondHigh && uint32_t(now-started)>=2000;
            if(fire) sent=true;
            if(first.released() && second.released() && firstHigh && secondHigh) chord=false;
            return {DebouncedButton::Event::None,DebouncedButton::Event::None,fire};
        }
        // A second press may still be debouncing when the first reaches its hold
        // threshold. Prefer the prospective chord rather than re-centering.
        if(!firstHigh && !secondHigh) return {};
        return out;
    }
private:
    DebouncedButton first,second;
    bool blocked=false,chord=false,cancelled=false,sent=false;
    uint32_t started=0;
};
