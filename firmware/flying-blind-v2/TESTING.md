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