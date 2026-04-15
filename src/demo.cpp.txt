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
bool CIPTCP();            //use TCP socket to send data
// ─────────────────────────────────────────────────────────────────────────────

// Diagnostic helper: send AT command and print modem reply
bool runAT(const char* cmd, unsigned long timeout=3000, char targetChar='0') {
  bool charFound = false;
  while (modemSS.available()) modemSS.read();
  Serial.print(F(">>> "));
  Serial.println(cmd);
  modemSS.println(cmd);
  unsigned long start = millis();
   //THIS BLOCK (USED TO) STOP MODEM RESPONSES FROM LOGGING IN SERIAL <_BUG (FIXED?)
  if(targetChar!='0'){
    while (millis() - start < timeout) {
      while (modemSS.available()) {
        char c = modemSS.read();
        Serial.print(c);
        if(c==targetChar){     
          Serial.println("\r\n[INFO] Target char recv");
          charFound = true;
        }
        
      }
    }
  }else{ //END PROBLEM BLOCK
    while (millis() - start < timeout) {
      while (modemSS.available()) {
        Serial.write(modemSS.read());
      }
    }
    Serial.println(); 
  }
  return charFound;
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

      //send HTTP over TCP/IP
      statusCode = CIPTCP();


      if (statusCode == -1) {
        Serial.println(F("[TASK] FAILED - could not connect or no response"));
      } else {
        successCount++;
        Serial.println(F("[TASK] SUCCESS - 200 OK received"));
        /* //to be implemented
        Serial.print(F("[TASK] HTTP status: "));
        Serial.println(statusCode);
        if (statusCode < 200 || statusCode >= 300) {
          Serial.println(F("[TASK] WARNING: non-2xx status"));
        } else {
          Serial.println(F("[TASK] OK"));
        }
          */
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


//
int readHttpStatusFromModem(unsigned long timeoutMs = 15000) {
  const size_t BUF_SZ = 256; // reduce if on AVR Uno; increase if enough RAM
  static char buf[BUF_SZ];
  static size_t head = 0; // next write index
  static size_t len = 0;  // current stored length (<= BUF_SZ)
  int codeSM = 0;
  int statusFound = -1;
  /* buffer currently unused
  auto push = [&](char c) {
    if (len < BUF_SZ) {
      size_t writeIdx = (head + len) % BUF_SZ;
      buf[writeIdx] = c;
      len++;
    } else {
      // buffer full: overwrite oldest by moving head forward and writing at tail
      buf[head] = c;
      head = (head + 1) % BUF_SZ;
    }
  };
  */
  /* //unused right now
  auto getAt = [&](size_t i) -> char {
    // i is 0..len-1
    size_t idx = (head + i) % BUF_SZ;
    return buf[idx];
  };
  */
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (modemSS.available()) {
      char c = (char)modemSS.read();
      Serial.print(c);
      //shitty state machine below
      if (c == '2' && codeSM == 0){
        Serial.println('state 1');
        codeSM += 1;
      }else if (c == '0' && codeSM == 1){
        Serial.println('state 2');
        codeSM += 1;
      }else if (c == '0' && codeSM == 2){
        Serial.println('state 3');
        statusFound = 1;
        codeSM = 0;
      }else{
        codeSM = 0;
      }
      //push(c); //buffer currently unused
    }
      delay(1);
  }
  return statusFound; // timeout / not found
}

//── TCP SOCKET  ──────────────────────────────────────────────────────────────────
bool CIPTCP() {
  bool CIPcharFound;
    
  runAT("AT+CIPSHUT");
  runAT("AT+CIPMUX=0");
  runAT("AT+CSTT=\"\""); //(or you already have CGDCONT)
  runAT("AT+CIICR");// (bring up wireless connection)
  runAT("AT+CIFSR");// (get local IP)
  runAT("AT+CIPSTART=\"TCP\",\"example.com\",\"80\"");
  CIPcharFound = runAT("AT+CIPSEND", 5000, '>');
  delay(3000);
  if (CIPcharFound) { 
    Serial.println("[CIP TCP] Sending HTTP GET:");
    Serial.println("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\nUser-Agent: SIM7000\r\nAccept: */*\r\n <CTRL+Z>");
    modemSS.print("GET / HTTP/1.1\r\n") ;       
    modemSS.print("Host: example.com\r\n");
    modemSS.print("Connection: close\r\n");
    modemSS.print("User-Agent: SIM7000\r\n");
    modemSS.print("Accept: */*\r\n");
    modemSS.print("\r\n");
    modemSS.write(0x1A);    // Ctrl+Z terminator
    Serial.println("[CIP TCP] HTTP GET sent");              
  }

  //NEED BETTER LOGIC BELOW
  int httpStatus = readHttpStatusFromModem(15000); // 15s timeout
  if (httpStatus > 0) {
    Serial.print(F("[CIP TCP] HTTP status OK "));
    //Serial.println(httpStatus);
  } else {
    Serial.println(F("[CIP TCP] HTTP status not OK"));
    //Dump raw modem bytes remaining:
    while (modemSS.available()) Serial.write(modemSS.read());
  }

  delay(3000);
  runAT("AT+CIPCLOSE");
  return httpStatus; //temp- this will eventually be an actual code not just a bool if 200 was recv.
}