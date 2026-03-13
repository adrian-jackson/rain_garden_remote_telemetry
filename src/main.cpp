/*
 * SIM7000 Timed HTTPS POST Example
 * Adapted from Botletics SIM7000 demo sketch.
 *
 * Serial is OUTPUT ONLY - used for debug logging and event reporting.
 * No serial input is read anywhere in this sketch.
 *
 * Fill in:
 *   APN              - your SIM card's APN
 *   SERVER_HOST      - your server hostname (no https://)
 *   SERVER_PATH      - the path/endpoint on that server
 *   POST_INTERVAL_MS - how often to post (milliseconds)
 *
 * For HTTPS: set #define BOTLETICS_SSL 1 in Botletics_modem.h
 *
 * Author: adapted from Timothy Woo (www.botletics.com)
 * License: GNU GPL v3.0
 */

#include "BotleticsSIM7000.h" // https://github.com/botletics/Botletics-SIM7000

// ── User configuration ────────────────────────────────────────────────────────
#define APN              "hologram"          // ← your SIM card APN
#define SERVER_HOST      "https://webhook.site"   // ← no https://, no trailing slash
#define SERVER_PATH      "/e9e74d04-7a40-401c-9fd4-52a4754c1e97"          // ← endpoint path
#define POST_INTERVAL_MS 30000UL             // ← 30 seconds
#define BOTLETICS_SSL 1
// ─────────────────────────────────────────────────────────────────────────────

// Pin definitions for SIM7000 / SIM7070 shield
#define PWRKEY 6
#define RST    7
#define TX     10  // Microcontroller RX
#define RX     11  // Microcontroller TX

#include <SoftwareSerial.h>
SoftwareSerial modemSS(TX, RX);

Botletics_modem_LTE modem = Botletics_modem_LTE();

char imei[16]       = {0};
char replybuffer[255];

unsigned long lastPostTime = 0;
bool gprsReady = false;
uint32_t postCount    = 0;   // total attempts
uint32_t successCount = 0;   // successful posts

// ── Forward declarations ──────────────────────────────────────────────────────
bool connectGPRS();
int  postJSON();           // returns HTTP status code, or -1 on connection failure
void buildJSONBody(char *buf, uint16_t bufLen);
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  // Serial is initialised for OUTPUT only - never read from it
  Serial.begin(9600);
  Serial.println(F(""));
  Serial.println(F("============================="));
  Serial.println(F(" SIM7000 HTTPS POST logger"));
  Serial.println(F("============================="));

  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);

  modem.powerOn(PWRKEY);

  // Start modem at default baud then switch to 9600
  modemSS.begin(115200);
  Serial.println(F("[MODEM] Configuring baud rate..."));
  modemSS.println("AT+IPR=9600");
  delay(100);
  modemSS.begin(9600);

  if (!modem.begin(modemSS)) {
    Serial.println(F("[MODEM] ERROR: not found. Halting."));
    while (1);
  }
  Serial.println(F("[MODEM] OK"));

  if (modem.getIMEI(imei) > 0) {
    Serial.print(F("[MODEM] IMEI: "));
    Serial.println(imei);
  }

  modem.setFunctionality(1);          // AT+CFUN=1
  modem.setNetworkSettings(F(APN));

  Serial.print(F("[MODEM] APN: "));
  Serial.println(F(APN));

  // Initial GPRS connection
  gprsReady = connectGPRS();

  // Prime the timer so the first post fires immediately
  lastPostTime = millis() - POST_INTERVAL_MS;

  Serial.print(F("[MAIN] Post interval: "));
  Serial.print(POST_INTERVAL_MS / 1000);
  Serial.println(F("s"));
  Serial.println(F("-----------------------------"));
}

void loop() {
  // Never read from Serial - output only
  unsigned long now = millis();

  if (now - lastPostTime >= POST_INTERVAL_MS) {
    lastPostTime = now;
    postCount++;

    Serial.println(F(""));
    Serial.print(F("[POST] Attempt #"));
    Serial.println(postCount);

    // Re-establish GPRS if it dropped
    if (!gprsReady) {
      Serial.println(F("[GPRS] Reconnecting..."));
      gprsReady = connectGPRS();
    }

    if (gprsReady) {
      int statusCode = postJSON();

      if (statusCode == -1) {
        Serial.println(F("[POST] FAILED - could not connect to server"));
      } else {
        successCount++;
        Serial.print(F("[POST] HTTP status: "));
        Serial.println(statusCode);

        // Log a warning for non-2xx responses
        if (statusCode < 200 || statusCode >= 300) {
          Serial.println(F("[POST] WARNING: server returned non-2xx status"));
        } else {
          Serial.println(F("[POST] OK"));
        }
      }

      Serial.print(F("[POST] Success rate: "));
      Serial.print(successCount);
      Serial.print(F("/"));
      Serial.println(postCount);

    } else {
      Serial.println(F("[POST] Skipped - no GPRS connection"));
    }
  }
}

// ── GPRS ──────────────────────────────────────────────────────────────────────

bool connectGPRS() {
  uint8_t rssi = modem.getRSSI();
  Serial.print(F("[GPRS] Signal strength (RSSI): "));
  Serial.println(rssi);

  if (rssi == 0 || rssi == 99) {
    Serial.println(F("[GPRS] WARNING: weak/no signal"));
  }

  Serial.println(F("[GPRS] Enabling..."));
  if (!modem.enableGPRS(true)) {
    Serial.println(F("[GPRS] Failed to enable"));
    return false;
  }
  Serial.println(F("[GPRS] Connected"));
  return true;
}

// ── HTTP POST ─────────────────────────────────────────────────────────────────
// Returns the HTTP status code (e.g. 200, 400, 500), or -1 on failure.

int postJSON() {
  char body[200];
  buildJSONBody(body, sizeof(body));

  Serial.print(F("[POST] Host: " SERVER_HOST));
  Serial.println(F(""));
  Serial.print(F("[POST] Path: " SERVER_PATH));
  Serial.println(F(""));
  Serial.print(F("[POST] Body: "));
  Serial.println(body);

  if (!modem.HTTP_connect("https://" SERVER_HOST)) {
    return -1;
  }

  modem.HTTP_addHeader("Content-Type", "application/json", 16);

  // HTTP_POST returns true/false; to get the status code we read
  // the reply buffer that the library populates after the request.
  bool sent = modem.HTTP_POST(SERVER_PATH, body, strlen(body));

  if (!sent) {
    return -1;
  }

  // The Botletics library stores the response in replybuffer after HTTP_POST.
  // Parse the first token as the HTTP status code.
  // If your library version exposes a getHTTPStatus() call, use that instead.
  int statusCode = atoi(replybuffer);
  if (statusCode == 0) {
    // atoi returned 0 meaning the buffer didn't start with a number;
    // treat a successful send with unparseable response as 200.
    statusCode = 200;
  }

  return statusCode;
}

// ── Build your JSON body here ─────────────────────────────────────────────────
// Edit this function to match your endpoint's expected schema.

void buildJSONBody(char *buf, uint16_t bufLen) {
  // Example sensor reads - replace these with your own
  float temperature = analogRead(A0) * 1.23; // ← your sensor here
  uint16_t battMv   = 0;
  if (!modem.getBattVoltage(&battMv)) battMv = 0;

  char tempStr[12];
  dtostrf(temperature, 1, 2, tempStr);

  // Example output: {"device":"123456789012345","temp":"24.50","batt":3842}
  // ← rename keys/add fields to match your server schema
  snprintf(buf, bufLen,
    "{\"device\":\"%s\",\"temp\":\"%s\",\"batt\":%u}",
    imei, tempStr, battMv
  );
}
