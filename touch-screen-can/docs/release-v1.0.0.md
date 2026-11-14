# Touch Screen CAN v1.0.0

November 14, 2026

- Two side-by-side gauges on one 2.8-inch SPI touchscreen. Tap anywhere to open
  the menu and choose a gauge independently for each side. Both selections and
  the touch calibration survive a reboot.
- Coolant, RPM, boost, ECU supply voltage, and intake air temperature use the
  shared Link Generic Dash decoder. Boost uses MAP minus barometric pressure.
  Gauges show distinct waiting, stale, and invalid-data states.
- Buffered digits and partial bar updates reduce flashing on changing readings.
- Demo mode makes all five readings available on the bench. The display marks
  simulated readings and returns to live mode at every restart.
- Normal and listen-only builds use Classic CAN at 500 kbit/s and standard ID
  1000. Normal mode acknowledges the ECU. The application sends no CAN messages.

The owner confirmed live readings from all five gauges on the CR-V with a Link
MonsoonX during development and approved the final touchscreen layout. Both
release builds and the shared Dual Gauge CAN host tests passed. The normal
build reported v1.0.0 over USB and restored the saved calibration and gauge
assignments. Both panels showed demo values and returned to waiting for ECU
data when switched back to live. A separate in-car soak test of v1.0.0 is pending.

See the [BOM](BOM.md), [wiring diagram](../README.md#wiring-diagram), and
[build and upload instructions](../README.md#build-and-use).
