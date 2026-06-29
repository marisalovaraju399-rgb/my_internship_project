// INTELLIGENT CLOUD TELEMENTRY ARCHITECHTURE

#define BLYNK_TEMPLATE_ID "TMPL00000000"
#define BLYNK_TEMPLATE_NAME "Intelligent Cloud Telemetry"
#define BLYNK_AUTH_TOKEN "Your_Blynk_Auth_Token_Here"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <queue>

// Pin Configurations
#define POT_PIN 34
#define POT_PIN 35
#define POT_PIN 32
#define POT_PIN 33
#define RELAY_PIN 18
#define BUZZER_PIN 19
#define BUTTON_PIN 23

// Threshold Settings
const float VOLTAGE_MAX_THRESHOLD = 2.80; 
const float VOLTAGE_MIN_THRESHOLD = 0.50;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds non-blocking retry

// System States and Variables
int lastButtonState = HIGH;
float lastVoltage = 0.0;
unsigned long lastReconnectAttempt = 0;
bool blynkConnected = false;

// Event Payload Structure for Queue Synchronization
struct TelemetryEvent {
  String type;
  float value;
  int state;
  int rssi;
};
std::queue<TelemetryEvent> eventQueue;

// Non-blocking WiFi and Blynk connection supervisor
void manageNetworkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    blynkConnected = false;
    if (millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      Serial.println("[SYSTEM] WiFi offline. Attempting non-blocking reconnect...");
      lastReconnectAttempt = millis();
      WiFi.begin("Wokwi-GUEST", ""); // Wokwi Simulated Open Wi-Fi
    }
    return;
  }

  // If WiFi is connected but Blynk isn't
  if (!blynkConnected) {
    Serial.println("[SYSTEM] WiFi OK. Synchronizing with Blynk Cloud...");
    Blynk.config(BLYNK_AUTH_TOKEN);
    // Use connect() with a minimal timeout instead of the blocking Blynk.begin()
    if (Blynk.connect(2000)) { 
      blynkConnected = true;
      Serial.println("[SYSTEM] Blynk Connected! Flushing queued events...");
      flushEventQueue();
    }
  }
}

// Push local events to Queue or transmit immediately if online
void processTelemetryEvent(String eventType, float val, int state) {
  int currentRSSI = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -99;
  
  TelemetryEvent ev = {eventType, val, state, currentRSSI};

  if (blynkConnected) {
    sendToCloud(ev);
  } else {
    Serial.printf("[QUEUE] Offline. Enqueuing event: %s | Val: %.2f\n", eventType.c_str(), val);
    // Constrain queue sizes to protect device Heap Memory from overflowing
    if (eventQueue.size() < 50) { 
      eventQueue.push(ev);
    } else {
      Serial.println("[WARNING] Queue full! Dropping oldest telemetry event.");
      eventQueue.pop();
      eventQueue.push(ev);
    }
  }
}

// Flush stored events sequentially on network restoration
void flushEventQueue() {
  while (!eventQueue.empty() && blynkConnected) {
    TelemetryEvent ev = eventQueue.front();
    sendToCloud(ev);
    eventQueue.pop();
    delay(50); // Small pacing delay between historical bursts
  }
}

// Asynchronous direct write helper
void sendToCloud(TelemetryEvent ev) {
  Serial.printf("[CLOUD] Sending: %s | Val: %.2f | RSSI: %d dBm\n", ev.type.c_str(), ev.value, ev.rssi);
  
  Blynk.virtualWrite(V1, ev.type);
  Blynk.virtualWrite(V2, ev.value);
  Blynk.virtualWrite(V3, ev.state);
  Blynk.virtualWrite(V4, ev.rssi); // Signal Quality Monitoring
}

void checkSensors() {
  // 1. Digital State Change Telemetry 
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState != lastButtonState) {
    lastButtonState = currentButtonState;
    processTelemetryEvent("STATE_CHANGE", 0.0, currentButtonState);
  }

  // 2. Analog Voltage & Anomaly Telemetry
  int rawAnalog = analogRead(POT_PIN);
  float voltage = (rawAnalog / 4095.0) * 3.3;

  // Transmit on anomalous threshold violations or if a massive delta variance occurs
  if (voltage >= VOLTAGE_MAX_THRESHOLD && lastVoltage < VOLTAGE_MAX_THRESHOLD) {
    processTelemetryEvent("ANOMALY_HIGH_VOLT", voltage, currentButtonState);
  } else if (voltage <= VOLTAGE_MIN_THRESHOLD && lastVoltage > VOLTAGE_MIN_THRESHOLD) {
    processTelemetryEvent("ANOMALY_LOW_VOLT", voltage, currentButtonState);
  } else if (abs(voltage - lastVoltage) > 0.50) { // Significant Delta Change
    processTelemetryEvent("DELTA_VARIANCE", voltage, currentButtonState);
  }
  
  lastVoltage = voltage;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("[SYSTEM] Initializing Smart Telemetry Node...");
  WiFi.begin("Wokwi-GUEST", ""); // Connect to Wokwi network asynchronously
}

void loop() {
  // Maintain network states asynchronously
  manageNetworkConnection();

  // If connected, execute the Blynk background routine without letting it loop block
  if (blynkConnected) {
    Blynk.run();
  }

  // Embedded application execution loops continuously independent of network health
  checkSensors();

  delay(50); // Dynamic pacing for sampling stability
}










