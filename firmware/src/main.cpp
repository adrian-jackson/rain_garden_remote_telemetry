#include <SoftwareSerial.h>
#include <Arduino.h>
#include <api_key.h>

// Software serial for shield (RX, TX)
SoftwareSerial shieldSerial(10, 11);

#define PWRKEY 6
#define RST    7

// APN, Dest, Password, Etc
// ── User configuration ────────────────────────────────────────────────────────
#define APN              "hologram"          // ← your SIM card APN
#define SERVER_HOST      "rcewrp-backend-23923596738.us-central1.run.app"       // ← no https://, no trailing slash
#define SERVER_PATH      "/admin/sample"                  // ← endpoint path for POST
#define POST_INTERVAL_MS 30000UL             // ← 30 seconds
#define BOTLETICS_SSL 0
// ─────────────────────────────────────────────────────────────────────────────

// ── Demo Values ──────────────────────────────────────────────────────
const char *API_KEY = ADMIN_PANEL_API_KEY;
const char *uniqueSiteId = "test";
int testSiteID = 103;
float temp_current = 1.0;
float inflow_current = 1.0;
float outflow_current = 1.0;
float downflow_current = 1.0;
float humidity_current = 1.0;
float precipitation_current = 1.0;
// ─────────────────────────────────────────────────────────────────────────────

void testBaudRate(long baudrate) {
  Serial.print("Testing ");
  Serial.print(baudrate);
  Serial.print(" baud... ");
  
  shieldSerial.end();
  delay(100);
  shieldSerial.begin(baudrate);
  delay(300);
  
  // Send AT
  shieldSerial.println("AT");
  
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < 1500) {
    if (shieldSerial.available()) {
      char c = shieldSerial.read();
      response += c;
    }
  }

  
  if (response.indexOf("OK") != -1) {
    Serial.println("✓ FOUND!");
    Serial.println("Response: " + response);
  } else {
    Serial.println("✗ No OK");
    if (response.length() > 0) {
      Serial.print("  Received: ");
      Serial.println(response);
    }
  }
}

void attemptConnection(long baudrate) {
  shieldSerial.begin(baudrate);
  delay(500);
  
  // Send AT command and wait for response
  shieldSerial.println("AT");
  
  unsigned long startTime = millis();
  String response = "";
  
  // Wait up to 2 seconds for a response
  while (millis() - startTime < 2000) {
    if (shieldSerial.available()) {
      char c = shieldSerial.read();
      response += c;
      
      // Check if we got an OK
      if (response.indexOf("OK") != -1) {
        Serial.print("✓ Connected at ");
        Serial.print(baudrate);
        Serial.println(" baud!");
        Serial.println("Response: " + response);
        return;
      }
    }
  }
  
  Serial.print("✗ No response at ");
  Serial.print(baudrate);
  Serial.println(" baud");
  if (response.length() > 0) {
    Serial.println("Received: " + response);
  }
  
  shieldSerial.end();
}

void sendCommand(String command, unsigned long timeout) {
  Serial.print("> ");
  Serial.println(command);
  
  shieldSerial.println(command);
  
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (shieldSerial.available()) {
      char c = shieldSerial.read();
      Serial.write(c);
    }
  }
  Serial.println();
}

void sendHTTPPost(const char* url, const char* body) {
    int bodyLen = strlen(body);
    char buf[128];

    sendCommand("AT+HTTPSSL=1", 1000);

    // init HTTP
    sendCommand("AT+HTTPINIT", 1000); //If AT+HTTPACTION=1 returns a non-200 or a connection error dig into SSL config rather than debug the server side.
    sendCommand("AT+HTTPPARA=\"CID\",1", 1000);
    sendCommand("AT+HTTPPARA=\"SSLCFG\",1", 1000);    //use correct SSL config



    snprintf(buf, sizeof(buf), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    sendCommand(buf, 1000);

    sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);

    // Add custom Authorization header        
    snprintf(buf, sizeof(buf), "AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer %s\"", API_KEY);    
    sendCommand(buf, 1000);

    // load body — 10000ms window to input data
    snprintf(buf, sizeof(buf), "AT+HTTPDATA=%d,15000", bodyLen);
    sendCommand(buf, 1000);          // modem responds with DOWNLOAD
    sendCommand(body, 3000);         // send body, wait for OK

    // fire POST and read response
    sendCommand("AT+HTTPACTION=1", 15000);   // 1 = POST
    sendCommand("AT+HTTPREAD", 15000);

    // teardown
    sendCommand("AT+HTTPTERM", 1000);
    sendCommand("AT+SAPBR=0,1", 1000);

}


String buildJSON(int siteID, float humidity, float temp_f, float precipitation, float Qin, float Qout, float Qinf)
{
  String json = "{";
  json += "\"siteId\":" + String(siteID) + ",";
  json += "\"temp_f\":" + String(temp_f, 2) + ",";
  json += "\"inflow\":" + String(Qin, 6) + ",";
  json += "\"outflow\":" + String(Qout, 6) + ",";
  json += "\"downflow\":" + String(Qinf, 6) + ",";
  json += "\"humidity\":" + String(humidity, 2) + ",";
  json += "\"precipitation\":" + String(precipitation, 4);
  json += "}";

  return json;
}

void setup() {
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, LOW);
  Serial.begin(9600);   // USB serial for debugging
  delay(1000);
  digitalWrite(PWRKEY, HIGH);
  
  Serial.println("\n=== SIM7000 Baud Rate Scanner ===\n");
  
  long baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400};

  int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
  if(true){
    for (int i = 0; i < numRates; i++) {
      testBaudRate(baudRates[i]);
      delay(500);
    }
    Serial.println("\n=== Scan Complete ===\n");
  }

  if (false){
    shieldSerial.begin(9600);
  }
  
  Serial.println("\n=== Setup Complete ===\n");
  shieldSerial.begin(115200);
  Serial.println(F("[MODEM] Configuring baud rate..."));
  shieldSerial.println("AT+IPR=9600");
  delay(100);
  shieldSerial.begin(9600);

  // debug
  sendCommand("AT+CGMR", 1000); // check firmware version
  sendCommand("AT+CSSLCFG=\"sslversion\",1,3", 1000); // set SSL version to TLS 1.2

  //config
  sendCommand("AT", 1000);
    lastCheck = millis();
    //set func
    sendCommand("AT+GSN", 1000);
    sendCommand("AT+CFUN=1", 1000);

    // --- TEARDOWN (ignore errors on all of these) ---
    sendCommand("AT+HTTPTERM", 1000);
    sendCommand("AT+SAPBR=0,1", 1000);
    sendCommand("AT+CGATT=0", 3000);

    //enable GPRS
    sendCommand("AT+CGATT=1", 5000);
    sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",1000);
    sendCommand("AT+SAPBR=3,1,\"APN\",\"hologram\"",1000);
    sendCommand("AT+SAPBR=1,1",10000);
    
    //query network registration
    sendCommand("AT+CREG?", 1000);   // GSM registration - need +CREG: 0,1 or 0,5
    sendCommand("AT+CGREG?", 1000); // GPRS registration - need +CGREG: 0,1 or 0,5

    sendCommand("AT+SAPBR=2,1",1000);

    //check signal strength
    sendCommand("AT+CSQ",1000);
}

void loop() {
  // Keep modem connection alive and monitor serial
  if (shieldSerial.available()) {
    String response = shieldSerial.readStringUntil('\n');
    Serial.print("< ");
    Serial.println(response);
  }
  
  // Send AT every 5 seconds to keep connection active
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > POST_INTERVAL_MS) {
    

    //HTTP STACK
    //demo to verify func
    if(false){
      sendCommand("AT+HTTPINIT", 1000);  
      sendCommand("AT+HTTPPARA=\"CID\",1", 1000);
      sendCommand("AT+HTTPPARA=\"URL\",\"http://httpbin.org/get\"", 1000);
      sendCommand("AT+HTTPACTION=0", 1000); //0=GET, 1=POST, 2=HEAD
      //; wait for +HTTPACTION: 0,200,<len>
      sendCommand("AT+HTTPREAD", 1000);
      sendCommand("AT+HTTPTERM", 1000);
      sendCommand("AT+SAPBR=0,1", 1000);
    }

    //test POST
    // sendHTTPPost("http://httpbin.org/post", "{\"test\":\"data\",\"key\":\"value\"}");
    String json = buildJSON(
      testSiteID,
      humidity_current,
      temp_current,
      precipitation_current,
      inflow_current,
      outflow_current,
      downflow_current
    );

    sendHTTPPost("http://rcewrp-backend-23923596738.us-central1.run.app/upload", json.c_str());


  }
}

