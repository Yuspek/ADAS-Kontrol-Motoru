// ADAS Karar Motoru Web Dashboard Logic (10 Hz Polling & Driver Inputs)

const API_BASE = 'http://localhost:8080/api';
let isGasPressed = false;
let leftSignalActive = false;
let rightSignalActive = false;

// DOM Elemanları
const elConnectionText = document.getElementById('connectionText');
const elConnectionBadge = document.getElementById('connectionBadge');

// TSR & Hız
const elSpeedVal = document.getElementById('speedVal');
const elSpeedBar = document.getElementById('speedBar');
const elTsrVal = document.getElementById('tsrVal');
const elTsrSign = document.getElementById('tsrSign');
const elSpeedBadge = document.getElementById('speedBadge');
const elSpeedWarningBanner = document.getElementById('speedWarningBanner');
const elOvershootVal = document.getElementById('overshootVal');

// AEB
const elRadarVal = document.getElementById('radarVal');
const elAebStatusBadge = document.getElementById('aebStatusBadge');
const elRadarLine = document.getElementById('radarLine');
const elAebAlert = document.getElementById('aebAlert');
const elOverrideAlert = document.getElementById('overrideAlert');

// Süspansiyon
const elSuspBadge = document.getElementById('suspBadge');
const elObstacleTypeVal = document.getElementById('obstacleTypeVal');
const elObstacleDistVal = document.getElementById('obstacleDistVal');
const elSuspStiffnessBar = document.getElementById('suspStiffnessBar');
const elSuspStiffnessPct = document.getElementById('suspStiffnessPct');

// LDW & Sinyal
const elLdwBadge = document.getElementById('ldwBadge');
const elBtnLeftSignal = document.getElementById('btnLeftSignal');
const elBtnRightSignal = document.getElementById('btnRightSignal');
const elLaneCar = document.getElementById('laneCar');
const elLaneOffsetVal = document.getElementById('laneOffsetVal');
const elLdwWarningBanner = document.getElementById('ldwWarningBanner');
const elLdwSuppressedBanner = document.getElementById('ldwSuppressedBanner');

// Butonlar
const elBtnGas = document.getElementById('btnGas');
const elBtnSpawnPothole = document.getElementById('btnSpawnPothole');
const elBtnSpawnBump = document.getElementById('btnSpawnBump');
const elBtnToggleSpeedLimit = document.getElementById('btnToggleSpeedLimit');
const elBtnReset = document.getElementById('btnReset');
const elLogTerminal = document.getElementById('logTerminal');

// API Kontrol Gönderici
async function sendControlCommand(payload) {
    try {
        await fetch(`${API_BASE}/control`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
    } catch (err) {
        console.error('Kontrol komutu gönderilemedi:', err);
    }
}

// Telemetri Verisini Çek (10 Hz = 100ms)
async function fetchTelemetry() {
    try {
        const response = await fetch(`${API_BASE}/telemetry`);
        if (!response.ok) throw new Error('API Hatası');
        
        const data = await response.json();
        updateUI(data);

        elConnectionBadge.style.background = 'rgba(16, 185, 129, 0.1)';
        elConnectionBadge.style.borderColor = 'rgba(16, 185, 129, 0.3)';
        elConnectionText.innerText = 'CANLI VERİ AKIŞI (10 FPS)';
    } catch (err) {
        elConnectionBadge.style.background = 'rgba(239, 68, 68, 0.15)';
        elConnectionBadge.style.borderColor = 'rgba(239, 68, 68, 0.4)';
        elConnectionText.innerText = 'C++ SUNUCUSU BEKLENİYOR...';
    }
}

// Arayüzü Güncelleme Mantığı
function updateUI(data) {
    const sensor = data.sensor;
    const adas = data.adas;
    const logs = data.logs || [];

    // 1. Hız ve TSR Güncelleme
    const speed = sensor.speed_kmh;
    elSpeedVal.innerText = speed.toFixed(1);
    elSpeedBar.style.width = `${Math.min((speed / 140) * 100, 100)}%`;
    elTsrVal.innerText = sensor.speed_limit;

    if (adas.speed_warning) {
        elSpeedBadge.innerText = 'HIZ İHLALİ';
        elSpeedBadge.style.background = 'rgba(239, 68, 68, 0.2)';
        elSpeedBadge.style.color = '#ef4444';
        elSpeedWarningBanner.classList.remove('hidden');
        elOvershootVal.innerText = adas.speed_overshoot.toFixed(1);
    } else {
        elSpeedBadge.innerText = 'NORMAL';
        elSpeedBadge.style.background = 'rgba(16, 185, 129, 0.2)';
        elSpeedBadge.style.color = '#10b981';
        elSpeedWarningBanner.classList.add('hidden');
    }

    // 2. AEB & Radar Güncelleme
    const radarDist = sensor.radar_distance;
    elRadarVal.innerText = radarDist.toFixed(1);
    elRadarLine.style.width = `${Math.max(0, Math.min(100, (radarDist / 90) * 100))}%`;

    if (adas.aeb_active) {
        elAebStatusBadge.innerText = 'AEB FREN YAPILIYOR';
        elAebStatusBadge.style.background = 'rgba(239, 68, 68, 0.3)';
        elAebStatusBadge.style.color = '#ff4d4d';
        elAebAlert.classList.remove('hidden');
    } else {
        elAebStatusBadge.innerText = 'GÜVENLİ';
        elAebStatusBadge.style.background = 'rgba(16, 185, 129, 0.2)';
        elAebStatusBadge.style.color = '#10b981';
        elAebAlert.classList.add('hidden');
    }

    if (adas.driver_override) {
        elOverrideAlert.classList.remove('hidden');
    } else {
        elOverrideAlert.classList.add('hidden');
    }

    // 3. Aktif Süspansiyon Güncelleme
    elObstacleTypeVal.innerText = sensor.obstacle_type;
    elObstacleDistVal.innerText = (sensor.obstacle_type !== 'Yok') ? `${sensor.obstacle_distance.toFixed(1)} m` : '-- m';
    
    const stiffness = adas.suspension_stiffness;
    elSuspStiffnessBar.style.width = `${stiffness}%`;
    elSuspStiffnessPct.innerText = `${stiffness}%`;
    elSuspBadge.innerText = adas.suspension_mode;

    // 4. Şerit Takip & Sinyaller (LDW)
    leftSignalActive = sensor.left_signal;
    rightSignalActive = sensor.right_signal;

    elBtnLeftSignal.classList.toggle('active', leftSignalActive);
    elBtnRightSignal.classList.toggle('active', rightSignalActive);

    // Şerit araba konumlandırması (-1.2m -> +1.2m pixel offset)
    const pxOffset = sensor.lane_offset * 40; 
    elLaneCar.style.transform = `translateX(${pxOffset}px)`;
    elLaneOffsetVal.innerText = `${sensor.lane_offset > 0 ? '+' : ''}${sensor.lane_offset.toFixed(2)} m`;

    if (adas.ldw_warning) {
        elLdwBadge.innerText = 'ŞERİT İHLALİ';
        elLdwBadge.style.background = 'rgba(239, 68, 68, 0.2)';
        elLdwBadge.style.color = '#ef4444';
        elLdwWarningBanner.classList.remove('hidden');
        elLdwSuppressedBanner.classList.add('hidden');
    } else if (adas.ldw_suppressed) {
        elLdwBadge.innerText = 'BİLİNÇLİ SOLLAMA';
        elLdwBadge.style.background = 'rgba(16, 185, 129, 0.2)';
        elLdwBadge.style.color = '#10b981';
        elLdwWarningBanner.classList.add('hidden');
        elLdwSuppressedBanner.classList.remove('hidden');
    } else {
        elLdwBadge.innerText = 'ŞERİT ORTASINDA';
        elLdwBadge.style.background = 'rgba(255, 255, 255, 0.1)';
        elLdwBadge.style.color = '#cbd5e1';
        elLdwWarningBanner.classList.add('hidden');
        elLdwSuppressedBanner.classList.add('hidden');
    }

    // 5. Terminal Log Akışı Güncelleme
    updateLogs(logs);
}

// Terminal Güncelleme
function updateLogs(logs) {
    if (!logs || logs.length === 0) return;

    elLogTerminal.innerHTML = '';
    logs.forEach(log => {
        const div = document.createElement('div');
        div.className = 'log-line';
        if (log.includes('KRİTİK') || log.includes('HIZ İHLALİ') || log.includes('ŞERİT İHLALİ')) {
            div.classList.add('critical');
        } else if (log.includes('Algılandı') || log.includes('Sertliği')) {
            div.classList.add('warning');
        } else {
            div.classList.add('info');
        }
        div.innerText = log;
        elLogTerminal.appendChild(div);
    });
    elLogTerminal.scrollTop = elLogTerminal.scrollHeight;
}

// Olay Dinleyicileri (Keyboard & Buttons)
function setGasState(state) {
    if (isGasPressed !== state) {
        isGasPressed = state;
        elBtnGas.classList.toggle('pressed', isGasPressed);
        sendControlCommand({ gas: isGasPressed });
    }
}

// Klavye Olayları
window.addEventListener('keydown', (e) => {
    if (e.key === 'w' || e.key === 'W' || e.key === 'ArrowUp') {
        setGasState(true);
    } else if (e.key === ' ') {
        sendControlCommand({ brake: true });
    } else if (e.key === 'a' || e.key === 'A' || e.key === 'q' || e.key === 'Q') {
        sendControlCommand({ left_signal: !leftSignalActive });
    } else if (e.key === 'd' || e.key === 'D' || e.key === 'e' || e.key === 'E') {
        sendControlCommand({ right_signal: !rightSignalActive });
    }
});

window.addEventListener('keyup', (e) => {
    if (e.key === 'w' || e.key === 'W' || e.key === 'ArrowUp') {
        setGasState(false);
    } else if (e.key === ' ') {
        sendControlCommand({ brake: false });
    }
});

// Fare ve Dokunmatik Olaylar
elBtnGas.addEventListener('mousedown', () => setGasState(true));
elBtnGas.addEventListener('mouseup', () => setGasState(false));
elBtnGas.addEventListener('mouseleave', () => setGasState(false));
elBtnGas.addEventListener('touchstart', (e) => { e.preventDefault(); setGasState(true); });
elBtnGas.addEventListener('touchend', (e) => { e.preventDefault(); setGasState(false); });

elBtnLeftSignal.addEventListener('click', () => {
    sendControlCommand({ left_signal: !leftSignalActive });
});

elBtnRightSignal.addEventListener('click', () => {
    sendControlCommand({ right_signal: !rightSignalActive });
});

elBtnSpawnPothole.addEventListener('click', () => {
    sendControlCommand({ spawn_obstacle: 'Çukur' });
});

elBtnSpawnBump.addEventListener('click', () => {
    sendControlCommand({ spawn_obstacle: 'Tümsek' });
});

let currentLimitIdx = 1; // 70
const limits = [50, 70, 90];
elBtnToggleSpeedLimit.addEventListener('click', () => {
    currentLimitIdx = (currentLimitIdx + 1) % limits.length;
    sendControlCommand({ set_speed_limit: limits[currentLimitIdx] });
});

elBtnReset.addEventListener('click', () => {
    sendControlCommand({ reset: true });
});

// Başlangıç: Saniyede 10 kez polla (100ms)
setInterval(fetchTelemetry, 100);
fetchTelemetry();
