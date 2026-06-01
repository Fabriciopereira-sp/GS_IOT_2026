/*
 * AgroSat Station v2 - Global Solution FIAP 2026/1
 *
 * Componentes:
 *  - DHT22          -> GPIO 4   (entrada 1: temperatura e umidade)
 *  - Soil Moisture  -> GPIO 34  (entrada 2: umidade do solo)
 *  - LED Verde      -> GPIO 26  (saida 1: status normal)
 *  - LED Vermelho   -> GPIO 27  (saida 2: alerta piscante)
 *  - Buzzer         -> GPIO 25  (saida 3: alerta sonoro)
 *  - LCD I2C 16x2   -> SDA 21 / SCL 22
 *
 * Endpoints:
 *  GET /         -> Dashboard com grafico historico
 *  GET /status   -> JSON completo
 *  GET /temp     -> JSON temperatura
 *  GET /humidity -> JSON umidade do ar
 *  GET /soil     -> JSON umidade do solo
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHTesp.h>
#include <LiquidCrystal_I2C.h>

const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

#define DHT_PIN     4
#define SOIL_PIN    34
#define LED_GREEN   26
#define LED_RED     27
#define BUZZER_PIN  25

#define TEMP_MAX      35.0
#define HUMIDITY_MIN  30.0
#define SOIL_DRY      2500

DHTesp            dht;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer         server(80);

float   temperature  = 0;
float   humidity     = 0;
int     soilRaw      = 0;
int     soilPct      = 0;
bool    alertActive  = false;
String  alertMessage = "OK";

unsigned long lastRead    = 0;
unsigned long lastBlink   = 0;
unsigned long lastBeep    = 0;
bool          ledRedState = false;

#define HIST_SIZE 30
float histTemp[HIST_SIZE];
float histHum[HIST_SIZE];
int   histSoil[HIST_SIZE];
int   histIndex = 0;
bool  histFull  = false;

void pushHistory() {
  histTemp[histIndex]  = temperature;
  histHum[histIndex]   = humidity;
  histSoil[histIndex]  = soilPct;
  histIndex = (histIndex + 1) % HIST_SIZE;
  if (histIndex == 0) histFull = true;
}

String buildHistoryJSON(float* arr) {
  int count = histFull ? HIST_SIZE : histIndex;
  int start = histFull ? histIndex : 0;
  String s = "[";
  for (int i = 0; i < count; i++) {
    if (i) s += ",";
    s += String(arr[(start + i) % HIST_SIZE], 1);
  }
  s += "]";
  return s;
}

String buildHistoryJSONInt(int* arr) {
  int count = histFull ? HIST_SIZE : histIndex;
  int start = histFull ? histIndex : 0;
  String s = "[";
  for (int i = 0; i < count; i++) {
    if (i) s += ",";
    s += String(arr[(start + i) % HIST_SIZE]);
  }
  s += "]";
  return s;
}

void readSensors() {
  TempAndHumidity data = dht.getTempAndHumidity();
  if (!isnan(data.temperature)) temperature = data.temperature;
  if (!isnan(data.humidity))    humidity    = data.humidity;

  soilRaw = analogRead(SOIL_PIN);
  soilPct = map(soilRaw, 4095, 0, 0, 100);
  soilPct = constrain(soilPct, 0, 100);

  alertActive  = false;
  alertMessage = "OK";

  if (temperature > TEMP_MAX) {
    alertActive  = true;
    alertMessage = "CALOR EXCESSIVO";
  } else if (humidity < HUMIDITY_MIN) {
    alertActive  = true;
    alertMessage = "SECA NO AR";
  } else if (soilPct < 20) {
    alertActive  = true;
    alertMessage = "SOLO MUITO SECO";
  }

  digitalWrite(LED_GREEN, alertActive ? LOW : HIGH);
  pushHistory();
}

void handleAlertEffects() {
  if (alertActive) {
    if (millis() - lastBlink >= 400) {
      lastBlink   = millis();
      ledRedState = !ledRedState;
      digitalWrite(LED_RED, ledRedState);
    }
    if (millis() - lastBeep >= 2000) {
      lastBeep = millis();
      tone(BUZZER_PIN, 1000, 150);
    }
  } else {
    digitalWrite(LED_RED, LOW);
    ledRedState = false;
    noTone(BUZZER_PIN);
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C U:");
  lcd.print((int)humidity);
  lcd.print("%");
  lcd.setCursor(0, 1);
  if (alertActive) {
    lcd.print("!");
    lcd.print(alertMessage.substring(0, 15));
  } else {
    lcd.print("Solo:");
    lcd.print(soilPct);
    lcd.print("% OK");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"temperatura_c\":"    + String(temperature, 2) + ",";
  json += "\"umidade_ar_pct\":"   + String(humidity, 2)    + ",";
  json += "\"solo_umidade_pct\":" + String(soilPct)        + ",";
  json += "\"solo_raw\":"         + String(soilRaw)        + ",";
  json += "\"alerta\":"           + String(alertActive ? "true" : "false") + ",";
  json += "\"mensagem\":\""       + alertMessage + "\",";
  json += "\"historico_temp\":"   + buildHistoryJSON(histTemp)    + ",";
  json += "\"historico_hum\":"    + buildHistoryJSON(histHum)     + ",";
  json += "\"historico_solo\":"   + buildHistoryJSONInt(histSoil) + ",";
  json += "\"uptime_ms\":"        + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}

void handleTemp() {
  server.send(200, "application/json",
    "{\"temperatura_c\":" + String(temperature, 2) + "}");
}

void handleHumidity() {
  server.send(200, "application/json",
    "{\"umidade_ar_pct\":" + String(humidity, 2) + "}");
}

void handleSoil() {
  server.send(200, "application/json",
    "{\"solo_umidade_pct\":" + String(soilPct) +
    ",\"solo_raw\":"         + String(soilRaw) + "}");
}

void handleRoot() {
  String tArr = buildHistoryJSON(histTemp);
  String sArr = buildHistoryJSONInt(histSoil);
  String stCls = alertActive ? "alert" : "ok";
  String stMsg = alertMessage;

  String html = R"(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="5">
<title>AgroSat Station</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0d1117;color:#e6edf3;padding:20px}
h1{text-align:center;color:#58a6ff;font-size:1.5rem;margin-bottom:4px}
.sub{text-align:center;color:#8b949e;font-size:.8rem;margin-bottom:20px}
.cards{display:flex;flex-wrap:wrap;gap:12px;justify-content:center;margin-bottom:24px}
.card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px 24px;min-width:140px;text-align:center}
.card .lbl{font-size:.7rem;color:#8b949e;text-transform:uppercase;margin-bottom:4px}
.card .val{font-size:1.8rem;font-weight:700}
.card .unit{font-size:.75rem;color:#8b949e}
.temp .val{color:#f97316}
.hum  .val{color:#38bdf8}
.soil .val{color:#a3e635}
.status-ok{background:#0d2b1f;border-color:#22c55e}
.status-al
