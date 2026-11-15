#pragma once
#include <Arduino.h>
#include <driver/twai.h>
#include "ecu_decoder.h"
class CanReceiver {
public:
    struct Config {
        int txPin, rxPin;
        uint32_t bitrate;
        bool acknowledgments;
    };
    using Logger = bool (*)(const char *, ...);
    CanReceiver(Config config, CanFrameFilter stream, Logger logger)
        : config(config), stream(stream), logLine(logger) {}
    void begin();
    void service(EcuDecoder &,engine::Data &);
    void report();
    void listIds() { listing=0; dumpMux=0; }
    bool ready() const { return running; }
    uint32_t frames=0, errors=0, logSkipped=0, idOverflowFrames=0, maxServiceUs=0;
    uint32_t lastFrameMs=0, pendingAlerts=0;
private:
    const Config config;
    const CanFrameFilter stream;
    const Logger logLine;
    const char *modeName() const { return config.acknowledgments ? "NORMAL_RX" : "LISTEN_ONLY"; }
    twai_handle_t handle=nullptr;
    bool running=false;
    uint32_t ids[128]{}, idCount=0, listing=128;
    uint32_t lastAttempt=0, lastPrint=0, lastIdPrint=0;
    uint32_t previousFrames=0, previousReport=0;
    esp_err_t lastError=ESP_OK;
    struct MuxSample { uint8_t bytes[8]{}; uint32_t count=0, at=0; };
    MuxSample mux[16]{};
    unsigned dumpMux=16;
    uint32_t lastMuxPrint=0;
    void observe(const CanFrame &);
};
