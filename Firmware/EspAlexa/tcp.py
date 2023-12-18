import requests

# Replace with the IP address and port of your ESP32 server
esp32_ip = "192.168.18.148"
esp32_port = 80  # Change this to the port your server is running on

# Define the URL to send the request to
#url = f"http://{esp32_ip}:{esp32_port}/index.html"
url = f"http://{esp32_ip}:{esp32_port}/description.xml"
#url = f"http://{esp32_ip}:{esp32_port}/nonexistant_path"

# Send the HTTP GET request
response = requests.get(url)

# Print the response content
print("Response from ESP32 server:")
print(response.text)

'''
Response of description.xml
<?xml version="1.0" ?><root xmlns="urn:schemas-upnp-org:device-1-0"><specVersion><major>1</major><minor>0</minor></specVersion><URLBase>http://192.168.18.145:80/</URLBase><device><deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType><friendlyName>Philips hue (192.168.18.145:80)</friendlyName><manufacturer>Royal Philips Electronics</manufacturer><manufacturerURL>http://www.philips.com</manufacturerURL><modelDescription>Philips hue Personal Wireless Lighting</modelDescription><modelName>Philips hue bridge 2012</modelName><modelNumber>929000226503</modelNumber><modelURL>http://www.meethue.com</modelURL><serialNumber>94e686045d4c</serialNumber><UDN>uuid:2f402f80-da50-11e1-9b23-94e686045d4c</UDN><presentationURL>index.html</presentationURL></device></root>

Response of index.html
I'm Alexa!

Response of nonexistant_path
Error
'''