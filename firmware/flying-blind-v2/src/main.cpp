/*
 * Flying Blind V2 bench recorder for Teensy 4.0.
 *
 * setup() initializes the I2C sensors and SPI card once, then creates a
 * session. loop() schedules one acquisition per second, reads both sensors,
 * calculates gyro corrections and barometric Kalman estimates, and formats
 * the same numeric CSV row for serial output and storage.
 *
 * Only 60 successful rows are saved per startup; live acquisition continues
 * afterward. Existing sensor startup failures disable this session rather
 * than triggering automatic retries. Sensor reads and file writes block.
 * This is an experimental recorder, with no flight-control outputs.
 */
#include <Arduino.h>
#include <Wire.h>
#include <MS5611.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <SD.h>
#include "AltitudeKalman.h"

// I2C addresses verified on this assembly. Both devices share Wire/SDA/SCL;
// their numeric addresses are unrelated to Teensy physical pin numbers.
MS5611 barometer(0x77);
Adafruit_MPU6050 imu;

// External SPI card: CS=10, MOSI=11, MISO=12, SCK=13.
// Changing captureLimit affects saved rows, not the live sampling duration.
const uint8_t sdChipSelect = 10;
const uint32_t sampleIntervalMs = 1000;
const uint32_t captureLimit = 60;

// These remember setup results. Separate begin/reset flags identify which
// barometer initialization stage failed; they are not live health checks.
// recordingReady also becomes false after an append failure.
bool barometerReady = false;
bool barometerBeginOK = false;
bool barometerResetOK = false;
bool imuReady = false;
bool sdReady = false;
bool recordingReady = false;

// sampleNumber counts scheduled attempts, including rejected measurements.
// rowsWritten counts successful appends, so gaps in saved sample numbers
// reveal skipped attempts. millis() timestamps are elapsed time, not dates.
uint32_t sampleNumber = 0;
uint32_t lastSampleMs = 0;
uint32_t rowsWritten = 0;

// Fits an 8.3-style filename plus its terminating null byte.
char sessionFilename[13] = "";

// Reference pressure stays fixed until the next startup. lastFilterMs marks
// the previous successful estimator sample, independently of loop scheduling.
AltitudeKalman altitudeFilter;
double referencePressureMbar = 0.0;
uint32_t lastFilterMs = 0;

// Column order must match snprintf() below and the analysis schema exactly.
// Temperature comes from the barometer. Acceleration is sensor-frame m/s^2
// including gravity; gyro values are rad/s. Sigma fields are model-based
// standard deviations. Repeated variance fields preserve the tuning used.
const char csvHeader[] =
    "sample,start_ms,pressure_mbar,temperature_c,"
    "ax_mps2,ay_mps2,az_mps2,gx_radps,gy_radps,gz_radps,"
    "gx_radps_corrected,gy_radps_corrected,gz_radps_corrected,"
    "reference_pressure_mbar,baro_altitude_m,kf_altitude_m,kf_velocity_mps,"
    "kf_altitude_sigma_m,kf_velocity_sigma_mps,"
    "kf_measurement_variance_m2,kf_acceleration_variance_m2s4\n";

// Select the first unused KAL000.CSV..KAL999.CSV, create its header, and close
// it. Existing recordings are skipped. Failure (including an exhausted name
// range) leaves recording disabled. The header's terminating null is not saved.
bool createSession()
{
    for (unsigned int index = 0; index < 1000; index++)
    {
        snprintf(sessionFilename, sizeof(sessionFilename),
                 "KAL%03u.CSV", index);

        if (SD.exists(sessionFilename))
        {
            continue;
        }

        File file = SD.open(sessionFilename, FILE_WRITE);

        if (!file)
        {
            return false;
        }

        size_t written = file.print(csvHeader);
        file.close();

        return written == sizeof(csvHeader) - 1;
    }

    return false;
}

// Append exactly length bytes, then close the file after each row. This
// simple bench strategy trades throughput for frequent filesystem updates;
// it is not buffered high-rate logging or a guarantee against power-loss
// corruption. The return value checks the accepted byte count, not a readback.
bool appendRow(const char *row, size_t length)
{
    File file = SD.open(sessionFilename, FILE_WRITE);

    if (!file)
    {
        return false;
    }

    size_t written = file.write(
        reinterpret_cast<const uint8_t *>(row), length);

    file.close();
    return written == length;
}

// Fixed profile: analysis/SEN004-gyro-bias.json
// Units: rad/s. Rounded to nine decimal places.
const double gyroOffsetX = -0.051916667;
const double gyroOffsetY =  0.014250000;
const double gyroOffsetZ = -0.016633333;

// Run once per boot/upload. Neither sensor initialization nor session
// creation is retried here. Gyro offsets above are fixed copied constants;
// firmware does not read the JSON calibration profile from the SD card.
void setup()
{
    Serial.begin(115200);

    // Configure this assembly's I2C pins before starting the bus.
    // USB serial is not awaited, so acquisition can run without a monitor.
    Wire.setSDA(18);
    Wire.setSCL(19);
    Wire.begin();
    Wire.setClock(100000);

    barometerBeginOK = barometer.begin();

    if (barometerBeginOK)
    {
        // Our testing showed that the pressure readings were off.  Using reset(1) doubles the values returned
        // which was closer to the expected values for our test site.
        barometerResetOK = barometer.reset(1);
    }

    barometerReady = barometerBeginOK && barometerResetOK;

    imuReady = imu.begin(0x68, &Wire);

    if (imuReady)
    {
        // Initial settings for gentle bench testing.
        imu.setAccelerometerRange(MPU6050_RANGE_8_G);
        imu.setGyroRange(MPU6050_RANGE_500_DEG);
        imu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    // SD.begin() checks card/filesystem access; it does not prove a write.
    // createSession() checks a header write after both sensors initialize.
    sdReady = SD.begin(sdChipSelect);

    // Create a recording only if both sensors initialized.
    if (sdReady && barometerReady && imuReady)
    {
        recordingReady = createSession();
    }
}

void loop()
{
    // This saves our starting time as the number of ms since the Teensy started
    uint32_t nowMs = millis();

    // we only want to sample if the time since the last samping is >= to our sampling interval
    if (nowMs - lastSampleMs < sampleIntervalMs)
    {
        return;
    }

    // Save our current sample time and sample index
    lastSampleMs = nowMs;
    uint32_t currentSample = sampleNumber++;

    Serial.print("Sample: ");
    Serial.print(currentSample);
    Serial.print(" | Start (ms since boot): ");
    Serial.println(nowMs);

    // If one of our sensors isn't ready, then log the data
    if (!barometerReady || !imuReady)
    {
        Serial.print("Barometer initialization: ");
        Serial.println(barometerReady ? "SUCCESS" : "FAILED");
        Serial.print("MPU6050 initialization: ");
        Serial.println(imuReady ? "SUCCESS" : "FAILED");
        Serial.println("Recording disabled.");
        Serial.print("Barometer begin: ");
        Serial.println(barometerBeginOK ? "PASSED" : "FAILED");

        Serial.print("Barometer reset(1): ");
        Serial.println(
            !barometerBeginOK ? "NOT ATTEMPTED" :
            (barometerResetOK ? "PASSED" : "FAILED"));

        Wire.beginTransmission(0x77);
        uint8_t barometerStatus = Wire.endTransmission();

        Serial.print("Barometer address response: ");
        Serial.println(barometerStatus);
        Serial.println();
        return;
    }

    // Reads are sequential, not simultaneous. nowMs identifies the start of
    // this group; it is not the exact conversion instant of every sensor.
    int barometerResult = barometer.read();

    sensors_event_t acceleration, rotation, imuTemperature;
    bool imuReadOK =
        imu.getEvent(&acceleration, &rotation, &imuTemperature);

    // A library-reported failure rejects the entire row. No stale row is
    // substituted. The IMU library return is used as reported; this does not
    // add an independent low-level transaction or sensor-validity check.
    if (barometerResult != MS5611_READ_OK || !imuReadOK)
    {
        Serial.print("Barometer read result: ");
        Serial.println(barometerResult);
        Serial.print("MPU6050 read reported: ");
        Serial.println(imuReadOK ? "SUCCESS" : "FAILED");
        Serial.println("Sample not recorded.");
        Serial.println();
        return;
    }

    const double pressureMbar = barometer.getPressure();
    if (!std::isfinite(pressureMbar) || pressureMbar <= 0.0)
    {
        Serial.println("ERROR: Invalid pressure. Sample not recorded.");
        return;
    }

    // Experimental barometer-only estimator. IMU acceleration is logged,
    // but NOT used as vertical acceleration before orientation validation.
    // First accepted combined sample defines relative zero. Its pressure
    // noise shifts the reference for the entire session. The same sample
    // initializes the state and is not counted as a second correction.
    if (!altitudeFilter.initialized())
    {
        referencePressureMbar = pressureMbar;
        altitudeFilter.reset(0.0);
        lastFilterMs = nowMs;
    }

    const double baroAltitudeM = relativeBarometricAltitudeM(
        pressureMbar, referencePressureMbar);
    if (!std::isfinite(baroAltitudeM))
    {
        Serial.println("ERROR: Invalid altitude. Sample not recorded.");
        return;
    }

    // After skipped reads, elapsed time spans the whole gap. No prediction-
    // only records are emitted during that gap; the next accepted observation
    // advances and corrects the estimator. Sensor failures remain unresolved.
    if (nowMs != lastFilterMs)
    {
        // Unsigned subtraction also handles millis() rollover.
        const double dtSeconds = uint32_t(nowMs - lastFilterMs) / 1000.0;
        if (!altitudeFilter.update(baroAltitudeM, dtSeconds))
        {
            Serial.println("ERROR: Kalman update rejected. Sample not recorded.");
            return;
        }
        lastFilterMs = nowMs;
    }

    // Preserve raw gyro alongside raw-minus-offset. This changes the mean,
    // not random variation, and does not correct accelerometer readings.
    const double correctedGyroX = rotation.gyro.x - gyroOffsetX;
    const double correctedGyroY = rotation.gyro.y - gyroOffsetY;
    const double correctedGyroZ = rotation.gyro.z - gyroOffsetZ;

    // snprintf() reports the length that WOULD have been written. Reject
    // truncation rather than storing a shortened/malformed CSV row. Extra
    // printed decimals aid software comparisons; they do not imply accuracy.
    char row[512];

    int length = snprintf(
        row, sizeof(row),
        "%lu,%lu,%.2f,%.2f,%.3f,%.3f,%.3f,"
        "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
        (unsigned long)currentSample,
        (unsigned long)nowMs,
        barometer.getPressure(),
        barometer.getTemperature(),
        acceleration.acceleration.x,
        acceleration.acceleration.y,
        acceleration.acceleration.z,
        rotation.gyro.x,
        rotation.gyro.y,
        rotation.gyro.z,
        correctedGyroX,
        correctedGyroY,
        correctedGyroZ,
        referencePressureMbar,
        baroAltitudeM,
        altitudeFilter.altitudeM(),
        altitudeFilter.velocityMps(),
        altitudeFilter.altitudeSigmaM(),
        altitudeFilter.velocitySigmaMps(),
        AltitudeKalman::measurementVarianceM2,
        AltitudeKalman::accelerationVarianceM2S4);

    if (length <= 0 || length >= (int)sizeof(row))
    {
        Serial.println("ERROR: CSV row formatting failed.");
        return;
    }

    // Display the same row that will be written to the card.
    Serial.print(csvHeader);
    Serial.println("Gyro calibration profile: SEN004-gyro-bias");
    Serial.println("Altitude model: experimental barometer-only Kalman; IMU not fused");
    Serial.print(row);

    // Serial output is available even when storage is unavailable. Human-
    // readable status lines and repeated headers are not appended to the CSV.
    // The estimator continues updating after the saved-row limit is reached.
    if (!recordingReady)
    {
        Serial.println("Recording unavailable: check card/session creation.");
    }
    else if (rowsWritten < captureLimit)
    {
        if (appendRow(row, (size_t)length))
        {
            rowsWritten++;

            Serial.print("Session: ");
            Serial.print(sessionFilename);
            Serial.print(" | Rows saved: ");
            Serial.println(rowsWritten);

            if (rowsWritten == captureLimit)
            {
                Serial.println("CSV capture complete. File closed.");
            }
        }
        else
        {
            // Stop after a write failure to avoid further partial rows.
            recordingReady = false;
            Serial.println("ERROR: CSV write failed. Recording stopped.");
        }
    }
    else
    {
        Serial.print("Capture already complete: ");
        Serial.println(sessionFilename);
    }

    Serial.println();
}
