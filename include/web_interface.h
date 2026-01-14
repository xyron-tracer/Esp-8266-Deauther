#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "defines.h"
#include "config_manager.h"
#include "attack_engine.h"

class WebInterface {
public:
    WebInterface(ConfigManager& config, AttackEngine& attack);
    ~WebInterface();
    
    bool begin();
    void update();
    void stop();
    
    // WebSocket
    void sendWSMessage(const String& type, const JsonDocument& doc);
    void broadcastWS(const String& message);
    
    // Server control
    void restartServer();
    bool isRunning() { return serverRunning; }
    
    // Authentication
    bool authenticate();
    String getAuthToken();
    
private:
    ConfigManager& configMgr;
    AttackEngine& attackEngine;
    
    ESP8266WebServer server;
    WebSocketsServer webSocket;
    
    bool serverRunning;
    bool wsConnected;
    unsigned long lastBroadcast;
    unsigned long lastClientCheck;
    uint8_t clientCount;
    
    String authToken;
    unsigned long authTokenTime;
    
    // Route handlers
    void handleRoot();
    void handleAPI();
    void handleConfig();
    void handleControl();
    void handleStats();
    void handleScan();
    void handleNetworks();
    void handleLogs();
    void handleReboot();
    void handleReset();
    void handleUpdate();
    void handleDownload();
    void handleUpload();
    void handleAuth();
    void handleNotFound();
    
    // WebSocket event handler
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // Helper functions
    void setupRoutes();
    void serveStaticFile(const String& path);
    String getContentType(const String& filename);
    String getSystemInfoJSON();
    String getAttackStatsJSON();
    String getConfigJSON();
    String getNetworksJSON();
    String getLogsJSON();
    
    // File operations
    bool loadFromFS(const String& path);
    bool saveToFS(const String& path, const String& data);
    
    // Authentication
    bool checkAuth();
    String generateToken();
    bool validateToken(const String& token);
    
    // Response helpers
    void sendJSON(int code, const JsonDocument& doc);
    void sendError(int code, const String& message);
    void sendSuccess(const String& message = "OK");
    
    // Broadcast updates
    void broadcastStats();
    void broadcastConfig();
    void broadcastLog(const String& message, uint8_t level = LOG_INFO);
    
    // Logging
    void addLog(const String& message, uint8_t level = LOG_INFO);
    struct LogEntry {
        String message;
        uint8_t level;
        unsigned long time;
    };
    
    LogEntry logs[100];
    uint8_t logIndex;
    uint8_t logCount;
};

#endif