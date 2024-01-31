#ifndef FAUXMO_H
#define FAUXMO_H

#include <string>
#include <vector>
#include "lwip/udp.h"
#include <functional>
#include <AsyncTCP.h>
#include "templates.h"
#include "lwip/sockets.h"
#include "esp_http_client.h"
#include "esp_netif_ip_addr.h"

#define FAUXMO_UDP_MULTICAST_IP     IPADDR4_INIT_BYTES(239,255,255,250)
#define FAUXMO_UDP_MULTICAST_PORT   1900
#define FAUXMO_TCP_MAX_CLIENTS      10
#define FAUXMO_TCP_PORT             1901
#define FAUXMO_RX_TIMEOUT           3
#define FAUXMO_DEVICE_UNIQUE_ID_LENGTH  12
#define UDP_BUFFER_SIZE 512

typedef std::function<void(unsigned char, const char *, bool, unsigned char)> TSetStateCallback;

typedef struct
{
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
        bool process(AsyncClient *client, bool isGet, char* url, char* body);
        void enable(bool enable);
        void createServer(bool internal) { _internal = internal; }
        void setPort(unsigned long tcp_port) { _tcp_port = tcp_port; }
        void handle();
        void _onTCPClient(AsyncClient *client);
        
    private:

        AsyncServer * _server;
        bool _enabled = false;
        bool _internal = true;
        unsigned int _tcp_port = FAUXMO_TCP_PORT;
        std::vector<fauxmoesp_device_t> _devices;
        AsyncClient * _tcpClients[FAUXMO_TCP_MAX_CLIENTS];
        TSetStateCallback _setCallback = NULL;

        char* _deviceJson(unsigned char id, bool all); 	// all = true means we are listing all devices so use full description template
        
        int _udpSocket;
        void _handleUDP();
        void _onUDPData(const esp_ip4_addr_t remoteIP, unsigned int remotePort, void *data, size_t len);
        void _sendUDPResponse(int udp_socket, struct sockaddr_in *dest_addr);

        bool _onTCPData(AsyncClient *client, void *data, size_t len);
        bool _onTCPRequest(AsyncClient *client, bool isGet, char* url, char* body);
        bool _onTCPDescription(AsyncClient *client, char* url, char* body);
        bool _onTCPList(AsyncClient *client, char* url, char* body);
        bool _onTCPControl(AsyncClient *client, char* url, char* body);
        void _sendTCPResponse(AsyncClient *client, const char * code, char* body, const char * mime);
        char* _byte2hex(uint8_t zahl);
        char* _makeMD5(char* text);
};
#endif // FAUXMO_H