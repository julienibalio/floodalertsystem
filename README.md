# ESP32 Flood Detection & Alert System (WiFi + GSM)

[Watch my demo on TikTok](https://www.tiktok.com/@juengi4/video/7555206065688825105)

## Overview
This project is an **ESP32-based flood detection and alert system** that monitors **low-level and high-level water sensors** and sends real-time alerts using **WiFi (email/webhook)** and **GSM (SMS & voice call)**.

The system is designed for early flood warning:
- **Low-level flood** → Email + SMS alerts
- **High-level (rising) flood** → Email + SMS + **automatic phone call**

It uses **Make.com webhooks** for email notifications and a **SIM900A GSM module** for SMS and voice calls.

---

## Features

### Flood Detection
- Two analog water sensors:
  - **Low-level sensor**
  - **High-level sensor**
- Adjustable detection thresholds
- Wet-duration validation to prevent false triggers

### 📡 Alerts
| Flood Level | Email (WiFi) | SMS (GSM) | Voice Call |
|------------|-------------|-----------|------------|
| Low        | ✅           | ✅         | ❌          |
| High       | ✅           | ✅         | ✅          |

### 🕒 Safety & Stability
- 10-minute email cooldown per alert level
- Strict 20-second global cooldown after any alert
- Priority handling for HIGH flood events
- Debug logging via Serial Monitor

---

## Hardware Requirements

- **ESP32 Development Board**
- **SIM900A GSM Module**
- **2× Water Level Sensors**
- **External Power Supply**  
  (SIM900A requires sufficient current, typically ≥2A)
- Jumper wires & SIM card (with SMS/call capability)

---

## Software Requirements

- **Arduino IDE**
- **ESP32 Board Support Package**
- **Libraries**
  - `WiFi.h`
  - `HTTPClient.h`
  - `TinyGSM`
- **Make.com** account (for webhook-based email alerts)

---

## Pin Configuration

### ESP32 → SIM900A
| SIM900A | ESP32 |
|-------|-------|
| TX    | GPIO 26 |
| RX    | GPIO 27 |
| GND   | GND |
| VCC   | External Power |

> ⚠️ TX/RX are intentionally swapped in software to account for wiring variations.

### Water Sensors
| Sensor | ESP32 Pin |
|------|----------|
| Low-level sensor  | GPIO 34 |
| High-level sensor | GPIO 35 |

---

## 🌐 Network Configuration

### WiFi
Update the WiFi credentials in the code:
```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
