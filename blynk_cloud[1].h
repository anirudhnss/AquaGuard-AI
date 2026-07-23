/*
  ===========================================================
  AquaGuard AI - blynk_cloud.h
  ===========================================================
  Blynk IoT (2.0 / Blynk.Cloud) integration.
  Only activates when WiFi STA mode succeeded (isHotspotMode == false),
  since Blynk needs real internet access, not just local hotspot.

  IMPORTANT: BLYNK_TEMPLATE_ID / TEMPLATE_NAME / AUTH_TOKEN
  MUST be defined BEFORE #include <BlynkSimpleEsp32.h>
  That's why this file defines them at the very top, and this
  file must be the FIRST include in the main .ino, before any
  other file that might include Blynk headers indirectly.
  ===========================================================
*/

#ifndef BLYNK_CLOUD_H
#define BLYNK_CLOUD_H

#include "config.h"

#define BLYNK_TEMPLATE_ID   BLYNK_TEMPLATE_ID_VALUE
#define BLYNK_TEMPLATE_NAME BLYNK_TEMPLATE_NAME_VALUE
#define BLYNK_AUTH_TOKEN    BLYNK_AUTH_TOKEN_VALUE

#include <BlynkSimpleEsp32.h>
#include "sensors.h"
#include "protection.h"
#include "display.h"
#include "connectivity.h"

BlynkTimer blynkTimer;
bool blynkActive = false;

// ===========================================================
// Push all live sensor + status data to Blynk virtual pins
// ===========================================================
void sendDataToBlynk() {
  if (!blynkActive) return;

  Blynk.virtualWrite(VPIN_WATER_PERCENT, liveData.waterPercent);
  Blynk.virtualWrite(VPIN_WATER_LITERS, liveData.waterLiters);
  Blynk.virtualWrite(VPIN_VOLTAGE, liveData.voltage);
  Blynk.virtualWrite(VPIN_CURRENT_R, liveData.currentR);
  Blynk.virtualWrite(VPIN_CURRENT_Y, liveData.currentY);
  Blynk.virtualWrite(VPIN_CURRENT_B, liveData.currentB);
  Blynk.virtualWrite(VPIN_MOTOR_STATUS, protState.motorRunning ? 1 : 0);
  Blynk.virtualWrite(VPIN_FAULT_TEXT, faultToString(protState.activeFault));
  Blynk.virtualWrite(VPIN_WATER_USED, totalWaterUsedLiters);
  Blynk.virtualWrite(VPIN_BOREWELL_HEALTH, getBorewellHealth());
}

// ===========================================================
// Blynk app button handlers (app writes to these virtual pins)
// ===========================================================
BLYNK_WRITE(VPIN_MOTOR_SWITCH) {
  int value = param.asInt();
  if (value == 1) {
    requestMotorStart();
  } else {
    requestMotorStop();
  }
}

BLYNK_WRITE(VPIN_RESET_SYSTEM) {
  int value = param.asInt();
  if (value == 1) {
    resetSystem();
  }
}

// Called automatically when hardware connects to Blynk server
BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Connected to Blynk Cloud.");
  // Sync app button states on reconnect
  Blynk.syncVirtual(VPIN_MOTOR_SWITCH);
}

// ===========================================================
// Init - only call this if WiFi STA succeeded
// ===========================================================
void initBlynk() {
  Blynk.config(BLYNK_AUTH_TOKEN_VALUE);
  bool connected = Blynk.connect(5000); // 5s timeout, non-blocking failure
  if (connected) {
    blynkActive = true;
    Serial.println("[BLYNK] Initialized successfully.");
    blynkTimer.setInterval(2000L, sendDataToBlynk); // push data every 2s
  } else {
    blynkActive = false;
    Serial.println("[BLYNK] Could not connect (will retry in background).");
  }
}

void handleBlynk() {
  if (!blynkActive) return;
  Blynk.run();
  blynkTimer.run();
}

#endif
