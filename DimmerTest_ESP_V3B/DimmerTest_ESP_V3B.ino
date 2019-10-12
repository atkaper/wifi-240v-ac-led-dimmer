// This piece of code allows me to control my dimmer using voice commands. It sends the brightness via I2C to
// an ATTiny-coprocessor, which is connected to a zero-cross-detectore/phase-start-triac module.
//
// This is mainly a generated example from the fauxmo library for ESP8266, to control the light via Alexa/Amazon-Echo-Dot.
// Example altered to send brightness via I2C (Wire library) to ATTINY Coprocessor, and added OTA update.
// I took the external webserver version instead of the embedded one, to have the option to add some
// html / rest-api calls later on. Not used yet (just a debug message on the root html page).
//
// Connect ESP D3 to SDA pin 5 of the ATTiny Coprocessor (see+use AvrDimmerCoprocessor_V3).
// Connect ESP D4 to SCK pin 7 of the ATTiny.
//
// Thijs Kaper, October 2019.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <Wire.h>
#define SDA_PIN D3 // 0 // GPIO0 - this pin has an external pull-up resistor, which is needed for I2C also.
#define SCL_PIN D4 // 2 // GPIO2 - same comment as above...
const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

int brightness = 0; // range 0-100% to I2C
int current_brightness = 1;

// Ugly hardcoded WIFI settings - new WifiManger wouldn't work together with old fauxmo library at first attempt (gave up on it).
#define WIFI_SSID "xxxx"
#define WIFI_PASS "xxxx"

fauxmoESP fauxmo;
AsyncWebServer server(80);

#define SERIAL_BAUDRATE                 115200

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}

String msg = "ok";

void serverSetup() {

    // Custom entry point (not required by the library, here just as an example)
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "status: " + msg);
    });

    // Custom entry point (not required by the library, here just as an example)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "status: " + msg);
    });

    // These two callbacks are required for gen1 and gen3 compatibility
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
        // Handle any other body request here...
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
        // Handle not found request here...
    });

    // Start the server
    server.begin();

}

void setup() {

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println();

//  Wire.begin(SDA_PIN,SCL_PIN); // initialize I2C (move down to initial setting of brightness)

    // Wifi
    wifiSetup();

    // Web server
    serverSetup();

    // Set fauxmoESP to not create an internal TCP server and redirect requests to the server on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxmo.createServer(false);
    fauxmo.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxmo.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn dimmer on" ("dimmer" is the name of the first device below)
    // "Alexa, turn on dimmer"
    // "Alexa, set dimmer to fifty" (50 means 50% of brightness)

    // Add virtual device(s)
    fauxmo.addDevice("dimmer");

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set dimmer light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
        // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
        // if (2 == device_id) analogWrite(LED1_PIN, value);
        
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

        // use map function to change 0-255 to 0-100 range.
        brightness = map(value, 0, 255, 0, 100);
        if (!state) {
          // OFF
          brightness = 0;
        }
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;

        // text for display on the "root" html page of this device, simple wireless debugging...
        msg = "state " + String(state) + ", value " + String(value) + ", brightness " + String(brightness);
    });

    // Make this updateable over the air...
    // Note: reminder for myself: DO NOT FORGET to enable SPIFFS memory in the arduino gui, e.g. flash 4M, spiffs 2M.
    // No OTA possible without SPIFFS.
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname("dimmer");
    ArduinoOTA.setPassword((const char *)"xxxx");
    ArduinoOTA.begin();

    Wire.begin(0, 2); // initialize I2C
    Wire.beginTransmission(I2C_SLAVE);
    Wire.write((byte) brightness);
    Wire.endTransmission();
    
}

void loop() {
    ArduinoOTA.handle();

    // fauxmoESP uses an async TCP server but a sync UDP server
    // Therefore, we have to manually poll for UDP packets
    fauxmo.handle();

    // I did have some OTA issues, and someone suggested to add MDNS.update().
    // In the end I do not think it is needed, but does not do any harm also.
    // My OTA issue is that when on cable internet, my router does not pass on the MDNS stuff from the Wifi.
    // Both mylaptop and ESP must be on Wifi to have OTA working.
    MDNS.update();

    // This is a sample code to output free heap every 5 seconds
    // This is a cheap way to detect memory leaks
    static unsigned long last = millis();
    if (millis() - last > 5000) {
        last = millis();
        Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    }

    if (current_brightness != brightness) {
        Wire.beginTransmission(I2C_SLAVE);
        Wire.write((byte) brightness);
        Wire.endTransmission();
        current_brightness = brightness;
    }

}
