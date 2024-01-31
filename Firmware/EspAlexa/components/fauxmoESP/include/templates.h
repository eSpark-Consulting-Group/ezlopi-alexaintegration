#pragma once

#define FAUXMO_TCP_HEADERS "HTTP/1.1 %s\r\n"\
    "Content-Type: %s\r\n"\
    "Content-Length: %d\r\n"\
    "Connection: close\r\n\r\n"

#define FAUXMO_TCP_STATE_RESPONSE "["\
    "{\"success\":{\"/lights/%d/state/on\":%s}},"\
    "{\"success\":{\"/lights/%d/state/bri\":%d}}"\
"]"

// Working with gen1 and gen3, ON/OFF/%, gen3 requires TCP port 80
#define FAUXMO_DEVICE_JSON_TEMPLATE "{"\
    "\"type\": \"Extended color light\","\
    "\"name\": \"%s\","\
    "\"uniqueid\": \"%s\","\
    "\"modelid\": \"LCT015\","\
    "\"manufacturername\": \"Philips\","\
    "\"productname\": \"E4\","\
    "\"state\":{"\
        "\"on\": %s,"\
	"\"bri\": %d,"\
	"\"xy\": [0,0],"\
	"\"hue\": 0,"\
	"\"sat\": 0,"\
	"\"effect\": \"none\","\
	"\"colormode\": \"xy\","\
	"\"ct\": 500,"\
	"\"mode\": \"homeautomation\","\
	"\"reachable\": true"\
    "},"\
    "\"capabilities\": {"\
        "\"certified\": false,"\
        "\"streaming\": {\"renderer\":true,\"proxy\":false}"\
    "},"\
    "\"swversion\": \"5.105.0.21169\""\
"}"

// Use shorter description template when listing all devices
#define FAUXMO_DEVICE_JSON_TEMPLATE_SHORT "{"\
    "\"type\": \"Extended color light\","\
    "\"name\": \"%s\","\
    "\"uniqueid\": \"%s\""\
"}"

#define FAUXMO_DESCRIPTION_TEMPLATE "<?xml version=\"1.0\" ?>"\
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"\
    "<specVersion><major>1</major><minor>0</minor></specVersion>"\
    "<URLBase>http://%d.%d.%d.%d:%d/</URLBase>"\
    "<device>"\
        "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"\
        "<friendlyName>Philips hue (%d.%d.%d.%d:%d)</friendlyName>"\
        "<manufacturer>Royal Philips Electronics</manufacturer>"\
        "<manufacturerURL>http://www.philips.com</manufacturerURL>"\
        "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"\
        "<modelName>Philips hue bridge 2012</modelName>"\
        "<modelNumber>929000226503</modelNumber>"\
        "<modelURL>http://www.meethue.com</modelURL>"\
        "<serialNumber>%s</serialNumber>"\
        "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>"\
        "<presentationURL>index.html</presentationURL>"\
    "</device>"\
"</root>"

#define FAUXMO_UDP_RESPONSE_TEMPLATE "HTTP/1.1 200 OK\r\n"\
    "EXT:\r\n"\
    "CACHE-CONTROL: max-age=100\r\n"\
    "LOCATION: http://%d.%d.%d.%d:%d/description.xml\r\n"\
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n"\
    "hue-bridgeid: %s\r\n"\
    "ST: urn:schemas-upnp-org:device:basic:1\r\n"\
    "USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n\r\n"
    // "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
    // "DATE: Mon, 29 Jan 2024 04:56:29 GMT\r\n"
    // "USN: uuid:%s::urn:Belkin:device:controller:1\r\n\r\n"

// PROGMEM const char FAUXMO_UDP_RESPONSE_TEMPLATE[] =
//     "HTTP/1.1 200 OK\r\n"
//     "EXT:\r\n"
//     "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
//     "LOCATION: http://%d.%d.%d.%d:%d/description.xml\r\n"
//     "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
//     "hue-bridgeid: %s\r\n"
//     "ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
//     "USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n" // _uuid::_deviceType
//     "\r\n";