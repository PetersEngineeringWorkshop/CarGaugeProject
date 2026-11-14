# Touch Screen CAN bill of materials

Quantities are for one instrument.

| Item | Qty | Part | Notes |
| --- | --- | --- | --- |
| 1 | 1 | ESP32-S3 SuperMini, ESP32-S3FH4R2 | Blue development board with 4 MB flash, 2 MB PSRAM, and USB-C. |
| 2 | 1 | 2.8-inch TFT SPI 240×320 V1.2 | ILI9341 display and XPT2046-compatible resistive touch. The 14-pin header carries both interfaces. |
| 3 | 1 | 3.3 V Classic CAN transceiver module | TXD/RXD logic pins and CANH/CANL bus pins. An SN65HVD230-style module fits this interface. The fitted board's exact part number and termination are unrecorded. |
| 4 | 1 set | Header wiring and a twisted CAN pair | Split the display and touch clock and data-input connections as shown in the wiring diagram. Include the ECU ground reference. |
| 5 | 1 | USB-C data cable | Firmware upload, diagnostics, and bench power. |
| 6 | 1 | Regulated automotive 5 V supply | For an installation powered from the vehicle. Size for the assembled load. |

See the [wiring diagram](../README.md#wiring-diagram) and the
[vehicle connection notes](../../dual-gauge-can/docs/vehicle-connection.md)
for power and bus termination. With the TFT jumper J1 open, use 5 V at TFT
VCC and 3.3 V at the backlight enable and logic pins.
