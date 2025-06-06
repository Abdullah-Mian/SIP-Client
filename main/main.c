/*
 * main.c - ESP-IDF SIP Connection Test Client
 * Tests SIP server connectivity and registration
 * For ESP32-S3-DEVKITC-1 N16R8
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

// ===============================================================================
// CONFIGURATION SECTION - CHANGE THESE VALUES
// ===============================================================================

// WiFi Configuration
#define WIFI_SSID      "Pixel 7"
#define WIFI_PASS      "68986898"

// SIP Server Configuration
#define SIP_SERVER      "opensips.org"      // Working SIP server
#define SIP_PORT        5060                // Standard SIP port
#define SIP_USER        "test"              // Test username
#define SIP_TIMEOUT     30                  // Response timeout in seconds

// ===============================================================================

// WiFi Event Bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "SIP_CLIENT";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool wifi_connected = false;

// SIP client variables
static char call_id[64];
static char local_tag[32];
static char branch_id[64];
static int cseq = 1;

// Function declarations
static void wifi_init_sta(void);
static void sip_client_task(void *pvParameters);
static int create_udp_socket(void);
static void generate_random_ids(void);
static bool send_sip_options(int sock);
static bool send_sip_register(int sock);
static void handle_sip_response(const char *response);
static bool test_network_connectivity(void);

// WiFi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        wifi_connected = false;
        ESP_LOGI(TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 SIP Connection Test Starting...");
    ESP_LOGI(TAG, "Serial Baud Rate: 115200 (ESP-IDF default)");
    ESP_LOGI(TAG, "Testing from Pakistan with direct IP server");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();
    
    // Wait for WiFi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", WIFI_SSID);
        
        // Generate random IDs for SIP
        generate_random_ids();
        
        // Start SIP client task
        xTaskCreate(sip_client_task, "sip_client", 8192, NULL, 6, NULL);
        
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", WIFI_SSID);
        
        // Restart after WiFi failure
        esp_restart();
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi init finished.");
}

static void generate_random_ids(void)
{
    uint32_t random1 = esp_random();
    uint32_t random2 = esp_random();
    uint32_t random3 = esp_random();
    
    snprintf(call_id, sizeof(call_id), "%08" PRIx32 "%08" PRIx32 "@esp32s3", random1, random2);
    snprintf(local_tag, sizeof(local_tag), "%08" PRIx32, random3);
    snprintf(branch_id, sizeof(branch_id), "z9hG4bK%08" PRIx32, random1);
    
    ESP_LOGI(TAG, "Generated Call-ID: %s", call_id);
    ESP_LOGI(TAG, "Generated Tag: %s", local_tag);
}

static int create_udp_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }
    
    // Set timeout for socket operations
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    ESP_LOGI(TAG, "UDP socket created successfully");
    return sock;
}

static bool test_network_connectivity(void)
{
    ESP_LOGI(TAG, "=== NETWORK CONNECTIVITY TEST ===");
    
    // Get network info
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    
    ESP_LOGI(TAG, "Network Configuration:");
    ESP_LOGI(TAG, "  Local IP: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "  Gateway:  " IPSTR, IP2STR(&ip_info.gw));
    ESP_LOGI(TAG, "  Netmask:  " IPSTR, IP2STR(&ip_info.netmask));
    
    // Test 1: Check if we have a valid IP
    if (ip_info.ip.addr == 0) {
        ESP_LOGE(TAG, "No valid IP address obtained");
        return false;
    }
    
    // Test 2: Test DNS resolution
    ESP_LOGI(TAG, "Testing DNS resolution...");
    struct hostent *he = gethostbyname("google.com");
    bool dns_ok = (he != NULL);
    ESP_LOGI(TAG, "DNS resolution: %s", dns_ok ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "Network Test Results:");
    ESP_LOGI(TAG, "  IP Address: %s", ip_info.ip.addr != 0 ? "OK" : "FAILED");
    ESP_LOGI(TAG, "  DNS Resolution: %s", dns_ok ? "OK" : "FAILED");
    
    return (ip_info.ip.addr != 0) && dns_ok;
}

static bool send_sip_options(int sock)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SIP_PORT);
    
    // Resolve server hostname to IP
    struct hostent *he = gethostbyname(SIP_SERVER);
    if (he == NULL) {
        ESP_LOGE(TAG, "Failed to resolve %s", SIP_SERVER);
        return false;
    }
    memcpy(&dest_addr.sin_addr, he->h_addr, 4);
    ESP_LOGI(TAG, "Resolved %s to %s", SIP_SERVER, inet_ntoa(dest_addr.sin_addr));
    
    // Get local IP
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    char local_ip[16];
    sprintf(local_ip, IPSTR, IP2STR(&ip_info.ip));
    
    // Build SIP OPTIONS message
    char sip_msg[512];
    snprintf(sip_msg, sizeof(sip_msg),
        "OPTIONS sip:%s:%d SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:5060;branch=%s;rport\r\n"
        "From: <sip:%s@%s:%d>;tag=%s\r\n"
        "To: <sip:%s:%d>\r\n"
        "Call-ID: %s\r\n"
        "CSeq: %d OPTIONS\r\n"
        "Max-Forwards: 70\r\n"
        "User-Agent: ESP32-S3-SIP-Client/1.0\r\n"
        "Accept: application/sdp, text/plain\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        SIP_SERVER, SIP_PORT, local_ip, branch_id,
        SIP_USER, SIP_SERVER, SIP_PORT, local_tag,
        SIP_SERVER, SIP_PORT,
        call_id, cseq++
    );
    
    ESP_LOGI(TAG, "Sending SIP OPTIONS to %s:%d (%s)", SIP_SERVER, SIP_PORT, inet_ntoa(dest_addr.sin_addr));
    ESP_LOGI(TAG, "Message length: %d bytes", strlen(sip_msg));
    
    // Log the message for debugging
    ESP_LOGI(TAG, "=== SIP OPTIONS MESSAGE ===");
    printf("%s", sip_msg);
    ESP_LOGI(TAG, "=== END MESSAGE ===");
    
    int send_err = sendto(sock, sip_msg, strlen(sip_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (send_err < 0) {
        ESP_LOGE(TAG, "Send error: errno %d (%s)", errno, strerror(errno));
        return false;
    }
    
    ESP_LOGI(TAG, "SIP OPTIONS sent successfully (%d bytes)", send_err);
    return true;
}

static bool send_sip_register(int sock)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SIP_PORT);
    
    // Resolve server hostname to IP
    struct hostent *he = gethostbyname(SIP_SERVER);
    if (he == NULL) {
        ESP_LOGE(TAG, "Failed to resolve %s", SIP_SERVER);
        return false;
    }
    memcpy(&dest_addr.sin_addr, he->h_addr, 4);
    
    // Get local IP
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    char local_ip[16];
    sprintf(local_ip, IPSTR, IP2STR(&ip_info.ip));
    
    // Build SIP REGISTER message
    char sip_msg[1024];
    snprintf(sip_msg, sizeof(sip_msg),
        "REGISTER sip:%s:%d SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:5060;branch=%s;rport\r\n"
        "From: <sip:%s@%s:%d>;tag=%s\r\n"
        "To: <sip:%s@%s:%d>\r\n"
        "Call-ID: %s\r\n"
        "CSeq: %d REGISTER\r\n"
        "Contact: <sip:%s@%s:5060>;expires=300\r\n"
        "Max-Forwards: 70\r\n"
        "User-Agent: ESP32-S3-SIP-Client/1.0\r\n"
        "Allow: INVITE,ACK,CANCEL,BYE,NOTIFY,REFER,MESSAGE,OPTIONS,INFO,SUBSCRIBE\r\n"
        "Expires: 300\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        SIP_SERVER, SIP_PORT, local_ip, branch_id,
        SIP_USER, SIP_SERVER, SIP_PORT, local_tag,
        SIP_USER, SIP_SERVER, SIP_PORT,
        call_id, cseq++,
        SIP_USER, local_ip
    );
    
    ESP_LOGI(TAG, "Sending SIP REGISTER to %s:%d", SIP_SERVER, SIP_PORT);
    ESP_LOGI(TAG, "Message length: %d bytes", strlen(sip_msg));
    
    int send_err = sendto(sock, sip_msg, strlen(sip_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (send_err < 0) {
        ESP_LOGE(TAG, "Send error: errno %d (%s)", errno, strerror(errno));
        return false;
    }
    
    ESP_LOGI(TAG, "SIP REGISTER sent successfully (%d bytes)", send_err);
    return true;
}

static void handle_sip_response(const char *response)
{
    ESP_LOGI(TAG, "SIP Response received:");
    ESP_LOGI(TAG, "=== BEGIN RESPONSE ===");
    printf("%s", response);
    ESP_LOGI(TAG, "=== END RESPONSE ===");
    
    // Parse response code
    if (strncmp(response, "SIP/2.0 ", 8) == 0) {
        int response_code = atoi(response + 8);
        
        switch (response_code) {
            case 200:
                ESP_LOGI(TAG, "SUCCESS: 200 OK - Registration successful!");
                break;
            case 401:
                ESP_LOGW(TAG, "401 Unauthorized - Authentication required");
                break;
            case 403:
                ESP_LOGW(TAG, "403 Forbidden - Registration denied");
                break;
            case 404:
                ESP_LOGW(TAG, "404 Not Found - User not found");
                break;
            case 407:
                ESP_LOGW(TAG, "407 Proxy Authentication Required");
                break;
            case 408:
                ESP_LOGW(TAG, "408 Request Timeout");
                break;
            case 500:
                ESP_LOGW(TAG, "500 Server Internal Error");
                break;
            default:
                ESP_LOGW(TAG, "Response Code: %d", response_code);
                break;
        }
    } else {
        ESP_LOGI(TAG, "Received SIP request (not a response)");
    }
}

static void sip_client_task(void *pvParameters)
{
    ESP_LOGI(TAG, "=== ESP32-S3 SIP CLIENT STARTING ===");
    ESP_LOGI(TAG, "Target Server: %s:%d", SIP_SERVER, SIP_PORT);
    ESP_LOGI(TAG, "Response Timeout: %d seconds", SIP_TIMEOUT);
    
    // Wait a bit for network to stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Test network connectivity
    ESP_LOGI(TAG, "Testing basic network connectivity...");
    if (!test_network_connectivity()) {
        ESP_LOGW(TAG, "Network connectivity issues detected!");
        ESP_LOGW(TAG, "Proceeding with SIP tests anyway...");
    } else {
        ESP_LOGI(TAG, "Network connectivity OK - proceeding with SIP tests");
    }
    
    int test_cycle = 0;
    
    while (1) {
        if (!wifi_connected) {
            ESP_LOGW(TAG, "WiFi disconnected, waiting...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        test_cycle++;
        ESP_LOGI(TAG, "=== TEST CYCLE #%d ===", test_cycle);
        
        bool response_received = false;
        
        int sock = create_udp_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create socket");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        
        // Generate new IDs for this test
        generate_random_ids();
        
        // Test 1: Send SIP OPTIONS
        ESP_LOGI(TAG, "Sending SIP OPTIONS to %s:%d", SIP_SERVER, SIP_PORT);
        
        if (send_sip_options(sock)) {
            char rx_buffer[2048];
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            
            // Set timeout
            struct timeval timeout;
            timeout.tv_sec = SIP_TIMEOUT;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            ESP_LOGI(TAG, "Waiting for response (%d seconds timeout)...", SIP_TIMEOUT);
            
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                              (struct sockaddr *)&source_addr, &socklen);
            
            if (len > 0) {
                rx_buffer[len] = 0;
                ESP_LOGI(TAG, "SUCCESS! Response from %s:%d (%d bytes)", 
                         inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port), len);
                handle_sip_response(rx_buffer);
                response_received = true;
            } else {
                ESP_LOGW(TAG, "No OPTIONS response (errno: %d - %s)", errno, strerror(errno));
            }
        } else {
            ESP_LOGW(TAG, "Failed to send SIP OPTIONS");
        }
        
        // Test 2: Try SIP REGISTER if OPTIONS failed
        if (!response_received) {
            ESP_LOGI(TAG, "Trying SIP REGISTER to %s:%d", SIP_SERVER, SIP_PORT);
            
            // Generate new IDs for REGISTER
            generate_random_ids();
            
            if (send_sip_register(sock)) {
                char rx_buffer[2048];
                struct sockaddr_in source_addr;
                socklen_t socklen = sizeof(source_addr);
                
                // Set timeout
                struct timeval timeout;
                timeout.tv_sec = SIP_TIMEOUT;
                timeout.tv_usec = 0;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                
                ESP_LOGI(TAG, "Waiting for REGISTER response (%d seconds timeout)...", SIP_TIMEOUT);
                
                int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                                  (struct sockaddr *)&source_addr, &socklen);
                
                if (len > 0) {
                    rx_buffer[len] = 0;
                    ESP_LOGI(TAG, "SUCCESS! REGISTER response from %s:%d (%d bytes)", 
                             inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port), len);
                    handle_sip_response(rx_buffer);
                    response_received = true;
                } else {
                    ESP_LOGW(TAG, "No REGISTER response (errno: %d - %s)", errno, strerror(errno));
                }
            }
        }
        
        close(sock);
        
        if (response_received) {
            ESP_LOGI(TAG, "*** ESP32 SIP CLIENT IS WORKING CORRECTLY! ***");
            ESP_LOGI(TAG, "Server %s:%d responded successfully", SIP_SERVER, SIP_PORT);
        } else {
            ESP_LOGW(TAG, "*** NO RESPONSE FROM SIP SERVER ***");
            ESP_LOGW(TAG, "Possible issues:");
            ESP_LOGW(TAG, "  1. Server %s:%d is down or unreachable", SIP_SERVER, SIP_PORT);
            ESP_LOGW(TAG, "  2. Network/ISP blocks SIP traffic");
            ESP_LOGW(TAG, "  3. Firewall blocking UDP packets");
            ESP_LOGW(TAG, "  4. DNS resolution issues");
        }
        
        // Wait before next test cycle
        ESP_LOGI(TAG, "Waiting 30 seconds before next test cycle...");
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}