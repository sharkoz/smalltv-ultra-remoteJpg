#include "image.h"
#include "display.h"
#include "ota.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>  // bodmer/TJpg_Decoder

static const size_t MAX_JPEG_BYTES = 100 * 1024; // 100KB cap
static const char* TMP_PATH = "/img.jpg";

extern TFT_eSPI tft;

// Conditional-fetch cache (RAM only; rebuilt after each reboot).
static String   lastEtag;          // last ETag received
static String   lastModified;      // last Last-Modified received
static uint32_t lastHash = 0;      // FNV-1a hash of last downloaded body
static bool     haveHash = false;  // false until first successful fetch

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
    if (url.isEmpty()) {
        logError("URL is empty");
        displayShowError("No URL configured");
        return false;
    }

    logInfo("Fetching: " + url);

    // Pick the transport from the URL scheme: plain TCP for http://,
    // TLS (insecure - no cert check) for https://. Both clients are stack
    // allocated; BearSSL only allocates its large buffers once it connects.
    bool useTls = url.startsWith("https://");
    WiFiClient            plainClient;
    BearSSL::WiFiClientSecure secureClient;
    WiFiClient* client = &plainClient;
    if (useTls) {
        secureClient.setInsecure();
        client = &secureClient;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    if (!http.begin(*client, url)) {
        logError("http.begin() failed - invalid URL");
        displayShowError("Invalid URL or\nconnection failed");
        return false;
    }

    // Collect cache validators so we can echo them back next time.
    const char* collectKeys[] = {"ETag", "Last-Modified"};
    http.collectHeaders(collectKeys, 2);

    // Conditional request: ask the server to skip the body if unchanged.
    if (lastEtag.length() > 0)     http.addHeader("If-None-Match", lastEtag);
    if (lastModified.length() > 0) http.addHeader("If-Modified-Since", lastModified);

    logInfo("Sending GET request...");
    int code = http.GET();
    logInfo("HTTP response: " + String(code));

    if (code == HTTP_CODE_NOT_MODIFIED) {
        logInfo("304 Not Modified - skipping");
        http.end();
        return true; // image already on screen, nothing to do
    }
    if (code != HTTP_CODE_OK) {
        logError("HTTP error " + String(code));
        String errorMsg = "HTTP Error: ";
        errorMsg += String(code);
        displayShowError(errorMsg);
        http.end();
        return false;
    }

    String newEtag     = http.header("ETag");
    String newModified = http.header("Last-Modified");

    File f = LittleFS.open(TMP_PATH, "w");
    if (!f) {
        logError("LittleFS open failed");
        displayShowError("Storage error");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        logError("Stream is null");
        displayShowError("No data stream");
        f.close();
        http.end();
        return false;
    }

    uint8_t buf[256];
    size_t total = 0;
    uint32_t hash = 2166136261u; // FNV-1a offset basis
    unsigned long lastData = millis();
    unsigned long startTime = millis();

    logInfo("Downloading data...");
    while (stream->connected() || stream->available()) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
            f.write(buf, n);
            for (size_t i = 0; i < n; i++) hash = (hash ^ buf[i]) * 16777619u;
            total += n;
            lastData = millis();
            if (total >= MAX_JPEG_BYTES) {
                logInfo("Size cap reached (" + String(total) + " bytes)");
                break;
            }
        } else {
            if (millis() - lastData > 3000) {
                logInfo("Timeout waiting for data");
                break;
            }
            delay(1);
        }
        // Safety timeout: 30 seconds max per download
        if (millis() - startTime > 30000) {
            logError("Total download timeout");
            break;
        }
    }
    f.close();
    http.end();

    if (total == 0) {
        logError("Empty response - no bytes received");
        displayShowError("Empty response\nfrom server");
        return false;
    }

    logInfo("Downloaded " + String(total) + " bytes in " + String(millis() - startTime) + "ms");

    // Server sent a 200 but the bytes are identical to last time (no validators
    // or unchanged content): refresh validators but skip decode + redraw.
    if (haveHash && hash == lastHash) {
        logInfo("Content hash unchanged - skipping redraw");
        lastEtag     = newEtag;
        lastModified = newModified;
        return true;
    }

    logInfo("Decoding JPEG...");
    JRESULT res = TJpgDec.drawFsJpg(0, 0, TMP_PATH, LittleFS);
    if (res != JDR_OK) {
        logError("JPEG decode error " + String(res));
        String errorMsg = "JPEG decode error: ";
        errorMsg += String(res);
        displayShowError(errorMsg);
        return false; // cache left untouched so we retry next poll
    }

    // Commit the cache only after a successful decode.
    lastHash     = hash;
    haveHash     = true;
    lastEtag     = newEtag;
    lastModified = newModified;

    logInfo("Success!");
    return true;
}
