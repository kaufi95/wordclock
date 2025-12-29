// UI Elements
const colorPicker = document.getElementById("color-picker");

const redSlider = document.getElementById("red-slider");
const greenSlider = document.getElementById("green-slider");
const blueSlider = document.getElementById("blue-slider");

const redValue = document.getElementById("red-value");
const greenValue = document.getElementById("green-value");
const blueValue = document.getElementById("blue-value");

const languageButtons = document.querySelectorAll('input[name="language"]');
const prefixModeButtons = document.querySelectorAll('input[name="prefixMode"]');
const transitionButtons = document.querySelectorAll(".btn-transition");

const brightnessSlider = document.getElementById("brightness-slider");
const brightnessValue = document.getElementById("brightness-value");
const superBrightToggle = document.getElementById("superbright-toggle");

const transitionSpeedSlider = document.getElementById(
  "transition-speed-slider"
);
const transitionSpeedValue = document.getElementById("transition-speed-value");

const powerToggle = document.getElementById("power-toggle");

const colorModeToggle = document.getElementById("color-mode-toggle");
const colorPickerSection = document.getElementById("color-picker-section");
const colorSlidersSection = document.getElementById("color-sliders-section");

// WiFi reset button
const resetWifiBtn = document.getElementById("reset-wifi-btn");

// Track current state to detect changes
let currentState = {
  red: 255,
  green: 255,
  blue: 255,
  language: "dialekt",
  brightness: 50,
  enabled: true,
  superBright: false,
  prefixMode: 0,
  transition: 0,
  transitionSpeed: 2
};

// Color synchronization between picker and sliders
function updateColorFromSliders(sendUpdate = false) {
  const r = parseInt(redSlider.value);
  const g = parseInt(greenSlider.value);
  const b = parseInt(blueSlider.value);

  const hex = `#${r.toString(16).padStart(2, "0")}${g
    .toString(16)
    .padStart(2, "0")}${b.toString(16).padStart(2, "0")}`;
  colorPicker.value = hex;

  redValue.textContent = r;
  greenValue.textContent = g;
  blueValue.textContent = b;

  // Only send update when explicitly requested
  if (sendUpdate) {
    sendUpdateRequest();
  }
}

function updateSlidersFromColor() {
  const hex = colorPicker.value;
  const r = Math.max(0, Math.min(255, parseInt(hex.substring(1, 3), 16) || 0));
  const g = Math.max(0, Math.min(255, parseInt(hex.substring(3, 5), 16) || 0));
  const b = Math.max(0, Math.min(255, parseInt(hex.substring(5, 7), 16) || 0));

  redSlider.value = r;
  greenSlider.value = g;
  blueSlider.value = b;

  // Update display without triggering another update request
  redValue.textContent = r;
  greenValue.textContent = g;
  blueValue.textContent = b;

  // Send the update request
  sendUpdateRequest();
}

// Event listeners - update display on input, send request on change
redSlider.addEventListener("input", () => updateColorFromSliders(false));
redSlider.addEventListener("change", () => updateColorFromSliders(true));
greenSlider.addEventListener("input", () => updateColorFromSliders(false));
greenSlider.addEventListener("change", () => updateColorFromSliders(true));
blueSlider.addEventListener("input", () => updateColorFromSliders(false));
blueSlider.addEventListener("change", () => updateColorFromSliders(true));
colorPicker.addEventListener("input", updateSlidersFromColor);

brightnessSlider.addEventListener("input", () => {
  brightnessValue.textContent = brightnessSlider.value + "%";
});

brightnessSlider.addEventListener("change", () => {
  sendUpdateRequest();
});

superBrightToggle.addEventListener("change", sendUpdateRequest);

transitionSpeedSlider.addEventListener("input", () => {
  const speedLabels = [
    "Extra Slow",
    "Slow",
    "Medium",
    "Fast",
    "Very Fast"
  ];
  transitionSpeedValue.textContent =
    speedLabels[parseInt(transitionSpeedSlider.value)];
});

transitionSpeedSlider.addEventListener("change", () => {
  sendUpdateRequest();
});

// Add real-time update listeners for language change
languageButtons.forEach((button) => {
  button.addEventListener("change", sendUpdateRequest);
});

// Add real-time update listeners for prefix mode change
prefixModeButtons.forEach((button) => {
  button.addEventListener("change", sendUpdateRequest);
});

// Add click listeners for transition buttons - always send update
let currentTransition = 0;
transitionButtons.forEach((button) => {
  button.addEventListener("click", () => {
    const previousTransition = currentTransition;
    const transition = parseInt(button.dataset.transition);
    currentTransition = transition;

    // Update active state
    transitionButtons.forEach((btn) => btn.classList.remove("active"));
    button.classList.add("active");

    // Always send update to trigger animation preview
    // If clicking the same transition, add forcePreview flag
    sendUpdateRequest(previousTransition === transition);
  });
});

// Power toggle event listener
powerToggle.addEventListener("change", sendUpdateRequest);

// Color mode toggle event listener
colorModeToggle.addEventListener("change", () => {
  setColorMode(colorModeToggle.checked);
});

// WiFi reset button event listener
resetWifiBtn.addEventListener("click", resetWiFiSettings);

document.addEventListener("DOMContentLoaded", () => {
  // Load saved color mode preference
  const useSliders = loadColorMode();
  setColorMode(useSliders);

  onLoad();
  setupEventSource();
});

function updateUI(data) {
  updateColor(data.red, data.green, data.blue);
  updateLanguage(data.language);
  updateBrightness(data.brightness);
  updatePowerToggle(data.enabled);
  updateSuperBright(data.superBright);
  updatePrefixMode(data.prefixMode);
  updateTransition(data.transition);
  updateTransitionSpeed(data.transitionSpeed);

  // Update current state
  currentState = {
    red: data.red,
    green: data.green,
    blue: data.blue,
    language: data.language,
    brightness: data.brightness,
    enabled: data.enabled,
    superBright: data.superBright,
    prefixMode: data.prefixMode,
    transition: data.transition,
    transitionSpeed: data.transitionSpeed
  };
}

function updateColor(red, green, blue) {
  redSlider.value = red;
  greenSlider.value = green;
  blueSlider.value = blue;
  updateColorFromSliders();
}

function updateLanguage(language) {
  languageButtons.forEach((button) => {
    if (button.value === language) {
      button.checked = true;
    }
  });
}

function updateBrightness(brightness) {
  brightnessSlider.value = brightness || 50;
  brightnessValue.textContent = (brightness || 50) + "%";
}

function updatePowerToggle(enabled) {
  powerToggle.checked = enabled !== undefined ? enabled : true;
}

function updateSuperBright(superBright) {
  superBrightToggle.checked = superBright !== undefined ? superBright : false;
}

function updatePrefixMode(prefixMode) {
  const prefixValue = prefixMode !== undefined ? prefixMode : 0; // Default to always
  prefixModeButtons.forEach((button) => {
    if (parseInt(button.value) === prefixValue) {
      button.checked = true;
    }
  });
}

function updateTransition(transition) {
  const transitionValue = transition !== undefined ? transition : 0; // Default to none
  currentTransition = transitionValue;
  transitionButtons.forEach((button) => {
    if (parseInt(button.dataset.transition) === transitionValue) {
      button.classList.add("active");
    } else {
      button.classList.remove("active");
    }
  });
}

function updateTransitionSpeed(speed) {
  const speedValue = speed !== undefined ? speed : 2; // Default to medium
  transitionSpeedSlider.value = speedValue;
  const speedLabels = [
    "Extra Slow",
    "Slow",
    "Medium",
    "Fast",
    "Very Fast"
  ];
  transitionSpeedValue.textContent = speedLabels[speedValue];
}

// Color mode functions
function saveColorMode(useSliders) {
  localStorage.setItem(
    "wordclock-color-mode",
    useSliders ? "sliders" : "picker"
  );
}

function loadColorMode() {
  const saved = localStorage.getItem("wordclock-color-mode");
  return saved === "sliders";
}

function setColorMode(useSliders) {
  colorModeToggle.checked = useSliders;
  if (useSliders) {
    colorPickerSection.style.display = "none";
    colorSlidersSection.style.display = "block";
  } else {
    colorPickerSection.style.display = "block";
    colorSlidersSection.style.display = "none";
  }
  saveColorMode(useSliders);
}

// WiFi status functions removed

function getSelectedRGB() {
  return {
    red: parseInt(redSlider.value),
    green: parseInt(greenSlider.value),
    blue: parseInt(blueSlider.value)
  };
}

function getSelectedLanguage() {
  let selectedLanguage = "";
  languageButtons.forEach((button) => {
    if (button.checked) {
      selectedLanguage = button.value;
    }
  });
  return selectedLanguage;
}

function getSelectedPrefixMode() {
  let selectedPrefixMode = 0; // Default to always
  prefixModeButtons.forEach((button) => {
    if (button.checked) {
      selectedPrefixMode = parseInt(button.value);
    }
  });
  return selectedPrefixMode;
}

function getSelectedTransition() {
  return currentTransition;
}

function getSelectedBrightness() {
  return parseInt(brightnessSlider.value);
}

function getSelectedTransitionSpeed() {
  return parseInt(transitionSpeedSlider.value);
}

function onLoad() {
  fetch("/status")
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      return response.json();
    })
    .then((data) => {
      console.log("GET request successful");
      console.log(data);
      updateUI(data);
    })
    .catch((error) => console.error("Error sending GET request:", error));
}

function setupEventSource() {
  const eventSource = new EventSource("/events");

  eventSource.addEventListener("settings", (event) => {
    console.log("Settings update received from server:");
    const data = JSON.parse(event.data);
    console.log(data);
    updateUI(data);
  });

  eventSource.addEventListener("open", () => {
    console.log("SSE connection established");
  });

  eventSource.addEventListener("error", (error) => {
    console.error("SSE connection error:", error);
    if (eventSource.readyState === EventSource.CLOSED) {
      console.log("SSE connection closed, will attempt to reconnect...");
    }
  });
}

function sendUpdateRequest(forcePreview = false) {
  const rgb = getSelectedRGB();
  const language = getSelectedLanguage();
  const brightness = getSelectedBrightness();
  const enabled = powerToggle.checked;
  const superBright = superBrightToggle.checked;
  const prefixMode = getSelectedPrefixMode();
  const transition = getSelectedTransition();
  const transitionSpeed = getSelectedTransitionSpeed();

  // Build request body with only changed values
  const body = {};

  if (rgb.red !== currentState.red) body.red = rgb.red;
  if (rgb.green !== currentState.green) body.green = rgb.green;
  if (rgb.blue !== currentState.blue) body.blue = rgb.blue;
  if (language !== currentState.language) body.language = language;
  if (brightness !== currentState.brightness) body.brightness = brightness;
  if (enabled !== currentState.enabled) body.enabled = enabled;
  if (superBright !== currentState.superBright) body.superBright = superBright;
  if (prefixMode !== currentState.prefixMode) body.prefixMode = prefixMode;

  const transitionChanged = transition !== currentState.transition;
  if (transitionChanged) body.transition = transition;
  if (transitionSpeed !== currentState.transitionSpeed) body.transitionSpeed = transitionSpeed;

  // Add forcePreview flag ONLY if explicitly requested (clicking transition button)
  // Don't add it when other settings change
  if (forcePreview === true) {
    body.forcePreview = true;
    body.transition = transition; // Ensure transition is included for preview
  }

  // If nothing changed, don't send request (unless forcePreview is set)
  if (Object.keys(body).length === 0) {
    console.log("No changes detected, skipping update");
    return;
  }

  fetch("/update", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(body)
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      console.log("Settings updated successfully");
      console.log(body);

      // Update current state with sent values
      Object.assign(currentState, body);
    })
    .catch((error) => {
      console.error("Error sending update request:", error);
    });
}

// WiFi reset function
function resetWiFiSettings() {
  console.log("Reset WiFi button clicked");

  if (
    confirm(
      "Are you sure you want to reset WiFi settings? This will restart the device and open the configuration portal."
    )
  ) {
    console.log("User confirmed WiFi reset");

    // Disable button to prevent multiple clicks
    resetWifiBtn.disabled = true;
    resetWifiBtn.textContent = "🔄 Resetting...";

    fetch("/resetwifi", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      }
    })
      .then((response) => {
        console.log("Reset WiFi response:", response.status);
        if (response.ok) {
          alert(
            "WiFi settings reset. The device will restart and open the configuration portal."
          );
          return response.text();
        } else {
          throw new Error("Server returned error: " + response.status);
        }
      })
      .then((text) => {
        console.log("Reset response text:", text);
      })
      .catch((error) => {
        console.error("Error resetting WiFi:", error);
        alert("Error resetting WiFi settings: " + error.message);

        // Re-enable button on error
        resetWifiBtn.disabled = false;
        resetWifiBtn.textContent = "🔄 Reset WiFi Settings";
      });
  } else {
    console.log("User cancelled WiFi reset");
  }
}
