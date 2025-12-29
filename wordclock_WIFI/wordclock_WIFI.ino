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

#define VERSION "4.1"

// define WiFi params
#define AP_SSID "WordClock-Setup"
#define DNS_NAME "wordclock"

// define matrix params
#define LED_PIN 4  // define pin for LEDs
#define NUM_LEDS 114

// define preferences namespace
#define PREFS_NAMESPACE "wordclock"

// Transition animation types
enum TransitionType {
  TRANSITION_NONE = 0,    // No animation, instant change
  TRANSITION_FADE = 1,    // Fade out old, fade in new
  TRANSITION_WIPE = 2,    // Wipe from left to right
  TRANSITION_SPARKLE = 3  // Random sparkle effect
};

// ES IST/ES ISCH display modes
enum PrefixMode {
  PREFIX_ALWAYS = 0,  // Always show ES IST/ES ISCH
  PREFIX_RANDOM = 1,  // Randomly show or hide
  PREFIX_OFF = 2      // Never show ES IST/ES ISCH
};

struct Config {
  uint8_t red;              // red component (0-255)
  uint8_t green;            // green component (0-255)
  uint8_t blue;             // blue component (0-255)
  uint8_t brightness;       // brightness percentage (5-100)
  String language;          // language
  bool enabled;             // wordclock on/off state
  uint8_t transition;       // transition animation type
  uint8_t prefixMode;       // ES IST/ES ISCH display mode
  uint8_t transitionSpeed;  // transition speed: 1=slow, 2=medium, 3=fast
  bool superBright;         // superbright mode: false=5-80%, true=5-100%
};

// create config object and set default values (brightness is 50% by default)
Config config = { 255, 255, 255, 50, "dialekt", true, TRANSITION_FADE, PREFIX_ALWAYS, 2, false };

// Status states for animation
enum StatusState {
  STATUS_BOOT = 1,  // 1 LED: Booting up
  STATUS_WIFI = 2,  // 2 LEDs: Connecting to WiFi
  STATUS_NTP = 3,   // 3 LEDs: Syncing time via NTP
  STATUS_READY = 0  // 0 LEDs: Ready, showing time
};

uint8_t lastMin = 255;  // Initialize to 255 to prevent animation on first display
bool update = false;
bool playPreviewAnimation = false;
bool wifiConnected = false;
bool timeIsSynced = false;
StatusState currentStatus = STATUS_BOOT;

// NTP timing
unsigned long nextTimeSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;  // 1 hour in milliseconds
const unsigned long NTP_RETRY_INTERVAL = 30000;   // 30 seconds for failed attempts
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;  // Check WiFi every 10 seconds

// Minute LED positions (corner LEDs on typical word clocks)
const uint8_t MINUTE_LEDS[] = { 110, 111, 112, 113 };  // Adjust these to your LED layout
const uint8_t NUM_MINUTE_LEDS = 4;

// Task handles for FreeRTOS
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// create preferences object
Preferences preferences;

// create WiFiManager
WiFiManager wm;

// create webserver
AsyncWebServer server(80);
AsyncEventSource events("/events");

// NTP management
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// RTC management
ESP32Time rtc;

// create NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// define time change rules and timezone
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // UTC + 2 hours
TimeChangeRule CET = { "CET", Last, Sun, Oct, 3, 60 };     // UTC + 1 hour
Timezone AT(CEST, CET);

// ------------------------------------------------------------
// Brightness mapping function
// Maps brightness percentage (5-100) to actual LED brightness (0-255)
// When superBright is OFF: 5-100% -> 5-80% of LED brightness (0.05*255 to 0.8*255)
// When superBright is ON: 5-100% -> 5-100% of LED brightness (0.05*255 to 1.0*255)
uint8_t mapBrightnessPercentage(uint8_t percentage, bool superBright) {
  // Clamp percentage to 5-100 range
  if (percentage < 5) percentage = 5;
  if (percentage > 100) percentage = 100;

  if (superBright) {
    // SuperBright mode: 5-100% -> 0.05*255 to 1.0*255 (13 to 255)
    return map(percentage, 5, 100, 13, 255);
  } else {
    // Normal mode: 5-100% -> 0.05*255 to 0.8*255 (13 to 204)
    return map(percentage, 5, 100, 13, 192);
  }
}

// ------------------------------------------------------------
// Safe serial printing functions to prevent task preemption corruption

void serialPrint(const String &str) {
  Serial.print(str);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrint(const __FlashStringHelper *str) {
  Serial.print(str);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrint(int val) {
  Serial.print(val);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrint(unsigned long val) {
  Serial.print(val);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrintln(const String &str) {
  Serial.println(str);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrintln(const __FlashStringHelper *str) {
  Serial.println(str);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrintln(int val) {
  Serial.println(val);
  Serial.flush();
  vTaskDelay(1);
}

void serialPrintln(unsigned long val) {
  Serial.println(val);
  Serial.flush();
  vTaskDelay(1);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("WordClock v" VERSION " by kaufi95"));

  while (!LittleFS.begin(true)) {
    Serial.println(F("File system mount failed..."));
    ESP.restart();
  }
  Serial.println(F("File system mounted"));
  delay(1000);

  loadSettings();

  strip.begin();
  strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
  strip.clear();
  strip.show();
  Serial.println("LED strip initialized");
  delay(1000);

  Serial.println(F("Initializing RTC with placeholder time"));
  rtc.setTime(0, 0, 0, 1, 1, 2020);

  // Configure WiFiManager (but don't connect yet)
  WiFi.setHostname(DNS_NAME);
  WiFi.mode(WIFI_STA);  // Force station mode
  wm.setConfigPortalTimeout(600);
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(wifiSaveCallback);
  wm.setConnectTimeout(30);
  wm.setDebugOutput(false);
  wm.setConnectRetries(1);
  wm.setConnectTimeout(5);

  // Create tasks - they will handle WiFi and display
  xTaskCreatePinnedToCore(
    displayTask,
    "DisplayTask",
    4096,
    NULL,
    2,
    &displayTaskHandle,
    1);

  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    &networkTaskHandle,
    0);

  delay(1000);
  Serial.println(F("Tasks created"));
}

// ------------------------------------------------------------
// main

void loop() {
  // Tasks handle everything now, so loop just sleeps
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void updateTime() {
  if (!wifiConnected) {
    return;
  }

  unsigned long now = millis();
  if (now < nextTimeSync) {
    return;
  }

  currentStatus = STATUS_NTP;

  timeClient.forceUpdate();
  bool updateSuccess = timeClient.isTimeSet();

  if (updateSuccess) {
    time_t time = timeClient.getEpochTime();
    delay(500);

    if (time > 1577836800) {
      rtc.setTime(time);
      timeIsSynced = true;
      currentStatus = STATUS_READY;
      displayTimeInfo(AT.toLocal(time));

      nextTimeSync = now + NTP_SYNC_INTERVAL;
      String msg = "Next NTP sync in " + String((int)(NTP_SYNC_INTERVAL / 60000)) + " minutes";
      serialPrintln(msg);
    } else {
      serialPrintln(F("NTP returned invalid time"));
      scheduleRetry();
    }
  } else {
    serialPrintln(F("NTP sync failed"));
    scheduleRetry();
  }
}

void scheduleRetry() {
  // Retry failed NTP syncs more frequently
  nextTimeSync = millis() + NTP_RETRY_INTERVAL;
  String msg = "Retry NTP sync in " + String((int)(NTP_RETRY_INTERVAL / 1000)) + " seconds";
  serialPrintln(msg);
}

void printSettings() {
  String line1 = "Red: " + String(config.red) + ", Green: " + String(config.green) + ", Blue: " + String(config.blue);
  serialPrintln(line1);

  String line2 = "Brightness: " + String(config.brightness) + ", Enabled: " + String(config.enabled);
  serialPrintln(line2);

  String line3 = "Language: " + config.language + ", PrefixMode: " + String(config.prefixMode);
  serialPrintln(line3);

  String line4 = "Transition: " + String(config.transition) + ", Speed: " + String(config.transitionSpeed);
  serialPrintln(line4);
}

// ------------------------------------------------------------
// wifi

void checkWiFiConnection() {
  unsigned long now = millis();

  // Only check periodically to avoid overhead
  if (now - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
    return;
  }
  lastWiFiCheck = now;

  // Check current WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      // WiFi just reconnected
      wifiConnected = true;
      currentStatus = STATUS_NTP;  // WiFi back, waiting for NTP
      serialPrintln(F("WiFi reconnected!"));
      String ipMsg = "IP address: " + WiFi.localIP().toString();
      serialPrintln(ipMsg);

      // Trigger immediate NTP sync
      nextTimeSync = 0;
    }
  } else {
    if (wifiConnected) {
      // WiFi just disconnected
      wifiConnected = false;
      timeIsSynced = false;
      currentStatus = STATUS_WIFI;  // Lost WiFi, trying to reconnect
      serialPrintln(F("WiFi connection lost! Attempting to reconnect..."));
    }

    // Try to reconnect
    WiFi.reconnect();
  }
}

void onWiFiConnected() {
  serialPrintln(F("WiFi connected successfully!"));
  String ipMsg = "IP address: " + WiFi.localIP().toString();
  serialPrintln(ipMsg);

  startNTP();
  startMDNS();
  startServer();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered config mode"));
  Serial.print(F("AP IP: "));
  Serial.println(WiFi.softAPIP());
  Serial.print(F("SSID: "));
  Serial.println(myWiFiManager->getConfigPortalSSID());
  delay(250);

  // Keep animation running during captive portal
  currentStatus = STATUS_WIFI;
}

// Callback that runs during WiFi connection attempts in captive portal
void wifiSaveCallback() {
  Serial.println(F("WiFi credentials saved, rebooting for clean start..."));
  delay(1000);
  ESP.restart();
}

void resetWiFiSettings() {
  Serial.println(F("Resetting WiFi settings and rebooting..."));
  delay(1000);
  wm.resetSettings();
  ESP.restart();
}

// ------------------------------------------------------------
// services

void startNTP() {
  timeClient.setPoolServerName("pool.ntp.org");
  timeClient.setTimeOffset(0);
  timeClient.begin();
  delay(500);
  nextTimeSync = 0;
  serialPrintln(F("NTP client started, will sync immediately"));
}

void startMDNS() {
  MDNS.begin(DNS_NAME);
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("wordclock", "tcp", 80);
  delay(500);
  serialPrintln(F("mDNS responder started"));
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

  // Add Server-Sent Events handler
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("SSE Client reconnected! Last message ID: %u\n", client->lastId());
      Serial.flush();
      vTaskDelay(1);
    } else {
      serialPrintln(F("SSE Client connected"));
    }
  });
  server.addHandler(&events);

  // Serve static files with caching headers
  server.serveStatic("/index.html", LittleFS, "/index.html")
    .setCacheControl("max-age=86400");
  server.serveStatic("/app.js", LittleFS, "/app.js")
    .setCacheControl("max-age=86400");
  server.serveStatic("/styles.css", LittleFS, "/styles.css")
    .setCacheControl("max-age=86400");

  // Start server
  server.begin();
  delay(500);
  serialPrintln(F("WebServer started"));
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->redirect("/index.html");
}

void handleConnect(AsyncWebServerRequest *request) {
  request->redirect("/index.html");
}

void handleStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<256> doc;

  doc["red"] = config.red;
  doc["green"] = config.green;
  doc["blue"] = config.blue;
  doc["brightness"] = config.brightness;
  doc["language"] = config.language;
  doc["enabled"] = config.enabled;
  doc["superBright"] = config.superBright;
  doc["transition"] = config.transition;
  doc["prefixMode"] = config.prefixMode;
  doc["transitionSpeed"] = config.transitionSpeed;

  String response;
  response.reserve(128);
  if (!serializeJson(doc, response)) {
    Serial.println(F("Failed to create response!"));
  }

  request->send(200, "application/json", response);
  delay(250);
}

void handleUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.println(F("Failed to deserialize json from update-request."));
    Serial.println(error.c_str());
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (doc.containsKey("red") || doc.containsKey("green") || doc.containsKey("blue")) {
    if (doc.containsKey("red"))
      config.red = (uint8_t)doc["red"];
    if (doc.containsKey("green"))
      config.green = (uint8_t)doc["green"];
    if (doc.containsKey("blue"))
      config.blue = (uint8_t)doc["blue"];
    String msg = "RGB: " + String(config.red) + "/" + String(config.green) + "/" + String(config.blue);
    serialPrintln(msg);
  }
  if (doc.containsKey("brightness")) {
    config.brightness = (uint8_t)doc["brightness"];
    uint8_t actualBrightness = mapBrightnessPercentage(config.brightness, config.superBright);
    String msg = "Brightness: " + String(config.brightness) + "% -> " + String(actualBrightness);
    serialPrintln(msg);
  }
  if (doc.containsKey("language")) {
    config.language = String(doc["language"]);
    String msg = "Language: " + config.language;
    serialPrintln(msg);
  }
  if (doc.containsKey("enabled")) {
    config.enabled = doc["enabled"];
    String msg = "Enabled: " + String(config.enabled);
    serialPrintln(msg);
  }
  if (doc.containsKey("superBright")) {
    config.superBright = doc["superBright"];
    String msg = "SuperBright: " + String(config.superBright);
    serialPrintln(msg);
  }
  if (doc.containsKey("transition")) {
    uint8_t newTransition = (uint8_t)doc["transition"];
    bool transitionChanged = (config.transition != newTransition);
    config.transition = newTransition;
    String msg = "Transition: " + String(config.transition);
    serialPrintln(msg);

    // Play preview if:
    // 1. Transition changed (switching to a different animation), OR
    // 2. User clicked the same transition button (force preview parameter)
    if (currentStatus == STATUS_READY) {
      if (transitionChanged) {
        playPreviewAnimation = true;
      } else if (doc.containsKey("forcePreview") && doc["forcePreview"]) {
        playPreviewAnimation = true;
      }
    }
  }
  if (doc.containsKey("prefixMode")) {
    config.prefixMode = (uint8_t)doc["prefixMode"];
    String msg = "PrefixMode: " + String(config.prefixMode);
    serialPrintln(msg);
  }
  if (doc.containsKey("transitionSpeed")) {
    config.transitionSpeed = (uint8_t)doc["transitionSpeed"];
    String msg = "Speed: " + String(config.transitionSpeed);
    serialPrintln(msg);
  }

  update = true;

  // Store settings immediately
  storeSettings();

  // Broadcast settings immediately to all connected clients
  broadcastSettings();

  request->send(200, "text/plain", "ok");
  delay(250);
}

void handleResetWiFi(AsyncWebServerRequest *request) {
  Serial.println(F("WiFi reset requested via web interface"));
  request->send(200, "text/plain", "WiFi settings will be reset. Device restarting...");

  // Small delay to send response before restarting
  delay(1000);

  resetWiFiSettings();
}

// ------------------------------------------------------------
// storage

void loadSettings() {
  preferences.begin(PREFS_NAMESPACE, true);

  config.red = preferences.getUChar("red", 255);
  config.green = preferences.getUChar("green", 255);
  config.blue = preferences.getUChar("blue", 255);

  // Load brightness and convert from old format (0-255) to new percentage (5-100) if needed
  uint8_t loadedBrightness = preferences.getUChar("brightness", 50);
  if (loadedBrightness > 100) {
    // Old format detected (0-255), convert to percentage
    config.brightness = map(loadedBrightness, 16, 255, 5, 100);
    config.brightness = constrain(config.brightness, 5, 100);
  } else {
    // New format (5-100)
    config.brightness = loadedBrightness;
  }

  config.language = preferences.getString("language", "dialekt");
  config.enabled = preferences.getBool("enabled", true);
  config.superBright = preferences.getBool("superBright", false);

  // Load transition with validation
  uint8_t loadedTransition = preferences.getUChar("transition", TRANSITION_NONE);
  if (loadedTransition > TRANSITION_SPARKLE) {
    config.transition = TRANSITION_NONE;  // Invalid value, use default
  } else {
    config.transition = loadedTransition;
  }

  config.prefixMode = preferences.getUChar("prefixMode", PREFIX_ALWAYS);

  // Load transition speed with validation
  uint8_t loadedSpeed = preferences.getUChar("transSpeed", 2);
  if (loadedSpeed > 4) {
    config.transitionSpeed = 2;  // Invalid value, use default (medium)
  } else {
    config.transitionSpeed = loadedSpeed;
  }

  preferences.end();

  delay(250);
  Serial.println(F("Settings loaded from preferences"));
  printSettings();
}

void storeSettings() {
  preferences.begin(PREFS_NAMESPACE, false);

  preferences.putUChar("red", config.red);
  preferences.putUChar("green", config.green);
  preferences.putUChar("blue", config.blue);
  preferences.putUChar("brightness", config.brightness);
  preferences.putString("language", config.language);
  preferences.putBool("enabled", config.enabled);
  preferences.putBool("superBright", config.superBright);
  preferences.putUChar("transition", config.transition);
  preferences.putUChar("prefixMode", config.prefixMode);
  preferences.putUChar("transSpeed", config.transitionSpeed);

  preferences.end();
}

void broadcastSettings() {
  StaticJsonDocument<256> doc;

  doc["red"] = config.red;
  doc["green"] = config.green;
  doc["blue"] = config.blue;
  doc["brightness"] = config.brightness;
  doc["language"] = config.language;
  doc["enabled"] = config.enabled;
  doc["superBright"] = config.superBright;
  doc["transition"] = config.transition;
  doc["prefixMode"] = config.prefixMode;
  doc["transitionSpeed"] = config.transitionSpeed;

  String response;
  response.reserve(128);
  serializeJson(doc, response);

  events.send(response.c_str(), "settings", millis());
}

// ------------------------------------------------------------
// wordclock logic

// Helper function to get delay values based on transition speed
// Speed 0 (Extra Slow): extra long delays, Speed 1 (Very Slow): very long delays, Speed 2 (Medium): normal delays, Speed 3 (Fast): short delays, Speed 4 (Very Fast): very short delays
void getTransitionDelays(int &fadeDelay, int &wipeDelay, int &sparkleDelay, int &pauseDelay) {
  switch (config.transitionSpeed) {
    case 0:  // Extra Slow
      fadeDelay = 60;
      wipeDelay = 20;
      sparkleDelay = 30;
      pauseDelay = 300;
      break;
    case 1:  // Very Slow
      fadeDelay = 40;
      wipeDelay = 15;
      sparkleDelay = 20;
      pauseDelay = 200;
      break;
    case 3:  // Fast
      fadeDelay = 8;
      wipeDelay = 2;
      sparkleDelay = 5;
      pauseDelay = 50;
      break;
    case 4:  // Very Fast
      fadeDelay = 5;
      wipeDelay = 1;
      sparkleDelay = 3;
      pauseDelay = 30;
      break;
    case 2:  // Medium (default)
    default:
      fadeDelay = 15;
      wipeDelay = 5;
      sparkleDelay = 10;
      pauseDelay = 100;
      break;
  }
}

// Helper functions for transition animations
void fadeOut(int fadeDelay) {
  uint8_t actualBrightness = mapBrightnessPercentage(config.brightness, config.superBright);
  for (int brightness = actualBrightness; brightness >= 0; brightness -= 8) {
    strip.setBrightness(brightness);
    strip.show();
    delay(fadeDelay);
  }
  strip.clear();
  strip.show();
}

void fadeIn(uint32_t *colors, int fadeDelay) {
  uint8_t actualBrightness = mapBrightnessPercentage(config.brightness, config.superBright);
  for (int brightness = 8; brightness <= actualBrightness; brightness += 8) {
    strip.setBrightness(brightness);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, colors[i]);
    }
    strip.show();
    delay(fadeDelay);
  }

  // Final display at exact target brightness
  strip.setBrightness(actualBrightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, colors[i]);
  }
  strip.show();
}

// Transition animation functions
void fadeTransition(time_t time, String *timeString) {
  int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
  getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);

  // Fade out
  fadeOut(fadeDelay);
  delay(pauseDelay);

  // Set new time pattern
  strip.setBrightness(255);
  setPixels(time, timeString);

  // Store colors
  uint32_t tempColors[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    tempColors[i] = strip.getPixelColor(i);
  }

  // Fade in
  fadeIn(tempColors, fadeDelay);
}

void wipeTransition(time_t time, String *timeString) {
  int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
  getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0, 0, 0);
    if (i % 3 == 0) {
      strip.show();
      delay(wipeDelay);
    }
  }
  strip.show();

  strip.clear();
  setPixels(time, timeString);

  uint32_t tempColors[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    tempColors[i] = strip.getPixelColor(i);
  }

  strip.clear();
  strip.show();

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, tempColors[i]);
    if (i % 3 == 0) {
      strip.show();
      delay(wipeDelay);
    }
  }

  strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
  strip.show();
}

void sparkleTransition(time_t time, String *timeString) {
  int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
  getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);

  bool ledsOff[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    ledsOff[i] = false;
  }

  int ledsRemaining = NUM_LEDS;
  while (ledsRemaining > 0) {
    int randomLed = random(NUM_LEDS);
    if (!ledsOff[randomLed]) {
      strip.setPixelColor(randomLed, 0, 0, 0);
      ledsOff[randomLed] = true;
      ledsRemaining--;

      if (ledsRemaining % 5 == 0) {
        strip.show();
        delay(sparkleDelay);
      }
    }
  }
  strip.show();

  strip.clear();
  setPixels(time, timeString);

  uint32_t tempColors[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    tempColors[i] = strip.getPixelColor(i);
    ledsOff[i] = false;
  }

  strip.clear();
  strip.show();

  ledsRemaining = NUM_LEDS;
  while (ledsRemaining > 0) {
    int randomLed = random(NUM_LEDS);
    if (!ledsOff[randomLed]) {
      strip.setPixelColor(randomLed, tempColors[randomLed]);
      ledsOff[randomLed] = true;
      ledsRemaining--;

      if (ledsRemaining % 5 == 0) {
        strip.show();
        delay(sparkleDelay);
      }
    }
  }

  strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
  strip.show();
}

void playTransition(time_t time, String *timeString) {
  switch (config.transition) {
    case TRANSITION_NONE:
      // No animation, just update directly
      strip.clear();
      setPixels(time, timeString);
      strip.show();
      break;

    case TRANSITION_FADE:
      fadeTransition(time, timeString);
      break;

    case TRANSITION_WIPE:
      wipeTransition(time, timeString);
      break;

    case TRANSITION_SPARKLE:
      sparkleTransition(time, timeString);
      break;

    default:
      // Unknown transition, fall back to no animation
      strip.clear();
      setPixels(time, timeString);
      strip.show();
      break;
  }
}

void refreshMatrix(bool settingsChanged) {
  static bool firstDisplay = true;
  static bool wasEnabled = true;                      // Track previous enabled state
  static uint8_t lastBrightness = config.brightness;  // Track previous brightness percentage
  static bool lastSuperBright = config.superBright;   // Track previous superbright mode

  // Handle power OFF - simple fade out regardless of transition type
  if (!config.enabled && wasEnabled) {
    int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
    getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);
    fadeOut(fadeDelay);
    wasEnabled = false;
    return;
  }

  if (!config.enabled) {
    strip.clear();
    strip.show();
    return;
  }

  // Handle power ON - use the configured transition
  if (config.enabled && !wasEnabled) {
    wasEnabled = true;

    if (currentStatus == STATUS_READY) {
      time_t timeUTC = rtc.getEpoch();
      time_t time = AT.toLocal(timeUTC);
      String timeString;

      // Set pixels and store colors
      strip.setBrightness(255);
      setPixels(time, &timeString);
      uint32_t tempColors[NUM_LEDS];
      for (int i = 0; i < NUM_LEDS; i++) {
        tempColors[i] = strip.getPixelColor(i);
      }

      int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
      getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);

      // Just fade in (simpler and cleaner)
      fadeIn(tempColors, fadeDelay);

      lastMin = minute(time);
      lastBrightness = config.brightness;
      lastSuperBright = config.superBright;
      serialPrintln(timeString);
      update = false;
      return;
    }

    firstDisplay = true;
  }

  if (currentStatus != STATUS_READY) {
    showStatusAnimation();
    firstDisplay = true;  // Reset flag when not ready
    return;
  }

  time_t timeUTC = rtc.getEpoch();
  time_t time = AT.toLocal(timeUTC);
  uint8_t currentMin = minute(time);

  // Play preview animation when user selects a transition
  if (playPreviewAnimation) {
    playPreviewAnimation = false;
    String timeString;
    strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
    playTransition(time, &timeString);
    Serial.println(F("Preview animation played"));
    return;
  }

  // Force initial display when first becoming ready
  if (firstDisplay) {
    String timeString;
    strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
    strip.clear();
    setPixels(time, &timeString);
    strip.show();
    lastMin = currentMin;
    serialPrintln(timeString);
    firstDisplay = false;
    return;
  }

  // Handle brightness or superbright change with smooth rolling transition
  if (settingsChanged && (config.brightness != lastBrightness || config.superBright != lastSuperBright)) {
    uint8_t lastActualBrightness = mapBrightnessPercentage(lastBrightness, lastSuperBright);
    uint8_t targetActualBrightness = mapBrightnessPercentage(config.brightness, config.superBright);

    // Smooth rolling brightness change in steps of 20
    int step = (targetActualBrightness > lastActualBrightness) ? 20 : -20;
    int currentBrightness = lastActualBrightness;

    while ((step > 0 && currentBrightness < targetActualBrightness) || (step < 0 && currentBrightness > targetActualBrightness)) {
      currentBrightness += step;

      // Clamp to target brightness
      if (step > 0 && currentBrightness > targetActualBrightness) {
        currentBrightness = targetActualBrightness;
      } else if (step < 0 && currentBrightness < targetActualBrightness) {
        currentBrightness = targetActualBrightness;
      }

      strip.setBrightness(currentBrightness);
      strip.show();
      delay(30);  // Delay between brightness steps
    }

    // Final brightness
    strip.setBrightness(targetActualBrightness);
    strip.show();

    lastBrightness = config.brightness;
    lastSuperBright = config.superBright;
    update = false;  // Clear update flag after processing brightness change
    return;
  }

  if (lastMin != currentMin) {
    String timeString;
    strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
    playTransition(time, &timeString);
    lastMin = currentMin;
    lastBrightness = config.brightness;
    lastSuperBright = config.superBright;
    serialPrintln(timeString);
  } else if (settingsChanged && config.brightness == lastBrightness && config.superBright == lastSuperBright) {
    // Only update display for non-brightness changes (color, language, etc.)
    strip.setBrightness(mapBrightnessPercentage(config.brightness, config.superBright));
    strip.clear();
    setPixels(time, nullptr);  // Don't build string for settings-only changes
    strip.show();
    update = false;  // Clear update flag after processing settings change
  }
}

void showStatusAnimation() {
  static unsigned long lastUpdate = 0;
  static uint8_t blinkBrightness = 3;
  static int8_t direction = 1;
  static StatusState lastStatus = STATUS_BOOT;

  // Reset animation when status changes
  if (lastStatus != currentStatus) {
    blinkBrightness = 3;
    direction = 1;
    lastStatus = currentStatus;
  }

  unsigned long now = millis();
  if (now - lastUpdate > 50) {
    // Update blinking brightness
    int16_t newBrightness = blinkBrightness + direction;

    if (newBrightness >= 25) {
      blinkBrightness = 25;
      direction = -1;
    } else if (newBrightness <= 3) {
      blinkBrightness = 3;
      direction = 1;
    } else {
      blinkBrightness = newBrightness;
    }

    strip.clear();
    strip.setBrightness(255);  // Full brightness for status LEDs

    // Progressive animation:
    // STATUS_BOOT (1): LED 0 blinks
    // STATUS_WIFI (2): LED 0 solid, LED 1 blinks
    // STATUS_NTP (3):  LED 0-1 solid, LED 2 blinks

    if (currentStatus == STATUS_BOOT) {
      // Boot: Only LED 0 blinks
      uint8_t r = (config.red * blinkBrightness) / 25;
      uint8_t g = (config.green * blinkBrightness) / 25;
      uint8_t b = (config.blue * blinkBrightness) / 25;
      strip.setPixelColor(MINUTE_LEDS[0], r, g, b);
    } else if (currentStatus == STATUS_WIFI) {
      // WiFi: LED 0 solid, LED 1 blinks
      strip.setPixelColor(MINUTE_LEDS[0], config.red, config.green, config.blue);
      uint8_t r = (config.red * blinkBrightness) / 25;
      uint8_t g = (config.green * blinkBrightness) / 25;
      uint8_t b = (config.blue * blinkBrightness) / 25;
      strip.setPixelColor(MINUTE_LEDS[1], r, g, b);
    } else if (currentStatus == STATUS_NTP) {
      // NTP: LED 0-1 solid, LED 2 blinks
      strip.setPixelColor(MINUTE_LEDS[0], config.red, config.green, config.blue);
      strip.setPixelColor(MINUTE_LEDS[1], config.red, config.green, config.blue);
      uint8_t r = (config.red * blinkBrightness) / 25;
      uint8_t g = (config.green * blinkBrightness) / 25;
      uint8_t b = (config.blue * blinkBrightness) / 25;
      strip.setPixelColor(MINUTE_LEDS[2], r, g, b);
    }

    strip.show();
    lastUpdate = now;
  }
}

void setPixels(time_t time, String *timeString) {
  if (config.language == "dialekt") {
    dialekt::timeToLeds(time, &strip, config.red, config.green, config.blue, config.prefixMode, timeString);
  }
  if (config.language == "deutsch") {
    deutsch::timeToLeds(time, &strip, config.red, config.green, config.blue, config.prefixMode, timeString);
  }
}

// ------------------------------------------------------------
// FreeRTOS Tasks

// Display Task - Runs on Core 1 (default Arduino core)
// Handles LED matrix updates and animations at high frequency
void displayTask(void *parameter) {
  static bool lastUpdate = false;

  for (;;) {
    // Detect rising edge of update flag - only refresh once when settings change
    bool settingsChanged = update && !lastUpdate;
    lastUpdate = update;

    refreshMatrix(settingsChanged);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void networkTask(void *parameter) {
  currentStatus = STATUS_WIFI;

  if (!wm.autoConnect(AP_SSID)) {
    serialPrintln(F("Failed to connect to WiFi"));
    ESP.restart();
  }

  // Wait for WiFi to be fully connected
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  wifiConnected = true;
  currentStatus = STATUS_NTP;  // Now waiting for NTP
  onWiFiConnected();

  // Main network task loop
  for (;;) {
    checkWiFiConnection();
    updateTime();

    vTaskDelay(pdMS_TO_TICKS(1000));  // Run every second
  }
}

// ------------------------------------------------------------
// display time information in readable format

void displayTimeInfo(time_t t) {
  const char *weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  // Build complete message in a single line
  String msg = "Local Time: ";
  msg += weekdays[weekday(t) - 1];
  msg += ", ";
  msg += months[month(t) - 1];
  msg += " ";
  if (day(t) < 10)
    msg += "0";
  msg += String(day(t));
  msg += ", ";
  msg += String(year(t));
  msg += " - ";
  if (hour(t) < 10)
    msg += "0";
  msg += String(hour(t));
  msg += ":";
  if (minute(t) < 10)
    msg += "0";
  msg += String(minute(t));
  msg += ":";
  if (second(t) < 10)
    msg += "0";
  msg += String(second(t));

  serialPrintln(msg);
}