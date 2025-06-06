# CMakeLists.txt for ESP-IDF SIP Client Component
# This file defines the component dependencies and source files

idf_component_register(
    SRCS "main.c"                    # Source files
    INCLUDE_DIRS "."                 # Include directories
    REQUIRES 
        driver                       # GPIO driver
        esp_common                   # Common ESP functions
        esp_wifi                     # WiFi functionality
        esp_netif                    # Network interface
        esp_event                    # Event handling
        nvs_flash                    # NVS storage
        lwip                         # TCP/IP stack (for sockets)
        esp_system                   # System functions
        console                      # Console support
)