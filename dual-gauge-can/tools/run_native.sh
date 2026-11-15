#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dgc_test_dir=$(mktemp -d "${TMPDIR:-/tmp}/dgc-native.XXXXXX")
trap 'rm -rf "$dgc_test_dir"' EXIT HUP INT TERM
python3 test/config_test.py
python3 tools/configure.py --config test/fixtures/instrument.json --output "$dgc_test_dir/config/instrument_config.h"
python3 tools/configure.py --config config/examples/listen-only.json --output "$dgc_test_dir/passive/instrument_config.h"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/gauge_style_test.cpp -o "$dgc_test_dir/style"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/link_decoder_test.cpp -o "$dgc_test_dir/decoder"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/engine_data_test.cpp -o "$dgc_test_dir/data"
c++ -std=c++17 -O2 -I test/fakes -I "$dgc_test_dir/config" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/settings_test.cpp src/display_settings.cpp -o "$dgc_test_dir/settings"
c++ -std=c++17 -O2 -I test/fakes -I "$dgc_test_dir/config" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/can_receiver_test.cpp ../shared/GaugeCan/src/CanReceiver.cpp -o "$dgc_test_dir/can"
c++ -std=c++17 -O2 -I test/fakes -I "$dgc_test_dir/passive" -I include -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/can_receiver_test.cpp ../shared/GaugeCan/src/CanReceiver.cpp -o "$dgc_test_dir/can_passive"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeButton/src test/button_test.cpp ../shared/GaugeButton/src/Button.cpp -o "$dgc_test_dir/button"
c++ -std=c++17 -O2 -I include -I ../shared/GaugeButton/src -I ../shared/GaugeCan/src -I ../shared/IndexedFramebuffer/src test/demo_mode_test.cpp ../shared/GaugeButton/src/Button.cpp -o "$dgc_test_dir/demo"
framebuffer_src=../shared/IndexedFramebuffer/src
c++ -std=c++17 -O2 -I test/fakes -I "$dgc_test_dir/config" -I include -I "$framebuffer_src" \
    test/display_test.cpp src/gauges/Display.cpp "$framebuffer_src/IndexedFramebuffer.cpp" -o "$dgc_test_dir/display"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeMath/src test/compass_math_test.cpp -o "$dgc_test_dir/compass"
c++ -std=c++17 -O2 -I "$dgc_test_dir/config" -I include -I ../shared/GaugeMath/src test/horizon_geometry_test.cpp -o "$dgc_test_dir/horizon"
c++ -std=c++17 -O2 -I test/fakes -I "$dgc_test_dir/config" -I include -I ../shared/GaugeMath/src -I ../shared/GaugeSensors/src -I ../shared/DualGaugeComponents/src test/calibration_workflow_test.cpp ../shared/DualGaugeComponents/src/Magnetometer.cpp ../shared/GaugeSensors/src/calibration_store.cpp -o "$dgc_test_dir/calibration"
for dgc_test in style decoder data settings can can_passive button demo display compass horizon calibration; do
    "$dgc_test_dir/$dgc_test"
done
