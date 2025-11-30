#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   16      
#define OLED_SDA     4       
#define OLED_SCL     15      

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* Add wifi credentials */
const char* ssid     =  "***"
const char* password = "***"


const char* apiBaseUrl = "BASE_URL_TEMPLATE/api/v1/bin";
const char* binId      = "1bbf9e40-3d9f-4a33-bb38-937866bc91e4";

const unsigned long sendIntervalMs = 5000;
unsigned long lastSent = 0;

const int trigPin  = 21;
const int echoPin  = 22;

const int greenLed = 13;
const int redLed   = 14;

const int buzzerPin = 32;

const int buttonPin = 33;

enum BinMode {
  MODE_MONITORING,
  MODE_MAINTENANCE
};

BinMode currentMode = MODE_MONITORING;

int  lastButtonReading    = LOW;
int  stableButtonState    = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

#define SOUND_SPEED 0.034

float distanceCm;
const float FULL_THRESHOLD_CM   = 15.0;
const float MEDIUM_THRESHOLD_CM = 40.0;

bool alarmOn = false;
bool alarmBeeping = false;
unsigned long nextAlarmToggle = 0;

void startAlarm() {
  if (alarmOn) return;
  alarmOn = true;
  alarmBeeping = false;         
  nextAlarmToggle = millis();   
}

void stopAlarm() {
  noTone(buzzerPin);
  alarmOn = false;
  alarmBeeping = false;
}

void updateAlarm() {
  if (!alarmOn) return;

  unsigned long now = millis();
  if (now >= nextAlarmToggle) {
    if (alarmBeeping) {
      noTone(buzzerPin);
      alarmBeeping = false;
      nextAlarmToggle = now + 250;   
    } else {
      tone(buzzerPin, 2500);         
      alarmBeeping = true;
      nextAlarmToggle = now + 250;   
    }
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi dropped, reconnecting...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi reconnected!");
}

void sendTelemetry(float currentLevel, const String& status) {
  ensureWiFiConnected();

  HTTPClient http;
  String url = String(apiBaseUrl) + "/telemetry/" + binId;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"currentLevel\":" + String(currentLevel, 1) + ",";
  payload += "\"status\":\"" + status + "\"";
  payload += "}";

  Serial.println("POST " + url);
  Serial.println("Payload: " + payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + http.getString());
  } else {
    Serial.print("HTTP POST failed: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void drawMonitoringOLED(const String& status, float dist) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("MODE: MONITORING");

  display.setCursor(0, 16);
  display.print("STATUS: ");
  display.println(status);

  display.setCursor(0, 32);
  display.print("Dist: ");
  display.print(dist, 1);
  display.println(" cm");

  display.display();
}

void drawNoEchoOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("MODE: MONITORING");
  display.setCursor(0, 20);
  display.println("No echo detected");

  display.display();
}

void drawMaintenanceOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("MODE: MAINTENANCE");
  display.setCursor(0, 20);
  display.println("Readings paused");
  display.setCursor(0, 36);
  display.println("Safe to change bag");

  display.display();
}

void handleButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == HIGH) {
        if (currentMode == MODE_MONITORING) {
          currentMode = MODE_MAINTENANCE;
          stopAlarm();
          digitalWrite(greenLed, LOW);
          digitalWrite(redLed, LOW);
          Serial.println("Mode: MAINTENANCE (readings paused)");
          drawMaintenanceOLED();
        } else {
          currentMode = MODE_MONITORING;
          Serial.println("Mode: MONITORING (readings active)");
        }
      }
    }
  }

  lastButtonReading = reading;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (true) { delay(100); }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(trigPin,  OUTPUT);
  pinMode(echoPin,  INPUT);

  pinMode(greenLed, OUTPUT);
  pinMode(redLed,   OUTPUT);

  pinMode(buzzerPin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLDOWN);

  stopAlarm();  
}

void loop() {
  ensureWiFiConnected();
  handleButton();
  updateAlarm();   

  if (currentMode == MODE_MAINTENANCE) {
    stopAlarm();
    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, LOW);

    Serial.println("MAINTENANCE MODE - readings paused");
    delay(300);
    return;
  }


  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) {
    Serial.println("No echo detected");
    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, LOW);
    stopAlarm();
    drawNoEchoOLED();
    delay(300);
    return;
  }

  distanceCm = duration * SOUND_SPEED / 2.0;
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);

  String status;
  float currentLevel = distanceCm;

  if (distanceCm <= FULL_THRESHOLD_CM) {
    status = "FULL";
    Serial.println("Status: FULL");

    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, HIGH);
    startAlarm();  
  }

  else if (distanceCm <= MEDIUM_THRESHOLD_CM) {
    status = "MEDIUM";
    Serial.println("Status: MEDIUM");

    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, LOW);
    stopAlarm();
  }
  else {
    status = "EMPTY";
    Serial.println("Status: EMPTY");

    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, LOW);
    stopAlarm();
  }

  drawMonitoringOLED(status, distanceCm);

  unsigned long now = millis();
  if (now - lastSent >= sendIntervalMs) {
    sendTelemetry(currentLevel, status);
    lastSent = now;
  }

  Serial.println("---------------------");
  delay(150);
}
