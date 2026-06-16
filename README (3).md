<div align="center">

# SupplyMesh Edge — ESP32 IoT Device Simulator

<p>
  <img src="https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B%2FArduino-00979D?logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/Simulator-Wokwi-7B2D8B?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PC9zdmc+" />
  <img src="https://img.shields.io/badge/MQTT-HiveMQ-orange" />
  <img src="https://img.shields.io/badge/Device-SME001-green" />
</p>

<p>
ESP32 simulator that reads live sensor data, tracks GPS movement and battery drain,<br/>
and publishes JSON telemetry to MQTT every 10 seconds.
</p>

</div>

---

## What it does

- Reads **temperature** and **humidity** from a DHT22 sensor
- Simulates **GPS movement** with a ±0.01° random walk from New Delhi
- Tracks **battery drain** (0.05% per publish cycle)
- Publishes a **JSON payload** to `supplymesh/SME001/telemetry` via MQTT every 10 seconds
- Displays all six fields live on a **SSD1306 OLED**
- **RGB LED** shows connection state at a glance
- **Push button** triggers an immediate publish

---

## Live Output

**OLED Display**
```
SME001             MQTT:OK
---------------------------
Temp : 25.4 C
Hum  : 72 %
Batt : 94 %
Lat  : 28.6134
Lon  : 77.1998
Pub#3               T-7s
```

**MQTT Payload** published to `supplymesh/SME001/telemetry`
```json
{
  "device_id":   "SME001",
  "temperature": 25.4,
  "humidity":    72,
  "battery":     94,
  "latitude":    28.6134,
  "longitude":   77.1998
}
```

---

## Hardware

| Component | Wokwi ID | GPIO |
|---|---|---|
| ESP32 DevKit V1 | `esp32` | — |
| DHT22 | `dht1` | 15 |
| SSD1306 OLED 128×64 | `oled1` | SDA=21, SCL=22 |
| RGB LED (common cathode) | `rgb1` | R=25, G=26, B=27 |
| 220Ω Resistors ×3 | `r1` `r2` `r3` | — |
| Push Button | `btn1` | 14 |

### RGB LED States

| Colour | Meaning |
|---|---|
| 🟡 Yellow | Connecting to WiFi |
| 🔵 Blue | Connecting to MQTT |
| 🟢 Green | Connected — publishing normally |
| 🔴 Red | Connection dropped or publish failed |
| ⚪ White flash | Data just published (150ms) |

---

## Quick Start (Wokwi)

**1.** Open [wokwi.com](https://wokwi.com) → New Project → ESP32

**2.** Replace `diagram.json` with the one from this repo

**3.** Replace `sketch.ino` with the one from this repo

**4.** Add a new file called `libraries.txt` and paste the contents from this repo

**5.** Click **▶ Start Simulation**

The device will connect to `Wokwi-GUEST` WiFi → HiveMQ broker → publish first reading immediately → then every 10 seconds.

---

## Monitoring MQTT

Open the HiveMQ WebSocket client in your browser:

```
http://www.hivemq.com/demos/websocket-client/
```

| Field | Value |
|---|---|
| Host | `broker.hivemq.com` |
| Port | `8000` |

Click **Connect** → Subscribe to `supplymesh/SME001/telemetry`

> The Wokwi Serial Monitor also prints a formatted version of every payload.

---

## Libraries

| Library | Purpose |
|---|---|
| `PubSubClient` | MQTT client |
| `DHT sensor library` | DHT22 driver |
| `Adafruit SSD1306` | OLED driver |
| `Adafruit GFX Library` | Graphics primitives |
| `Adafruit BusIO` | I2C/SPI abstraction |
| `ArduinoJson` | JSON serialization |

`WiFi.h` is part of the ESP32 core — no install needed.

---

## How It Works

```
Boot
 ├── Connect WiFi (Wokwi-GUEST)
 ├── Connect MQTT (broker.hivemq.com:1883)
 └── Publish immediately

Loop every 10s (or button press)
 ├── Read DHT22 → temp + humidity
 ├── Walk GPS   → ±0.01° drift, bounded to Delhi area
 ├── Drain battery → -0.05% per cycle
 ├── Build JSON → dtostrf for exact decimal places
 ├── Publish to supplymesh/SME001/telemetry (retain=true)
 ├── Flash RGB white → back to green
 └── Update OLED
```

**GPS** starts at `28.61, 77.20` (New Delhi). Drifts ±0.01° per cycle within a bounding box.

**Battery** starts at 95%. Hits 0% after ~1900 cycles (~5.3 hours at 10s interval).

**DHT22 fallback** — if a read returns NaN, the last valid reading is reused so the publish still goes out.

**retain=true** — any subscriber that connects late immediately receives the last published reading without waiting for the next cycle.

---

## Project Structure

```
esp32-iot-simulator/
├── sketch.ino                  main firmware
├── diagram.json                Wokwi circuit wiring
├── libraries.txt               library list for Wokwi
├── README.md
└── docs/
    └── technical_explanation.docx
```

---

## MQTT Details

| Field | Value |
|---|---|
| Broker | `broker.hivemq.com` |
| Port | `1883` (TCP) / `8000` (WebSocket) |
| Topic | `supplymesh/SME001/telemetry` |
| Client ID | `SME001-ESP32` |
| Retain | `true` |
| Auth | none (public broker) |

---

<div align="center">
  <sub>Kaushal Kumar &nbsp;·&nbsp; B.Tech Mathematics & Computing, Central University of Jammu &nbsp;·&nbsp; <a href="https://github.com/ratwet">github.com/ratwet</a></sub>
</div>
