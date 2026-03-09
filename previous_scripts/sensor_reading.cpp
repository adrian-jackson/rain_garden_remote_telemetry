/*

SECTION A — SENSOR READING (Rain Garden)

Outputs per window (exact names you'll upload later):

humidity, temp_f, precipitation, inflow, outflow, downflow

Hardware:

- SHT31 (I2C) weatherproof sensor

- Tipping bucket rain gauge (reed switch -> interrupt)

- Grove 10 cm water level sensor, 20 sections (I2C @ 0x78, 0x77)

- Two 0.5–4.5 V pressure transducers (0–30 psi), bottom + atmosphere

Notes:

- Pipe is 4" corrugated HDPE; blending Manning with orifice when near/full.

- Keep this file focused on sensing + hydraulics. No networking here.

*/

#include <Wire.h>

#include <Adafruit_SHT31.h>

// ----------------------- CONFIG (EDIT PER SITE) -----------------------

// General

const float VREF = 5.000; // UNO ADC reference (measure your 5V if you can)

const float ADC_RES = 1023.0;

// Rain gauge windowing

unsigned long WINDOW_SECONDS = 600; // default 10 minutes. Change as needed.

float TIP_SIZE_IN = 0.0100; // inches per tip (change per site)

float DRAINAGE_AREA_AC = 0.25; // acres (change per site)

// Pipe & hydraulics

const float PIPE_DIAM_IN = 4.0; // KEEP at 4.0 for these A/WP formulas

float N_MANNING = 0.021; // corrugated HDPE

float SLOPE_S = 0.010; // ft/ft (1% default; change per site)

// Orifice fallback (pressurized/full pipe)

float CD_ORIFICE = 0.65; // discharge coeff (tune if you can)

const float G_FTPS2 = 32.174; // gravity

// Water level (Grove 10cm, 20 sections)

const uint8_t WL_ADDR_LOW = 0x78;

const uint8_t WL_ADDR_HIGH = 0x77;

const float MAX_HEIGHT_IN = 3.937f; // 10 cm in inches

const int TOTAL_SECTIONS = 20;

// Pins

const uint8_t PIN_RAIN = 2; // interrupt pin for reed switch (UNO: 2 or 3)

const uint8_t PIN_PSI_BOTTOM = A0;

const uint8_t PIN_PSI_AIR = A1;

// ----------------------- STATE -----------------------

Adafruit_SHT31 sht31;

volatile unsigned long tipCount = 0;

volatile unsigned long lastTipMicros = 0;

const unsigned long TIP_DEBOUNCE_US = 250000UL; // 250 ms debounce

unsigned long windowStartMs = 0;

// ----------------------- UTILS -----------------------

static inline float clampf(float x, float lo, float hi) {

if (x < lo) return lo;

if (x > hi) return hi;

return x;

}

static inline float safe_acos(float x) {

return acosf(clampf(x, -1.0f, 1.0f));

}

// ----------------------- RAIN ISR -----------------------

void rainISR() {

unsigned long now = micros();

if (now - lastTipMicros > TIP_DEBOUNCE_US) {

tipCount++;

lastTipMicros = now;

}

}

// ----------------------- I2C helper for water level -----------------------

uint8_t readSectionByte(uint8_t addr) {

Wire.beginTransmission(addr);

Wire.write(0x00); // section count register (per Seeed examples)

if (Wire.endTransmission(false) != 0) return 0;

if (Wire.requestFrom(addr, (uint8_t)1) != 1) return 0;

return Wire.read();

}

// ----------------------- SENSOR READS -----------------------

float readTempF(float &humidity) {

float tC = sht31.readTemperature();

humidity = sht31.readHumidity();

// Handle occasional NaNs:

if (isnan(tC)) tC = 0.0;

if (isnan(humidity)) humidity = 0.0;

return tC * 9.0f/5.0f + 32.0f;

}

float readWaterLevelY_inches() {

uint8_t lowSec = readSectionByte(WL_ADDR_LOW);

uint8_t highSec = readSectionByte(WL_ADDR_HIGH);

int active = (int)lowSec + (int)highSec;

active = (int)clampf(active, 0, TOTAL_SECTIONS);

float pct = (float)active / (float)TOTAL_SECTIONS;

float y_in = pct * MAX_HEIGHT_IN;

// cannot exceed diameter

return clampf(y_in, 0.0f, PIPE_DIAM_IN);

}

float readVoltage(uint8_t analogPin) {

int raw = analogRead(analogPin);

return (raw * VREF / ADC_RES);

}

float psiFromVoltage_0to30(float v) {

// calibration: 0.5V->0 psi, 4.5V->30 psi

// slope = 30 / (4.5 - 0.5) = 7.5 psi/V

float psi = (v - 0.5f) * 7.5f;

if (psi < 0) psi = 0;

if (psi > 30) psi = 30;

return psi;

}

float headFeetFromTwoTransducers(uint8_t pinBottom, uint8_t pinAir) {

float vB = readVoltage(pinBottom);

float vA = readVoltage(pinAir);

float psiB = psiFromVoltage_0to30(vB);

float psiA = psiFromVoltage_0to30(vA);

float dPsi = psiB - psiA;

// 1 ft H2O ≈ 0.433 psi

return (dPsi / 0.433f);

}

// ----------------------- HYDRAULICS -----------------------

// NOTE: The A(y) and WP(y) formulas provided by you are specific to R = 2 in (D=4 in).

// Units: y in inches; A returned in ft^2; WP returned in ft.

float area_ft2_from_y_in(float y) {

y = clampf(y, 0.0f, PIPE_DIAM_IN);

// A = [4*acos((2-y)/2) - (2-y)*sqrt(4y - y^2)] / 144

float term = (2.0f - y);

float inside = (4.0f * y - y * y);

if (inside < 0) inside = 0;

float A_in2 = 4.0f * safe_acos(term / 2.0f) - term * sqrtf(inside);

return (A_in2 / 144.0f);

}

float wettedPerimeter_ft_from_y_in(float y) {

y = clampf(y, 0.0f, PIPE_DIAM_IN);

// WP = (acos((2-y)/2)) / 3

float WP_ft = safe_acos((2.0f - y) / 2.0f) / 3.0f;

if (!isfinite(WP_ft) || WP_ft < 0) WP_ft = 0.0f;

return WP_ft;

}

float Q_manning_cfs(float y_in) {

float A = area_ft2_from_y_in(y_in);

float WP = wettedPerimeter_ft_from_y_in(y_in);

if (A <= 0 || WP <= 0 || SLOPE_S <= 0 || N_MANNING <= 0) return 0.0f;

float R = A / WP;

return (1.49f / N_MANNING) * A * powf(R, 2.0f/3.0f) * sqrtf(SLOPE_S);

}

float Q_orifice_cfs(float head_ft) {

if (head_ft <= 0) return 0.0f;

float D_ft = PIPE_DIAM_IN / 12.0f;

float A_full = 3.1415926535f * D_ft * D_ft / 4.0f;

return CD_ORIFICE * A_full * sqrtf(2.0f * G_FTPS2 * head_ft);

}

// Blend: Manning when <95% full; Orifice when >=100% full; linear blend in between.

float Qout_blended_cfs(float y_in, float head_ft) {

float D = PIPE_DIAM_IN;

float y95 = 0.95f * D;

if (y_in < y95) {

return Q_manning_cfs(y_in);

} else if (y_in >= D) {

return Q_orifice_cfs(head_ft);

} else {

float qM = Q_manning_cfs(y_in);

float qO = Q_orifice_cfs(head_ft);

float t = (y_in - y95) / (D - y95); // 0..1

t = clampf(t, 0.0f, 1.0f);

return qM * (1.0f - t) + qO * t;

}

}

// Qin from rainfall over the window

float Qin_from_rain_cfs(float precip_in_over_window) {

// 1 acre-inch = 3630 ft^3

float vol_cuft = precip_in_over_window * DRAINAGE_AREA_AC * 3630.0f;

return vol_cuft / (float)WINDOW_SECONDS;

}

// ----------------------- SETUP/LOOP -----------------------

void setup() {

Serial.begin(115200);

Wire.begin();

// SHT31 init: try 0x44 then 0x45

if (!sht31.begin(0x44)) {

if (!sht31.begin(0x45)) {

Serial.println(F("SHT31 not found! (0x44/0x45)"));

}

} else {

Serial.println(F("SHT31 ready"));

}

pinMode(PIN_RAIN, INPUT_PULLUP);

attachInterrupt(digitalPinToInterrupt(PIN_RAIN), rainISR, FALLING);

windowStartMs = millis();

Serial.println(F("Section A initialized."));

}

void loop() {

// 1) Read temp/humidity

float humidity = 0.0f;

float temp_f = readTempF(humidity);

// 2) Read water level -> y (in)

float y_in = readWaterLevelY_inches();

// 3) Read pressure head (ft)

float head_ft = headFeetFromTwoTransducers(PIN_PSI_BOTTOM, PIN_PSI_AIR);

// 4) Compute Qout (blended)

float Qout = Qout_blended_cfs(y_in, head_ft);

// 5) Windowing for rain/Qin/Qinf

float precipitation = 0.0f; // inches over window

float Qin = 0.0f;

float Qinf = 0.0f;

unsigned long now = millis();

if (now - windowStartMs >= WINDOW_SECONDS * 1000UL) {

noInterrupts();

unsigned long tips = tipCount;

tipCount = 0;

interrupts();

precipitation = tips * TIP_SIZE_IN; // inches in last window

Qin = Qin_from_rain_cfs(precipitation); // cfs from rain

Qinf = Qin - Qout; // cfs

If (Qinf < 0) Qinf = 0; // clamp infiltration to zero

// ---------------- OUTPUT LINE (your six fields) ----------------

// Order: humidity, temp_f, precipitation, inflow(Qin), outflow(Qout), downflow(Qinf)

Serial.print(F("OUT,"));

Serial.print(humidity, 2); Serial.print(',');

Serial.print(temp_f, 2); Serial.print(',');

Serial.print(precipitation, 4); Serial.print(',');

Serial.print(Qin, 6); Serial.print(',');

Serial.print(Qout, 6); Serial.print(',');

Serial.println(Qinf, 6);

// Optional debug

Serial.print(F("DBG y_in=")); Serial.print(y_in, 3);

Serial.print(F(" in, head_ft=")); Serial.print(head_ft, 3);

Serial.print(F(", tips=")); Serial.print(tips);

Serial.print(F(", WP_SLOPE=")); Serial.print(SLOPE_S, 4);

Serial.print(F(", N=")); Serial.print(N_MANNING, 3);

Serial.print(F(", Cd=")); Serial.println(CD_ORIFICE, 2);

windowStartMs = now; // reset window

}

delay(250); // light polling; window timing handles reporting cadence

}