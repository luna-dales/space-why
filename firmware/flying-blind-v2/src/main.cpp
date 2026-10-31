#include <Arduino.h>
#include <Wire.h>
#include <MS5611.h>

MS5611 barometer(0x77);
bool barometerReady = false;

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
        // Test alternate pressure calculation for compatible sensors.
        barometerReady = barometer.reset(1);
    }
}

void loop()
{
    if (!barometerReady)
    {
        Serial.println("ERROR: Barometer initialization failed.");
        delay(1000);
        return;
    }

    int result = barometer.read();

    if (result == MS5611_READ_OK)
    {
        Serial.print("Pressure: ");
        Serial.print(barometer.getPressure(), 2);
        Serial.print(" mbar | Temperature: ");
        Serial.print(barometer.getTemperature(), 2);
        Serial.println(" C");
    }
    else
    {
        Serial.print("ERROR: Barometer read failed. Code: ");
        Serial.println(result);
    }

    delay(1000);
}