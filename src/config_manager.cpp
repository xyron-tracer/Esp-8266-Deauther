#include "config_manager.h"
#include <ArduinoJson.h>

ConfigManager::ConfigManager() {}

bool ConfigManager::begin() {
    EEPROM.begin(EEPROM_SIZE);
    initialized = true;
    
    if (!load()) {
        Serial.println("[CONFIG] Load failed, using defaults");
        setDefaults();
        save(true);
    }
    
    return true;
}

bool ConfigManager::load() {
    if (!initialized) return false;
    
    EEPROM.get(CONFIG_ADDRESS, config);
    
    if (!config.isValid()) {
        Serial.println("[CONFIG] Invalid version");
        return false;
    }
    
    if (!config.verifyChecksum()) {
        Serial.println("[CONFIG] Checksum failed");
        return false;
    }
    
    dirty = false;
    Serial.println("[CONFIG] Loaded successfully");
    return true;
}

bool ConfigManager::save(bool force) {
    if (!initialized) return false;
    
    if (!force && !dirty) {
        return true;
    }
    
    config.updateChecksum();
    EEPROM.put(CONFIG_ADDRESS, config);
    
    bool success = EEPROM.commit();
    
    if (success) {
        dirty = false;
        Serial.println("[CONFIG] Saved successfully");
    } else {
        Serial.println("[CONFIG] Save failed");
    }
    
    return success;
}

bool ConfigManager::reset() {
    setDefaults();
    dirty = true;
    return save(true);
}

void ConfigManager::setDefaults() {
    config = Config();
    config.updateChecksum();
}

bool ConfigManager::addNetwork(const WiFiNetwork& network) {
    if (config.networkCount >= 10) return false;
    
    config.savedNetworks[config.networkCount] = network;
    config.networkCount++;
    dirty = true;
    
    return true;
}

bool ConfigManager::removeNetwork(uint8_t index) {
    if (index >= config.networkCount) return false;
    
    for (uint8_t i = index; i < config.networkCount - 1; i++) {
        config.savedNetworks[i] = config.savedNetworks[i + 1];
    }
    
    config.networkCount--;
    dirty = true;
    return true;
}

WiFiNetwork* ConfigManager::getNetwork(uint8_t index) {
    if (index >= config.networkCount) return nullptr;
    return &config.savedNetworks[index];
}

void ConfigManager::incrementPacketCount(uint32_t count) {
    config.packetCount += count;
    dirty = true;
}

void ConfigManager::print() {
    Serial.println("\n=== CONFIGURATION ===");
    Serial.printf("Version: 0x%02X\n", config.version);
    Serial.printf("AP SSID: %s\n", config.apSSID);
    Serial.printf("AP Pass: %s\n", config.apPassword);
    Serial.printf("Channel: %d\n", config.apChannel);
    Serial.printf("Attack Mode: %d\n", config.attackMode);
    Serial.printf("Deauth Rate: %d pps\n", config.deauthRate);
    Serial.printf("Beacon Rate: %d pps\n", config.beaconRate);
    Serial.printf("Stealth Level: %d\n", config.stealthLevel);
    Serial.printf("Channel Hop: %s\n", config.channelHop ? "ON" : "OFF");
    Serial.printf("Hop Interval: %d ms\n", config.hopInterval);
    Serial.printf("Random MAC: %s\n", config.randomMac ? "ON" : "OFF");
    Serial.printf("MAC Change: %d ms\n", config.macChangeInterval);
    Serial.printf("Rogue AP: %s\n", config.rogueAP ? "ON" : "OFF");
    Serial.printf("Rogue SSID: %s\n", config.rogueSSID);
    Serial.printf("Web Enabled: %s\n", config.webEnabled ? "ON" : "OFF");
    Serial.printf("WS Enabled: %s\n", config.wsEnabled ? "ON" : "OFF");
    Serial.printf("Packet Count: %u\n", config.packetCount);
    Serial.printf("Attack Time: %u s\n", config.attackTime);
    Serial.printf("Uptime: %u s\n", config.uptime);
    Serial.printf("Network Count: %d\n", config.networkCount);
    Serial.println("====================\n");
}

void ConfigManager::printNetworks() {
    if (config.networkCount == 0) {
        Serial.println("[CONFIG] No saved networks");
        return;
    }
    
    Serial.println("\n=== SAVED NETWORKS ===");
    for (uint8_t i = 0; i < config.networkCount; i++) {
        WiFiNetwork* net = &config.savedNetworks[i];
        Serial.printf("[%d] %s (Ch:%d, RSSI:%d, %02X:%02X:%02X:%02X:%02X:%02X)\n",
            i, net->ssid, net->channel, net->rssi,
            net->bssid[0], net->bssid[1], net->bssid[2],
            net->bssid[3], net->bssid[4], net->bssid[5]);
    }
    Serial.println("======================\n");
}

String ConfigManager::toJSON() {
    DynamicJsonDocument doc(4096);
    
    // AP Settings
    doc["ap"]["ssid"] = config.apSSID;
    doc["ap"]["password"] = config.apPassword;
    doc["ap"]["channel"] = config.apChannel;
    doc["ap"]["hidden"] = config.apHidden;
    
    // Attack Settings
    doc["attack"]["mode"] = config.attackMode;
    doc["attack"]["deauth_rate"] = config.deauthRate;
    doc["attack"]["beacon_rate"] = config.beaconRate;
    doc["attack"]["probe_rate"] = config.probeRate;
    doc["attack"]["channel"] = config.targetChannel;
    doc["attack"]["attack_all"] = config.attackAll;
    
    // Stealth Settings
    doc["stealth"]["level"] = config.stealthLevel;
    doc["stealth"]["random_mac"] = config.randomMac;
    doc["stealth"]["channel_hop"] = config.channelHop;
    doc["stealth"]["hop_interval"] = config.hopInterval;
    doc["stealth"]["mac_change"] = config.macChangeInterval;
    doc["stealth"]["tx_power"] = config.txPower;
    
    // Rogue AP
    doc["rogue"]["enabled"] = config.rogueAP;
    doc["rogue"]["ssid"] = config.rogueSSID;
    doc["rogue"]["password"] = config.roguePassword;
    
    // Web
    doc["web"]["enabled"] = config.webEnabled;
    doc["web"]["ws_enabled"] = config.wsEnabled;
    doc["web"]["auth_enabled"] = config.authEnabled;
    doc["web"]["username"] = config.webUser;
    doc["web"]["password"] = config.webPass;
    
    // Stats
    doc["stats"]["packets"] = config.packetCount;
    doc["stats"]["attack_time"] = config.attackTime;
    doc["stats"]["uptime"] = config.uptime;
    
    // Networks
    JsonArray networks = doc.createNestedArray("networks");
    for (uint8_t i = 0; i < config.networkCount; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = config.savedNetworks[i].ssid;
        net["channel"] = config.savedNetworks[i].channel;
        net["rssi"] = config.savedNetworks[i].rssi;
        
        char bssidStr[18];
        sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                config.savedNetworks[i].bssid[0],
                config.savedNetworks[i].bssid[1],
                config.savedNetworks[i].bssid[2],
                config.savedNetworks[i].bssid[3],
                config.savedNetworks[i].bssid[4],
                config.savedNetworks[i].bssid[5]);
        net["bssid"] = bssidStr;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool ConfigManager::fromJSON(const String& json) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("[CONFIG] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    // AP Settings
    if (doc.containsKey("ap")) {
        JsonObject ap = doc["ap"];
        if (ap.containsKey("ssid")) strlcpy(config.apSSID, ap["ssid"], MAX_SSID_LENGTH);
        if (ap.containsKey("password")) strlcpy(config.apPassword, ap["password"], MAX_PASS_LENGTH);
        if (ap.containsKey("channel")) config.apChannel = ap["channel"];
        if (ap.containsKey("hidden")) config.apHidden = ap["hidden"];
    }
    
    // Attack Settings
    if (doc.containsKey("attack")) {
        JsonObject attack = doc["attack"];
        if (attack.containsKey("mode")) config.attackMode = attack["mode"];
        if (attack.containsKey("deauth_rate")) config.deauthRate = attack["deauth_rate"];
        if (attack.containsKey("beacon_rate")) config.beaconRate = attack["beacon_rate"];
        if (attack.containsKey("probe_rate")) config.probeRate = attack["probe_rate"];
        if (attack.containsKey("channel")) config.targetChannel = attack["channel"];
        if (attack.containsKey("attack_all")) config.attackAll = attack["attack_all"];
    }
    
    // Stealth Settings
    if (doc.containsKey("stealth")) {
        JsonObject stealth = doc["stealth"];
        if (stealth.containsKey("level")) config.stealthLevel = stealth["level"];
        if (stealth.containsKey("random_mac")) config.randomMac = stealth["random_mac"];
        if (stealth.containsKey("channel_hop")) config.channelHop = stealth["channel_hop"];
        if (stealth.containsKey("hop_interval")) config.hopInterval = stealth["hop_interval"];
        if (stealth.containsKey("mac_change")) config.macChangeInterval = stealth["mac_change"];
        if (stealth.containsKey("tx_power")) config.txPower = stealth["tx_power"];
    }
    
    // Rogue AP
    if (doc.containsKey("rogue")) {
        JsonObject rogue = doc["rogue"];
        if (rogue.containsKey("enabled")) config.rogueAP = rogue["enabled"];
        if (rogue.containsKey("ssid")) strlcpy(config.rogueSSID, rogue["ssid"], MAX_SSID_LENGTH);
        if (rogue.containsKey("password")) strlcpy(config.roguePassword, rogue["password"], MAX_PASS_LENGTH);
    }
    
    // Web
    if (doc.containsKey("web")) {
        JsonObject web = doc["web"];
        if (web.containsKey("enabled")) config.webEnabled = web["enabled"];
        if (web.containsKey("ws_enabled")) config.wsEnabled = web["ws_enabled"];
        if (web.containsKey("auth_enabled")) config.authEnabled = web["auth_enabled"];
        if (web.containsKey("username")) strlcpy(config.webUser, web["username"], 32);
        if (web.containsKey("password")) strlcpy(config.webPass, web["password"], 32);
    }
    
    dirty = true;
    return true;
}

void ConfigManager::set(const Config& newConfig) {
    config = newConfig;
    dirty = true;
}