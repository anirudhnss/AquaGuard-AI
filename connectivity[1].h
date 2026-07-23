/*
  ===========================================================
  AquaGuard AI - connectivity.h
  ===========================================================
  WiFi connection logic:
   1. Try connecting to saved WiFi (STA mode) -> enables Cloud/Blynk
   2. If WiFi unavailable -> fallback to SoftAP Hotspot mode
      with local dashboard at 192.168.4.1

  Also hosts the offline local dashboard (simple HTML page
  served directly from the ESP32, works in BOTH modes).
  ===========================================================
*/

#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "sensors.h"
#include "protection.h"
#include "display.h"

// ---- EDIT THESE with your home/college WiFi credentials ----
const char* STA_SSID     = "YOUR_WIFI_SSID";
const char* STA_PASSWORD = "YOUR_WIFI_PASSWORD";

WebServer server(80);
bool isHotspotMode = false;
bool wifiConnected = false;

// ===========================================================
// Build the local dashboard HTML (auto-refreshes every 2s)
// ===========================================================
String buildDashboardHTML() {
  String html = "<!DOCTYPE html><html><head><title>AquaGuard AI</title>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;background:#0d1b2a;color:#e0fbfc;text-align:center;padding:20px;}";
  html += "h1{color:#48cae4;} .card{background:#1b263b;border-radius:12px;padding:15px;margin:10px auto;max-width:380px;}";
  html += ".val{font-size:28px;font-weight:bold;color:#90e0ef;} .fault{color:#ff4d4d;font-weight:bold;}";
  html += "button{padding:12px 20px;margin:8px;border:none;border-radius:8px;font-size:16px;cursor:pointer;}";
  html += ".btnOn{background:#2ecc71;color:white;} .btnOff{background:#e74c3c;color:white;} .btnReset{background:#f39c12;color:white;}";
  html += "</style></head><body>";

  html += "<h1>AquaGuard AI Dashboard</h1>";

  html += "<div class='card'><div>Water Level</div><div class='val'>";
  html += String((int)liveData.waterPercent) + "% (" + String((int)liveData.waterLiters) + " L)</div></div>";

  html += "<div class='card'><div>Voltage</div><div class='val'>" + String((int)liveData.voltage) + " V</div></div>";

  html += "<div class='card'><div>R / Y / B Current</div><div class='val'>";
  html += String(liveData.currentR, 1) + "A / " + String(liveData.currentY, 1) + "A / " + String(liveData.currentB, 1) + "A</div></div>";

  html += "<div class='card'><div>Motor Status</div><div class='val'>";
  html += protState.motorRunning ? "RUNNING" : "STOPPED";
  html += "</div></div>";

  html += "<div class='card'><div>Water Used</div><div class='val'>" + String((int)totalWaterUsedLiters) + " L</div></div>";

  html += "<div class='card'><div>Borewell Health</div><div class='val'>" + String(getBorewellHealth()) + "</div></div>";

  if (protState.activeFault != FAULT_NONE) {
    html += "<div class='card'><div>FAULT</div><div class='val fault'>" + String(faultToString(protState.activeFault)) + "</div></div>";
  }

  html += "<div class='card'>";
  html += "<form action='/start' method='get' style='display:inline'><button class='btnOn' type='submit'>START MOTOR</button></form>";
  html += "<form action='/stop' method='get' style='display:inline'><button class='btnOff' type='submit'>STOP MOTOR</button></form><br>";
  html += "<form action='/reset' method='get' style='display:inline'><button class='btnReset' type='submit'>RESET SYSTEM</button></form>";
  html += "</div>";

  html += "</body></html>";
  return html;
}

// ===========================================================
// HTTP route handlers
// ===========================================================
void handleRoot() {
  server.send(200, "text/html", buildDashboardHTML());
}

void handleStart() {
  requestMotorStart();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStop() {
  requestMotorStop();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReset() {
  resetSystem();
  server.sendHeader("Location", "/");
  server.send(303);
}

// Simple JSON endpoint, useful if you later want to fetch() from JS instead of full reload
void handleData() {
  String json = "{";
  json += "\"waterPercent\":" + String(liveData.waterPercent, 1) + ",";
  json += "\"waterLiters\":" + String(liveData.waterLiters, 1) + ",";
  json += "\"voltage\":" + String(liveData.voltage, 1) + ",";
  json += "\"currentR\":" + String(liveData.currentR, 2) + ",";
  json += "\"currentY\":" + String(liveData.currentY, 2) + ",";
  json += "\"currentB\":" + String(liveData.currentB, 2) + ",";
  json += "\"motorRunning\":" + String(protState.motorRunning ? "true" : "false") + ",";
  json += "\"fault\":\"" + String(faultToString(protState.activeFault)) + "\",";
  json += "\"waterUsed\":" + String(totalWaterUsedLiters, 1) + ",";
  json += "\"health\":\"" + String(getBorewellHealth()) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/reset", handleReset);
  server.on("/data", handleData);
  server.begin();
  Serial.println("[WEB] Dashboard server started.");
}

// ===========================================================
// WiFi connection attempt with fallback to hotspot
// ===========================================================
void initConnectivity() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASSWORD);

  Serial.print("[WIFI] Connecting to ");
  Serial.println(STA_SSID);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    isHotspotMode = false;
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] Failed. Starting Hotspot mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(HOTSPOT_SSID, HOTSPOT_PASSWORD);
    isHotspotMode = true;
    wifiConnected = false;
    Serial.print("[HOTSPOT] SSID: ");
    Serial.println(HOTSPOT_SSID);
    Serial.print("[HOTSPOT] IP Address: ");
    Serial.println(WiFi.softAPIP()); // typically 192.168.4.1
  }

  setupWebServer();
}

void handleWebServer() {
  server.handleClient();
}

#endif
