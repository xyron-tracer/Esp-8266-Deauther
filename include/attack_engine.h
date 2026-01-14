#ifndef ATTACK_ENGINE_H
#define ATTACK_ENGINE_H

#include <Arduino.h>
#include <Ticker.h>
#include "defines.h"
#include "config_manager.h"

// ❌ STRUCT ATTACKSTATS DIHAPUS (SUDAH ADA DI defines.h)

class AttackEngine {
public:
    AttackEngine(ConfigManager& config);
    ~AttackEngine();
    
    bool begin();
    bool start();
    bool stop();
    bool pause();
    bool resume();
    bool restart();
    
    // Target management
    bool setTarget(const uint8_t* bssid, uint8_t channel = 0);
    bool addTarget(const WiFiNetwork& network);
    bool removeTarget(uint8_t index);
    void clearTargets();
    
    // Rate control
    void setRate(uint8_t type, uint32_t rate); // type: 0=deauth, 1=beacon, 2=probe
    void setChannel(uint8_t channel);
    void setStealthLevel(uint8_t level);
    
    // Status
    bool isActive() const { return active; }
    bool isPaused() const { return paused; }
    AttackStats getStats() const; // ✅ MENGGUNAKAN AttackStats dari defines.h
    
    // Control
    void update();
    void scanNetworks();
    void saveTargets();
    
    // Callbacks
    void setPacketCallback(void (*callback)(uint32_t count));
    void setStatusCallback(void (*callback)(bool active));
    
private:
    ConfigManager& config;
    
    // State
    volatile bool active;
    volatile bool paused;
    volatile bool scanning;
    
    // Statistics
    AttackStats stats; // ✅ MENGGUNAKAN AttackStats dari defines.h
    
    // Targets
    WiFiNetwork* targets; // ✅ MENGGUNAKAN WiFiNetwork dari defines.h
    uint8_t targetCount;
    uint8_t maxTargets;
    
    // Timers
    Ticker attackTicker;
    Ticker scanTicker;
    Ticker statsTicker;
    
    // Current settings
    uint8_t currentChannel;
    uint8_t currentMAC[6];
    uint32_t attackStartTime;
    
    // Callbacks
    void (*packetCallback)(uint32_t);
    void (*statusCallback)(bool);
    
    // Internal methods
    void sendDeauth();
    void sendBeacon();
    void sendProbe();
    void sendRogueBeacon();
    
    void hopChannel();
    void rotateMAC();
    void updateStealth();
    
    void updateStatistics();
    void resetStatistics();
    
    bool initHardware();
    bool initTimers();
    void cleanup();
    
    // Packet generation
    void generateDeauthPacket(uint8_t* packet, const uint8_t* dest, 
                             const uint8_t* src, const uint8_t* bssid);
    void generateBeaconPacket(uint8_t* packet, const char* ssid,
                             uint8_t channel, const uint8_t* bssid);
    void generateProbePacket(uint8_t* packet, const char* ssid,
                            const uint8_t* src);
    
    // WiFi functions
    void setupPromiscuous();
    void setChannelInternal(uint8_t channel);
    void setMACInternal(const uint8_t* mac);
    
    // Utilities
    void generateRandomSSID(char* buffer, size_t length);
    void generateRandomMAC(uint8_t* mac);
    uint8_t getRandomChannel();
    
    // Debug
    void printDebugInfo();
};

#endif