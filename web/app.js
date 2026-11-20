const MAX_SPEED = 160;
const ARC_MAX = 480; // 270 derece yay uzunluğu (r=102 için)
const SPEED_LIMITS = [50, 70, 90];

let currentState = {
  accEnabled: false,
  accSetSpeed: 70,
  speedLimitIdx: 1,
  fuel: 88.0,
  temp: 88.0,
  roadScroll: 0
};

// Dikey segment barları oluştur (Yakıt & Sıcaklık)
function initSegmentedBar(id, count = 18) {
  const el = document.getElementById(id);
  el.innerHTML = "";
  for (let i = 0; i < count; i++) {
    const seg = document.createElement("div");
    seg.className = "seg";
    el.appendChild(seg);
  }
}

function updateSegmentedBar(id, ratio, warnLow = false) {
  const segs = document.querySelectorAll(`#${id} .seg`);
  const activeCount = Math.round(Math.max(0, Math.min(1, ratio)) * segs.length);
  segs.forEach((seg, idx) => {
    seg.className = "seg";
    if (idx < activeCount) {
      if (warnLow && ratio < 0.2) seg.classList.add("crit");
      else if (!warnLow && ratio > 0.82) seg.classList.add("crit");
      else if ((!warnLow && ratio > 0.68) || (warnLow && ratio < 0.35)) seg.classList.add("warn");
      else seg.classList.add("on");
    }
  });
}

// Kadran çizgilerini ve sayılarını oluştur
function buildTicks(groupId, maxVal, step, isRpm = false) {
  const g = document.getElementById(groupId);
  g.innerHTML = "";
  for (let v = 0; v <= maxVal; v += step) {
    const angleDeg = 135 + (v / maxVal) * 270;
    const rad = (angleDeg * Math.PI) / 180;
    const x1 = 130 + Math.cos(rad) * 90;
    const y1 = 130 + Math.sin(rad) * 90;
    const x2 = 130 + Math.cos(rad) * 99;
    const y2 = 130 + Math.sin(rad) * 99;
    const tx = 130 + Math.cos(rad) * 76;
    const ty = 130 + Math.sin(rad) * 76 + 4;

    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1);
    line.setAttribute("y1", y1);
    line.setAttribute("x2", x2);
    line.setAttribute("y2", y2);
    line.setAttribute("class", "tick-line");
    g.appendChild(line);

    const text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx);
    text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("class", "tick-label");
    text.textContent = isRpm ? v : v;
    g.appendChild(text);
  }
}

// Kırmızı hız limiti tabelasını kadran üzerinde doğru açıya yerleştir
function updateTsrMarker(limit) {
  const angleDeg = 135 + (Math.min(limit, MAX_SPEED) / MAX_SPEED) * 270;
  const rad = (angleDeg * Math.PI) / 180;
  const cx = 130 + Math.cos(rad) * 112;
  const cy = 130 + Math.sin(rad) * 112;
  const marker = document.getElementById("tsrMarker");
  marker.querySelector("circle").setAttribute("cx", cx);
  marker.querySelector("circle").setAttribute("cy", cy);
  const txt = document.getElementById("tsrVal");
  txt.setAttribute("x", cx);
  txt.setAttribute("y", cy + 4);
  txt.textContent = limit;
}

// Aksiyon Kamerası (HTML5 Canvas)
function drawActionCamera(sensor, adas) {
  const canvas = document.getElementById("actionCam");
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  // Gece gökyüzü ve asfalt
  const skyGrad = ctx.createLinearGradient(0, 0, 0, h * 0.42);
  skyGrad.addColorStop(0, "#030712");
  skyGrad.addColorStop(1, "#0f172a");
  ctx.fillStyle = skyGrad;
  ctx.fillRect(0, 0, w, h * 0.42);

  ctx.fillStyle = "#0b1220";
  ctx.fillRect(0, h * 0.42, w, h * 0.58);

  const horizonY = h * 0.42;
  const centerX = w / 2;
  const offsetPx = (sensor.lane_offset || 0) * 65;

  // Yol yüzeyi (perspektif trapez)
  ctx.fillStyle = "#111a2c";
  ctx.beginPath();
  ctx.moveTo(centerX - 45, horizonY);
  ctx.lineTo(centerX + 45, horizonY);
  ctx.lineTo(centerX + 190 - offsetPx, h);
  ctx.lineTo(centerX - 190 - offsetPx, h);
  ctx.closePath();
  ctx.fill();

  // Sol ve Sağ Şerit Çizgileri (LDW renk mantığı)
  const leftViolation = adas.ldw_warning && sensor.lane_offset < -0.5;
  const rightViolation = adas.ldw_warning && sensor.lane_offset > 0.5;

  function drawLaneLine(topX, botX, isWarn, isSuppressed) {
    ctx.strokeStyle = isWarn ? "#ef4444" : isSuppressed ? "#22c55e" : "#00e5ff";
    ctx.lineWidth = isWarn ? 4.5 : 3;
    ctx.beginPath();
    ctx.moveTo(topX, horizonY);
    ctx.lineTo(botX, h);
    ctx.stroke();
  }

  drawLaneLine(centerX - 42, centerX - 175 - offsetPx, leftViolation, adas.ldw_suppressed && sensor.left_signal);
  drawLaneLine(centerX + 42, centerX + 175 - offsetPx, rightViolation, adas.ldw_suppressed && sensor.right_signal);

  // Akan orta kesikli şerit çizgisi
  currentState.roadScroll = (currentState.roadScroll + (sensor.speed_kmh || 0) * 0.08) % 40;
  ctx.strokeStyle = "rgba(148, 163, 184, 0.35)";
  ctx.lineWidth = 2;
  ctx.setLineDash([14, 26]);
  ctx.lineDashOffset = -currentState.roadScroll;
  ctx.beginPath();
  ctx.moveTo(centerX, horizonY);
  ctx.lineTo(centerX - offsetPx, h);
  ctx.stroke();
  ctx.setLineDash([]);

  // Yol Anomalisi (Çukur / Tümsek)
  if (sensor.obstacle_type && sensor.obstacle_type !== "Yok" && sensor.obstacle_distance < 60) {
    const t = 1 - Math.max(0, Math.min(1, sensor.obstacle_distance / 60));
    const oy = horizonY + t * (h - horizonY - 25);
    const ox = centerX - offsetPx * t;
    const rw = 14 + t * 44;
    const rh = 5 + t * 14;
    ctx.fillStyle = sensor.obstacle_type === "Cukur" ? "rgba(56, 189, 248, 0.7)" : "rgba(245, 158, 11, 0.85)";
    ctx.beginPath();
    ctx.ellipse(ox, oy, rw, rh, 0, 0, Math.PI * 2);
    ctx.fill();
  }

  // Öndeki Lider Araç & Radar Kilit Çerçevesi
  const dist = Math.max(5, Math.min(90, sensor.radar_distance || 55));
  const depth = 1 - (dist - 5) / 85;
  const carY = horizonY + 10 + depth * (h - horizonY - 65);
  const carW = 24 + depth * 64;
  const carH = 14 + depth * 36;
  const carX = centerX - carW / 2;

  // Radar yayları
  ctx.strokeStyle = adas.aeb_active ? "rgba(239, 68, 68, 0.85)" : "rgba(0, 229, 255, 0.45)";
  ctx.lineWidth = 2;
  for (let i = 1; i <= 3; i++) {
    ctx.beginPath();
    ctx.arc(centerX, h + 10, 35 + i * 24, Math.PI * 1.25, Math.PI * 1.75);
    ctx.stroke();
  }

  // Lider araç gövdesi
  ctx.fillStyle = "#334155";
  ctx.fillRect(carX, carY, carW, carH);
  ctx.fillStyle = "#ef4444"; // Stop lambaları
  ctx.fillRect(carX + 3, carY + carH * 0.55, carW * 0.2, carH * 0.22);
  ctx.fillRect(carX + carW * 0.77, carY + carH * 0.55, carW * 0.2, carH * 0.22);

  // Hedef kilit köşeleri
  ctx.strokeStyle = adas.aeb_active ? "#ef4444" : "#00e5ff";
  ctx.lineWidth = 2;
  ctx.strokeRect(carX - 5, carY - 5, carW + 10, carH + 10);

  // AEB Kırmızı Flaş Katmanı
  if (adas.aeb_active) {
    ctx.fillStyle = "rgba(239, 68, 68, 0.22)";
    ctx.fillRect(0, 0, w, h);
  }
}

// C++ Backend'e Kontrol Komutu Gönder
async function sendControl(payload) {
  try {
    await fetch("/api/control", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
  } catch (e) {
    // Bağlantı hatası sessizce geçilir
  }
}

// 20 Hz Telemetri Güncellemesi
async function pollTelemetry() {
  try {
    const res = await fetch("/api/telemetry");
    if (!res.ok) throw new Error("HTTP");
    const data = await res.json();
    const s = data.sensor;
    const a = data.adas;

    document.getElementById("connDot").classList.add("online");
    document.getElementById("connText").textContent = "CANLI (20 Hz)";

    currentState.accEnabled = s.acc_enabled;
    currentState.accSetSpeed = s.acc_set_speed;

    // 1. Hız & TSR & ACC
    const speed = Math.round(s.speed_kmh);
    const speedEl = document.getElementById("digitalSpeed");
    speedEl.textContent = speed;
    speedEl.classList.toggle("overspeed", a.speed_warning);

    const speedArcLen = (Math.min(s.speed_kmh, MAX_SPEED) / MAX_SPEED) * ARC_MAX;
    document.getElementById("speedArc").setAttribute("stroke-dasharray", `${speedArcLen} 641`);
    updateTsrMarker(s.speed_limit);

    const accMini = document.getElementById("accMini");
    accMini.textContent = s.acc_enabled ? `ACC: ${Math.round(s.acc_set_speed)} km/s` : "ACC: KAPALI";
    accMini.classList.toggle("active", a.acc_active);

    // 2. Devir (RPM) ve Vites (Hızdan türetilmiş)
    const rpm = s.speed_kmh < 1 ? 0.8 : Math.min(7.5, 1.1 + (s.speed_kmh % 32) * 0.14 + (s.is_gas_pressed ? 0.9 : 0));
    const rpmArcLen = (rpm / 8) * ARC_MAX;
    document.getElementById("rpmArc").setAttribute("stroke-dasharray", `${rpmArcLen} 641`);

    const gear = s.speed_kmh < 0.5 && !s.is_gas_pressed ? "P" : "D";
    document.getElementById("activeGear").textContent = gear;
    document.querySelectorAll(".gear-item").forEach(el => {
      el.classList.toggle("active", el.dataset.gear === gear);
    });

    // 3. Yakıt ve Sıcaklık (Gerçekçi simülasyon)
    if (s.speed_kmh > 1) currentState.fuel = Math.max(5, currentState.fuel - 0.002);
    const targetTemp = s.is_gas_pressed ? 94 : 89;
    currentState.temp += (targetTemp - currentState.temp) * 0.02;
    updateSegmentedBar("fuelBar", currentState.fuel / 100, true);
    updateSegmentedBar("tempBar", (currentState.temp - 40) / 80, false);
    document.getElementById("fuelVal").textContent = `${Math.round(currentState.fuel)}%`;
    document.getElementById("tempVal").textContent = `${Math.round(currentState.temp)}°C`;

    // 4. Sinyaller & Süspansiyon
    document.getElementById("sigLeft").classList.toggle("active", s.left_signal);
    document.getElementById("sigRight").classList.toggle("active", s.right_signal);
    document.getElementById("suspText").textContent = `%${a.suspension_stiffness} ${a.suspension_mode}`;

    // 5. Aksiyon Kamerası & Radar
    drawActionCamera(s, a);
    document.getElementById("cameraBox").classList.toggle("aeb-alarm", a.aeb_active);
    document.getElementById("radarBadge").textContent = `RADAR: ${s.radar_distance.toFixed(1)} m`;
    document.getElementById("ttcBadge").textContent = a.ttc_seconds < 20 ? `TTC: ${a.ttc_seconds.toFixed(1)} s` : "TTC: GÜVENLİ";

    // 6. Uyarı Paneli ve ADAS Rozetleri
    const alertBox = document.getElementById("alertBox");
    const alertMsg = document.getElementById("alertMessage");
    alertBox.className = "alert-box";

    if (a.aeb_active) {
      alertBox.classList.add("danger");
      alertMsg.textContent = a.driver_override
        ? "🛑 AEB ACİL FREN — GAZ EZİLDİ (MANUAL OVERRIDE)"
        : "🛑 AEB ACİL FREN DEVREDE!";
    } else if (a.ldw_warning) {
      alertBox.classList.add("danger");
      alertMsg.textContent = "⚠️ ŞERİT İHLALİ — SİNYALSİZ KAYMA ALGILANDI";
    } else if (a.speed_warning) {
      alertBox.classList.add("warn");
      alertMsg.textContent = `⚠️ HIZ İHLALİ (+${a.speed_overshoot.toFixed(0)} km/s)`;
    } else if (s.obstacle_type !== "Yok" && s.obstacle_distance < 40) {
      alertBox.classList.add("warn");
      alertMsg.textContent = `🛡️ ${s.obstacle_type.toUpperCase()} (${s.obstacle_distance.toFixed(0)}m) — SÜSPANSİYON %${a.suspension_stiffness}`;
    } else {
      alertMsg.textContent = a.acc_active ? `⚡ ACC AKTİF — ${a.acc_mode.toUpperCase()}` : "SİSTEM NORMAL — GÜVENLİ SÜRÜŞ";
    }

    document.getElementById("pillAcc").className = "pill" + (a.acc_active ? " active-cyan" : "");
    document.getElementById("pillLdw").className = "pill" + (a.ldw_warning ? " active-red" : a.ldw_suppressed ? " active-cyan" : "");
    document.getElementById("pillAeb").className = "pill" + (a.aeb_active ? " active-red" : "");
    document.getElementById("pillTsr").className = "pill" + (a.speed_warning ? " active-amber" : " active-cyan");

    // 7. Terminal Logları
    if (Array.isArray(data.logs)) {
      const logEl = document.getElementById("terminalLogs");
      logEl.innerHTML = data.logs
        .map(line => {
          const cls = line.includes("KRİTİK") || line.includes("İHLAL") ? "crit" : line.includes("[ADAS]") ? "adas" : "";
          return `<div class="log-line ${cls}">${line}</div>`;
        })
        .join("");
      logEl.scrollTop = logEl.scrollHeight;
    }
  } catch (err) {
    document.getElementById("connDot").classList.remove("online");
    document.getElementById("connText").textContent = "BAĞLANTI YOK (localhost:8080)";
  }
}

// Klavye Kısayolları
const pressed = {};
window.addEventListener("keydown", (e) => {
  const k = e.key.toLowerCase();
  if (pressed[k]) return;
  pressed[k] = true;

  if (k === "w" || e.key === "ArrowUp") sendControl({ gas: true });
  else if (e.code === "Space") { e.preventDefault(); sendControl({ brake: true }); }
  else if (k === "a" || k === "q") sendControl({ left_signal: true });
  else if (k === "d" || k === "e") sendControl({ right_signal: true });
  else if (k === "c") sendControl({ acc_enabled: !currentState.accEnabled });
  else if (k === "+" || k === "=") sendControl({ acc_set_speed: currentState.accSetSpeed + 5 });
  else if (k === "-") sendControl({ acc_set_speed: currentState.accSetSpeed - 5 });
});

window.addEventListener("keyup", (e) => {
  const k = e.key.toLowerCase();
  pressed[k] = false;

  if (k === "w" || e.key === "ArrowUp") sendControl({ gas: false });
  else if (e.code === "Space") sendControl({ brake: false });
  else if (k === "a" || k === "q") sendControl({ left_signal: false });
  else if (k === "d" || k === "e") sendControl({ right_signal: false });
});

// Buton Olayları
document.getElementById("btnAcc").onclick = () => sendControl({ acc_enabled: !currentState.accEnabled });
document.getElementById("btnAccUp").onclick = () => sendControl({ acc_set_speed: currentState.accSetSpeed + 5 });
document.getElementById("btnAccDown").onclick = () => sendControl({ acc_set_speed: currentState.accSetSpeed - 5 });
document.getElementById("btnPothole").onclick = () => sendControl({ spawn_obstacle: "Cukur", distance: 40.0 });
document.getElementById("btnBump").onclick = () => sendControl({ spawn_obstacle: "Tumsek", distance: 40.0 });
document.getElementById("btnSpeedLimit").onclick = () => {
  currentState.speedLimitIdx = (currentState.speedLimitIdx + 1) % SPEED_LIMITS.length;
  sendControl({ set_speed_limit: SPEED_LIMITS[currentState.speedLimitIdx] });
};
document.getElementById("btnReset").onclick = () => {
  currentState.fuel = 88.0;
  sendControl({ reset: true });
};

// Başlat
initSegmentedBar("fuelBar");
initSegmentedBar("tempBar");
buildTicks("speedTicks", MAX_SPEED, 20, false);
buildTicks("rpmTicks", 8, 1, true);
setInterval(pollTelemetry, 50);