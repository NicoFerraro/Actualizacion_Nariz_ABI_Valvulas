#ifndef WEB_INDEX_H
#define WEB_INDEX_H

#include <Arduino.h>

// --- INTERFAZ WEB LOCAL (SD) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>NARIZ METATRÓN PRO</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0b0b0b; --card: #161616; --accent: #00ff88; }
    body { font-family: sans-serif; background: var(--bg); color: #fff; margin: 0; padding: 15px; }
    .header { display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 10px; margin-bottom: 20px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 15px; }
    .card { background: var(--card); border-radius: 12px; padding: 15px; text-align: center; }
    canvas { width: 100%; height: 180px; background: #000; border-radius: 5px; margin-top: 10px; }
    .val { font-size: 1.5em; font-weight: bold; color: var(--accent); }
    .nav-btn { border: 1px solid var(--accent); color: var(--accent); background: none; padding: 8px; border-radius: 5px; cursor: pointer; text-decoration: none; font-size: 0.8em; }
    .view { display: none; } .active { display: block; }
  </style>
</head>
<body>
  <div class="header">
    <h3>NARIZ METATRÓN <small id="date-display" style="color:#666"></small></h3>
    <div>
      <button class="nav-btn" onclick="showView('main')">Gráficos</button>
      <button class="nav-btn" onclick="showView('files')">Archivos</button>
    </div>
  </div>

  <div id="view-main" class="view active">
    <div class="grid">
      <div class="card">CO (PPM) <div id="v-co" class="val">0</div><canvas id="c-co"></canvas></div>
      <div class="card">O2 (%) <div id="v-o2" class="val">0</div><canvas id="c-o2"></canvas></div>
      <div class="card">CO2 (PPM) <div id="v-co2" class="val">0</div><canvas id="c-co2"></canvas></div>
      <div class="card">CH4 (%LEL) <div id="v-ch4" class="val">0</div><canvas id="c-ch4"></canvas></div>
    </div>
  </div>

  <div id="view-files" class="view">
    <div class="card" style="text-align:left;">
      <h4>📂 Archivos SD</h4>
      <div id="file-list">Cargando...</div>
      <hr>
      <h4>📡 Configurar WiFi</h4>
      <form action="/wifisave"><input name="s" placeholder="SSID"><input name="p" type="password"><input type="submit" value="Guardar"></form>
    </div>
  </div>

  <script>
    const dataHistory = { co: [], o2: [], co2: [], ch4: [] };
    const limit = 50;

    function draw(id, data, color) {
      const c = document.getElementById(id);
      const ctx = c.getContext('2d');
      const w = c.width = c.offsetWidth;
      const h = c.height = c.offsetHeight;
      ctx.clearRect(0,0,w,h);
      ctx.strokeStyle = color; ctx.lineWidth = 2;
      ctx.beginPath();
      const max = Math.max(...data, 1) * 1.2;
      data.forEach((v, i) => {
        const x = (i / (limit-1)) * w;
        const y = h - (v / max) * h;
        if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
      });
      ctx.stroke();
    }

    function update() {
      fetch('/data').then(r => r.json()).then(d => {
        document.getElementById('date-display').innerText = d.time;
        const keys = ['co','o2','co2','ch4'];
        keys.forEach(k => {
          document.getElementById('v-'+k).innerText = d[k];
          dataHistory[k].push(d[k]);
          if(dataHistory[k].length > limit) dataHistory[k].shift();
          draw('c-'+k, dataHistory[k], k==='o2'?'#00ccff':k==='co2'?'#00ff88':'#ffcc00');
        });
      });
    }

    function showView(v) { 
        document.querySelectorAll('.view').forEach(x => x.classList.remove('active'));
        document.getElementById('view-'+v).classList.add('active');
        if(v==='files') loadFiles();
    }

    function loadFiles() {
      fetch('/list').then(r => r.json()).then(files => {
        let h = '<table>';
        files.forEach(f => h += `<tr><td>${f.name}</td><td><a href="/get?file=${f.name}">Descargar</a></td></tr>`);
        document.getElementById('file-list').innerHTML = h + '</table>';
      });
    }

    setInterval(update, 1000);
  </script>
</body></html>
)rawliteral";

#endif