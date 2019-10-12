// This example has been taken from RobotDyn.
// It was for an ESP8266, originally using a PMW output to connect to a DC lamp.
// I did alter it to use the RBDdimmer library to control a zero-cross-detector/phase-start-dimmer for AC lights.
// WARNING: This was NOT a success! The attached led lamps looked like flickering candlesticks...
// My guess is that an ESP8266 has too much to do to keep the wifi active, and during wifi negotiations
// it probably messes up the interrupts needed for the dimmer.
// Thijs Kaper, October 2019.

/**************
 *  RobotDyn
 *  Dimmer Library
 *  **************
 * 
 * The following sketch is meant to define the dimming value through the serial port of the controller, 
 * using Serial.begin 
 *    void printSpace() function is used for adding of space after functional data
 *    void loop()  serial port evaluator, used to register and define values in dimmer.setPower(outVal);
 *    
 * 
 *  ---------------------- OUTPUT & INPUT Pin table ---------------------
 *  +---------------+-------------------------+-------------------------+
 *  |   Board       | INPUT Pin               | OUTPUT Pin              |
 *  |               | Zero-Cross              |                         |
 *  +---------------+-------------------------+-------------------------+
 *  | Lenardo       | D7 (NOT CHANGABLE)      | D0-D6, D8-D13           |
 *  +---------------+-------------------------+-------------------------+
 *  | Mega          | D2 (NOT CHANGABLE)      | D0-D1, D3-D70           |
 *  +---------------+-------------------------+-------------------------+
 *  | Uno           | D2 (NOT CHANGABLE)      | D0-D1, D3-D20           |
 *  +---------------+-------------------------+-------------------------+
 *  | ESP8266       | D1(IO5),    D2(IO4),    | D0(IO16),   D1(IO5),    |
 *  |               | D5(IO14),   D6(IO12),   | D2(IO4),    D5(IO14),   |
 *  |               | D7(IO13),   D8(IO15),   | D6(IO12),   D7(IO13),   |
 *  |               |                         | D8(IO15)                |
 *  +---------------+-------------------------+-------------------------+
 *  | ESP32         | 4(GPI36),   6(GPI34),   | 8(GPO32),   9(GP033),   |
 *  |               | 5(GPI39),   7(GPI35),   | 10(GPIO25), 11(GPIO26), |
 *  |               | 8(GPO32),   9(GP033),   | 12(GPIO27), 13(GPIO14), |
 *  |               | 10(GPI025), 11(GPIO26), | 14(GPIO12), 16(GPIO13), |
 *  |               | 12(GPIO27), 13(GPIO14), | 23(GPIO15), 24(GPIO2),  |
 *  |               | 14(GPIO12), 16(GPIO13), | 25(GPIO0),  26(GPIO4),  |
 *  |               | 21(GPIO7),  23(GPIO15), | 27(GPIO16), 28(GPIO17), |
 *  |               | 24(GPIO2),  25(GPIO0),  | 29(GPIO5),  30(GPIO18), |
 *  |               | 26(GPIO4),  27(GPIO16), | 31(GPIO19), 33(GPIO21), |
 *  |               | 28(GPIO17), 29(GPIO5),  | 34(GPIO3),  35(GPIO1),  |
 *  |               | 30(GPIO18), 31(GPIO19), | 36(GPIO22), 37(GPIO23), |
 *  |               | 33(GPIO21), 35(GPIO1),  |                         |
 *  |               | 36(GPIO22), 37(GPIO23), |                         |
 *  +---------------+-------------------------+-------------------------+
 *  | Arduino M0    | D7 (NOT CHANGABLE)      | D0-D6, D8-D13           |
 *  | Arduino Zero  |                         |                         |
 *  +---------------+-------------------------+-------------------------+
 *  | Arduino Due   | D0-D53                  | D0-D53                  |
 *  +---------------+-------------------------+-------------------------+
 *  | STM32         | PA0-PA15,PB0-PB15       | PA0-PA15,PB0-PB15       |
 *  | Black Pill    | PC13-PC15               | PC13-PC15               |
 *  | BluePill      |                         |                         |
 *  | Etc...        |                         |                         |
 *  +---------------+-------------------------+-------------------------+
 */

#include <RBDdimmer.h>//
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#define outputPin  D5 
#define zerocross  D6

#define LIGHTS_COUNT 1

dimmerLamp dimmer(outputPin, zerocross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards

int outVal = 0;

uint8_t scene;
bool light_state[LIGHTS_COUNT], in_transition;
int transitiontime[LIGHTS_COUNT], bri[LIGHTS_COUNT];
float step_level[LIGHTS_COUNT], current_bri[LIGHTS_COUNT];
byte mac[6];

ESP8266WebServer server(80);

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void apply_scene(uint8_t new_scene,  uint8_t light) {
  if ( new_scene == 0) {
    bri[light] = 144;
  } else if ( new_scene == 1) {
    bri[light] = 254;
  } else if ( new_scene == 2) {
    bri[light] = 10;
  }
}

void process_lightdata(uint8_t light, float transitiontime) {
  transitiontime *= 16;
  if (light_state[light]) {
    step_level[light] = (bri[light] - current_bri[light]) / transitiontime;
  } else {
    step_level[light] = current_bri[light] / transitiontime;
  }
}

void lightEngine() {
  for (int i = 0; i < LIGHTS_COUNT; i++) {
    if (light_state[i]) {
      if (bri[i] != current_bri[i]) {
        in_transition = true;
        current_bri[i] += step_level[i];
        if ((step_level[i] > 0.0 && current_bri[i] > bri[i]) || (step_level[i] < 0.0 && current_bri[i] < bri[i])) {
          current_bri[i] = bri[i];
        }
        //analogWrite(i, (int)(current_bri[i]));
        int percentage = current_bri[i]*100/256;
        dimmer.setPower(percentage); // setPower(0-100%);
        Serial.print("value");
        Serial.println(percentage);
      }
    } else {
      if (current_bri[i] != 0 ) {
        in_transition = true;
        current_bri[i] -= step_level[i];
        if (current_bri[i] < 0) {
          current_bri[i] = 0;
        }
//        analogWrite(i, (int)(current_bri[i]));
        int percentage = current_bri[i]*100/256;
        dimmer.setPower(percentage); // setPower(0-100%);
        Serial.print("value");
        Serial.println(percentage);
      }
    }
  }
  if (in_transition) {
    delay(6);
    in_transition = false;
  }
}

void setup() {
  Serial.begin(9600); 
  dimmer.begin(NORMAL_MODE, ON); //dimmer initialisation: name.begin(MODE, STATE) 
  Serial.println("Dimmer Program is starting...");

  EEPROM.begin(512);

  for (uint8_t light = 0; light < LIGHTS_COUNT; light++) {
    apply_scene(EEPROM.read(2), light);
    step_level[light] = bri[light] / 150.0;
  }


  if (EEPROM.read(1) == 1 || (EEPROM.read(1) == 0 && EEPROM.read(0) == 1)) {
    for (int i = 0; i < LIGHTS_COUNT; i++) {
      light_state[i] = true;
    }
    for (int j = 0; j < 200; j++) {
      lightEngine();
    }
  }
  WiFiManager wifiManager;
  wifiManager.autoConnect("New Hue Light");
  if (! light_state[0])  {
    // Show that we are connected
    dimmer.setPower(70); // setPower(0-100%);
    delay(500);
    dimmer.setPower(0); // setPower(0-100%);
  }

  WiFi.macAddress(mac);

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("dimmer");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"x");

  ArduinoOTA.begin();

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH



  server.on("/switch", []() {
    server.send(200, "text/plain", "OK");
    float transitiontime = 4;
    int button;
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "button") {
        button = server.arg(i).toInt();
      }
    }
    for (int i = 0; i < LIGHTS_COUNT; i++) {
      if (button == 1000) {
        if (light_state[i] == false) {
          light_state[i] = true;
          scene = 0;
        } else {
          apply_scene(scene, i);
          scene++;
          if (scene == 11) {
            scene = 0;
          }
        }
      } else if (button == 2000) {
        if (light_state[i] == false) {
          bri[i] = 30;
          light_state[i] = true;
        } else {
          bri[i] += 30;
        }
        if (bri[i] > 255) bri[i] = 255;
      } else if (button == 3000 && light_state[i] == true) {
        bri[i] -= 30;
        if (bri[i] < 1) bri[i] = 1;
      } else if (button == 4000) {
        light_state[i] = false;
      }
      if (light_state[i]) {
        step_level[i] = ((float)bri[i] - current_bri[i]) / transitiontime;
      } else {
        step_level[i] = current_bri[i] / transitiontime;
      }
    }
  });

  server.on("/set", []() {
    uint8_t light;
    float transitiontime = 4;
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "light") {
        light = server.arg(i).toInt() - 1;
      }
      else if (server.argName(i) == "on") {
        if (server.arg(i) == "True" || server.arg(i) == "true") {
          light_state[light] = true;
          if (EEPROM.read(1) == 0 && EEPROM.read(0) == 0) {
            EEPROM.write(0, 1);
          }
        }
        else {
          light_state[light] = false;
          if (EEPROM.read(1) == 0 && EEPROM.read(0) == 1) {
            EEPROM.write(0, 0);
          }
        }
        EEPROM.commit();
      }
      else if (server.argName(i) == "bri") {
        light_state[light] = true;
        if (server.arg(i).toInt() != 0)
          bri[light] = server.arg(i).toInt();
      }
      else if (server.argName(i) == "bri_inc") {
        bri[light] += server.arg(i).toInt();
        if (bri[light] > 255) bri[light] = 255;
        else if (bri[light] < 0) bri[light] = 0;
      }
      else if (server.argName(i) == "alert" && server.arg(i) == "select") {
        if (light_state[light]) {
          current_bri[light] = 0;
        } else {
          current_bri[light] = 255;
        }
      }
      else if (server.argName(i) == "transitiontime") {
        transitiontime = server.arg(i).toInt();
      }
    }
    process_lightdata(light, transitiontime);
    server.send(200, "text/plain", "OK, bri:" + (String)bri[light] + ", state:" + light_state[light]);
  });

  server.on("/get", []() {
    uint8_t light;
    if (server.hasArg("light"))
      light = server.arg("light").toInt() - 1;
    String power_status;
    power_status = light_state[light] ? "true" : "false";
    server.send(200, "text/plain", "{\"on\": " + power_status + ", \"bri\": " + (String)bri[light] + "}");
  });

  server.on("/detect", []() {
    server.send(200, "text/plain", "{\"hue\": \"bulb\",\"lights\": " + (String)LIGHTS_COUNT + ",\"modelid\": \"LWB010\",\"mac\": \"" + String(mac[5], HEX) + ":"  + String(mac[4], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[0], HEX) + "\"}");
  });

  server.on("/", []() {
    float transitiontime = 4;
    if (server.hasArg("startup")) {
      if (  EEPROM.read(1) != server.arg("startup").toInt()) {
        EEPROM.write(1, server.arg("startup").toInt());
        EEPROM.commit();
      }
    }

    for (int light = 0; light < LIGHTS_COUNT; light++) {
      if (server.hasArg("scene")) {
        if (server.arg("bri") == "" && server.arg("hue") == "" && server.arg("ct") == "" && server.arg("sat") == "") {
          if (  EEPROM.read(2) != server.arg("scene").toInt()) {
            EEPROM.write(2, server.arg("scene").toInt());
            EEPROM.commit();
          }
          apply_scene(server.arg("scene").toInt(), light);
        } else {
          if (server.arg("bri") != "") {
            bri[light] = server.arg("bri").toInt();
          }
        }
      } else if (server.hasArg("on")) {
        if (server.arg("on") == "true") {
          light_state[light] = true; {
            if (EEPROM.read(1) == 0 && EEPROM.read(0) == 0) {
              EEPROM.write(0, 1);
            }
          }
        } else {
          light_state[light] = false;
          if (EEPROM.read(1) == 0 && EEPROM.read(0) == 1) {
            EEPROM.write(0, 0);
          }
        }
        EEPROM.commit();
      } else if (server.hasArg("alert")) {
        if (light_state[light]) {
          current_bri[light] = 0;
        } else {
          current_bri[light] = 255;
        }
      }
      if (light_state[light]) {
        step_level[light] = ((float)bri[light] - current_bri[light]) / transitiontime;
      } else {
        step_level[light] = current_bri[light] / transitiontime;
      }
    }
    if (server.hasArg("reset")) {
      ESP.reset();
    }


    String http_content = "<!doctype html>";
    http_content += "<html>";
    http_content += "<head>";
    http_content += "<meta charset=\"utf-8\">";
    http_content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    http_content += "<title>Light Setup</title>";
    http_content += "<link rel=\"stylesheet\" href=\"https://unpkg.com/purecss@0.6.2/build/pure-min.css\">";
    http_content += "</head>";
    http_content += "<body>";
    http_content += "<fieldset>";
    http_content += "<h3>Light Setup</h3>";
    http_content += "<form class=\"pure-form pure-form-aligned\" action=\"/\" method=\"post\">";
    http_content += "<div class=\"pure-control-group\">";
    http_content += "<label for=\"power\"><strong>Power</strong></label>";
    http_content += "<a class=\"pure-button"; if (light_state[0]) http_content += "  pure-button-primary"; http_content += "\" href=\"/?on=true\">ON</a>";
    http_content += "<a class=\"pure-button"; if (!light_state[0]) http_content += "  pure-button-primary"; http_content += "\" href=\"/?on=false\">OFF</a>";
    http_content += "</div>";
    http_content += "<div class=\"pure-control-group\">";
    http_content += "<label for=\"startup\">Startup</label>";
    http_content += "<select onchange=\"this.form.submit()\" id=\"startup\" name=\"startup\">";
    http_content += "<option "; if (EEPROM.read(1) == 0) http_content += "selected=\"selected\""; http_content += " value=\"0\">Last state</option>";
    http_content += "<option "; if (EEPROM.read(1) == 1) http_content += "selected=\"selected\""; http_content += " value=\"1\">On</option>";
    http_content += "<option "; if (EEPROM.read(1) == 2) http_content += "selected=\"selected\""; http_content += " value=\"2\">Off</option>";
    http_content += "</select>";
    http_content += "</div>";
    http_content += "<div class=\"pure-control-group\">";
    http_content += "<label for=\"scene\">Scene</label>";
    http_content += "<select onchange = \"this.form.submit()\" id=\"scene\" name=\"scene\">";
    http_content += "<option "; if (EEPROM.read(2) == 0) http_content += "selected=\"selected\""; http_content += " value=\"0\">Relax</option>";
    http_content += "<option "; if (EEPROM.read(2) == 1) http_content += "selected=\"selected\""; http_content += " value=\"1\">Bright</option>";
    http_content += "<option "; if (EEPROM.read(2) == 2) http_content += "selected=\"selected\""; http_content += " value=\"2\">Nightly</option>";
    http_content += "</select>";
    http_content += "</div>";
    http_content += "<br>";
    http_content += "<div class=\"pure-control-group\">";
    http_content += "<label for=\"state\"><strong>State</strong></label>";
    http_content += "</div>";
    http_content += "<div class=\"pure-control-group\">";
    http_content += "<label for=\"bri\">Bri</label>";
    http_content += "<input id=\"bri\" name=\"bri\" type=\"text\" placeholder=\"" + (String)bri[0] + "\">";
    http_content += "</div>";

    http_content += "<div class=\"pure-controls\">";
    http_content += "<span class=\"pure-form-message\"><a href=\"/?alert=1\">alert</a> or <a href=\"/?reset=1\">reset</a></span>";
    http_content += "<label for=\"cb\" class=\"pure-checkbox\">";
    http_content += "</label>";
    http_content += "<button type=\"submit\" class=\"pure-button pure-button-primary\">Save</button>";
    http_content += "</div>";
    http_content += "</fieldset>";
    http_content += "</form>";
    http_content += "</body>";
    http_content += "</html>";


    server.send(200, "text/html", http_content);

  });

  server.onNotFound(handleNotFound);

  server.begin();

}

void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  server.handleClient();
  lightEngine();
}
