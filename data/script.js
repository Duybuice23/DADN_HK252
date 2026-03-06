// ================= WebSocket =================
let websocket = null;

function initWebSocket() {
  const gateway = `ws://${window.location.hostname}/ws`;
  console.log("🔌 Connecting WebSocket:", gateway);
  websocket = new WebSocket(gateway);

  websocket.onopen = () => {
    console.log("✅ WebSocket opened");
    sendJson({ page: "get_config" });
  };

  websocket.onclose = () => {
    console.log("❌ WebSocket closed, retry in 2s");
    setTimeout(() => {
      initWebSocket();
    }, 2000);
  };

  websocket.onmessage = onMessage;
}

function sendJson(obj) {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(JSON.stringify(obj));
  } else {
    console.log("⚠️ WebSocket not ready");
  }
}

// ================ Xử lý dữ liệu từ Board ================
function onMessage(event) {
  let data;
  try {
    data = JSON.parse(event.data);
  } catch (e) {
    console.warn("JSON parse error:", e);
    return;
  }

  if (!data.page) return;

  switch (data.page) {
    case "sensor":
      updateSensorCard(data);
      break;

    case "tinyml":
      updateTinyMlCard(data);
      break;

    case "config":
      fillConfigData(data.value);
      break;

    case "device":
      // Cập nhật trạng thái thiết bị từ server gửi về
      updateDeviceStateFromServer(data.value);
      break;

    case "setting_saved":
      setStatusMsg("settings-msg", "✅ Đã lưu cài đặt mạng.", "success");
      break;

    case "threshold_saved":
      setStatusMsg("threshold-msg", "✅ Đã lưu ngưỡng.", "success");
      break;

    case "led_pattern_saved":
      setStatusMsg("led-msg", "✅ Đã lưu pattern LED.", "success");
      break;

    case "neo_color_saved":
      setStatusMsg("neo-msg", "✅ Đã lưu màu NeoPixel.", "success");
      break;

    case "reset_done":
      alert("✅ Đã Reset Factory! Board đang khởi động lại...");
      location.reload();
      break;

    default:
      console.log("ℹ️ Unknown page:", data.page);
      break;
  }
}

// Hàm điền dữ liệu cấu hình ban đầu
function fillConfigData(cfg) {
  console.log("📥 Filling config:", cfg);

  // 1. Fill Thresholds
  if (cfg.thresholds) {
    document.getElementById("temp-cold").value = cfg.thresholds.tempCold ?? 25;
    document.getElementById("temp-hot").value = cfg.thresholds.tempHot ?? 30;
    document.getElementById("humi-dry").value = cfg.thresholds.humiDry ?? 40;
    document.getElementById("humi-humid").value = cfg.thresholds.humiHumid ?? 70;
    currentThresholds = cfg.thresholds;
  }

  // 2. Fill LED Pattern
  if (cfg.ledPattern) {
    document.getElementById("cold-on").value = cfg.ledPattern.coldOn;
    document.getElementById("cold-off").value = cfg.ledPattern.coldOff;
    document.getElementById("normal-on").value = cfg.ledPattern.normalOn;
    document.getElementById("normal-off").value = cfg.ledPattern.normalOff;
    document.getElementById("hot-on").value = cfg.ledPattern.hotOn;
    document.getElementById("hot-off").value = cfg.ledPattern.hotOff;
  }

  // 3. Fill NeoPixel Color
  if (cfg.neoColors) {
    document.getElementById("color-dry").value = cfg.neoColors.dry || "#0000ff";
    document.getElementById("color-ok").value = cfg.neoColors.ok || "#00ff00";
    document.getElementById("color-humid").value = cfg.neoColors.humid || "#ff0000";
  }

  // 4. Fill Devices (Quan trọng: Cập nhật relayList từ config)
  if (cfg.devices && Array.isArray(cfg.devices)) {
    cfg.devices.forEach((dev) => {
      const relay = relayList.find(r => r.name === dev.name);
      if (relay) {
        relay.state = (dev.status === "ON");
      }
    });
    renderRelays(); // Vẽ lại giao diện sau khi sync
  }

  // 5. Fill Settings
  if (cfg.settings) {
    if (cfg.settings.ssid !== undefined) document.getElementById("wifi-ssid").value = cfg.settings.ssid;
    if (cfg.settings.password !== undefined) document.getElementById("wifi-pass").value = cfg.settings.password;
    if (cfg.settings.token !== undefined) document.getElementById("core-token").value = cfg.settings.token;
    if (cfg.settings.server !== undefined) document.getElementById("core-server").value = cfg.settings.server;
    if (cfg.settings.port !== undefined) document.getElementById("core-port").value = cfg.settings.port;
  }
}

// ================ Navigation ================
function initNavigation() {
  const menuItems = document.querySelectorAll(".nav-item");
  const pages = document.querySelectorAll(".page");

  menuItems.forEach((item) => {
    item.addEventListener("click", () => {
      const target = item.getAttribute("data-target");
      menuItems.forEach((i) => i.classList.remove("active"));
      item.classList.add("active");
      pages.forEach((p) => {
        if (p.id === target) p.classList.add("active");
        else p.classList.remove("active");
      });
    });
  });
}

// ================ QUẢN LÝ THIẾT BỊ (RELAY/LED) ================

// 1. Định nghĩa danh sách thiết bị
const relayList = [
  { 
    id: "LED1", 
    name: "LED1", 
    gpio: 48, 
    state: false, 
    label: "LED Nhiệt", 
    icon: "fa-lightbulb" 
  },
  { 
    id: "LED2", 
    name: "LED2", 
    gpio: 45, 
    state: false, 
    label: "LED Độ ẩm", 
    icon: "fa-fan" 
  }
];

// 2. Hàm Render: Vẽ giao diện từ danh sách relayList
function renderRelays() {
  const container = document.querySelector(".device-items");
  if (!container) return;

  container.innerHTML = ""; // Xóa nội dung cũ

  relayList.forEach(relay => {
    // Tạo thẻ div cha
    const div = document.createElement("div");
    div.className = "device-item";

    // Button class dựa trên trạng thái
    const btnClass = relay.state ? "toggle-btn on" : "toggle-btn";
    const btnText = relay.state ? "BẬT" : "TẮT";

    div.innerHTML = `
      <div class="d-icon"><i class="fa-solid ${relay.icon}"></i></div>
      <div class="d-info">
        <span>${relay.label}</span>
        <small>GPIO ${relay.gpio}</small>
      </div>
      <button class="${btnClass}" onclick="toggleRelay('${relay.id}')">${btnText}</button>
    `;

    container.appendChild(div);
  });
}

// 3. Hàm Toggle: Xử lý logic khi bấm nút (Theo yêu cầu của bạn)
function toggleRelay(id) {
  const relay = relayList.find(r => r.id === id);
  if (relay) {
    relay.state = !relay.state; // Đảo trạng thái

    // Gửi JSON xuống board
    const relayJSON = {
      page: "device",
      value: {
        name: relay.name,
        status: relay.state ? "ON" : "OFF",
        gpio: relay.gpio
      }
    };
    sendJson(relayJSON);

    // Vẽ lại giao diện ngay lập tức để phản hồi nhanh
    renderRelays();
  }
}

// 4. Hàm cập nhật trạng thái khi nhận tin nhắn từ server (nếu có)
function updateDeviceStateFromServer(devVal) {
  if (!devVal || !devVal.name) return;
  
  const relay = relayList.find(r => r.name === devVal.name);
  if (relay) {
    relay.state = (devVal.status === "ON");
    renderRelays();
  }
}

// ================ Forms & Ngưỡng ================
let currentThresholds = { tempCold: 25, tempHot: 30, humiDry: 40, humiHumid: 70 };

function initForms() {
  const settingsForm = document.getElementById("settings-form");
  if (settingsForm) {
    settingsForm.addEventListener("submit", (e) => {
      e.preventDefault();
      const ssid = document.getElementById("wifi-ssid").value.trim();
      const pass = document.getElementById("wifi-pass").value.trim();
      const token = document.getElementById("core-token").value.trim();
      const server = document.getElementById("core-server").value.trim();
      const port = document.getElementById("core-port").value.trim();
      sendJson({ page: "setting", value: { ssid, password: pass, token, server, port } });
      setStatusMsg("settings-msg", "⏳ Đang lưu...", "info");
    });
  }

  const threshForm = document.getElementById("threshold-form");
  if (threshForm) {
    threshForm.addEventListener("submit", (e) => {
      e.preventDefault();
      currentThresholds = {
        tempCold: parseFloat(document.getElementById("temp-cold").value),
        tempHot: parseFloat(document.getElementById("temp-hot").value),
        humiDry: parseFloat(document.getElementById("humi-dry").value),
        humiHumid: parseFloat(document.getElementById("humi-humid").value),
      };
      sendJson({ page: "threshold", value: currentThresholds });
      setStatusMsg("threshold-msg", "⏳ Đang gửi...", "info");
    });
  }

  const ledForm = document.getElementById("temp-led-form");
  if (ledForm) {
    ledForm.addEventListener("submit", (e) => {
      e.preventDefault();
      const payload = {
        page: "led_pattern",
        value: {
          coldOn: parseInt(document.getElementById("cold-on").value),
          coldOff: parseInt(document.getElementById("cold-off").value),
          normalOn: parseInt(document.getElementById("normal-on").value),
          normalOff: parseInt(document.getElementById("normal-off").value),
          hotOn: parseInt(document.getElementById("hot-on").value),
          hotOff: parseInt(document.getElementById("hot-off").value),
        },
      };
      sendJson(payload);
      setStatusMsg("led-msg", "⏳ Đang gửi...", "info");
    });
  }

  const neoForm = document.getElementById("neo-form");
  if (neoForm) {
    neoForm.addEventListener("submit", (e) => {
      e.preventDefault();
      sendJson({
        page: "neo_color",
        value: {
          dry: document.getElementById("color-dry").value,
          ok: document.getElementById("color-ok").value,
          humid: document.getElementById("color-humid").value,
        },
      });
      setStatusMsg("neo-msg", "⏳ Đang gửi màu...", "info");
    });
  }
}

// ================ Chart (Dashboard) ================
let mainChart = null;
let mainChartUseFallback = false;
let mainChartCanvas = null;
let mainChartCtx = null;
let mainChartHistory = { labels: [], temp: [], humi: [] };

function initMainChart() {
  const canvas = document.getElementById("mainChart");
  if (!canvas) return;
  mainChartCanvas = canvas;

  if (window.Chart) {
    const ctx = canvas.getContext("2d");
    mainChart = new Chart(ctx, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          {
            label: "Nhiệt độ (°C)",
            data: [],
            borderColor: "#f39c12",
            backgroundColor: "rgba(243, 156, 18, 0.1)",
            tension: 0.4,
            fill: true,
            pointRadius: 2,
            borderWidth: 2
          },
          {
            label: "Độ ẩm (%)",
            data: [],
            borderColor: "#3498db",
            backgroundColor: "rgba(52, 152, 219, 0.1)",
            tension: 0.4,
            fill: true,
            pointRadius: 2,
            borderWidth: 2
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { position: 'top' },
        },
        scales: {
          x: { 
            display: true, 
            grid: { color: '#f0f0f0' },
            ticks: { font: { size: 10 }, maxTicksLimit: 6 } 
          },
          y: { 
            beginAtZero: true, 
            grid: { color: '#f0f0f0' },
            ticks: { font: { size: 10 } }
          },
        },
        animation: { duration: 0 },
      },
    });
    mainChartUseFallback = false;
  } else {
    console.warn("⚠️ Chart.js không tải được, dùng chế độ vẽ tay.");
    mainChartUseFallback = true;
    mainChartCtx = canvas.getContext("2d");
    resizeMainChartCanvas();
    window.addEventListener("resize", resizeMainChartCanvas);
  }
}

function resizeMainChartCanvas() {
  if (!mainChartUseFallback || !mainChartCanvas) return;
  const parent = mainChartCanvas.parentElement;
  if (!parent) return;
  mainChartCanvas.width = parent.clientWidth;
  mainChartCanvas.height = parent.clientHeight;
  drawMainChartFallback();
}

function updateMainChartData(temp, humi) {
  const now = new Date().toLocaleTimeString('vi-VN', { hour12: false, hour:'2-digit', minute:'2-digit', second:'2-digit' });

  if (mainChartUseFallback) {
    const maxPoints = 40;
    if (mainChartHistory.labels.length >= maxPoints) {
      mainChartHistory.labels.shift();
      mainChartHistory.temp.shift();
      mainChartHistory.humi.shift();
    }
    mainChartHistory.labels.push(now);
    mainChartHistory.temp.push(temp);
    mainChartHistory.humi.push(humi);
    drawMainChartFallback();
    return;
  }

  // Chart.js mode
  if (mainChart.data.labels.length > 40) {
    mainChart.data.labels.shift();
    mainChart.data.datasets[0].data.shift();
    mainChart.data.datasets[1].data.shift();
  }
  mainChart.data.labels.push(now);
  mainChart.data.datasets[0].data.push(temp);
  mainChart.data.datasets[1].data.push(humi);
  mainChart.update();
}

function drawMainChartFallback() {
  if (!mainChartUseFallback || !mainChartCanvas || !mainChartCtx) return;
  if (mainChartHistory.labels.length === 0) return;

  const ctx = mainChartCtx;
  const canvas = mainChartCanvas;
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);
  const marginLeft = 40;
  const marginRight = 10;
  const marginTop = 10;
  const marginBottom = 24;
  const plotW = w - marginLeft - marginRight;
  const plotH = h - marginTop - marginBottom;

  const allVals = mainChartHistory.temp.concat(mainChartHistory.humi);
  let minV = Math.min.apply(null, allVals);
  let maxV = Math.max.apply(null, allVals);
  if (minV === maxV) { minV -= 1; maxV += 1; }
  const pad = (maxV - minV) * 0.1;
  minV -= pad; maxV += pad;

  const n = mainChartHistory.labels.length;
  const stepX = n > 1 ? plotW / (n - 1) : plotW;

  // Background
  ctx.fillStyle = "#ffffff"; ctx.fillRect(0, 0, w, h);
  
  // Grid & Axis
  ctx.strokeStyle = "#eee"; ctx.lineWidth = 1;
  // Vẽ 5 dòng lưới ngang
  for(let i=0; i<=4; i++) {
      let y = marginTop + (plotH/4)*i;
      ctx.beginPath(); ctx.moveTo(marginLeft, y); ctx.lineTo(w-marginRight, y); ctx.stroke();
      // Vẽ số trục Y
      ctx.fillStyle = "#888"; ctx.font = "10px sans-serif"; ctx.textAlign = "right";
      let val = maxV - ((maxV-minV)/4)*i;
      ctx.fillText(Math.round(val), marginLeft-5, y+3);
  }

  function mapY(v) { return marginTop + plotH * (1 - (v - minV) / (maxV - minV)); }

  function drawLine(values, color) {
    if (!values.length) return;
    ctx.beginPath();
    for (let i = 0; i < values.length; i++) {
      const x = marginLeft + stepX * i;
      const y = mapY(values[i]);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.stroke();
  }
  drawLine(mainChartHistory.temp, "#f39c12");
  drawLine(mainChartHistory.humi, "#3498db");
}

// ================ Dashboard logic ================
function updateSensorCard(data) {
  const temp = data.temp;
  const humi = data.humi;

  if (typeof temp === "number") document.getElementById("temp-value").textContent = temp.toFixed(1);
  if (typeof humi === "number") document.getElementById("humi-value").textContent = humi.toFixed(0);

  const stateEl = document.getElementById("env-state");
  if (!stateEl) return;

  if (temp == null || humi == null) {
    stateEl.textContent = "Mất kết nối";
    stateEl.style.color = "#95a5a6";
    return;
  }

  let text = "BÌNH THƯỜNG";
  stateEl.style.color = "#2ecc71";

  if (temp > currentThresholds.tempHot || humi > currentThresholds.humiHumid) {
    text = "NGUY HIỂM";
    stateEl.style.color = "#e74c3c";
  } else if (temp < currentThresholds.tempCold || humi < currentThresholds.humiDry) {
    text = "CẢNH BÁO";
    stateEl.style.color = "#f39c12";
  }
  stateEl.textContent = text;
  updateMainChartData(temp, humi);
}

function updateTinyMlCard(data) {
  const score = data.score;
  if (typeof score === "number") {
    document.getElementById("tiny-score").textContent = score.toFixed(3);
    const chart = document.getElementById("tiny-chart");
    
    // Tạo cột mới
    const bar = document.createElement("div");
    bar.className = "bar";
    if (data.pred === "ANOM") bar.classList.add("pred-anom");
    if (data.gt === "ANOM") bar.classList.add("gt-anom");
    
    // Phần hiển thị giá trị (segment)
    const seg = document.createElement("div");
    seg.className = "bar-seg score";
    // Đảm bảo chiều cao tối thiểu để nhìn thấy
    let hPercent = Math.min(1, score) * 100;
    if(hPercent < 5) hPercent = 5; 
    seg.style.height = `${hPercent}%`;
    
    bar.appendChild(seg);
    chart.appendChild(bar);
    
    // Giới hạn số lượng cột hiển thị
    while (chart.children.length > 50) chart.removeChild(chart.firstChild);
  }

  document.getElementById("tiny-pred").textContent = data.pred === "ANOM" ? "Bất thường" : "Bình thường";
  document.getElementById("tiny-gt").textContent = data.gt === "ANOM" ? "Bất thường" : "Bình thường";
  if (typeof data.acc === "number") document.getElementById("tiny-acc").textContent = data.acc.toFixed(1);
}

function setStatusMsg(id, text, type) {
  const el = document.getElementById(id);
  if (el) {
    el.textContent = text;
    el.className = "msg-text " + (type === "error" ? "msg-error" : type === "success" ? "msg-success" : "msg-info");
    setTimeout(() => (el.textContent = ""), 5000);
  }
}

// ================ START ================
document.addEventListener("DOMContentLoaded", () => {
  initWebSocket();
  initNavigation();
  // Khởi tạo giao diện relay lần đầu
  renderRelays();
  initForms();
  initMainChart();
});

// Hàm confirm global
window.confirmFactoryReset = function() {
    if(confirm("Bạn có chắc chắn muốn xóa toàn bộ cài đặt về mặc định?")) {
        sendJson({ page: "reset_factory" });
    }
}