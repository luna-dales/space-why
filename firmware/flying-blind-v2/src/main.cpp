#include <Arduino.h>
#include <Wire.h>
#include <MS5611.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <SD.h>

MS5611 barometer(0x77);
Adafruit_MPU6050 imu;

const uint8_t sdChipSelect = 10;
const uint32_t sampleIntervalMs = 1000;
const uint32_t captureLimit = 60;

bool barometerReady = false;
bool imuReady = false;
bool sdReady = false;
bool recordingReady = false;

uint32_t sampleNumber = 0;
uint32_t lastSampleMs = 0;
uint32_t rowsWritten = 0;

char sessionFilename[13] = "";

const char csvHeader[] =
    "sample,start_ms,pressure_mbar,temperature_c,"
    "ax_mps2,ay_mps2,az_mps2,gx_radps,gy_radps,gz_radps,"
    "gx_radps_corrected,gy_radps_corrected,gz_radps_corrected\n";

bool createSession()
{
    for (unsigned int index = 0; index < 1000; index++)
    {
        snprintf(sessionFilename, sizeof(sessionFilename),
                 "CAL%03u.CSV", index);

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

void setup()
{
    Serial.begin(115200);

    Wire.setSDA(18);
    Wire.setSCL(19);
    Wire.begin();
    Wire.setClock(100000);

    barometerReady = barometer.begin();

    if (barometerReady)
    {
        // Alternate calculation used by this barometer module.
        barometerReady = barometer.reset(1);
    }

    imuReady = imu.begin(0x68, &Wire);

    if (imuReady)
    {
        // Initial settings for gentle bench testing.
        imu.setAccelerometerRange(MPU6050_RANGE_8_G);
        imu.setGyroRange(MPU6050_RANGE_500_DEG);
        imu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    sdReady = SD.begin(sdChipSelect);

    // Create a recording only if both sensors initialized.
    if (sdReady && barometerReady && imuReady)
    {
        recordingReady = createSession();
    }
}

void loop()
{
    uint32_t nowMs = millis();

    if (nowMs - lastSampleMs < sampleIntervalMs)
    {
        return;
    }

    lastSampleMs = nowMs;
    uint32_t currentSample = sampleNumber++;

    Serial.print("Sample: ");
    Serial.print(currentSample);
    Serial.print(" | Start (ms since boot): ");
    Serial.println(nowMs);

    if (!barometerReady || !imuReady)
    {
        Serial.print("Barometer initialization: ");
        Serial.println(barometerReady ? "SUCCESS" : "FAILED");
        Serial.print("MPU6050 initialization: ");
        Serial.println(imuReady ? "SUCCESS" : "FAILED");
        Serial.println("Recording disabled.");
        Serial.println();
        return;
    }

    int barometerResult = barometer.read();

    sensors_event_t acceleration, rotation, imuTemperature;
    bool imuReadOK =
        imu.getEvent(&acceleration, &rotation, &imuTemperature);

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

    const double correctedGyroX = rotation.gyro.x - gyroOffsetX;
    const double correctedGyroY = rotation.gyro.y - gyroOffsetY;
    const double correctedGyroZ = rotation.gyro.z - gyroOffsetZ;

    char row[256];

    int length = snprintf(
        row, sizeof(row),
        "%lu,%lu,%.2f,%.2f,%.3f,%.3f,%.3f,"
        "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
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
        correctedGyroZ);

    if (length <= 0 || length >= (int)sizeof(row))
    {
        Serial.println("ERROR: CSV row formatting failed.");
        return;
    }

    // Display the same row that will be written to the card.
    Serial.print(csvHeader);
    Serial.println("Gyro calibration profile: SEN004-gyro-bias");
    Serial.print(row);

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