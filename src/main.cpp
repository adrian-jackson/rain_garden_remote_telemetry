#include <Arduino.h>
#include "BotleticsSIM7000.h" 
#include <SoftwareSerial.h>

//SIM7000 modem setup
SoftwareSerial simSerial(7, 8); // RX, TX — adjust to your wiring
Botletics_modem modem = Botletics_modem();
bool GPS = false; //set to true if GPS is needed

//battery monitoring
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

void setupSim7000(){
    modem.begin(simSerial);
    simSerial.begin(9600);
    Serial.println("Initializing modem...");
    while (!modem.begin(simSerial)) {
        Serial.println("Retrying...");
        delay(2000);
    }

    Serial.println("Setting preferred mode to LTE-M...");
    modem.setPreferredLTEMode(1);      // 1 = LTE-M, 2 = NB-IoT
    modem.setOperatingBand("CAT-M", 12); // Band 12 is common in the US (AT&T/Hologram)

    modem.setNetworkSettings(F("hologram")); // Hologram APN

    if(GPS){
        Serial.println("Enabling GPRS...");
        while (!modem.enableGPRS(true)) {
            Serial.println("Retrying GPRS...");
            delay(2000);
        }
    }
    Serial.println("Connected!\nModem Connection Status and Strength:\n");

    Serial.println(modem.getNetworkStatus()); // Should return 1 (registered)
    Serial.println(modem.getRSSI());          // Signal strength, >5 is usable
}

void testTransmitSIM7000() {
// Send a simple HTTP POST to a test server
  uint16_t statusCode;
  int16_t responseLen;

  if (!modem.HTTP_POST_start("httpbin.org/post", F("application/json"),
                              F("{\"test\":\"hello\"}"), 17,
                              &statusCode, (uint16_t *)&responseLen)) {
    Serial.println("HTTP POST failed");
  } else {
    Serial.print("Status: "); Serial.println(statusCode);
    while (responseLen > 0) {
      uint8_t buf[64];
      uint16_t toRead = min((uint16_t)sizeof(buf), (uint16_t)responseLen);
      modem.HTTP_readAll_data(buf, toRead);
      responseLen -= toRead;
    }
    modem.HTTP_POST_end();
  }
}

void setup() {
    //battery monitoring pins
    pinMode(POS_BATT_PIN, INPUT);
    pinMode(NEG_BATT_PIN, INPUT);

    //SIM7000 modem setup
    setupSim7000();

    //general
    Serial.begin(9600);
}

void loop() {
  testTransmitSIM7000();
    delay(30000); // Send every 30s
}

