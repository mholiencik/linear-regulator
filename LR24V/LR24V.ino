#include <LiquidCrystal.h>

// Define rotary encoder pins
#define ENC_A 2
#define ENC_B 3
#define PUSHB 4

unsigned long _lastIncReadTime = micros(); 
unsigned long _lastDecReadTime = micros(); 
int _pauseLength = 25000;
int _fastIncrement = 50;

volatile int value = 0;
static int lastValue = 0;
int previousPush = 1;

bool turnedON = true;

const int rs = 12, en = 11, d4 = 8, d5 = 7, d6 = 6, d7 = 5;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

float targetVoltage = 0.0;
float measuredVoltage = 0.0;

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  lcd.begin(16, 4);
  lcd.print("Linear regulator");
  analogWriteResolution(10);
  
  pinMode(A1, INPUT);

  // rotary encoder setup
  pinMode(PUSHB, INPUT_PULLUP);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);
}


void loop() {
  // Check if there's incoming serial data
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    if (command.startsWith("SET:")) {
      // Extract voltage value from command
      float requestedVoltage = command.substring(4).toFloat();
      // Convert voltage to DAC value (0-1023)
      value = (requestedVoltage / 6.6) * (1023.0/3.3);
      if (value > 1023) value = 1023;
      if (value < 0) value = 0;
      
    
      targetVoltage = requestedVoltage;
      value = (targetVoltage/6.6)*(1023.0/3.3);

      if (turnedON) {
        analogWrite(DAC0, value);
      }
    }
    else if (command == "POWER:TOGGLE") {
      togglePower();
    }
  }

  // Regular voltage measurement and update
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 200) { // Update 5 times a second
    measureVoltage();
    sendVoltageData();
    lastUpdate = millis();
  }


  int buttonState = digitalRead(PUSHB);
  if (previousPush > buttonState) {
    togglePower();
  }
  previousPush = buttonState;

  // If voltage has changed, print the new value to serial and change the voltage
  if(value != lastValue){
    targetVoltage = value * (3.3/1023.0) * 6.6;

    lcd.setCursor(0, 1);

    lcd.print("S: ");
    lcd.print(targetVoltage);
    lcd.print("V (");
    lcd.print(value);
    lcd.print(")    ");

    analogWrite(DAC0, value);
    lastValue = value;

    delay(500);

    measureVoltage();
  }
}

void togglePower() {
  lcd.setCursor(13, 3);
  if (turnedON) {
    analogWrite(DAC0, 0); // turn off
    Serial.println("Power supply turned OFF");
    lcd.print("OFF");
    turnedON = false;
  } else {
    analogWrite(DAC0, value); // turn on
    Serial.println("Power supply turned ON");
    lcd.print("ON ");
    turnedON = true;
  }
  measureVoltage();
  sendVoltageData(); // Send immediate update after power toggle
}

void measureVoltage(){
  
  int sum = 0;
  for (int i = 0; i < 100; i++){
    sum += analogRead(A1);
  }
  int sensorValue = sum/100;

  float a_voltage = sensorValue * (3.3/1023.0) * 6.6; // from 3.3 readings, max 21.78
  measuredVoltage = a_voltage;
  lcd.setCursor(0, 2);

  lcd.print("M: ");
  lcd.print(a_voltage);
  lcd.print("V (");
  lcd.print(sensorValue);
  lcd.print(")    ");
}

void sendVoltageData(){
  // Send JSON formatted voltage data
  Serial.print("{\"target\":");
  Serial.print(targetVoltage);
  Serial.print(",\"measured\":");
  Serial.print(measuredVoltage);
  Serial.print(",\"isOn\":");
  Serial.print(turnedON ? "true" : "false");
  Serial.println("}");
}

void read_encoder() {
  // Encoder interrupt routine for both pins. Updates value
  // if they are valid and have rotated a full indent
 
  static uint8_t old_AB = 3;  // Lookup table index
  static int8_t encval = 0;   // Encoder value  
  static const int8_t enc_states[]  = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Lookup table

  old_AB <<=2;  // Remember previous state

  if (digitalRead(ENC_A)) old_AB |= 0x02; // Add current state of pin A
  if (digitalRead(ENC_B)) old_AB |= 0x01; // Add current state of pin B
  
  encval += enc_states[( old_AB & 0x0f )];

  // Update value if encoder has rotated a full indent, that is at least 4 steps
  if( encval > 3 ) {        // Four steps forward
    int changevalue = 1;
    if((micros() - _lastIncReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastIncReadTime = micros();
    value = value + changevalue;              // Update value
    encval = 0;
  }
  else if( encval < -3 ) {        // Four steps backward
    int changevalue = -1;
    if((micros() - _lastDecReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastDecReadTime = micros();
    value = value + changevalue;              // Update value
    encval = 0;
  }

  double maxValue = 1023.0;
  if (value > maxValue){
    value = maxValue;
  }
  if (value < 0){
    value = 0;
  }
} 
