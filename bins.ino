#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED CONFIG (TTGO LoRa32 onboard)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 16
#define OLED_SDA 4
#define OLED_SCL 15

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi CONFIG
const char *ssid = "****";
const char *password = "****";

// BACKEND CONFIG (NGROK)
const char *apiBaseUrl = "****/api/v1/bin";
const char *binId = "****-****-****-****-*****";

// How often to send telemetry in milliseconds
const unsigned long sendIntervalMs = 5000;
unsigned long lastSent = 0;

// PINS
const int trigPin = 21;
const int echoPin = 22;

const int greenLed = 13;
const int redLed = 14;

const int buzzerPin = 32;

// Button
const int buttonPin = 33;

// MODE STATE
enum BinMode
{
  MODE_MONITORING,
  MODE_MAINTENANCE
};

BinMode currentMode = MODE_MONITORING;

// BUTTON DEBOUNCE (INPUT_PULLDOWN)
int lastButtonReading = LOW;
int stableButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// DISTANCE / THRESHOLDS
#define SOUND_SPEED 0.034

float distanceCm;
const float FULL_THRESHOLD_CM = 3.0;
const float MEDIUM_THRESHOLD_CM = 20.0;

// ALARM STATE (BEEP–PAUSE PATTERN)
bool alarmOn = false;
bool alarmBeeping = false;
unsigned long nextAlarmToggle = 0;

// ALARM HELPERS
void startAlarm()
{
  if (alarmOn)
    return;
  alarmOn = true;
  alarmBeeping = false;
  nextAlarmToggle = millis();
}

void stopAlarm()
{
  noTone(buzzerPin);
  alarmOn = false;
  alarmBeeping = false;
}

void updateAlarm()
{
  if (!alarmOn)
    return;

  unsigned long now = millis();
  if (now >= nextAlarmToggle)
  {
    if (alarmBeeping)
    {

      // turn OFF sound
      noTone(buzzerPin);
      alarmBeeping = false;
      nextAlarmToggle = now + 250; // 250 ms silence for alarm sounds
    }
    else
    {
      // turn ON sound
      tone(buzzerPin, 2500); // 2.5 kHz frequency for a higher pitch
      alarmBeeping = true;
      nextAlarmToggle = now + 250; // 250 ms beep... next beep
    }
  }
}

// WIFI HELPERS
void ensureWiFiConnected()
{
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.println("WiFi dropped, reconnecting...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi reconnected!");
}

// TELEMETRY: POST JSON request sent to the springboot application
void sendTelemetry(float currentLevel, const String &status)
{
  ensureWiFiConnected();

  HTTPClient http;
  String url = String(apiBaseUrl) + "/telemetry/" + binId;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  float adjustedCurrentLevel = currentLevel - 3;

  String payload = "{";
  payload += "\"currentLevel\":" + String(adjustedCurrentLevel < 0 ? 0 : adjustedCurrentLevel, 1) + ",";
  payload += "\"status\":\"" + status + "\"";
  payload += "}";

  Serial.println("POST " + url);
  Serial.println("Payload: " + payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + http.getString());
  }
  else
  {
    Serial.print("HTTP POST failed: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// OLED HELPERS
void drawMonitoringOLED(const String &status, float dist)
{
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

void drawNoEchoOLED()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("MODE: MONITORING");
  display.setCursor(0, 20);
  display.println("No echo detected");

  display.display();
}

void drawMaintenanceOLED()
{
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

// BUTTON HANDLER (mode toggle) between MONITORING & MAINTENANCE
void handleButton()
{
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (reading != stableButtonState)
    {
      stableButtonState = reading;

      // Rising edge: LOW -> HIGH = button pressed (we need to press & hold the button till toggle)
      if (stableButtonState == HIGH)
      {
        if (currentMode == MODE_MONITORING)
        {
          currentMode = MODE_MAINTENANCE;
          stopAlarm();
          digitalWrite(greenLed, LOW);
          digitalWrite(redLed, LOW);
          Serial.println("Mode: MAINTENANCE (readings paused)");
          drawMaintenanceOLED();
        }
        else
        {
          currentMode = MODE_MONITORING;
          Serial.println("Mode: MONITORING (readings active)");
        }
      }
    }
  }

  lastButtonReading = reading;
}

// SETUP
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // OLED initialization via specific pins
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("SSD1306 allocation failed");
    while (true)
    {
      delay(100);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(greenLed, OUTPUT);
  pinMode(redLed, OUTPUT);

  pinMode(buzzerPin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLDOWN);

  stopAlarm(); // make sure buzzer is off
}

// MAIN LOOP
void loop()
{
  ensureWiFiConnected();
  handleButton();
  updateAlarm(); // runs alarm beep pause pattern if active

  // If in maintenance mode: skip readings & telemetry
  if (currentMode == MODE_MAINTENANCE)
  {
    stopAlarm();
    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, LOW);

    Serial.println("MAINTENANCE MODE - readings paused");
    // OLED already shows maintenance when toggled
    delay(300);
    return;
  }

  // MONITORING MODE

  // Trigger ultrasonic pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure echo (timeout 30 ms)
  unsigned long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0)
  {
    Serial.println("No echo detected");
    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, LOW);
    stopAlarm();
    drawNoEchoOLED();
    delay(300);
    return;
  }

  // Compute distance in cm -> range = [2, 400] with an accuracy of 0.3cm
  distanceCm = duration * SOUND_SPEED / 2.0;
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);

  String status;
  float currentLevel = distanceCm;

  // STATUSES
  // FULL
  if (distanceCm <= FULL_THRESHOLD_CM)
  {
    status = "FULL";
    Serial.println("Status: FULL");

    digitalWrite(greenLed, LOW);
    digitalWrite(redLed, HIGH);
    startAlarm(); // start beeping alarm (only first time)
  }

  // MEDIUM
  else if (distanceCm <= MEDIUM_THRESHOLD_CM)
  {
    status = "MEDIUM";
    Serial.println("Status: MEDIUM");

    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, LOW);
    stopAlarm();
  }

  // EMPTY
  else
  {
    status = "EMPTY";
    Serial.println("Status: EMPTY");

    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, LOW);
    stopAlarm();
  }

  // Update OLED
  drawMonitoringOLED(status, distanceCm);

  // Send telemetry every sendIntervalMs
  unsigned long now = millis();
  if (now - lastSent >= sendIntervalMs)
  {
    sendTelemetry(currentLevel, status);
    lastSent = now;
  }

  Serial.println("---------------------");
  delay(150);
}
