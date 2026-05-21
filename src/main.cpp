#include <Arduino.h>
#include "display.h"
#include "image.h"
#include "config.h"
#include "ota.h"

static unsigned long lastImageFetch = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n[main] starting");

    displayInit();
    displayShowConnecting();

    configInit();
    Serial.println("[main] WiFi connected");

    displaySetBrightness(getBrightness());
    imageInit();
    otaInit();
    Serial.println("[main] OTA ready");

    if (!isConfigured()) {
        Serial.println("[main] no image URL configured");
        displayShowSetup();
        return;
    }

    if (!imageFetch(getImageUrl())) {
        Serial.println("[main] initial image fetch failed");
    }
    lastImageFetch = millis();
}

void loop() {
    otaHandle();

    if (!isConfigured()) return;

    unsigned long now = millis();
    unsigned long interval = (unsigned long)getRefreshInterval() * 1000UL;

    if (now - lastImageFetch >= interval) {
        lastImageFetch = now;
        if (!imageFetch(getImageUrl())) {
            Serial.println("[main] image fetch failed");
        }
    }
}
