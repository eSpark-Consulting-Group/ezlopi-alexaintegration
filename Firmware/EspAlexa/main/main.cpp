#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "fauxmoESP.h"
#include "esp_netif.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "protocol_examples_common.h"

fauxmoESP fauxmo;
const int state = 1;

#define BLINK_GPIO GPIO_NUM_2

esp_err_t hello_get_handler(httpd_req_t *req) 
{
    const char* response = "I'm Alexa!";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t handle_request_body(httpd_req_t *req, const char *data, size_t len)
{
    ESP_LOGI("REQUEST","Received Data: %s",data);
    return ESP_OK;
}
void processRequest(httpd_req_t *req, char *data)
{
    if (req->method == HTTP_GET)
    {
        // Assuming fauxmo.process signature is similar to fauxmo.process(TCPClient &client, bool isGet, const char *url, const String &data)
        // Modify the arguments accordingly based on the actual signature of fauxmo.process

        AsyncClient *client = (AsyncClient *)req->handle;
        char *uriCopy = strdup(req->uri);
        if(uriCopy)
        {
            if (fauxmo.process(client, true, uriCopy, (char*)data))
            {
                // Returning early if fauxmo.process returns true
                free(uriCopy);
                return;
            }
            free(uriCopy);
        }
        
    }
    // Handle any other body request here...
}
esp_err_t request_body_handler(httpd_req_t *req)
{
    size_t content_length = req->content_len;

    if (content_length > 0)
    {
        // Allocate buffer for the request body
        char *data = (char *)malloc(content_length + 1);
        if (data)
        {
            // Read the request body
            if (httpd_req_recv(req, data, content_length) == ESP_OK)
            {
                // Null-terminate the data
                data[content_length] = '\0';
                ESP_LOGI("REQ","REQ PROCESSED!");
                // Process the request body
                processRequest(req, data);
                ESP_LOGI("REQ","REQ PROCESS COMPLETE!");
            }
            // Free the allocated buffer
            free(data);
        }
    }
    // Respond to the client
    //if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
    return ESP_OK;
}

httpd_uri_t hello = 
{
    .uri       = "/index.html",
    .method    = HTTP_POST,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

httpd_uri_t request_body_uri =
{
    .uri = "/description.xml",
    .method = HTTP_POST,
    .handler = request_body_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_LOGI("WEB SERVER", "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI("WEB SERVER", "Registering URI handlers");
        // httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &request_body_uri);
        return server;
    }
    ESP_LOGI("WEB SERVER", "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL)
    {
        ESP_LOGI("WEB SERVER", "Starting webserver");
        *server = start_webserver();
    }
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server)
    {
        ESP_LOGI("WEB SERVER", "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}
    
void alexa()
{
    ESP_LOGI("DEBUGGING","ALEXA LOOP STARTED!");
    gpio_set_level(BLINK_GPIO,state);
    fauxmo.createServer(false);
    fauxmo.setPort(80);
    fauxmo.enable(true);

    // You can add more devices
	//fauxmo.addDevice("light 3");
    //fauxmo.addDevice("light 4");
    //fauxmo.addDevice("light 5");
    //fauxmo.addDevice("light 6");
    //fauxmo.addDevice("light 7");
    //fauxmo.addDevice("light 8");

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value)
    {

        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
        // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
        // if (2 == device_id) analogWrite(LED1_PIN, value);
        
        ESP_LOGI("DEVICE_STATE","[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

        // For the example we are turning the same LED on and off regardless fo the device triggered or the value
        gpio_set_level(BLINK_GPIO,!state); // we are nor-ing the state because our LED has inverse logic.
    });

    while(1)
    {
        // fauxmoESP uses an async TCP server but a sync UDP server
        // Therefore, we have to manually poll for UDP packets
        fauxmo.handle();

        // This is a sample code to output free heap every 5 seconds
        // This is a cheap way to detect memory leaks
        static unsigned long last = esp_timer_get_time()/1000ULL;
        if ((esp_timer_get_time()/1000ULL) - last > 5000) {
            last = esp_timer_get_time()/1000ULL;
            ESP_LOGI("HEAP_MEMORY","[MAIN] Free heap: %d bytes\n", esp_get_free_heap_size());
        }
        vTaskDelay(1000);
    }
}

extern "C" {void app_main();}

void app_main(void)
{
    static httpd_handle_t server = NULL;
    printf("HELLO!\n");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(example_connect());

#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif

    server = start_webserver();
    alexa();
}