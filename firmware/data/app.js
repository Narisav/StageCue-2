const RECONNECT_DELAY = 2000;
let socket;
let reconnectTimer;

const cueCards = new Map();

function $(selector) {
  return document.querySelector(selector);
}

function updateStatus(message) {
  const el = $("#status");
  if (el) {
    el.textContent = message;
  }
}

function updateWifiStatus(message) {
  const el = $("#wifiStatus");
  if (el) {
    el.textContent = message || "";
  }
}

function ensureSocket() {
  if (socket && socket.readyState === WebSocket.OPEN) {
    return true;
  }
  updateStatus("🔴 Déconnecté. Tentative de reconnexion...");
  return false;
}

function sendMessage(payload) {
  if (!ensureSocket()) {
    return;
  }
  socket.send(JSON.stringify(payload));
}

function triggerCue(index) {
  const input = document.querySelector(`[data-cue-input="${index}"]`);
  const text = input ? input.value.trim() : "";
  sendMessage({ type: "trigger", cue: index, text });
}

function releaseCue(index) {
  sendMessage({ type: "release", cue: index });
}

function renameCue(index, text) {
  sendMessage({ type: "rename", cue: index, text });
}

function handlePointerStart(event) {
  const index = Number(event.currentTarget.dataset.cue);
  event.preventDefault();
  triggerCue(index);
  if (typeof event.pointerId === "number") {
    event.currentTarget.setPointerCapture(event.pointerId);
  }
}

function handlePointerEnd(event) {
  const index = Number(event.currentTarget.dataset.cue);
  event.preventDefault();
  releaseCue(index);
  if (typeof event.pointerId === "number") {
    event.currentTarget.releasePointerCapture(event.pointerId);
  }
}

function bindCueControls() {
  document.querySelectorAll(".cue").forEach((card) => {
    const index = Number(card.dataset.cue);
    const triggerButton = card.querySelector(".cue-trigger");
    const releaseButton = card.querySelector(".cue-release");
    const input = card.querySelector("input");

    if (triggerButton) {
      triggerButton.addEventListener("pointerdown", handlePointerStart);
      triggerButton.addEventListener("pointerup", handlePointerEnd);
      triggerButton.addEventListener("pointercancel", handlePointerEnd);
      triggerButton.addEventListener("pointerleave", handlePointerEnd);

      triggerButton.addEventListener("keydown", (evt) => {
        if (evt.code === "Space" || evt.code === "Enter") {
          evt.preventDefault();
          triggerCue(index);
        }
      });

      triggerButton.addEventListener("keyup", (evt) => {
        if (evt.code === "Space" || evt.code === "Enter") {
          evt.preventDefault();
          releaseCue(index);
        }
      });
    }

    if (releaseButton) {
      releaseButton.addEventListener("click", () => releaseCue(index));
      releaseButton.addEventListener("touchend", (evt) => {
        evt.preventDefault();
        releaseCue(index);
      });
    }

    if (input) {
      input.dataset.lastValue = input.value.trim();
      const commitRename = () => {
        const value = input.value.trim();
        if (input.dataset.lastValue !== value) {
          renameCue(index, value);
          input.dataset.lastValue = value;
        }
      };

      input.addEventListener("change", commitRename);
      input.addEventListener("blur", commitRename);
    }

    cueCards.set(index, card);
  });
}

function applyCueState({ index, text, active }) {
  const card = cueCards.get(index);
  if (!card) {
    return;
  }

  const input = card.querySelector("input");
  if (input && document.activeElement !== input) {
    input.value = text;
    input.dataset.lastValue = text;
  }

  card.classList.toggle("active", Boolean(active));
}

function handleInitMessage(payload) {
  if (Array.isArray(payload.cues)) {
    payload.cues.forEach(applyCueState);
  }

  if (payload.wifi) {
    const { mode, ip } = payload.wifi;
    updateWifiStatus(`📡 Mode: ${mode || "inconnu"} – IP: ${ip || "n/a"}`);
  }

  updateStatus("🟢 Connecté au contrôleur");
}

function handleAckMessage(payload) {
  if (!payload.ok) {
    console.warn("Action refusée", payload);
  }
}

function handleSocketMessage(event) {
  try {
    const payload = JSON.parse(event.data);
    switch (payload.type) {
      case "init":
        handleInitMessage(payload);
        break;
      case "cue":
        applyCueState(payload);
        break;
      case "snapshot":
        if (Array.isArray(payload.cues)) {
          payload.cues.forEach(applyCueState);
        }
        break;
      case "ack":
        handleAckMessage(payload);
        break;
      default:
        console.debug("Message inconnu", payload);
        break;
    }
  } catch (error) {
    console.error("Message invalide", event.data, error);
  }
}

function openSocket() {
  clearTimeout(reconnectTimer);

  const protocol = location.protocol === "https:" ? "wss" : "ws";
  socket = new WebSocket(`${protocol}://${location.host}/ws`);

  socket.addEventListener("open", () => {
    updateStatus("🟢 Connecté au contrôleur");
  });

  socket.addEventListener("close", () => {
    updateStatus("🔴 Déconnecté. Reconnexion en cours...");
    reconnectTimer = setTimeout(openSocket, RECONNECT_DELAY);
  });

  socket.addEventListener("error", (event) => {
    console.error("WebSocket error", event);
    socket.close();
  });

  socket.addEventListener("message", handleSocketMessage);
}

document.addEventListener("DOMContentLoaded", () => {
  bindCueControls();
  openSocket();
});

