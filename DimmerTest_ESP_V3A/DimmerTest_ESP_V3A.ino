// Ok, this was a super simple bit of ESP8266 code, to offer a web page to send light brightness over I2C.
// No connection with Alexa/Amazon-Echo-Dot yet.
//
// Connect ESP D3 to SDA pin 5 of the ATTiny Coprocessor (see+use AvrDimmerCoprocessor_V3).
// Connect ESP D4 to SCK pin 7 of the ATTiny.
//
// Thijs Kaper, October 2019.

#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>

#include <Wire.h>

#define SDA_PIN D3 // 0 // GPIO0 - this pin has an external pull-up resistor, which is needed for I2C also.
#define SCL_PIN D4 // 2 // GPIO2 - same comment as above...
const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

ESP8266WebServer server(80);

int brightness = 50;

void setup() {

    pinMode(2, INPUT); // this was left over from an earlier test, probably not needed... (setting as output broke I2C)
    digitalWrite(2, LOW); // this was left over from an earlier test, probably not needed... (setting it high broke I2C)

    Serial.begin(115200);

//  Wire.begin(SDA_PIN,SCL_PIN); // initialize I2C (same as next line, forgot to re-enable this one in final version)
    Wire.begin(0,2); // initialize I2C

    WiFiManager wifiManager;
    //wifiManager.resetSettings();
    
    wifiManager.autoConnect("DimmerTest");
    
    Serial.println("WiFi-connected");

    server.on ( "/", handleRoot );
    server.onNotFound ( handleNotFound );
    server.begin();

    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname("dimmer");
    ArduinoOTA.setPassword((const char *)"xxxx");
    ArduinoOTA.begin();

    Wire.beginTransmission(I2C_SLAVE);
    Wire.write((byte) brightness);
    Wire.endTransmission();
}

void handleNotFound() {
  String message = "This is the DimmerTest...\n\nFile Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  Serial.println("handleNotFound; " + message);
}


void handleRoot() {
  Serial.println("handleRoot");

  if (server.arg("light").length()>0) {
    brightness = server.arg("light").toInt();
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    Wire.beginTransmission(I2C_SLAVE);
    Wire.write((byte) brightness);
    Wire.endTransmission();
  }

  // Some simpel test links - not even a complete html page, but it works...
  String page = "<a href=\"?light=" + String(brightness - 5) + "\">?light=" + String(brightness - 5) + "</a><br/>\n";
  page = page + "<a href=\"?light=" + String(brightness) + "\">?light=" + String(brightness) + "</a><br/>\n";
  page = page + "<a href=\"?light=" + String(brightness + 5) + "\">?light=" + String(brightness + 5) + "</a><br/>\n";
  
  server.send ( 200, "text/html", page);
}

void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  server.handleClient();
}
