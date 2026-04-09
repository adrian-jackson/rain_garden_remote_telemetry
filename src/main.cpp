/*
 * SIM7000 Timed HTTPS POST/GET Example
 * Adapted from Botletics SIM7000 demo sketch.
 *
 * Serial is OUTPUT ONLY - used for debug logging and event reporting.
 * No serial input is read anywhere in this sketch.
 *
 * Fill in:
 *   APN              - your SIM card's APN
 *   SERVER_HOST      - your server hostname (no https://)
 *   SERVER_PATH      - the path/endpoint on that server (for POST)
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
#define SERVER_HOST      "example.com"       // ← no https://, no trailing slash
#define SERVER_PATH      ""                  // ← endpoint path for POST
#define POST_INTERVAL_MS 30000UL             // ← 30 seconds
#define BOTLETICS_SSL 0

// Set to 1 to perform an HTTP GET to http://example.com/ instead of POST
#define GET_MODE 1
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
int  performGET();        // returns HTTP status or -1 on failure
// ─────────────────────────────────────────────────────────────────────────────

// Diagnostic helper: send AT command and print modem reply
void runAT(const char* cmd, unsigned long timeout=3000) {
  while (modemSS.available()) modemSS.read();
  Serial.print(F(">>> "));
  Serial.println(cmd);
  modemSS.println(cmd);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (modemSS.available()) {
      Serial.write(modemSS.read());
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  Serial.println(F(""));
  Serial.println(F("============================="));
  Serial.println(F(" SIM7000 HTTPS POST/GET logger"));
  Serial.println(F("============================="));

  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);

  modem.powerOn(PWRKEY);

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

  gprsReady = connectGPRS();

  lastPostTime = millis() - POST_INTERVAL_MS;

  Serial.print(F("[MAIN] Interval (ms): "));
  Serial.println(POST_INTERVAL_MS);
  Serial.println(F("-----------------------------"));
}

void loop() {
  unsigned long now = millis();

  if (now - lastPostTime >= POST_INTERVAL_MS) {
    lastPostTime = now;
    postCount++;

    Serial.println(F(""));
    Serial.print(F("[TASK] Attempt #"));
    Serial.println(postCount);

    if (!gprsReady) {
      Serial.println(F("[GPRS] Reconnecting..."));
      gprsReady = connectGPRS();
    }

    if (gprsReady) {
      // Add this before calling AT+SHCONN / before modem.HTTP_connect in your sketch.
      // Assumes runAT(cmd, timeout) helper from your sketch is available
      // and that PDP context 1 is the active context (from AT+CGPADDR).

      // Ensure verbose errors
      runAT("AT+CMEE=2", 2000);

      // Bind SH service to PDP CID 1 (use the CID that returned a non-zero IP)
      runAT("AT+SHCONF=\"CID\",1", 2000);

      // Disable SSL for plain HTTP
      runAT("AT+SHSSL=0", 2000);

      // Set the target URL (include http:// or https:// as appropriate)
      runAT("AT+SHCONF=\"URL\",\"http://example.com\"", 2000);

      // Optional: set expected header/body buffer sizes
      runAT("AT+SHCONF=\"HEADERLEN\",350", 2000);
      runAT("AT+SHCONF=\"BODYLEN\",1024", 2000);

      // Now attempt to connect
      runAT("AT+SHCONN", 10000);

      // Check state
      runAT("AT+SHSTATE?", 2000);


        

      int statusCode = -1;
      if (GET_MODE) {
        statusCode = performGET();
      } else {
        statusCode = postJSON();
      }

      if (statusCode == -1) {
        Serial.println(F("[TASK] FAILED - could not connect or no response"));
      } else {
        successCount++;
        Serial.print(F("[TASK] HTTP status: "));
        Serial.println(statusCode);
        if (statusCode < 200 || statusCode >= 300) {
          Serial.println(F("[TASK] WARNING: non-2xx status"));
        } else {
          Serial.println(F("[TASK] OK"));
        }
      }

      Serial.print(F("[TASK] Success rate: "));
      Serial.print(successCount);
      Serial.print(F("/"));
      Serial.println(postCount);

    } else {
      Serial.println(F("[TASK] Skipped - no GPRS connection"));
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

// ── HTTP GET ──────────────────────────────────────────────────────────────────
// Returns HTTP status code or -1 on failure.
int performGET() {
  Serial.print(F("[GET] Host: " SERVER_HOST));
  Serial.println(F(""));
  Serial.println(F("[GET] Path: /"));

  // Connect using HTTP_connect (library expects full URL)
  if (!modem.HTTP_connect("http://" SERVER_HOST)) {
    Serial.println(replybuffer);
    return -1;
  }

  // Use HTTP_GET provided by library; it stores response in replybuffer
  bool got = modem.HTTP_GET("/");

  if (!got) {
    Serial.println(F("[GET] HTTP_GET failed"));
    return -1;
  }

  // replybuffer should start with status code (library behavior)
  Serial.print(F("[GET] Raw reply: "));
  Serial.println(replybuffer);

  int statusCode = atoi(replybuffer);
  if (statusCode == 0) statusCode = 200;
  return statusCode;
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

  if (!modem.HTTP_connect("http://" SERVER_HOST)) {
    Serial.println(replybuffer);
    return -1;
  }

  modem.HTTP_addHeader("Content-Type", "application/json", 16);

  bool sent = modem.HTTP_POST(SERVER_PATH, body, strlen(body));

  if (!sent) {
    return -1;
  }

  int statusCode = atoi(replybuffer);
  if (statusCode == 0) statusCode = 200;

  return statusCode;
}

// ── Build your JSON body here ─────────────────────────────────────────────────

void buildJSONBody(char *buf, uint16_t bufLen) {
  float temperature = analogRead(A0) * 1.23;
  uint16_t battMv   = 0;
  if (!modem.getBattVoltage(&battMv)) battMv = 0;

  char tempStr[12];
  dtostrf(temperature, 1, 2, tempStr);

  snprintf(buf, bufLen,
    "{\"device\":\"%s\",\"temp\":\"%s\",\"batt\":%u}",
    imei, tempStr, battMv
  );
}
