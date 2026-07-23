/*
  ===========================================================
  AquaGuard AI - display.h
  ===========================================================
  Single 16x2 I2C LCD showing 3 rotating screens:
    Screen 1: R/Y/B current + Voltage
    Screen 2: Water % + Liters + Motor status
    Screen 3: Water used + Borewell health

  Controlled by:
   - Auto-cycle timer (every SCREEN_CYCLE_INTERVAL_MS)
   - Push button (single press -> next screen, resets timer)
   - Serial input ('1' / '2' / '3' -> jump to screen)

  Faults override all screens immediately and freeze cycling
  until the fault clears.
  ===========================================================
*/

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "sensors.h"
#include "protection.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

int currentScreen = 1;
unsigned long lastScreenChangeTime = 0;
unsigned long lastWaterUsedCalcTime = 0;

float waterBeforePump = 0;
float totalWaterUsedLiters = 0;
bool wasMotorRunningLastCheck = false;

// Button debounce state
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// ===========================================================
// Init
// ===========================================================
void initDisplay() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AquaGuard AI");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1500);
  lcd.clear();
}

// ===========================================================
// Track water usage: detect pump-stop events to calculate
// "Used Liters" per the doc (compare volume before/after pump)
// ===========================================================
void trackWaterUsage() {
  if (protState.motorRunning && !wasMotorRunningLastCheck) {
    // Motor just turned ON -> record starting water level
    waterBeforePump = liveData.waterLiters;
  }
  if (!protState.motorRunning && wasMotorRunningLastCheck) {
    // Motor just turned OFF -> calculate usage (water dropped during pumping
    // means it was being drawn down faster than recharge, common in direct-use systems;
    // for borewell-fill-to-tank setups this also tracks delivered volume)
    float delta = liveData.waterLiters - waterBeforePump;
    if (delta != 0) {
      totalWaterUsedLiters += abs(delta);
    }
  }
  wasMotorRunningLastCheck = protState.motorRunning;
}

// ===========================================================
// Borewell health heuristic (simple, expandable later)
// ===========================================================
const char* getBorewellHealth() {
  // crude heuristic: frequent dry-run faults => poor recovery => weak borewell
  if (protState.restartAttempts >= MAX_RESTART_ATTEMPTS) return "POOR";
  if (protState.restartAttempts >= 1) return "FAIR";
  return "GOOD";
}

// ===========================================================
// Screen renderers
// ===========================================================
void renderScreen1() {
  lcd.setCursor(0, 0);
  lcd.print("R:");
  lcd.print(liveData.currentR, 1);
  lcd.print(" Y:");
  lcd.print(liveData.currentY, 1);
  lcd.print("   "); // clear trailing chars
  lcd.setCursor(0, 1);
  lcd.print("B:");
  lcd.print(liveData.currentB, 1);
  lcd.print(" V:");
  lcd.print((int)liveData.voltage);
  lcd.print("   ");
}

void renderScreen2() {
  lcd.setCursor(0, 0);
  lcd.print("W:");
  lcd.print((int)liveData.waterPercent);
  lcd.print("% ");
  lcd.print((int)liveData.waterLiters);
  lcd.print("L      ");
  lcd.setCursor(0, 1);
  lcd.print("MOTOR:");
  lcd.print(protState.motorRunning ? "ON " : "OFF");
  lcd.print("     ");
}

void renderScreen3() {
  lcd.setCursor(0, 0);
  lcd.print("USED:");
  lcd.print((int)totalWaterUsedLiters);
  lcd.print("L       ");
  lcd.setCursor(0, 1);
  lcd.print("HEALTH:");
  lcd.print(getBorewellHealth());
  lcd.print("   ");
}

void renderFaultScreen() {
  lcd.setCursor(0, 0);
  lcd.print("** FAULT **     ");
  lcd.setCursor(0, 1);
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", faultToString(protState.activeFault));
  lcd.print(buf);
}

// ===========================================================
// Button handling (single press = next screen)
// ===========================================================
void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    // Button is pressed when reading == LOW (INPUT_PULLUP wiring)
    static int stableState = HIGH;
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {
        currentScreen++;
        if (currentScreen > 3) currentScreen = 1;
        lastScreenChangeTime = millis(); // reset auto-cycle timer on manual press
      }
    }
  }
  lastButtonState = reading;
}

// ===========================================================
// Serial input handling ('1'/'2'/'3' jumps directly)
// ===========================================================
void handleSerialScreenInput() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1' || c == '2' || c == '3') {
      currentScreen = c - '0';
      lastScreenChangeTime = millis();
      Serial.print("[DISPLAY] Switched to screen ");
      Serial.println(currentScreen);
    } else if (c == 'r' || c == 'R') {
      resetSystem();
    } else if (c == 's' || c == 'S') {
      requestMotorStart();
    } else if (c == 'x' || c == 'X') {
      requestMotorStop();
    }
  }
}

// ===========================================================
// Master display update — call every loop
// ===========================================================
void updateDisplay() {
  handleButton();
  handleSerialScreenInput();
  trackWaterUsage();

  // Faults override everything
  if (protState.activeFault != FAULT_NONE) {
    renderFaultScreen();
    return;
  }

  // Auto-cycle screens
  if (millis() - lastScreenChangeTime >= SCREEN_CYCLE_INTERVAL_MS) {
    currentScreen++;
    if (currentScreen > 3) currentScreen = 1;
    lastScreenChangeTime = millis();
  }

  switch (currentScreen) {
    case 1: renderScreen1(); break;
    case 2: renderScreen2(); break;
    case 3: renderScreen3(); break;
  }
}

#endif
