/*
  ===========================================================
  AquaGuard AI / Smart Borewell Intelligence System (SBIS)
  ===========================================================
  Main sketch file.

  Hardware: ESP32 DevKit V1, 3x ACS712-20A, ZMPT101B,
            HC-SR04, 16x2 I2C LCD, 1-channel Relay, Buzzer,
            Push Button.

  Author: Built for Ranjith's ECE Mini Project
  ===========================================================

  FILE STRUCTURE (all files must sit in this same sketch folder):
    AquaGuard_AI.ino     <- this file
    config.h             <- pins, thresholds, calibration
    sensors.h             <- sensor reading + RMS calculation
    protection.h          <- fault detection + relay/auto-restart logic
    display.h             <- LCD screen cycling (button + serial input)
    connectivity.h        <- WiFi STA + Hotspot fallback + local dashboard
    blynk_cloud.h          <- Blynk IoT cloud integration

  ===========================================================
  REQUIRED ARDUINO LIBRARIES (install via Library Manager):
    1. LiquidCrystal I2C   (by Frank de Brabander, or "LiquidCrystal_I2C")
    2. Blynk                (by Volodymyr Shymanskyy - search "Blynk")
    (WiFi.h and WebServer.h are built into ESP32 core, no install needed)
  ===========================================================
*/

// IMPORTANT: blynk_cloud.h must be included FIRST because it defines
// BLYNK_TEMPLATE_ID / BLYNK_TEMPLATE_NAME / BLYNK_AUTH_TOKEN before
// the Blynk library header is pulled in.
#include "blynk_cloud.h"

#include "config.h"
#include "sensors.h"
#include "protection.h"
#include "display.h"
#include "connectivity.h"

unsigned long lastSensorReadTime = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== AquaGuard AI Booting ===");

  // ---- Pin Modes ----
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // ADC pins (34,35,32,33) don't need pinMode() on ESP32, but explicit is fine:
  // analogReadResolution defaults to 12-bit (0-4095), which our config assumes.
  analogReadResolution(12);

  // Start with motor OFF and buzzer OFF for safety
  motorOff();
  buzzerOff();

  // ---- LCD ----
  initDisplay();

  // ---- Initial sensor read so display isn't blank/garbage on first screen ----
  updateAllSensors();

  // ---- WiFi (STA with Hotspot fallback) + local dashboard ----
  initConnectivity();

  // ---- Blynk (only meaningful if STA WiFi succeeded) ----
  if (wifiConnected) {
    initBlynk();
  } else {
    Serial.println("[BLYNK] Skipped - no internet (Hotspot mode active).");
  }

  Serial.println("=== Setup Complete ===");
  Serial.println("Serial commands: 1/2/3 = switch LCD screen, s = start motor, x = stop motor, r = reset faults");
}

void loop() {
  // ---- Sensor sampling (every SENSOR_READ_INTERVAL_MS) ----
  if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS) {
    updateAllSensors();
    runProtectionLogic();
    lastSensorReadTime = millis();

    // Status LED: ON solid = WiFi connected, blinking handled implicitly by fault buzzer
    digitalWrite(STATUS_LED_PIN, wifiConnected ? HIGH : LOW);
  }

  // ---- Display: button + serial input handled inside ----
  updateDisplay();

  // ---- Local web dashboard (works in both STA and Hotspot mode) ----
  handleWebServer();

  // ---- Blynk cloud (only runs if it was successfully initialized) ----
  handleBlynk();
}
