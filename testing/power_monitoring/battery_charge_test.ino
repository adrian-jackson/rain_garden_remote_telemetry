int POS_PIN = 1;
int NEG_PIN = 2;

//note down starting time of program!
//assuming voltage divider for positive terminal with 1k and 10kOhm resistor

void setup() {
  pinMode(POS_PIN, INPUT);
  pinMode(NEG_PIN, INPUT);
}

void loop() {
  raw_pos = analogRead(POS_PIN);
  raw_neg = analogRead(NEG_PIN);
  voltage = raw_pos * 11 - raw_neg
  Serial.println(voltage);

  delay(600000) //log every 10 mins
}