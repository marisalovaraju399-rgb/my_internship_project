// SAFETY PROTECTION ( RELAY , BUZZER , FAULT DETECTION) 

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define CELL1_PIN 34
#define CELL2_PIN 35
#define CELL3_PIN 32
#define CELL4_PIN 33

#define RELAY_PIN 18
#define BUZZER_PIN 19

unsigned long previousMillis = 0;
unsigned long faultStartTime = 0;

const unsigned long sampleInterval = 500;
const unsigned long faultDelay = 3000;

float prevCell1 = 0;

bool relayState = true;
bool faultDetected = false;

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Battery System");
  lcd.setCursor(0,1);
  lcd.print("Initializing");
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= sampleInterval) {

    previousMillis = currentMillis;

    float cell1 = analogRead(CELL1_PIN) * 3.3 / 4095.0;
    float cell2 = analogRead(CELL2_PIN) * 3.3 / 4095.0;
    float cell3 = analogRead(CELL3_PIN) * 3.3 / 4095.0;
    float cell4 = analogRead(CELL4_PIN) * 3.3 / 4095.0;

    String faultMessage = "SYSTEM NORMAL";
    faultDetected = false;

    // Weak cell detection
    if (cell1 < 2.8 || cell2 < 2.8 || cell3 < 2.8 || cell4 < 2.8) {

      faultDetected = true;
      faultMessage = "WEAK CELL";
    }

    // Overvoltage detection
    if (cell1 > 4.2 || cell2 > 4.2 || cell3 > 4.2 || cell4 > 4.2) {

      faultDetected = true;
      faultMessage = "OVER VOLTAGE";
    }

    // Sensor anomaly
    if (cell1 < 0.1 || cell2 < 0.1 || cell3 < 0.1 || cell4 < 0.1) {

      faultDetected = true;
      faultMessage = "SENSOR ERROR";
    }

    // Rapid voltage fluctuation
    if (abs(cell1 - prevCell1) > 0.5) {

      faultDetected = true;
      faultMessage = "VOLT FLUCT";
    }

    prevCell1 = cell1;

    // Relay chatter protection
    if (faultDetected) {

      if (faultStartTime == 0)
        faultStartTime = currentMillis;

      if (currentMillis - faultStartTime >= faultDelay) {

        relayState = false;
      }

    } else {

      faultStartTime = 0;
      relayState = true;
    }

    // Relay control
    digitalWrite(RELAY_PIN, relayState);

    // Buzzer control
    if (!relayState) {

      tone(BUZZER_PIN, 1000);

    } else {

      noTone(BUZZER_PIN);
    }

    // LCD display
    lcd.clear();

    if (relayState) {

      lcd.setCursor(0,0);
      lcd.print("SYSTEM NORMAL");

      lcd.setCursor(0,1);
      lcd.print("Relay ON");

    } else {

      lcd.setCursor(0,0);
      lcd.print(faultMessage);

      lcd.setCursor(0,1);
      lcd.print("Relay OFF");
    }

    // Serial Monitor
    Serial.println("---------------------");

    Serial.print("Cell1: ");
    Serial.println(cell1);

    Serial.print("Cell2: ");
    Serial.println(cell2);

    Serial.print("Cell3: ");
    Serial.println(cell3);

    Serial.print("Cell4: ");
    Serial.println(cell4);

    Serial.print("Status: ");
    Serial.println(faultMessage);

    Serial.print("Relay: ");

    if (relayState)
      Serial.println("ON");
    else
      Serial.println("OFF");
  }
}