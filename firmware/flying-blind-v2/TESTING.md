# Flying Blind V2 — Bench Test Notes

## Hardware
- Teensy 4.0
- GY-521 / MPU6050 at I2C address 0x68
- GY-63 barometer module at I2C address 0x77
- SDA: pin 18
- SCL: pin 19

## Verified behavior
- LED blink and USB serial output.
- Both I2C addresses respond.
- Barometer initially reported approximately 502 mbar.
- Selecting the library's alternate calculation with reset(1)
  produced approximately 1005 mbar, plausible for this Florida test.
- Absolute pressure accuracy has not been independently verified.
- Accelerometer readings change with orientation.
- Gyro readings change during rotation and settle after movement stops.
- Stationary gyro readings have an offset; calibration is pending.
- Sample numbers increment and acquisition-start timestamps advance
  approximately 1005 ms per group.

## Startup observation
One upload was followed by an MPU6050 initialization failure.
A full power restart restored operation.
Three subsequent uploads without disconnecting power all initialized
both sensors successfully. The original failure's cause is unknown.

## Intermittent SCL connection and repair
Both sensor initialization failures occurred during testing.
Restoring earlier firmware did not restore MPU6050 communication.
The original I2C scanner detected only the barometer at 0x77.

Measurements at the MPU6050 module:
- VCC to GND: 3.3 V
- SCL to GND: 3.24 V
- SDA to GND: 3.26 V
- Continuity across the SCL mounting joint was intermittent.

After resoldering the SCL joint:
- Continuity became steady.
- The scanner detected both 0x68 and 0x77.
- The committed combined-sensor firmware produced readings again.

The first acceleration sample after restoration was unusually low.
Subsequent samples settled near gravity. Its cause remains unconfirmed.

## microSD initialization
- Module supply measured 3.3 V.
- SPI connections: CS=10, MOSI=11, MISO=12, SCK=13.
- SD.begin(10) returned success.
- Both sensors continued producing readings.
- Observed sample starts remained 1000 ms apart.
- File writing and readback have not yet been tested.

## microSD file write and readback
The firmware appended a known test line to V2TEST.TXT and closed it.
After disconnecting power, the card was read on the computer.
The file contained:
Flying Blind V2: microSD write test

This verifies a basic file write and computer readback.
Continuous sensor logging has not yet been tested.

## Barometer CSV recording
Verified V2BARO.CSV by reading the card on the computer.
It contained two complete startup batches of 10 samples each.
Each batch numbered samples 0–9 with 1000 ms timestamp intervals.
The second batch matched the Serial Monitor output.

Original data preserved in data/barometer-two-startups.csv.
The current program appends batches to one file.
Separate session files and IMU recording are pending.

## Separate recording sessions
Each startup selects the first unused BAR000.CSV–BAR999.CSV filename.
Computer readback verified BAR001.CSV and BAR006.CSV:
each contained one header and 10 rows numbered 0–9.
Copies are preserved in data/.

MPU6050 communication failed during some intervening startups
(address response 2), then recovered (response 0).
The cause of this recurrence remains unresolved.
The latest no-continuity report was invalid because the meter
was on the wrong setting; it does not establish another failed joint.
Both sensors worked during the BAR006.CSV capture.

## Combined sensor CSV recording
Verified data/SEN001.CSV by reading the card on the computer:
- One header and 10 rows, numbered 0–9.
- Ten columns containing sample number, acquisition-start time,
  pressure, barometer temperature, acceleration XYZ, and gyro XYZ.
- Acquisition starts spaced 1000 ms apart.
- Saved values matched Serial Monitor output.
- Recording stopped after 10 rows; live readings continued.

Measurements are uncalibrated and acquired sequentially.
The first acceleration sample differed from subsequent stationary samples;
its cause remains unconfirmed.
Intermittent MPU6050 communication remains an unresolved reliability issue.