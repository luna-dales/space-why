#include <Arduino.h>
#include <Wire.h>

void setup()
{
    Serial.begin(115200);

    Wire.setSDA(18);
    Wire.setSCL(19);
    Wire.begin();
    Wire.setClock(100000);
}

void loop()
{
    Serial.println("Flying Blind V2 — I2C scan");
    int found = 0;

    for (uint8_t address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        uint8_t result = Wire.endTransmission();

        if (result == 0)
        {
            Serial.print("Device found at 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
            found++;
        }
    }

    Serial.print("Devices found: ");
    Serial.println(found);
    Serial.println();
    delay(3000);
}