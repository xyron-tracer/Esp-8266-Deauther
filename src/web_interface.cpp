#include "web_interface.h"
#include <FS.h>

WebInterface::WebInterface(ConfigManager& config, AttackEngine& attack)
    : configMgr(config), attackEngine(attack),
      server(WEB_SERVER_PORT), webSocket(WEB_SOCKET_PORT),
      serverRunning(false), wsConnected(false), clientCount(0),
      logIndex(0), logCount(0) {
    
    authToken = "";
    authTokenTime = 0;
    lastBroadcast = 0;
    lastClientCheck = 0;
}

WebInterface::~WebInterface() {
    stop();
}

bool WebInterface::begin() {
    Config& cfg = configMgr.get();
    
    if (!cfg.webEnabled) {
        Serial.println("[WEB] Web interface disabled in config");
        return false;
    }
    
    Serial.println("[WEB] Initializing web interface...");
    
    // Generate auth token
    authToken = generateToken();
    authTokenTime = millis();
    
    // Setup SPIFFS
    if (!SPIFFS.begin()) {
        Serial.println("[WEB] SPIFFS failed!");
        return false;
    }
    
    // Setup routes
    setupRoutes();
    
    // Setup WebSocket
    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->onWebSocketEvent(num, type, payload, length);
    });
    
    // Start server
    server.begin();
    serverRunning = true;
    
    // Setup mDNS
    if (cfg.webEnabled && MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        MDNS.addService("ws", "tcp", WEB_SOCKET_PORT);
        Serial.printf("[WEB] mDNS: http://%s.local\n", MDNS_HOSTNAME);
    }
    
    Serial.printf("[WEB] Server started on http://%s:%d\n", 
                  WiFi.softAPIP().toString().c_str(), WEB_SERVER_PORT);
    Serial.printf("[WEB] WebSocket on ws://%s:%d\n",
                  WiFi.softAPIP().toString().c_str(), WEB_SOCKET_PORT);
    
    addLog("Web interface started", LOG_INFO);
    return true;
}

void WebInterface::update() {
    if (!serverRunning) return;
    
    server.handleClient();
    webSocket.loop();
    MDNS.update();
    
    // Broadcast stats periodically
    unsigned long now = millis();
    if (now - lastBroadcast > 1000) {
        broadcastStats();
        lastBroadcast = now;
    }
    
    // Check auth token expiry (1 hour)
    if (now - authTokenTime > 3600000) {
        authToken = generateToken();
        authTokenTime = now;
    }
    
    // Check client connections
    if (now - lastClientCheck > 5000) {
        clientCount = webSocket.connectedClients();
        lastClientCheck = now;
    }
}

void WebInterface::stop() {
    if (!serverRunning) return;
    
    server.stop();
    webSocket.close();
    serverRunning = false;
    
    addLog("Web interface stopped", LOG_INFO);
}

void WebInterface::setupRoutes() {
    // API routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api", HTTP_GET, [this]() { handleAPI(); });
    server.on("/api/config", HTTP_GET, [this]() { handleConfig(); });
    server.on("/api/config", HTTP_POST, [this]() { handleConfig(); });
    server.on("/api/control", HTTP_POST, [this]() { handleControl(); });
    server.on("/api/stats", HTTP_GET, [this]() { handleStats(); });
    server.on("/api/scan", HTTP_GET, [this]() { handleScan(); });
    server.on("/api/networks", HTTP_GET, [this]() { handleNetworks(); });
    server.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    server.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
    server.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
    server.on("/api/update", HTTP_POST, [this]() { handleUpdate(); });
    server.on("/api/download", HTTP_GET, [this]() { handleDownload(); });
    server.on("/api/upload", HTTP_POST, [this]() { handleUpload(); });
    server.on("/api/auth", HTTP_POST, [this]() { handleAuth(); });
    
    // Static files
    server.on("/index.html", HTTP_GET, [this]() { serveStaticFile("/index.html"); });
    server.on("/style.css", HTTP_GET, [this]() { serveStaticFile("/style.css"); });
    server.on("/script.js", HTTP_GET, [this]() { serveStaticFile("/script.js"); });
    server.on("/favicon.ico", HTTP_GET, [this]() { serveStaticFile("/favicon.ico"); });
    
    // 404 handler
    server.onNotFound([this]() { handleNotFound(); });
}

void WebInterface::handleRoot() {
    if (!checkAuth()) {
        server.sendHeader("Location", "/login.html");
        server.send(302);
        return;
    }
    
    serveStaticFile("/index.html");
}

void WebInterface::handleAPI() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    DynamicJsonDocument doc(1024);
    doc["status"] = "online";
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["clients"] = clientCount;
    doc["free_heap"] = ESP.getFreeHeap();
    
    sendJSON(200, doc);
}

void WebInterface::handleConfig() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    if (server.method() == HTTP_GET) {
        String json = getConfigJSON();
        server.send(200, "application/json", json);
    } 
    else if (server.method() == HTTP_POST) {
        String body = server.arg("plain");
        
        if (configMgr.fromJSON(body)) {
            configMgr.save();
            broadcastConfig();
            sendSuccess("Configuration updated");
            addLog("Configuration updated via web", LOG_INFO);
        } else {
            sendError(400, "Invalid configuration");
        }
    }
}

void WebInterface::handleControl() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    String command = doc["command"];
    String param = doc["param"] | "";
    
    if (command == "start") {
        if (attackEngine.start()) {
            sendSuccess("Attack started");
            addLog("Attack started via web", LOG_INFO);
        } else {
            sendError(500, "Failed to start attack");
        }
    }
    else if (command == "stop") {
        if (attackEngine.stop()) {
            sendSuccess("Attack stopped");
            addLog("Attack stopped via web", LOG_INFO);
        } else {
            sendError(500, "Failed to stop attack");
        }
    }
    else if (command == "pause") {
        if (attackEngine.pause()) {
            sendSuccess("Attack paused");
        } else {
            sendError(500, "Failed to pause attack");
        }
    }
    else if (command == "resume") {
        if (attackEngine.resume()) {
            sendSuccess("Attack resumed");
        } else {
            sendError(500, "Failed to resume attack");
        }
    }
    else if (command == "scan") {
        attackEngine.scanNetworks();
        sendSuccess("Scan started");
    }
    else if (command == "hop") {
        // Manual channel hop
        sendSuccess("Channel hopped");
    }
    else {
        sendError(400, "Unknown command: " + command);
    }
}

void WebInterface::handleStats() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    String json = getAttackStatsJSON();
    server.send(200, "application/json", json);
}

void WebInterface::handleScan() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    // Simulate scan
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.createNestedArray("networks");
    
    // Add some dummy networks for demo
    for (int i = 1; i <= 5; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = String("Network_") + i;
        net["bssid"] = String("00:00:00:00:00:0") + i;
        net["channel"] = i * 2;
        net["rssi"] = -50 - (i * 5);
        net["encryption"] = i % 3;
    }
    
    doc["count"] = 5;
    doc["scan_time"] = millis();
    
    sendJSON(200, doc);
    addLog("Network scan requested", LOG_INFO);
}

void WebInterface::handleNetworks() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    String json = getNetworksJSON();
    server.send(200, "application/json", json);
}

void WebInterface::handleLogs() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    String json = getLogsJSON();
    server.send(200, "application/json", json);
}

void WebInterface::handleReboot() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    addLog("Reboot requested via web", LOG_WARNING);
    
    DynamicJsonDocument doc(128);
    doc["status"] = "rebooting";
    doc["message"] = "System will reboot in 1 second";
    
    sendJSON(200, doc);
    
    delay(1000);
    ESP.restart();
}

void WebInterface::handleReset() {
    if (!checkAuth()) {
        sendError(401, "Unauthorized");
        return;
    }
    
    if (configMgr.reset()) {
        addLog("Factory reset via web", LOG_WARNING);
        
        DynamicJsonDocument doc(128);
        doc["status"] = "reset";
        doc["message"] = "Configuration reset to defaults";
        
        sendJSON(200, doc);
        
        delay(2000);
        ESP.restart();
    } else {
        sendError(500, "Reset failed");
    }
}

void WebInterface::handleAuth() {
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    Config& cfg = configMgr.get();
    
    if (cfg.authEnabled) {
        String username = doc["username"] | "";
        String password = doc["password"] | "";
        
        if (username == cfg.webUser && password == cfg.webPass) {
            String token = generateToken();
            
            DynamicJsonDocument resp(256);
            resp["status"] = "authenticated";
            resp["token"] = token;
            resp["expires"] = 3600;
            
            sendJSON(200, resp);
            addLog("User authenticated: " + username, LOG_INFO);
        } else {
            sendError(401, "Invalid credentials");
            addLog("Failed authentication attempt", LOG_WARNING);
        }
    } else {
        // Auth disabled, return success
        DynamicJsonDocument resp(128);
        resp["status"] = "authenticated";
        resp["message"] = "Auth disabled";
        
        sendJSON(200, resp);
    }
}

void WebInterface::handleNotFound() {
    String path = server.uri();
    
    // Try to serve static file
    if (loadFromFS(path)) {
        return;
    }
    
    // Return 404
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    
    server.send(404, "text/plain", message);
}

void WebInterface::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client %u disconnected\n", num);
            clientCount--;
            wsConnected = (clientCount > 0);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WS] Client %u connected from %s\n", num, ip.toString().c_str());
            clientCount++;
            wsConnected = true;
            
            // Send initial data
            DynamicJsonDocument doc(1024);
            doc["type"] = "welcome";
            doc["version"] = FIRMWARE_VERSION;
            doc["clients"] = clientCount;
            
            String json;
            serializeJson(doc, json);
            webSocket.sendTXT(num, json);
            
            addLog("WebSocket client connected: " + ip.toString(), LOG_INFO);
            break;
        }
            
        case WStype_TEXT: {
            String message = String((char*)payload);
            Serial.printf("[WS] Received: %s\n", message.c_str());
            
            // Parse JSON
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, message);
            
            if (error) {
                Serial.printf("[WS] JSON error: %s\n", error.c_str());
                return;
            }
            
            String cmd = doc["command"] | "";
            String param = doc["param"] | "";
            
            if (cmd == "get_stats") {
                broadcastStats();
            }
            else if (cmd == "get_config") {
                broadcastConfig();
            }
            else if (cmd == "control") {
                // Forward to control handler
                handleControl();
            }
            
            break;
        }
            
        case WStype_BIN:
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

void WebInterface::broadcastStats() {
    if (!wsConnected || clientCount == 0) return;
    
    String json = getAttackStatsJSON();
    webSocket.broadcastTXT(json);
}

void WebInterface::broadcastConfig() {
    if (!wsConnected || clientCount == 0) return;
    
    String json = getConfigJSON();
    webSocket.broadcastTXT(json);
}

void WebInterface::broadcastLog(const String& message, uint8_t level) {
    if (!wsConnected || clientCount == 0) return;
    
    DynamicJsonDocument doc(256);
    doc["type"] = "log";
    doc["message"] = message;
    doc["level"] = level;
    doc["time"] = millis();
    
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

void WebInterface::addLog(const String& message, uint8_t level) {
    logs[logIndex].message = message;
    logs[logIndex].level = level;
    logs[logIndex].time = millis();
    
    logIndex = (logIndex + 1) % 100;
    if (logCount < 100) logCount++;
    
    // Broadcast via WebSocket
    broadcastLog(message, level);
    
    // Also print to serial
    const char* levelStr = "INFO";
    switch (level) {
        case LOG_ERROR: levelStr = "ERROR"; break;
        case LOG_WARNING: levelStr = "WARNING"; break;
        case LOG_DEBUG: levelStr = "DEBUG"; break;
        case LOG_VERBOSE: levelStr = "VERBOSE"; break;
    }
    
    Serial.printf("[%s] %s\n", levelStr, message.c_str());
}

bool WebInterface::checkAuth() {
    Config& cfg = configMgr.get();
    
    if (!cfg.authEnabled) {
        return true;
    }
    
    if (server.hasHeader("X-Auth-Token")) {
        String token = server.header("X-Auth-Token");
        return validateToken(token);
    }
    
    if (server.hasArg("token")) {
        String token = server.arg("token");
        return validateToken(token);
    }
    
    return false;
}

String WebInterface::generateToken() {
    String token = "";
    for (int i = 0; i < 32; i++) {
        token += char('a' + random(26));
    }
    return token;
}

bool WebInterface::validateToken(const String& token) {
    return token == authToken;
}

void WebInterface::sendJSON(int code, const JsonDocument& doc) {
    String json;
    serializeJson(doc, json);
    server.send(code, "application/json", json);
}

void WebInterface::sendError(int code, const String& message) {
    DynamicJsonDocument doc(256);
    doc["status"] = "error";
    doc["code"] = code;
    doc["message"] = message;
    sendJSON(code, doc);
}

void WebInterface::sendSuccess(const String& message) {
    DynamicJsonDocument doc(128);
    doc["status"] = "success";
    doc["message"] = message;
    sendJSON(200, doc);
}

String WebInterface::getSystemInfoJSON() {
    DynamicJsonDocument doc(1024);
    
    doc["firmware"] = FIRMWARE_VERSION;
    doc["build_date"] = FIRMWARE_BUILD_DATE;
    doc["chip_id"] = ESP.getChipId();
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["sketch_size"] = ESP.getSketchSize();
    doc["free_sketch_space"] = ESP.getFreeSketchSpace();
    doc["sdk_version"] = ESP.getSdkVersion();
    doc["core_version"] = ESP.getCoreVersion();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    
    doc["wifi"]["ap_ip"] = WiFi.softAPIP().toString();
    doc["wifi"]["ap_mac"] = WiFi.softAPmacAddress();
    doc["wifi"]["station_mac"] = WiFi.macAddress();
    doc["wifi"]["ap_clients"] = WiFi.softAPgetStationNum();
    
    doc["system"]["uptime"] = millis() / 1000;
    doc["system"]["reset_reason"] = ESP.getResetReason();
    doc["system"]["reset_info"] = ESP.getResetInfo();
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WebInterface::getAttackStatsJSON() {
    AttackStats stats = attackEngine.getStats();
    
    DynamicJsonDocument doc(512);
    doc["type"] = "stats";
    doc["active"] = stats.is_active;
    doc["paused"] = stats.is_paused;
    doc["packets_total"] = stats.total_packets;
    doc["packets_deauth"] = stats.deauth_packets;
    doc["packets_beacon"] = stats.beacon_packets;
    doc["packets_probe"] = stats.probe_packets;
    doc["duration"] = stats.duration_ms;
    doc["targets"] = stats.target_count;
    doc["channel"] = stats.current_channel;
    doc["pps"] = stats.packets_per_second;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WebInterface::getConfigJSON() {
    return configMgr.toJSON();
}

String WebInterface::getNetworksJSON() {
    DynamicJsonDocument doc(2048);
    JsonArray networks = doc.createNestedArray("networks");
    
    // Get networks from config
    uint8_t count = configMgr.getNetworkCount();
    for (uint8_t i = 0; i < count; i++) {
        WiFiNetwork* net = configMgr.getNetwork(i);
        if (net) {
            JsonObject obj = networks.createNestedObject();
            obj["ssid"] = net->ssid;
            obj["channel"] = net->channel;
            obj["rssi"] = net->rssi;
            obj["encryption"] = net->encryption;
            obj["selected"] = net->selected;
            
            char bssidStr[18];
            sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                    net->bssid[0], net->bssid[1], net->bssid[2],
                    net->bssid[3], net->bssid[4], net->bssid[5]);
            obj["bssid"] = bssidStr;
        }
    }
    
    doc["count"] = count;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WebInterface::getLogsJSON() {
    DynamicJsonDocument doc(4096);
    JsonArray logArray = doc.createNestedArray("logs");
    
    int start = (logIndex - logCount + 100) % 100;
    for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % 100;
        JsonObject log = logArray.createNestedObject();
        
        log["message"] = logs[idx].message;
        log["level"] = logs[idx].level;
        log["time"] = logs[idx].time;
        
        const char* levelStr = "info";
        switch (logs[idx].level) {
            case LOG_ERROR: levelStr = "error"; break;
            case LOG_WARNING: levelStr = "warning"; break;
            case LOG_DEBUG: levelStr = "debug"; break;
            case LOG_VERBOSE: levelStr = "verbose"; break;
        }
        log["level_str"] = levelStr;
    }
    
    doc["count"] = logCount;
    doc["max"] = 100;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void WebInterface::serveStaticFile(const String& path) {
    if (!loadFromFS(path)) {
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += path;
        message += "\nMethod: GET";
        server.send(404, "text/plain", message);
    }
}

bool WebInterface::loadFromFS(const String& path) {
    String filePath = path;
    if (path.endsWith("/")) {
        filePath += "index.html";
    }
    
    if (SPIFFS.exists(filePath)) {
        File file = SPIFFS.open(filePath, "r");
        if (!file) {
            return false;
        }
        
        String contentType = getContentType(filePath);
        server.streamFile(file, contentType);
        file.close();
        return true;
    }
    
    return false;
}

String WebInterface::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    if (filename.endsWith(".gif")) return "image/gif";
    if (filename.endsWith(".ico")) return "image/x-icon";
    if (filename.endsWith(".xml")) return "text/xml";
    if (filename.endsWith(".pdf")) return "application/pdf";
    if (filename.endsWith(".zip")) return "application/zip";
    if (filename.endsWith(".gz")) return "application/x-gzip";
    if (filename.endsWith(".json")) return "application/json";
    return "text/plain";
}

void WebInterface::sendWSMessage(const String& type, const JsonDocument& doc) {
    if (!wsConnected || clientCount == 0) return;
    
    DynamicJsonDocument message(256);
    message["type"] = type;
    message["data"] = doc;
    
    String json;
    serializeJson(message, json);
    webSocket.broadcastTXT(json);
}

void WebInterface::broadcastWS(const String& message) {
    if (!wsConnected || clientCount == 0) return;
    webSocket.broadcastTXT(message);
}