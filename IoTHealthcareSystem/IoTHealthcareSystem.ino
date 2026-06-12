#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30100_PulseOximeter.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// --- OLED CONFIGURATION ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- MAX30100 CONFIGURATION ---
PulseOximeter pox;
uint32_t tsLastReport = 0;
#define REPORTING_PERIOD_MS 1000 

// --- HEART RATE FILTER ---
const int FILTER_SIZE = 10;
float bpmHistory[FILTER_SIZE];
int spo2History[FILTER_SIZE];
int filterIndex = 0;

// --- DS18B20 CONFIGURATION ---
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// BPM noise filter function
float getFilteredBPM(float newBPM) {
  if (newBPM > 180.0 || newBPM < 40.0) {
    float sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += bpmHistory[i];
    return sum / FILTER_SIZE;
  }
  bpmHistory[filterIndex] = newBPM;
  float sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += bpmHistory[i];
  return sum / FILTER_SIZE;
}

// SpO2 noise filter function
int getFilteredSPO2(int newSPO2) {
  if (newSPO2 > 100 || newSPO2 < 70) {
    int sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) sum += spo2History[i];
    return sum / FILTER_SIZE;
  }
  spo2History[filterIndex] = newSPO2;
  int sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += spo2History[i];
  return sum / FILTER_SIZE;
}

void onBeatDetected() {
  Serial.println("♥ Heartbeat detected!");
}

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Wire.begin(21, 22);

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR");
    for(;;); 
  }
  
  // Clear filter array
  for(int i = 0; i < FILTER_SIZE; i++) {
    bpmHistory[i] = 0;
    spo2History[i] = 0;
  }

  // Initialize MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 ERROR");
    for(;;); 
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_50MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  // Initialize DS18B20
  sensors.begin();
  sensors.setWaitForConversion(false);
  
  Serial.println("INITIALIZATION SUCCESSFUL! System ready.");
}

void loop() {
  pox.update(); // Continuously poll the optical sensor

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    // 1. READ HEART RATE AND SPO2
    float rawBPM = pox.getHeartRate();
    int rawSPO2 = pox.getSpO2();
    float finalBPM = 0;
    int finalSPO2 = 0;

    // Check for finger (If = 0 means finger removed)
    if (rawBPM == 0) {
      for(int i = 0; i < FILTER_SIZE; i++) {
        bpmHistory[i] = 0;
        spo2History[i] = 0;
      }
    } else {
      finalBPM = getFilteredBPM(rawBPM);
      finalSPO2 = getFilteredSPO2(rawSPO2);
      filterIndex = (filterIndex + 1) % FILTER_SIZE;
    }

    // 2. READ TEMPERATURE
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    // 3. UPDATE OLED DISPLAY
    display.clearDisplay();
    display.setTextColor(WHITE);
    
    display.setTextSize(1);
    display.setCursor(15, 0);
    display.println("Smart Healthcare");
    display.drawLine(0, 10, 128, 10, WHITE);

    // BPM Line
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print("BPM : ");
    display.setTextSize(2);
    if (finalBPM == 0) display.println("--");
    else display.println((int)finalBPM);
    
    // SpO2 Line
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.print("SpO2: ");
    display.setTextSize(2);
    if (finalSPO2 == 0) display.println("-- %");
    else { display.print(finalSPO2); display.println(" %"); }

    // Temperature Line
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.print("Temp: ");
    if (tempC == DEVICE_DISCONNECTED_C || tempC == -127.00) {
      display.println("DS18 Err");
    } else { 
      display.print(tempC); display.println(" °C"); 
    }

    display.display();
    tsLastReport = millis();
  }
}
