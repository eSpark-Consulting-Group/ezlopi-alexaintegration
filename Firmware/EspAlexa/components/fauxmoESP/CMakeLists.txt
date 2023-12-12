idf_component_register(SRCS "fauxmoESP.cpp"
                        INCLUDE_DIRS include
                        REQUIRES 
                            esp_http_client
                            lwip
                            esp_netif
                            mbedtls
                            freertos
                            AsyncTCP-master
                            esp_wifi
)
