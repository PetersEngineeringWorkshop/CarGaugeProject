#pragma once

enum class CompassStatus { Waiting, Live, Uncalibrated, Calibrating, Fault, WeakField, CalibrationFailed, CalibrationSaved, SaveFailed };
struct CompassState {
    float heading=0;
    CompassStatus status=CompassStatus::Waiting;
    int seconds=0;
    bool saved=false;
    bool light=false;
    bool sensorFresh=true;
};
