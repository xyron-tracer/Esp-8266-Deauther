#ifndef STEALTH_MANAGER_H
#define STEALTH_MANAGER_H

#include <Arduino.h>
#include <Ticker.h>
#include "defines.h"
#include "config_manager.h"

class StealthManager {
public:
    StealthManager(ConfigManager& config);
    ~StealthManager();
    
    bool begin();
    void update();
    void stop();
    
    // Stealth control
    void setLevel(uint8_t level);
    uint8_t getLevel() const { return currentLevel; }
    
    // MAC management
    void rotateMAC();
    String getCurrentMAC() const;
    
    // Channel management
    void hopChannel();
    uint8_t getCurrentChannel() const { return currentChannel; }
    
    // TX Power control
    void setTXPower(uint8_t level);
    uint8_t getTXPower() const { return currentTXPower; }
    
    // Packet timing
    void setJitter(uint16_t jitter);
    uint16_t getJitter() const { return packetJitter; }
    
    // Anti-detection
    void enableAntiDetection(bool enable);
    bool isAntiDetectionEnabled() const { return antiDetection; }
    
    // Statistics
    struct StealthStats {
        uint32_t macChanges;
        uint32_t channelHops;
        uint32_t txPowerChanges;
        uint32_t packetsSent;
        uint32_t detectionAttempts;
        uint32_t evasionSuccess;
        uint8_t currentLevel;
        bool isActive;
    };
    
    StealthStats getStats() const;
    
private:
    ConfigManager& config;
    
    // State
    volatile bool active;
    uint8_t currentLevel;
    uint8_t currentChannel;
    uint8_t currentTXPower;
    uint16_t packetJitter;
    bool antiDetection;
    
    // Current MAC
    uint8_t currentMAC[6];
    
    // Statistics
    StealthStats stats;
    
    // Timers
    Ticker macTimer;
    Ticker channelTimer;
    Ticker powerTimer;
    Ticker patternTimer;
    
    // Internal methods
    void applyStealthLevel();
    void generateRandomMAC(uint8_t* mac);
    uint8_t getRandomChannel();
    uint8_t getRandomTXPower();
    
    void updateMAC();
    void updateChannel();
    void updateTXPower();
    void updatePattern();
    
    void detectEnvironment();
    void evadeDetection();
    
    // Pattern generation
    void generatePacketPattern();
    void generateTimingPattern();
    
    // Debug
    void printDebugInfo();
};

#endif