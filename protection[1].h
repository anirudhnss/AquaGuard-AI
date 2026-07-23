/*
  ===========================================================
  AquaGuard AI - protection.h
  ===========================================================
  Motor protection logic: fault detection, relay control,
  auto-restart with stability timer, and failure lockout.
  ===========================================================
*/

#ifndef PROTECTION_H
#define PROTECTION_H

#include "config.h"
#include "sensors.h"

enum FaultType {
  FAULT_NONE,
  FAULT_PHASE_LOSS,
  FAULT_OVERLOAD,
  FAULT_DRY_RUN,
  FAULT_CURRENT_IMBALANCE,
  FAULT_UNDER_VOLTAGE,
  FAULT_OVER_VOLTAGE,
  FAULT_LOCKED_OUT
};

struct ProtectionState {
  bool motorRunning = false;
  bool systemLocked = false;
  FaultType activeFault = FAULT_NONE;
  unsigned long faultStartTime = 0;
  unsigned long stableStartTime = 0;
  int restartAttempts = 0;
  bool waitingForRestart = false;
};

ProtectionState protState;

// ===========================================================
// Relay control helpers
// ===========================================================
void motorOn() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
  protState.motorRunning = true;
}

void motorOff() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  protState.motorRunning = false;
}

void buzzerOn()  { digitalWrite(BUZZER_PIN, HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); }

// ===========================================================
// Fault name lookup (for LCD / Serial / Blynk text)
// ===========================================================
const char* faultToString(FaultType f) {
  switch (f) {
    case FAULT_NONE:               return "OK";
    case FAULT_PHASE_LOSS:         return "PHASE LOSS";
    case FAULT_OVERLOAD:           return "OVERLOAD";
    case FAULT_DRY_RUN:            return "DRY RUN";
    case FAULT_CURRENT_IMBALANCE:  return "CURR IMBALANCE";
    case FAULT_UNDER_VOLTAGE:      return "UNDER VOLTAGE";
    case FAULT_OVER_VOLTAGE:       return "OVER VOLTAGE";
    case FAULT_LOCKED_OUT:         return "SYSTEM LOCKED";
    default:                       return "UNKNOWN";
  }
}

// ===========================================================
// Core fault evaluation — returns the highest-priority fault
// found, or FAULT_NONE if everything is healthy.
//
// Priority order matters: voltage faults checked first (can
// damage motor immediately), then phase loss, then overload,
// then dry run, then imbalance.
// ===========================================================
FaultType evaluateFaults() {
  // --- Voltage faults (checked even if motor is off) ---
  if (liveData.voltage < UNDER_VOLTAGE_LIMIT && liveData.voltage > 30.0) {
    // > 30V check avoids false trip when mains is simply absent/ESP32 just booted
    return FAULT_UNDER_VOLTAGE;
  }
  if (liveData.voltage > OVER_VOLTAGE_LIMIT) {
    return FAULT_OVER_VOLTAGE;
  }

  // --- Faults below only matter while motor is supposed to be running ---
  if (!protState.motorRunning) return FAULT_NONE;

  // --- Phase loss: any one phase reads near-zero while others don't ---
  bool rLost = liveData.currentR < PHASE_LOSS_CURRENT_LIMIT;
  bool yLost = liveData.currentY < PHASE_LOSS_CURRENT_LIMIT;
  bool bLost = liveData.currentB < PHASE_LOSS_CURRENT_LIMIT;
  int lostCount = rLost + yLost + bLost;

  if (lostCount > 0 && lostCount < 3) {
    return FAULT_PHASE_LOSS; // one or two phases lost, but not all (all-zero = dry run/off)
  }

  // --- Dry run: all three phases show very low current together ---
  if (liveData.currentR < DRY_RUN_CURRENT_LIMIT &&
      liveData.currentY < DRY_RUN_CURRENT_LIMIT &&
      liveData.currentB < DRY_RUN_CURRENT_LIMIT) {
    return FAULT_DRY_RUN;
  }

  // --- Overload: any phase exceeds the limit ---
  if (liveData.currentR > OVERLOAD_CURRENT_LIMIT ||
      liveData.currentY > OVERLOAD_CURRENT_LIMIT ||
      liveData.currentB > OVERLOAD_CURRENT_LIMIT) {
    return FAULT_OVERLOAD;
  }

  // --- Current imbalance between phases ---
  float avg = (liveData.currentR + liveData.currentY + liveData.currentB) / 3.0;
  if (avg > 0.5) { // only check imbalance if motor is actually drawing meaningful current
    float maxDevPercent = 0;
    float devR = abs(liveData.currentR - avg) / avg * 100.0;
    float devY = abs(liveData.currentY - avg) / avg * 100.0;
    float devB = abs(liveData.currentB - avg) / avg * 100.0;
    maxDevPercent = max(devR, max(devY, devB));
    if (maxDevPercent > CURRENT_IMBALANCE_PERCENT) {
      return FAULT_CURRENT_IMBALANCE;
    }
  }

  return FAULT_NONE;
}

// ===========================================================
// Smart Start Permission — checked before allowing motor ON
// ===========================================================
bool startPermissionGranted() {
  if (protState.systemLocked) return false;
  if (liveData.waterPercent < MIN_WATER_PERCENT_TO_START) return false;
  if (liveData.voltage < UNDER_VOLTAGE_LIMIT || liveData.voltage > OVER_VOLTAGE_LIMIT) return false;
  return true;
}

// ===========================================================
// Main protection state machine — call once per loop after
// updateAllSensors()
// ===========================================================
void runProtectionLogic() {
  if (protState.systemLocked) {
    motorOff();
    buzzerOn();
    protState.activeFault = FAULT_LOCKED_OUT;
    return;
  }

  FaultType detected = evaluateFaults();

  if (detected != FAULT_NONE) {
    // New fault detected -> trip motor off immediately
    if (protState.activeFault != detected) {
      protState.activeFault = detected;
      protState.faultStartTime = millis();
      protState.waitingForRestart = true;
      motorOff();
      buzzerOn();
      Serial.print("[FAULT] ");
      Serial.println(faultToString(detected));
    }
    protState.stableStartTime = 0; // reset stability timer, conditions not OK
    return;
  }

  // No fault currently present
  buzzerOff();

  if (protState.waitingForRestart) {
    // Start/continue the stability timer
    if (protState.stableStartTime == 0) {
      protState.stableStartTime = millis();
    }

    unsigned long stableFor = millis() - protState.stableStartTime;

    if (stableFor >= AUTO_RESTART_STABLE_TIME_MS) {
      // Conditions held stable for required time -> attempt restart
      protState.restartAttempts++;
      if (protState.restartAttempts > MAX_RESTART_ATTEMPTS) {
        protState.systemLocked = true;
        motorOff();
        Serial.println("[SYSTEM] Max restart attempts exceeded. System LOCKED.");
        return;
      }
      Serial.println("[SYSTEM] Conditions stable. Auto-restarting motor.");
      motorOn();
      protState.activeFault = FAULT_NONE;
      protState.waitingForRestart = false;
      protState.stableStartTime = 0;
    }
  } else {
    protState.activeFault = FAULT_NONE;
  }
}

// ===========================================================
// Manual reset (called from Blynk app button or serial command)
// ===========================================================
void resetSystem() {
  protState.systemLocked = false;
  protState.restartAttempts = 0;
  protState.activeFault = FAULT_NONE;
  protState.waitingForRestart = false;
  protState.stableStartTime = 0;
  buzzerOff();
  Serial.println("[SYSTEM] Manual reset performed.");
}

// ===========================================================
// Manual start request (from button/app) — respects permission
// ===========================================================
bool requestMotorStart() {
  if (!startPermissionGranted()) {
    Serial.println("[SYSTEM] Start denied - permission conditions not met.");
    return false;
  }
  motorOn();
  protState.activeFault = FAULT_NONE;
  protState.waitingForRestart = false;
  Serial.println("[SYSTEM] Motor started manually.");
  return true;
}

void requestMotorStop() {
  motorOff();
  protState.waitingForRestart = false;
  Serial.println("[SYSTEM] Motor stopped manually.");
}

#endif
