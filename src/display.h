#pragma once

void displayInit();
void displaySetBrightness(int percent);
void displayShowConnecting();
void displayShowSetup();
void displayShowIP(const String& ip);
void displayShowError(const String& errorMsg);
void displayClear();
