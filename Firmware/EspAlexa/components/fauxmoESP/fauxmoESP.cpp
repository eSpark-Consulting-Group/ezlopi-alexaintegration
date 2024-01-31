#include <string>
#include <stdio.h>
#include <ESP_LOG.h>
#include <mbedtls/md5.h>
#include "fauxmoESP.h"
#include <freertos/event_groups.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_wifi.h"
// #include "ESP_LOG.h"
#include "esp_system.h"
#include <sys/socket.h>

// // -----------------------------------------------------------------------------
// // UDP
// // -----------------------------------------------------------------------------

static const char *TAG = "[FAUXMO]";

void fauxmoESP::_sendUDPResponse(int udp_socket, struct sockaddr_in *dest_addr) 
{
	const char *func_TAG = "_sendUDPResponse:";
    //ESP_LOGI(TAG, "%s Responding to M-SEARCH request", func_TAG);

	// char ip[16];
  	// inet_ntop(AF_INET, &dest_addr->sin_addr, ip, sizeof(ip));
	// //ESP_LOGW(TAG,"%s source ip address is %s", func_TAG, ip);

    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
	uint8_t index = 0;
	char macId[50];
	for(uint8_t i=0; i<6; i++){
   		index += sprintf(&macId[index], "%02x", mac[i]);
	}

    // Create a buffer for your response
    char response[strlen(FAUXMO_UDP_RESPONSE_TEMPLATE) + 128 + 64] = {NULL};  // Adjust the size as needed
    int len = snprintf(response, sizeof(response), FAUXMO_UDP_RESPONSE_TEMPLATE, IP2STR(&ip_info.ip), 80, macId, macId);

    //ESP_LOGI(TAG, "%s response is: %s", func_TAG, response);
	// //ESP_LOGI(TAG, "%s strlen(response) is %d", func_TAG, len);
	int err = sendto(_udpSocket, response, len, 0, (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in));
	vTaskDelay(100);
    if (err < 0) 
    {
		perror("sendto");
		//ESP_LOGE(TAG, "%s Error occurred during sending: errno %d", func_TAG, errno);
        //ESP_LOGE(TAG, "%s Error sending UDP response. Error: %d", func_TAG, err);
    } 
    else 
    {
        //ESP_LOGI(TAG, "%s UDP response sent", func_TAG);
    }
}

void fauxmoESP::_handleUDP() {
	const char *func_TAG = "_handleUDP:";

	struct sockaddr_in source_addr;

    char buffer[UDP_BUFFER_SIZE + 64] = {NULL};
    socklen_t socklen = sizeof(source_addr);
	int len = recvfrom(_udpSocket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
	
	// char ip[16];
	// inet_ntop(AF_INET, &source_addr.sin_addr, ip, sizeof(ip));
	// //ESP_LOGI(TAG,"%s source ip address is %s", func_TAG, ip);
	// //ESP_LOGI(TAG,"%s udp_socket is %d", func_TAG, _udpSocket);
    
	if (len > 0)
	{
        buffer[len] = 0;
		//ESP_LOGI(TAG,"%s UDP packet received\n%s", func_TAG, buffer);
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
	const char *func_TAG = "_sendTCPResponse:";

	size_t header_size = strlen(FAUXMO_TCP_HEADERS) + 32;
    char headers [header_size] = {NULL};
    // char* headers = (char*)malloc(header_size);
	if(headers != NULL)
    {
        snprintf(
		headers, header_size,
		FAUXMO_TCP_HEADERS,
		code, mime, strlen(body)
    	);
    }

	//ESP_LOGI(TAG,"%s Response is\n%s%s", func_TAG, headers, body);

	// char temp[strlen(headers) + strlen(body) + 16] = {NULL};
	// strcat(temp, headers);
	// strcat(temp, body);
	// client->write(temp);

	client->write(headers);
	client->write(body);
}

char* fauxmoESP::_deviceJson(unsigned char id, bool all = true) {
	const char *func_TAG = "_deviceJson:";

	if (id >= _devices.size()) return "{}";

	fauxmoesp_device_t device = _devices[id];

	//ESP_LOGI(TAG,"%s Sending device info for \"%s\", uniqueID = \"%s\"", func_TAG, device.name, device.uniqueid);
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
	const char *func_TAG = "_byte2hex:";
	char* hstring = new char[3];  // Assuming you want a two-character hex char* plus the null terminator
	sprintf(hstring, "%02X", zahl);  // Use sprintf to format the hex value with leading zero if necessary
	return hstring;
}

char* fauxmoESP::_makeMD5(char* text)
{
	const char *func_TAG = "_makeMD5:";

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
	const char *func_TAG = "_onTCPDescription:";

	(void) url;
	(void) body;

	//ESP_LOGI(TAG,"%s Handling /description.xml request", func_TAG);

	tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	char response[1024];  // Adjust the size based on your requirements
    snprintf(response, sizeof(response), FAUXMO_DESCRIPTION_TEMPLATE, IP2STR(&ip_info.ip), _tcp_port, IP2STR(&ip_info.ip), _tcp_port, macStr, macStr);
	// //ESP_LOGI(TAG,"%s Generated response is %s", func_TAG, response);
	_sendTCPResponse(client, "200 OK", response, "text/xml");
	return true;
}

bool fauxmoESP::_onTCPList(AsyncClient *client, char* url, char* body) {
	const char *func_TAG = "_onTCPList:";

	//ESP_LOGI(TAG,"%s Handling list request", func_TAG);

	// Get the index
	char *pos = strstr(url,"lights");
	if (pos == NULL){ 
	// //ESP_LOGW(TAG,"%s STATE IS FALSE", func_TAG);
	return false;
	}
	// Get the id
	int id = atoi(pos + 7);
	// //ESP_LOGW(TAG,"%s id is %d", func_TAG, id);
	// //ESP_LOGW(TAG,"%s _devices.size() is %d", func_TAG, _devices.size());

	// This will hold the response char*	
	char response[UDP_BUFFER_SIZE] = {NULL};
	// response[UDP_BUFFER_SIZE-1] = '\0';

	// Client is requesting all devices
	if (0 == id) {

		strcat(response, "{");
        for (int i = 0; i < _devices.size(); i++) 
        {
            if (i > 0) 
            {
                strcat(response, ",");
            }
            // strcat(response, "\"");
			char tempChar[UDP_BUFFER_SIZE] = {NULL};
        	snprintf(tempChar, UDP_BUFFER_SIZE - strlen(tempChar),"\"%d\":%s", i + 1, _deviceJson(i, false));
			strcat(response, tempChar);     
			// strcat(response, itoa(i + 1, response + strlen(response), 10));
            // strcat(response, "\":");
            // strcat(response, _deviceJson(i, false));
        }
        strcat(response, "}");

	// Client is requesting a single device
	} else {
		strcat(response, _deviceJson(id - 1));
	}
	// //ESP_LOGI(TAG,"%s Generated response is %s\nResponse length is %d", func_TAG, response, strlen(response));

	_sendTCPResponse(client, "200 OK", (char *) response, "application/json");
	
	return true;

}

bool fauxmoESP::_onTCPControl(AsyncClient *client, char* url, char* body) {
	const char *func_TAG = "_onTCPControl:";

	// "devicetype" request
	if (strstr(body,"devicetype") != NULL) {
		//ESP_LOGI(TAG,"%s Handling devicetype request", func_TAG);
		_sendTCPResponse(client, "200 OK", (char *) "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]", "application/json");
		return true;
	}

	// if((strstr(url,"renameDevice") != NULL) && (strlen(body) > 0)){

	// }

	// "state" request
	if ((strstr(url,"state") != NULL) && (strlen(body) > 0)) {
		// Get the index
		char *pos = strstr(url,"lights");
		if (pos == NULL) return false;

		//ESP_LOGI(TAG,"%s Handling state request", func_TAG);

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

			char response[512] = {NULL};
			snprintf(
				response, sizeof(response),
				FAUXMO_TCP_STATE_RESPONSE,
				id+1, _devices[id].state ? "true" : "false", id+1, _devices[id].value
			);
			//ESP_LOGI(TAG,"%s Response generated is: %s", func_TAG, response);
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
	const char *func_TAG = "_onTCPRequest:";

    if (!_enabled) return false;

	//ESP_LOGI(TAG,"%s isGet: %s", func_TAG, isGet ? "true" : "false");
	//ESP_LOGI(TAG,"%s URL: %s", func_TAG, url);
	/*if (!isGet)*/ //ESP_LOGI(TAG,"Body:\n%s\n", body);

	if (strstr(url, "/description.xml") != NULL) {
        return _onTCPDescription(client, url, body);
    }

	if (strstr(url, "/api") != NULL) {
		if (isGet) {
			return _onTCPList(client, url, body);
		} else {
       		return _onTCPControl(client, url, body);
		}
	}
	return false;
}

bool fauxmoESP::_onTCPData(AsyncClient *client, void *data, size_t len) {
	const char *func_TAG = "_onTCPData:";

    if (!_enabled) return false;

	char * p = (char *) data;
	p[len] = 0;

	//ESP_LOGI(TAG,"%s TCP request\n%s", func_TAG, p);

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
	
	// //ESP_LOGI(TAG,"%s TCP method: %s", func_TAG, method);
	// //ESP_LOGI(TAG,"%s TCP url: %s", func_TAG, url);
	// //ESP_LOGI(TAG,"%s TCP body: %s", func_TAG, body);

	bool isGet = (strncmp(method, "GET", 3) == 0);

	return _onTCPRequest(client, isGet, url, body);

}

void fauxmoESP::_onTCPClient(AsyncClient *client) {
	const char *func_TAG = "_onTCPClient:";

	if (_enabled)
	{
		// //ESP_LOGI("debug","_onTCPClient enabled");
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
	                    //ESP_LOGI(TAG,"Client %d already disconnected", i);
	                }
	                delete c;
	                //ESP_LOGI(TAG,"Client #%d disconnected", i);
	            }, 0);

	            client->onError([i](void *s, AsyncClient *c, int8_t error)
				{
					char buffer[50];
	                //ESP_LOGE(TAG,"Error %s (%d) on client #%d\n", itoa(error,buffer,10), error, i);
	            }, 0);

	            client->onTimeout([i](void *s, AsyncClient *c, uint32_t time) {
	                //ESP_LOGE(TAG,"Timeout on client #%d at %i", i, time);
	                c->close();
	            }, 0);

                    client->setRxTimeout(FAUXMO_RX_TIMEOUT);

	            //ESP_LOGI(TAG,"Client #%d connected", i);
	            return;
	        }
	    }
		//ESP_LOGE(TAG,"Rejecting - Too many connections");

	} else {
		//ESP_LOGE(TAG,"Rejecting - Disabled");
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
	const char *func_TAG = "~fauxmoESP:";

	if (_udpSocket >= 0) {
        close(_udpSocket);
        //ESP_LOGI(TAG,"%s UDP socket closed", func_TAG);
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
	const char *func_TAG = "addDevice:";

    fauxmoesp_device_t device;
    unsigned int device_id = _devices.size();

    // init properties
    device.name = strdup(device_name);
  	device.state = false;
	device.value = 0;

    // create the uniqueid
    uint8_t mac[6];
	esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(device.uniqueid, 27, "%02X:%02X:%02X:%02X:%02X:%02X:00:00-%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], device_id);

    // Attach
    _devices.push_back(device);
    //ESP_LOGI(TAG,"%s Device '%s' added as #%d", func_TAG, device_name, device_id);
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
	const char *func_TAG = "renameDevice:";

    if (id < _devices.size()) {
        free(_devices[id].name);
        _devices[id].name = strdup(device_name);
        //ESP_LOGI(TAG,"%s Device #%d renamed to '%s'", func_TAG, id, device_name);
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
	const char *func_TAG = "removeDevice:";

    if (id < _devices.size()) {
        free(_devices[id].name);
		_devices.erase(_devices.begin()+id);
        //ESP_LOGI(TAG,"%s Device #%d removed", func_TAG, id);
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
	const char *func_TAG = "enable:";

    if (enable == _enabled) return;
    _enabled = enable;

	if (_enabled) {
		//ESP_LOGI(TAG, "%s Device Enabled", func_TAG);
	} 
	else {
		//ESP_LOGI(TAG, "%s Device Disabled", func_TAG);
	}

	if(_enabled)	{
		if (_internal) {


			//ESP_LOGI(TAG,"%s Starting TCP.", func_TAG);
			_server = new AsyncServer(_tcp_port);
			_server->onClient([this](void *s, AsyncClient* c) {
				_onTCPClient(c);
			}, 0);
			
			_server->begin();
			//ESP_LOGI(TAG,"%s TCP started with port %d", func_TAG, _tcp_port);
			
			// Start UDP server
			//ESP_LOGI(TAG,"%s Starting UDP.", func_TAG);
			_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
			if (_udpSocket < 0) {
				//ESP_LOGE(TAG, "%s Error creating UDP socket: %s", func_TAG, strerror(errno));
				return;
			}

			struct sockaddr_in udp_bind_addr;
			memset(&udp_bind_addr, 0, sizeof(udp_bind_addr));
			udp_bind_addr.sin_family = AF_INET;
			udp_bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			udp_bind_addr.sin_port = htons(FAUXMO_UDP_MULTICAST_PORT);

			if (bind(_udpSocket, (struct sockaddr*)&udp_bind_addr, sizeof(udp_bind_addr)) < 0) {
				//ESP_LOGE(TAG, "%s Error binding UDP socket: %s", func_TAG, strerror(errno));
				close(_udpSocket);
				return;
			}

			// Join multicast group
			struct ip_mreq mreq;
			mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

			if (setsockopt(_udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
				//ESP_LOGE(TAG, "%s Failed to join multicast group: %s", func_TAG, strerror(errno));
				close(_udpSocket);
				return;
			}


			//ESP_LOGI(TAG,"%s UDP server started with port %d", func_TAG, FAUXMO_UDP_MULTICAST_PORT);
		} 
		else {
			// Stop UDP server
			if (_udpSocket >= 0) {
				close(_udpSocket);
				_udpSocket = -1;
				//ESP_LOGI(TAG,"%s UDP server stopped", func_TAG);
			}
		}
	}
}