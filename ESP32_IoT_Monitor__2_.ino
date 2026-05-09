#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>

const char* ssid = "vrijeman";
const char* pass = "112233445566";

#define MQ5PIN 34
#define DHTPIN 4
#define SD_CS 5
#define LED_PIN 2 

DHT dht(DHTPIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

int interval = 2000;
unsigned long prevLCD = 0;
int lcdPage = 0;
long dataCounter = 0;
float lastH = 0, lastT = 0; 

void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>ALDEAN & FAAIZ ULTIMATE V45</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/xlsx/0.18.5/xlsx.full.min.js"></script>
<style>
  :root { --bg: #0d1117; --card: #161b22; --text: #c9d1d9; --prim: #58a6ff; --danger: #f85149; --safe: #3fb950; --warn: #d29922; }
  body { background: var(--bg); color: var(--text); font-family: sans-serif; padding: 15px; margin: 0; }
  .card { background: var(--card); border: 1px solid #30363d; border-radius: 12px; padding: 15px; margin-bottom: 15px; text-align: center; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 10px; }
  .val { font-size: 2.2em; font-weight: bold; margin: 5px 0; }
  .unit { font-size: 0.5em; color: #8b949e; }
  .danger-mode { background: #490000 !important; animation: blink 0.6s infinite; }
  @keyframes blink { 50% { opacity: 0.7; } }
  table { width: 100%; border-collapse: collapse; font-size: 0.8em; margin-top: 10px; }
  th, td { border: 1px solid #30363d; padding: 8px; text-align: center; }
  input, button { padding: 10px; border-radius: 6px; border: 1px solid #30363d; background: #21262d; color: white; cursor: pointer; }
</style></head><body>

<div class="card" id="alertCard">
  <h1 style="margin:0">INDUSTRIAL MONITORING V45</h1>
  <div class="grid" style="margin-top:10px">
    <div>Interval (ms): <input type="number" id="intv" value="2000" step="500" onchange="updateIntv()"></div>
    <button id="btn" onclick="toggle()" style="background:var(--safe)">START LOGGING</button>
    <button onclick="exportExcel()" style="background:#217346">SAVE EXCEL</button>
  </div>
  <div id="condMsg" style="font-weight:bold; margin-top:10px; font-size:1.2em">STATUS: READY</div>
</div>

<div class="grid">
  <div class="card">LPG<div class="val" id="vL" style="color:var(--danger)">0<span class="unit">PPM</span></div></div>
  <div class="card">CH4<div class="val" id="vM" style="color:var(--safe)">0<span class="unit">PPM</span></div></div>
  <div class="card">TEMP<div class="val" id="vT" style="color:yellow">0<span class="unit">°C</span></div></div>
  <div class="card">HUMIDITY<div class="val" id="vH" style="color:cyan">0<span class="unit">%</span></div></div>
</div>

<div class="grid">
  <div class="card" style="grid-column: span 2"><h3>Gas Analysis Trend</h3><canvas id="gCh"></canvas></div>
  <div class="card" style="grid-column: span 2"><h3>Environment Trend</h3><canvas id="eCh"></canvas></div>
</div>

<div class="card">
  <h3>Data Log Table (1-Hour Session)</h3>
  <table id="myTable"><thead><tr><th>No</th><th>Time</th><th>LPG</th><th>CH4</th><th>H2</th><th>Temp</th><th>Hum</th><th>Status</th></tr></thead><tbody id="tb"></tbody></table>
</div>

<script>
  let gCh, eCh, run = false, timer, count = 0;
  window.onload = () => {
    // Grafik Gas - 2 Garis (LPG & CH4)
    gCh = new Chart(document.getElementById('gCh'), {
      type: 'line', 
      data: { labels: [], datasets: [
        { label: 'LPG', borderColor: 'red', data: [], tension: 0.3 },
        { label: 'CH4', borderColor: '#3fb950', data: [], tension: 0.3 }
      ]}
    });
    // Grafik Lingkungan - 2 Garis (Temp & Hum)
    eCh = new Chart(document.getElementById('eCh'), {
      type: 'line', 
      data: { labels: [], datasets: [
        { label: 'Temp', borderColor: 'yellow', data: [] },
        { label: 'Hum', borderColor: 'cyan', data: [] }
      ]}
    });
  };

  function updateIntv() {
    const v = document.getElementById('intv').value;
    fetch('/set?v=' + v);
    if(run) { clearInterval(timer); timer = setInterval(update, v); }
  }

  function toggle() {
    run = !run;
    const b = document.getElementById('btn');
    b.innerText = run ? "STOP SYSTEM" : "START SYSTEM";
    b.style.background = run ? "var(--danger)" : "var(--safe)";
    if(run) timer = setInterval(update, document.getElementById('intv').value); else clearInterval(timer);
  }

  async function update() {
    try {
      const res = await fetch('/data');
      const d = await res.json();
      const now = new Date().toLocaleTimeString();
      count++;
      
      document.getElementById('vL').innerHTML = d.lpg.toFixed(0) + '<span class="unit">PPM</span>';
      document.getElementById('vM').innerHTML = d.ch4.toFixed(0) + '<span class="unit">PPM</span>';
      document.getElementById('vT').innerHTML = d.t.toFixed(1) + '<span class="unit">°C</span>';
      document.getElementById('vH').innerHTML = d.h.toFixed(1) + '<span class="unit">%</span>';

      const danger = d.lpg > 400 || d.ch4 > 500;
      document.getElementById('alertCard').className = danger ? "card danger-mode" : "card";
      document.getElementById('condMsg').innerText = danger ? "DANGER: GAS LEAK!" : "STATUS: SAFE";

      // Update Grafik Gas (2 datasets)
      gCh.data.labels.push(now); 
      gCh.data.datasets[0].data.push(d.lpg); 
      gCh.data.datasets[1].data.push(d.ch4); 
      
      // Update Grafik Env (2 datasets)
      eCh.data.labels.push(now); 
      eCh.data.datasets[0].data.push(d.t); 
      eCh.data.datasets[1].data.push(d.h);
      
      [gCh, eCh].forEach(c => { 
        if(c.data.labels.length > 15) { 
          c.data.labels.shift(); 
          c.data.datasets.forEach(s => s.data.shift()); 
        } 
        c.update('none'); 
      });

      const tb = document.getElementById('tb');
      tb.innerHTML = `<tr><td>${count}</td><td>${now}</td><td>${d.lpg.toFixed(0)}</td><td>${d.ch4.toFixed(0)}</td><td>${d.h2.toFixed(0)}</td><td>${d.t.toFixed(1)}</td><td>${d.h.toFixed(1)}</td><td>${danger?'DANGER':'SAFE'}</td></tr>` + tb.innerHTML;
      if(tb.rows.length > 50) tb.deleteRow(50);
    } catch(e) {}
  }
  function exportExcel() { XLSX.writeFile(XLSX.utils.table_to_book(document.getElementById("myTable")), "Data_Log_V45.xlsx"); }
</script></body></html>
)rawhtml";
  server.send(200, "text/html", html);
}

void handleData() {
  int r = analogRead(MQ5PIN);
  float l = (r / 4095.0) * 1000.0;
  float c = (r / 4095.0) * 1200.0;
  float h2 = (r / 4095.0) * 600.0;
  
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) lastT = t;
  if (!isnan(h)) lastH = h;

  String json = "{\"lpg\":"+String(l)+",\"ch4\":"+String(c)+",\"h2\":"+String(h2)+",\"t\":"+String(lastT)+",\"h\":"+String(lastH)+"}";
  server.send(200, "application/json", json);
  
  if(l > 400 || c > 500) digitalWrite(LED_PIN, HIGH); else digitalWrite(LED_PIN, LOW);
  
  File f = SD.open("/datalog.csv", FILE_APPEND);
  if(f){ 
    dataCounter++;
    f.printf("%ld,%.0f,%.0f,%.0f,%.1f,%.1f\n", dataCounter, l, c, h2, lastT, lastH); 
    f.close(); 
  }
}

void setup() {
  Serial.begin(115200); dht.begin(); lcd.init(); lcd.backlight(); pinMode(LED_PIN, OUTPUT);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED) delay(500);
  if(!SD.begin(SD_CS)) Serial.println("SD FAIL");
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/set", [](void){ interval = server.arg("v").toInt(); server.send(200); });
  server.begin();
}

void loop() {
  server.handleClient();
  if(millis() - prevLCD > 3000) {
    prevLCD = millis(); lcd.clear();
    if(lcdPage == 0) {
      lcd.print("LPG:"); lcd.print((int)((analogRead(MQ5PIN)/4095.0)*1000));
      lcd.setCursor(0,1); lcd.print("T:"); lcd.print(lastT,1); lcd.print(" H:"); lcd.print(lastH,0);
    } else {
      lcd.print("IP:"); lcd.print(WiFi.localIP().toString());
      lcd.setCursor(0,1); lcd.print("Intv:"); lcd.print(interval); lcd.print("ms");
    }
    lcdPage = !lcdPage;
  }
}