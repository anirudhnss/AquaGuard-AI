/*
  ===========================================================
  AquaGuard AI - sensors.h
  ===========================================================
  Handles all physical sensor reads:
   - ACS712 current sensors (R, Y, B phases) -> RMS Amps
   - ZMPT101B voltage sensor -> RMS Volts
   - HC-SR04 ultrasonic -> water %, water Litres
  ===========================================================
*/

#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <Arduino.h>

// ---------- Live sensor data structure ----------
struct SensorData {
  float currentR;
  float currentY;
  float currentB;
  float voltage;
  float distanceCM;
  float waterPercent;
  float waterLiters;
};

SensorData liveData;

// ===========================================================
// Read RMS current from an ACS712 channel
// ===========================================================
float readACS712_RMS(int pin) {
  float sumSquares = 0;
  // Find the zero-current baseline (midpoint) by averaging first
  long sumRaw = 0;
  for (int i = 0; i < ACS712_SAMPLES; i++) {
    sumRaw += analogRead(pin);
    delayMicroseconds(100);
  }
  float zeroOffset = (float)sumRaw / ACS712_SAMPLES;

  for (int i = 0; i < ACS712_SAMPLES; i++) {
    int raw = analogRead(pin);
    float voltage = ((raw - zeroOffset) * ADC_VREF) / ADC_RESOLUTION;
    sumSquares += voltage * voltage;
    delayMicroseconds(100);
  }
  float vRMS = sqrt(sumSquares / ACS712_SAMPLES);
  float currentRMS = vRMS / ACS712_SENSITIVITY;

  // Clean up tiny noise readings near zero
  if (currentRMS < 0.15) currentRMS = 0.0;
  return currentRMS;
}

// ===========================================================
// Read RMS voltage from ZMPT101B
// ===========================================================
float readZMPT_RMS() {
  long sumRaw = 0;
  for (int i = 0; i < ZMPT_SAMPLES; i++) {
    sumRaw += analogRead(VOLTAGE_PIN);
    delayMicroseconds(100);
  }
  float zeroOffset = (float)sumRaw / ZMPT_SAMPLES;

  float sumSquares = 0;
  for (int i = 0; i < ZMPT_SAMPLES; i++) {
    int raw = analogRead(VOLTAGE_PIN);
    float v = ((raw - zeroOffset) * ADC_VREF) / ADC_RESOLUTION;
    sumSquares += v * v;
    delayMicroseconds(100);
  }
  float vRMS = sqrt(sumSquares / ZMPT_SAMPLES);
  float mainsVoltage = vRMS * ZMPT_CALIBRATION;
  return mainsVoltage;
}

// ===========================================================
// Read distance from HC-SR04 (returns cm)
// ===========================================================
float readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout (~5m range)
  if (duration == 0) {
    return -1; // sensor timeout / no echo - signal invalid reading
  }
  float distanceCM = duration * 0.0343 / 2.0;
  return distanceCM;
}

// ===========================================================
// Convert distance to water % and Liters
// ===========================================================
void calculateWaterLevel(float distanceCM, float &percent, float &liters) {
  if (distanceCM < 0) {
    // invalid reading, keep last known good value (handled by caller)
    percent = liveData.waterPercent;
    liters = liveData.waterLiters;
    return;
  }

  // Clamp distance within calibrated range
  float d = distanceCM;
  if (d < WATER_FULL_DISTANCE_CM) d = WATER_FULL_DISTANCE_CM;
  if (d > WATER_EMPTY_DISTANCE_CM) d = WATER_EMPTY_DISTANCE_CM;

  float waterHeightCM = WATER_EMPTY_DISTANCE_CM - d; // how much water column exists
  float maxHeightCM = WATER_EMPTY_DISTANCE_CM - WATER_FULL_DISTANCE_CM;

  percent = (waterHeightCM / maxHeightCM) * 100.0;

  float radiusCM = TANK_DIAMETER_CM / 2.0;
  float volumeCM3 = PI * radiusCM * radiusCM * waterHeightCM;
  liters = volumeCM3 / 1000.0; // 1 Litre = 1000 cm^3
}

// ===========================================================
// Master sensor update — call this once per sensor cycle
// ===========================================================
void updateAllSensors() {
  liveData.currentR = readACS712_RMS(CURRENT_R_PIN);
  liveData.currentY = readACS712_RMS(CURRENT_Y_PIN);
  liveData.currentB = readACS712_RMS(CURRENT_B_PIN);
  liveData.voltage  = readZMPT_RMS();

  float d = readUltrasonicDistance();
  if (d > 0) {
    liveData.distanceCM = d;
  }
  calculateWaterLevel(liveData.distanceCM, liveData.waterPercent, liveData.waterLiters);
}

#endif
