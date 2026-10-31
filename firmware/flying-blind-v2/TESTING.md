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

## 60-sample recording and repeatable analysis
Recorded SEN002.CSV with a capture limit of 60 samples.
Analysis verified:
- 60 rows and 10 numeric columns.
- Consecutive samples 0–59.
- All 59 timestamp intervals were 1000 ms.
- First-to-last sample span was 59 seconds.
- Mean pressure: 1005.116 mbar.
- Pressure sample standard deviation: 0.114392 mbar.
- Mean acceleration magnitude: 9.673996 m/s².

All samples were retained, including the gyro deviation at sample 20.
The cause of that deviation is unconfirmed.
Gyro averages are provisional offset estimates if the board was stationary.
This test does not establish absolute sensor accuracy.

Reproduce:
python3 analysis/analyze_csv.py data/SEN002.CSV

Saved report: analysis/SEN002-summary.txt

## Sensor plots
Generated analysis/SEN002-plots.png using analysis/plot_csv.py.
The plots show pressure variation, approximately steady acceleration,
persistent gyro offsets, and a brief gyro deviation at sample 20.
All recorded acquisition-start intervals are 1000 ms.

All samples remain included. No calibration or smoothing was applied.
The 1 Hz recording does not characterize high-frequency sensor noise.
Python dependencies are recorded in analysis/requirements.txt.

## Warmed-up stationary repeat test
V2 remained powered for two minutes, then the same firmware was
uploaded again without disconnecting power. The board was left stationary.

SEN004.CSV contained 60 samples with 1000 ms intervals.
Mean gyro readings (rad/s):
- X: -0.051917
- Y:  0.014250
- Z: -0.016633

The averages were close to SEN002, with lower variation on all gyro axes.
No deviation comparable to SEN002 sample 20 appeared in this recording.
All samples were retained.

These are provisional stationary offset estimates.
Temperature dependence and corrected performance remain untested.
The recorded temperature is from the barometer, not the IMU.

## Independent gyro-offset validation
Estimated fixed offsets from warmed-up stationary recording SEN004.CSV.
Applied that saved profile to independent stationary recording SEN006.CSV.

SEN006 contained 60 samples with 1000 ms intervals.
Corrected gyro means (rad/s):
- X: +0.000466667
- Y: -0.000150000
- Z: approximately zero

Mean offsets decreased on all axes.
Subtracting constants did not reduce sample standard deviations.
All samples and original raw values were retained.
Temperature dependence and performance during movement remain untested.

Reproduce:
python analysis/apply_gyro_bias.py data/SEN006.CSV analysis/SEN004-gyro-bias.json

Derived data: analysis/SEN006-gyro-corrected.csv
The script refuses to overwrite an existing derived file.

## Firmware gyro-offset correction — October 31, 2026
Applied the fixed SEN004 gyro offsets in firmware.
Preserved raw gyro readings and added three corrected columns.
CAL-prefixed session files use the expanded 13-column format.

CAL000.CSV readback verified:
- 60 samples numbered 0–59.
- All acquisition-start intervals were 1000 ms.
- All corrected values matched raw minus the firmware offsets
  to the saved precision.

Corrected gyro means (rad/s):
- X: -0.000470
- Y: -0.000056
- Z: -0.000323

Analysis and plotting tools support both 10- and 13-column files.
Raw and corrected gyro plots use independent vertical scales.
Offset subtraction does not reduce random variation.

The supplied console output also contained an earlier MPU6050
initialization failure and a USB disconnect/reconnect.
Startup reliability remains unresolved.
The card's filesystem date was incorrect; elapsed acquisition
timestamps were checked independently.