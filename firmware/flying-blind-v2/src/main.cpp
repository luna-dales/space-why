	#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Flying Blind V2 — LED ON");
    delay(500);

    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Flying Blind V2 — LED OFF");
    delay(500);
}
