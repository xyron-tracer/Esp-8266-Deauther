# XYRON TRACER - ESP-DEAUTH PRO v5.0
![XYRON TRACER Banner](https://img.shields.io/badge/XYRON-TRACER-red)
![ESP8266](https://img.shields.io/badge/ESP8266-NodeMCU-green)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Build-blue)
![Version](https://img.shields.io/badge/Version-5.0--VIP-yellow)
![License](https://img.shields.io/badge/License-For_Testing_Only-red)

**Professional WiFi Deauthentication & Security Testing Platform**
*Full Stealth Mode • Military Grade • VIP System*

---

## 🚀 **FEATURES**

### **🔥 ATTACK CAPABILITIES**
- **Multi-Mode Deauthentication** (Broadcast & Targeted)
- **Beacon Spam & Probe Flood**
- **Rogue Access Point** with Phishing Page
- **Mixed Attack Mode** (Combined techniques)
- **Custom Packet Injection**

### **🛡️ STEALTH & EVASION**
- **MAC Address Rotation** (Auto & Manual)
- **Channel Hopping** (Random & Sequential)
- **TX Power Control** (0-20 dBm)
- **Packet Timing Jitter**
- **Anti-Detection Algorithms**
- **4 Stealth Levels** (Off → Extreme)

### **🌐 WEB INTERFACE**
- **Real-time Dashboard** with Live Stats
- **WebSocket Updates** (No page refresh)
- **Network Scanner** with SSID/BSSID detection
- **Configuration Management**
- **System Logs** with color coding
- **Mobile Responsive** Design
- **Keyboard Shortcuts**

### **⚙️ SYSTEM FEATURES**
- **Configuration Save/Load** (EEPROM)
- **Factory Reset** with confirmation
- **OTA Updates** (Planned)
- **Serial Console** with full control
- **LED Status Indicators**
- **Button Controls** (Short/Long press)
- **mDNS Support** (`xyron-tracer.local`)

---

## 📦 **HARDWARE REQUIREMENTS**

### **Mandatory:**
- **ESP8266 NodeMCU** (or compatible)
- **Micro USB Cable**
- **Computer** with USB port

### **Recommended:**
- **External Antenna** (for better range)
- **Power Bank** (for mobile operations)
- **SD Card Module** (for logging)

### **Tested Boards:**
- NodeMCU v1.0 (ESP-12E)
- Wemos D1 Mini
- ESP-12F Development Board

---

## 🔧 **INSTALLATION**

### **Method 1: PlatformIO (Recommended)**
```bash
# Clone repository
git clone https://github.com/xyrontracer/esp-deauth-pro.git
cd esp-deauth-pro

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor