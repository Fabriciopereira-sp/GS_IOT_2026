#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

#define DHT_PIN     4
#define DHT_TYPE    DHT22
#define SOIL_PIN    34
#define LED_GREEN   26
#define LED_RED     27
#define BUZZER_PIN  25

#define TEMP_MAX      35.0
#define HUMIDITY_MIN  30.0

DHT               dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer         server(80);

float   temperature  = 25.0;
float   humidity     = 60.0;
int     soilRaw      = 0;
int     soilPct      = 50;
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

void lcdPrint(int col, int row, String text, int totalWidth) {
  lcd.setCursor(col, row);
  while ((int)text.length() < totalWidth) text += " ";
  lcd.print(text.substring(0, totalWidth));
}

void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity    = h;

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
  String linha0 = "T:" + String(temperature, 1) + "C U:" + String((int)humidity) + "%";
  lcdPrint(0, 0, linha0, 16);

  String linha1;
  if (alertActive) {
    linha1 = "!" + alertMessage;
  } else {
    linha1 = "Solo:" + String(soilPct) + "% OK";
  }
  lcdPrint(0, 1, linha1, 16);
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
.status-alert{background:#2b0d0d;border-color:#ef4444;animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
.status-ok .val{color:#22c55e;font-size:1rem}
.status-alert .val{color:#ef4444;font-size:1rem}
.chart-box{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px;margin-bottom:16px}
.chart-box h2{font-size:.85rem;color:#8b949e;margin-bottom:12px;font-weight:400}
canvas{max-height:180px}
footer{text-align:center;font-size:.7rem;color:#484f58;margin-top:16px}
</style>
</head>
<body>
<h1>&#x1F6F0;&#xFE0F; AgroSat Station</h1>
<p class="sub">Monitoramento de solo via dados satelitais &mdash; FIAP GS 2026/1</p>
<div class="cards">
  <div class="card temp"><div class="lbl">Temperatura</div><div class="val">)";
  html += String(temperature, 1);
  html += R"(</div><div class="unit">&deg;C</div></div>
  <div class="card hum"><div class="lbl">Umidade do ar</div><div class="val">)";
  html += String((int)humidity);
  html += R"(</div><div class="unit">%</div></div>
  <div class="card soil"><div class="lbl">Umidade do solo</div><div class="val">)";
  html += String(soilPct);
  html += R"(</div><div class="unit">%</div></div>
  <div class="card status-)";
  html += stCls;
  html += R"("><div class="lbl">Status</div><div class="val">)";
  html += stMsg;
  html += R"(</div></div>
</div>
<div class="chart-box">
  <h2>Temperatura &deg;C (historico)</h2>
  <canvas id="cT"></canvas>
</div>
<div class="chart-box">
  <h2>Umidade do solo % (historico)</h2>
  <canvas id="cS"></canvas>
</div>
<footer>Atualiza a cada 5s &nbsp;|&nbsp; ESP32 WebServer &nbsp;|&nbsp; IoT FIAP GS 2026/1</footer>
<script>
const tData=)";
  html += tArr;
  html += R"(;
const sData=)";
  html += sArr;
  html += R"(;
const labels=tData.map((_,i)=>i+1);
const cfg=(label,data,color)=>({
  type:'line',
  data:{labels,datasets:[{label,data,borderColor:color,backgroundColor:color+'22',
    borderWidth:2,pointRadius:2,tension:.3,fill:true}]},
  options:{responsive:true,plugins:{legend:{display:false}},
    scales:{x:{display:false},y:{ticks:{color:'#8b949e'},grid:{color:'#21262d'}}}}
});
new Chart(document.getElementById('cT'),cfg('Temp',tData,'#f97316'));
new Chart(document.getElementById('cS'),cfg('Solo',sData,'#a3e635'));
</script>
</body></html>)";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);

  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("AgroSat v2");
  lcd.setCursor(0, 1); lcd.print("Conectando...");

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("IP:");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
  delay(2000);

  server.on("/",         handleRoot);
  server.on("/status",   handleStatus);
  server.on("/temp",     handleTemp);
  server.on("/humidity", handleHumidity);
  server.on("/soil",     handleSoil);
  server.begin();
  Serial.println("Servidor iniciado.");
}

void loop() {
  server.handleClient();
  handleAlertEffects();

  if (millis() - lastRead >= 1000) {
    lastRead = millis();
    readSensors();
    updateLCD();
    Serial.printf("[AgroSat] T:%.1f C  H:%.1f%%  Solo:%d%%  %s\n",
      temperature, humidity, soilPct,
      alertActive ? alertMessage.c_str() : "OK");
  }
}
