#ifndef WEB_INDEX_H
#define WEB_INDEX_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>NARIZ METATRÓN PRO</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0b0b0b; --card: #161616; --accent: #00ff88; --purga: #ff4444; }
    body { font-family: sans-serif; background: var(--bg); color: #fff; margin: 0; padding: 15px; }
    .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #333; padding-bottom: 10px; margin-bottom: 20px; }
    
    /* Estilo para el indicador de Válvula */
    .status-container { display: flex; align-items: center; gap: 10px; }
    #v-status { 
      background: #333; padding: 5px 12px; border-radius: 20px; 
      font-weight: bold; font-size: 0.9em; transition: all 0.3s;
      border: 1px solid #444;
    }
    .active-v { background: var(--accent) !important; color: #000; border-color: #fff !important; }
    .active-p { background: var(--purga) !important; color: #fff; border-color: #fff !important; }

    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 15px; }
    .card { background: var(--card); border-radius: 12px; padding: 15px; text-align: center; border: 1px solid #222; }
    canvas { width: 100%; height: 150px; background: #000; border-radius: 5px; margin-top: 10px; }
    .val { font-size: 1.8em; font-weight: bold; color: var(--accent); margin: 5px 0; }
    .nav-btn { border: 1px solid var(--accent); color: var(--accent); background: none; padding: 8px 15px; border-radius: 5px; cursor: pointer; text-decoration: none; font-size: 0.8em; }
    .view { display: none; } .active { display: block; }
    
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    td { padding: 8px; border-bottom: 1px solid #333; }
    .btn-dl { color: var(--accent); text-decoration: none; font-size: 0.9em; }
  </style>
</head>
<body>
  <div class="header">
    <div>
      <h3 style="margin:0">NARIZ METATRÓN</h3>
      <small id="date-display" style="color:#666"></small>
      <div style="font-size: 0.8em; color: #888;">
        WiFi: <span id="w-status">...</span> | IP: <span id="w-ip">...</span>
      </div>
    </div>
    <div class="status-container">
      <div id="v-status">ESPERANDO...</div>
      <button class="nav-btn" onclick="showView('main')">Gráficos</button>
      <button class="nav-btn" onclick="showView('files')">Archivos</button>
      <button class="nav-btn" onclick="showView('config')">Configuración</button>
    </div>
  </div>

  <div id="view-config" class="view">
    <div class="card" style="text-align:left;">
      <h4>🕒 Ajuste de Fecha y Hora</h4>
      
      <p>Manual:</p>
      <input type="date" id="manual-date">
      <input type="time" id="manual-time">
      <button class="nav-btn" onclick="saveManualTime()">Guardar Cambio</button>
      
      <hr style="border-color:#333">
      
      <p>Automático (desde el navegador):</p>
      <button class="nav-btn" onclick="syncBrowserTime()">Sincronizar ahora</button>
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
      <hr style="border-color:#333">
      <h4>📡 Configurar WiFi</h4>
      <form action="/wifisave">
        <input name="s" placeholder="SSID" style="padding:5px">
        <input name="p" type="password" placeholder="Password" style="padding:5px">
        <input type="submit" value="Guardar" class="nav-btn">
      </form>
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
        // Actualizar Reloj
        document.getElementById('w-status').innerText = d.wifiStatus;
        document.getElementById('w-ip').innerText = d.localIP;
        document.getElementById('date-display').innerText = d.date + " - " + d.time;
        
        // Actualizar Indicador de Válvula
        const statusEl = document.getElementById('v-status');
        statusEl.innerText = d.valvula;
        statusEl.className = ''; // Limpiar clases
        if(d.valvula === "PURGA") {
            statusEl.classList.add('active-p');
        } else {
            statusEl.classList.add('active-v');
        }

        // Actualizar Sensores y Gráficos
        const keys = ['co','o2','co2','ch4'];
        keys.forEach(k => {
          document.getElementById('v-'+k).innerText = d[k];
          dataHistory[k].push(d[k]);
          if(dataHistory[k].length > limit) dataHistory[k].shift();
          draw('c-'+k, dataHistory[k], k==='o2'?'#00ccff':k==='co2'?'#00ff88':'#ffcc00');
        });
      }).catch(e => console.error("Error fetching data"));
    }

    function saveManualTime() {
      // 1. Obtenemos los valores de los cuadros de texto
      const dateVal = document.getElementById('manual-date').value;
      const timeVal = document.getElementById('manual-time').value;

      // 2. Verificamos que no estén vacíos
      if (!dateVal || !timeVal) {
        alert("Por favor, completa la fecha y la hora 📅🕒");
        return;
      }

      // 3. Convertimos la fecha y hora a un número "Unix" (segundos desde 1970)
      const epoch = Math.floor(new Date(dateVal + ' ' + timeVal).getTime() / 1000);

      // 4. Se lo enviamos al ESP32
      fetch('/set_time?t=' + epoch).then(r => {
        if(r.ok) alert("✅ Fecha y hora manual actualizadas");
      });
    }

    function syncBrowserTime() {
      // 1. Tomamos la hora exacta de tu computadora o celular ahora mismo
      const epoch = Math.floor(Date.now() / 1000);

      // 2. Se la enviamos al ESP32 por la ruta que ya tienes en main.cpp
      fetch('/set_time?t=' + epoch).then(r => {
        if(r.ok) alert("🚀 Hora sincronizada con tu dispositivo");
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
        files.forEach(f => h += `<tr><td>${f.name}</td><td>${f.size}</td><td><a class="btn-dl" href="/get?file=${f.name}">Descargar</a></td></tr>`);
        document.getElementById('file-list').innerHTML = h + '</table>';
      });
    }

    setInterval(update, 1000);
  </script>
</body></html>
)rawliteral";

#endif