// Minimal OTA bootstrap firmware for GeekMagic SmallTV-Ultra
// Purpose: fits within the stock OTA partition (~520KB) and provides
// a web-based firmware upload endpoint so the full firmware can be flashed.
//
// Build: pio run -e bootstrap
// Flash via stock OTA: upload .pio/build/bootstrap/firmware.bin

#ifdef BOOTSTRAP

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>

static ESP8266WebServer server(80);

// --- Display (ST7789 240x240) -------------------------------------------
// Minimal, self-contained display support for the bootstrap. vendorInit()
// is required for this panel (see src/display.cpp / CLAUDE.md).

static TFT_eSPI tft = TFT_eSPI();

static const int SCREEN_W = 240;
static const uint16_t COL_BG   = TFT_BLACK;
static const uint16_t COL_TEXT = TFT_WHITE;

static void vendorInit() {
    tft.writecommand(0x11); delay(120);
    tft.writecommand(0xB2); tft.writedata(0x1F); tft.writedata(0x1F);
    tft.writedata(0x00); tft.writedata(0x33); tft.writedata(0x33);
    tft.writecommand(0xB7); tft.writedata(0x00);
    tft.writecommand(0xBB); tft.writedata(0x36);
    tft.writecommand(0xC0); tft.writedata(0x2C);
    tft.writecommand(0xC2); tft.writedata(0x01);
    tft.writecommand(0xC3); tft.writedata(0x13);
    tft.writecommand(0xC4); tft.writedata(0x20);
    tft.writecommand(0xC6); tft.writedata(0x13);
    tft.writecommand(0xD6); tft.writedata(0xA1);
    tft.writecommand(0xD0); tft.writedata(0xA4); tft.writedata(0xA1);
    tft.writecommand(0xD6); tft.writedata(0xA1);
    tft.writecommand(0xE0);
    uint8_t pgamma[] = {0xF0,0x08,0x0E,0x09,0x08,0x04,0x2F,0x33,0x45,0x36,0x13,0x12,0x2A,0x2D};
    for (uint8_t b : pgamma) tft.writedata(b);
    tft.writecommand(0xE1);
    uint8_t ngamma[] = {0xF0,0x0E,0x12,0x0C,0x0A,0x15,0x2E,0x32,0x44,0x39,0x17,0x18,0x2B,0x2F};
    for (uint8_t b : ngamma) tft.writedata(b);
    tft.writecommand(0xE4); tft.writedata(0x1D); tft.writedata(0x00); tft.writedata(0x00);
    tft.writecommand(0x21);
    tft.writecommand(0x29);
}

static void displayInit() {
    tft.init();
    tft.setRotation(0);
    vendorInit();

    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);  // backlight active-low: LOW = full brightness

    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Bootstrap...", SCREEN_W / 2, SCREEN_W / 2, 4);
}

// Phase 1: WiFiManager config portal is open.
static void showSetupScreen(const char* apName) {
    tft.fillScreen(COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.drawString("WiFi Setup", SCREEN_W / 2, 12, 4);

    tft.setTextDatum(TL_DATUM);
    int y = 60;
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("1. Join WiFi:", 10, y, 2);          y += 18;
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(String("   ") + apName, 10, y, 2);    y += 30;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("2. Open browser to:", 10, y, 2);     y += 18;
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("   192.168.4.1", 10, y, 2);          y += 30;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("3. Enter WiFi creds", 10, y, 2);
}

// Phase 2: WiFi connected, OTA upload server running.
static void showUploadScreen(IPAddress ip) {
    tft.fillScreen(COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("WiFi Connected", SCREEN_W / 2, 12, 4);

    tft.setTextDatum(TL_DATUM);
    int y = 70;
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Open in browser:", 10, y, 2);        y += 22;
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(String("http://") + ip.toString(), 10, y, 4);  y += 36;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("to upload firmware", 10, y, 2);
}

static const char PAGE[] PROGMEM = R"rawliteral(
<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{background:#222;color:#eee;font-family:sans-serif;max-width:500px;margin:0 auto;padding:16px;text-align:center}
h2{color:#fff}
p{color:#aaa}
.btn{background:#5865F2;color:#fff;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;margin:8px}
</style></head><body>
<h2>ImageDisplay Bootstrap</h2>
<p>Upload the full firmware below.</p>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='firmware' accept='.bin'><br><br>
<input type='submit' value='Upload Firmware' class='btn'>
</form>
<p style='margin-top:40px;font-size:12px;color:#666'>
IP: <script>document.write(location.hostname)</script> | Port: 80
</p>
</body></html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    Serial.println("\n[bootstrap] starting");

    displayInit();

    WiFiManager wm;
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180);

    // Show AP name + portal IP on screen when the config portal opens.
    wm.setAPCallback([](WiFiManager* mgr) {
        showSetupScreen(mgr->getConfigPortalSSID().c_str());
    });

    if (!wm.autoConnect("ImageDisplay")) {
        Serial.println("[bootstrap] WiFi failed - restarting");
        ESP.restart();
    }

    Serial.printf("[bootstrap] connected, IP: %s\n", WiFi.localIP().toString().c_str());
    showUploadScreen(WiFi.localIP());

    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", PAGE);
    });

    server.on("/update", HTTP_POST, []() {
        bool ok = !Update.hasError();
        if (ok) {
            server.send(200, "text/html",
                "<html><body style='background:#222;color:#eee;font-family:sans-serif;text-align:center;padding-top:80px'>"
                "<h2>Firmware updated!</h2><p>Rebooting...</p>"
                "<script>setTimeout(function r(){fetch('/').then(()=>location.href='/').catch(()=>setTimeout(r,2000))},5000)</script>"
                "</body></html>");
        } else {
            server.send(200, "text/html",
                "<html><body style='background:#222;color:#eee;font-family:sans-serif;text-align:center;padding-top:80px'>"
                "<h2 style='color:#d33'>Update failed!</h2><p><a href='/' style='color:#88f'>Try again</a></p>"
                "</body></html>");
        }
        delay(500);
        if (ok) ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            uint32_t maxSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            Update.begin(maxSize);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            Update.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            Update.end(true);
        }
    });

    server.begin();
    Serial.println("[bootstrap] OTA ready on port 80");
}

void loop() {
    server.handleClient();
}

#endif
