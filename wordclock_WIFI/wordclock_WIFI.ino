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

// Optional: ArduinoOTA for wireless updates
// Uncomment the lines below if you want OTA support
// #include <ArduinoOTA.h>
// #define ENABLE_OTA

// Optional: Watchdog timer for auto-recovery from hangs
// Uncomment the lines below to enable watchdog
// #include <esp_task_wdt.h>
// #define ENABLE_WATCHDOG
// #define WDT_TIMEOUT 30  // Watchdog timeout in seconds

#include "src/dialekt.h"
#include "src/deutsch.h"

#define VERSION "4"

// define WiFi params
#define AP_SSID "WordClock-Setup"
#define DNS_NAME "wordclock"

// define matrix params
#define LED_PIN 4         // define pin for LEDs
#define NUM_LEDS 114

// define preferences namespace
#define PREFS_NAMESPACE "wordclock"

// Transition animation types
enum TransitionType {
  TRANSITION_NONE = 0,     // No animation, instant change
  TRANSITION_FADE = 1,     // Fade out old, fade in new
  TRANSITION_WIPE = 2,     // Wipe from left to right
  TRANSITION_SPARKLE = 3   // Random sparkle effect
};

// ES IST/ES ISCH display modes
enum PrefixMode {
  PREFIX_ALWAYS = 0,   // Always show ES IST/ES ISCH
  PREFIX_RANDOM = 1,   // Randomly show or hide
  PREFIX_OFF = 2       // Never show ES IST/ES ISCH
};

struct Config {
  uint8_t red;              // red component (0-255)
  uint8_t green;            // green component (0-255)
  uint8_t blue;             // blue component (0-255)
  uint8_t brightness;       // brightness
  String language;          // language
  bool enabled;             // wordclock on/off state
  uint8_t transition;       // transition animation type
  uint8_t prefixMode;       // ES IST/ES ISCH display mode
  uint8_t transitionSpeed;  // transition speed: 1=slow, 2=medium, 3=fast
};

// create config object and set default values
Config config = { 255, 255, 255, 128, "dialekt", true, TRANSITION_FADE, PREFIX_ALWAYS, 2 };

// Status states for animation
enum StatusState {
  STATUS_BOOT = 1,        // 1 LED: Booting up
  STATUS_WIFI = 2,        // 2 LEDs: Connecting to WiFi
  STATUS_NTP = 3,         // 3 LEDs: Syncing time via NTP
  STATUS_READY = 0        // 0 LEDs: Ready, showing time
};

uint8_t lastMin = 255;  // Initialize to 255 to prevent animation on first display
bool update = false;
bool playPreviewAnimation = false;
bool wifiConnected = false;
bool timeIsSynced = false;
StatusState currentStatus = STATUS_BOOT;

// NTP timing
unsigned long nextTimeSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000; // 1 hour in milliseconds
const unsigned long NTP_RETRY_INTERVAL = 30000; // 30 seconds for failed attempts
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // Check WiFi every 10 seconds

// Minute LED positions (corner LEDs on typical word clocks)
const uint8_t MINUTE_LEDS[] = {110, 111, 112, 113};  // Adjust these to your LED layout
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
  Serial.println(F("WordClock v" VERSION " by kaufi95"));

  while (!LittleFS.begin(true)) {
    Serial.println(F("File system mount failed..."));
    ESP.restart();
  }

  loadSettings();

  // HARDWARE TEST MODE - Uncomment to test if serial corruption is power-related
  // config.brightness = 10;  // Very low brightness
  // config.red = 255; config.green = 0; config.blue = 0;  // Single color only

  strip.begin();
  strip.setBrightness(config.brightness);
  strip.clear();
  strip.show();

  Serial.println(F("Initializing RTC with placeholder time"));
  rtc.setTime(0, 0, 0, 1, 1, 2020);

  // Initialize watchdog timer
#ifdef ENABLE_WATCHDOG
  Serial.print(F("Enabling watchdog timer ("));
  Serial.print(WDT_TIMEOUT);
  Serial.println(F(" seconds)..."));
  esp_task_wdt_init(WDT_TIMEOUT, true);  // timeout, panic on timeout
  Serial.println(F("Watchdog enabled - system will auto-reboot if frozen"));
#endif

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
  Serial.println(F("Creating tasks..."));

  xTaskCreatePinnedToCore(
    displayTask,
    "DisplayTask",
    4096,
    NULL,
    2,
    &displayTaskHandle,
    1
  );

  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    &networkTaskHandle,
    0
  );

  delay(100);
  Serial.println(F("Tasks created successfully"));

#ifdef ENABLE_WATCHDOG
  // Add tasks to watchdog monitoring
  esp_task_wdt_add(displayTaskHandle);
  esp_task_wdt_add(networkTaskHandle);
  Serial.println(F("Tasks added to watchdog monitoring"));
#endif
}

// ------------------------------------------------------------
// main

void loop() {
  // Tasks handle everything now, so loop just sleeps
#ifdef ENABLE_WATCHDOG
  esp_task_wdt_reset();  // Feed the watchdog
#endif
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void updateSettings() {
  if (!update) return;
  storeSettings();
  update = false;
}

void updateTime() {
  if (!wifiConnected) {
    return;
  }

  unsigned long now = millis();
  if (now < nextTimeSync) {
    return;
  }

  Serial.println(F("Attempting NTP sync..."));
  currentStatus = STATUS_NTP;

  timeClient.forceUpdate();
  bool updateSuccess = timeClient.isTimeSet();

  if (updateSuccess) {
    time_t time = timeClient.getEpochTime();

    if (time > 1577836800) {
      rtc.setTime(time);
      timeIsSynced = true;
      currentStatus = STATUS_READY;
      Serial.println(F("✓ Time synced successfully over NTP"));
      displayTimeInfo(AT.toLocal(time));

      nextTimeSync = now + NTP_SYNC_INTERVAL;
      Serial.print(F("Next NTP sync in "));
      Serial.print(NTP_SYNC_INTERVAL / 60000);
      Serial.println(F(" minutes"));
    } else {
      Serial.println(F("✗ NTP returned invalid time"));
      scheduleRetry();
    }
  } else {
    Serial.println(F("✗ NTP sync failed, will retry soon"));
    scheduleRetry();
  }
}

void scheduleRetry() {
  // Retry failed NTP syncs more frequently
  nextTimeSync = millis() + NTP_RETRY_INTERVAL;
  Serial.print(F("Retry NTP sync in "));
  Serial.print(NTP_RETRY_INTERVAL / 1000);
  Serial.println(F(" seconds"));
}

void printSettings() {
  Serial.print(F("Red: "));
  Serial.println(config.red);
  Serial.print(F("Green: "));
  Serial.println(config.green);
  Serial.print(F("Blue: "));
  Serial.println(config.blue);
  Serial.print(F("Brightness: "));
  Serial.println(config.brightness);
  Serial.print(F("Language: "));
  Serial.println(config.language);
  Serial.print(F("Enabled: "));
  Serial.println(config.enabled);
  Serial.print(F("Transition: "));
  Serial.println(config.transition);
  Serial.print(F("PrefixMode: "));
  Serial.println(config.prefixMode);
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
      Serial.println(F("WiFi reconnected!"));
      Serial.print(F("IP address: "));
      Serial.println(WiFi.localIP());

      // Trigger immediate NTP sync
      nextTimeSync = 0;
    }
  } else {
    if (wifiConnected) {
      // WiFi just disconnected
      wifiConnected = false;
      timeIsSynced = false;
      currentStatus = STATUS_WIFI;  // Lost WiFi, trying to reconnect
      Serial.println(F("WiFi connection lost! Attempting to reconnect..."));
    }

    // Try to reconnect
    WiFi.reconnect();
  }
}

void onWiFiConnected() {
  Serial.println(F("WiFi connected successfully!"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  startNTP();
  startMDNS();
  startServer();

#ifdef ENABLE_OTA
  setupOTA();
#endif
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered config mode"));
  Serial.print(F("AP IP: "));
  Serial.println(WiFi.softAPIP());
  Serial.print(F("SSID: "));
  Serial.println(myWiFiManager->getConfigPortalSSID());

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
  nextTimeSync = 0;
  Serial.println(F("NTP client started, will sync immediately"));
}

void startMDNS() {
  MDNS.begin(DNS_NAME);
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("wordclock", "tcp", 80);
  Serial.println(F("mDNS responder started"));
}

#ifdef ENABLE_OTA
void setupOTA() {
  ArduinoOTA.setHostname(DNS_NAME);
  ArduinoOTA.setPassword("wordclock");  // Change this password!

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println(F("OTA Update starting: ") + type);
    currentStatus = STATUS_BOOT;  // Show animation during update
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nOTA Update complete!"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });

  ArduinoOTA.begin();
  Serial.println(F("OTA ready - use 'wordclock.local' as network port in Arduino IDE"));
}
#endif

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

  // Serve static files with caching headers
  server.serveStatic("/index.html", LittleFS, "/index.html")
    .setCacheControl("max-age=3600");
  server.serveStatic("/app.js", LittleFS, "/app.js")
    .setCacheControl("max-age=86400");
  server.serveStatic("/styles.css", LittleFS, "/styles.css")
    .setCacheControl("max-age=86400");

  // Start server
  server.begin();
  Serial.println(F("WebServer started"));
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
  doc["transition"] = config.transition;
  doc["prefixMode"] = config.prefixMode;
  doc["transitionSpeed"] = config.transitionSpeed;

  String response;
  response.reserve(128);
  if (!serializeJson(doc, response)) {
    Serial.println(F("Failed to create response!"));
  }

  request->send(200, "application/json", response);
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
  if (doc.containsKey("transition")) {
    uint8_t newTransition = (uint8_t)doc["transition"];
    config.transition = newTransition;
    // Always request preview animation, even if same transition
    if (currentStatus == STATUS_READY) {
      playPreviewAnimation = true;
    }
  }
  if (doc.containsKey("prefixMode")) {
    config.prefixMode = (uint8_t)doc["prefixMode"];
  }
  if (doc.containsKey("transitionSpeed")) {
    config.transitionSpeed = (uint8_t)doc["transitionSpeed"];
  }

  update = true;

  request->send(200, "text/plain", "ok");
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
  config.brightness = preferences.getUChar("brightness", 128);
  config.language = preferences.getString("language", "dialekt");
  config.enabled = preferences.getBool("enabled", true);

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

  Serial.println(F("Settings loaded from preferences"));
  Serial.print(F("Transition: "));
  Serial.println(config.transition);
  Serial.print(F("Transition Speed: "));
  Serial.println(config.transitionSpeed);
  // printSettings();  // Disabled to prevent serial output corruption
}

void storeSettings() {
  preferences.begin(PREFS_NAMESPACE, false);

  preferences.putUChar("red", config.red);
  preferences.putUChar("green", config.green);
  preferences.putUChar("blue", config.blue);
  preferences.putUChar("brightness", config.brightness);
  preferences.putString("language", config.language);
  preferences.putBool("enabled", config.enabled);
  preferences.putUChar("transition", config.transition);
  preferences.putUChar("prefixMode", config.prefixMode);
  preferences.putUChar("transSpeed", config.transitionSpeed);

  preferences.end();

  Serial.print(F("Transition saved: "));
  Serial.println(config.transition);
  Serial.print(F("Transition Speed saved: "));
  Serial.println(config.transitionSpeed);

  // Serial output disabled - causes corruption with display task
  // safePrintln(F("Settings saved to preferences"));
  // printSettings();
}

// ------------------------------------------------------------
// wordclock logic

// Helper function to get delay values based on transition speed
// Speed 0 (Extra Slow): extra long delays, Speed 1 (Very Slow): very long delays, Speed 2 (Medium): normal delays, Speed 3 (Fast): short delays, Speed 4 (Very Fast): very short delays
void getTransitionDelays(int& fadeDelay, int& wipeDelay, int& sparkleDelay, int& pauseDelay) {
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

// Transition animation functions
void fadeTransition(time_t time, String* timeString) {
  int fadeDelay, wipeDelay, sparkleDelay, pauseDelay;
  getTransitionDelays(fadeDelay, wipeDelay, sparkleDelay, pauseDelay);

  // Fade out old time completely (including minute LEDs)
  for (int brightness = config.brightness; brightness >= 0; brightness -= 8) {
    strip.setBrightness(brightness);
    strip.show();
    delay(fadeDelay);
  }

  // Clear and pause briefly
  strip.clear();
  strip.show();
  delay(pauseDelay);

  // Set new time pattern WITHOUT any brightness changes yet
  strip.setBrightness(255);  // Set to full brightness to store full color values
  setPixels(time, timeString);

  // Store pixel colors in temp array BEFORE any brightness manipulation
  uint32_t tempColors[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    tempColors[i] = strip.getPixelColor(i);
  }

  // Fade in by restoring pixels at increasing brightness
  for (int brightness = 8; brightness <= config.brightness; brightness += 8) {
    strip.setBrightness(brightness);
    // Restore all pixel colors
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, tempColors[i]);
    }
    strip.show();
    delay(fadeDelay);
  }

  // Final display at exact target brightness
  strip.setBrightness(config.brightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, tempColors[i]);
  }
  strip.show();
}

void wipeTransition(time_t time, String* timeString) {
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

  strip.setBrightness(config.brightness);
  strip.show();
}

void sparkleTransition(time_t time, String* timeString) {
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

  strip.setBrightness(config.brightness);
  strip.show();
}

void playTransition(time_t time, String* timeString) {
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

  if (!config.enabled) {
    strip.clear();
    strip.show();
    return;
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
    strip.setBrightness(config.brightness);
    playTransition(time, &timeString);
    Serial.println(F("Preview animation played"));
    return;
  }

  // Force initial display when first becoming ready
  if (firstDisplay) {
    String timeString;
    strip.setBrightness(config.brightness);
    strip.clear();
    setPixels(time, &timeString);
    strip.show();
    lastMin = currentMin;
    Serial.println(timeString);
    firstDisplay = false;
    return;
  }

  if (lastMin != currentMin) {
    String timeString;
    strip.setBrightness(config.brightness);
    playTransition(time, &timeString);
    lastMin = currentMin;
    Serial.println(timeString);
  } else if (settingsChanged) {
    strip.setBrightness(config.brightness);
    strip.clear();
    setPixels(time, nullptr);  // Don't build string for settings-only changes
    strip.show();
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
    }
    else if (currentStatus == STATUS_WIFI) {
      // WiFi: LED 0 solid, LED 1 blinks
      strip.setPixelColor(MINUTE_LEDS[0], config.red, config.green, config.blue);
      uint8_t r = (config.red * blinkBrightness) / 25;
      uint8_t g = (config.green * blinkBrightness) / 25;
      uint8_t b = (config.blue * blinkBrightness) / 25;
      strip.setPixelColor(MINUTE_LEDS[1], r, g, b);
    }
    else if (currentStatus == STATUS_NTP) {
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

void setPixels(time_t time, String* timeString) {
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
  Serial.print(F("Display task started on core "));
  Serial.println(xPortGetCoreID());

  static bool lastUpdate = false;

  for (;;) {
    // Detect rising edge of update flag - only refresh once when settings change
    bool settingsChanged = update && !lastUpdate;
    lastUpdate = update;

    refreshMatrix(settingsChanged);

#ifdef ENABLE_WATCHDOG
    esp_task_wdt_reset();
#endif

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void networkTask(void *parameter) {
  Serial.print(F("Network task started on core "));
  Serial.println(xPortGetCoreID());

  // First, connect to WiFi
  Serial.println(F("Starting WiFi connection..."));
  currentStatus = STATUS_WIFI;

  if (!wm.autoConnect(AP_SSID)) {
    Serial.println(F("Failed to start WiFi connection"));
    ESP.restart();
  }

  // Wait for WiFi to be fully connected
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  Serial.println(F("WiFi connected successfully"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  wifiConnected = true;
  currentStatus = STATUS_NTP;  // Now waiting for NTP
  onWiFiConnected();

  // Main network task loop
  for (;;) {
    checkWiFiConnection();
    updateTime();
    updateSettings();

#ifdef ENABLE_OTA
    ArduinoOTA.handle();  // Handle OTA updates
#endif

#ifdef ENABLE_WATCHDOG
    esp_task_wdt_reset();  // Feed watchdog
#endif

    vTaskDelay(pdMS_TO_TICKS(1000));  // Run every second
  }
}

// ------------------------------------------------------------
// display time information in readable format

void displayTimeInfo(time_t t) {
  const char* weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  Serial.println(F(""));
  Serial.println(F("=== Local Time ==="));

  Serial.print(F("Date: "));
  Serial.print(weekdays[weekday(t) - 1]);
  Serial.print(F(", "));
  Serial.print(months[month(t) - 1]);
  Serial.print(F(" "));
  if (day(t) < 10) Serial.print(F("0"));
  Serial.print(day(t));
  Serial.print(F(", "));
  Serial.println(year(t));

  Serial.print(F("Time: "));
  if (hour(t) < 10) Serial.print(F("0"));
  Serial.print(hour(t));
  Serial.print(F(":"));
  if (minute(t) < 10) Serial.print(F("0"));
  Serial.print(minute(t));
  Serial.print(F(":"));
  if (second(t) < 10) Serial.print(F("0"));
  Serial.println(second(t));

  Serial.println(F("=================="));
}