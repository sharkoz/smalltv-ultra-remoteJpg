#include <Arduino.h>
#include "display.h"
#include "image.h"
#include "config.h"
#include "ota.h"

static unsigned long lastImageFetch = 0;

void setup() {
    Serial.begin(115200);
    logInfo("Starting...");

    displayInit();
    displayShowConnecting();

    configInit();
    logInfo("WiFi connected");

    // Show IP for 3 seconds
    String ip = getWiFiIP();
    logInfo("IP: " + ip);
    displayShowIP(ip);
    delay(3000);

    displaySetBrightness(getBrightness());
    imageInit();
    otaInit();
    logInfo("Setup complete");

    if (!isConfigured()) {
        logInfo("No image URL configured");
        displayShowSetup();
        return;
    }

    // Try to fetch the initial image
    if (!imageFetch(getImageUrl())) {
        logError("Initial image fetch failed");
        displayShowError("Failed to load image\nCheck URL");
        delay(3000);
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
            logError("Image refresh failed");
            displayShowError("Failed to refresh\nimage");
            delay(3000);
        }
    }
}
