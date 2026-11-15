# Firmware releases

These source releases consolidate common PlatformIO code. Each firmware uses
its own `VERSION` file for the serial startup banner. See the project guide for
build and USB upload instructions.

| Firmware | Version | Git tag |
| --- | --- | --- |
| [Attitude Indicator](attitude-indicator/README.md) | 1.0.1 | `attitude-indicator-v1.0.1` |
| [Standalone Compass](compass/README.md) | 1.0.1 | `compass-v1.0.1` |
| [Compass & Horizon](compass-horizon/README.md) | 1.1.1 | `compass-horizon-v1.1.1` |
| [Dual Gauge with Buttons](dual-gauge-with-buttons/README.md) | 1.0.1 | `dual-gauge-with-buttons-v1.0.1` |
| [Dual Gauge CAN](dual-gauge-can/README.md) | 1.1.1 | `dual-gauge-can-v1.1.1` |
| [Touch Screen CAN](touch-screen-can/README.md) | 1.1.1 | `touch-screen-can-v1.1.1` |

## Attitude Indicator

Uses the shared horizon geometry and RGB565 transfer code. The display still
sends eight-row stripes and services the IMU between transfers.

## Standalone Compass

Uses the shared QMC5883P driver, calibration storage, and compass math. The
existing NVS calibration format is retained.

## Compass and Horizon

Uses the shared button, sensor, calibration, and geometry libraries. Mode
selection and the horizon and compass controls keep their existing behavior.

## Dual Gauge with Buttons

Uses shared sensor, calibration, and attitude-rendering code. Both displays
continue to update independently with their existing button controls.

## Dual Gauge CAN

Uses the shared CAN receiver, decoder, and engine readings. This is the first
stable release of the two-button demo/live toggle developed in `1.1.0-dev`.
Existing manifest settings and saved gauge selections carry forward.

## Touch Screen CAN

Uses the same CAN library as Dual Gauge CAN without requiring that firmware's
source folder. The touchscreen continues to update individual reading fields
and uses its own touch controls, layout, and persisted settings.

## Verification

The PlatformIO builds and native tests were checked locally. Eight rendered
round-gauge preview images matched the pre-refactor output byte for byte. The
patch releases have yet to be flashed and verified on physical devices.
