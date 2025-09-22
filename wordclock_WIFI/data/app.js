// UI Elements
const colorPicker = document.getElementById('color-picker');

const redSlider = document.getElementById('red-slider');
const greenSlider = document.getElementById('green-slider');
const blueSlider = document.getElementById('blue-slider');

const redValue = document.getElementById('red-value');
const greenValue = document.getElementById('green-value');
const blueValue = document.getElementById('blue-value');

const languageButtons = document.querySelectorAll('input[name="language"]');

const brightnessSlider = document.getElementById('brightness-slider');
const brightnessValue = document.getElementById('brightness-value');

const powerToggle = document.getElementById('power-toggle');

const colorModeToggle = document.getElementById('color-mode-toggle');
const colorPickerSection = document.getElementById('color-picker-section');
const colorSlidersSection = document.getElementById('color-sliders-section');

// WiFi reset button
const resetWifiBtn = document.getElementById('reset-wifi-btn');

// Color synchronization between picker and sliders
function updateColorFromSliders(sendUpdate = false) {
  const r = parseInt(redSlider.value);
  const g = parseInt(greenSlider.value);
  const b = parseInt(blueSlider.value);

  const hex = `#${r.toString(16).padStart(2, '0')}${g
    .toString(16)
    .padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
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
redSlider.addEventListener('input', () => updateColorFromSliders(false));
redSlider.addEventListener('change', () => updateColorFromSliders(true));
greenSlider.addEventListener('input', () => updateColorFromSliders(false));
greenSlider.addEventListener('change', () => updateColorFromSliders(true));
blueSlider.addEventListener('input', () => updateColorFromSliders(false));
blueSlider.addEventListener('change', () => updateColorFromSliders(true));
colorPicker.addEventListener('input', updateSlidersFromColor);

brightnessSlider.addEventListener('input', () => {
  brightnessValue.textContent = brightnessSlider.value;
});

brightnessSlider.addEventListener('change', () => {
  sendUpdateRequest();
});

// Add real-time update listeners for language change
languageButtons.forEach((button) => {
  button.addEventListener('change', sendUpdateRequest);
});

// Power toggle event listener
powerToggle.addEventListener('change', sendUpdateRequest);

// Color mode toggle event listener
colorModeToggle.addEventListener('change', () => {
  setColorMode(colorModeToggle.checked);
});

// WiFi reset button event listener
resetWifiBtn.addEventListener('click', resetWiFiSettings);

document.addEventListener('DOMContentLoaded', () => {
  // Load saved color mode preference
  const useSliders = loadColorMode();
  setColorMode(useSliders);

  onLoad();
});

function updateUI(data) {
  updateColor(data.red, data.green, data.blue);
  updateLanguage(data.language);
  updateBrightness(data.brightness);
  updatePowerToggle(data.enabled);
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
  brightnessSlider.value = brightness || 128;
  brightnessValue.textContent = brightness || 128;
}

function updatePowerToggle(enabled) {
  powerToggle.checked = enabled !== undefined ? enabled : true;
}

// Color mode functions
function saveColorMode(useSliders) {
  localStorage.setItem('wordclock-color-mode', useSliders ? 'sliders' : 'picker');
}

function loadColorMode() {
  const saved = localStorage.getItem('wordclock-color-mode');
  return saved === 'sliders';
}

function setColorMode(useSliders) {
  colorModeToggle.checked = useSliders;
  if (useSliders) {
    colorPickerSection.style.display = 'none';
    colorSlidersSection.style.display = 'block';
  } else {
    colorPickerSection.style.display = 'block';
    colorSlidersSection.style.display = 'none';
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
  let selectedLanguage = '';
  languageButtons.forEach((button) => {
    if (button.checked) {
      selectedLanguage = button.value;
    }
  });
  return selectedLanguage;
}

function getSelectedBrightness() {
  return parseInt(brightnessSlider.value);
}

function onLoad() {
  fetch('/status')
    .then((response) => {
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      return response.json();
    })
    .then((data) => {
      console.log('GET request successful');
      console.log(data);
      updateUI(data);
    })
    .catch((error) => console.error('Error sending GET request:', error));
}

function sendUpdateRequest() {
  const rgb = getSelectedRGB();
  const language = getSelectedLanguage();
  const brightness = getSelectedBrightness();
  const enabled = powerToggle.checked;

  const body = {
    red: rgb.red,
    green: rgb.green,
    blue: rgb.blue,
    language: language,
    brightness: brightness,
    enabled: enabled
  };

  fetch('/update', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      console.log('Settings updated successfully');
      console.log(body);
    })
    .catch((error) => {
      console.error('Error sending update request:', error);
    });
}

// WiFi reset function
function resetWiFiSettings() {
  console.log('Reset WiFi button clicked');

  if (
    confirm(
      'Are you sure you want to reset WiFi settings? This will restart the device and open the configuration portal.'
    )
  ) {
    console.log('User confirmed WiFi reset');

    // Disable button to prevent multiple clicks
    resetWifiBtn.disabled = true;
    resetWifiBtn.textContent = '🔄 Resetting...';

    fetch('/resetwifi', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      }
    })
      .then((response) => {
        console.log('Reset WiFi response:', response.status);
        if (response.ok) {
          alert(
            'WiFi settings reset. The device will restart and open the configuration portal.'
          );
          return response.text();
        } else {
          throw new Error('Server returned error: ' + response.status);
        }
      })
      .then((text) => {
        console.log('Reset response text:', text);
      })
      .catch((error) => {
        console.error('Error resetting WiFi:', error);
        alert('Error resetting WiFi settings: ' + error.message);

        // Re-enable button on error
        resetWifiBtn.disabled = false;
        resetWifiBtn.textContent = '🔄 Reset WiFi Settings';
      });
  } else {
    console.log('User cancelled WiFi reset');
  }
}
