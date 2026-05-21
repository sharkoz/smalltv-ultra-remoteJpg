#include "config.h"
#include "display.h"
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char* KEY_URL      = "imageUrl";
static const char* KEY_BRT      = "brightness";
static const char* KEY_THEME    = "theme";
static const char* KEY_REFRESH  = "refreshInterval";

static String cachedUrl;
static int brightness       = 100;
static int themeCfg         = 0;
static int refreshInterval  = 300; // seconds

static void loadConfig() {
    File f = LittleFS.open("/config.json", "r");
    if (!f) return;
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    cachedUrl       = doc[KEY_URL]     | "";
    brightness      = doc[KEY_BRT]     | 100;
    themeCfg        = doc[KEY_THEME]   | 0;
    refreshInterval = doc[KEY_REFRESH] | 300;
}

void saveConfig() {
    JsonDocument doc;
    doc[KEY_URL]     = cachedUrl;
    doc[KEY_BRT]     = brightness;
    doc[KEY_THEME]   = themeCfg;
    doc[KEY_REFRESH] = refreshInterval;
    File f = LittleFS.open("/config.json", "w");
    serializeJson(doc, f);
    f.close();
}

void configInit() {
    LittleFS.begin();
    loadConfig();

    WiFiManager wm;

    WiFiManagerParameter urlParam("image_url", "Image URL (JPEG)", cachedUrl.c_str(), 256);
    wm.addParameter(&urlParam);

    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180);
    wm.setAPCallback([](WiFiManager*) {
        displayShowSetup();
    });

    if (!wm.autoConnect("ImageDisplay")) {
        Serial.println("[config] WiFi failed - restarting");
        ESP.restart();
    }

    String newUrl = String(urlParam.getValue());
    if (newUrl.length() > 0 && newUrl != cachedUrl) {
        cachedUrl = newUrl;
        saveConfig();
    }
}

const String& getImageUrl() {
    return cachedUrl;
}

void setImageUrl(const String& url) {
    cachedUrl = url;
}

bool isConfigured() {
    return cachedUrl.length() > 0;
}

int getBrightness() {
    return brightness;
}

void setBrightness(int percent) {
    brightness = constrain(percent, 5, 100);
}

int getTheme() {
    return themeCfg;
}

void setTheme(int theme) {
    themeCfg = constrain(theme, 0, 1);
}

int getRefreshInterval() {
    return refreshInterval;
}

void setRefreshInterval(int seconds) {
    refreshInterval = constrain(seconds, 30, 3600);
}
