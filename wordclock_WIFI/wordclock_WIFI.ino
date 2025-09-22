#include <TimeLib.h>
#include <Timezone.h>

#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>

#include <Adafruit_NeoPixel.h>

#include <ESP32Time.h>

#include "src/dialekt.h"
#include "src/deutsch.h"

#define VERSION "3.5"

// define WiFi params
#define AP_SSID "WordClock-Setup"
#define DNS_NAME "wordclock"

// define matrix params
#define LED_PIN 4         // define pin for LEDs
#define NUM_LEDS 114

// define preferences namespace
#define PREFS_NAMESPACE "wordclock"

struct Config {
  uint8_t red;         // red component (0-255)
  uint8_t green;       // green component (0-255)
  uint8_t blue;        // blue component (0-255)
  uint8_t brightness;  // brightness
  String language;     // language
  bool enabled;        // wordclock on/off state
};

// create config object and set default values
Config config = { 255, 255, 255, 128, "dialekt", true };  // white color, enabled by default

uint8_t lastMin;
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
AsyncWebServer server(80);

// NTP management
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// RTC management
ESP32Time rtc;

// create NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// define time change rules and timezone
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // UTC + 2 hours
TimeChangeRule CET = { "CET", Last, Sun, Oct, 3, 60 };   // UTC + 1 hour
Timezone AT(CEST, CET);

void setup() {
  Serial.begin(115200);
  Serial.print("WordClock");
  Serial.println(" v" + String(VERSION));
  Serial.println("by kaufi95");

  while (!LittleFS.begin(true)) {
    Serial.println("File system mount failed...");
    ESP.restart();
  }

  loadSettings();

  strip.begin();
  strip.setBrightness(config.brightness);
  strip.clear();
  strip.show();

  Serial.println("Initializing RTC");
  rtc.setTime(2, 0, 0, 1, 1, 2025);

  Serial.println("Starting WiFi connection...");
  WiFi.setHostname(DNS_NAME);
  wm.setConfigPortalTimeout(600);
  wm.setAPCallback(configModeCallback);
  wm.setConnectTimeout(30);
  wm.setDebugOutput(false);
  
  if (!wm.autoConnect(AP_SSID)) {
    Serial.println("Failed to start WiFi connection");
    ESP.restart();
  }
  
  Serial.println("WiFiManager connection process completed");
  
  // Wait for WiFi connection
  Serial.println("Waiting for WiFi connection...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    onWiFiConnected();
  }
}

// ------------------------------------------------------------
// main

void loop() {
  updateTime();
  refreshMatrix(update);
  updateSettings();
  delay(1000);
}

void updateSettings() {
  if (!update) return;
  storeSettings();
  update = false;
}

void updateTime() {
  if (!wifiConnected || millis() < nextTimeSync) {
    return;
  }

  timeClient.update();
  if (!timeClient.isTimeSet()) {
    Serial.println("TimeClient not ready yet...");
    return;
  }
  
  Serial.println("Time synced over NTP.");
  time_t time = timeClient.getEpochTime();
  
  // Update RTC with NTP time
  rtc.setTime(time);
  Serial.println("RTC synchronized with NTP time");
  
  displayTimeInfo(AT.toLocal(time));
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
  Serial.println("Enabled:\t" + String(config.enabled));
}

// ------------------------------------------------------------
// wifi

void onWiFiConnected() {
  Serial.println("WiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
   
  startNTP();
  startMDNS();
  startServer();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
  Serial.println("SSID: " + String(myWiFiManager->getConfigPortalSSID()));
}

void resetWiFiSettings() {
  Serial.println("Resetting WiFi settings and rebooting...");
  delay(1000);
  wm.resetSettings();
  ESP.restart();
}

// ------------------------------------------------------------
// services

void startNTP() {
  timeClient.begin();
  nextTimeSync = 0;
  Serial.println("NTP client started");
}

void startMDNS() {
  MDNS.begin(DNS_NAME);
  MDNS.addService("http", "tcp", 80);
  Serial.println("mDNS responder started");
}

void startServer() {
  // Configure server with minimal handlers first
  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleConnect);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/resetwifi", HTTP_POST, handleResetWiFi);

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (request->url() == "/update") {
      if (index + len == total) {
        handleUpdate(request, data, len);
      }
    }
  });

  // Serve static files
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.serveStatic("/app.js", LittleFS, "/app.js");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");

  // Start server
  server.begin();
  Serial.println("WebServer started");
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
  doc["enabled"] = config.enabled;

  String response;
  if (!serializeJson(doc, response)) {
    Serial.println("Failed to create response!");
  }

  request->send(200, "application/json", response);
}

void handleUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
  String jsonString;
  jsonString.reserve(len + 1);
  for (size_t i = 0; i < len; i++) {
    jsonString += (char)data[i];
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.println("Failed to deserialize json from update-request.");
    Serial.println(error.c_str());
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (doc.containsKey("red")) {
    config.red = (uint8_t)doc["red"];
  }
  if (doc.containsKey("green")) {
    config.green = (uint8_t)doc["green"];
  }
  if (doc.containsKey("blue")) {
    config.blue = (uint8_t)doc["blue"];
  }
  if (doc.containsKey("brightness")) {
    config.brightness = (uint8_t)doc["brightness"];
  }
  if (doc.containsKey("language")) {
    config.language = String(doc["language"]);
  }
  if (doc.containsKey("enabled")) {
    config.enabled = doc["enabled"];
  }

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

// ------------------------------------------------------------
// storage

void loadSettings() {
  preferences.begin(PREFS_NAMESPACE, true);  // read-only mode

  config.red = preferences.getUChar("red", 255);
  config.green = preferences.getUChar("green", 255);
  config.blue = preferences.getUChar("blue", 255);
  config.brightness = preferences.getUChar("brightness", 128);
  config.language = preferences.getString("language", "dialekt");
  config.enabled = preferences.getBool("enabled", true);

  preferences.end();

  Serial.println("Settings loaded from preferences");
  printSettings();
}

void storeSettings() {
  preferences.begin(PREFS_NAMESPACE, false);  // read-write mode

  preferences.putUChar("red", config.red);
  preferences.putUChar("green", config.green);
  preferences.putUChar("blue", config.blue);
  preferences.putUChar("brightness", config.brightness);
  preferences.putString("language", config.language);
  preferences.putBool("enabled", config.enabled);

  preferences.end();

  Serial.println("Settings saved to preferences");
  printSettings();
}

// ------------------------------------------------------------
// wordclock logic

void refreshMatrix(bool settingsChanged) {
  if (!config.enabled) {
    strip.clear();
    strip.show();
    return;
  }

  time_t timeUTC = rtc.getEpoch();
  time_t time = AT.toLocal(timeUTC);

  if (lastMin != minute(time) || settingsChanged) {
    strip.setBrightness(config.brightness);
    strip.clear();
    setPixels(time);
    strip.show();
    lastMin = minute(time);
  }
}

void setPixels(time_t time) {
  if (config.language == "dialekt") {
    dialekt::timeToLeds(time, &strip, config.red, config.green, config.blue);
  }
  if (config.language == "deutsch") {
    deutsch::timeToLeds(time, &strip, config.red, config.green, config.blue);
  }
}

// ------------------------------------------------------------
// display time information in readable format

void displayTimeInfo(time_t t) {
  // Get weekday name
  String weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  String months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  Serial.println("┌─ Local Time ───────────────────────────────────");

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
  Serial.println();

  Serial.println("└────────────────────────────────────────────────");
}