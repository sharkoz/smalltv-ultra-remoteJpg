#pragma once

#include <Arduino.h>

void otaInit();
void otaHandle();
void logError(const String& msg);
void logInfo(const String& msg);
String getLastLogs();
