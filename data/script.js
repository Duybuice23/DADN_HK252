let websocket = null;
let currentThresholds = {
  tempCold: 25,
  tempHot: 30,
  humiDry: 40,
  humiHumid: 70,
};

const relayList = [
  {
    id: "LED1",
    name: "LED1",
    gpio: 48,
    state: false,
    label: "LED Nhiệt",
    icon: "fa-lightbulb",
  },
  {
    id: "LED2",
    name: "LED2",
    gpio: 45,
    state: false,
    label: "LED Độ ẩm",
    icon: "fa-fan",
  },
  {
    id: "LED_GAS",
    name: "LED Gas",
    gpio: 1,
    state: false,
    label: "LED Gas",
    icon: "fa-fire",
  },
];

function initWebSocket() {
  const gateway = `ws://${window.location.hostname}/ws`;
  console.log("Connecting WebSocket:", gateway);
  websocket = new WebSocket(gateway);

  websocket.onopen = () => {
    console.log("WebSocket opened");
    sendJson({ page: "get_config" });
  };

  websocket.onclose = () => {
    console.log("WebSocket closed, retry in 2s");
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
    console.log("WebSocket not ready");
  }
}

function onMessage(event) {
  let data;
  try {
    data = JSON.parse(event.data);
  } catch (error) {
    console.warn("JSON parse error:", error);
    return;
  }

  if (!data.page) return;

  switch (data.page) {
    case "sensor":
      updateSensorCard(data);
      break;

    case "config":
      fillConfigData(data.value);
      break;

    case "device":
      updateDeviceStateFromServer(data.value);
      break;

    case "setting_saved":
      setStatusMsg("settings-msg", "Đã lưu cài đặt mạng.", "success");
      break;

    case "reset_done":
      alert("Đã reset factory. Board đang khởi động lại...");
      location.reload();
      break;

    default:
      console.log("Unknown page:", data.page);
      break;
  }
}

function fillConfigData(cfg) {
  if (!cfg) return;

  if (cfg.thresholds) {
    currentThresholds = {
      tempCold: cfg.thresholds.tempCold ?? currentThresholds.tempCold,
      tempHot: cfg.thresholds.tempHot ?? currentThresholds.tempHot,
      humiDry: cfg.thresholds.humiDry ?? currentThresholds.humiDry,
      humiHumid: cfg.thresholds.humiHumid ?? currentThresholds.humiHumid,
    };
  }

  if (Array.isArray(cfg.devices)) {
    cfg.devices.forEach((dev) => {
      const relay = relayList.find((item) => item.name === dev.name);
      if (relay) {
        relay.state = dev.status === "ON";
      }
    });
    renderRelays();
  }

  if (cfg.settings) {
    if (cfg.settings.ssid !== undefined) {
      document.getElementById("wifi-ssid").value = cfg.settings.ssid;
    }
    if (cfg.settings.password !== undefined) {
      document.getElementById("wifi-pass").value = cfg.settings.password;
    }
    if (cfg.settings.token !== undefined) {
      document.getElementById("core-token").value = cfg.settings.token;
    }
    if (cfg.settings.server !== undefined) {
      document.getElementById("core-server").value = cfg.settings.server;
    }
    if (cfg.settings.port !== undefined) {
      document.getElementById("core-port").value = cfg.settings.port;
    }
  }
}

function initNavigation() {
  const menuItems = document.querySelectorAll(".nav-item");
  const pages = document.querySelectorAll(".page");

  menuItems.forEach((item) => {
    item.addEventListener("click", () => {
      const target = item.getAttribute("data-target");
      menuItems.forEach((menuItem) => menuItem.classList.remove("active"));
      item.classList.add("active");

      pages.forEach((page) => {
        if (page.id === target) page.classList.add("active");
        else page.classList.remove("active");
      });
    });
  });
}

function renderRelays() {
  const container = document.querySelector(".device-items");
  if (!container) return;

  container.innerHTML = "";

  relayList.forEach((relay) => {
    const div = document.createElement("div");
    div.className = "device-item";

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

function toggleRelay(id) {
  const relay = relayList.find((item) => item.id === id);
  if (!relay) return;

  relay.state = !relay.state;

  sendJson({
    page: "device",
    value: {
      name: relay.name,
      status: relay.state ? "ON" : "OFF",
      gpio: relay.gpio,
    },
  });

  renderRelays();
}

function updateDeviceStateFromServer(devVal) {
  if (!devVal || !devVal.name) return;

  const relay = relayList.find((item) => item.name === devVal.name);
  if (!relay) return;

  relay.state = devVal.status === "ON";
  renderRelays();
}

function initForms() {
  const settingsForm = document.getElementById("settings-form");
  if (!settingsForm) return;

  settingsForm.addEventListener("submit", (event) => {
    event.preventDefault();

    const ssid = document.getElementById("wifi-ssid").value.trim();
    const password = document.getElementById("wifi-pass").value.trim();
    const token = document.getElementById("core-token").value.trim();
    const server = document.getElementById("core-server").value.trim();
    const port = document.getElementById("core-port").value.trim();

    sendJson({
      page: "setting",
      value: { ssid, password, token, server, port },
    });

    setStatusMsg("settings-msg", "Đang lưu...", "info");
  });
}

function updateSensorCard(data) {
  const temp = data.temp;
  const humi = data.humi;
  const gas = data.gas;

  if (typeof temp === "number") {
    document.getElementById("temp-value").textContent = temp.toFixed(1);
  }

  if (typeof humi === "number") {
    document.getElementById("humi-value").textContent = humi.toFixed(0);
  }

  if (typeof gas === "number") {
    document.getElementById("gas-value").textContent = gas.toFixed(0);
  }

  const stateEl = document.getElementById("env-state");
  if (!stateEl) return;

  if (temp == null || humi == null) {
    stateEl.textContent = "Mất kết nối";
    stateEl.style.color = "#95a5a6";
    return;
  }

  let text = "BÌNH THƯỜNG";
  let color = "#2ecc71";

  if (temp > currentThresholds.tempHot || humi > currentThresholds.humiHumid) {
    text = "NGUY HIỂM";
    color = "#e74c3c";
  } else if (temp < currentThresholds.tempCold || humi < currentThresholds.humiDry) {
    text = "CẢNH BÁO";
    color = "#f39c12";
  }

  stateEl.textContent = text;
  stateEl.style.color = color;
}

function setStatusMsg(id, text, type) {
  const el = document.getElementById(id);
  if (!el) return;

  el.textContent = text;
  el.className =
    "msg-text " +
    (type === "error"
      ? "msg-error"
      : type === "success"
        ? "msg-success"
        : "msg-info");

  setTimeout(() => {
    el.textContent = "";
  }, 5000);
}

document.addEventListener("DOMContentLoaded", () => {
  initWebSocket();
  initNavigation();
  renderRelays();
  initForms();
});

window.confirmFactoryReset = function confirmFactoryReset() {
  if (confirm("Bạn có chắc chắn muốn xóa toàn bộ cài đặt về mặc định?")) {
    sendJson({ page: "reset_factory" });
  }
};
