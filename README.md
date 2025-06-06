# ESP32-S3 SIP Client - main.c Documentation

## Overview

The `main.c` file implements a simple SIP (Session Initiation Protocol) client for ESP32-S3 that tests connectivity to SIP servers. It connects to WiFi, sends SIP OPTIONS requests, and displays server responses.

## What It Does

1. **Connects to WiFi** using configured credentials
2. **Tests basic network connectivity** (IP, DNS)
3. **Sends SIP OPTIONS** to opensips.org:5060
4. **Waits 30 seconds** for server response
5. **Falls back to SIP REGISTER** if OPTIONS fails
6. **Repeats every 30 seconds** in a continuous loop

## How It Works

### 1. Configuration Section (Lines 27-37)
```c
// WiFi Configuration
#define WIFI_SSID      "Pixel 7"        // Your WiFi network name
#define WIFI_PASS      "68986898"       // Your WiFi password

// SIP Server Configuration  
#define SIP_SERVER      "opensips.org"  // Target SIP server
#define SIP_PORT        5060             // SIP port (UDP)
#define SIP_USER        "test"           // Test username
#define SIP_TIMEOUT     30               // Response timeout
```

### 2. Main Program Flow
```
app_main() → WiFi Init → Network Tests → SIP Testing Loop
```

### 3. SIP Message Structure
The code sends standard SIP OPTIONS messages:
- **Request Line**: `OPTIONS sip:opensips.org:5060 SIP/2.0`
- **Headers**: Via, From, To, Call-ID, CSeq, etc.
- **Body**: Empty (Content-Length: 0)

### 4. Key Functions

| Function | Purpose |
|----------|---------|
| `app_main()` | Entry point, WiFi setup |
| `sip_client_task()` | Main SIP testing loop |
| `send_sip_options()` | Builds and sends OPTIONS request |
| `send_sip_register()` | Builds and sends REGISTER request |
| `handle_sip_response()` | Parses server responses |
| `generate_random_ids()` | Creates unique SIP identifiers |

## Requirements to Make This Work

### 1. Hardware Requirements
- **ESP32-S3 Development Board** (ESP32-S3-DEVKITC-1 N16R8)
- **USB Cable** for programming and power
- **WiFi Network** with internet access

### 2. Software Requirements
- **ESP-IDF v5.3.1** (Espressif IoT Development Framework)
- **Visual Studio Code** with ESP-IDF extension (recommended)
- **Git** (for ESP-IDF installation)

### 3. Network Requirements
- **WiFi Access Point** with internet connectivity
- **UDP port 5060** not blocked by firewall/ISP
- **DNS resolution** working (to resolve opensips.org)
- **No SIP traffic blocking** by network provider

### 4. Configuration Steps

#### Step 1: Update WiFi Credentials
```c
#define WIFI_SSID      "YourWiFiName"     // Change this
#define WIFI_PASS      "YourWiFiPassword" // Change this
```

#### Step 2: Optional - Change SIP Server
```c
#define SIP_SERVER      "your.sip.server.com"  // Change if needed
#define SIP_PORT        5060                    // Change if needed
```

### 5. Build and Flash Commands
```bash
# Navigate to project directory
cd c:\Users\aabdu\workspace\SIPClient

# Build the project
idf.py build

# Flash to ESP32
idf.py flash

# Monitor serial output
idf.py monitor
```

### 6. Expected Output
```
I (xxx) SIP_CLIENT: Connected to WiFi SSID: Pixel 7
I (xxx) SIP_CLIENT: Network connectivity OK
I (xxx) SIP_CLIENT: Sending SIP OPTIONS to opensips.org:5060
I (xxx) SIP_CLIENT: SUCCESS! Response from 136.243.23.236:5060
I (xxx) SIP_CLIENT: SUCCESS: 200 OK - Registration successful!
```

## Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| WiFi connection fails | Check SSID/password in code |
| DNS resolution fails | Check internet connectivity |
| No SIP response | Network may block SIP traffic |
| Build errors | Verify ESP-IDF installation |

### Network Testing
The code includes built-in network diagnostics:
- IP address validation
- DNS resolution test (google.com)
- Detailed error logging

### SIP Server Testing
If opensips.org doesn't respond:
1. **Try different network** (mobile hotspot)
2. **Check firewall settings**
3. **Contact ISP** about SIP restrictions
4. **Use different SIP server** in configuration

## Code Structure Summary

```
main.c (400+ lines)
├── Configuration (WiFi + SIP settings)
├── Global variables (SIP IDs, WiFi state)
├── WiFi functions (connection, event handling)
├── Network functions (connectivity testing)
├── SIP functions (message building, sending)
├── Response handling (parsing, display)
└── Main task loop (continuous testing)
```

This documentation covers everything needed to understand and run the ESP32 SIP client code successfully.
