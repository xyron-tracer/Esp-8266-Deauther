#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "defines.h"

// ❌ TIDAK PERLU FORWARD DECLARATION AttackStats LAGI
// ❌ TIDAK PERLU DEFINISI WiFiNetwork LAGI (SUDAH ADA DI defines.h)

struct Config {
    // Header
    uint8_t version = CONFIG_VERSION;
    uint8_t checksum = 0;
    
    // AP Settings
    char apSSID[MAX_SSID_LENGTH] = DEFAULT_AP_SSID;
    char apPassword[MAX_PASS_LENGTH] = DEFAULT_AP_PASSWORD;
    uint8_t apChannel = DEFAULT_CHANNEL;
    bool apHidden = DEFAULT_AP_HIDDEN;
    uint8_t apMaxConn = DEFAULT_AP_MAX_CONN;
    
    // Attack Settings
    uint8_t attackMode = MODE_DEAUTH;
    uint8_t targetBSSID[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t targetChannel = DEFAULT_CHANNEL;
    bool attackAll = true;
    int deauthRate = DEFAULT_DEAUTH_RATE;
    int beaconRate = DEFAULT_BEACON_RATE;
    int probeRate = DEFAULT_PROBE_RATE;
    
    // Stealth Settings
    uint8_t stealthLevel = STEALTH_LEVEL_DEFAULT;
    bool randomMac = true;
    bool channelHop = true;
    bool beaconSpam = true;
    bool probeFlood = false;
    bool packetInjection = true;
    int hopInterval = CHANNEL_HOP_DEFAULT;
    int macChangeInterval = MAC_CHANGE_DEFAULT;
    uint8_t txPower = POWER_HIGH;
    
    // Rogue AP Settings
    bool rogueAP = false;
    char rogueSSID[MAX_SSID_LENGTH] = "Free_Public_WiFi";
    char roguePassword[MAX_PASS_LENGTH] = "password123";
    
    // Web Interface
    bool webEnabled = true;
    bool wsEnabled = true;
    bool authEnabled = false;
    char webUser[32] = "admin";
    char webPass[32] = "admin";
    
    // Saved Networks - ✅ MENGGUNAKAN WiFiNetwork dari defines.h
    WiFiNetwork savedNetworks[10];
    uint8_t networkCount = 0;
    
    // System
    uint32_t packetCount = 0;
    uint32_t attackTime = 0;
    uint32_t uptime = 0;
    
    // Validation
    bool isValid() {
        return version == CONFIG_VERSION;
    }
    
    void updateChecksum() {
        checksum = 0;
        uint8_t* data = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(Config) - 1; i++) {
            checksum ^= data[i];
        }
    }
    
    bool verifyChecksum() {
        uint8_t oldChecksum = checksum;
        updateChecksum();
        return oldChecksum == checksum;
    }
};

class ConfigManager {
public:
    ConfigManager();
    
    bool begin();
    bool load();
    bool save(bool force = false);
    bool reset();
    bool backup();
    bool restore();
    
    Config& get() { return config; }
    void set(const Config& newConfig);
    
    // Network management
    bool addNetwork(const WiFiNetwork& network); // ✅ WiFiNetwork dari defines.h
    bool removeNetwork(uint8_t index);
    bool updateNetwork(uint8_t index, const WiFiNetwork& network);
    WiFiNetwork* getNetwork(uint8_t index);
    uint8_t getNetworkCount() { return config.networkCount; }
    
    // Stats
    void updateStats(const AttackStats& stats); // ✅ AttackStats dari defines.h
    void incrementPacketCount(uint32_t count = 1);
    
    // Utility
    void print();
    void printNetworks();
    String toJSON();
    bool fromJSON(const String& json);
    
private:
    Config config;
    bool initialized = false;
    bool dirty = false;
    
    bool validate();
    void setDefaults();
};

#endif