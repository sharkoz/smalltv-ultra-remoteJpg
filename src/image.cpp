#include "image.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>  // bodmer/TJpg_Decoder

static const size_t MAX_JPEG_BYTES = 100 * 1024; // 100KB cap
static const char* TMP_PATH = "/img.jpg";

extern TFT_eSPI tft;

static bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void imageInit() {
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpgOutput);
}

bool imageFetch(const String& url) {
    if (url.isEmpty()) return false;

    BearSSL::WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    if (!http.begin(secureClient, url)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[image] HTTP %d\n", code);
        http.end();
        return false;
    }

    File f = LittleFS.open(TMP_PATH, "w");
    if (!f) {
        Serial.println("[image] LittleFS open failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[256];
    size_t total = 0;
    unsigned long lastData = millis();

    while (stream->connected() || stream->available()) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
            f.write(buf, n);
            total += n;
            lastData = millis();
            if (total >= MAX_JPEG_BYTES) {
                Serial.printf("[image] size cap reached (%u bytes)\n", total);
                break;
            }
        } else {
            if (millis() - lastData > 3000) break;
            delay(1);
        }
    }
    f.close();
    http.end();

    if (total == 0) {
        Serial.println("[image] empty response");
        return false;
    }

    Serial.printf("[image] %u bytes downloaded\n", total);

    JRESULT res = TJpgDec.drawFsJpg(0, 0, TMP_PATH, LittleFS);
    if (res != JDR_OK) {
        Serial.printf("[image] JPEG decode error %d\n", res);
        return false;
    }

    return true;
}
