
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

const static char *TAG = "Fauxmo";

#define UDP_BUFFER_SIZE 512
#define MAX_TCP_CLIENTS 5  // Adjust as needed
// -----------------------------------------------------------------------------
// UDP
// -----------------------------------------------------------------------------

void fauxmoESP::_sendUDPResponse(int udp_socket, struct sockaddr_in *dest_addr) 
{
    ESP_LOGI(TAG, "Responding to M-SEARCH request");

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

    if (sendto(udp_socket, response, strlen(response), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) 
    {
        ESP_LOGE("UDP", "Error sending UDP response");
    } 
    else 
    {
        ESP_LOGI("UDP", "UDP response sent");
    }
}

void fauxmoESP::_handleUDP() 
{
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    char buffer[UDP_BUFFER_SIZE];

	int len = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

    if (len > 0) 
    {
        buffer[len] = 0;

		#if DEBUG_FAUXMO_VERBOSE_UDP
			ESP_LOGI("UDP", "[FAUXMO] UDP packet received\n%s", buffer);
		#endif

       const char* request = buffer;
        if (strstr(request,"M-SEARCH") != NULL)
        {
            if (strstr(request,"ssdp:discover") != NULL || strstr(request,"upnp:rootdevice") != NULL || strstr(request,"device:basic:1") != NULL)
            {
                _sendUDPResponse(udp_socket, &source_addr);
            }
        }
    }
}

// // -----------------------------------------------------------------------------
// // TCP
// // -----------------------------------------------------------------------------

void fauxmoESP::_sendTCPResponse(esp_http_client_handle_t client, const char * code, char * body, const char * mime) 
{

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

	esp_http_client_write(client, headers, strlen(headers));
    esp_http_client_write(client, body, strlen(body));
    free(headers);
}

char* fauxmoESP::_deviceJson(unsigned char id, bool all = true) 
{
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

std::string fauxmoESP::_byte2hex(uint8_t zahl)
{
  std::string hstring = std::to_string(zahl);
  if (zahl < 16)
  {
    hstring = "0" + hstring;
  }
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

bool fauxmoESP::_onTCPDescription(esp_http_client_handle_t client, char* url, char* body)
{
	(void) url;
	(void) body;
	ESP_LOGI("TCP","[FAUXMO] Handling /description.xml request\n");
	tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char response[1024];  // Adjust the size based on your requirements

    // Create the response
    snprintf(response, sizeof(response), FAUXMO_DESCRIPTION_TEMPLATE,
                                  IP2STR(&ip_info.ip), _tcp_port,
                                  IP2STR(&ip_info.ip), _tcp_port,
                                  macStr, macStr);

	_sendTCPResponse(client, "200 OK", response, "text/xml");
	return true;
}

bool fauxmoESP::_onTCPList(esp_http_client_handle_t client, char* url, char* body)
{
	ESP_LOGI("TCP","[FAUXMO] Handling list request\n");
	// Get the index
	const char *pos = strstr(url,"lights");
	if (pos == NULL) return false;

	// Get the id
	int id = atoi(pos + 7);

	// // This will hold the response string	
	char response[512];

	// Client is requesting all devices
	if (id == 0) 
    {
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
	} 
    else 
    {
		strcat(response, _deviceJson(id - 1));
	}
	_sendTCPResponse(client, "200 OK", (char *) response, "application/json");
	return true;
}

bool fauxmoESP::_onTCPControl(esp_http_client_handle_t client, char* url, char* body)
{
	// "devicetype" request
	if (strstr(body,"devicetype") != NULL)
	{
		ESP_LOGI("TCP","[FAUXMO] Handling devicetype request\n");
		_sendTCPResponse(client, "200 OK", "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]", "application/json");
		return true;
	}

	// "state" request
	if ((strstr(url,"state") != NULL) && (strlen(body) > 0))
	{
		// Get the index
		char *pos = strstr(url,"lights");
		if (pos == NULL) return false;

		ESP_LOGI("TCP","[FAUXMO] Handling state request\n");

		// Get the index
		int id = atoi(pos+7);
		if (id > 0)
		{
			--id;
			// Brightness
			pos = strstr(body,"bri");
			if (pos > 0)
			{
				int value = atoi(pos+5);
				_devices[id].value = value;
				_devices[id].state = (value > 0);
			}
			else if (strstr(body,"false") != NULL) 
			{
				_devices[id].state = false;
			}
			else 
			{
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

			if (_setCallback)
			{
				_setCallback(id, _devices[id].name, _devices[id].state, _devices[id].value);
			}
			return true;
		}
	}
	return false;
}

bool fauxmoESP::_onTCPRequest(esp_http_client_handle_t client, bool isGet, char* url, char* body)
{
    if (!_enabled) return false;

	#if DEBUG_FAUXMO_VERBOSE_TCP
		ESP_LOGI("TCP","[FAUXMO] isGet: %s\n", isGet ? "true" : "false");
		ESP_LOGI("TCP","[FAUXMO] URL: %s\n", url);
		if (!isGet) ESP_LOGI("TCP","[FAUXMO] Body:\n%s\n", body);
	#endif

	if (strcmp(url, "/description.xml") == 0)
	{
        return _onTCPDescription(client, url, body);
    }
	if (strcmp(url, "/api") == 0)
	{
		if (isGet)
		{
			return _onTCPList(client, url, body);
		}
		else
		{
       		return _onTCPControl(client, url, body);
		}
	}
	return false;
}

bool fauxmoESP::_onTCPData(esp_http_client_handle_t client, void *data, size_t len)
{
    if (!_enabled) return false;
	char * p = (char *) data;
	p[len] = 0;

	#if DEBUG_FAUXMO_VERBOSE_TCP
		ESP_LOGI("TCP","[FAUXMO] TCP request\n%s\n", p);
	#endif

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
	while((*p != 0) && (c < 2))
	{
		if (*p != '\r')
		{
			c = (*p == '\n') ? c + 1 : 0;
		}
		p++;
	}
	char * body = p;
	bool isGet = (strncmp(method, "GET", 3) == 0);
	return _onTCPRequest(client, isGet, url, body);
}

static esp_http_client_handle_t tcpClients[MAX_TCP_CLIENTS] = {NULL};

static esp_err_t on_ack_handler(esp_http_client_event_t *evt)
{
    // Handle ACK event
    return ESP_OK;
}

static esp_err_t on_data_handler(esp_http_client_event_t *evt)
{
    // Handle DATA event
    return ESP_OK;
}

static esp_err_t on_disconnect_handler(esp_http_client_event_t *evt)
{
    // Handle DISCONNECT event
    // Clean up resources and free client
    esp_http_client_handle_t client = evt->client;
    for (int i = 0; i < MAX_TCP_CLIENTS; i++)
    {
        if (tcpClients[i] == client)
        {
            tcpClients[i] = NULL;
            ESP_LOGI("TCP", "[FAUXMO] Client #%d disconnected\n", i);
            break;
        }
    }
    esp_http_client_cleanup(client);
    return ESP_OK;
}

static esp_err_t on_error_handler(esp_http_client_event_t *evt)
{
    esp_http_client_handle_t client = evt->client;
    ESP_LOGI("TCP", "[FAUXMO] Error %s on client\n", esp_err_to_name(esp_http_client_get_status_code(client)));
    return ESP_OK;
}

static esp_err_t on_timeout_handler(esp_http_client_event_t *evt)
{
    // Handle TIMEOUT event
    ESP_LOGI("TCP", "[FAUXMO] Timeout on client\n");
    esp_http_client_handle_t client = evt->client;
    esp_http_client_cleanup(client);
    return ESP_OK;
}

static esp_err_t on_tcp_disconnect(esp_http_client_handle_t client)
{
    return ESP_OK;
}

static esp_err_t on_tcp_data(esp_http_client_handle_t client, esp_http_client_event_t *evt)
{
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        on_ack_handler(evt);
        break;
    case HTTP_EVENT_ON_DATA:
        on_data_handler(evt);
        break;
    case HTTP_EVENT_DISCONNECTED:
        on_disconnect_handler(evt);
        break;
    case HTTP_EVENT_ERROR:
        on_error_handler(evt);
        break;
    case HTTP_EVENT_TIMEOUT:
        on_timeout_handler(evt);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t on_tcp_connect(esp_http_client_handle_t client)
{
    for (int i = 0; i < MAX_TCP_CLIENTS; i++)
    {
        if (tcpClients[i] == NULL)
        {
            tcpClients[i] = client;

            // Set your callback functions here
            esp_http_client_config_t config = {
                // ... other configuration ...
                .event_handler = http_event_handler,
                .user_data = (void *)client,
            };

            esp_err_t err = esp_http_client_perform(client);
            if (err != ESP_OK)
            {
                ESP_LOGE("TCP", "[FAUXMO] Client connection error: %s", esp_err_to_name(err));
                return err;
            }

            ESP_LOGI("TCP", "[FAUXMO] Client #%d connected\n", i);
            return ESP_OK;
        }
    }
    ESP_LOGI("TCP", "[FAUXMO] Rejecting - Too many connections\n");
    return ESP_FAIL; // Change this to a meaningful error code
}

#include <esp_log.h>
#include <esp_http_client.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void fauxmoESP::_onTCPClient(esp_http_client_handle_t client)
{
    int new_sock = -1;

    // Obtain the underlying socket descriptor from the HTTP client handle
    int client_sock = esp_http_client_get_transport_handle(client);

    if (client_sock >= 0)
    {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        new_sock = accept(client_sock, (struct sockaddr *)&addr, &addr_len);
    }

    if (new_sock < 0)
    {
        ESP_LOGE("TCP", "Unable to accept connection: errno %d", errno);
        return;
    }

    // Configure the HTTP client
    esp_http_client_config_t config = {
        .host = "localhost", // Set your host here
        .port = 80,           // Set your port here
        .method = HTTP_METHOD_GET,
        .path = "/",          // Set your path here
        .event_handler = {
            .cb = on_tcp_connect,
            .user_data = NULL,
        },
    };

     // Initialize and perform HTTP request
    esp_http_client_handle_t new_client = esp_http_client_init(&config);
    esp_http_client_set_transport_type(new_client, HTTP_TRANSPORT_OVER_TCP);
    esp_err_t err = esp_http_client_perform(new_client);
    if (err != ESP_OK)
    {
        ESP_LOGE("HTTP", "HTTP request failed: %s", esp_err_to_name(err));
    }

    // Cleanup HTTP client
    esp_http_client_cleanup(new_client);

    // Close the new socket
    close(new_sock);
}

// -----------------------------------------------------------------------------
// Devices
// -----------------------------------------------------------------------------

// fauxmoESP::~fauxmoESP()
// {  	
// 	// Free the name for each device
// 	for (auto& device : _devices)
// 	{
// 		free(device.name);
//   	}
  	
// 	// Delete devices  
// 	_devices.clear();

// }

// void fauxmoESP::setDeviceUniqueId(unsigned char id, const char *uniqueid)
// {
//     strncpy(_devices[id].uniqueid, uniqueid, FAUXMO_DEVICE_UNIQUE_ID_LENGTH);
// }

// unsigned char fauxmoESP::addDevice(const char * device_name) {

//     fauxmoesp_device_t device;
//     unsigned int device_id = _devices.size();

//     // init properties
//     device.name = strdup(device_name);
//   	device.state = false;
// 	  device.value = 0;

//     // create the uniqueid
//     String mac = WiFi.macAddress();

//     snprintf(device.uniqueid, 27, "%s:%s-%02X", mac.c_str(), "00:00", device_id);

//     // Attach
//     _devices.push_back(device);

//     ESP_LOGI("[FAUXMO] Device '%s' added as #%d\n", device_name, device_id);

//     return device_id;

// }

// int fauxmoESP::getDeviceId(const char * device_name) {
//     for (unsigned int id=0; id < _devices.size(); id++) {
//         if (strcmp(_devices[id].name, device_name) == 0) {
//             return id;
//         }
//     }
//     return -1;
// }

// bool fauxmoESP::renameDevice(unsigned char id, const char * device_name) {
//     if (id < _devices.size()) {
//         free(_devices[id].name);
//         _devices[id].name = strdup(device_name);
//         ESP_LOGI("[FAUXMO] Device #%d renamed to '%s'\n", id, device_name);
//         return true;
//     }
//     return false;
// }

// bool fauxmoESP::renameDevice(const char * old_device_name, const char * new_device_name) {
// 	int id = getDeviceId(old_device_name);
// 	if (id < 0) return false;
// 	return renameDevice(id, new_device_name);
// }

// bool fauxmoESP::removeDevice(unsigned char id) {
//     if (id < _devices.size()) {
//         free(_devices[id].name);
// 		_devices.erase(_devices.begin()+id);
//         ESP_LOGI("[FAUXMO] Device #%d removed\n", id);
//         return true;
//     }
//     return false;
// }

// bool fauxmoESP::removeDevice(const char * device_name) {
// 	int id = getDeviceId(device_name);
// 	if (id < 0) return false;
// 	return removeDevice(id);
// }

// char * fauxmoESP::getDeviceName(unsigned char id, char * device_name, size_t len) {
//     if ((id < _devices.size()) && (device_name != NULL)) {
//         strncpy(device_name, _devices[id].name, len);
//     }
//     return device_name;
// }

// bool fauxmoESP::setState(unsigned char id, bool state, unsigned char value) {
//     if (id < _devices.size()) {
// 		_devices[id].state = state;
// 		_devices[id].value = value;
// 		return true;
// 	}
// 	return false;
// }

// bool fauxmoESP::setState(const char * device_name, bool state, unsigned char value) {
// 	int id = getDeviceId(device_name);
// 	if (id < 0) return false;
// 	_devices[id].state = state;
// 	_devices[id].value = value;
// 	return true;
// }

// // -----------------------------------------------------------------------------
// // Public API
// // -----------------------------------------------------------------------------

bool fauxmoESP::process(esp_http_client_handle_t client, bool isGet, char *url, char *body)
{
	return _onTCPRequest(client, isGet, url, body);
}

void fauxmoESP::handle()
{
    if (_enabled) _handleUDP();
}

// void fauxmoESP::enable(bool enable)
// {
// 	if (enable == _enabled) return;
//     _enabled = enable;
// 	if (_enabled) {
// 		ESP_LOGI("[FAUXMO] Enabled\n");
// 	} else {
// 		ESP_LOGI("[FAUXMO] Disabled\n");
// 	}
//     if (_enabled)
		// {
// 		// Start TCP server if internal
// 		if (_internal) {
// 			if (NULL == _server) {
// 				_server = new AsyncServer(_tcp_port);
// 				_server->onClient([this](void *s, AsyncClient* c) {
// 					_onTCPClient(c);
// 				}, 0);
// 			}
// 			_server->begin();
// 		}
// 		// UDP setup
// 		#ifdef ESP32
//             _udp.beginMulticast(FAUXMO_UDP_MULTICAST_IP, FAUXMO_UDP_MULTICAST_PORT);
//         #else
//             _udp.beginMulticast(WiFi.localIP(), FAUXMO_UDP_MULTICAST_IP, FAUXMO_UDP_MULTICAST_PORT);
//         #endif
//         ESP_LOGI("[FAUXMO] UDP server started\n");
// 	}
// }
