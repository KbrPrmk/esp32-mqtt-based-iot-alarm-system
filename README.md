<h1 align="center">🚨 ESP32 MQTT-Based IoT Alarm System</h1>
<p align="center"><b>PIR-triggered ultrasonic intrusion detection with MQTT publishing and Node-RED dashboard control</b></p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-00979D?style=flat&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Protocol-MQTT-660066?style=flat&logo=mqtt&logoColor=white" />
  <img src="https://img.shields.io/badge/Dashboard-Node--RED-8F0000?style=flat&logo=nodered&logoColor=white" />
  <img src="https://img.shields.io/badge/Simulated%20on-Wokwi-1A1A1A?style=flat&logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=flat" />
</p>

<p align="center">
  <a href="https://wokwi.com/projects/464392081577753601"><b>▶️ Run the live simulation on Wokwi</b></a>
</p>

---

## 📌 Overview

**ESP32 MQTT-Based IoT Alarm System** is a motion-triggered intrusion detection setup: a PIR sensor watches for movement, and once triggered, an HC-SR04 ultrasonic sensor measures the distance to the intruder. If that distance falls within an alarm range, an RGB LED blinks red — driven by a **hardware timer interrupt**, not a blocking `delay()` — while the live distance reading is published to an **MQTT broker**. A **Node-RED dashboard** visualizes the distance on a gauge and lets a user remotely arm/disarm the alarm LED.

---

## ✨ Features

- 🏃 **PIR-triggered sensing** — ultrasonic measurement only runs when motion is detected, saving cycles
- 📏 **Ultrasonic ranging** (HC-SR04) to determine how close the detected motion is
- 🔴 **Interrupt-driven alarm LED** — blinks every 500 ms via an ESP32 hardware timer + ISR, fully decoupled from the main loop
- 🔒 **Thread-safe shared state** — `portENTER_CRITICAL`/`_ISR` guards protect flags shared between the ISR and main loop
- 📡 **MQTT publishing** of live distance readings to a public broker
- 🎛️ **Remote control via Node-RED** — dashboard buttons publish ON/OFF commands to arm or disarm the LED
- 📊 **Real-time dashboard gauge** (0–400 cm) for live distance monitoring

---

## 🧰 Hardware Components

| Component | Role | Connection |
|---|---|---|
| ESP32 DevKit C V4 | Main controller — WiFi + MQTT client | — |
| PIR motion sensor | Motion trigger | OUT → GPIO 33 |
| HC-SR04 | Ultrasonic distance sensor | TRIG → GPIO 25, ECHO → GPIO 35 |
| RGB LED (common cathode) | Alarm indicator (red blink) | R → GPIO 5, G → GPIO 17, B → GPIO 16 (each via 1kΩ resistor) |
| 3× 1kΩ resistors | Current limiting for RGB channels | — |

---

## 🌐 Software & Platforms

| Tool | Purpose |
|---|---|
| **Wokwi** | Circuit simulation environment |
| **Arduino (C++)** | ESP32 firmware — WiFi, MQTT, sensor reads, timer interrupt |
| **MQTT** (PubSubClient) | Publish/subscribe messaging between ESP32 and Node-RED |
| **broker.hivemq.com:1883** | Public MQTT broker (used since Wokwi's network access to `test.mosquitto.org` is restricted) |
| **Node-RED** + dashboard | Visual flow programming — displays the distance gauge and LED control buttons |

---

## 📡 MQTT Topic Structure

> Topics below use a generic placeholder namespace — replace `esp32-alarm` with your own device ID if deploying multiple units.

| Topic | Direction | Description |
|---|---|---|
| `esp32-alarm/distance` | ESP32 → Node-RED | Distance sensor reading (cm) |
| `esp32-alarm/led` | Node-RED → ESP32 | LED control command (`ON` / `OFF`) |

---

## ⚙️ How It Works

**1. Motion-gated sensing**
The main loop only triggers an ultrasonic reading when the PIR sensor reports motion — avoiding constant polling.

**2. Distance evaluation**
If the measured distance falls between 0–200 cm, the system treats it as an alarm condition; beyond that, the LED stays off.

**3. Interrupt-driven blinking**
A hardware timer fires every 500 ms regardless of what the main loop is doing. Inside the ISR, if the alarm condition is active *and* the LED hasn't been remotely disabled, the RGB LED toggles red. All shared flags (`blinkActive`, `ledEnabled`, `ledState`) are protected by critical sections to prevent race conditions between the ISR and the main loop.

**4. MQTT publishing**
Every valid distance reading is formatted as a string and published to the broker, where Node-RED picks it up for the dashboard gauge.

**5. Remote arm/disarm**
Node-RED dashboard buttons publish `ON`/`OFF` to the LED topic. The ESP32's MQTT callback updates the `ledEnabled` flag accordingly — `OFF` also immediately clears the LED instead of waiting for the next ISR tick.

---

## 🔌 Circuit Diagram

Full wiring and component layout are available in the live simulation:

👉 **[Open in Wokwi](https://wokwi.com/projects/464392081577753601)**

<details>
<summary>📐 Pin connection summary</summary>

```
PIR OUT       → GPIO 33
HC-SR04 TRIG  → GPIO 25
HC-SR04 ECHO  → GPIO 35
RGB LED R     → GPIO 5  (via 1kΩ)
RGB LED G     → GPIO 17 (via 1kΩ)
RGB LED B     → GPIO 16 (via 1kΩ)
```
</details>

---

## 📊 Sample Serial Output

```
Hareket algilandi! Mesafe olculuyor...
Mesafe: 403.44 cm
MQTT publish: esp32-alarm/distance -> 403.44
LED blink PASIF (200 cm ustu).
```

```
LED blink AKTIF (0-200 cm araliginda).
```

```
MQTT mesaji alindi [esp32-alarm/led]: OFF
LED devre disi birakildi.
```

---

## 🚀 Getting Started

**Run in simulation (no hardware required):**
1. Open the [Wokwi project link](https://wokwi.com/projects/464392081577753601)
2. Click ▶️ **Play** to start the simulation and wait for the WiFi/MQTT connection log
3. Click the PIR sensor → **Simulate motion**
4. Drag the HC-SR04 distance slider below 200 cm to trigger the alarm LED

**Run with a live Node-RED dashboard:**
1. Install [Node.js](https://nodejs.org) (LTS) and then Node-RED: `npm install -g --unsafe-perm node-red`
2. Start it with `node-red` and open `http://localhost:1880`
3. Install the dashboard package via **Manage Palette → Install** → search `node-red-dashboard`
4. Import a flow that subscribes to `esp32-alarm/distance` (gauge) and publishes to `esp32-alarm/led` (buttons), using `broker.hivemq.com` as the MQTT broker
5. Deploy the flow, then open the dashboard at `http://localhost:1880/ui`

**Run on real hardware:**
1. Wire the components per the [pin connection summary](#-circuit-diagram) above
2. Install the required library: `PubSubClient`
3. Flash `sketch.ino` to an ESP32 via Arduino IDE or PlatformIO
4. Open the Serial Monitor at `115200` baud to view live logs

---

## 📁 Project Structure

```
esp32-mqtt-based-iot-alarm-system/
├── sketch.ino          # Main firmware — WiFi, MQTT, sensors, timer ISR
├── diagram.json         # Wokwi circuit/wiring definition
└── libraries.txt        # Required Arduino libraries
```

---

## 🛠️ Tech Stack

<p>
  <img src="https://img.shields.io/badge/ESP32-00979D?style=flat&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Arduino%20C%2B%2B-00979D?style=flat&logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/MQTT-660066?style=flat&logo=mqtt&logoColor=white" />
  <img src="https://img.shields.io/badge/Node--RED-8F0000?style=flat&logo=nodered&logoColor=white" />
  <img src="https://img.shields.io/badge/HC--SR04-Ultrasonic-orange?style=flat" />
  <img src="https://img.shields.io/badge/Wokwi-Simulator-1A1A1A?style=flat" />
</p>

---

## 📄 License

This project is licensed under the **MIT License**.

---

## 🙋 Author

**Kübra Parmak**
- GitHub: [@KbrPrmk](https://github.com/KbrPrmk)
- Wokwi: [@kbrprmk](https://wokwi.com/makers/kbrprmk)
