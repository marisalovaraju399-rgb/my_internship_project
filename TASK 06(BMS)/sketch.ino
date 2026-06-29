// EXECUTIVE BATTERY INTELLIGENCE DASHBOARD

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PIN ASSIGNMENTS ---
#define POT_V_PIN  34   // Voltage Slider
#define POT_C_PIN  35   // Current Slider
#define POT_T_PIN  32   // Temperature Slider

#define LED_RED    25
#define LED_YLW    26
#define LED_GRN    27
#define BUZZER     13

// Initialize I2C LCD (Address 0x27, 16 Columns, 2 Rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long prevTime = 0;
const int intervals = 1000; 
bool toggleState = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YLW, OUTPUT);
  pinMode(LED_GRN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Initialize display
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("BMS INITIALIZING");
  delay(1500);
  lcd.clear();
}

void loop() {
  if (millis() - prevTime >= intervals) {
    prevTime = millis();
    toggleState = !toggleState;

    // 1. Read individual dedicated battery sensors
    float voltage = (analogRead(POT_V_PIN) / 4095.0) * 5.0;       // Scale 0 - 5V
    float current = ((analogRead(POT_C_PIN) / 4095.0) * 20.0) - 10.0; // Scale -10A to +10A
    float tempC = (analogRead(POT_T_PIN) / 4095.0) * 100.0;     // Scale 0°C - 100°C

    // 2. Clear status flags
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YLW, LOW);
    digitalWrite(LED_GRN, LOW);
    noTone(BUZZER);
    
    String riskLevel = "NOMINAL";

    // 3. Risk Threshold Management Logic
    if (voltage > 4.2 || voltage < 2.8 || tempC > 55.0) {
      riskLevel = "CRITICAL";
      digitalWrite(LED_RED, HIGH);
      if(toggleState) { 
        tone(BUZZER, 880); // Pulsing alert sequence
      }
    } else if (voltage > 4.1 || voltage < 3.2 || tempC > 42.0) {
      riskLevel = "WARNING";
      digitalWrite(LED_YLW, HIGH);
    } else {
      digitalWrite(LED_GRN, HIGH);
    }

    // 4. Update LCD View Screen Matrix (Alternating dynamic pages)
    lcd.clear();
    if (toggleState) {
      // Page 1: Live Battery Operations Layout
      lcd.setCursor(0, 0);
      lcd.print("V:" + String(voltage, 2) + "V  A:" + String(current, 1) + "A");
      lcd.setCursor(0, 1);
      lcd.print("TEMP: " + String(tempC, 1) + " C");
    } else {
      // Page 2: Risk Assessment & Advisory Layout
      lcd.setCursor(0, 0);
      lcd.print("RISK: " + riskLevel);
      lcd.setCursor(0, 1);
      if (riskLevel == "CRITICAL") {
        lcd.print("STOP PACK CYCLING");
      } else if (riskLevel == "WARNING") {
        lcd.print("CHECK CELL TEMP");
      } else {
        lcd.print("SYS STATUS: OK");
      }
    }

    // 5. Output Legacy Standard JSON telemetry string
    Serial.print("{\"voltage\":"); Serial.print(voltage);
    Serial.print(",\"current\":"); Serial.print(current);
    Serial.print(",\"temp\":"); Serial.print(tempC);
    Serial.print(",\"risk\":\""); Serial.print(riskLevel);
    Serial.println("\"}");
  }
}