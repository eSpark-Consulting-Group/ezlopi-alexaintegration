#include <string>
#include <stdio.h>
#include <esp_log.h>
#include <mbedtls/md5.h>
#include "fauxmoESP.h"
#include <freertos/event_groups.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include <sys/socket.h>

// // -----------------------------------------------------------------------------
// // UDP
// // -----------------------------------------------------------------------------

void fauxmoESP::_sendUDPResponse(int udp_socket, struct sockaddr_in *dest_addr) 
{
    ESP_LOGI("UDP", "Responding to M-SEARCH request");

    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Create a buffer for your response
    char response[UDP_BUFFER_SIZE];  // Adjust the size as needed
    snprintf(response, sizeof(response), FAUXMO_UDP_RESPONSE_TEMPLATE, IP2STR(&ip_info.ip), 80, mac, mac);

    // Assuming you have created a UDP socket
    struct sockaddr_in dest_address;
    dest_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    dest_address.sin_family = AF_INET;
    dest_address.sin_port = htons(1900);

    if (sendto(_udpSocket, response, strlen(response), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) 
    {
        ESP_LOGE("UDP", "Error sending UDP response");
    } 
    else 
    {
        ESP_LOGI("UDP", "UDP response sent");
    }
}

void fauxmoESP::_handleUDP() {

	struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    char buffer[UDP_BUFFER_SIZE];

	int len = recvfrom(_udpSocket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    if (len > 0) {


        buffer[len] = 0;

		#if DEBUG_FAUXMO_VERBOSE_UDP
			ESP_LOGI("UDP", "[FAUXMO] UDP packet received\n%s", buffer);
		#endif

        const char* request = buffer;
        if(strstr(request,"M-SEARCH") != NULL) {
            if (strstr(request,"ssdp:discover") != NULL || strstr(request,"upnp:rootdevice") != NULL || strstr(request,"device:basic:1") != NULL) {
                _sendUDPResponse(_udpSocket, &source_addr);
            }
        }
    }
}


// // -----------------------------------------------------------------------------
// // TCP
// // -----------------------------------------------------------------------------

void fauxmoESP::_sendTCPResponse(AsyncClient *client, const char * code, char * body, const char * mime) {

	size_t header_size = strlen(FAUXMO_TCP_HEADERS) + 32;
    char* headers = (char*)malloc(header_size);
	if(headers != NULL)
    {
        snprintf(
		headers, header_size,
		FAUXMO_TCP_HEADERS,
		code, mime, strlen(body)
    	);
    }

	#if DEBUG_FAUXMO_VERBOSE_TCP
		ESP_LOGI("TCP","[FAUXMO] Response:\n%s%s\n", headers, body);
	#endif

	client->write(headers);
	client->write(body);
}

char* fauxmoESP::_deviceJson(unsigned char id, bool all = true) {

	if (id >= _devices.size()) return "{}";

	fauxmoesp_device_t device = _devices[id];

	ESP_LOGI("DEVICEJSON","[FAUXMO] Sending device info for \"%s\", uniqueID = \"%s\"\n", device.name, device.uniqueid);
	size_t buffer_size = strlen(FAUXMO_DEVICE_JSON_TEMPLATE) + 64;
    char* buffer = (char*)malloc(buffer_size);

	if (all)
	{
		snprintf(
			buffer, buffer_size,
			FAUXMO_DEVICE_JSON_TEMPLATE,
			device.name, device.uniqueid,
			device.state ? "true": "false",
			device.value
		);
	}
	else
	{
		snprintf(
			buffer, buffer_size,
			FAUXMO_DEVICE_JSON_TEMPLATE_SHORT,
			device.name, device.uniqueid
		);
	}

	return buffer;
}

char* fauxmoESP::_byte2hex(uint8_t zahl)
{
  char* hstring = new char[3];  // Assuming you want a two-character hex char* plus the null terminator
  sprintf(hstring, "%02X", zahl);  // Use sprintf to format the hex value with leading zero if necessary
  return hstring;
}

char* fauxmoESP::_makeMD5(char* text)
{
  unsigned char bbuf[16];
    char* hash = new char[33];
    mbedtls_md5_context ctx;
    mbedtls_md5_starts_ret(&ctx);
    size_t textLength = strlen(text);
    mbedtls_md5_update_ret(&ctx, reinterpret_cast<const unsigned char*>(text), textLength);
    mbedtls_md5_finish_ret(&ctx, bbuf);
    for (uint8_t i = 0; i < 16; i++)
    {
        hash[i * 2] = (bbuf[i] >> 4) + ((bbuf[i] >> 4) < 10 ? '0' : 'a' - 10);  // High nibble
        hash[i * 2 + 1] = (bbuf[i] & 0x0F) + ((bbuf[i] & 0x0F) < 10 ? '0' : 'a' - 10);  // Low nibble
    }
    hash[32] = '\0';
    return hash;
}

bool fauxmoESP::_onTCPDescription(AsyncClient *client, char* url, char* body) {

	(void) url;
	(void) body;

	ESP_LOGI("FAUXMO"," Handling /description.xml request\n");

	tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	char response[1024];  // Adjust the size based on your requirements
    snprintf(response, sizeof(response), FAUXMO_DESCRIPTION_TEMPLATE,
                                  IP2STR(&ip_info.ip), _tcp_port,
                                  IP2STR(&ip_info.ip), _tcp_port,
                                  macStr, macStr);
	_sendTCPResponse(client, "200 OK", response, "text/xml");
	return true;
}

bool fauxmoESP::_onTCPList(AsyncClient *client, char* url, char* body) {

	ESP_LOGI("FAUXMO","Handling list request\n");

	// Get the index
	const char *pos = strstr(url,"lights");
	if (pos == NULL) return false;

	// Get the id
	int id = atoi(pos + 7);

	// This will hold the response char*	
	char response[UDP_BUFFER_SIZE];

	// Client is requesting all devices
	if (0 == id) {

		strcat(response, "{");
        for (int i = 0; i < _devices.size(); i++) 
        {
            if (i > 0) 
            {
                strcat(response, ",");
            }
            strcat(response, "\"");
            strcat(response, itoa(i + 1, response + strlen(response), 10));
            strcat(response, "\":");
            strcat(response, _deviceJson(i, false));
        }
        strcat(response, "}");

	// Client is requesting a single device
	} else {
		strcat(response, _deviceJson(id - 1));
	}

	_sendTCPResponse(client, "200 OK", (char *) response, "application/json");
	
	return true;

}

bool fauxmoESP::_onTCPControl(AsyncClient *client, char* url, char* body) {

	// "devicetype" request
	if (strstr(body,"devicetype") != NULL) {
		ESP_LOGI("FAUXMO","Handling devicetype request\n");
		_sendTCPResponse(client, "200 OK", (char *) "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]", "application/json");
		return true;
	}

	// "state" request
	if ((strstr(url,"state") != NULL) && (strlen(body) > 0)) {

		// Get the index
		char *pos = strstr(url,"lights");
		if (pos == NULL) return false;

		ESP_LOGI("FAUXMO","Handling state request\n");

		// Get the index
		int id = atoi(pos+7);
		if (id > 0) {

			--id;

			// Brightness
			pos = strstr(body,"bri");
			if (pos > 0) {
				int value = atoi(pos+5);
				_devices[id].value = value;
				_devices[id].state = (value > 0);
			} else if (strstr(body,"false") != NULL) {
				_devices[id].state = false;
			} else {
				_devices[id].state = true;
				if (0 == _devices[id].value) _devices[id].value = 255;
			}

			char response[512];
			snprintf(
				response, sizeof(response),
				FAUXMO_TCP_STATE_RESPONSE,
				id+1, _devices[id].state ? "true" : "false", id+1, _devices[id].value
			);
			_sendTCPResponse(client, "200 OK", response, "text/xml");

			if (_setCallback) {
				_setCallback(id, _devices[id].name, _devices[id].state, _devices[id].value);
			}
			return true;
		}
	}
	return false;
}

bool fauxmoESP::_onTCPRequest(AsyncClient *client, bool isGet, char* url, char* body) {

    if (!_enabled) return false;

	ESP_LOGI("FAUXMO","isGet: %s\n", isGet ? "true" : "false");
	ESP_LOGI("FAUXMO","URL: %s\n", url);
	if (!isGet) ESP_LOGW("FAUXMO","Body:\n%s\n", body);

	if (strcmp(url, "/description.xml") == 0) {
        return _onTCPDescription(client, url, body);
    }

	if (strcmp(url, "/api") == 0) {
		if (isGet) {
			return _onTCPList(client, url, body);
		} else {
       		return _onTCPControl(client, url, body);
		}
	}
	return false;
}

bool fauxmoESP::_onTCPData(AsyncClient *client, void *data, size_t len) {

    if (!_enabled) return false;

	char * p = (char *) data;
	p[len] = 0;

	ESP_LOGI("FAUXMO","TCP request\n%s\n", p);

	// Method is the first word of the request
	char * method = p;

	while (*p != ' ') p++;
	*p = 0;
	p++;
	
	// Split word and flag start of url
	char * url = p;

	// Find next space
	while (*p != ' ') p++;
	*p = 0;
	p++;

	// Find double line feed
	unsigned char c = 0;
	while ((*p != 0) && (c < 2)) {
		if (*p != '\r') {
			c = (*p == '\n') ? c + 1 : 0;
		}
		p++;
	}
	char * body = p;

	bool isGet = (strncmp(method, "GET", 3) == 0);

	return _onTCPRequest(client, isGet, url, body);

}

void fauxmoESP::_onTCPClient(AsyncClient *client) {

	if (_enabled)
	{
	    for (unsigned char i = 0; i < FAUXMO_TCP_MAX_CLIENTS; i++) {

	        if (!_tcpClients[i] || !_tcpClients[i]->connected()) {

	            _tcpClients[i] = client;

	            client->onAck([i](void *s, AsyncClient *c, size_t len, uint32_t time) {
	            }, 0);

	            client->onData([this, i](void *s, AsyncClient *c, void *data, size_t len) {
	                _onTCPData(c, data, len);
	            }, 0);
	            client->onDisconnect([this, i](void *s, AsyncClient *c) {
			if(_tcpClients[i] != NULL) {
	                    _tcpClients[i]->free();
	                    _tcpClients[i] = NULL;
	                }
			else {
	                    ESP_LOGI("FAUXMO","Client %d already disconnected\n", i);
	                }
	                delete c;
	                ESP_LOGI("FAUXMO","Client #%d disconnected\n", i);
	            }, 0);

	            client->onError([i](void *s, AsyncClient *c, int8_t error)
				{
					char buffer[50];
	                ESP_LOGE("FAUXMO","Error %s (%d) on client #%d\n", itoa(error,buffer,10), error, i);
	            }, 0);

	            client->onTimeout([i](void *s, AsyncClient *c, uint32_t time) {
	                ESP_LOGE("FAUXMO","Timeout on client #%d at %i\n", i, time);
	                c->close();
	            }, 0);

                    client->setRxTimeout(FAUXMO_RX_TIMEOUT);

	            ESP_LOGI("FAUXMO","Client #%d connected\n", i);
	            return;
	        }
	    }
		ESP_LOGE("FAUXMO","Rejecting - Too many connections\n");

	} else {
		ESP_LOGE("FAUXMO","Rejecting - Disabled\n");
	}

    client->onDisconnect([](void *s, AsyncClient *c) {
        c->free();
        delete c;
    });
    client->close(true);
}

// -----------------------------------------------------------------------------
// Devices
// -----------------------------------------------------------------------------
fauxmoESP::~fauxmoESP() {
	if (_udpSocket >= 0) {
        close(_udpSocket);
        ESP_LOGI("FAUXMO", "UDP socket closed");
    }
	// Free the name for each device
	for (auto& device : _devices) {
		free(device.name);
  	}
	// Delete devices  
	_devices.clear();
}

void fauxmoESP::setDeviceUniqueId(unsigned char id, const char *uniqueid)
{
    strncpy(_devices[id].uniqueid, uniqueid, FAUXMO_DEVICE_UNIQUE_ID_LENGTH);
}

unsigned char fauxmoESP::addDevice(const char * device_name) {

    fauxmoesp_device_t device;
    unsigned int device_id = _devices.size();

    // init properties
    device.name = strdup(device_name);
  	device.state = false;
	device.value = 0;

    // create the uniqueid
    uint8_t mac[6];
	esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(device.uniqueid, 27, "%02X:%02X:%02X:%02X:%02X:%02X-%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], device_id);

    // Attach
    _devices.push_back(device);
    ESP_LOGI("FAUXMO","Device '%s' added as #%d\n", device_name, device_id);
    return device_id;
}

int fauxmoESP::getDeviceId(const char * device_name) {
    for (unsigned int id=0; id < _devices.size(); id++) {
        if (strcmp(_devices[id].name, device_name) == 0) {
            return id;
        }
    }
    return -1;
}

bool fauxmoESP::renameDevice(unsigned char id, const char * device_name) {
    if (id < _devices.size()) {
        free(_devices[id].name);
        _devices[id].name = strdup(device_name);
        ESP_LOGI("FAUXMO","Device #%d renamed to '%s'\n", id, device_name);
        return true;
    }
    return false;
}

bool fauxmoESP::renameDevice(const char * old_device_name, const char * new_device_name) {
	int id = getDeviceId(old_device_name);
	if (id < 0) return false;
	return renameDevice(id, new_device_name);
}

bool fauxmoESP::removeDevice(unsigned char id) {
    if (id < _devices.size()) {
        free(_devices[id].name);
		_devices.erase(_devices.begin()+id);
        ESP_LOGI("FAUXMO","[FAUXMO] Device #%d removed\n", id);
        return true;
    }
    return false;
}

bool fauxmoESP::removeDevice(const char * device_name) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return removeDevice(id);
}

char * fauxmoESP::getDeviceName(unsigned char id, char * device_name, size_t len) {
    if ((id < _devices.size()) && (device_name != NULL)) {
        strncpy(device_name, _devices[id].name, len);
    }
    return device_name;
}

bool fauxmoESP::setState(unsigned char id, bool state, unsigned char value) {
    if (id < _devices.size()) {
		_devices[id].state = state;
		_devices[id].value = value;
		return true;
	}
	return false;
}

bool fauxmoESP::setState(const char * device_name, bool state, unsigned char value) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	_devices[id].state = state;
	_devices[id].value = value;
	return true;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool fauxmoESP::process(AsyncClient *client, bool isGet, char* url, char* body) {
	return _onTCPRequest(client, isGet, url, body);
}

void fauxmoESP::handle() {
    if (_enabled) _handleUDP();
}

void fauxmoESP::enable(bool enable) {
    if (enable == _enabled) return;
    _enabled = enable;

    if (_enabled) {
        // Start UDP server
		ESP_LOGI("UDP","IN UDP BEFORE SOCKET!");
        _udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (_udpSocket < 0) {
            ESP_LOGE("FAUXMO", "Error creating UDP socket: %s", strerror(errno));
            return;
        }

        struct sockaddr_in udp_bind_addr;
        memset(&udp_bind_addr, 0, sizeof(udp_bind_addr));
        udp_bind_addr.sin_family = AF_INET;
        udp_bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        udp_bind_addr.sin_port = htons(1900);

        if (bind(_udpSocket, (struct sockaddr*)&udp_bind_addr, sizeof(udp_bind_addr)) < 0) {
            ESP_LOGE("FAUXMO", "Error binding UDP socket: %s", strerror(errno));
            close(_udpSocket);
            return;
        }

        // Join multicast group
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(_udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            ESP_LOGE("FAUXMO", "Failed to join multicast group: %s", strerror(errno));
            close(_udpSocket);
            return;
        }

        ESP_LOGI("UDP","[FAUXMO] UDP server started\n");
    } else {
        // Stop UDP server
        if (_udpSocket >= 0) {
            close(_udpSocket);
            _udpSocket = -1;
            ESP_LOGI("UDP","[FAUXMO] UDP server stopped\n");
        }
    }
}