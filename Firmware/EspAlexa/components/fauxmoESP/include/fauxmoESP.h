#ifndef FAUXMO_H
#define FAUXMO_H

#define FAUXMO_UDP_MULTICAST_IP     IPAddress(239,255,255,250)
#define FAUXMO_UDP_MULTICAST_PORT   1900
#define FAUXMO_TCP_MAX_CLIENTS      10
#define FAUXMO_TCP_PORT             1901
#define FAUXMO_RX_TIMEOUT           3
#define FAUXMO_DEVICE_UNIQUE_ID_LENGTH  12

//#define DEBUG_FAUXMO                Serial
#ifdef DEBUG_FAUXMO
    #if defined(ARDUINO_ARCH_ESP32)
        #define DEBUG_MSG_FAUXMO(fmt, ...) { DEBUG_FAUXMO.printf_P((PGM_P) PSTR(fmt), ## __VA_ARGS__); }
    #else
        #define DEBUG_MSG_FAUXMO(fmt, ...) { DEBUG_FAUXMO.printf(fmt, ## __VA_ARGS__); }
    #endif
#else
    #define DEBUG_MSG_FAUXMO(...)
#endif

#ifndef DEBUG_FAUXMO_VERBOSE_TCP
#define DEBUG_FAUXMO_VERBOSE_TCP    true//false
#endif

#ifndef DEBUG_FAUXMO_VERBOSE_UDP
#define DEBUG_FAUXMO_VERBOSE_UDP    true//false
#endif

#include <string>
#include <vector>
#include <functional>
#include "templates.h"
#include "lwip/ip_addr.h"
#include "esp_netif_ip_addr.h"
#include "esp_http_client.h"

typedef std::function<void(unsigned char, const char *, bool, unsigned char)> TSetStateCallback;

typedef struct {
    char * name;
    bool state;
    unsigned char value;
    char uniqueid[28];
} fauxmoesp_device_t;

class fauxmoESP 
{
    public:
        ~fauxmoESP();

        unsigned char addDevice(const char * device_name);
        bool renameDevice(unsigned char id, const char * device_name);
        bool renameDevice(const char * old_device_name, const char * new_device_name);
        bool removeDevice(unsigned char id);
        bool removeDevice(const char * device_name);
        char * getDeviceName(unsigned char id, char * buffer, size_t len);
        int getDeviceId(const char * device_name);
        void setDeviceUniqueId(unsigned char id, const char *uniqueid);
        void onSetState(TSetStateCallback fn) { _setCallback = fn; }
        bool setState(unsigned char id, bool state, unsigned char value);
        bool setState(const char * device_name, bool state, unsigned char value);
        bool process(esp_http_client_handle_t client, bool isGet, char *url, char *body);
        //bool process(AsyncClient *client, bool isGet, String url, String body);
        void enable(bool enable);
        void createServer(bool internal) { _internal = internal; }
        void setPort(unsigned long tcp_port) { _tcp_port = tcp_port; }
        void handle();
        
    private:
        // AsyncServer * _server;
        bool _enabled = false;
        bool _internal = true;
        unsigned int _tcp_port = FAUXMO_TCP_PORT;
        std::vector<fauxmoesp_device_t> _devices;
        esp_http_client_handle_t * _tcpClients[FAUXMO_TCP_MAX_CLIENTS];
        TSetStateCallback _setCallback = NULL;
        char* _deviceJson(unsigned char id, bool all); // all = true means we are listing all devices so use full description template

        int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        void _handleUDP();
        // void _onUDPData(const esp_ip4_addr_t remoteIP, unsigned int remotePort, void *data, size_t len);
        void _sendUDPResponse(int udp_socket, struct sockaddr_in *dest_addr);

        void _onTCPClient(esp_http_client_handle_t client);
        bool _onTCPData(esp_http_client_handle_t client, void *data, size_t len);
        bool _onTCPRequest(esp_http_client_handle_t client, bool isGet, char* url, char* body);
        bool _onTCPDescription(esp_http_client_handle_t client, char* url, char* body);
        bool _onTCPList(esp_http_client_handle_t client, char* url, char* body);
        bool _onTCPControl(esp_http_client_handle_t client, char* url, char* body);
        void _sendTCPResponse(esp_http_client_handle_t client, const char * code, char * body, const char * mime);

        std::string _byte2hex(uint8_t zahl);
        char* _makeMD5(char* text);
};
#endif // FAUXMO_H