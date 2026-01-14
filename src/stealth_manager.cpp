#include "stealth_manager.h"
extern "C" {
    #include "user_interface.h"
}

StealthManager::StealthManager(ConfigManager& configMgr)
    : config(configMgr), active(false), currentLevel(STEALTH_LEVEL_DEFAULT),
      currentChannel(1), currentTXPower(POWER_MEDIUM), packetJitter(0),
      antiDetection(false) {
    
    memset(currentMAC, 0, 6);
    memset(&stats, 0, sizeof(StealthStats));
}

StealthManager::~StealthManager() {
    stop();
}

bool StealthManager::begin() {
    Serial.println("[STEALTH] Initializing stealth manager...");
    
    Config& cfg = config.get();
    currentLevel = cfg.stealthLevel;
    currentChannel = cfg.targetChannel;
    currentTXPower = cfg.txPower;
    
    // Generate initial MAC
    generateRandomMAC(currentMAC);
    wifi_set_macaddr(STATION_IF, currentMAC);
    
    // Set initial TX power
    WiFi.setOutputPower(currentTXPower * 2.0f);
    
    // Apply stealth level
    applyStealthLevel();
    
    active = true;
    stats.isActive = true;
    stats.currentLevel = currentLevel;
    
    Serial.println("[STEALTH] Manager initialized");
    Serial.printf("[STEALTH] Level: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  currentLevel, currentMAC[0], currentMAC[1], currentMAC[2],
                  currentMAC[3], currentMAC[4], currentMAC[5]);
    
    return true;
}

void StealthManager::update() {
    if (!active) return;
    
    // Environment detection
    static unsigned long lastDetection = 0;
    if (millis() - lastDetection > 10000) {
        detectEnvironment();
        lastDetection = millis();
    }
    
    // Anti-detection evasion
    if (antiDetection) {
        static unsigned long lastEvasion = 0;
        if (millis() - lastEvasion > 5000) {
            evadeDetection();
            lastEvasion = millis();
        }
    }
}

void StealthManager::stop() {
    if (!active) return;
    
    macTimer.detach();
    channelTimer.detach();
    powerTimer.detach();
    patternTimer.detach();
    
    active = false;
    stats.isActive = false;
    
    Serial.println("[STEALTH] Manager stopped");
}

void StealthManager::setLevel(uint8_t level) {
    level = CONSTRAIN(level, STEALTH_LEVEL_MIN, STEALTH_LEVEL_MAX);
    
    if (level == currentLevel) return;
    
    currentLevel = level;
    stats.currentLevel = level;
    
    // Update config
    Config& cfg = config.get();
    cfg.stealthLevel = level;
    config.save();
    
    applyStealthLevel();
    
    Serial.printf("[STEALTH] Level changed to %d\n", level);
}

void StealthManager::applyStealthLevel() {
    // Stop all timers first
    macTimer.detach();
    channelTimer.detach();
    powerTimer.detach();
    patternTimer.detach();
    
    Config& cfg = config.get();
    
    switch (currentLevel) {
        case STEALTH_OFF:
            // No stealth features
            antiDetection = false;
            packetJitter = 0;
            break;
            
        case STEALTH_LOW:
            // Basic MAC rotation
            if (cfg.randomMac) {
                macTimer.attach_ms(cfg.macChangeInterval, [this]() { this->rotateMAC(); });
            }
            antiDetection = false;
            packetJitter = 10;
            break;
            
        case STEALTH_MEDIUM:
            // MAC rotation + channel hop
            if (cfg.randomMac) {
                macTimer.attach_ms(cfg.macChangeInterval, [this]() { this->rotateMAC(); });
            }
            if (cfg.channelHop) {
                channelTimer.attach_ms(cfg.hopInterval, [this]() { this->hopChannel(); });
            }
            antiDetection = true;
            packetJitter = 25;
            break;
            
        case STEALTH_HIGH:
            // Full stealth with pattern changes
            if (cfg.randomMac) {
                macTimer.attach_ms(cfg.macChangeInterval / 2, [this]() { this->rotateMAC(); });
            }
            if (cfg.channelHop) {
                channelTimer.attach_ms(cfg.hopInterval / 2, [this]() { this->hopChannel(); });
            }
            // Random TX power changes
            powerTimer.attach_ms(30000, [this]() { this->updateTXPower(); });
            // Pattern changes
            patternTimer.attach_ms(60000, [this]() { this->updatePattern(); });
            antiDetection = true;
            packetJitter = 50;
            break;
            
        case STEALTH_EXTREME:
            // Maximum stealth with aggressive changes
            macTimer.attach_ms(10000, [this]() { this->rotateMAC(); });
            channelTimer.attach_ms(5000, [this]() { this->hopChannel(); });
            powerTimer.attach_ms(15000, [this]() { this->updateTXPower(); });
            patternTimer.attach_ms(30000, [this]() { this->updatePattern(); });
            antiDetection = true;
            packetJitter = 100;
            break;
    }
    
    Serial.printf("[STEALTH] Applied level %d settings\n", currentLevel);
}

void StealthManager::rotateMAC() {
    generateRandomMAC(currentMAC);
    wifi_set_macaddr(STATION_IF, currentMAC);
    
    stats.macChanges++;
    
    Serial.printf("[STEALTH] MAC rotated to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  currentMAC[0], currentMAC[1], currentMAC[2],
                  currentMAC[3], currentMAC[4], currentMAC[5]);
}

String StealthManager::getCurrentMAC() const {
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            currentMAC[0], currentMAC[1], currentMAC[2],
            currentMAC[3], currentMAC[4], currentMAC[5]);
    return String(macStr);
}

void StealthManager::hopChannel() {
    currentChannel = getRandomChannel();
    wifi_set_channel(currentChannel);
    
    stats.channelHops++;
    
    // Update config
    Config& cfg = config.get();
    cfg.targetChannel = currentChannel;
    
    Serial.printf("[STEALTH] Channel hopped to %d\n", currentChannel);
}

void StealthManager::setTXPower(uint8_t level) {
    level = CONSTRAIN(level, POWER_MIN, POWER_MAX);
    
    if (level == currentTXPower) return;
    
    currentTXPower = level;
    WiFi.setOutputPower(level * 2.0f);
    
    stats.txPowerChanges++;
    
    // Update config
    Config& cfg = config.get();
    cfg.txPower = level;
    
    Serial.printf("[STEALTH] TX power set to %d dBm\n", level * 5);
}

void StealthManager::updateTXPower() {
    uint8_t newPower = getRandomTXPower();
    setTXPower(newPower);
}

void StealthManager::setJitter(uint16_t jitter) {
    packetJitter = jitter;
    Serial.printf("[STEALTH] Packet jitter set to %d ms\n", jitter);
}

void StealthManager::enableAntiDetection(bool enable) {
    antiDetection = enable;
    Serial.printf("[STEALTH] Anti-detection %s\n", enable ? "enabled" : "disabled");
}

void StealthManager::generateRandomMAC(uint8_t* mac) {
    mac[0] = 0x02; // Locally administered
    for (int i = 1; i < 6; i++) {
        mac[i] = random(256);
    }
}

uint8_t StealthManager::getRandomChannel() {
    return random(MIN_CHANNEL, MAX_CHANNEL + 1);
}

uint8_t StealthManager::getRandomTXPower() {
    return random(POWER_MIN, POWER_MAX + 1);
}

void StealthManager::detectEnvironment() {
    // Simulate environment detection
    int clientCount = WiFi.softAPgetStationNum();
    int channelActivity = random(0, 100);
    
    // Adjust stealth based on environment
    if (clientCount > 5 || channelActivity > 70) {
        // High activity environment
        if (currentLevel < STEALTH_HIGH) {
            setLevel(STEALTH_HIGH);
            stats.detectionAttempts++;
        }
    } else {
        // Low activity environment
        if (currentLevel > STEALTH_MEDIUM) {
            setLevel(STEALTH_MEDIUM);
        }
    }
}

void StealthManager::evadeDetection() {
    // Random evasion techniques
    int technique = random(0, 4);
    
    switch (technique) {
        case 0:
            // Quick MAC change
            rotateMAC();
            break;
        case 1:
            // Channel hop
            hopChannel();
            break;
        case 2:
            // TX power adjustment
            updateTXPower();
            break;
        case 3:
            // Pattern change
            updatePattern();
            break;
    }
    
    stats.evasionSuccess++;
    Serial.printf("[STEALTH] Evasion technique %d applied\n", technique);
}

void StealthManager::updatePattern() {
    generatePacketPattern();
    generateTimingPattern();
    
    Serial.println("[STEALTH] Attack patterns updated");
}

void StealthManager::generatePacketPattern() {
    // Modify packet patterns to avoid signature detection
    // This would modify how packets are structured
}

void StealthManager::generateTimingPattern() {
    // Modify timing patterns to avoid behavioral detection
    // This would modify packet timing intervals
}

StealthManager::StealthStats StealthManager::getStats() const {
    StealthStats currentStats = stats;
    currentStats.isActive = active;
    currentStats.currentLevel = currentLevel;
    return currentStats;
}

void StealthManager::printDebugInfo() {
    Serial.println("\n=== STEALTH MANAGER DEBUG ===");
    Serial.printf("Active: %s\n", active ? "YES" : "NO");
    Serial.printf("Level: %d\n", currentLevel);
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  currentMAC[0], currentMAC[1], currentMAC[2],
                  currentMAC[3], currentMAC[4], currentMAC[5]);
    Serial.printf("Channel: %d\n", currentChannel);
    Serial.printf("TX Power: %d dBm\n", currentTXPower * 5);
    Serial.printf("Jitter: %d ms\n", packetJitter);
    Serial.printf("Anti-detection: %s\n", antiDetection ? "ON" : "OFF");
    Serial.printf("Stats - MAC Changes: %u, Channel Hops: %u\n",
                  stats.macChanges, stats.channelHops);
    Serial.printf("Stats - TX Power Changes: %u, Evasions: %u/%u\n",
                  stats.txPowerChanges, stats.evasionSuccess, stats.detectionAttempts);
    Serial.println("=============================\n");
}