#include <Arduino.h>
#include <Wire.h>
#include <MS5611.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <SD.h>

MS5611 barometer(0x77);
Adafruit_MPU6050 imu;

bool barometerReady = false;
bool imuReady = false;

uint32_t sampleNumber = 0;

const uint32_t sampleIntervalMs = 1000;
uint32_t lastSampleMs = 0;

const uint8_t sdChipSelect = 10;
bool sdReady = false;
bool sdTestWritePassed = false;

uint32_t barometerRowsWritten = 0;

bool saveBarometerSample(uint32_t number, uint32_t timeMs, float pressure, float temperature)
{
    File dataFile = SD.open("V2BARO.CSV", FILE_WRITE);

    if (!dataFile)
    {
        return false;
    }

    const char header[] =
        "sample,start_ms,pressure_mbar,temperature_c\n";

    if (dataFile.size() == 0)
    {
        if (dataFile.print(header) != sizeof(header) - 1)
        {
            dataFile.close();
            return false;
        }
    }

    char row[96];
    int length = snprintf(
        row, sizeof(row), "%lu,%lu,%.2f,%.2f\n",
        (unsigned long)number,
        (unsigned long)timeMs,
        pressure, temperature);

    if (length <= 0 || length >= (int)sizeof(row))
    {
        dataFile.close();
        return false;
    }

    size_t written = dataFile.write(
        reinterpret_cast<const uint8_t *>(row), length);

    dataFile.close();
    return written == (size_t)length;
}

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
        // Alternate calculation required by this barometer module.
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
    if (sdReady)
    {
        File testFile = SD.open("V2TEST.TXT", FILE_WRITE);

        if (testFile)
        {
            const char testLine[] = "Flying Blind V2: microSD write test\n";

            size_t bytesWritten = testFile.print(testLine);
            testFile.close();

            sdTestWritePassed =
                (bytesWritten == sizeof(testLine) - 1);
        }
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

    Serial.print("Sample: ");
    Serial.print(sampleNumber++);
    Serial.print(" | Start (ms since boot): ");
    Serial.println(nowMs);

    Serial.print("microSD initialization: ");
    Serial.println(sdReady ? "SUCCESS" : "FAILED");

    Serial.print("microSD test write: ");
    Serial.println(sdTestWritePassed ? "PASSED" : "FAILED");

    if (!barometerReady)
    {
        Serial.println("ERROR: Barometer initialization failed.");
    }
    else
    {
        int result = barometer.read();

        if (result == MS5611_READ_OK)
        {
            Serial.print("Pressure: ");
            Serial.print(barometer.getPressure(), 2);
            Serial.print(" mbar | Temperature: ");
            Serial.print(barometer.getTemperature(), 2);
            Serial.println(" C");

            if (sdReady && barometerRowsWritten < 10)
            {
                // sampleNumber was incremented when printed above.
                bool saved = saveBarometerSample(
                    sampleNumber - 1, nowMs,
                    barometer.getPressure(),
                    barometer.getTemperature());

                if (saved)
                {
                    barometerRowsWritten++;
                    Serial.print("Barometer CSV rows saved: ");
                    Serial.println(barometerRowsWritten);

                    if (barometerRowsWritten == 10)
                    {
                        Serial.println("CSV capture complete. File closed.");
                    }
                }
                else
                {
                    Serial.println("ERROR: Barometer CSV write failed.");
                }
            }
        }
        else
        {
            Serial.print("ERROR: Barometer read failed. Code: ");
            Serial.println(result);
        }
    }

    if (!imuReady)
    {
        Serial.println("ERROR: MPU6050 initialization failed.");
    }
    else
    {
        sensors_event_t acceleration, rotation, temperature;

        if (imu.getEvent(&acceleration, &rotation, &temperature))
        {
            Serial.print("Acceleration (m/s^2): X=");
            Serial.print(acceleration.acceleration.x, 3);
            Serial.print(" Y=");
            Serial.print(acceleration.acceleration.y, 3);
            Serial.print(" Z=");
            Serial.println(acceleration.acceleration.z, 3);

            Serial.print("Angular velocity (rad/s): X=");
            Serial.print(rotation.gyro.x, 3);
            Serial.print(" Y=");
            Serial.print(rotation.gyro.y, 3);
            Serial.print(" Z=");
            Serial.println(rotation.gyro.z, 3);
        }
        else
        {
            Serial.println("ERROR: MPU6050 read failed.");
        }
    }

    Serial.println();
}