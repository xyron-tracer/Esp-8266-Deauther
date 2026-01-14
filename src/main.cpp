#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "defines.h"
#include "config_manager.h"
#include "attack_engine.h"
#include "web_interface.h"
#include "stealth_manager.h"

// ==================== GLOBAL OBJECTS ====================
ConfigManager configManager;
AttackEngine* attackEngine = nullptr;
WebInterface* webInterface = nullptr;
StealthManager* stealthManager = nullptr;

// ==================== GLOBAL VARIABLES ====================
volatile bool SYSTEM_RUNNING = true;
volatile bool ATTACK_ACTIVE = false;
volatile bool CONFIG_DIRTY = false;
volatile bool SCAN_REQUESTED = false;

bool FEATURE_WEB_ENABLED = true;
bool FEATURE_WS_ENABLED = true;
bool FEATURE_TELNET_ENABLED = false;
bool FEATURE_MDNS_ENABLED = true;
bool FEATURE_OTA_ENABLED = false;
bool FEATURE_SD_ENABLED = false;

Ticker statusTicker;
Ticker configSaveTicker;
Ticker heartbeatTicker;

// ==================== FUNCTION PROTOTYPES ====================
void setupPins();
void setupSerial();
void setupWiFi();
void setupServices();
void setupTimers();
void systemCheck();

void updateStatus();
void saveConfigIfDirty();
void sendHeartbeat();

void handleButtonPress();
void handleSerialInput();

void emergencyStop();
void systemReboot();
void factoryReset();

void printBanner();
void printSystemInfo();
void printHelp();

// ==================== SETUP ====================
void setup() {
    // Phase 1: Basic initialization
    setupPins();
    setupSerial();
    
    printBanner();
    DEBUG_PRINTLN("[SYSTEM] Initializing...");
    
    // Phase 2: Core systems
    if (!configManager.begin()) {
        DEBUG_PRINTLN("[ERROR] Config manager failed!");
        while (1) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(100);
        }
    }
    
    // Phase 3: Create managers
    attackEngine = new AttackEngine(configManager);
    stealthManager = new StealthManager(configManager);
    
    // Phase 4: Initialize subsystems
    setupWiFi();
    setupServices();
    setupTimers();
    
    // Phase 5: Start systems
    if (attackEngine->begin()) {
        DEBUG_PRINTLN("[ATTACK] Engine started");
    } else {
        DEBUG_PRINTLN("[ERROR] Attack engine failed!");
    }
    
    if (stealthManager->begin()) {
        DEBUG_PRINTLN("[STEALTH] Manager started");
    }
    
    // Phase 6: Start web interface if enabled
    if (FEATURE_WEB_ENABLED) {
        webInterface = new WebInterface(configManager, *attackEngine);
        if (webInterface->begin()) {
            DEBUG_PRINTLN("[WEB] Interface started");
        } else {
            DEBUG_PRINTLN("[ERROR] Web interface failed!");
            delete webInterface;
            webInterface = nullptr;
        }
    }
    
    // Phase 7: Final
    systemCheck();
    printSystemInfo();
    
    DEBUG_PRINTLN("[SYSTEM] Ready!");
    digitalWrite(LED_PIN, LED_ON);
    delay(500);
    digitalWrite(LED_PIN, LED_OFF);
}

// ==================== LOOP ====================
void loop() {
    // Handle button presses
    handleButtonPress();
    
    // Handle serial input
    if (Serial.available()) {
        handleSerialInput();
    }
    
    // Update web interface if enabled
    if (webInterface) {
        webInterface->update();
    }
    
    // Update attack engine
    if (attackEngine && ATTACK_ACTIVE) {
        attackEngine->update();
    }
    
    // Update stealth manager
    if (stealthManager) {
        stealthManager->update();
    }
    
    // Handle scan requests
    if (SCAN_REQUESTED) {
        SCAN_REQUESTED = false;
        if (attackEngine) {
            DEBUG_PRINTLN("[SYSTEM] Scanning networks...");
            attackEngine->scanNetworks();
        }
    }
    
    // Small delay to prevent watchdog
    delay(1);
}

// ==================== SETUP FUNCTIONS ====================
void setupPins() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Optional: Setup other pins
    // pinMode(16, OUTPUT); // WAKE pin for deep sleep
}

void setupSerial() {
    Serial.begin(SERIAL_BAUD);
    Serial.setTimeout(SERIAL_TIMEOUT);
    delay(100);
    
    // Clear any garbage
    while (Serial.available()) {
        Serial.read();
    }
}

void setupWiFi() {
    DEBUG_PRINTLN("[WIFI] Setting up AP...");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(DEFAULT_IP, DEFAULT_GATEWAY, DEFAULT_SUBNET);
    
    Config& config = configManager.get();
    WiFi.softAP(config.apSSID, config.apPassword, config.apChannel, config.apHidden, config.apMaxConn);
    
    DEBUG_PRINTF("[WIFI] AP: %s\n", config.apSSID);
    DEBUG_PRINTF("[WIFI] IP: %s\n", WiFi.softAPIP().toString().c_str());
    DEBUG_PRINTF("[WIFI] MAC: %s\n", WiFi.softAPmacAddress().c_str());
}

void setupServices() {
    DEBUG_PRINTLN("[SERVICES] Initializing...");
    
    if (FEATURE_MDNS_ENABLED) {
        if (MDNS.begin(MDNS_HOSTNAME)) {
            MDNS.addService("http", "tcp", WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", WEB_SOCKET_PORT);
            DEBUG_PRINTLN("[MDNS] Started");
        }
    }
    
    // Initialize SPIFFS for web files
    if (FEATURE_WEB_ENABLED) {
        if (!SPIFFS.begin()) {
            DEBUG_PRINTLN("[ERROR] SPIFFS failed!");
        } else {
            DEBUG_PRINTLN("[SPIFFS] Started");
        }
    }
}

void setupTimers() {
    // Status update every second
    statusTicker.attach(1.0, updateStatus);
    
    // Config save every 30 seconds if dirty
    configSaveTicker.attach(30.0, saveConfigIfDirty);
    
    // Heartbeat every 5 seconds
    heartbeatTicker.attach(5.0, sendHeartbeat);
    
    DEBUG_PRINTLN("[TIMERS] Started");
}

// ==================== TIMER CALLBACKS ====================
void updateStatus() {
    static uint32_t counter = 0;
    counter++;
    
    // Blink LED based on status
    if (ATTACK_ACTIVE) {
        // Fast blink when attacking
        digitalWrite(LED_PIN, (counter % 2) ? LED_ON : LED_OFF);
    } else {
        // Slow blink when idle
        digitalWrite(LED_PIN, (counter % 10 == 0) ? LED_ON : LED_OFF);
    }
    
    // Print stats every 10 seconds
    if (counter % 10 == 0 && attackEngine) {
        AttackStats stats = attackEngine->getStats();
        DEBUG_PRINTF("[STATS] Pkts: %u, Ch: %d, PPS: %.1f\n",
                    stats.total_packets, stats.current_channel, stats.packets_per_second);
    }
}

void saveConfigIfDirty() {
    if (CONFIG_DIRTY) {
        if (configManager.save()) {
            DEBUG_PRINTLN("[CONFIG] Auto-saved");
            CONFIG_DIRTY = false;
        }
    }
}

void sendHeartbeat() {
    // Update system uptime in config
    Config& config = configManager.get();
    config.uptime = millis() / 1000;
    CONFIG_DIRTY = true;
    
    // Update web interface if available
    if (webInterface) {
        // Send stats update via WebSocket
        if (attackEngine) {
            AttackStats stats = attackEngine->getStats();
            
            // Convert to JSON and broadcast
            String json = "{\"type\":\"stats\",\"packets\":";
            json += String(stats.total_packets);
            json += ",\"channel\":";
            json += String(stats.current_channel);
            json += ",\"active\":";
            json += ATTACK_ACTIVE ? "true" : "false";
            json += "}";
            
            // Broadcast to all WebSocket clients
            // Note: This is a simplified version
        }
    }
    
    // Check system health
    if (ESP.getFreeHeap() < 4000) {
        DEBUG_PRINTLN("[WARNING] Low memory!");
    }
}

// ==================== INPUT HANDLERS ====================
void handleButtonPress() {
    static unsigned long lastPress = 0;
    static bool buttonState = HIGH;
    static bool longPressDetected = false;
    
    bool currentState = digitalRead(BUTTON_PIN);
    unsigned long now = millis();
    
    // Button pressed (LOW)
    if (currentState == LOW && buttonState == HIGH) {
        lastPress = now;
        longPressDetected = false;
    }
    
    // Button held
    if (currentState == LOW && !longPressDetected) {
        if (now - lastPress > 3000) { // 3 second hold
            DEBUG_PRINTLN("[BUTTON] Long press - Emergency stop!");
            emergencyStop();
            longPressDetected = true;
        } else if (now - lastPress > 1000) { // 1 second hold
            DEBUG_PRINTLN("[BUTTON] Medium press - Toggle attack");
            if (attackEngine) {
                if (ATTACK_ACTIVE) {
                    attackEngine->stop();
                    ATTACK_ACTIVE = false;
                } else {
                    if (attackEngine->start()) {
                        ATTACK_ACTIVE = true;
                    }
                }
            }
            longPressDetected = true;
        }
    }
    
    // Button released
    if (currentState == HIGH && buttonState == LOW) {
        if (!longPressDetected && (now - lastPress < 1000)) {
            DEBUG_PRINTLN("[BUTTON] Short press - Toggle LED");
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    
    buttonState = currentState;
}

void handleSerialInput() {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    DEBUG_PRINTF("[SERIAL] Command: %s\n", input.c_str());
    
    // Parse command
    if (input == "help" || input == "?") {
        printHelp();
    } else if (input == "info") {
        printSystemInfo();
    } else if (input == "start") {
        if (attackEngine && !ATTACK_ACTIVE) {
            if (attackEngine->start()) {
                ATTACK_ACTIVE = true;
                DEBUG_PRINTLN("[SERIAL] Attack started");
            }
        }
    } else if (input == "stop") {
        if (attackEngine && ATTACK_ACTIVE) {
            attackEngine->stop();
            ATTACK_ACTIVE = false;
            DEBUG_PRINTLN("[SERIAL] Attack stopped");
        }
    } else if (input == "pause") {
        if (attackEngine && ATTACK_ACTIVE) {
            attackEngine->pause();
            DEBUG_PRINTLN("[SERIAL] Attack paused");
        }
    } else if (input == "resume") {
        if (attackEngine && ATTACK_ACTIVE) {
            attackEngine->resume();
            DEBUG_PRINTLN("[SERIAL] Attack resumed");
        }
    } else if (input == "scan") {
        SCAN_REQUESTED = true;
    } else if (input == "config") {
        configManager.print();
    } else if (input == "reboot") {
        systemReboot();
    } else if (input == "reset") {
        factoryReset();
    } else if (input == "status") {
        if (attackEngine) {
            AttackStats stats = attackEngine->getStats();
            DEBUG_PRINTF("Active: %s\n", ATTACK_ACTIVE ? "YES" : "NO");
            DEBUG_PRINTF("Packets: %u\n", stats.total_packets);
            DEBUG_PRINTF("Channel: %d\n", stats.current_channel);
            DEBUG_PRINTF("PPS: %.1f\n", stats.packets_per_second);
        }
    } else if (input == "web") {
        DEBUG_PRINTF("Web Interface: http://%s\n", WiFi.softAPIP().toString().c_str());
        DEBUG_PRINTF("WebSocket: ws://%s:81\n", WiFi.softAPIP().toString().c_str());
    } else if (input == "stealth") {
        if (stealthManager) {
            uint8_t level = stealthManager->getLevel();
            const char* levels[] = {"OFF", "LOW", "MEDIUM", "HIGH", "EXTREME"};
            DEBUG_PRINTF("Stealth Level: %s\n", levels[level]);
        }
    } else {
        DEBUG_PRINTLN("Unknown command. Type 'help' for list.");
    }
}

// ==================== SYSTEM FUNCTIONS ====================
void systemCheck() {
    DEBUG_PRINTLN("[CHECK] Running system checks...");
    
    // Check memory
    uint32_t freeHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[CHECK] Free heap: %u bytes\n", freeHeap);
    
    if (freeHeap < 8000) {
        DEBUG_PRINTLN("[WARNING] Low memory!");
    }
    
    // Check config
    if (!configManager.get().isValid()) {
        DEBUG_PRINTLN("[ERROR] Invalid config!");
    }
    
    // Check WiFi
    if (WiFi.softAPgetStationNum() < 0) {
        DEBUG_PRINTLN("[WARNING] WiFi AP started (no clients)");
    }
    
    DEBUG_PRINTLN("[CHECK] All checks passed");
}

void emergencyStop() {
    DEBUG_PRINTLN("[EMERGENCY] Stopping all operations!");
    
    if (attackEngine) {
        attackEngine->stop();
        ATTACK_ACTIVE = false;
    }
    
    if (webInterface) {
        // Can't stop web server easily, but we can disable updates
        delete webInterface;
        webInterface = nullptr;
    }
    
    if (stealthManager) {
        stealthManager->stop();
    }
    
    // Turn off LED
    digitalWrite(LED_PIN, LED_OFF);
    
    // Stop all timers
    statusTicker.detach();
    configSaveTicker.detach();
    heartbeatTicker.detach();
    
    // Wait for button release
    while (digitalRead(BUTTON_PIN) == LOW) {
        delay(100);
    }
    
    DEBUG_PRINTLN("[EMERGENCY] System halted. Press reset.");
    DEBUG_PRINTLN("[EMERGENCY] Type 'reboot' in serial to restart.");
    
    while (SYSTEM_RUNNING) {
        // Still handle serial for reboot command
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            if (cmd == "reboot") {
                systemReboot();
            }
        }
        delay(100);
    }
}

void systemReboot() {
    DEBUG_PRINTLN("[SYSTEM] Rebooting...");
    delay(1000);
    ESP.restart();
}

void factoryReset() {
    DEBUG_PRINTLN("[SYSTEM] Factory reset...");
    
    if (configManager.reset()) {
        DEBUG_PRINTLN("[SYSTEM] Config reset. Rebooting...");
        delay(2000);
        ESP.restart();
    } else {
        DEBUG_PRINTLN("[ERROR] Reset failed!");
    }
}

// ==================== PRINT FUNCTIONS ====================
void printBanner() {
    Serial.println();
    Serial.println("==================================================");
    Serial.println("    ██╗░░██╗██╗░░░██╗██████╗░░█████╗░███╗░░██╗");
    Serial.println("    ╚██╗██╔╝╚██╗░██╔╝██╔══██╗██╔══██╗████╗░██║");
    Serial.println("    ░╚███╔╝░░╚████╔╝░██████╔╝██║░░██║██╔██╗██║");
    Serial.println("    ░██╔██╗░░░╚██╔╝░░██╔══██╗██║░░██║██║╚████║");
    Serial.println("    ██╔╝╚██╗░░░██║░░░██║░░██║╚█████╔╝██║░╚███║");
    Serial.println("    ╚═╝░░╚═╝░░░╚═╝░░░╚═╝░░╚═╝░╚════╝░╚═╝░░╚══╝");
    Serial.println();
    Serial.println("    ESP-DEAUTH PRO v5.0 - VIP SYSTEM");
    Serial.println("    XYRON TRACER - FULL STEALTH MODE");
    Serial.println("==================================================");
    Serial.println();
}

void printSystemInfo() {
    Config& config = configManager.get();
    
    Serial.println();
    Serial.println("=== SYSTEM INFORMATION ===");
    Serial.printf("Firmware: %s %s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.printf("Build: %s\n", FIRMWARE_BUILD_DATE);
    Serial.printf("Chip ID: 0x%08X\n", ESP.getChipId());
    Serial.printf("Flash Size: %u MB\n", ESP.getFlashChipSize() / 1048576);
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
    Serial.printf("AP SSID: %s\n", config.apSSID);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("AP Clients: %d\n", WiFi.softAPgetStationNum());
    Serial.println("==========================");
    Serial.println();
}

void printHelp() {
    Serial.println();
    Serial.println("=== AVAILABLE COMMANDS ===");
    Serial.println("help/?       - Show this help");
    Serial.println("info         - System information");
    Serial.println("start        - Start attack");
    Serial.println("stop         - Stop attack");
    Serial.println("pause        - Pause attack");
    Serial.println("resume       - Resume attack");
    Serial.println("scan         - Scan networks");
    Serial.println("config       - Show config");
    Serial.println("status       - Attack status");
    Serial.println("web          - Show web interface URLs");
    Serial.println("stealth      - Show stealth level");
    Serial.println("reboot       - Reboot system");
    Serial.println("reset        - Factory reset");
    Serial.println();
    Serial.println("=== BUTTON CONTROLS ===");
    Serial.println("Short press  - Toggle LED");
    Serial.println("1 sec hold   - Toggle attack");
    Serial.println("3 sec hold   - Emergency stop");
    Serial.println("==========================");
    Serial.println();
}

// ==================== WEB INTERFACE CALLBACKS ====================
// These functions are called from web_interface.cpp when needed
extern "C" {
    // Called when web interface wants to start attack
    void webStartAttack() {
        if (attackEngine && !ATTACK_ACTIVE) {
            if (attackEngine->start()) {
                ATTACK_ACTIVE = true;
                DEBUG_PRINTLN("[WEB] Attack started");
            }
        }
    }
    
    // Called when web interface wants to stop attack
    void webStopAttack() {
        if (attackEngine && ATTACK_ACTIVE) {
            attackEngine->stop();
            ATTACK_ACTIVE = false;
            DEBUG_PRINTLN("[WEB] Attack stopped");
        }
    }
    
    // Called when web interface updates config
    void webConfigUpdated() {
        CONFIG_DIRTY = true;
        DEBUG_PRINTLN("[WEB] Configuration updated");
    }
    
    // Get current attack stats for web interface
    AttackStats webGetStats() {
        if (attackEngine) {
            return attackEngine->getStats();
        }
        return AttackStats(); // Return empty stats
    }
    
    // Get current config for web interface
    Config* webGetConfig() {
        return &configManager.get();
    }
}