// BUILDING A FAULT TOLERANT EMBEDDED RUNTIME SYSTEM

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- PIN DEFINITIONS ---
#define C1_PIN 34
#define C2_PIN 35
#define C3_PIN 32
#define C4_PIN 33
#define RED_LED 4
#define GREEN_LED 2
#define YELLOW_LED 5
#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUTTON_PIN 23

// --- SYSTEM STATES ---
enum SystemState {
  STATE_NORMAL,
  STATE_DEGRADED,
  STATE_FAILSAFE,
  STATE_SHUTDOWN
};
SystemState currentSystemState = STATE_NORMAL;

// --- STRUCTURES FOR FAULT LOGGING ---
struct FaultLog {
  unsigned long timestamp;
  String message;
  SystemState stateAtFault;
};
FaultLog activeFault;

// --- GLOBAL VARIABLES & DATA BUFFERS ---

float cellVoltage[4] = {0, 0, 0, 0};
float prevCellVoltage[4] = {0, 0, 0, 0};
int adcFreezeCounter[4] = {0, 0, 0, 0};
bool cellIsolated[4] = {false, false, false, false};

float packVoltage = 0.0;
float avgVoltage = 0.0;
float imbalance = 0.0;
int strongestCell = 0;
int weakestCell = 0;

String batteryHealth = "UNKNOWN";
String criticalFaultMsg = "";

bool relayState = true;   // true = Active/Closed, false = Tripped/Open
bool buzzerState = false;
bool faultDetected = false;

// --- TIMING MANAGERS ---
unsigned long prevSampleMillis = 0;
unsigned long prevLCDMillis = 0;
unsigned long faultStartTime = 0;
const unsigned long sampleInterval = 500;
const unsigned long lcdInterval = 2000;
const unsigned long faultDelayThreshold = 3000; // 3 seconds verification before latching Failsafe

byte screenIndex = 0;

// --- FUNCTION DECLARATIONS ---
void readAndValidateSensors();
void processBMSLogic();
void determineSystemState();
void executeSafetyActions();
void updateHMI();
void logFault(String msg, SystemState state);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize safe state
  digitalWrite(RELAY_PIN, HIGH); 
  relayState = true;

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FT-BMS ENGINE");
  lcd.setCursor(0, 1);
  lcd.print("INITIALIZING...");
  delay(2000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Run core protection, state engine, and calculations at regular intervals
  if (currentMillis - prevSampleMillis >= sampleInterval) {
    prevSampleMillis = currentMillis;
    
    readAndValidateSensors();
    processBMSLogic();
    determineSystemState();
    executeSafetyActions();
    
    // Output diagnostics to Serial Console
    Serial.print("State: "); Serial.print(currentSystemState);
    Serial.print(" | C1: "); Serial.print(cellVoltage[0]);
    Serial.print(" | C2: "); Serial.print(cellVoltage[1]);
    Serial.print(" | C3: "); Serial.print(cellVoltage[2]);
    Serial.print(" | C4: "); Serial.print(cellVoltage[3]);
    Serial.print(" | Health: "); Serial.println(batteryHealth);
  }

  // Handle HMI Multi-Screen cycling
  if (currentMillis - prevLCDMillis >= lcdInterval) {
    prevLCDMillis = currentMillis;
    updateHMI();
  }
}

// --- TASK 4: FAULT DETECTION & SENSOR VALIDATION MODULE ---
void readAndValidateSensors() {
  int pins[4] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};
  
  for (int i = 0; i < 4; i++) {
    float rawRead = analogRead(pins[i]) * 3.3 / 4095.0;
    
    // 1. Sensor Disconnection / Invalid Lower Limits
    if (rawRead < 0.1) {
      logFault("C" + String(i+1) + " SENSOR ERR", currentSystemState);
      cellIsolated[i] = true;
      cellVoltage[i] = 0.0; // Isolate raw read
      continue;
    }
    
    // 2. Frozen ADC Condition Detection
    if (abs(rawRead - prevCellVoltage[i]) < 0.0001) {
      adcFreezeCounter[i]++;
      if (adcFreezeCounter[i] > 10) { // Frozen across 10 sample periods
        logFault("C" + String(i+1) + " ADC FROZEN", currentSystemState);
        cellIsolated[i] = true;
      }
    } else {
      adcFreezeCounter[i] = 0; // Reset counter on active fluctuation
      cellIsolated[i] = false;
    }
    
    prevCellVoltage[i] = rawRead;
    
    // Update data if cell performance reads valid parameters
    if (!cellIsolated[i]) {
      cellVoltage[i] = rawRead;
    }
  }
}

// --- TASK 1 & TASK 2: DATA PROCESSING ENGINE ---
void processBMSLogic() {
  packVoltage = 0;
  strongestCell = -1;
  weakestCell = -1;
  
  int validCellCount = 0;
  float maxV = -1.0;
  float minV = 6.0;

  for (int i = 0; i < 4; i++) {
    if (!cellIsolated[i]) {
      packVoltage += cellVoltage[i];
      validCellCount++;
      
      if (cellVoltage[i] > maxV) {
        maxV = cellVoltage[i];
        strongestCell = i;
      }
      if (cellVoltage[i] < minV) {
        minV = cellVoltage[i];
        weakestCell = i;
      }
    }
  }

  // Fallback if all cells report errors
  if (validCellCount == 0) {
    avgVoltage = 0;
    imbalance = 100.0;
    batteryHealth = "PACK FAILURE";
    return;
  }

  avgVoltage = packVoltage / (float)validCellCount;
  
  if (validCellCount > 1) {
    imbalance = ((maxV - minV) / avgVoltage) * 100.0;
  } else {
    imbalance = 0.0; // Can't calculate variance on single functional cell
  }

  // Rapid voltage fluctuation check (Task 2 metric applied on valid elements)
  for(int i=0; i<4; i++) {
    if(!cellIsolated[i] && abs(cellVoltage[i] - prevCellVoltage[i]) > 0.5) {
       logFault("VOLT FLUCT", currentSystemState);
    }
  }

  // Health assignment rules
  if (minV < 1.5 || validCellCount < 3) {
    batteryHealth = "PACK FAILURE";
  } else if (imbalance < 2.0) {
    batteryHealth = "HEALTHY";
  } else if (imbalance < 5.0) {
    batteryHealth = "MINOR IMBALANCE";
  } else {
    batteryHealth = "CRITICAL IMBALANCE";
  }
}

// --- TASK 4: FINITE STATE MACHINE ENGINE ---
void determineSystemState() {
  bool hardFault = false;
  bool softFault = false;
  criticalFaultMsg = "SYSTEM NORMAL";

  // Scan through hardware states to look for system dependencies
  for(int i = 0; i < 4; i++) {
    if (cellIsolated[i]) {
      softFault = true;
      criticalFaultMsg = "SENSOR / ADC ERR";
    }
    if (!cellIsolated[i] && (cellVoltage[i] > 4.2 || cellVoltage[i] < 2.8)) {
      hardFault = true;
      criticalFaultMsg = (cellVoltage[i] > 4.2) ? "OVER VOLTAGE" : "WEAK CELL";
    }
  }

  if (batteryHealth == "PACK FAILURE") {
    hardFault = true;
    criticalFaultMsg = "PACK FAILURE";
  }

  // State Transition Machine Logic
  switch (currentSystemState) {
    case STATE_NORMAL:
      if (hardFault) {
        currentSystemState = STATE_FAILSAFE;
        faultStartTime = millis();
      } else if (softFault || batteryHealth == "CRITICAL IMBALANCE") {
        currentSystemState = STATE_DEGRADED;
      }
      break;

    case STATE_DEGRADED:
      if (hardFault) {
        currentSystemState = STATE_FAILSAFE;
        faultStartTime = millis();
      } else if (!softFault && batteryHealth != "CRITICAL IMBALANCE") {
        currentSystemState = STATE_NORMAL; // Self-recovery block
      }
      break;

    case STATE_FAILSAFE:
      // Debounce and delay safety latch to avoid relay chatter
      if (millis() - faultStartTime >= faultDelayThreshold) {
        if (hardFault) {
          currentSystemState = STATE_SHUTDOWN; // Permanent latch shutdown
        } else {
          currentSystemState = STATE_NORMAL; // Safe to resume operation
        }
      }
      break;

    case STATE_SHUTDOWN:
      // Lockdown state requiring physical power cycle reset to escape
      break;
  }
}

// --- TASK 2 & TASK 4: HARDWARE ISOLATION & OUTPUT SAFETY MAPPING ---
void executeSafetyActions() {
  // Translate system software states into hardware safety outputs
  if (currentSystemState == STATE_NORMAL || currentSystemState == STATE_DEGRADED) {
    relayState = true;   // Safe operation connection
    buzzerState = false;
  } else {
    relayState = false;  // Disconnect high voltage loop
    buzzerState = true;
  }

  // Actuate Hardware Outputs
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

  if (buzzerState) {
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  // Task 4: Relay Mismatch Evaluation
  // Simulates reading a digital input connected to the relay status contact point
  bool expectedFeedbackSignal = relayState; 
  if (digitalRead(RELAY_PIN) != (relayState ? HIGH : LOW)) {
     logFault("RELAY MISMATCH", currentSystemState);
     currentSystemState = STATE_SHUTDOWN; // Lock down due to safety critical switch failure
  }
}

// --- TASK 4: STRUCT LOGGING SYSTEM ---
void logFault(String msg, SystemState state) {
  activeFault.timestamp = millis();
  activeFault.message = msg;
  activeFault.stateAtFault = state;
  
  Serial.print("[FAULT LOGGED] ");
  Serial.print(activeFault.timestamp);
  Serial.print("ms - Msg: ");
  Serial.println(activeFault.message);
}

// --- TASK 3: RE-ENGINIERED ADAPTIVE HMI SYSTEM ---
void updateHMI() {
  lcd.clear();

  // Global override display if runtime subsystem flags a system shutdown fault
  if (currentSystemState == STATE_SHUTDOWN) {
    lcd.setCursor(0, 0);
    lcd.print("!!! SHUTDOWN !!!");
    lcd.setCursor(0, 1);
    lcd.print(criticalFaultMsg);
    return;
  }

  switch (screenIndex) {
    case 0: // Pack Metrics
      lcd.setCursor(0, 0);
      lcd.print("Pack:"); lcd.print(packVoltage, 2); lcd.print("V");
      lcd.setCursor(0, 1);
      lcd.print("Avg :"); lcd.print(avgVoltage, 2);  lcd.print("V");
      break;

    case 1: // Imbalance & Index tracking
      lcd.setCursor(0, 0);
      lcd.print("Imbal:"); lcd.print(imbalance, 1);  lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("S:"); lcd.print(strongestCell >= 0 ? String(strongestCell + 1) : "ERR");
      lcd.print(" W:"); lcd.print(weakestCell >= 0 ? String(weakestCell + 1) : "ERR");
      break;

    case 2: // Control Matrix Outputs
      lcd.setCursor(0, 0);
      lcd.print("Relay:"); lcd.print(relayState ? "ON (CONNECTED)" : "OFF (TRIPPED)");
      lcd.setCursor(0, 1);
      lcd.print("Buzz :"); lcd.print(buzzerState ? "ACTIVE" : "OFF");
      break;

    case 3: // Matrix Individual Cells
      lcd.setCursor(0, 0); lcd.print("C1:"); lcd.print(cellIsolated[0] ? "ERR " : String(cellVoltage[0], 2));
      lcd.setCursor(9, 0); lcd.print("C2:"); lcd.print(cellIsolated[1] ? "ERR " : String(cellVoltage[1], 2));
      lcd.setCursor(0, 1); lcd.print("C3:"); lcd.print(cellIsolated[2] ? "ERR " : String(cellVoltage[2], 2));
      lcd.setCursor(9, 1); lcd.print("C4:"); lcd.print(cellIsolated[3] ? "ERR " : String(cellVoltage[3], 2));
      break;

    case 4: // Runtime Mode Status
      lcd.setCursor(0, 0);
      lcd.print("Health: "); lcd.print(batteryHealth);
      lcd.setCursor(0, 1);
      lcd.print("Mode  : ");
      if (currentSystemState == STATE_NORMAL) lcd.print("NORMAL");
      else if (currentSystemState == STATE_DEGRADED) lcd.print("DEGRADED");
      else if (currentSystemState == STATE_FAILSAFE) lcd.print("FAILSAFE");
      break;
  }

  // Cycle index pointer
  screenIndex++;
  if (screenIndex > 4) {
    screenIndex = 0;
  }
}