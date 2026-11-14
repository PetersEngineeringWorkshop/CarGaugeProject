# Touch Screen CAN v1.1.0

November 14, 2026

- FreeSans readings, 9 pt gauge headings, and temperature units closer to
  their values. The status line displays warnings and data problems.
- Both gauge bars are 18 pixels tall. The RPM bar goes from blue at 0 RPM to
  green at 900 RPM, then through yellow to red at 6,500 RPM. A valid reading at
  or above the redline shows **LIMIT**. The idle and redline are build-time
  settings in [instrument_config.h](../include/instrument_config.h).
- A Honda H splash fades out at startup. Set `showHondaSplash` to `false` in
  the same configuration file to skip it.

The normal and listen-only firmware builds passed, along with host checks of
the RPM color mapping. The RPM gradient still needs a check on the TFT.

See the [BOM](BOM.md), [wiring diagram](../README.md#wiring-diagram), and
[build and upload instructions](../README.md#build-and-use).
