#include "Arduino.h"
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
#define GET_MODE 0
// ─────────────────────────────────────────────────────────────────────────────

// Pin definitions for SIM7000 / SIM7070 shield
#define PWRKEY 6
#define RST    7
#define TX     10  // Microcontroller RX
#define RX     11  // Microcontroller TX

#include <SoftwareSerial.h>
SoftwareSerial modemSS(TX, RX);

Botletics_modem_LTE modem = Botletics_modem_LTE();

char imei[16] = {0};
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
void CIPTCP();            //use TCP socket to send data
bool waitForChar(Stream &serial, char target, unsigned long timeoutMs, char *buf = nullptr, size_t bufLen = 0);
// ─────────────────────────────────────────────────────────────────────────────

// Diagnostic helper: send AT command and print modem reply
bool runAT(const char* cmd, unsigned long timeout=3000, char targetChar='1') {
  while (modemSS.available()) modemSS.read();
  Serial.print(F(">>> "));
  Serial.println(cmd);
  modemSS.println(cmd);
  unsigned long start = millis();
  /*
  if(targetChar!=1){
    while (millis() - start < timeout) {
      while (modemSS.available()) {
        char c = modemSS.read();
        if(c==targetChar){
          Serial.println(c);
          Serial.println("[INFO] Target char recv");
        }
        Serial.println(c);
        return true;
      }
    }
  }else{*/
    while (millis() - start < timeout) {
      while (modemSS.available()) {
        Serial.write(modemSS.read());
      }
    }
    Serial.println();
    return false;
  //}
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
      int statusCode = -1;
      //sendCloudSocketPayload();


      if (GET_MODE) {
        statusCode = performGET();
      } else {
        CIPTCP();
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


//── TCP SOCKET  ──────────────────────────────────────────────────────────────────
void CIPTCP() {
  runAT("AT+CIPSHUT");
  runAT("AT+CIPMUX=0");
  runAT("AT+CSTT=\"\""); //(or you already have CGDCONT)
  runAT("AT+CIICR");// (bring up wireless connection)
  runAT("AT+CIFSR");// (get local IP)
  runAT("AT+CIPSTART=\"TCP\",\"example.com\",\"80\"");
  runAT("AT+CIPSEND");
  delay(1000);
  //if (waitForChar(Serial, '>', 5000)) {         
    Serial.print("GET / HTTP/1.1\r\n");
    Serial.print("Host: example.com\r\n");
    Serial.print("Connection: close\r\n");
    Serial.print("User-Agent: SIM7000\r\n");
    Serial.print("Accept: */*\r\n");
    Serial.print("\r\n");
    Serial.write(0x1A);                  // Ctrl+Z terminator
  //}
    unsigned long start = millis();
    unsigned long timeout = 5000; // 15 seconds

    while (millis() - start < timeout) {
      if (Serial.available()) {
        char c = Serial.read();
        // forward to USB monitor or another port:
        //Serial.write(c);
        // optionally accumulate: buffer += c;
      }
      // small yield to avoid locking the MCU
      delay(10);
    }
  //delay(10000);
  runAT("AT+CIPCLOSE");
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


bool waitForChar(Stream &serial, char target, unsigned long timeoutMs, char *buf = nullptr, size_t bufLen = 0) {
  unsigned long start = millis();
  size_t idx = 0;
  if (buf && bufLen > 0) buf[0] = '\0';
  while (millis() - start < timeoutMs) {
    while (serial.available()) {
      int c = serial.read();
      if (c < 0) continue;
      if (buf && idx + 1 < bufLen) {
        buf[idx++] = (char)c;
        buf[idx] = '\0';
      }
      if ((char)c == target) return true;
    }
    delay(1);
  }
  return false;
}