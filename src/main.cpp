#include <Arduino.h>

int POS_BATT_PIN = 1;
int NEG_BATT_PIN = 2;

//note down starting time of program!
//assuming voltage divider for positive terminal with 1k and 10kOhm resistor

int readVoltage() {
    int r1k_pos = analogRead(POS_BATT_PIN);
    int r1k_neg = analogRead(NEG_BATT_PIN);
    return 11 * (5/1024) * (r1k_pos - r1k_neg);
}

void testSerial() {
    Serial.println("test!");
}

void setup() {
    pinMode(POS_BATT_PIN, INPUT);
    pinMode(NEG_BATT_PIN, INPUT);

    Serial.begin(9600);
}

void loop() {
  testSerial();
  delay(10); //delay n seconds
}

