/*
  ===========================================================
  AquaGuard AI - config.h
  Smart Borewell Intelligence System (SBIS)
  ===========================================================
  All pin mappings, calibration constants, and thresholds.
  Edit values in THIS file only — never hunt through main code
  to change a threshold or pin.
  ===========================================================
*/

#ifndef CONFIG_H
#define CONFIG_H

// ---------------- I2C LCD ----------------
#define LCD_ADDRESS   0x27
#define LCD_COLS      16
#define LCD_ROWS      2
// SDA = GPIO21, SCL = GPIO22 (ESP32 default I2C pins, set automatically by Wire.begin())

// ---------------- Ultrasonic Sensor (HC-SR04) ----------------
#define TRIG_PIN      5
#define ECHO_PIN      18

// ---------------- Current Sensors (ACS712-20A) ----------------
#define CURRENT_R_PIN 34
#define CURRENT_Y_PIN 35
#define CURRENT_B_PIN 32

// ---------------- Voltage Sensor (ZMPT101B) ----------------
#define VOLTAGE_PIN   33

// ---------------- Relay (Motor Contactor) ----------------
#define RELAY_PIN     27
#define RELAY_ACTIVE_LOW  true   // Relay module triggers ON when pin is LOW

// ---------------- Buzzer ----------------
#define BUZZER_PIN    26

// ---------------- Push Button (manual screen switch) ----------------
#define BUTTON_PIN    13         // Wired to GND, uses INPUT_PULLUP

// ---------------- Status LED ----------------
#define STATUS_LED_PIN 2

// ===========================================================
// CALIBRATION CONSTANTS — adjust after bench testing
// ===========================================================

// --- ACS712-20A ---
// 20A module sensitivity = 100mV/A, zero current = Vcc/2
#define ACS712_SENSITIVITY   0.100   // V per Amp (100mV/A for 20A variant)
#define ADC_VREF              3.3
#define ADC_RESOLUTION        4095.0
#define ACS712_SAMPLES         200    // samples per RMS reading

// --- ZMPT101B ---
// Calibrate this multiplier against a known mains voltage (see calibration notes)
#define ZMPT_CALIBRATION       260.0  // multiply raw RMS by this to get actual volts
#define ZMPT_SAMPLES            500

// --- HC-SR04 / Tank-Borewell Geometry ---
#define SENSOR_MOUNT_HEIGHT_CM   200.0  // distance from sensor to bottom (empty) level
#define WATER_FULL_DISTANCE_CM    20.0  // distance reading when tank/borewell is FULL
#define WATER_EMPTY_DISTANCE_CM  200.0  // distance reading when EMPTY
#define TANK_DIAMETER_CM          100.0 // for volume calc (cylindrical tank assumption)
// Volume (Litres) = PI * r^2 * height(cm) / 1000

// ===========================================================
// FAULT THRESHOLDS
// ===========================================================
#define RATED_VOLTAGE        230.0
#define UNDER_VOLTAGE_LIMIT  180.0
#define OVER_VOLTAGE_LIMIT   260.0

#define RATED_CURRENT          8.0    // amps, adjust to your demo load
#define OVERLOAD_CURRENT_LIMIT 10.0
#define DRY_RUN_CURRENT_LIMIT   1.0   // below this = motor likely dry running
#define PHASE_LOSS_CURRENT_LIMIT 0.3  // below this = phase considered "lost"
#define CURRENT_IMBALANCE_PERCENT 20.0 // % deviation between phases allowed

#define MIN_WATER_PERCENT_TO_START 10.0 // don't allow motor start if water too low

// ===========================================================
// TIMING CONSTANTS
// ===========================================================
#define SCREEN_CYCLE_INTERVAL_MS   3000   // auto screen change every 3s
#define SENSOR_READ_INTERVAL_MS     1000  // read sensors every 1s
#define AUTO_RESTART_STABLE_TIME_MS 30000 // 30s stable before auto restart
#define MAX_RESTART_ATTEMPTS         3    // lock system after this many faults
#define DEBOUNCE_DELAY_MS             50

// ===========================================================
// WIFI / CONNECTIVITY
// ===========================================================
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define HOTSPOT_SSID      "AquaGuard_AI"
#define HOTSPOT_PASSWORD  "aquaguard123"   // min 8 chars required by ESP32 SoftAP

// ===========================================================
// BLYNK CONFIGURATION
// ===========================================================
// Get these 3 from your Blynk.Cloud Template -> Info tab
#define BLYNK_TEMPLATE_ID_VALUE   "TMPxxxxxxx"      // <-- replace with your Template ID
#define BLYNK_TEMPLATE_NAME_VALUE "AquaGuard AI"     // <-- replace with your Template Name
#define BLYNK_AUTH_TOKEN_VALUE    "xxxxxxxxxxxxx"    // <-- replace with your Device Auth Token

// Blynk Virtual Pin Map
#define VPIN_WATER_PERCENT   V0
#define VPIN_WATER_LITERS    V1
#define VPIN_VOLTAGE         V2
#define VPIN_CURRENT_R       V3
#define VPIN_CURRENT_Y       V4
#define VPIN_CURRENT_B       V5
#define VPIN_MOTOR_STATUS    V6
#define VPIN_FAULT_TEXT      V7
#define VPIN_WATER_USED      V8
#define VPIN_BOREWELL_HEALTH V9
#define VPIN_MOTOR_SWITCH    V10   // app button to start/stop motor (input from app)
#define VPIN_RESET_SYSTEM    V11   // app button to reset fault lock (input from app)

#endif
