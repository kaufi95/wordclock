#include <TimeLib.h>
#include <Timezone.h>

#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <FastLED.h>

#include "src/dialekt.h"
#include "src/deutsch.h"

#include <NTPClient.h>

#define VERSION "3.2"

// define WiFi params
#define AP_SSID "WordClock-Setup"
#define AP_TIMEOUT 600  // seconds
#define DNS_NAME "wordclock"
#define HTTP_PORT 80

// define matrix params
#define MATRIX_WIDTH 11   // width of LED matrix
#define MATRIX_HEIGHT 10  // height of LED matrix + additional row for minute leds
#define LED_PIN 4         // define pin for LEDs
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT + 4)
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// define preferences namespace
#define PREFS_NAMESPACE "wordclock"

struct Config {
  uint8_t red;         // red component (0-255)
  uint8_t green;       // green component (0-255)
  uint8_t blue;        // blue component (0-255)
  uint8_t brightness;  // brightness
  String language;     // language
};

// create config object and set default values
Config config = { 255, 255, 255, 128, "dialekt" };  // white color by default

uint8_t lastMin;
bool timeSynced = false;
bool update = false;
bool wifiConnected = false;

// NTP timing
unsigned long nextTimeSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1 hour in milliseconds

// create preferences object
Preferences preferences;

// create WiFiManager
WiFiManager wm;

// create webserver
AsyncWebServer server(HTTP_PORT);

// NTP management
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// create FastLED array
CRGB leds[NUM_LEDS];

// define time change rules and timezone
TimeChangeRule atST = { "ST", Last, Sun, Mar, 2, 120 };  // UTC + 2 hours
TimeChangeRule atRT = { "RT", Last, Sun, Oct, 3, 60 };   // UTC + 1 hour
Timezone AT(atST, atRT);

void setup() {
  // enable serial output
  Serial.begin(115200);
  Serial.println("WordClock");
  Serial.println("v" + String(VERSION));
  Serial.println("by kaufi95");

  while (!LittleFS.begin(true)) {
    Serial.println("File system mount failed...");
    ESP.restart();
  }
  Serial.println("File system mounted");

  // load stored values from preferences
  Serial.println("Loading settings from preferences");
  loadSettings();

  Serial.println("initiating matrix");
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(config.brightness);
  FastLED.clear();
  FastLED.show();

  // Start WiFi connection
  startWiFiConnection();
  
  // Wait for WiFi connection
  Serial.println("Waiting for WiFi connection...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    onWiFiConnected();
    startNTP();
  }
}

// ------------------------------------------------------------
// main

void loop() {
  updateSettings();
  updateTime();
  displayTime();
  refreshMatrix(false);
  
  delay(1000);
}

void updateSettings() {
  if (!update) return;

  FastLED.setBrightness(config.brightness);
  refreshMatrix(true);

  storeSettings();
  update = false;
}

void displayTime() {
  if (!timeSynced) return;

  time_t timeUTC = timeClient.getEpochTime();
  time_t timeLocal = AT.toLocal(timeUTC);

  // Only display time when minute changes
  uint8_t currentMin = minute(timeLocal);
  static uint8_t lastDisplayedMin = 255;

  if (currentMin != lastDisplayedMin) {
    displayTimeInfo(timeLocal, "Local");
    lastDisplayedMin = currentMin;
  }
}

void updateTime() {
  if (!wifiConnected) {
    return;
  }

  if (millis() >= nextTimeSync) {
    timeSynced = false;
  }

  if (timeSynced) {
    return;
  }

  timeClient.update();
  if (!timeClient.isTimeSet()) {
    Serial.println("TimeClient not ready yet...");
    return;
  }

  time_t time_ntp = timeClient.getEpochTime();
  displayTimeInfo(time_ntp, "NTP");
  Serial.println("Time synced over NTP.");
  timeSynced = true;
  scheduleNextTimeSync();
}

void scheduleNextTimeSync() {
  nextTimeSync = millis() + NTP_SYNC_INTERVAL;
  Serial.println("Next NTP sync scheduled in " + String(NTP_SYNC_INTERVAL / 60000) + " minutes");
}

void printSettings() {
  Serial.println("Red:\t\t" + String(config.red));
  Serial.println("Green:\t\t" + String(config.green));
  Serial.println("Blue:\t\t" + String(config.blue));
  Serial.println("Brightness:\t" + String(config.brightness));
  Serial.println("Language:\t" + config.language);
}

// ------------------------------------------------------------
// wifi

void startWiFiConnection() {
  Serial.println("Starting WiFi connection...");
  //WiFi.setHostname(DNS_NAME);
  
  // Configure WiFiManager
  wm.setConfigPortalTimeout(AP_TIMEOUT);
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConnectTimeout(30);
  wm.setDebugOutput(false);
  
  // Start connection - this will create WiFiManager's internal webserver
  if (!wm.autoConnect(AP_SSID)) {
    Serial.println("Failed to start WiFi connection");
    ESP.restart();
  }
  
  Serial.println("WiFiManager connection process completed");
}

void onWiFiConnected() {
  Serial.println("\nWiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Clear display
  FastLED.clear();
  FastLED.show();

  // CRITICAL: Properly shutdown WiFiManager's webserver
  Serial.println("Shutting down WiFiManager webserver...");
  wm.setConfigPortalBlocking(true);  // Ensure blocking mode
  wm.stopConfigPortal();             // Stop the config portal and its webserver
  
  // Wait for WiFiManager cleanup - ESSENTIAL for resource cleanup
  Serial.println("Waiting for WiFiManager cleanup...");
  delay(3000);
  
  // Now safe to start our services
  Serial.println("Starting services...");
  startMDNS();
  startServer();
}

// WiFi processing functions above

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
  Serial.println("SSID: " + String(myWiFiManager->getConfigPortalSSID()));
}

void saveConfigCallback() {
  Serial.println("WiFi configuration saved");
}

void resetWiFiSettings() {
  Serial.println("Resetting WiFi settings...");
  wm.resetSettings();
  delay(1000);
  ESP.restart();
}

// ------------------------------------------------------------
// mdns

void startMDNS() {
  while (!MDNS.begin(DNS_NAME)) {
    Serial.println("mDNS responder not started yet...");
    delay(1000);
  }
  MDNS.addService("http", "tcp", 80);
  Serial.println("mDNS responder started");
}

// ------------------------------------------------------------
// webserver

void startServer() {
  Serial.println("Starting AsyncWebServer...");

  // Configure server with minimal handlers first
  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleConnect);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/resetwifi", HTTP_POST, handleResetWiFi);

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (request->url() == "/update") {
      handleUpdate(request, data);
    }
  });

  // Serve static files
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.serveStatic("/app.js", LittleFS, "/app.js");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");

  // Start server
  server.begin();
  Serial.println("AsyncWebServer started");
}

void startNTP() {
  // Initialize NTP client after WiFi is stable
  timeClient.begin();
  Serial.println("NTP client started");
  
  // Force initial time sync
  nextTimeSync = 0;
  Serial.println("NTP client started");
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->redirect("/index.html");
}

void handleConnect(AsyncWebServerRequest *request) {
  request->redirect("/index.html");
}

void handleStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;

  doc["red"] = config.red;
  doc["green"] = config.green;
  doc["blue"] = config.blue;
  doc["brightness"] = config.brightness;
  doc["language"] = config.language;

  String response;
  if (!serializeJson(doc, response)) {
    Serial.println("Failed to create response!");
  }

  request->send(200, "application/json", response);
}

void handleUpdate(AsyncWebServerRequest *request, uint8_t *data) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.println("Failed to deserialize json from update-request.");
    Serial.println(error.c_str());
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  config.red = (uint8_t)doc["red"];
  config.green = (uint8_t)doc["green"];
  config.blue = (uint8_t)doc["blue"];
  config.brightness = (uint8_t)doc["brightness"];
  config.language = String(doc["language"]);

  update = true;

  request->send(200, "text/plain", "ok");
}

void handleResetWiFi(AsyncWebServerRequest *request) {
  Serial.println("WiFi reset requested via web interface");
  request->send(200, "text/plain", "WiFi settings will be reset. Device restarting...");
  
  // Small delay to send response before restarting
  delay(1000);
  
  resetWiFiSettings();
}

// File handlers removed - using AsyncWebServer serveStatic instead

// ------------------------------------------------------------
// storage

void loadSettings() {
  preferences.begin(PREFS_NAMESPACE, true);  // read-only mode

  config.red = preferences.getUChar("red", 255);
  config.green = preferences.getUChar("green", 255);
  config.blue = preferences.getUChar("blue", 255);
  config.brightness = preferences.getUChar("brightness", 128);
  config.language = preferences.getString("language", "dialekt");

  preferences.end();

  printSettings();
}

void storeSettings() {
  preferences.begin(PREFS_NAMESPACE, false);  // read-write mode

  preferences.putUChar("red", config.red);
  preferences.putUChar("green", config.green);
  preferences.putUChar("blue", config.blue);
  preferences.putUChar("brightness", config.brightness);
  preferences.putString("language", config.language);

  preferences.end();

  Serial.println("Settings saved to preferences");
  printSettings();
}

// ------------------------------------------------------------
// wordclock logic

// clears, generates and fills pixels
void refreshMatrix(bool settingsChanged) {
  if (!timeSynced) return;

  time_t timeUTC = timeClient.getEpochTime();
  time_t time = AT.toLocal(timeUTC);
  if (lastMin != minute(time) || settingsChanged) {
    FastLED.clear();
    setPixels(time);
    FastLED.show();
    lastMin = minute(time);
  }
}

// converts time directly to LED array
void setPixels(time_t time) {
  if (config.language == "dialekt") {
    dialekt::timeToLeds(time, leds, config.red, config.green, config.blue);
  }
  if (config.language == "deutsch") {
    deutsch::timeToLeds(time, leds, config.red, config.green, config.blue);
  }
}

// ------------------------------------------------------------
// serial output

// display time information in readable format
void displayTimeInfo(time_t t, String component) {
  // Get weekday name
  String weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  String months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  Serial.print("┌─ " + component + " Time ─────────────────────────────────");
  Serial.println();

  // Print date with weekday
  Serial.print("│ Date: ");
  Serial.print(weekdays[weekday(t) - 1]);
  Serial.print(", ");
  Serial.print(months[month(t) - 1]);
  Serial.print(" ");
  if (day(t) < 10) Serial.print("0");
  Serial.print(day(t));
  Serial.print(", ");
  Serial.print(year(t));
  Serial.println();

  // Print time in 24h format
  Serial.print("│ Time: ");
  if (hour(t) < 10) Serial.print("0");
  Serial.print(hour(t));
  Serial.print(":");
  if (minute(t) < 10) Serial.print("0");
  Serial.print(minute(t));
  Serial.print(":");
  if (second(t) < 10) Serial.print("0");
  Serial.print(second(t));

  // Add 12h format for reference
  Serial.print(" (");
  int h12 = hour(t);
  String ampm = "AM";
  if (h12 == 0) h12 = 12;
  else if (h12 > 12) {
    h12 -= 12;
    ampm = "PM";
  } else if (h12 == 12) ampm = "PM";

  Serial.print(h12);
  Serial.print(":");
  if (minute(t) < 10) Serial.print("0");
  Serial.print(minute(t));
  Serial.print(" " + ampm + ")");
  Serial.println();

  Serial.println("└────────────────────────────────────────────────");
}
