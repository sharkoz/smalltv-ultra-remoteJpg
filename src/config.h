#pragma once

#include <Arduino.h>

void configInit();
const String& getImageUrl();
void setImageUrl(const String& url);
bool isConfigured();
int getBrightness();
void setBrightness(int percent);
int getTheme();
void setTheme(int theme);
int getRefreshInterval();
void setRefreshInterval(int seconds);
void saveConfig();
