// BUILDING AN INTELLIGENT EMBEDDED HMI(HUMAN MACHINE INTERFACE)

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

// Values received from previous tasks
float cellVoltage[4];
float packVoltage;
float avgVoltage;
float imbalance;

int strongestCell;
int weakestCell;

String batteryHealth;

bool relayStatus=false;
bool buzzerStatus=false;
bool faultDetected=false;

unsigned long previousLCD=0;
const unsigned long lcdInterval=2000;

byte screen=0;

void updateBatteryData()
{
  cellVoltage[0]=analogRead(34)*3.3/4095.0;
  cellVoltage[1]=analogRead(35)*3.3/4095.0;
  cellVoltage[2]=analogRead(32)*3.3/4095.0;
  cellVoltage[3]=analogRead(33)*3.3/4095.0;

  packVoltage=0;

  strongestCell=0;
  weakestCell=0;

  for(int i=0;i<4;i++)
  {
    packVoltage+=cellVoltage[i];

    if(cellVoltage[i]>cellVoltage[strongestCell])
      strongestCell=i;

    if(cellVoltage[i]<cellVoltage[weakestCell])
      weakestCell=i;
  }

  avgVoltage=packVoltage/4.0;

  imbalance=((cellVoltage[strongestCell]-cellVoltage[weakestCell])/avgVoltage)*100;

  if(cellVoltage[weakestCell]<1.5)
      batteryHealth="PACK FAILURE";
  else if(imbalance<2)
      batteryHealth="HEALTHY";
  else if(imbalance<5)
      batteryHealth="MINOR";
  else
      batteryHealth="CRITICAL";

  relayStatus=(batteryHealth=="CRITICAL" || batteryHealth=="PACK FAILURE");
  buzzerStatus=relayStatus;

  faultDetected=relayStatus;
}

void screen0()
{
  lcd.setCursor(0,0);
  lcd.print("Pack:");
  lcd.print(packVoltage,2);
  lcd.print("V   ");

  lcd.setCursor(0,1);
  lcd.print("Avg:");
  lcd.print(avgVoltage,2);
  lcd.print("V    ");
}

void screen1()
{
  lcd.setCursor(0,0);
  lcd.print("Imbal:");
  lcd.print(imbalance,1);
  lcd.print("% ");

  lcd.setCursor(0,1);
  lcd.print("S");
  lcd.print(strongestCell+1);

  lcd.print(" W");
  lcd.print(weakestCell+1);
}

void screen2()
{
  lcd.setCursor(0,0);
  lcd.print("Relay:");

  if(relayStatus)
      lcd.print("ON ");
  else
      lcd.print("OFF");

  lcd.setCursor(0,1);

  lcd.print("Buzz:");

  if(buzzerStatus)
      lcd.print("ON ");
  else
      lcd.print("OFF");
}

void screen3()
{
  lcd.setCursor(0,0);
  lcd.print("C1:");
  lcd.print(cellVoltage[0],2);

  lcd.setCursor(9,0);
  lcd.print("C2:");
  lcd.print(cellVoltage[1],2);

  lcd.setCursor(0,1);
  lcd.print("C3:");
  lcd.print(cellVoltage[2],2);

  lcd.setCursor(9,1);
  lcd.print("C4:");
  lcd.print(cellVoltage[3],2);
}

void screen4()
{
  lcd.setCursor(0,0);
  lcd.print("Health:");

  lcd.setCursor(0,1);
  lcd.print(batteryHealth);
}

void faultScreen()
{
  lcd.clear();

  lcd.setCursor(1,0);
  lcd.print("***FAULT***");

  lcd.setCursor(0,1);
  lcd.print(batteryHealth);
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(21,22);

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Embedded BMS");

  lcd.setCursor(0,1);
  lcd.print("Initializing");

  delay(2000);
  
  lcd.clear();
}

void loop()
{
  updateBatteryData();

  if(millis()-previousLCD>lcdInterval)
  {
    previousLCD=millis();

    if(faultDetected)
    {
      faultScreen();
    }
    else
    {
      lcd.clear();

      switch(screen)
      {
        case 0:
        screen0();
        break;

        case 1:
        screen1();
        break;

        case 2:
        screen2();
        break;

        case 3:
        screen3();
        break;

        case 4:
        screen4();
        break;
      }

      screen++;

      if(screen>4)
        screen=0;
    }
  }
Serial.print("C1 = "); Serial.println(cellVoltage[0], 2);
Serial.print("C2 = "); Serial.println(cellVoltage[1], 2);
Serial.print("C3 = "); Serial.println(cellVoltage[2], 2);
Serial.print("C4 = "); Serial.println(cellVoltage[3], 2);


}