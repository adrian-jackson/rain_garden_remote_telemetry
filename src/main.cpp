#include <Arduino.h>
#include "BotleticsSIM7000.h" 
#include <SoftwareSerial.h>

//SIM7000 modem setup//

#define SIMCOM_7000

#define BOTLETICS_PWRKEY 6
#define RST 7
//#define DTR 8 // Connect with solder jumper
//#define RI 9 // Need to enable via AT commands
#define TX 10 // Microcontroller RX
#define RX 11 // Microcontroller TX

SoftwareSerial modemSS = SoftwareSerial(TX, RX);
SoftwareSerial *modemSerial = &modemSS;
#if defined(SIMCOM_7000) || defined(SIMCOM_7070) || defined(SIMCOM_7500) || defined(SIMCOM_7600)
  Botletics_modem_LTE modem = Botletics_modem_LTE();
#endif

char replybuffer[255];
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
uint8_t type;
char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!

bool GPS = false; //set to true if GPS is needed

//battery monitoring
int POS_BATT_PIN = A1;
int NEG_BATT_PIN = A2;

//general
bool serial_up = false;

//note down starting time of program!
//assuming voltage divider for positive terminal with 1k and 10kOhm resistor

int readVoltage() {
    int r1k_pos = analogRead(POS_BATT_PIN);
    int r1k_neg = analogRead(NEG_BATT_PIN);
    return 11 * (5.0/1024.0) * (r1k_pos - r1k_neg);
}

void testSerial() {
    Serial.println("test!");
}

void printSim7000Menu(void) {
  Serial.println(F("-------------------------------------"));
  // General
  Serial.println(F("[?] Print this menu"));
  Serial.println(F("[a] Read the ADC; 2.8V max for SIM800/808, 0V-VBAT for SIM7000 shield"));
  Serial.println(F("[b] Read supply voltage")); // Will also give battery % charged for most modules
  Serial.println(F("[C] Read the SIM CCID"));
  Serial.println(F("[U] Unlock SIM with PIN code"));
  Serial.println(F("[i] Read signal strength (RSSI)"));
  Serial.println(F("[n] Get network status"));
  Serial.println(F("[1] Get network connection info")); // See what connection type and band you're on!

  // Calling
  Serial.println(F("[c] Make phone Call"));
  Serial.println(F("[A] Get call status"));
  Serial.println(F("[h] Hang up phone"));
  Serial.println(F("[p] Pick up phone"));

  // SMS
  Serial.println(F("[N] Number of SMS's"));
  Serial.println(F("[r] Read SMS #"));
  Serial.println(F("[R] Read all SMS"));
  Serial.println(F("[d] Delete SMS #"));
  Serial.println(F("[D] Delete all SMS"));
  Serial.println(F("[s] Send SMS"));
  Serial.println(F("[u] Send USSD"));
  
  // Time
  Serial.println(F("[y] Enable local time stamp (SIM800/808/70X0)"));
  Serial.println(F("[Y] Enable NTP time sync (SIM800/808/70X0)")); // Need to use "G" command first!
  Serial.println(F("[t] Get network time")); // Works just by being connected to network

  // Data Connection
  Serial.println(F("[G] Enable cellular data"));
  Serial.println(F("[g] Disable cellular data"));
  Serial.println(F("[l] Query GSMLOC (2G)"));
#if !defined(SIMCOM_3G) && !defined(SIMCOM_7500) && !defined(SIMCOM_7600)
  Serial.println(F("[w] Read webpage"));
  Serial.println(F("[W] Post to website"));
#endif
  // The following option below posts dummy data to dweet.io for demonstration purposes. See the
  // IoT_example sketch for an actual application of this function!
  // NOTE: dweet.io shut down in early 2025 and is no longer used as an example
  // To use the Adafruit IO example, please go to the code and input your own Adafruit IO username, key, and feed key
  Serial.println(F("[2] Post to Adafruit IO - 2G / LTE CAT-M / NB-IoT")); // SIM800/808/900/7000/7070
  Serial.println(F("[3] Post to Adafruit IO - 3G / 4G LTE")); // SIM5320/7500/7600

 
  Serial.println(F("-------------------------------------"));
  Serial.println(F(""));
}


void setupSim7000(){
  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH); // Default state

  pinMode(BOTLETICS_PWRKEY, OUTPUT);

  // Turn on the module by pulsing PWRKEY low for a little bit
  // This amount of time depends on the specific module that's used
  modem.powerOn(BOTLETICS_PWRKEY); // Power on the module

  Serial.begin(9600);
  Serial.println(F("Modem basic test"));
  Serial.println(F("Initializing....(May take several seconds)"));

  // SIM7000 takes about 3s to turn on and SIM7500 takes about 15s
  // Press Arduino reset button if the module is still turning on and the board doesn't find it.
  // When the module is on it should communicate right after pressing reset

  // Software serial:
  modemSS.begin(115200); // Default SIM7000 shield baud rate

  Serial.println(F("Configuring to 9600 baud"));
  modemSS.println("AT+IPR=9600"); // Set baud rate
  delay(100); // Short pause to let the command run
  modemSS.begin(9600);
  if (! modem.begin(modemSS)) {
    Serial.println(F("Couldn't find modem"));
    while (1); // Don't proceed if it couldn't find the device
  }
  type = modem.type();
  Serial.println(F("Modem is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case SIM7000:
      Serial.println(F("SIM7000")); break;
    default:
      Serial.println(F("???")); break;
  }

  // Print module IMEI number
  uint8_t imeiLen = modem.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Set modem to full functionality
  modem.setFunctionality(1); // AT+CFUN=1
  modem.setNetworkSettings(F("hologram")); // For Hologram SIM card

  printSim7000Menu();
}


//helpers
void flushSerial() {
  while (Serial.available())
    Serial.read();
}

char readBlocking() {
  while (!Serial.available());
  return Serial.read();
}
uint16_t readnumber() {
  uint16_t x = 0;
  char c;
  while (! isdigit(c = readBlocking())) {
    //Serial.print(c);
  }
  Serial.print(c);
  x = c - '0';
  while (isdigit(c = readBlocking())) {
    Serial.print(c);
    x *= 10;
    x += c - '0';
  }
  return x;
}

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout) {
  uint16_t buffidx = 0;
  boolean timeoutvalid = true;
  if (timeout == 0) timeoutvalid = false;

  while (true) {
    if (buffidx > maxbuff) {
      //Serial.println(F("SPACE"));
      break;
    }

    while (Serial.available()) {
      char c =  Serial.read();

      //Serial.print(c, HEX); Serial.print("#"); Serial.println(c);

      if (c == '\r') continue;
      if (c == 0xA) {
        if (buffidx == 0)   // the first 0x0A is ignored
          continue;

        timeout = 0;         // the second 0x0A is the end of the line
        timeoutvalid = true;
        break;
      }
      buff[buffidx] = c;
      buffidx++;
    }

    if (timeoutvalid && timeout == 0) {
      //Serial.println(F("TIMEOUT"));
      break;
    }
    delay(1);
  }
  buff[buffidx] = 0;  // null term
  return buffidx;
}


void SIM7000loop() {
    Serial.print(F("Modem> "));
  while (! Serial.available() ) {
    if (modem.available()) {
      Serial.write(modem.read());
    }
  }

  char command = Serial.read();
  Serial.println(command);


  switch (command) {
    case '?': {
        printSim7000Menu();
        break;
      }

    case 'a': {
        // read the ADC
        uint16_t adc;
        if (! modem.getADCVoltage(&adc)) {
          Serial.println(F("Failed to read ADC"));
        } else {
          Serial.print(F("ADC = ")); Serial.print(adc); Serial.println(F(" mV"));
        }
        break;
      }

    case 'b': {
        // read the battery voltage and percentage
        uint16_t vbat;
        if (! modem.getBattVoltage(&vbat)) {
          Serial.println(F("Failed to read Batt"));
        } else {
          Serial.print(F("VBat = ")); Serial.print(vbat); Serial.println(F(" mV"));
        }

        if (type != SIM7500 && type != SIM7600) {
          if (! modem.getBattPercent(&vbat)) {
            Serial.println(F("Failed to read Batt"));
          } else {
            Serial.print(F("VPct = ")); Serial.print(vbat); Serial.println(F("%"));
          }
        }

        break;
      }

    case 'U': {
        // Unlock the SIM with a PIN code
        char PIN[5];
        flushSerial();
        Serial.println(F("Enter 4-digit PIN"));
        readline(PIN, 3);
        Serial.println(PIN);
        Serial.print(F("Unlocking SIM card: "));
        if (! modem.unlockSIM(PIN)) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("OK!"));
        }
        break;
      }

    case 'C': {
        // read the CCID
        modem.getSIMCCID(replybuffer);  // make sure replybuffer is at least 21 bytes!
        Serial.print(F("SIM CCID = ")); Serial.println(replybuffer);
        break;
      }

    case 'i': {
        // read the RSSI
        uint8_t n = modem.getRSSI();
        int8_t r;

        Serial.print(F("RSSI = ")); Serial.print(n); Serial.print(": ");
        if (n == 0) r = -115;
        if (n == 1) r = -111;
        if (n == 31) r = -52;
        if ((n >= 2) && (n <= 30)) {
          r = map(n, 2, 30, -110, -54);
        }
        Serial.print(r); Serial.println(F(" dBm"));

        break;
      }

    case 'n': {
        // read the network/cellular status
        uint8_t n = modem.getNetworkStatus();
        Serial.print(F("Network status "));
        Serial.print(n);
        Serial.print(F(": "));
        if (n == 0) Serial.println(F("Not registered"));
        if (n == 1) Serial.println(F("Registered (home)"));
        if (n == 2) Serial.println(F("Not registered (searching)"));
        if (n == 3) Serial.println(F("Denied"));
        if (n == 4) Serial.println(F("Unknown"));
        if (n == 5) Serial.println(F("Registered roaming"));
        break;
      }
    case '1': {
        // Get connection type, cellular band, carrier name, etc.
        modem.getNetworkInfo();        
        break;
      }
    
    /*** Calling ***/
    case 'c': {
        // call a phone!
        char number[30];
        flushSerial();
        Serial.print(F("Call #"));
        readline(number, 30);
        Serial.println();
        Serial.print(F("Calling ")); Serial.println(number);
        if (!modem.callPhone(number)) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("Sent!"));
        }

        break;
      }
    case 'A': {
        // get call status
        int8_t callstat = modem.getCallStatus();
        switch (callstat) {
          case 0: Serial.println(F("Ready")); break;
          case 1: Serial.println(F("Could not get status")); break;
          case 3: Serial.println(F("Ringing (incoming)")); break;
          case 4: Serial.println(F("Ringing/in progress (outgoing)")); break;
          default: Serial.println(F("Unknown")); break;
        }
        break;
      }
      
    case 'h': {
        // hang up!
        if (! modem.hangUp()) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("OK!"));
        }
        break;
      }

    case 'p': {
        // pick up!
        if (! modem.pickUp()) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("OK!"));
        }
        break;
      }

      case 'N': {
        // read the number of SMS's!
        int8_t smsnum = modem.getNumSMS();
        if (smsnum < 0) {
          Serial.println(F("Could not read # SMS"));
        } else {
          Serial.print(smsnum);
          Serial.println(F(" SMS's on SIM card!"));
        }
        break;
      }
    case 'r': {
        // read an SMS
        flushSerial();
        Serial.print(F("Read #"));
        uint8_t smsn = readnumber();
        Serial.print(F("\n\rReading SMS #")); Serial.println(smsn);

        // Retrieve SMS sender address/phone number.
        if (! modem.getSMSSender(smsn, replybuffer, 250)) {
          Serial.println("Failed!");
          break;
        }
        Serial.print(F("FROM: ")); Serial.println(replybuffer);

        // Retrieve SMS value.
        uint16_t smslen;
        if (! modem.readSMS(smsn, replybuffer, 250, &smslen)) { // pass in buffer and max len!
          Serial.println("Failed!");
          break;
        }
        Serial.print(F("***** SMS #")); Serial.print(smsn);
        Serial.print(" ("); Serial.print(smslen); Serial.println(F(") bytes *****"));
        Serial.println(replybuffer);
        Serial.println(F("*****"));

        break;
      }
    case 'R': {
        // read all SMS
        int8_t smsnum = modem.getNumSMS();
        uint16_t smslen;
        int8_t smsn;

        if ( (type == SIM5320A) || (type == SIM5320E) || (type == SIM7000) || (type == SIM7070)) {
          smsn = 0; // zero indexed
          smsnum--;
        } else {
          smsn = 1;  // 1 indexed
        }

        for ( ; smsn <= smsnum; smsn++) {
          Serial.print(F("\n\rReading SMS #")); Serial.println(smsn);
          if (!modem.readSMS(smsn, replybuffer, 250, &smslen)) {  // pass in buffer and max len!
            Serial.println(F("Failed!"));
            break;
          }
          // if the length is zero, its a special case where the index number is higher
          // so increase the max we'll look at!
          if (smslen == 0) {
            Serial.println(F("[empty slot]"));
            smsnum++;
            continue;
          }

          Serial.print(F("***** SMS #")); Serial.print(smsn);
          Serial.print(" ("); Serial.print(smslen); Serial.println(F(") bytes *****"));
          Serial.println(replybuffer);
          Serial.println(F("*****"));
        }
        break;
      }

    case 'd': {
        // Delete an SMS
        flushSerial();
        Serial.print(F("Delete #"));
        uint8_t smsn = readnumber();

        Serial.print(F("\n\rDeleting SMS #")); Serial.println(smsn);
        if (modem.deleteSMS(smsn)) {
          Serial.println(F("OK!"));
        } else {
          Serial.println(F("Couldn't delete"));
        }
        break;
      }

    case 'D': {
        // Delete all SMS
        flushSerial();
        Serial.println(F("\n\rDeleting all SMS"));
        if (modem.deleteAllSMS()) {
          Serial.println(F("OK!"));
        } else {
          Serial.println(F("Couldn't delete"));
        }
        break;
      }

    case 's': {
        // send an SMS!
        char sendto[21], message[141];
        flushSerial();
        Serial.print(F("Send to #"));
        readline(sendto, 20);
        Serial.println(sendto);
        Serial.print(F("Type out one-line message (140 char): "));
        readline(message, 140);
        Serial.println(message);
        if (!modem.sendSMS(sendto, message)) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("Sent!"));
        }

        break;
      }

    case 'u': {
      // send a USSD!
      char message[141];
      flushSerial();
      Serial.print(F("Type out one-line message (140 char): "));
      readline(message, 140);
      Serial.println(message);

      uint16_t ussdlen;
      if (!modem.sendUSSD(message, replybuffer, 250, &ussdlen)) { // pass in buffer and max len!
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
        Serial.print(F("***** USSD Reply"));
        Serial.print(" ("); Serial.print(ussdlen); Serial.println(F(") bytes *****"));
        Serial.println(replybuffer);
        Serial.println(F("*****"));
      }
    }

    /*** Time ***/

    case 'y': {
        // enable network time sync
        if (!modem.enableRTC(true))
          Serial.println(F("Failed to enable"));
        break;
      }

    case 'Y': {
        // enable NTP time sync
        if (!modem.enableNTPTimeSync(true, F("pool.ntp.org")))
          Serial.println(F("Failed to enable"));
        break;
      }

    case 't': {
        // read the time
        char buffer[23];

        modem.getTime(buffer, 23);  // make sure replybuffer is at least 23 bytes!
        Serial.print(F("Time = ")); Serial.println(buffer);
        break;
      }


    case 'g': {
        // disable data
        if (!modem.enableGPRS(false))
          Serial.println(F("Failed to turn off"));
        break;
      }
    case 'G': {
        // turn GPRS off first for SIM7500
        #if defined(SIMCOM_7500) || defined (SIMCOM_7600)
          modem.enableGPRS(false);
        #endif
        
        // enable data
        if (!modem.enableGPRS(true))
            Serial.println(F("Failed to turn on"));
        break;
      }
    case 'l': {
        // check for GSMLOC (requires GPRS)
        uint16_t returncode;

        if (!modem.getGSMLoc(&returncode, replybuffer, 250))
          Serial.println(F("Failed!"));
        if (returncode == 0) {
          Serial.println(replybuffer);
        } else {
          Serial.print(F("Fail code #")); Serial.println(returncode);
        }

        break;
      }

    // The code below was written by Adafruit and only works on some modules
    case 'w': {
        // read website URL
        uint16_t statuscode;
        int16_t length;
        char url[80];

        flushSerial();
        Serial.println(F("URL to read (e.g. dweet.io/get/latest/dweet/for/sim7500test123):"));
        Serial.print(F("http://")); readline(url, 79);
        Serial.println(url);

        Serial.println(F("****"));
        if (!modem.HTTP_GET_start(url, &statuscode, (uint16_t *)&length)) {
          Serial.println("Failed!");
          break;
        }
        while (length > 0) {
          while (modem.available()) {
            char c = modem.read();

            // Serial.write is too slow, we'll write directly to Serial register!
            #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
                        loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
                        UDR0 = c;
            #else
                        Serial.write(c);
            #endif
            length--;
            if (! length) break;
          }
        }
        Serial.println(F("\n****"));
        modem.HTTP_GET_end();
        break;
      }

    case 'W': {
        // Post data to website
        uint16_t statuscode;
        int16_t length;
        char url[80];
        char data[80];

        flushSerial();
        Serial.println(F("NOTE: in beta! Use simple websites to post!"));
        Serial.println(F("URL to post (e.g. httpbin.org/post):"));
        Serial.print(F("http://")); readline(url, 79);
        Serial.println(url);
        Serial.println(F("Data to post (e.g. \"foo\" or \"{\"simple\":\"json\"}\"):"));
        readline(data, 79);
        Serial.println(data);

        Serial.println(F("****"));
        if (!modem.HTTP_POST_start(url, F("text/plain"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
          Serial.println("Failed!");
          break;
        }
        while (length > 0) {
          while (modem.available()) {
            char c = modem.read();

            #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
                        loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
                        UDR0 = c;
            #else
                        Serial.write(c);
            #endif

            length--;
            if (! length) break;
          }
        }
        Serial.println(F("\n****"));
        modem.HTTP_POST_end();
        break;
      }

    case '2': {
        // Post data to website via 2G or LTE CAT-M/NB-IoT
        
        float temperature = analogRead(A0)*1.23; // Change this to suit your needs
        
        uint16_t battLevel;
        if (! modem.getBattVoltage(&battLevel)) battLevel = 3800; // Use dummy voltage if can't read

        // Create char buffers for the floating point numbers for sprintf
        // Make sure these buffers are long enough for your request URL
        char URL[150];
        char body[100];
        char tempBuff[16];
      
        // Format the floating point numbers as needed
        dtostrf(temperature, 1, 2, tempBuff); // float_val, min_width, digits_after_decimal, char_buffer

        // #ifdef SIMCOM_7070 // Use this line if you have the SIM7000G because the 1529B01SIM7000G firmware doesn't seem to run the commands below well
        #if defined(SIMCOM_7000) || defined(SIMCOM_7070) // Use this if you have SIM7000A, especially with SSL
            // Add headers as needed
            // modem.HTTP_addHeader("User-Agent", "SIM7070", 7);
            // modem.HTTP_addHeader("Cache-control", "no-cache", 8);
            // modem.HTTP_addHeader("Connection", "keep-alive", 10);
            // modem.HTTP_addHeader("Accept", "*/*, 3);
            
            // Connect to server
            // If https:// is used, #define BOTLETICS_SSL 1 in Botletics_modem.h
            if (! modem.HTTP_connect("http://io.adafruit.com")) {
              Serial.println(F("Failed to connect to server..."));
              break;
            }

            // POST request (to create data on the feed)
            // Insert your AIO username and feed key: "/api/v2/{username}/feeds/{feed_key}/data"
            strcpy(URL, "/api/v2/username/feeds/sim7000/data");

            modem.HTTP_addHeader("x-aio-key", "450a3ef9db96445dac2ffe22893c4939", 33); // Adafruit IO authentication key
            modem.HTTP_addHeader("Content-Type", "application/x-www-form-urlencoded", 34);
            modem.HTTP_addPara("value", tempBuff, strlen(tempBuff)); // Test value
            modem.HTTP_POST(URL, body, strlen(body));
                        
            // GET request (to get the last data point that was posted)
            /*
            // Format URI with GET request query string
            // Format: "/api/v2/{username}/feeds/{feed_key}/data/last"
            strcpy(URL, "/api/v2/username/feeds/sim7000/data/last"); // Get only the last data point
            modem.HTTP_addHeader("x-aio-key", "450a3ef9db96445dac2ffe22893c4939", 33); // Adafruit IO authentication key
            modem.HTTP_GET(URL); // Read the last data point from your feed on Adafruit IO
            */                     
        #endif

        break;
        }

        case 'S': {
        Serial.println(F("Creating SERIAL TUBE"));
        while (1) {
          while (Serial.available()) {
            delay(1);
            modem.write(Serial.read());
          }
          if (modem.available()) {
            Serial.write(modem.read());
          }
        }
        break;
      }

    default: {
        Serial.println(F("Unknown command"));
        printSim7000Menu();
        break;
        }
  }
  // flush input
  flushSerial();
  while (modem.available()) {
    Serial.write(modem.read());
  }

 }



void setup() {
    //battery monitoring pins
    pinMode(POS_BATT_PIN, INPUT);
    pinMode(NEG_BATT_PIN, INPUT);

    //SIM7000 modem setup
    setupSim7000();

    //general
    if(not serial_up){
    Serial.begin(9600);
    }
}

void loop() {
  //test SIM7000
  SIM7000loop();

  delay(30000); // run every 30s
}

