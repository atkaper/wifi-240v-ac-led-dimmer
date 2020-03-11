// This piece of code allows me to control my dimmer using voice commands. It sends the brightness via I2C to
// an ATTiny-coprocessor, which is connected to a zero-cross-detectore/phase-start-triac module.
//
// This is mainly a generated example from the Espalexa library for ESP8266, to control the light via Alexa/Amazon-Echo-Dot.
// Example altered to send brightness via I2C (Wire library) to ATTINY Coprocessor, and added OTA update.
//
// Connect ESP D3 to SDA pin 5 of the ATTiny Coprocessor (see+use AvrDimmerCoprocessor_V3).
// Connect ESP D4 to SCK pin 7 of the ATTiny.
//
// Thijs Kaper, March 11, 2020.

#include <ESP8266WiFi.h>
#include <Espalexa.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <Wire.h>
#define SDA_PIN D3 // 0 // GPIO0 - this pin has an external pull-up resistor, which is needed for I2C also.
#define SCL_PIN D4 // 2 // GPIO2 - same comment as above...

const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

int brightness = 0; // range 0-100% to I2C
int current_brightness = 1; // range 0-100% to I2C

// prototypes
boolean connectWifi();

//callback function
void firstLightChanged(uint8_t brightness);

// Ugly hardcoded WIFI settings - should look at using WifiManger...
#define ssid "xxxx"
#define password "xxxx"

boolean wifiConnected = false;

Espalexa espalexa;

void setup()
{
  Serial.begin(115200);
  // Initialise wifi connection
  wifiConnected = connectWifi();
  
  if(wifiConnected) {
    
    // Define your devices here. 
    espalexa.addDevice("dimmer", firstLightChanged); //simplest definition, default state off
    espalexa.begin();
    
  } else {
    while (1) {
      Serial.println("Cannot connect to WiFi. Please check data and reset the ESP.");
      delay(2500);
    }
  }

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
 
void loop()
{
    espalexa.loop();
    delay(1);
    
    ArduinoOTA.handle();

    // I did have some OTA issues, and someone suggested to add MDNS.update().
    // In the end I do not think it is needed, but does not do any harm also.
    // My OTA issue is that when on cable internet, my router does not pass on the MDNS stuff from the Wifi.
    // Both my laptop and ESP must be on Wifi to have OTA working.
    MDNS.update();
    
    if (current_brightness != brightness) {
        Wire.beginTransmission(I2C_SLAVE);
        Wire.write((byte) brightness);
        Wire.endTransmission();
        current_brightness = brightness;
    }
}

//our callback functions
void firstLightChanged(uint8_t newBrightness) {
    brightness = espalexa.toPercent(newBrightness);
}

// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false; break;
    }
    i++;
  }
  Serial.println("");
  if (state){
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Connection failed.");
  }
  return state;
}
