#include "display.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <LittleFS.h>

TFT_eSPI tft = TFT_eSPI();

static const int SCREEN_W = 240;

static uint16_t colBg;
static uint16_t colText;
static uint16_t colSubtext;

static void applyTheme(int theme) {
    uint16_t gray     = tft.color565(0x88, 0x88, 0x88);
    uint16_t darkGray = tft.color565(0x55, 0x55, 0x55);
    uint16_t offWhite = tft.color565(0xF0, 0xF0, 0xF0);

    switch (theme) {
        default:
        case 0: // Dark
            colBg      = TFT_BLACK;
            colText    = TFT_WHITE;
            colSubtext = gray;
            break;
        case 1: // Light
            colBg      = offWhite;
            colText    = TFT_BLACK;
            colSubtext = darkGray;
            break;
    }
}

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

void displayInit() {
    tft.init();
    tft.setRotation(0);
    vendorInit();

    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);

    applyTheme(getTheme());

    tft.fillScreen(colBg);
    tft.setTextColor(colText, colBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting...", SCREEN_W / 2, SCREEN_W / 2, 4);
    Serial.println("[display] init done");
}

void displaySetBrightness(int percent) {
    if (percent > 100) percent = 100;
    if (percent < 5)   percent = 5;
    int pwm = 250 - (percent * 250 / 100);
    analogWrite(5, pwm);
}

void displayShowConnecting() {
    applyTheme(getTheme());
    tft.fillScreen(colBg);
    tft.setTextColor(colText, colBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connecting", SCREEN_W / 2, SCREEN_W / 2 - 10, 4);
    tft.drawString("to WiFi...", SCREEN_W / 2, SCREEN_W / 2 + 20, 4);
}

void displayShowSetup() {
    applyTheme(getTheme());
    tft.fillScreen(colBg);
    tft.setTextDatum(TC_DATUM);

    tft.setTextColor(TFT_YELLOW, colBg);
    tft.drawString("Setup Required", SCREEN_W / 2, 10, 4);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(colText, colBg);

    int y = 50;
    tft.drawString("1. Connect to WiFi:", 10, y, 2);      y += 18;
    tft.setTextColor(TFT_CYAN, colBg);
    tft.drawString("   ImageDisplay", 10, y, 2);           y += 26;

    tft.setTextColor(colText, colBg);
    tft.drawString("2. Open browser to:", 10, y, 2);       y += 18;
    tft.setTextColor(TFT_CYAN, colBg);
    tft.drawString("   192.168.4.1", 10, y, 2);            y += 26;

    tft.setTextColor(colText, colBg);
    tft.drawString("3. Enter WiFi creds", 10, y, 2);       y += 18;
    tft.drawString("4. Paste Image URL", 10, y, 2);

    tft.setTextColor(colSubtext, colBg);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("JPEG, 240x240 recommended", SCREEN_W / 2, 220, 2);
}
