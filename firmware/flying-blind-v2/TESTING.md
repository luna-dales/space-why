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