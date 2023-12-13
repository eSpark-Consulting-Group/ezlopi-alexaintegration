#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "fauxmoESP.h"
#include "driver/gpio.h"
#include "credentials.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"

fauxmoESP fauxmo;

const int state = 1;
#define BLINK_GPIO GPIO_NUM_2


esp_err_t hello_get_handler(httpd_req_t *req) 
{
    const char* response = "I'm Alexa!";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

httpd_uri_t hello = 
{
    .uri       = "/index.html",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

static httpd_handle_t server = NULL;

void wifiSetup() 
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    
    wifi_config_t wifi_config;
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    
    // Wait for connection
    while (1) 
    {
        if (esp_wifi_connect() == ESP_OK) 
        {
            wifi_config_t current_config;
            esp_wifi_get_config(WIFI_IF_STA, &current_config);
            if (strlen(reinterpret_cast<char*>(current_config.sta.ssid)) > 0) 
            {
                ESP_LOGI("WIFI", "Connected to SSID: %s", reinterpret_cast<char*>(current_config.sta.ssid));
                break;
            }
            break;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void serverSetup() 
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        httpd_register_uri_handler(server, &hello);
    }
}
    
void alexa()
{
    ESP_LOGI("DEBUGGING","HERE");
    gpio_set_level(BLINK_GPIO,state);
    wifiSetup();
    vTaskDelay(1000);
    serverSetup();
    vTaskDelay(1000);
    // Set fauxmoESP to not create an internal TCP server and redirect requests to the server on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxmo.createServer(false);
    fauxmo.setPort(80);
    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxmo.enable(true);
    
    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn kitchen on" ("kitchen" is the name of the first device below)
    // "Alexa, turn on kitchen"
    // "Alexa, set kitchen to fifty" (50 means 50% of brightness)

    // Add virtual devices
    fauxmo.addDevice("kitchen");
	fauxmo.addDevice("livingroom");

    // You can add more devices
	//fauxmo.addDevice("light 3");
    //fauxmo.addDevice("light 4");
    //fauxmo.addDevice("light 5");
    //fauxmo.addDevice("light 6");
    //fauxmo.addDevice("light 7");
    //fauxmo.addDevice("light 8");

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
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
    });
}

extern "C" {void app_main();}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("HELLO!\n");
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    //xTaskCreate(alexa, "Alexa_task", 4096*2, NULL, 10, NULL);
    alexa();
}