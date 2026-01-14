#ifndef XYRON_DEFINES_H
#define XYRON_DEFINES_H

// ==================== HARDWARE DEFINITIONS ====================
#ifdef ESP8266
    // AUTO-DETECT BOARD TYPE
    #if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(ARDUINO_ESP8266_D1_MINI)
        // LOLIN D1 Mini Configuration
        #define LED_PIN            2      // Blue LED on D1 Mini (D4/GPIO2) - Active LOW
        #define LED_ON             LOW
        #define LED_OFF            HIGH
        #define BOARD_NAME         "LOLIN D1 Mini"
    #else
        // Default NodeMCU Configuration
        #define LED_PIN            16     // NodeMCU built-in LED (D0/GPIO16) - Active LOW
        #define LED_ON             LOW
        #define LED_OFF            HIGH
        #define BOARD_NAME         "NodeMCU"
    #endif
    
    #define BUTTON_PIN         0      // FLASH button (GPIO0)
    
#elif ESP32
    #define LED_PIN            2      // ESP32 built-in LED
    #define BUTTON_PIN         0      // BOOT button
    #define LED_ON             HIGH
    #define LED_OFF            LOW
    #define BOARD_NAME         "ESP32"
#endif
// ============================================
// XYRON TRACER - ESP DEAUTH PRO v5.0
// DEFINES & CONFIGURATION - REVISED
// ============================================

// ==================== FIRMWARE INFO ====================
#define FIRMWARE_NAME          "XYRON TRACER ESP-DEAUTH PRO"
#define FIRMWARE_VERSION       "v5.0"
#define FIRMWARE_AUTHOR        "SHADOW TRACER VIP"
#define FIRMWARE_BUILD_DATE    __DATE__ " " __TIME__

// ==================== HARDWARE DEFINITIONS ====================
#ifdef ESP8266
    #define LED_PIN            2      // NodeMCU built-in LED (active LOW)
    #define BUTTON_PIN         0      // FLASH button
    #define LED_ON             LOW
    #define LED_OFF            HIGH
#elif ESP32
    #define LED_PIN            2      // ESP32 built-in LED
    #define BUTTON_PIN         0      // BOOT button
    #define LED_ON             HIGH
    #define LED_OFF            LOW
#endif

// ==================== NETWORK CONFIG ====================
#define DEFAULT_AP_SSID        "XYRON-TRACER-PRO"
#define DEFAULT_AP_PASSWORD    "87654321"
#define DEFAULT_AP_CHANNEL     1
#define DEFAULT_AP_HIDDEN      false
#define DEFAULT_AP_MAX_CONN    8

#define WEB_SERVER_PORT        80
#define WEB_SOCKET_PORT        81
#define TELNET_PORT            23
#define MDNS_HOSTNAME          "xyron-tracer"

#define DEFAULT_IP             IPAddress(192, 168, 4, 1)
#define DEFAULT_GATEWAY        IPAddress(192, 168, 4, 1)
#define DEFAULT_SUBNET         IPAddress(255, 255, 255, 0)

// ==================== EEPROM & STORAGE ====================
#define EEPROM_SIZE            4096
#define CONFIG_VERSION         0x55
#define CONFIG_ADDRESS         0
#define MAX_SSID_LENGTH        32
#define MAX_PASS_LENGTH        64

// ==================== ATTACK PARAMETERS ====================
#define MIN_DEAUTH_RATE        1
#define MAX_DEAUTH_RATE        1000
#define DEFAULT_DEAUTH_RATE    150

#define MIN_BEACON_RATE        0
#define MAX_BEACON_RATE        200
#define DEFAULT_BEACON_RATE    30

#define MIN_PROBE_RATE         0
#define MAX_PROBE_RATE         100
#define DEFAULT_PROBE_RATE     15

#define MIN_CHANNEL            1
#define MAX_CHANNEL            13
#define DEFAULT_CHANNEL        6

#define CHANNEL_HOP_MIN        100    // ms
#define CHANNEL_HOP_MAX        10000  // ms
#define CHANNEL_HOP_DEFAULT    1500   // ms

#define MAC_CHANGE_MIN         10000  // ms
#define MAC_CHANGE_MAX         300000 // ms
#define MAC_CHANGE_DEFAULT     60000  // ms

// ==================== PACKET DEFINITIONS ====================
#define DEAUTH_PACKET_SIZE     26
#define DISASSOC_PACKET_SIZE   26
#define BEACON_PACKET_SIZE     128
#define PROBE_PACKET_SIZE      128
#define AUTH_PACKET_SIZE       36
#define ASSOC_PACKET_SIZE      36

#define REASON_CODE_DEAUTH     0x07
#define REASON_CODE_DISASSOC   0x01

// ==================== TIMING & INTERVALS ====================
#define STATS_UPDATE_INTERVAL  1000   // ms
#define CONFIG_SAVE_INTERVAL   30000  // ms
#define HEARTBEAT_INTERVAL     5000   // ms
#define SCAN_INTERVAL          30000  // ms

#define WATCHDOG_TIMEOUT       30000  // ms
#define RECONNECT_INTERVAL     10000  // ms

// ==================== SECURITY & STEALTH ====================
#define MAX_MAC_CHANGES         1000
#define MAX_CHANNEL_HOPS        5000
#define MAX_PACKETS_PER_SEC     1000
#define STEALTH_LEVEL_MIN       0
#define STEALTH_LEVEL_MAX       3
#define STEALTH_LEVEL_DEFAULT   2

// Stealth levels:
// 0 = No stealth (fastest)
// 1 = Basic (MAC rotation only)
// 2 = Advanced (MAC + channel hop)
// 3 = Maximum (full stealth, slower)

// ==================== SERIAL CONFIG ====================
#define SERIAL_BAUD            115200
#define SERIAL_TIMEOUT         1000

// ==================== DEBUG & LOGGING ====================
#ifdef XYRON_DEBUG
    #define DEBUG_PRINT(x)     Serial.print(x)
    #define DEBUG_PRINTLN(x)   Serial.println(x)
    #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#define LOG_ERROR              0
#define LOG_WARNING            1
#define LOG_INFO               2
#define LOG_DEBUG              3
#define LOG_VERBOSE            4

#define DEFAULT_LOG_LEVEL      LOG_INFO

// ==================== ERROR CODES ====================
#define ERR_SUCCESS            0
#define ERR_CONFIG_FAIL        1
#define ERR_WIFI_FAIL          2
#define ERR_WEB_FAIL           3
#define ERR_ATTACK_FAIL        4
#define ERR_MEMORY_FAIL        5
#define ERR_SD_FAIL            6
#define ERR_UNKNOWN            255

// ==================== ENUMS ====================
enum AttackMode {
    MODE_IDLE = 0,
    MODE_DEAUTH,
    MODE_BEACON_SPAM,
    MODE_PROBE_FLOOD,
    MODE_ROGUE_AP,
    MODE_MIXED,
    MODE_CUSTOM
};

enum StealthMode {
    STEALTH_OFF = 0,
    STEALTH_LOW,
    STEALTH_MEDIUM,
    STEALTH_HIGH,
    STEALTH_EXTREME
};

enum PowerLevel {
    POWER_MIN = 0,      // 0 dBm
    POWER_LOW,          // 5 dBm
    POWER_MEDIUM,       // 10 dBm
    POWER_HIGH,         // 15 dBm
    POWER_MAX           // 20 dBm
};

// ==================== STRUCTURES ====================
struct WiFiNetwork {
    uint8_t bssid[6];
    char ssid[MAX_SSID_LENGTH];
    int32_t rssi;
    uint8_t channel;
    uint8_t encryption;
    bool selected;
    unsigned long lastSeen;  // ✅ DITAMBAHKAN - Untuk config_manager
};

struct AttackStats {
    uint32_t total_packets;
    uint32_t deauth_packets;
    uint32_t beacon_packets;
    uint32_t probe_packets;
    uint32_t duration_ms;
    uint32_t target_count;
    uint8_t current_channel;
    float packets_per_second;
    uint32_t start_time;
    bool is_active;
    bool is_paused;
};

// ==================== MACROS ====================
#define ARRAY_SIZE(x)          (sizeof(x) / sizeof(x[0]))
#define MIN(a,b)               (((a)<(b))?(a):(b))
#define MAX(a,b)               (((a)>(b))?(a):(b))
#define CONSTRAIN(x,a,b)       (((x)<(a))?(a):(((x)>(b))?(b):(x)))

#define BIT_SET(var, bit)      ((var) |= (1 << (bit)))
#define BIT_CLEAR(var, bit)    ((var) &= ~(1 << (bit)))
#define BIT_TOGGLE(var, bit)   ((var) ^= (1 << (bit)))
#define BIT_CHECK(var, bit)    ((var) & (1 << (bit)))

// ==================== GLOBAL FLAGS ====================
// System flags
extern volatile bool SYSTEM_RUNNING;
extern volatile bool ATTACK_ACTIVE;
extern volatile bool CONFIG_DIRTY;
extern volatile bool SCAN_REQUESTED;

// Feature flags
extern bool FEATURE_WEB_ENABLED;
extern bool FEATURE_WS_ENABLED;
extern bool FEATURE_TELNET_ENABLED;
extern bool FEATURE_MDNS_ENABLED;
extern bool FEATURE_OTA_ENABLED;
extern bool FEATURE_SD_ENABLED;

#endif // XYRON_DEFINES_H