// ADAPTIVE MULTI-CELL BATTERY INTELLIGENCE ENGINE

#define C1_PIN 34
#define C2_PIN 35
#define C3_PIN 32
#define C4_PIN 33

float c1, c2, c3, c4;

void setup() {
  Serial.begin(115200);
}

void loop() {

  // Read voltages
  c1 = analogRead(C1_PIN) * 3.3 / 4095.0;
  c2 = analogRead(C2_PIN) * 3.3 / 4095.0;
  c3 = analogRead(C3_PIN) * 3.3 / 4095.0;
  c4 = analogRead(C4_PIN) * 3.3 / 4095.0;

  // Pack voltage
  float packVoltage = c1 + c2 + c3 + c4;

  // Average voltage
  float avgVoltage = packVoltage / 4.0;

  // Find maximum voltage
  float maxV = c1;
  int strongestCell = 1;

  if (c2 > maxV) {
    maxV = c2;
    strongestCell = 2;
  }

  if (c3 > maxV) {
    maxV = c3;
    strongestCell = 3;
  }

  if (c4 > maxV) {
    maxV = c4;
    strongestCell = 4;
  }

  // Find minimum voltage
  float minV = c1;
  int weakestCell = 1;

  if (c2 < minV) {
    minV = c2;
    weakestCell = 2;
  }

  if (c3 < minV) {
    minV = c3;
    weakestCell = 3;
  }

  if (c4 < minV) {
    minV = c4;
    weakestCell = 4;
  }

  // Imbalance calculation
  float imbalance = ((maxV - minV) / avgVoltage) * 100.0;

  // Health classification
  String healthStatus;

  if (minV < 1.5) {
    healthStatus = "PACK FAILURE";
  }
  else if (imbalance < 2) {
    healthStatus = "HEALTHY";
  }
  else if (imbalance < 5) {
    healthStatus = "MINOR IMBALANCE";
  }
  else {
    healthStatus = "CRITICAL IMBALANCE";
  }

  // Display results
  Serial.println("\n======================");

  Serial.print("C1 Voltage: ");
  Serial.print(c1, 2);
  Serial.println(" V");

  Serial.print("C2 Voltage: ");
  Serial.print(c2, 2);
  Serial.println(" V");

  Serial.print("C3 Voltage: ");
  Serial.print(c3, 2);
  Serial.println(" V");

  Serial.print("C4 Voltage: ");
  Serial.print(c4, 2);
  Serial.println(" V");

  Serial.print("Pack Voltage: ");
  Serial.print(packVoltage, 2);
  Serial.println(" V");

  Serial.print("Average Voltage: ");
  Serial.print(avgVoltage, 2);
  Serial.println(" V");

  Serial.print("Strongest Cell: Cell ");
  Serial.println(strongestCell);

  Serial.print("Weakest Cell: Cell ");
  Serial.println(weakestCell);

  Serial.print("Imbalance Percentage: ");
  Serial.print(imbalance, 2);
  Serial.println("%");

  Serial.print("Battery Health: ");
  Serial.println(healthStatus);

  delay(2000);
}