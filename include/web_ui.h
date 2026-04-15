#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Nariz Metatron</title>
  <style>
    :root {
      --bg: #071218;
      --card: #10212c;
      --line: #26485e;
      --text: #eef6fb;
      --muted: #9cb4c3;
      --accent: #57e3a0;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      padding: 18px;
      font-family: "Segoe UI", Tahoma, sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at top left, rgba(87, 227, 160, 0.15), transparent 35%),
        radial-gradient(circle at top right, rgba(76, 173, 255, 0.15), transparent 30%),
        linear-gradient(160deg, #041017 0%, #071218 45%, #0d1c25 100%);
    }
    .shell { max-width: 1220px; margin: 0 auto; }
    .header { display:flex; flex-wrap:wrap; justify-content:space-between; gap:14px; align-items:center; margin-bottom:18px; }
    .title h1 { margin:0; font-size:1.8rem; letter-spacing:0.08em; text-transform:uppercase; }
    .subtitle { color:var(--muted); font-size:0.95rem; margin-top:6px; }
    .nav { display:flex; flex-wrap:wrap; gap:10px; }
    .nav button, .btn, .small-btn {
      border:1px solid var(--accent);
      color:var(--text);
      background:rgba(87, 227, 160, 0.08);
      padding:10px 14px;
      border-radius:999px;
      cursor:pointer;
      font-weight:600;
    }
    .small-btn { padding:8px 12px; }
    .hero { display:grid; grid-template-columns:1.2fr 1fr; gap:16px; margin-bottom:18px; }
    .card {
      background:linear-gradient(180deg, rgba(16, 33, 44, 0.96), rgba(12, 27, 36, 0.96));
      border:1px solid var(--line);
      border-radius:18px;
      padding:16px;
      box-shadow:0 18px 35px rgba(0,0,0,0.22);
    }
    .section-title, .sensor-card h3 {
      margin:0 0 10px 0;
      font-size:1rem;
      color:var(--muted);
      text-transform:uppercase;
      letter-spacing:0.08em;
    }
    .status-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(160px, 1fr)); gap:12px; }
    .status-pill {
      padding:12px 14px;
      border-radius:14px;
      border:1px solid rgba(255,255,255,0.08);
      background:rgba(255,255,255,0.03);
    }
    .status-pill small { color:var(--muted); display:block; text-transform:uppercase; letter-spacing:0.08em; }
    .status-pill b { display:block; margin-top:6px; }
    .state-sample { border-color:rgba(87, 227, 160, 0.5); }
    .state-purge { border-color:rgba(240, 98, 98, 0.55); }
    .state-idle { border-color:rgba(242, 184, 75, 0.5); }
    .views { margin-top:16px; }
    .view { display:none; }
    .view.active { display:block; }
    .grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(260px, 1fr)); gap:15px; }
    .value { font-size:2rem; font-weight:700; color:var(--accent); margin-bottom:10px; }
    canvas { width:100%; height:150px; border-radius:12px; background:rgba(0,0,0,0.28); }
    .config-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:15px; }
    label { display:block; font-size:0.9rem; color:var(--muted); margin-bottom:6px; }
    input[type="number"], input[type="text"], input[type="password"], input[type="date"], input[type="time"], select {
      width:100%;
      border-radius:12px;
      border:1px solid var(--line);
      background:rgba(3, 12, 17, 0.72);
      color:var(--text);
      padding:11px 12px;
      margin-bottom:10px;
    }
    input[type="checkbox"] { transform:scale(1.2); margin-right:8px; }
    .inline { display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
    .hint { color:var(--muted); font-size:0.88rem; line-height:1.45; margin-bottom:10px; }
    .message {
      margin-top:10px;
      padding:10px 12px;
      border-radius:12px;
      background:rgba(255,255,255,0.05);
      border:1px solid rgba(255,255,255,0.08);
      min-height:42px;
      white-space:pre-wrap;
    }
    table { width:100%; border-collapse:collapse; }
    td, th { padding:10px 8px; border-bottom:1px solid rgba(255,255,255,0.08); text-align:left; }
    .link { color:var(--accent); text-decoration:none; font-weight:600; }
    .hidden { display:none !important; }
    .login-overlay {
      position:fixed;
      inset:0;
      background:rgba(2, 10, 14, 0.88);
      display:flex;
      align-items:center;
      justify-content:center;
      padding:18px;
      z-index:1000;
      backdrop-filter:blur(10px);
    }
    .login-card {
      width:min(420px, 100%);
      background:linear-gradient(180deg, rgba(16, 33, 44, 0.98), rgba(10, 24, 32, 0.98));
      border:1px solid var(--line);
      border-radius:20px;
      padding:22px;
      box-shadow:0 24px 50px rgba(0,0,0,0.34);
    }
    .login-card h2 { margin:0 0 8px 0; text-transform:uppercase; letter-spacing:0.08em; }
    .login-actions { display:flex; gap:10px; flex-wrap:wrap; margin-top:8px; }
    .role-chip {
      display:inline-flex;
      align-items:center;
      padding:9px 12px;
      border-radius:999px;
      border:1px solid rgba(255,255,255,0.1);
      background:rgba(255,255,255,0.05);
      color:var(--muted);
      font-size:0.9rem;
    }
    @media (max-width: 860px) {
      .hero { grid-template-columns:1fr; }
    }
  </style>
</head>
<body>
  <div id="login-overlay" class="login-overlay">
    <div class="login-card">
      <h2>Ingresar</h2>
      <div class="hint">Identificate para usar la nariz. El acceso maestro fijo es <b>admin / admin</b>.</div>
      <label for="login-user">Usuario</label><input id="login-user" type="text" autocomplete="username" value="admin">
      <label for="login-password">Clave</label><input id="login-password" type="password" autocomplete="current-password" value="admin">
      <div class="login-actions">
        <button class="btn" onclick="login()">Entrar</button>
      </div>
      <div class="message" id="msg-login">Esperando credenciales.</div>
    </div>
  </div>
  <div class="shell">
    <div class="header">
      <div class="title">
        <h1 id="product-name">Nariz Metatron</h1>
        <div class="subtitle" id="date-display">Sin datos de reloj</div>
      </div>
      <div class="nav">
        <div class="role-chip" id="role-chip">Sin sesion</div>
        <button onclick="showView('main')">Graficos</button>
        <button onclick="showView('files')">Archivos</button>
        <button id="nav-config" onclick="showView('config')">Configuracion</button>
        <button onclick="logout()">Salir</button>
      </div>
    </div>

    <div class="hero">
      <div class="card">
        <div class="section-title">Estado del sistema</div>
        <div class="status-grid">
          <div class="status-pill" id="pill-state"><small>Modo</small><b id="state-label">-</b></div>
          <div class="status-pill"><small id="source-title">Origen actual</small><b id="source-label">-</b></div>
          <div class="status-pill"><small id="detail-title">Detalle</small><b id="detail-label">-</b></div>
          <div class="status-pill"><small id="cycle-title">Tiempo restante</small><b id="cycle-label">-</b></div>
        </div>
      </div>
      <div class="card">
        <div class="section-title">Red y firmware</div>
        <div class="status-grid">
          <div class="status-pill"><small>WiFi STA</small><b id="wifi-status">-</b></div>
          <div class="status-pill"><small>SSID / IP</small><b id="wifi-ip">-</b></div>
          <div class="status-pill"><small>Access Point</small><b id="ap-ip">-</b></div>
          <div class="status-pill"><small>Firmware</small><b id="firmware-version">-</b></div>
        </div>
        <div class="message" id="ota-summary">Sin informacion OTA.</div>
      </div>
    </div>

    <div class="views">
      <div id="view-main" class="view active">
        <div class="grid">
          <div class="card sensor-card"><h3>CO (ppm)</h3><div class="value" id="v-co">0</div><canvas id="c-co"></canvas></div>
          <div class="card sensor-card"><h3>H2S (ppm)</h3><div class="value" id="v-h2s">0</div><canvas id="c-h2s"></canvas></div>
          <div class="card sensor-card"><h3>O2 (%)</h3><div class="value" id="v-o2">0</div><canvas id="c-o2"></canvas></div>
          <div class="card sensor-card"><h3>CH4 (%LEL)</h3><div class="value" id="v-ch4">0</div><canvas id="c-ch4"></canvas></div>
          <div class="card sensor-card"><h3>CO2 (ppm)</h3><div class="value" id="v-co2">0</div><canvas id="c-co2"></canvas></div>
        </div>
      </div>
      <div id="view-files" class="view">
        <div class="card">
          <div class="section-title">Archivos en la SD</div>
          <div class="hint">Los usuarios de solo lectura pueden descargar archivos. Solo operador y maestro pueden borrarlos.</div>
          <div id="file-list" class="message">Cargando lista...</div>
        </div>
      </div>
      <div id="view-config" class="view">
        <div class="config-grid">
          <div class="card">
            <div class="section-title" id="sampling-title">Programacion de muestreo</div>
            <div id="sampling-config-fields">
              <label for="cfg-v1">Valvula 1 (segundos, 0 = deshabilitada)</label><input id="cfg-v1" type="number" min="0" max="86400">
              <label for="cfg-v2">Valvula 2 (segundos, 0 = deshabilitada)</label><input id="cfg-v2" type="number" min="0" max="86400">
              <label for="cfg-v3">Valvula 3 (segundos, 0 = deshabilitada)</label><input id="cfg-v3" type="number" min="0" max="86400">
              <label for="cfg-v4">Valvula 4 (segundos, 0 = deshabilitada)</label><input id="cfg-v4" type="number" min="0" max="86400">
              <label for="cfg-purge">Purga entre muestras (segundos, 0 = sin purga)</label><input id="cfg-purge" type="number" min="0" max="86400">
              <div class="hint" id="sampling-description">Configura el tiempo de cada etapa de muestreo.</div>
              <button class="btn" onclick="saveValveConfig()">Guardar configuracion</button>
            </div>
            <div class="hint hidden" id="sampling-static-note"></div>
            <div class="message" id="msg-sampling"></div>
          </div>
          <div class="card">
            <div class="section-title">Fecha y hora</div>
            <label for="manual-date">Fecha</label><input id="manual-date" type="date">
            <label for="manual-time">Hora</label><input id="manual-time" type="time" step="1">
            <div class="inline">
              <button class="btn" onclick="saveManualTime()">Guardar fecha y hora</button>
              <button class="btn" onclick="syncBrowserTime()">Usar hora del dispositivo</button>
            </div>
            <div class="message" id="msg-time"></div>
          </div>
          <div class="card">
            <div class="section-title">WiFi del sitio</div>
            <label for="wifi-ssid">SSID</label><input id="wifi-ssid" type="text" autocomplete="off">
            <label for="wifi-password">Clave WiFi</label><input id="wifi-password" type="password" autocomplete="new-password">
            <div class="hint">Si dejas el SSID vacio, el equipo queda solo en Access Point.</div>
            <button class="btn" onclick="saveWifiConfig()">Guardar WiFi</button>
            <div class="message" id="msg-wifi"></div>
          </div>
          <div class="card">
            <div class="section-title">OTA remota</div>
            <div class="hint">Solo el usuario maestro puede cambiar o ejecutar OTA.</div>
            <label class="inline"><input id="ota-enabled" type="checkbox">Habilitar chequeo OTA cada 1 hora</label>
            <label for="ota-manifest">URL del manifest OTA</label><input id="ota-manifest" type="text" placeholder="https://.../manifest.txt">
            <div class="hint">El manifest debe tener: version=, firmware_url= y sha256=. Cuando aparezca una version mas nueva, el equipo la instala solo.</div>
            <div class="inline">
              <button class="btn" onclick="saveOtaConfig()">Guardar OTA</button>
              <button class="btn" onclick="checkOtaNow()">Chequear ahora</button>
            </div>
            <div class="message" id="msg-ota"></div>
          </div>
          <div class="card" id="security-card">
            <div class="section-title">Seguridad</div>
            <div class="hint">Maestro fijo: <b>admin / admin</b>. Tiene acceso total y no se puede borrar desde la web.</div>
            <div class="hint" id="operator-status">Operador actual: sin configurar.</div>
            <label for="operator-user">Usuario operador</label><input id="operator-user" type="text" autocomplete="off" placeholder="Configura todo menos OTA">
            <label for="operator-password">Clave operador</label><input id="operator-password" type="password" autocomplete="new-password" placeholder="Nueva clave del operador">
            <div class="inline">
              <button class="btn" onclick="saveOperatorAccount()">Guardar operador</button>
              <button class="btn" onclick="clearOperatorAccount()">Eliminar operador</button>
            </div>
            <div class="hint">El operador puede entrar en configuracion, cambiar WiFi, hora, seguridad y borrar archivos. No puede usar OTA. Si dejas el mismo usuario y escribes una clave nueva, actualizas su clave.</div>
            <hr style="border-color:rgba(255,255,255,0.08); margin:16px 0;">
            <div class="hint" id="viewer-summary">Lectores configurados: ninguno.</div>
            <label for="viewer-slot">Lector a editar</label>
            <select id="viewer-slot" onchange="refreshViewerEditor()">
              <option value="1">Lector 1</option>
              <option value="2">Lector 2</option>
              <option value="3">Lector 3</option>
            </select>
            <label for="viewer-user">Usuario del lector</label><input id="viewer-user" type="text" autocomplete="off" placeholder="Usuario para ver graficos y bajar archivos">
            <label for="viewer-password">Clave del lector</label><input id="viewer-password" type="password" autocomplete="new-password" placeholder="Clave del lector">
            <div class="inline">
              <button class="btn" onclick="saveSelectedViewerAccount()">Guardar lector</button>
              <button class="btn" onclick="clearSelectedViewerAccount()">Eliminar lector</button>
            </div>
            <div class="hint">Selecciona el lector 1, 2 o 3. Si ya existe, se muestra el nombre actual. Para cambiar solo la clave, deja el mismo usuario y escribe una clave nueva.</div>
            <div class="message" id="msg-auth"></div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    const dataHistory = { co: [], h2s: [], o2: [], ch4: [], co2: [] };
    const graphLimit = 50;
    const session = { authHeader: "", role: "none", canViewData: false, canManageConfig: false, canManageSecurity: false, canDeleteFiles: false, canManageOta: false };
    const viewerUsers = ["", "", ""];

    function buildAuthHeader(user, password) { return "Basic " + btoa(user + ":" + password); }
    function setMessage(id, text) { document.getElementById(id).innerText = text; }
    function roleLabel(role) { return role === "master" ? "Maestro" : role === "operator" ? "Operador" : role === "viewer" ? "Solo lectura" : "Sin sesion"; }

    function updateOperatorStatus() {
      const user = document.getElementById("operator-user").value.trim();
      document.getElementById("operator-status").innerText = user ? "Operador actual: " + user : "Operador actual: sin configurar.";
    }

    function updateViewerSummary() {
      const summary = viewerUsers.map((user, index) => user ? ("Lector " + (index + 1) + ": " + user) : ("Lector " + (index + 1) + ": libre"));
      document.getElementById("viewer-summary").innerText = "Lectores configurados: " + summary.join(" | ");
    }

    function refreshViewerEditor(clearPassword = true) {
      const slot = Number(document.getElementById("viewer-slot").value || 1);
      document.getElementById("viewer-user").value = viewerUsers[slot - 1] || "";
      if (clearPassword) {
        document.getElementById("viewer-password").value = "";
      }
    }

    function applySessionUi() {
      document.getElementById("role-chip").innerText = "Rol: " + roleLabel(session.role);
      document.getElementById("nav-config").classList.toggle("hidden", !session.canManageConfig);
      document.getElementById("security-card").classList.toggle("hidden", !session.canManageSecurity);
      document.getElementById("ota-enabled").closest(".card").classList.toggle("hidden", !session.canManageOta);
      if (!session.canManageConfig && document.getElementById("view-config").classList.contains("active")) showView("main");
    }

    function resetSession() {
      session.authHeader = "";
      session.role = "none";
      session.canViewData = false;
      session.canManageConfig = false;
      session.canManageSecurity = false;
      session.canDeleteFiles = false;
      session.canManageOta = false;
      applySessionUi();
    }

    async function authenticatedFetch(url, options = {}) {
      if (!session.authHeader) throw new Error("Debes iniciar sesion");
      const opts = { ...options };
      opts.headers = { ...(options.headers || {}), Authorization: session.authHeader };
      const response = await fetch(url, opts);
      if (response.status === 401) {
        resetSession();
        document.getElementById("login-overlay").classList.remove("hidden");
        throw new Error("Credenciales invalidas o sin permiso");
      }
      return response;
    }

    async function login() {
      const user = document.getElementById("login-user").value.trim();
      const password = document.getElementById("login-password").value.trim();
      if (!user || !password) {
        setMessage("msg-login", "Completa usuario y clave.");
        return;
      }
      try {
        const authHeader = buildAuthHeader(user, password);
        const response = await fetch("/auth/session", { headers: { Authorization: authHeader } });
        if (response.status === 401) {
          setMessage("msg-login", "Credenciales invalidas.");
          return;
        }
        const auth = await response.json();
        session.authHeader = authHeader;
        session.role = auth.role || "none";
        session.canViewData = !!auth.canViewData;
        session.canManageConfig = !!auth.canManageConfig;
        session.canManageSecurity = !!auth.canManageSecurity;
        session.canDeleteFiles = !!auth.canDeleteFiles;
        session.canManageOta = !!auth.canManageOta;
        applySessionUi();
        document.getElementById("login-overlay").classList.add("hidden");
        setMessage("msg-login", "Sesion iniciada.");
        await updateData();
        if (session.canManageConfig) await loadProtectedConfig();
        else showView("main");
      } catch (error) {
        setMessage("msg-login", error.message);
      }
    }

    function logout() {
      resetSession();
      document.getElementById("login-password").value = "";
      document.getElementById("login-overlay").classList.remove("hidden");
      setMessage("msg-login", "Sesion cerrada.");
    }

    function formatSeconds(seconds) {
      if (!seconds || seconds <= 0) return "Sin cambio automatico";
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = Math.floor(seconds % 60);
      return [h, m, s].map(v => String(v).padStart(2, "0")).join(":");
    }

    function drawGraph(id, data, color) {
      const canvas = document.getElementById(id);
      const ctx = canvas.getContext("2d");
      const width = canvas.width = canvas.offsetWidth;
      const height = canvas.height = canvas.offsetHeight;
      ctx.clearRect(0, 0, width, height);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      const maxValue = Math.max(...data, 1) * 1.2;
      data.forEach((value, index) => {
        const x = (index / Math.max(graphLimit - 1, 1)) * width;
        const y = height - (value / maxValue) * height;
        if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.stroke();
    }

    async function updateData() {
      if (!session.canViewData) return;
      try {
        const response = await authenticatedFetch("/data");
        const data = await response.json();
        document.title = data.productName || document.title;
        document.getElementById("product-name").innerText = data.productName || document.getElementById("product-name").innerText;
        document.getElementById("date-display").innerText = data.date + " - " + data.time;
        document.getElementById("state-label").innerText = data.state;
        document.getElementById("source-title").innerText = data.sourceTitle || "Origen actual";
        document.getElementById("source-label").innerText = data.source || "-";
        document.getElementById("detail-title").innerText = data.detailTitle || "Detalle";
        document.getElementById("detail-label").innerText = data.detailValue || "-";
        document.getElementById("cycle-title").innerText = data.cycleTitle || "Tiempo restante";
        document.getElementById("cycle-label").innerText = data.cycleValue || formatSeconds(Math.floor((data.remainingMs || 0) / 1000));
        document.getElementById("wifi-status").innerText = data.wifiStatus;
        document.getElementById("wifi-ip").innerText = data.wifiSSID + " / " + data.localIP;
        document.getElementById("ap-ip").innerText = data.apIP;
        document.getElementById("firmware-version").innerText = data.firmwareVersion;
        document.getElementById("ota-summary").innerText = "OTA: " + data.otaMessage + "\nUltimo chequeo: " + data.otaLastCheck + "\nVersion disponible: " + data.otaAvailableVersion + "\nBuild: " + data.buildStamp + "\nReset: " + data.resetReason;
        const pill = document.getElementById("pill-state");
        pill.className = "status-pill";
        if (data.state === "MUESTRA" || data.state === "CONTINUO") pill.classList.add("state-sample");
        else if (data.state === "PURGA") pill.classList.add("state-purge");
        else pill.classList.add("state-idle");
        const keys = ["co", "h2s", "o2", "ch4", "co2"];
        const colors = { co: "#57e3a0", h2s: "#ff8b8b", o2: "#6ec4ff", ch4: "#ffd56d", co2: "#88ff9f" };
        keys.forEach((key) => {
          document.getElementById("v-" + key).innerText = data[key];
          dataHistory[key].push(Number(data[key]));
          if (dataHistory[key].length > graphLimit) dataHistory[key].shift();
          drawGraph("c-" + key, dataHistory[key], colors[key]);
        });
      } catch (error) { console.error(error); }
    }

    function showView(viewName) {
      if (viewName === "config" && !session.canManageConfig) return;
      document.querySelectorAll(".view").forEach((view) => view.classList.remove("active"));
      document.getElementById("view-" + viewName).classList.add("active");
      if (viewName === "files") loadFiles();
      if (viewName === "config") loadProtectedConfig();
    }

    async function loadFiles() {
      try {
        const response = await authenticatedFetch("/list");
        const files = await response.json();
        if (!files.length) {
          document.getElementById("file-list").innerHTML = "No hay archivos CSV disponibles.";
          return;
        }
        let html = "<table><thead><tr><th>Archivo</th><th>Tamano</th><th>Acciones</th></tr></thead><tbody>";
        files.forEach((file) => {
          html += "<tr><td>" + file.name + "</td><td>" + file.size + "</td><td>";
          html += "<button class='small-btn' onclick=\"downloadFile('" + file.name.replace(/'/g, "\\'") + "')\">Descargar</button> ";
          if (session.canDeleteFiles) html += "<button class='small-btn' onclick=\"deleteFile('" + file.name.replace(/'/g, "\\'") + "')\">Borrar</button>";
          html += "</td></tr>";
        });
        html += "</tbody></table>";
        document.getElementById("file-list").innerHTML = html;
      } catch (error) {
        document.getElementById("file-list").innerHTML = error.message;
      }
    }

    async function downloadFile(fileName) {
      try {
        const response = await authenticatedFetch("/get?file=" + encodeURIComponent(fileName));
        const blob = await response.blob();
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = fileName.replace(/^\//, "");
        document.body.appendChild(link);
        link.click();
        link.remove();
        URL.revokeObjectURL(url);
      } catch (error) {
        setMessage("msg-ota", error.message);
      }
    }

    async function deleteFile(fileName) {
      if (!confirm("Eliminar " + fileName + "?")) return;
      try {
        const params = new URLSearchParams();
        params.set("file", fileName);
        const response = await authenticatedFetch("/delete", { method: "POST", body: params });
        setMessage("msg-ota", await response.text());
        loadFiles();
      } catch (error) {
        setMessage("msg-ota", error.message);
      }
    }

    async function loadProtectedConfig() {
      if (!session.canManageConfig) return;
      try {
        const response = await authenticatedFetch("/config");
        const data = await response.json();
        document.title = data.productName || document.title;
        document.getElementById("product-name").innerText = data.productName || document.getElementById("product-name").innerText;
        document.getElementById("sampling-title").innerText = data.samplingTitle || "Programacion de muestreo";
        document.getElementById("sampling-description").innerText = data.samplingDescription || "";
        document.getElementById("sampling-static-note").innerText = data.samplingDescription || "";
        document.getElementById("sampling-config-fields").classList.toggle("hidden", !data.supportsValveConfig);
        document.getElementById("sampling-static-note").classList.toggle("hidden", !!data.supportsValveConfig);
        document.getElementById("cfg-v1").value = data.v1;
        document.getElementById("cfg-v2").value = data.v2;
        document.getElementById("cfg-v3").value = data.v3;
        document.getElementById("cfg-v4").value = data.v4;
        document.getElementById("cfg-purge").value = data.purge;
        document.getElementById("wifi-ssid").value = data.wifiSSID || "";
        document.getElementById("operator-user").value = data.operatorUser || "";
        document.getElementById("operator-password").value = "";
        viewerUsers[0] = data.viewer1User || "";
        viewerUsers[1] = data.viewer2User || "";
        viewerUsers[2] = data.viewer3User || "";
        updateOperatorStatus();
        updateViewerSummary();
        refreshViewerEditor();
        document.getElementById("ota-enabled").checked = !!data.otaEnabled;
        document.getElementById("ota-manifest").value = data.otaManifestUrl || "";
        setMessage("msg-ota", "Version actual: " + data.firmwareVersion + "\nBuild: " + data.buildStamp + "\nReset: " + data.resetReason + "\nUltimo estado OTA: " + data.otaStatus + "\nUltimo chequeo: " + data.otaLastCheck + "\nVersion disponible: " + data.otaAvailableVersion);
      } catch (error) {
        setMessage("msg-sampling", error.message);
      }
    }

    function validateValveSeconds(value) { return value === 0 || (value >= 30 && value <= 86400); }

    async function saveValveConfig() {
      const v1 = Number(document.getElementById("cfg-v1").value || 0);
      const v2 = Number(document.getElementById("cfg-v2").value || 0);
      const v3 = Number(document.getElementById("cfg-v3").value || 0);
      const v4 = Number(document.getElementById("cfg-v4").value || 0);
      const purge = Number(document.getElementById("cfg-purge").value || 0);
      if (![v1, v2, v3, v4].every(validateValveSeconds)) {
        setMessage("msg-sampling", "Cada valvula debe ser 0 o un valor entre 30 y 86400 segundos.");
        return;
      }
      const enabledValveCount = [v1, v2, v3, v4].filter((value) => value >= 30).length;
      if (purge < 0 || purge > 86400) {
        setMessage("msg-sampling", "La purga debe estar entre 0 y 86400 segundos.");
        return;
      }
      if (enabledValveCount > 1 && purge < 30) {
        setMessage("msg-sampling", "Con 2 o mas valvulas activas la purga debe ser de 30 a 86400 segundos.");
        return;
      }
      const params = new URLSearchParams();
      params.set("v1", v1); params.set("v2", v2); params.set("v3", v3); params.set("v4", v4); params.set("purge", purge);
      try {
        const response = await authenticatedFetch("/config/save", { method: "POST", body: params });
        setMessage("msg-sampling", await response.text());
      } catch (error) { setMessage("msg-sampling", error.message); }
    }

    async function saveWifiConfig() {
      const params = new URLSearchParams();
      params.set("ssid", document.getElementById("wifi-ssid").value);
      params.set("password", document.getElementById("wifi-password").value);
      try {
        const response = await authenticatedFetch("/wifi/save", { method: "POST", body: params });
        setMessage("msg-wifi", await response.text());
      } catch (error) { setMessage("msg-wifi", error.message); }
    }

    async function saveOperatorAccount() {
      const user = document.getElementById("operator-user").value.trim();
      const password = document.getElementById("operator-password").value.trim();
      if (!user || !password) {
        setMessage("msg-auth", "Completa usuario y clave del operador.");
        return;
      }
      const params = new URLSearchParams();
      params.set("user", user); params.set("password", password);
      try {
        const response = await authenticatedFetch("/security/operator/save", { method: "POST", body: params });
        setMessage("msg-auth", await response.text());
        if (session.role === "operator") {
          session.authHeader = buildAuthHeader(user, password);
          document.getElementById("login-user").value = user;
          document.getElementById("login-password").value = "";
        }
        document.getElementById("operator-password").value = "";
        await loadProtectedConfig();
      } catch (error) { setMessage("msg-auth", error.message); }
    }

    async function clearOperatorAccount() {
      try {
        const response = await authenticatedFetch("/security/operator/clear", { method: "POST" });
        setMessage("msg-auth", await response.text());
        if (session.role === "operator") {
          resetSession();
          document.getElementById("login-user").value = "admin";
          document.getElementById("login-password").value = "";
          document.getElementById("login-overlay").classList.remove("hidden");
          setMessage("msg-login", "Operador eliminado. Ingresa nuevamente con admin / admin.");
          return;
        }
        document.getElementById("operator-user").value = "";
        document.getElementById("operator-password").value = "";
        updateOperatorStatus();
        await loadProtectedConfig();
      } catch (error) { setMessage("msg-auth", error.message); }
    }

    async function saveSelectedViewerAccount() {
      const slot = Number(document.getElementById("viewer-slot").value || 1);
      const user = document.getElementById("viewer-user").value.trim();
      const password = document.getElementById("viewer-password").value.trim();
      if (!user || !password) {
        setMessage("msg-auth", "Completa usuario y clave para el lector " + slot + ".");
        return;
      }
      const params = new URLSearchParams();
      params.set("slot", String(slot)); params.set("user", user); params.set("password", password);
      try {
        const response = await authenticatedFetch("/security/viewer/save", { method: "POST", body: params });
        setMessage("msg-auth", await response.text());
        document.getElementById("viewer-password").value = "";
        await loadProtectedConfig();
      } catch (error) { setMessage("msg-auth", error.message); }
    }

    async function clearSelectedViewerAccount() {
      const slot = Number(document.getElementById("viewer-slot").value || 1);
      const params = new URLSearchParams();
      params.set("slot", String(slot));
      try {
        const response = await authenticatedFetch("/security/viewer/clear", { method: "POST", body: params });
        setMessage("msg-auth", await response.text());
        viewerUsers[slot - 1] = "";
        refreshViewerEditor();
        updateViewerSummary();
        await loadProtectedConfig();
      } catch (error) { setMessage("msg-auth", error.message); }
    }

    async function saveManualTime() {
      const dateValue = document.getElementById("manual-date").value;
      const timeValue = document.getElementById("manual-time").value;
      if (!dateValue || !timeValue) {
        setMessage("msg-time", "Completa fecha y hora.");
        return;
      }
      const localDate = new Date(dateValue + "T" + timeValue);
      const params = new URLSearchParams();
      params.set("epoch", Math.floor(localDate.getTime() / 1000));
      params.set("tz", localDate.getTimezoneOffset());
      try {
        const response = await authenticatedFetch("/time/set", { method: "POST", body: params });
        setMessage("msg-time", await response.text());
      } catch (error) { setMessage("msg-time", error.message); }
    }

    async function syncBrowserTime() {
      const now = new Date();
      const params = new URLSearchParams();
      params.set("epoch", Math.floor(now.getTime() / 1000));
      params.set("tz", now.getTimezoneOffset());
      try {
        const response = await authenticatedFetch("/time/set", { method: "POST", body: params });
        setMessage("msg-time", await response.text());
      } catch (error) { setMessage("msg-time", error.message); }
    }

    async function saveOtaConfig() {
      const params = new URLSearchParams();
      params.set("enabled", document.getElementById("ota-enabled").checked ? "1" : "0");
      params.set("manifest_url", document.getElementById("ota-manifest").value.trim());
      try {
        const response = await authenticatedFetch("/ota/save", { method: "POST", body: params });
        setMessage("msg-ota", await response.text());
      } catch (error) { setMessage("msg-ota", error.message); }
    }

    async function checkOtaNow() {
      try {
        const response = await authenticatedFetch("/ota/check", { method: "POST" });
        setMessage("msg-ota", await response.text());
      } catch (error) { setMessage("msg-ota", error.message); }
    }

    document.getElementById("login-password").addEventListener("keydown", (event) => {
      if (event.key === "Enter") login();
    });

    setInterval(updateData, 1000);
    resetSession();
  </script>
</body>
</html>
)rawliteral";

#endif
